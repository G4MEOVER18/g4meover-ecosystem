// Host-Repro fuer scenes/scene_payload_run_confirm.c (lora-ukfe) -- die neue
// Einschritt-Zahleneingabe (Payload-Index) fuer den Katalog-Menuepunkt
// "Payload-Run [V3]" (command_catalog.h, FN_PAYLOAD[1]).
//
// scenes/scene_payload_run_confirm.c ist die UNVERAENDERTE, echte Datei aus
// dieser Sitzung (verbatim kopiert, kein Nachbau) -- ebenso rf/ukfe_rf.c/.h
// (reine Protokolllogik ohne furi-Abhaengigkeit, 1:1 wie in den vorherigen
// lora-ukfe-Repros dieses Verzeichnisses). Nur lora_ukfe.h/scenes.h sind
// Mock-Ersatz: sie stellen ausschliesslich die Typen/Funktionssignaturen
// bereit, die scene_payload_run_confirm.c tatsaechlich referenziert
// (NumberInput, SceneManager, ViewDispatcher, LораUkfeApp-Felder) -- die
// echten Header haengen an <furi.h>/<gui/...>, die auf dem Host nicht
// existieren. Die eigentliche GUI-Mechanik (Tastatur-Navigation in
// number_input.c) wird NICHT nachgebaut, sondern als bereits produktiv
// verifiziert vorausgesetzt: sie ruft ihren Ergebnis-Callback laut ihrer
// eigenen Implementierung (number_input_handle_ok() in
// gui/modules/number_input.c) nur mit einem Wert auf, der im uebergebenen
// [min,max]-Fenster liegt -- genau das simuliert simulate_enter() hier durch
// eine eigene Bereichspruefung vor dem Callback-Aufruf. Getestet wird
// ausschliesslich, was scene_payload_run_confirm.c selbst beitraegt: die
// Abfrage (Index -> Senden), dass Back NICHT konsumiert wird (anders als der
// Delay-Schritt beim Trigger-Fund, da es hier nur einen einzigen
// Eingabeschritt gibt) sowie der tatsaechlich gesendete ukfe_rf-Frame.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lora_ukfe.h"
#include "scenes/scenes.h"

// ---- Aufzeichnende Stubs fuer die von scene_payload_run_confirm.c
//      aufgerufenen GUI-/UART-Funktionen ----

static NumberInputCallback s_last_cb;
static void*               s_last_cb_ctx;
static int32_t             s_last_current, s_last_min, s_last_max;
static char                s_last_header[64];
static int                 s_result_cb_calls;

void number_input_set_header_text(NumberInput* number_input, const char* text) {
    (void)number_input;
    strncpy(s_last_header, text, sizeof(s_last_header) - 1);
    s_last_header[sizeof(s_last_header) - 1] = '\0';
}

void number_input_set_result_callback(
    NumberInput* number_input,
    NumberInputCallback callback,
    void* callback_context,
    int32_t current_number,
    int32_t min_value,
    int32_t max_value) {
    (void)number_input;
    s_last_cb      = callback;
    s_last_cb_ctx  = callback_context;
    s_last_current = current_number;
    s_last_min     = min_value;
    s_last_max     = max_value;
    s_result_cb_calls++;
}

static uint32_t s_last_view;
void view_dispatcher_switch_to_view(ViewDispatcher* view_dispatcher, uint32_t view_id) {
    (void)view_dispatcher;
    s_last_view = view_id;
}

static uint32_t s_last_next_scene;
static int      s_next_scene_calls;
void scene_manager_next_scene(SceneManager* scene_manager, uint32_t next_scene_id) {
    (void)scene_manager;
    s_last_next_scene = next_scene_id;
    s_next_scene_calls++;
}

static UkfeRfMessage s_last_sent_msg;
static int           s_send_rf_calls;
void ukfe_uart_send_rf(LораUkfeApp* app, const UkfeRfMessage* msg) {
    (void)app;
    s_last_sent_msg = *msg;
    s_send_rf_calls++;
}

// Simuliert "Nutzer tippt number, drueckt Enter": ruft den zuletzt
// registrierten NumberInput-Callback auf, genau wie es
// number_input_handle_ok() in der echten number_input.c fuer einen im
// [min,max]-Fenster liegenden Wert taete.
static void simulate_enter(int32_t number) {
    assert(s_last_cb != NULL);
    assert(number >= s_last_min && number <= s_last_max);
    NumberInputCallback cb = s_last_cb;
    void*                ctx = s_last_cb_ctx;
    cb(ctx, number);
}

static void reset_recorders(void) {
    s_last_cb = NULL;
    s_last_cb_ctx = NULL;
    s_last_header[0] = '\0';
    s_result_cb_calls = 0;
    s_last_view = 0xFFFFFFFFu;
    s_last_next_scene = 0xFFFFFFFFu;
    s_next_scene_calls = 0;
    memset(&s_last_sent_msg, 0, sizeof(s_last_sent_msg));
    s_send_rf_calls = 0;
}

static const uint8_t TEST_SECRET[UKFE_RF_SECRET_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01,
};

// Baut+parst den zuletzt aufgezeichneten Frame gegen den echten
// ukfe_rf_build_frame()/ukfe_rf_parse_frame() und prueft idx.
static void assert_wire_roundtrip(uint8_t expect_idx) {
    assert(s_send_rf_calls == 1);
    assert(s_last_sent_msg.cmd == UkfeRfCmdPayloadRun);
    assert(s_last_sent_msg.arg_len == 1);

    UkfeRfMessage m = s_last_sent_msg;
    m.counter = 1;
    uint8_t frame[UKFE_RF_MAX_FRAME];
    size_t n = ukfe_rf_build_frame(TEST_SECRET, &m, frame, sizeof(frame));
    assert(n == UKFE_RF_HDR_OVERHEAD + 1);

    UkfeRfMessage parsed;
    bool ok = ukfe_rf_parse_frame(TEST_SECRET, frame, n, &parsed, NULL);
    assert(ok);
    assert(parsed.args[0] == expect_idx);
}

int main(void) {
    LораUkfeApp app_storage = {0};
    LораUkfeApp* app = &app_storage;

    // ---- Szenario 1: normaler Ablauf Index -> Senden ----
    {
        reset_recorders();
        scene_payload_run_confirm_on_enter(app);

        assert(s_last_view == UkfeViewNumberInput);
        assert(s_result_cb_calls == 1);
        assert(strcmp(s_last_header, "Payload-Idx (0-255)") == 0);
        assert(s_last_min == 0 && s_last_max == 255 && s_last_current == 0);
        assert(s_send_rf_calls == 0); // noch nicht gesendet

        simulate_enter(17); // Payload-Index

        assert(s_send_rf_calls == 1);
        assert(s_next_scene_calls == 1 && s_last_next_scene == UkfeSceneLog);
        assert(s_last_sent_msg.args[0] == 17);
        assert_wire_roundtrip(17);

        printf("OK:   Normalablauf Index=17 -> korrekter 1-Byte-PayloadRun-Frame, "
               "danach UkfeSceneLog.\n");
    }

    // ---- Szenario 2: Back wird NICHT konsumiert (normale
    //      Scene-Manager-Navigation zurueck zum Katalog-Menue soll greifen --
    //      anders als beim Trigger-Fund gibt es hier nur einen einzigen
    //      Eingabeschritt, also keinen Zwischenzustand zum Zurueckgehen) ----
    {
        reset_recorders();
        scene_payload_run_confirm_on_enter(app);
        char header_at_enter[64];
        strcpy(header_at_enter, s_last_header);
        int calls_at_enter = s_result_cb_calls;

        SceneManagerEvent back_event = {.type = SceneManagerEventTypeBack, .event = 0};
        bool consumed = scene_payload_run_confirm_on_event(app, back_event);

        assert(consumed == false);
        // Keine Rekonfiguration ausgeloest -- Zustand unveraendert seit on_enter.
        assert(s_result_cb_calls == calls_at_enter);
        assert(strcmp(s_last_header, header_at_enter) == 0);
        assert(s_send_rf_calls == 0);

        printf("OK:   Back bleibt unkonsumiert (consumed=false) -- "
               "Standardnavigation der Scene zurueck zum Katalog-Menue greift.\n");
    }

    // ---- Szenario 3: Rundlauf ueber mehrere Indizes inkl. Randfaellen
    //      (0, 255) durch den vollen on_enter -> Eingabe -> send-Ablauf ----
    {
        static const uint8_t indices[] = {0, 1, 42, 128, 254, 255};
        for(size_t i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
            reset_recorders();
            scene_payload_run_confirm_on_enter(app);
            simulate_enter((int32_t)indices[i]);
            assert_wire_roundtrip(indices[i]);
        }
        printf("OK:   %zu Payload-Indizes (inkl. Randfaellen 0 und 255) verlustfrei "
               "durch den vollen Ablauf gesendet.\n",
               sizeof(indices) / sizeof(indices[0]));
    }

    // ---- Szenario 4: jeder on_enter-Aufruf setzt den Callback/Header
    //      unabhaengig vom vorherigen Aufruf komplett neu (kein Reststand
    //      aus einer fruehen Sitzung dieser Scene, z. B. nach Back ins
    //      Katalog-Menue und erneutem Aufruf desselben Menuepunkts) ----
    {
        reset_recorders();
        scene_payload_run_confirm_on_enter(app);
        simulate_enter(99);
        assert(s_send_rf_calls == 1);

        reset_recorders();
        scene_payload_run_confirm_on_enter(app);
        assert(s_result_cb_calls == 1); // frische Registrierung, kein Ueberbleibsel
        assert(s_last_current == 0);    // wieder bei 0, nicht bei der vorherigen 99
        assert(s_send_rf_calls == 0);

        simulate_enter(200);
        assert(s_last_sent_msg.args[0] == 200);

        printf("OK:   Wiederholter Scene-Aufruf startet wieder bei Index 0, "
               "unabhaengig vom zuvor gesendeten Index.\n");
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
