#include "../lora_ukfe.h"
#include "scenes.h"

// Zwei-Schritt-Zahleneingabe fuer den Katalog-Menuepunkt "Trigger [V3]"
// (UkfeRfCmdTrigger, command_catalog.h FN_PAYLOAD[0]): bisher lief dieser
// Eintrag ueber scene_menu.c/send_fn() immer mit fest verdrahtetem arg=0 --
// jede Payload-ID ausser 0 sowie ein delay_ms > 0 waren ueber das Katalog-Menue
// gar nicht erreichbar (siehe "Offen" in
// app-improvements/lora-ukfe/catalog-trigger-arg-format-mismatch-fix.md).
// Diese Scene fragt beides ab und baut den Frame direkt ueber
// ukfe_rf_make_trigger() -- denselben Builder, den der dort dokumentierte
// Wire-Format-Fix fuer send_fn() vorschlaegt --, macht den dortigen
// send_fn()-Sonderfall fuer diesen Katalog-Eintrag also ueberfluessig, da er
// jetzt nie mehr ueber den generischen Pfad laeuft.

// Obergrenze fuer delay_ms im UI: 5 Minuten. UkfeRfCmdTrigger traegt intern
// ein volles uint32 (siehe rf/ukfe_rf.h), aber NumberInput rechnet mit
// int32_t -- 5 Minuten deckt jeden realistischen Trigger-Verzug ab und bleibt
// mit wenigen Tastendruecken eingebbar.
#define TRIGGER_DELAY_MS_MAX 300000

typedef enum {
    TriggerStepId = 0,
    TriggerStepDelay,
} TriggerStep;

static TriggerStep s_step;
static uint8_t     s_id;

static void trigger_delay_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    uint32_t delay_ms = (uint32_t)number;

    UkfeRfMessage m;
    ukfe_rf_make_trigger(&m, s_id, delay_ms);
    ukfe_uart_send_rf(app, &m);

    scene_manager_next_scene(app->scene_manager, UkfeSceneLog);
}

static void show_delay_step(LораUkfeApp* app, int32_t preset) {
    s_step = TriggerStepDelay;
    number_input_set_header_text(app->number_input, "Delay ms (0-300000)");
    number_input_set_result_callback(
        app->number_input, trigger_delay_cb, app, preset, 0, TRIGGER_DELAY_MS_MAX);
}

static void trigger_id_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    s_id = (uint8_t)number;
    show_delay_step(app, 0);
}

static void show_id_step(LораUkfeApp* app, int32_t preset) {
    s_step = TriggerStepId;
    number_input_set_header_text(app->number_input, "Payload-ID (0-255)");
    number_input_set_result_callback(app->number_input, trigger_id_cb, app, preset, 0, 255);
}

void scene_trigger_confirm_on_enter(void* ctx) {
    LораUkfeApp* app = ctx;
    s_id = 0;
    show_id_step(app, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewNumberInput);
}

bool scene_trigger_confirm_on_event(void* ctx, SceneManagerEvent event) {
    LораUkfeApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(s_step == TriggerStepDelay) {
            // Ein Back waehrend der Delay-Eingabe geht einen Schritt zurueck zur
            // ID-Eingabe (mit der zuletzt eingegebenen ID vorbelegt) statt die
            // ganze Scene zu verlassen und beide Eingaben zu verwerfen.
            show_id_step(app, s_id);
            consumed = true;
        }
        // Back auf der ID-Eingabe: nicht konsumiert -> Standardverhalten
        // (scene_manager verlaesst die Scene, zurueck zum Katalog-Menue).
    }
    return consumed;
}

void scene_trigger_confirm_on_exit(void* ctx) {
    UNUSED(ctx);
}
