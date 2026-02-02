#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <furi_hal_nfc.h>
#include <furi_hal_infrared.h>

typedef enum {
    FlipperMonViewSubmenu,
    FlipperMonViewNursery,
} FlipperMonView;

// The Model: Data that is "owned" by the View for safe drawing
typedef struct {
    uint8_t health;
    uint8_t level;
    uint8_t happiness;
    int8_t y_offset;
    bool evolving;
} NurseryModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* nursery_view;
    NotificationApp* notify;
    uint32_t current_view;
    NurseryModel global_stats; // Master stats
} FlipperMonApp;

static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t yeti_large[] = { 0x3C, 0x42, 0x99, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C, 0x42, 0x81, 0xBD, 0xBD, 0x81, 0x42, 0x3C, 0x3C, 0x42, 0x81, 0xA5, 0xA5, 0x81, 0x42, 0x3C };

static void nursery_draw_callback(Canvas* canvas, void* model) {
    NurseryModel* data = model;
    if(!data) return;

    canvas_clear(canvas);
    
    // Level Up Flash
    if(data->evolving) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "EVOLVING!");
        return;
    }

    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 12, "NURSERY");

    const uint8_t* sprite = (data->level >= 5) ? yeti_large : yeti_small;
    canvas_draw_xbm(canvas, 56, 25 + data->y_offset, 16, 16, sprite);

    canvas_set_font(canvas, FontSecondary);
    char stats[64];
    snprintf(stats, sizeof(stats), "HP:%d HAP:%d LVL:%d", data->health, data->happiness, data->level);
    canvas_draw_str(canvas, 5, 58, stats);

    if(data->y_offset < 0) data->y_offset += 1;
}

bool nursery_input_callback(InputEvent* event, void* context) {
    View* view = context;
    if(event->type == InputTypeShort) {
        with_view_model(view, NurseryModel* data, {
            if(event->key == InputKeyLeft) data->health = (data->health < 100) ? data->health + 5 : 100;
            if(event->key == InputKeyRight) data->happiness = (data->happiness < 100) ? data->happiness + 10 : 100;
            data->y_offset = -4;
        }, true);
        return true;
    }
    return false;
}

void flippermon_menu_callback(void* context, uint32_t index) {
    FlipperMonApp* app = context;
    if(index == 0) {
        app->current_view = FlipperMonViewNursery;
        // Sync global stats to the view model before switching
        with_view_model(app->nursery_view, NurseryModel* data, {
            data->health = app->global_stats.health;
            data->level = app->global_stats.level;
            data->happiness = app->global_stats.happiness;
        }, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewNursery);
    } else if(index == 1) { // Scavenge
        if(furi_hal_nfc_field_is_present()) {
            app->global_stats.level++;
            notification_message(app->notify, &sequence_success);
            
            // Trigger Evolution Animation if hitting level 5
            if(app->global_stats.level == 5) {
                app->global_stats.evolving = true;
                notification_message(app->notify, &sequence_display_backlight_on);
            }
        }
    }
}

bool flippermon_back_event_callback(void* context) {
    FlipperMonApp* app = context;
    if(app->current_view == FlipperMonViewNursery) {
        // Sync stats back to global before leaving
        with_view_model(app->nursery_view, NurseryModel* data, {
            app->global_stats.health = data->health;
            app->global_stats.happiness = data->happiness;
            app->global_stats.level = data->level;
        }, false);
        app->current_view = FlipperMonViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperMonViewSubmenu);
        return true; 
    }
    return false;
}

int32_t flippermon_app(void* p) {
    UNUSED(p);
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    
    // Initial Stats
    app->global_stats.health = 100;
    app->global_stats.level = 1;
    app->global_stats.happiness = 50;
    app->global_stats.evolving = false;
    app->current_view = FlipperMonViewSubmenu;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, flippermon_back_event_callback);

    // Submenu
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Flipper-Mon");
    submenu_add_item(app->submenu, "Nursery", 0, flippermon_menu_callback, app);
    submenu_add_item(app->submenu, "Scavenge (NFC)", 1, flippermon_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, FlipperMonViewSubmenu, submenu_get_view(app->submenu));

    // Nursery View with allocated Model
    app->nursery_view = view_alloc();
    view_allocate_model(app->nursery_view, ViewModelTypeLockFree, sizeof(NurseryModel));
    view_set_draw_callback(app->nursery_view, nursery_draw_callback);
    view_set_input_callback(app->nursery_view, nursery_input_callback);
    view_set_context(app->nursery_view, app->nursery_view); // Context is the view itself for the model
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