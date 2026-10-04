// Host-Repro fuer scenes/scene_payload_list.c (lora-ukfe) -- die neue
// Delay-ms-Eingabe fuer die vom Heltec gemeldete Payload-Liste (Menuepunkt
// "Payloads", command_catalog-unabhaengiger JSON-Pfad). Schliesst den in
// trigger-catalog-number-input-port.md und
// payload-run-catalog-number-input-port.md jeweils unter "Bewusst nicht
// mitgeliefert"/"Offen" vermerkten Rueckstellpunkt: payload_callback() rief
// ukfe_uart_send_trigger(app, id, 0) bisher mit fest verdrahtetem delay_ms=0
// auf -- eine Payload aus der Liste auszuwaehlen loeste sie immer sofort
// aus, ohne Moeglichkeit, einen Delay einzugeben.
//
// scenes/scene_payload_list.c ist die UNVERAENDERTE, echte Datei aus dieser
// Sitzung (verbatim kopiert, kein Nachbau), nach Anwendung von
// trigger_catalog_number_input.patch (liefert app->number_input/
// UkfeViewNumberInput) UND payload_list_delay_input.patch (dieser Fix).
// Nur lora_ukfe.h/scenes.h sind Mock-Ersatz: sie stellen ausschliesslich die
// Typen/Funktionssignaturen bereit, die scene_payload_list.c tatsaechlich
// referenziert (NumberInput, Submenu, SceneManager, ViewDispatcher,
// LораUkfeApp-Felder) -- die echten Header haengen an <furi.h>/<gui/...>,
// die auf dem Host nicht existieren. Die eigentliche GUI-Mechanik
// (Tastatur-Navigation in number_input.c, Listen-Navigation in submenu.c)
// wird NICHT nachgebaut, sondern als bereits produktiv verifiziert
// vorausgesetzt: number_input ruft seinen Ergebnis-Callback laut seiner
// eigenen Implementierung (number_input_handle_ok() in
// gui/modules/number_input.c) nur mit einem Wert im uebergebenen
// [min,max]-Fenster auf, submenu ruft sein Item-Callback mit genau dem
// Index auf, unter dem das Item registriert wurde -- simulate_select()/
// simulate_delay_enter() bilden genau das nach.
//
// ukfe_uart_send_trigger() selbst (uart_comm.c) ist unveraendert und haengt
// an furi_hal_serial_tx()/ukfe_log_append() (furi-Abhaengigkeit) -- sie wird
// hier, wie ukfe_uart_send_rf() in den beiden vorherigen lora-ukfe-Repros
// dieses Verzeichnisses, als aufzeichnender Stub nachgebildet. Ihr
// JSON-Format (mit delay_ms-Feld nur fuer delay_ms>0) ist bereits in
// trigger-catalog-number-input-port.md zitiert und bleibt durch diesen Fix
// unangetastet; getestet wird ausschliesslich, was scene_payload_list.c
// selbst beitraegt: dass die Funktion jetzt mit der vom Nutzer eingegebenen
// Payload-ID UND dem eingegebenen delay_ms aufgerufen wird (statt sofort mit
// delay_ms=0), sowie dass build_list()/die 0x11-/0x12-Custom-Events und der
// Index-Bounds-Check unveraendert funktionieren.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lora_ukfe.h"
#include "scenes/scenes.h"

// ---- Aufzeichnende Stubs fuer die von scene_payload_list.c aufgerufenen
//      GUI-/UART-Funktionen ----

static NumberInputCallback s_last_cb;
static void*               s_last_cb_ctx;
static int32_t             s_last_current, s_last_min, s_last_max;
static char                s_last_ni_header[64];
static int                 s_result_cb_calls;

void number_input_set_header_text(NumberInput* number_input, const char* text) {
    (void)number_input;
    strncpy(s_last_ni_header, text, sizeof(s_last_ni_header) - 1);
    s_last_ni_header[sizeof(s_last_ni_header) - 1] = '\0';
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

static int         s_submenu_reset_calls;
static char         s_last_submenu_header[32];
static SubmenuItemCallback s_item_cb[UKFE_MAX_PAYLOADS];
static void*        s_item_cb_ctx[UKFE_MAX_PAYLOADS];
static uint32_t      s_item_count;

void submenu_reset(Submenu* submenu) {
    (void)submenu;
    s_submenu_reset_calls++;
    s_item_count = 0;
}

void submenu_set_header(Submenu* submenu, const char* header) {
    (void)submenu;
    strncpy(s_last_submenu_header, header, sizeof(s_last_submenu_header) - 1);
    s_last_submenu_header[sizeof(s_last_submenu_header) - 1] = '\0';
}

void submenu_add_item(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallback callback,
    void* callback_context) {
    (void)submenu;
    (void)label;
    assert(s_item_count < UKFE_MAX_PAYLOADS);
    // Repro nutzt ausschliesslich sequentiell aufsteigende index-Werte
    // (wie build_list() sie vergibt) als Array-Position.
    s_item_cb[index]     = callback;
    s_item_cb_ctx[index] = callback_context;
    if(index + 1 > s_item_count) s_item_count = index + 1;
}

static uint32_t s_last_view;
void view_dispatcher_switch_to_view(ViewDispatcher* view_dispatcher, uint32_t view_id) {
    (void)view_dispatcher;
    s_last_view = view_id;
}

static int s_previous_scene_calls;
void scene_manager_previous_scene(SceneManager* scene_manager) {
    (void)scene_manager;
    s_previous_scene_calls++;
}

static uint8_t  s_last_sent_id;
static uint32_t s_last_sent_delay_ms;
static int      s_send_trigger_calls;
void ukfe_uart_send_trigger(LораUkfeApp* app, uint8_t id, uint32_t delay_ms) {
    (void)app;
    s_last_sent_id       = id;
    s_last_sent_delay_ms = delay_ms;
    s_send_trigger_calls++;
}

static int s_send_payload_list_calls;
void ukfe_uart_send_payload_list(LораUkfeApp* app) {
    (void)app;
    s_send_payload_list_calls++;
}

// Simuliert "Nutzer waehlt Submenu-Eintrag [index]": ruft das dafuer
// registrierte Item-Callback auf, genau wie submenu.c es fuer einen Klick
// auf dieses Item taete.
static void simulate_select(LораUkfeApp* app, uint32_t index) {
    (void)app;
    assert(index < s_item_count);
    assert(s_item_cb[index] != NULL);
    s_item_cb[index](s_item_cb_ctx[index], index);
}

// Simuliert "Nutzer tippt number, drueckt Enter" im NumberInput-Schritt:
// ruft den zuletzt registrierten Ergebnis-Callback auf, genau wie
// number_input_handle_ok() in der echten number_input.c fuer einen im
// [min,max]-Fenster liegenden Wert taete.
static void simulate_delay_enter(int32_t number) {
    assert(s_last_cb != NULL);
    assert(number >= s_last_min && number <= s_last_max);
    NumberInputCallback cb  = s_last_cb;
    void*               ctx = s_last_cb_ctx;
    cb(ctx, number);
}

static void reset_recorders(void) {
    s_last_cb = NULL;
    s_last_cb_ctx = NULL;
    s_last_ni_header[0] = '\0';
    s_result_cb_calls = 0;
    s_submenu_reset_calls = 0;
    s_last_submenu_header[0] = '\0';
    memset(s_item_cb, 0, sizeof(s_item_cb));
    memset(s_item_cb_ctx, 0, sizeof(s_item_cb_ctx));
    s_item_count = 0;
    s_last_view = 0xFFFFFFFFu;
    s_previous_scene_calls = 0;
    s_last_sent_id = 0xFF;
    s_last_sent_delay_ms = 0xFFFFFFFFu;
    s_send_trigger_calls = 0;
    s_send_payload_list_calls = 0;
}

static void set_payloads(LораUkfeApp* app, const UkfePayloadEntry* entries, uint8_t count) {
    app->payload_count = count;
    for(uint8_t i = 0; i < count; i++) app->payloads[i] = entries[i];
}

int main(void) {
    LораUkfeApp app_storage = {0};
    LораUkfeApp* app = &app_storage;

    // ---- Szenario 1: Normalablauf -- Liste aufbauen, Payload auswaehlen,
    //      Delay eingeben, korrekter ukfe_uart_send_trigger()-Aufruf mit
    //      genau dieser ID und diesem delay_ms, danach erst
    //      scene_manager_previous_scene() (nicht schon beim Klick) ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {
            {.id = 7, .name = "alpha"},
            {.id = 42, .name = "beta"}, // id != index -- id muss verwendet werden, nicht index
            {.id = 200, .name = "gamma"},
        };
        set_payloads(app, entries, 3);

        scene_payload_list_on_enter(app);
        assert(s_submenu_reset_calls == 1);
        assert(strcmp(s_last_submenu_header, "Payloads (3)") == 0);
        assert(s_item_count == 3);
        assert(s_last_view == UkfeViewMenu);
        assert(s_send_payload_list_calls == 1); // Liste weiterhin vom Heltec angefordert

        simulate_select(app, 1); // Klick auf Eintrag "beta" (id=42)

        assert(s_last_view == UkfeViewNumberInput); // jetzt Delay-Eingabe statt Sofortversand
        assert(s_result_cb_calls == 1);
        assert(strcmp(s_last_ni_header, "Delay ms (0-300000)") == 0);
        assert(s_last_min == 0 && s_last_max == 300000 && s_last_current == 0);
        assert(s_send_trigger_calls == 0);       // noch nichts gesendet
        assert(s_previous_scene_calls == 0);     // noch nicht zurueckgesprungen

        simulate_delay_enter(2500); // Delay bestaetigen

        assert(s_send_trigger_calls == 1);
        assert(s_last_sent_id == 42);         // ID des angeklickten Eintrags (nicht Index 1)
        assert(s_last_sent_delay_ms == 2500);
        assert(s_previous_scene_calls == 1);  // genau jetzt zurueck ins Menue

        printf("OK:   Normalablauf: Payload id=42 ausgewaehlt, Delay=2500ms eingegeben -> "
               "ukfe_uart_send_trigger(id=42, delay_ms=2500), danach erst previous_scene().\n");
    }

    // ---- Szenario 2: delay_ms=0 -- muss weiterhin gesendet werden (gemaess
    //      dem bestehenden, unveraenderten Verhalten von
    //      ukfe_uart_send_trigger() fuer delay_ms=0), nicht etwa
    //      uebersprungen werden ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {{.id = 9, .name = "solo"}};
        set_payloads(app, entries, 1);
        scene_payload_list_on_enter(app);

        simulate_select(app, 0);
        simulate_delay_enter(0);

        assert(s_send_trigger_calls == 1);
        assert(s_last_sent_id == 9);
        assert(s_last_sent_delay_ms == 0);
        assert(s_previous_scene_calls == 1);

        printf("OK:   delay_ms=0 wird weiterhin gesendet (wie zuvor bei Sofortversand), "
               "jetzt ueber die explizite Delay-Eingabe statt stillschweigend fest verdrahtet.\n");
    }

    // ---- Szenario 3: Obergrenze delay_ms=300000 (TRIGGER_DELAY_MS_MAX) ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {{.id = 255, .name = "max-id"}};
        set_payloads(app, entries, 1);
        scene_payload_list_on_enter(app);

        simulate_select(app, 0);
        assert(s_last_max == 300000);
        simulate_delay_enter(300000);

        assert(s_send_trigger_calls == 1);
        assert(s_last_sent_id == 255);
        assert(s_last_sent_delay_ms == 300000);

        printf("OK:   Obergrenze delay_ms=300000 (TRIGGER_DELAY_MS_MAX) wird unveraendert "
               "durchgereicht.\n");
    }

    // ---- Szenario 4: Index >= payload_count -- payload_callback() kehrt
    //      sofort zurueck (Regressions-Check: unveraendertes Verhalten aus
    //      der Vor-Fix-Version), keine Delay-Eingabe, kein Sendeversuch ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {{.id = 1, .name = "only"}};
        set_payloads(app, entries, 1);
        scene_payload_list_on_enter(app);

        // Ruft payload_callback() direkt mit einem Index aus, der nie ueber
        // submenu_add_item() registriert wurde (ausserhalb 0..payload_count-1),
        // exakt wie der echte `if(index >= app->payload_count) return;`-Pfad
        // es fuer eine veraltete/verzoegerte Submenu-Callback-Auswahl abfangen
        // soll (z. B. Klick kurz vor einem 0x12-Refresh, der die Liste
        // verkleinert).
        assert(s_item_cb[0] != NULL);
        s_item_cb[0](app, 5); // index=5, payload_count=1 -> ausserhalb

        assert(s_last_view == UkfeViewMenu);    // kein Wechsel zu NumberInput
        assert(s_result_cb_calls == 0);         // kein Delay-Schritt aufgesetzt
        assert(s_send_trigger_calls == 0);
        assert(s_previous_scene_calls == 0);

        printf("OK:   index >= payload_count: payload_callback() kehrt unveraendert sofort "
               "zurueck, keine Delay-Eingabe, kein Sendeversuch.\n");
    }

    // ---- Szenario 5: build_list()/0x12-Refresh und 0x11-trigger_ack bleiben
    //      unveraendert funktionsfaehig -- auch waehrend/nach einer laufenden
    //      Delay-Eingabe eines zuvor ausgewaehlten Eintrags ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {
            {.id = 3, .name = "first"},
            {.id = 4, .name = "second"},
        };
        set_payloads(app, entries, 2);
        scene_payload_list_on_enter(app);
        assert(s_item_count == 2);

        simulate_select(app, 0); // id=3 ausgewaehlt, jetzt in der Delay-Eingabe

        // Waehrenddessen trifft ein payload_list-Refresh vom Heltec ein (0x12)
        // mit einer veraenderten Liste -- build_list() muss weiterhin greifen,
        // unabhaengig von der laufenden Delay-Eingabe fuer den zuvor
        // ausgewaehlten Eintrag (dessen id=3 bereits in s_selected_id
        // gesichert ist, bevor die Liste sich aendert).
        const UkfePayloadEntry refreshed[] = {
            {.id = 10, .name = "new-a"},
            {.id = 11, .name = "new-b"},
            {.id = 12, .name = "new-c"},
        };
        set_payloads(app, refreshed, 3);
        SceneManagerEvent refresh_event = {.type = SceneManagerEventTypeCustom, .event = 0x12};
        bool consumed = scene_payload_list_on_event(app, refresh_event);
        assert(consumed == true);
        assert(s_submenu_reset_calls == 2); // build_list() erneut gelaufen
        assert(strcmp(s_last_submenu_header, "Payloads (3)") == 0);
        assert(s_item_count == 3);

        // Die laufende Delay-Eingabe fuer den zuvor (vor dem Refresh)
        // ausgewaehlten Eintrag bleibt unbeeinflusst -- Bestaetigen sendet
        // weiterhin id=3, nicht etwa einen Wert aus der neuen Liste.
        simulate_delay_enter(750);
        assert(s_send_trigger_calls == 1);
        assert(s_last_sent_id == 3);
        assert(s_last_sent_delay_ms == 750);

        // 0x11 (trigger_ack) bleibt ebenfalls konsumiert, ohne Seiteneffekt.
        SceneManagerEvent ack_event = {.type = SceneManagerEventTypeCustom, .event = 0x11};
        consumed = scene_payload_list_on_event(app, ack_event);
        assert(consumed == true);

        printf("OK:   0x12-Refresh (build_list()) und 0x11-trigger_ack bleiben unveraendert "
               "funktionsfaehig, auch waehrend einer laufenden Delay-Eingabe fuer einen zuvor "
               "ausgewaehlten Eintrag.\n");
    }

    // ---- Szenario 6: Back waehrend der Delay-Eingabe -- bestehendes,
    //      unveraendertes Verhalten der Scene (Back wird in JEDER View
    //      dieser Scene unkonditional konsumiert und verlaesst die ganze
    //      Scene via scene_manager_previous_scene()); Back im Menue-View
    //      verhaelt sich identisch ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {{.id = 5, .name = "x"}};
        set_payloads(app, entries, 1);
        scene_payload_list_on_enter(app);
        simulate_select(app, 0); // jetzt in der Delay-Eingabe (NumberInput-View)

        SceneManagerEvent back_event = {.type = SceneManagerEventTypeBack, .event = 0};
        bool consumed = scene_payload_list_on_event(app, back_event);

        assert(consumed == true);
        assert(s_previous_scene_calls == 1); // Scene wird verlassen
        assert(s_send_trigger_calls == 0);   // die begonnene Delay-Eingabe wird verworfen,
                                              // nicht etwa mit einem Default-Wert gesendet

        printf("OK:   Back waehrend der Delay-Eingabe verlaesst die Scene (bestehendes, "
               "unveraendertes on_event()-Verhalten) und verwirft die begonnene Eingabe, "
               "ohne zu senden.\n");
    }

    // ---- Szenario 7: on_exit raeumt unabhaengig davon auf, ob gerade die
    //      Menue- oder die NumberInput-View aktiv war (kein Haengenbleiben
    //      der NumberInput-View bei Navigation weg von der Scene) ----
    {
        reset_recorders();
        const UkfePayloadEntry entries[] = {{.id = 6, .name = "y"}};
        set_payloads(app, entries, 1);
        scene_payload_list_on_enter(app);
        simulate_select(app, 0); // NumberInput-View aktiv, nichts bestaetigt

        scene_payload_list_on_exit(app);
        assert(s_submenu_reset_calls == 2); // einmal on_enter/build_list, einmal on_exit

        printf("OK:   on_exit() raeumt (submenu_reset()) unveraendert auf, auch wenn die "
               "Scene mit aktiver NumberInput-View (unbestaetigte Delay-Eingabe) verlassen "
               "wird.\n");
    }

    // ---- Szenario 8: Randfall-Rundlauf -- mehrere ID/Delay-Kombinationen
    //      (inkl. 0/255 und 0/300000-Obergrenze) je durch den vollen
    //      on_enter -> Auswahl -> Delay -> send-Ablauf ----
    {
        static const struct { uint8_t id; uint32_t delay_ms; } cases[] = {
            {0, 0}, {255, 0}, {0, 300000}, {255, 300000}, {1, 1}, {200, 299999},
        };
        for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            reset_recorders();
            const UkfePayloadEntry entries[] = {{.id = cases[i].id, .name = "case"}};
            set_payloads(app, entries, 1);
            scene_payload_list_on_enter(app);
            simulate_select(app, 0);
            simulate_delay_enter((int32_t)cases[i].delay_ms);
            assert(s_send_trigger_calls == 1);
            assert(s_last_sent_id == cases[i].id);
            assert(s_last_sent_delay_ms == cases[i].delay_ms);
        }
        printf("OK:   %zu ID/Delay-Kombinationen (inkl. Randfaellen 0/255 und "
               "0/300000-Obergrenze) verlustfrei durch den vollen Ablauf gesendet.\n",
               sizeof(cases) / sizeof(cases[0]));
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
