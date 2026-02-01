#include <furi.h>
#include <furi_hal.h> // This is the "Master" HAL include
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_infrared.h>
#include <furi_hal_nfc.h>

// --- Game Definitions ---
typedef enum {
    StateNursery,
    StateBattle,
} GameState;

typedef struct {
    uint8_t health;
    uint8_t level;
    char name[12];
} Creature;

typedef struct {
    FuriMutex* mutex;
    GameState state;
    Creature pet;
    NotificationApp* notify;
} FlipperMonApp;

// --- Rendering ---
static void render_callback(Canvas* canvas, void* ctx) {
    FlipperMonApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "FLIPPER-MON");

    if(app->state == StateNursery) {
        // Draw the pet
        canvas_draw_frame(canvas, 40, 20, 48, 32);
        canvas_draw_circle(canvas, 55, 30, 2); // Eye
        canvas_draw_circle(canvas, 73, 30, 2); // Eye
        
        canvas_set_font(canvas, FontSecondary);
        char stats[32];
        snprintf(stats, sizeof(stats), "%s | LV: %d | HP: %d", app->pet.name, app->pet.level, app->pet.health);
        canvas_draw_str(canvas, 2, 60, stats);
    }

    furi_mutex_release(app->mutex);
}

// --- Hardware Hook: NFC Scavenge ---
void scavenge_nfc(FlipperMonApp* app) {
    // Current SDK uses a Polled check for detection
    bool detected = furi_hal_nfc_detect(NULL, 100); 
    
    // Note: If the error persists, some firmware versions require 
    // a 'NfcDevice' pointer instead of NULL.
    if(detected) { 
        app->pet.level++;
        notification_message(app->notify, &sequence_success);
    } else {
        notification_message(app->notify, &sequence_blink_yellow_100);
    }
}

// --- Hardware Hook: IR Attack ---
void send_ir_attack(FlipperMonApp* app) {
    // Current SDK requires the frequency to be defined for transmit
    uint32_t timings[] = {1000, 500, 1000, 500};
    size_t timings_cnt = sizeof(timings) / sizeof(uint32_t);
    uint32_t frequency = 38000; // Standard 38kHz IR frequency

    if(!furi_hal_infrared_is_busy()) {
        // Updated to use the explicit transmit function
        furi_hal_infrared_transmit(frequency, timings, timings_cnt);
    }
    
    notification_message(app->notify, &sequence_blink_red_100);
}

// --- Main App Entry ---
int32_t flippermon_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FlipperMonApp* app = malloc(sizeof(FlipperMonApp));
    
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->state = StateNursery;
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    
    // Initial Stats
    app->pet.health = 100;
    app->pet.level = 1;
    strncpy(app->pet.name, "YETI", 12);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, app);
    view_port_input_callback_set(view_port, (void*)furi_message_queue_put, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        if(event.type == InputTypeShort) {
            if(event.key == InputKeyBack) break;
            if(event.key == InputKeyOk) scavenge_nfc(app); // Press OK to "Scan NFC"
            if(event.key == InputKeyUp) send_ir_attack(app); // Press UP to shoot IR
        }
        view_port_update(view_port);
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
