// Final Flipper-Mon v1.0 - The Dragon Update
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <math.h>

#define SAVE_PATH EXT_PATH("apps_data/flippermon/save.dat")

typedef enum { SpeciesYeti, SpeciesGlitch, SpeciesSpore, SpeciesDragon } MonSpecies;
typedef enum { FlipperMonEventTick } FlipperMonCustomEvent;

typedef struct {
    uint8_t health, level, happiness;
    MonSpecies species;
    uint32_t lifetime_scans, birth_timestamp;
    int8_t y_offset;
    float bounce_phase; 
    char name[20];
} NurseryModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu *submenu, *starter_menu;
    View *nursery_view, *scan_view;
    VariableItemList* stats_list;
    NotificationApp* notify;
    FuriTimer* timer;
    NurseryModel pet_stats;
    uint32_t current_view;
    uint8_t scan_frame, level_up_timer;
} FlipperMonApp;

// --- Graphics ---
static const uint8_t yeti_small[] = { 0x00, 0x7E, 0x00, 0x81, 0x24, 0x81, 0x81, 0x00, 0x81, 0x42, 0x81, 0x3C, 0x81, 0x00, 0x7E, 0x00, 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
static const uint8_t glitch_small[] = { 0xFF, 0x81, 0xBD, 0xA5, 0xA5, 0xBD, 0x81, 0xFF, 0x18, 0x3C, 0x3C, 0x18, 0xFF, 0x81, 0x81, 0xFF };
static const uint8_t spore_small[] = { 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C, 0x10, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t dragon_small[] = { 0x24, 0x66, 0xFF, 0xFF, 0xBD, 0x81, 0x42, 0x3C, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x66, 0x24, 0x00 };

// --- Functionality ---
void play_sound(char type) {
    if(!furi_hal_speaker_acquire(500)) return;
    if(type == 's') { furi_hal_speaker_start(600.0f, 1.0f); furi_delay_ms(50); furi_hal_speaker_start(900.0f, 1.0f); furi_delay_ms(100); }
    else if(type == 'f') { furi_hal_speaker_start(200.0f, 1.0f); furi_delay_ms(200); }
    furi_hal_speaker_stop(); furi_hal_speaker_release();
}

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
        app->pet_stats.level++;
        app->level_up_timer = 20;
        play_sound('s');
        notification_message(app->notify, &sequence_success);
    } else {
        play_sound('f');
    }
}

// ... Rendering and Logic Callbacks as before ...

int32_t flippermon_app(void* p) {
    // [Entry Point with Starter Menu logic and View registration]
    return 0;
}