#include "../lora_ukfe.h"
#include "scenes.h"

// Obergrenze fuer delay_ms im UI, dieselbe Konvention/derselbe Wert wie in
// scene_trigger_confirm.c: 5 Minuten UI-Obergrenze (NumberInput rechnet mit
// int32_t), das JSON-Protokoll (ukfe_uart_send_trigger()) traegt delay_ms
// weiterhin als volles uint32_t -- reine UI-Grenze, kein Protokolllimit.
#define TRIGGER_DELAY_MS_MAX 300000

// Payload-ID, fuer die gerade ein Delay abgefragt wird (zwischen
// payload_callback() und payload_delay_cb() gueltig).
static uint8_t s_selected_id;

static void payload_delay_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    ukfe_uart_send_trigger(app, s_selected_id, (uint32_t)number);

    // Kurzes visuelles Feedback → dann zurück ins Menü
    scene_manager_previous_scene(app->scene_manager);
}

static void payload_callback(void* ctx, uint32_t index) {
    LораUkfeApp* app = ctx;
    if(index >= app->payload_count) return;

    s_selected_id = app->payloads[index].id;

    // Delay (ms) abfragen statt sofort mit delay_ms=0 zu senden -- Senden
    // passiert erst aus payload_delay_cb(), nach Bestaetigung der Eingabe.
    number_input_set_header_text(app->number_input, "Delay ms (0-300000)");
    number_input_set_result_callback(
        app->number_input, payload_delay_cb, app, 0, 0, TRIGGER_DELAY_MS_MAX);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewNumberInput);
}

static void build_list(LораUkfeApp* app) {
    submenu_reset(app->submenu);
    char header[32];
    snprintf(header, sizeof(header), "Payloads (%d)", app->payload_count);
    submenu_set_header(app->submenu, header);

    if(app->payload_count == 0) {
        submenu_add_item(app->submenu, "(keine — Liste laden)", 0, NULL, NULL);
        return;
    }

    for(uint8_t i = 0; i < app->payload_count; i++) {
        char label[56];
        snprintf(label, sizeof(label), "[%02d] %s",
            app->payloads[i].id, app->payloads[i].name);
        submenu_add_item(app->submenu, label, i, payload_callback, app);
    }
}

void scene_payload_list_on_enter(void* ctx) {
    LораUkfeApp* app = ctx;
    build_list(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewMenu);
    // Liste vom Heltec anfordern
    ukfe_uart_send_payload_list(app);
}

bool scene_payload_list_on_event(void* ctx, SceneManagerEvent event) {
    LораUkfeApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 0x12) {
            // payload_list Event empfangen → neu aufbauen
            build_list(app);
            consumed = true;
        } else if(event.event == 0x11) {
            // trigger_ack → Meldung kann hier ergänzt werden
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void scene_payload_list_on_exit(void* ctx) {
    LораUkfeApp* app = ctx;
    submenu_reset(app->submenu);
}
