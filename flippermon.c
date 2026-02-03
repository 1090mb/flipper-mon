#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <math.h>

#define SAVE_PATH EXT_PATH("apps_data/flippermon/save.dat")

// --- Constants & Enums ---
typedef enum { SpeciesYeti, SpeciesGlitch, SpeciesSpore, SpeciesDragon } MonSpecies;
typedef enum { FlipperMonEventTick } FlipperMonCustomEvent;

typedef struct {
    uint8_t health, level, happiness;
    MonSpecies species;
    uint32_t lifetime_scans;
    int8_t y_offset;
    float bounce_phase; 
    char name[20];
} NurseryModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu *submenu, *starter_menu;
    View *nursery_view, *scan_view;
    VariableItemList* stats_list;
    DialogEx* dialog;
    NotificationApp* notify;
    FuriTimer* timer;
    NurseryModel pet_stats;
    uint32_t current_view;
    uint8_t scan_frame, level_up_timer;
} FlipperMonApp;

// --- Sprites ---
static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t glitch_small[] = { 0xFF, 0x81, 0xBD, 0xA5, 0xA5, 0xBD, 0x81, 0xFF, 0x18, 0x3C, 0x3C, 0x18, 0xFF, 0x81, 0x81, 0xFF };
static const uint8_t spore_small[] = { 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C, 0x10, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t dragon_small[] = { 0x24, 0x66, 0xFF, 0xFF, 0xBD, 0x81, 0x42, 0x3C, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x66, 0x24, 0x00 };

static const uint8_t evo_horns[] = { 0x41, 0x22, 0x14, 0x08 }; 
static const uint8_t evo_flower[] = { 0x10, 0x38, 0x10, 0x00 }; 
static const uint8_t evo_pixels[] = { 0x82, 0x00, 0x28, 0x00, 0x01, 0x40 };

// --- Hardware Abstraction ---
void play_sound(char type) {
    if(!furi_hal_speaker_is_mine() && !furi_hal_speaker_acquire(500)) return;
    if(type == 's') { // Success
        furi_hal_speaker_start(600.0f, 1.0f); furi_delay_ms(50);
        furi_hal_speaker_start(900.0f, 1.0f); furi_delay_ms(100);
    } else if(type == 'f') { // Fail
        furi_hal_speaker_start(200.0f, 1.0f); furi_delay_ms(200);
    } else if(type == 'c') { // Chomp
        furi_hal_speaker_start(300.0f, 1.0f); furi_delay_ms(30); furi_hal_speaker_stop(); furi_delay_ms(20); furi_hal_speaker_start(250.0f, 1.0f); furi_delay_ms(40);
    } else if(type == 'b') { // Boing
        for(float i = 400; i < 800; i += 40) { furi_hal_speaker_start(i, 1.0f); furi_delay_ms(10); }
    }
    furi_hal_speaker_stop(); furi_hal_speaker_release();
}

void save_game(FlipperMonApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, EXT_PATH("apps_data/flippermon"));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->pet_stats, sizeof(NurseryModel));
    }
    storage_file_close(file); storage_file_free(file); furi_record_close(RECORD_STORAGE);
}

// --- NFC Core ---
void perform_safe_scavenge(FlipperMonApp* app) {
    Nfc* nfc = furi_record_open("nfc");
    if(!nfc) return;
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_4a);
    if(!poller) { furi_record_close("nfc"); return; }
    
    nfc_poller_start(poller, NULL, NULL);
    furi_delay_ms(500); 
    bool detected = (nfc_poller_get_protocol(poller) != NfcProtocolInvalid);
    nfc_poller_stop(poller); nfc_poller_free(poller); furi_record_close("nfc");

    if(detected) {
        uint8_t gain = ((furi_get_tick() % 100) < 5) ? 5 : 1;
        app->pet_stats.level += gain;
        app->pet_stats.lifetime_scans++;
        app->level_up_timer = 20; 
        play_sound('s');
        notification_message(app->notify, (gain == 5) ? &sequence_blink_magenta_100 : &sequence_success);
        save_game(app);
    } else {
        play_sound('f'); notification_message(app->notify, &sequence_error);
    }
}

// --- Drawing Callbacks ---
static void nursery_draw_callback(Canvas* canvas, void* model) {
    NurseryModel* data = model;
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 12, data->name);
    
    canvas_set_font(canvas, FontSecondary);
    if(data->health < 20) canvas_draw_str(canvas, 5, 22, "Starving");
    else if(data->happiness < 20) canvas_draw_str(canvas, 5, 22, "Bored");
    else canvas_draw_str(canvas, 5, 22, "Content");

    int8_t breathe = (data->health < 20) ? (rand() % 2) : (int8_t)(sinf(data->bounce_phase) * 1.5f);
    int16_t x = 56, y = 24 + data->y_offset + breathe;
    
    const uint8_t* sprite = (data->species == SpeciesGlitch) ? glitch_small : (data->species == SpeciesSpore) ? spore_small : (data->species == SpeciesDragon ? dragon_small : yeti_small);
    canvas_draw_xbm(canvas, x, y, 16, 16, sprite);
    
    if(data->level >= 5) {
        if(data->species == SpeciesYeti) canvas_draw_xbm(canvas, x + 4, y - 4, 8, 4, evo_horns);
        else if(data->species == SpeciesSpore) canvas_draw_xbm(canvas, x + 4, y - 3, 8, 4, evo_flower);
        else if(data->species == SpeciesGlitch) canvas_draw_xbm(canvas, x - 2, y - 2, 20, 20, evo_pixels);
    }

    char stats[64];
    snprintf(stats, sizeof(stats), "HP:%d HAP:%d LVL:%d", data->health, data->happiness, data->level);
    canvas_draw_str(canvas, 5, 52, stats);
    canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, "L: Feed R: Play Back: Exit");
    if(data->y_offset < 0) data->y_offset += 1;
}

static void scan_draw_callback(Canvas* canvas, void* model) {
    FlipperMonApp* app = (FlipperMonApp*)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    if(app->level_up_timer > 0) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "!!! LEVEL UP !!!");
        canvas_draw_frame(canvas, 20, 20, 88, 24);
    } else {
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "Searching...");
        canvas_draw_circle(canvas, 64, 40, (app->scan_frame * 4) % 30);
    }
}

// --- Navigation & Logic ---
void exit_dialog_callback(DialogExResult result, void* context) {
    FlipperMonApp* app = context;
    if(result == DialogExResultLeft) {
        app->current_view = 0; view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    } else if(result == DialogExResultRight) {
        view_dispatcher_stop(app->view_dispatcher);
    }
}

bool back_callback(void* context) {
    FlipperMonApp* app = context;
    if(app->current_view == 0) {
        app->current_view = 6;
        dialog_ex_set_header(app->dialog, "Exit Flippermon?", 64, 10, AlignCenter, AlignTop);
        dialog_ex_set_text(app->dialog, "Your Mon will miss you!", 64, 32, AlignCenter, AlignCenter);
        dialog_ex_set_left_button_text(app->dialog, "Stay");
        dialog_ex_set_right_button_text(app->dialog, "Exit");
        dialog_ex_set_result_callback(app->dialog, exit_dialog_callback);
        dialog_ex_set_context(app->dialog, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, 6);
        return true;
    }
    app->current_view = 0; view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    return true;
}

bool nursery_input(InputEvent* event, void* context) {
    FlipperMonApp* app = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) return false;
        with_view_model(app->nursery_view, NurseryModel* data, {
            if(event->key == InputKeyLeft) { data->health = (data->health <= 95) ? data->health + 5 : 100; data->y_offset = -4; play_sound('c'); }
            if(event->key == InputKeyRight) { data->happiness = (data->happiness <= 90) ? data->happiness + 10 : 100; data->y_offset = -6; play_sound('b'); }
            app->pet_stats.health = data->health; app->pet_stats.happiness = data->happiness;
        }, true);
    }
    return false;
}

void starter_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    app->pet_stats.species = (MonSpecies)index;
    app->pet_stats.health = 100; app->pet_stats.level = 1; app->pet_stats.happiness = 100;
    snprintf(app->pet_stats.name, 20, "Monster-%ld", index);
    save_game(app);
    app->current_view = 0; view_dispatcher_switch_to_view(app->view_dispatcher, 0);
}

void menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == 0) {
        app->current_view = 1;
        with_view_model(app->nursery_view, NurseryModel* data, { memcpy(data, &app->pet_stats, sizeof(NurseryModel)); }, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, 1);
    } else if(index == 1) {
        app->current_view = 5; view_dispatcher_switch_to_view(app->view_dispatcher, 5);
        for(uint8_t i = 0; i < 20; i++) { app->scan_frame = i; furi_delay_ms(80); }
        perform_safe_scavenge(app);
        app->current_view = 0; view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    } else if(index == 2) {
        variable_item_list_reset(app->stats_list);
        char buf[32]; snprintf(buf, sizeof(buf), "Level: %d", app->pet_stats.level);
        variable_item_list_add(app->stats_list, buf, 0, NULL, NULL);
        app->current_view = 2; view_dispatcher_switch_to_view(app->view_dispatcher, 2);
    }
}

static void timer_callback(void* context) {
    FlipperMonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipperMonEventTick);
}

bool custom_event_callback(void* context, uint32_t event) {
    FlipperMonApp* app = context;
    if(event == FlipperMonEventTick) {
        if(app->level_up_timer > 0) app->level_up_timer--;
        with_view_model(app->nursery_view, NurseryModel* data, { 
            data->bounce_phase += 0.15f; 
            static uint16_t decay = 0;
            if(++decay >= 300) {
                decay = 0;
                if(app->pet_stats.health > 0) app->pet_stats.health--;
                if(app->pet_stats.happiness > 0) app->pet_stats.happiness--;
                data->health = app->pet_stats.health; data->happiness = app->pet_stats.happiness;
            }
        }, true);
        return true;
    }
    return false;
}

// --- Entry Point ---
int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &app->pet_stats, sizeof(NurseryModel));
    } else { memset(&app->pet_stats, 0, sizeof(NurseryModel)); }
    storage_file_close(file); storage_file_free(file); furi_record_close(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Flippermon");
    submenu_add_item(app->submenu, "Nursery", 0, menu_callback, app);
    submenu_add_item(app->submenu, "Scavenge", 1, menu_callback, app);
    submenu_add_item(app->submenu, "Stats", 2, menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));

    app->nursery_view = view_alloc();
    view_allocate_model(app->nursery_view, ViewModelTypeLockFree, sizeof(NurseryModel));
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_input_callback(app->nursery_view, nursery_input);
    view_set_context(app->nursery_view, app);
    view_dispatcher_add_view(app->view_dispatcher, 1, app->nursery_view);

    app->stats_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, 2, variable_item_list_get_view(app->stats_list));

    app->starter_menu = submenu_alloc();
    submenu_set_header(app->starter_menu, "Choose Species");
    submenu_add_item(app->starter_menu, "Frost Yeti", SpeciesYeti, starter_callback, app);
    submenu_add_item(app->starter_menu, "Cyber Glitch", SpeciesGlitch, starter_callback, app);
    submenu_add_item(app->starter_menu, "Nature Spore", SpeciesSpore, starter_callback, app);
    if(app->pet_stats.level >= 50) submenu_add_item(app->starter_menu, "VOID DRAGON", SpeciesDragon, starter_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, 4, submenu_get_view(app->starter_menu));

    app->scan_view = view_alloc();
    view_set_draw_callback(app->scan_view, scan_draw_callback);
    view_set_context(app->scan_view, app);
    view_dispatcher_add_view(app->view_dispatcher, 5, app->scan_view);

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, 6, dialog_ex_get_view(app->dialog));

    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, (app->pet_stats.level == 0) ? 4 : 0);
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    furi_timer_stop(app->timer); furi_timer_free(app->timer);
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_dispatcher_remove_view(app->view_dispatcher, 1);
    view_dispatcher_remove_view(app->view_dispatcher, 2);
    view_dispatcher_remove_view(app->view_dispatcher, 4);
    view_dispatcher_remove_view(app->view_dispatcher, 5);
    view_dispatcher_remove_view(app->view_dispatcher, 6);
    submenu_free(app->submenu); submenu_free(app->starter_menu);
    view_free(app->nursery_view); view_free(app->scan_view); dialog_ex_free(app->dialog);
    variable_item_list_free(app->stats_list);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI); furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}