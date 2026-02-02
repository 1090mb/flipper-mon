#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <furi_hal_nfc.h>
#include <furi_hal_infrared.h>

// --- App Types ---
typedef enum {
    FlipperMonViewSubmenu,
    FlipperMonViewNursery,
} FlipperMonView;

typedef struct {
    uint8_t health;
    uint8_t level;
    char name[12];
} Creature;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* nursery_view;
    NotificationApp* notify;
    Creature pet;
    uint32_t current_view;
} FlipperMonApp;

static const uint8_t yeti_sprite[] = {
    0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 
    0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00,
    0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C
};

static void nursery_draw_callback(Canvas* canvas, void* model) {
    Creature* pet = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "NURSERY");
    canvas_draw_xbm(canvas, 48, 22, 16, 16, yeti_sprite);
    
    canvas_set_font(canvas, FontSecondary);
    char stats[64];
    snprintf(stats, sizeof(stats), "LVL: %d | HP: %d", pet->level, pet->health);
    canvas_draw_str(canvas, 2, 54, stats);
    canvas_draw_str(canvas, 2, 63, "Press BACK to Menu");
}

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
            app->pet.level++;
            notification_message(app->notify, &sequence_success);
        } else {
            notification_message(app->notify, &sequence_blink_yellow_100);
        }
    } else if(index == FlipperMonMenuAttack) {
        // We go back to the most stable IR toggle for 2026
        furi_hal_infrared_set_tx_output(true);
        furi_delay_ms(50);
        furi_hal_infrared_set_tx_output(false);
        notification_message(app->notify, &sequence_blink_red_100);
    }
}

// Navigation Callback must return BOOL
// True = Event handled, False = Pass to system (which usually exits)
bool flippermon_back_event_callback(void* context) {
    FlipperMonApp* app = context;
    if(app->current_view == FlipperMonViewNursery) {
        app->current_view = FlipperMonViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);
        return true; 
    }
    return false; // This will trigger the app exit
}

int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    app->pet.health = 100;
    app->pet.level = 1;
    app->current_view = FlipperMonViewSubmenu;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, flippermon_back_event_callback);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Flipper-Mon");
    submenu_add_item(app->submenu, "Nursery", FlipperMonMenuNursery, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "NFC Scavenge", FlipperMonMenuScavenge, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "IR Attack", FlipperMonMenuAttack, flippermon_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewSubmenu, submenu_get_view(app->submenu));

    app->nursery_view = view_alloc();
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_context(app->nursery_view, &app->pet);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewNursery, app->nursery_view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);

    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
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