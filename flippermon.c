#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <furi_hal_nfc.h>
#include <furi_hal_infrared.h>

// --- App Types & View IDs ---
typedef enum {
    FlipperMonViewSubmenu,
    FlipperMonViewNursery,
} FlipperMonView;

// The "Model" - Data that lives inside the View
typedef struct {
    uint8_t health;
    uint8_t level;
    uint8_t happiness;
    int8_t y_offset;
    uint32_t last_tick;
    char name[12];
} Creature;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* nursery_view;
    NotificationApp* notify;
    uint32_t current_view;
    // We keep a pointer here to access the model from the menu
    Creature* global_pet_ptr; 
} FlipperMonApp;

// --- 16x16 Yeti Sprite ---
static const uint8_t yeti_sprite[] = {
    0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 
    0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00,
    0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C
};

// --- Nursery Drawing Logic ---
static void nursery_draw_callback(Canvas* canvas, void* model) {
    Creature* pet = model;
    if(!pet) return;

    // Gravity & Hunger Logic
    if(pet->y_offset < 0) pet->y_offset += 1;
    uint32_t now = furi_get_tick();
    if(now - pet->last_tick > 30000) {
        if(pet->health > 0) pet->health--;
        pet->last_tick = now;
    }

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 12, "NURSERY");
    canvas_draw_xbm(canvas, 56, 22 + pet->y_offset, 16, 16, yeti_sprite);
    
    canvas_set_font(canvas, FontSecondary);
    char stats[64];
    snprintf(stats, sizeof(stats), "HP:%d  HAP:%d  LVL:%d", pet->health, pet->happiness, pet->level);
    canvas_draw_str(canvas, 5, 58, stats);
    canvas_draw_str(canvas, 75, 12, "L:Feed R:Play");
}

// --- Nursery Input Logic ---
bool nursery_input_callback(InputEvent* event, void* context) {
    View* view = context;
    if(event->type == InputTypeShort) {
        with_view_model(view, Creature* pet, {
            if(event->key == InputKeyLeft) {
                if(pet->health < 100) pet->health += 5;
                pet->y_offset = -4;
            } else if(event->key == InputKeyRight) {
                if(pet->happiness < 100) pet->happiness += 10;
                pet->y_offset = -6;
            }
        }, true);
    }
    return false; 
}

// --- Submenu Callbacks ---
typedef enum {
    FlipperMonMenuNursery,
    FlipperMonMenuScavenge,
    FlipperMonMenuAttack,
} FlipperMonMenuIndex;

void flippermon_menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == FlipperMonMenuNursery) {
        app->current_view = FlipperMonViewNursery;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewNursery);
    } else if(index == FlipperMonMenuScavenge) {
        if(furi_hal_nfc_field_is_present()) {
            // Update the model directly
            with_view_model(app->nursery_view, Creature* pet, { pet->level++; }, true);
            notification_message(app->notify, &sequence_success);
        } else {
            notification_message(app->notify, &sequence_blink_yellow_100);
        }
    } else if(index == FlipperMonMenuAttack) {
        furi_hal_infrared_set_tx_output(true);
        furi_delay_ms(50);
        furi_hal_infrared_set_tx_output(false);
        notification_message(app->notify, &sequence_blink_red_100);
    }
}

// --- Back Button Logic ---
bool flippermon_back_event_callback(void* context) {
    FlipperMonApp* app = context;
    if(app->current_view == FlipperMonViewNursery) {
        app->current_view = FlipperMonViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);
        return true; 
    }
    return false; // Exit App
}

// --- Main App Setup ---
int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    app->current_view = FlipperMonViewSubmenu;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, flippermon_back_event_callback);

    // 1. Submenu View
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Flipper-Mon");
    submenu_add_item(app->submenu, "Enter Nursery", FlipperMonMenuNursery, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "NFC Scavenge", FlipperMonMenuScavenge, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "IR Attack", FlipperMonMenuAttack, flippermon_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewSubmenu, submenu_get_view(app->submenu));

    // 2. Nursery View with Internal Model
    app->nursery_view = view_alloc();
    view_allocate_model(app->nursery_view, ViewModelTypeLockFree, sizeof(Creature));
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_input_callback(app->nursery_view, nursery_input_callback);
    view_set_context(app->nursery_view, app->nursery_view);

    // Initialize the Model Data safely
    with_view_model(app->nursery_view, Creature* pet, {
        pet->health = 100;
        pet->happiness = 80;
        pet->level = 1;
        pet->y_offset = 0;
        pet->last_tick = furi_get_tick();
        strncpy(pet->name, "YETI", 12);
    }, true);

    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewNursery, app->nursery_view);

    // Start UI
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);

    view_dispatcher_run(app->view_dispatcher);

    // --- Cleanup ---
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