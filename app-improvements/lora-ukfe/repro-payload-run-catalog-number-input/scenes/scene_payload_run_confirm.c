#include "../lora_ukfe.h"
#include "scenes.h"

// Einschritt-Zahleneingabe fuer den Katalog-Menuepunkt "Payload-Run [V3]"
// (UkfeRfCmdPayloadRun, command_catalog.h FN_PAYLOAD[1]): bisher lief dieser
// Eintrag ueber scene_menu.c/send_fn() immer mit fest verdrahtetem arg=0 --
// jeder Payload-Index ausser 0 war ueber das Katalog-Menue gar nicht
// erreichbar (siehe "Offen" in
// app-improvements/lora-ukfe/catalog-trigger-arg-format-mismatch-fix.md).
//
// Anders als beim Trigger-Fund (siehe scene_trigger_confirm.c) ist das
// Wire-Format hier bereits korrekt -- rf/ukfe_rf.h:27 dokumentiert
// UkfeRfCmdPayloadRun mit genau 1 Arg-Byte ("args: uint8 idx"), das passt
// exakt zum generischen 1-Byte-Pfad in scene_menu.c/send_fn(). Reine
// Bedienungsluecke, kein Protokollfehler: nur eine Zahl statt des festen 0
// abfragen, danach ueber denselben generischen Rahmen senden wie send_fn()
// es fuer jeden anderen has_arg-Katalog-Eintrag tut.

static void payload_run_idx_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;

    UkfeRfMessage m;
    m.cmd = UkfeRfCmdPayloadRun;
    m.arg_len = 1;
    m.args[0] = (uint8_t)number;
    ukfe_uart_send_rf(app, &m);

    scene_manager_next_scene(app->scene_manager, UkfeSceneLog);
}

void scene_payload_run_confirm_on_enter(void* ctx) {
    LораUkfeApp* app = ctx;
    number_input_set_header_text(app->number_input, "Payload-Idx (0-255)");
    number_input_set_result_callback(app->number_input, payload_run_idx_cb, app, 0, 0, 255);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewNumberInput);
}

bool scene_payload_run_confirm_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    // Back wird nicht konsumiert -> Standardnavigation zurueck zum
    // Katalog-Menue greift (einziger Eingabeschritt, kein Zwischenzustand
    // wie bei scene_trigger_confirm.c).
    return false;
}

void scene_payload_run_confirm_on_exit(void* ctx) {
    UNUSED(ctx);
}
