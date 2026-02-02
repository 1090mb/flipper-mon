#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>

#define SAVE_PATH EXT_PATH("apps_data/flippermon/save.dat")

// Events for the main loop
typedef enum {
    FlipperMonEventScavenge,
} FlipperMonCustomEvent;

// The data structure for our pet
typedef struct {
    uint8_t health;
    uint8_t level;
    uint8_t happiness;
    int8_t y_offset;
    char name[16];
} NurseryModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* nursery_view;
    NotificationApp* notify;
    NurseryModel pet_stats;
} FlipperMonApp;

// --- Sprites ---
static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t yeti_large[] = { 0x3C, 0x42, 0x99, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C, 0x42, 0x81, 0xBD, 0xBD, 0x81, 0x42, 0x3C, 0x3C, 0x42, 0x81, 0xA5, 0xA5, 0x81, 0x42, 0x3C };

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

void load_game(FlipperMonApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool loaded = false;
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &app->pet_stats, sizeof(NurseryModel)) == sizeof(NurseryModel)) loaded = true;
    }
    if(!loaded) {
        app->pet_stats.health = 100; app->pet_stats.level = 1; app->pet_stats.happiness = 80;
        snprintf(app->pet_stats.name, 16, "%s-mon", furi_hal_version_get_name_ptr());
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- NFC Logic ---
void perform_safe_scavenge(FlipperMonApp* app) {
    Nfc* nfc = furi_record_open("nfc");
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolInvalid);
    nfc_poller_start(poller, NULL, NULL);
    
    furi_delay_ms(250); 
    bool detected = (nfc_poller_get_protocol(poller) != NfcProtocolInvalid);
    
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_record_close("nfc");
    furi_delay_ms(100);

    if(detected) {
        app->pet_stats.level++;
        notification_message(app->notify, &sequence_success);
        save_game(app);
    } else {
        notification_message(app->notify, &sequence_error);
    }
}

// --- Nursery Drawing ---
static void nursery_draw_callback(Canvas* canvas, void* model) {
    NurseryModel* data = model;
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 12, data->name);
    
    // Select sprite based on level
    const uint8_t* sprite = (data->level >= 5) ? yeti_large : yeti_small;
    canvas_draw_xbm(canvas, 56, 22 + data->y_offset, 16, 16, sprite);
    
    canvas_set_font(canvas, FontSecondary);
    char stats[64];
    snprintf(stats, sizeof(stats), "HP:%d  HAP:%d  LVL:%d", data->health, data->happiness, data->level);
    canvas_draw_str(canvas, 5, 52, stats);
    canvas_draw_str(canvas, 5, 61, "L: Feed  R: Play");

    if(data->y_offset < 0) data->y_offset += 1; // Animation gravity
}

// --- Input Handling ---
bool nursery_input_callback(InputEvent* event, void* context) {
    FlipperMonApp* app = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            view_dispatcher_switch_to_view(app->view_dispatcher, 0);
            return true;
        }
        with_view_model(app->nursery_view, NurseryModel* data, {
            if(event->key == InputKeyLeft) {
                data->health = (data->health < 100) ? data->health + 5 : 100;
                data->y_offset = -4;
            }
            if(event->key == InputKeyRight) {
                data->happiness = (data->happiness < 100) ? data->happiness + 10 : 100;
                data->y_offset = -6;
            }
            // Sync back to main stats
            app->pet_stats.health = data->health;
            app->pet_stats.happiness = data->happiness;
        }, true);
    }
    return false;
}

// --- Submenu ---
void flippermon_menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == 0) {
        with_view_model(app->nursery_view, NurseryModel* data, {
            memcpy(data, &app->pet_stats, sizeof(NurseryModel));
        }, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, 1);
    } else if(index == 1) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipperMonEventScavenge);
    }
}

bool flippermon_custom_event_callback(void* context, uint32_t event) {
    FlipperMonApp* app = context;
    if(event == FlipperMonEventScavenge) {
        perform_safe_scavenge(app);
        return true;
    }
    return false;
}

// --- App Entry ---
int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    load_game(app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flippermon_custom_event_callback);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Flipper-Mon");
    submenu_add_item(app->submenu, "Nursery", 0, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "Scavenge (NFC)", 1, flippermon_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));

    app->nursery_view = view_alloc();
    view_allocate_model(app->nursery_view, ViewModelTypeLockFree, sizeof(NurseryModel));
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_input_callback(app->nursery_view, nursery_input_callback);
    view_set_context(app->nursery_view, app); 
    view_dispatcher_add_view(app->view_dispatcher, 1, app->nursery_view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    view_dispatcher_run(app->view_dispatcher);

    save_game(app);
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_dispatcher_remove_view(app->view_dispatcher, 1);
    submenu_free(app->submenu);
    view_free(app->nursery_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}