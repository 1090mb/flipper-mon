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

typedef enum {
    FlipperMonViewSubmenu,
    FlipperMonViewNursery,
} FlipperMonView;

typedef struct {
    uint8_t health;
    uint8_t level;
    uint8_t happiness;
    int8_t y_offset;
    char name[16]; // Increased for unique names
} Creature;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* nursery_view;
    NotificationApp* notify;
    uint32_t current_view;
    Creature pet;
    FuriMutex* mutex;
} FlipperMonApp;

static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t yeti_large[] = { 0x3C, 0x42, 0x99, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C, 0x42, 0x81, 0xBD, 0xBD, 0x81, 0x42, 0x3C, 0x3C, 0x42, 0x81, 0xA5, 0xA5, 0x81, 0x42, 0x3C };

// --- Audio Functions ---
void play_happy_beeps() {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(440.0f, 1.0f); // A4
        furi_delay_ms(50);
        furi_hal_speaker_start(880.0f, 1.0f); // A5
        furi_delay_ms(100);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

void play_level_up_fanfare() {
    float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f}; // C5, E5, G5, C6
    if(furi_hal_speaker_acquire(1000)) {
        for(int i = 0; i < 4; i++) {
            furi_hal_speaker_start(notes[i], 1.0f);
            furi_delay_ms(100);
        }
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// --- Unique Name Generator ---
void generate_pet_name(char* name_out) {
    const char* flipper_name = furi_hal_version_get_name_ptr();
    if(flipper_name) {
        snprintf(name_out, 16, "%s-mon", flipper_name);
    } else {
        strncpy(name_out, "Yeti-mon", 16);
    }
}

// --- Storage Logic ---
void save_game(FlipperMonApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, EXT_PATH("apps_data/flippermon"));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->pet, sizeof(Creature));
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
        if(storage_file_read(file, &app->pet, sizeof(Creature)) == sizeof(Creature)) {
            loaded = true;
        }
    }
    
    if(!loaded) {
        app->pet.health = 100;
        app->pet.level = 1;
        app->pet.happiness = 80;
        generate_pet_name(app->pet.name);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- UI Callbacks ---
static void nursery_draw_callback(Canvas* canvas, void* context) {
    FlipperMonApp* app = context;
    if(!app || !app->mutex || furi_mutex_acquire(app->mutex, 0) != FuriStatusOk) return;

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 12, app->pet.name);

    const uint8_t* sprite = (app->pet.level >= 5) ? yeti_large : yeti_small;
    canvas_draw_xbm(canvas, 56, 22 + app->pet.y_offset, 16, 16, sprite);
    
    canvas_set_font(canvas, FontSecondary);
    char stats[64];
    snprintf(stats, sizeof(stats), "HP:%d HAP:%d LVL:%d", app->pet.health, app->pet.happiness, app->pet.level);
    canvas_draw_str(canvas, 5, 58, stats);

    if(app->pet.y_offset < 0) app->pet.y_offset += 1;
    furi_mutex_release(app->mutex);
}

bool nursery_input_callback(InputEvent* event, void* context) {
    FlipperMonApp* app = context;
    if(event->type == InputTypeShort) {
        if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
            if(event->key == InputKeyLeft && app->pet.health < 100) {
                app->pet.health += 5;
                play_happy_beeps();
            }
            app->pet.y_offset = -4;
            furi_mutex_release(app->mutex);
            return true;
        }
    }
    return false;
}

void flippermon_menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == 0) {
        app->current_view = FlipperMonViewNursery;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewNursery);
    } else if(index == 1) { // Scavenge
        if(furi_hal_nfc_field_is_present()) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->pet.level++;
            furi_mutex_release(app->mutex);
            play_level_up_fanfare();
            notification_message(app->notify, &sequence_success);
            save_game(app);
        }
    }
}

// (Navigation and Main App entry remain standard)
bool flippermon_back_event_callback(void* context) {
    FlipperMonApp* app = context;
    if(app->current_view == FlipperMonViewNursery) {
        app->current_view = FlipperMonViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);
        return true; 
    }
    return false;
}

int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    
    load_game(app);
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
    view_set_context(app->nursery_view, app);
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_input_callback(app->nursery_view, nursery_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewNursery, app->nursery_view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);

    view_dispatcher_run(app->view_dispatcher);

    save_game(app);
    // Cleanup code (same as before)
    view_dispatcher_remove_view(app->view_dispatcher, FlipperMonViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperMonViewNursery);
    submenu_free(app->submenu);
    view_free(app->nursery_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}