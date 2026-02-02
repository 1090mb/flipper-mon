#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <furi_hal_nfc.h>
#include <furi_hal_infrared.h>
#include <storage/storage.h> // SD Card Support

#define SAVE_PATH EXT_PATH("apps_data/flippermon/save.dat")

// --- View Model ---
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
    NurseryModel pet_stats; // Master copy for saving
} FlipperMonApp;

// --- Sprites ---
static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t yeti_large[] = { 0x3C, 0x42, 0x99, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C, 0x42, 0x81, 0xBD, 0xBD, 0x81, 0x42, 0x3C, 0x3C, 0x42, 0x81, 0xA5, 0xA5, 0x81, 0x42, 0x3C };

// --- Audio ---
void play_beep(float freq, uint32_t duration) {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(freq, 1.0f);
        furi_delay_ms(duration);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// --- Persistence (SAVE/LOAD) ---
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

void load_game(FlipperMonApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool loaded = false;
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &app->pet_stats, sizeof(NurseryModel)) == sizeof(NurseryModel)) {
            loaded = true;
        }
    }
    if(!loaded) {
        app->pet_stats.health = 100;
        app->pet_stats.level = 1;
        app->pet_stats.happiness = 80;
        snprintf(app->pet_stats.name, 16, "%s-mon", furi_hal_version_get_name_ptr());
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- Rendering ---
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

// --- Input ---
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

// --- Logic/Menu ---
void flippermon_menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == 0) { // Enter Nursery
        app->current_view = FlipperMonViewNursery;
        with_view_model(app->nursery_view, NurseryModel* data, {
            data->health = app->pet_stats.health;
            data->level = app->pet_stats.level;
            data->happiness = app->pet_stats.happiness;
            strncpy(data->name, app->pet_stats.name, 16);
        }, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewNursery);
    } else if(index == 1) { // Scavenge
        if(furi_hal_nfc_field_is_present()) {
            app->pet_stats.level++;
            notification_message(app->notify, &sequence_success);
            play_beep(1200.0f, 150);
            save_game(app); // Save on Level Up
        }
    }
}

bool flippermon_back_event_callback(void* context) {
    FlipperMonApp* app = context;
    if(app->current_view == FlipperMonViewNursery) {
        with_view_model(app->nursery_view, NurseryModel* data, {
            app->pet_stats.health = data->health;
            app->pet_stats.happiness = data->happiness;
            app->pet_stats.level = data->level;
        }, false);
        save_game(app); // Save on exit from Nursery
        app->current_view = FlipperMonViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);
        return true; 
    }
    return false;
}

// --- App Entry ---
int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    
    load_game(app); // Load SD Card data at Start
    app->current_view = FlipperMonViewSubmenu;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, flippermon_back_event_callback);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Flipper-Mon");
    submenu_add_item(app->submenu, "Nursery", 0, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "Scavenge (NFC)", 1, flippermon_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewSubmenu, submenu_get_view(app->submenu));

    app->nursery_view = view_alloc();
    view_allocate_model(app->nursery_view, ViewModelTypeLockFree, sizeof(NurseryModel));
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_input_callback(app->nursery_view, nursery_input_callback);
    view_set_context(app->nursery_view, app->nursery_view);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewNursery, app->nursery_view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);

    view_dispatcher_run(app->view_dispatcher);

    save_game(app); // Final Save at App Closure

    view_dispatcher_remove_view(app->view_dispatcher, FlipperMonViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperMonViewNursery);
    submenu_free(app->submenu);
    view_free(app->nursery_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}