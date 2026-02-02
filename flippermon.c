#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <furi_hal_nfc.h>
#include <furi_hal_infrared.h>
#include <storage/storage.h>

#define SAVE_PATH EXT_PATH("apps_data/flippermon/save.dat")

typedef struct {
    uint8_t health;
    uint8_t level;
    uint8_t happiness;
    int8_t y_offset;
    char name[16];
} NurseryModel;

typedef enum {
    FlipperMonViewSubmenu,
    FlipperMonViewNursery,
} FlipperMonView;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* nursery_view;
    NotificationApp* notify;
    uint32_t current_view;
    NurseryModel pet_stats;
} FlipperMonApp;

// --- Sprites & Audio ---
static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t yeti_large[] = { 0x3C, 0x42, 0x99, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C, 0x42, 0x81, 0xBD, 0xBD, 0x81, 0x42, 0x3C, 0x3C, 0x42, 0x81, 0xA5, 0xA5, 0x81, 0x42, 0x3C };

void play_beep(float freq, uint32_t duration) {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(freq, 1.0f);
        furi_delay_ms(duration);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// --- Persistence ---
void save_game(FlipperMonApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, EXT_PATH("apps_data/flippermon"));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->pet_stats, sizeof(NurseryModel));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- NFC Reward Logic ---
void process_nfc_scavenge(FlipperMonApp* app) {
    FuriHalNfcDevData nfc_data;
    // Attempt to read the tag in the field
    if(furi_hal_nfc_detect(&nfc_data, 200)) {
        // Calculate a simple hash of the UID
        uint32_t uid_sum = 0;
        for(uint8_t i = 0; i < nfc_data.uid_len; i++) {
            uid_sum += nfc_data.uid[i];
        }

        uint8_t reward_roll = uid_sum % 10; // 0-9

        if(reward_roll == 7) { // Rare Candy (1 in 10 chance)
            app->pet_stats.level += 3;
            play_beep(1500.0f, 300);
            notification_message(app->notify, &sequence_blink_green_100);
        } else if(reward_roll == 3) { // Feast
            app->pet_stats.health = 100;
            play_beep(600.0f, 200);
            notification_message(app->notify, &sequence_blink_blue_100);
        } else if(reward_roll == 5) { // Super Toy
            app->pet_stats.happiness = 100;
            play_beep(900.0f, 200);
            notification_message(app->notify, &sequence_blink_magenta_100);
        } else { // Standard XP
            app->pet_stats.level++;
            play_beep(1200.0f, 100);
            notification_message(app->notify, &sequence_success);
        }
        save_game(app);
    } else {
        // No tag found
        notification_message(app->notify, &sequence_error);
    }
}

// --- View Callbacks ---
static void nursery_draw_callback(Canvas* canvas, void* model) {
    NurseryModel* data = model;
    if(!data) return;
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 12, data->name);

    const uint8_t* sprite = (data->level >= 5) ? yeti_large : yeti_small;
    canvas_draw_xbm(canvas, 56, 22 + data->y_offset, 16, 16, sprite);

    canvas_set_font(canvas, FontSecondary);
    char hud[64];
    snprintf(hud, sizeof(hud), "HP:%d  HAP:%d  LVL:%d", data->health, data->happiness, data->level);
    canvas_draw_str(canvas, 5, 50, hud);
    canvas_draw_str(canvas, 5, 60, "L: Feed  R: Play");
    if(data->y_offset < 0) data->y_offset += 1;
}

bool nursery_input_callback(InputEvent* event, void* context) {
    View* view = context;
    if(event->type == InputTypeShort) {
        with_view_model(view, NurseryModel* data, {
            if(event->key == InputKeyLeft) {
                data->health = (data->health < 100) ? data->health + 5 : 100;
                data->y_offset = -4;
                play_beep(440.0f, 50);
            } else if(event->key == InputKeyRight) {
                data->happiness = (data->happiness < 100) ? data->happiness + 10 : 100;
                data->y_offset = -6;
                play_beep(880.0f, 50);
            }
        }, true);
        return true;
    }
    return false;
}

void flippermon_menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == 0) {
        app->current_view = FlipperMonViewNursery;
        with_view_model(app->nursery_view, NurseryModel* data, {
            data->health = app->pet_stats.health;
            data->level = app->pet_stats.level;
            data->happiness = app->pet_stats.happiness;
            strncpy(data->name, app->pet_stats.name, 16);
        }, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewNursery);
    } else if(index == 1) { // Scavenge
        process_nfc_scavenge(app);
    }
}

// (App entry and boilerplate remains the same)
// ... [Navigation and Start functions from previous version] ...