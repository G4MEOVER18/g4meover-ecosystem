// Host-Repro: RX-Dispatch fuer UkfeRfRespPayload (0x84) in lora-ukfe/uart_comm.c,
// handle_rf_response(). Reine Zustandslogik (kein UART/Storage/GUI-Zugriff) --
// bildet ALT (Ist-Zustand nach der Binaer-RX-Migration, Commit 0dba410a) und NEU
// (mit ergaenztem RespPayload-Case) 1:1 aus dem Patch nach.
//
// Zeigt:
//   1) BUG (ALT): UkfeRfRespPayload-Frames landen im generischen "default:"-Zweig
//      von handle_rf_response() -- app->payloads[]/app->payload_count bleiben immer
//      auf 0, das Custom-Event 0x12 (das scene_payload_list_on_event() zum Neuaufbau
//      des Menues braucht) wird nie gesendet. Die Payload-Liste zeigt dauerhaft
//      "(keine -- Liste laden)", unabhaengig davon, wie viele Eintraege der Hub
//      tatsaechlich sendet.
//   2) FIX (NEU): RespPayload befuellt payloads[idx]/payload_count bounds-geprueft
//      (idx gegen UKFE_MAX_PAYLOADS, Namenslaenge gegen die Feldkapazitaet) und
//      dispatcht 0x12 bei jedem gueltigen Eintrag.
//   3) FIX bleibt speichersicher bei einem kartenkontrollierten idx aus dem
//      gueltigen UkfeRfMessage-Wertebereich (0..255, da args[0] ein uint8_t ist),
//      der ausserhalb von UKFE_MAX_PAYLOADS liegt -- kein Schreibzugriff.
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---- Konstanten/Typen 1:1 aus lora_ukfe.h / rf/ukfe_rf.h ----
#define UKFE_MAX_PAYLOADS   32u
#define UKFE_RF_MAX_ARGS    40u

typedef struct {
    uint8_t id;
    char    name[48];
} UkfePayloadEntry;

typedef struct {
    uint32_t counter;
    uint8_t  cmd;
    uint8_t  arg_len;
    uint8_t  args[UKFE_RF_MAX_ARGS];
} UkfeRfMessage;

enum {
    UkfeRfRespAck     = 0x80,
    UkfeRfRespStatus  = 0x81,
    UkfeRfRespRelayed = 0x82,
    UkfeRfRespPayload = 0x84,
};

// ---- App-Zustand (Ausschnitt aus LораUkfeApp, so viel wie fuer den Dispatch noetig) ----
typedef struct {
    UkfePayloadEntry payloads[UKFE_MAX_PAYLOADS];
    uint8_t          payload_count;
    int              event_0x12_count;   // Ersatz fuer view_dispatcher_send_custom_event(..., 0x12)
    int              log_lines;          // Ersatz fuer ukfe_log_append()
} AppState;

// ---- ALT: handle_rf_response() vor dem Fix (Ist-Zustand) ----
// Ack/Status/Relayed behandelt, alles andere (inkl. RespPayload) faellt in
// den generischen default-Zweig -- nur Log, kein Zustand, kein Event.
static void handle_rf_response_old(AppState* app, const UkfeRfMessage* m) {
    switch(m->cmd) {
    case UkfeRfRespAck:
    case UkfeRfRespStatus:
    case UkfeRfRespRelayed:
        app->log_lines++;
        break;
    default:
        app->log_lines++;   // [RX] cmd=... -- RespPayload landet genau hier
        break;
    }
}

// ---- NEU: handle_rf_response() mit ergaenztem RespPayload-Case ----
static void handle_rf_response_new(AppState* app, const UkfeRfMessage* m) {
    switch(m->cmd) {
    case UkfeRfRespAck:
    case UkfeRfRespStatus:
    case UkfeRfRespRelayed:
        app->log_lines++;
        break;
    case UkfeRfRespPayload: {
        uint8_t idx = m->arg_len > 0 ? m->args[0] : 0xFFu;
        if(m->arg_len > 0 && idx < UKFE_MAX_PAYLOADS) {
            size_t name_len = m->arg_len > 1 ? (size_t)(m->arg_len - 1) : 0;
            size_t name_cap = sizeof(app->payloads[idx].name) - 1;
            if(name_len > name_cap) name_len = name_cap;
            app->payloads[idx].id = idx;
            memcpy(app->payloads[idx].name, &m->args[1], name_len);
            app->payloads[idx].name[name_len] = '\0';
            if(idx >= app->payload_count) app->payload_count = idx + 1;
            app->event_0x12_count++;
        }
        app->log_lines++;
        break;
    }
    default:
        app->log_lines++;
        break;
    }
}

static UkfeRfMessage make_payload_resp(uint8_t idx, const char* name) {
    UkfeRfMessage m = {0};
    m.cmd = UkfeRfRespPayload;
    size_t nlen = strlen(name);
    if(nlen > UKFE_RF_MAX_ARGS - 1) nlen = UKFE_RF_MAX_ARGS - 1;   // wie args[] es kartenseitig begrenzt
    m.args[0] = idx;
    memcpy(&m.args[1], name, nlen);
    m.arg_len = (uint8_t)(1 + nlen);
    return m;
}

int main(void) {
    // ---- Szenario 1: BUG -- Hub sendet drei Payload-Eintraege, ALT ignoriert sie alle ----
    {
        AppState app = {0};
        UkfeRfMessage frames[3] = {
            make_payload_resp(0, "usb_ducky_marker.txt"),
            make_payload_resp(1, "wifi_deauth_all.ds"),
            make_payload_resp(2, "ble_spam_apple.ds"),
        };
        for(int i = 0; i < 3; i++) handle_rf_response_old(&app, &frames[i]);

        assert(app.payload_count == 0);
        assert(app.event_0x12_count == 0);
        for(unsigned i = 0; i < UKFE_MAX_PAYLOADS; i++) assert(app.payloads[i].name[0] == '\0');
        printf("BUG:  3 RespPayload-Frames empfangen, payload_count blieb bei %u, "
               "0x12 nie gesendet (Payload-Liste bleibt dauerhaft leer)\n", app.payload_count);
    }

    // ---- Szenario 2: FIX -- dieselben drei Frames werden korrekt eingetragen ----
    {
        AppState app = {0};
        UkfeRfMessage frames[3] = {
            make_payload_resp(0, "usb_ducky_marker.txt"),
            make_payload_resp(1, "wifi_deauth_all.ds"),
            make_payload_resp(2, "ble_spam_apple.ds"),
        };
        for(int i = 0; i < 3; i++) handle_rf_response_new(&app, &frames[i]);

        assert(app.payload_count == 3);
        assert(app.event_0x12_count == 3);
        assert(strcmp(app.payloads[0].name, "usb_ducky_marker.txt") == 0);
        assert(app.payloads[0].id == 0);
        assert(strcmp(app.payloads[1].name, "wifi_deauth_all.ds") == 0);
        assert(strcmp(app.payloads[2].name, "ble_spam_apple.ds") == 0);
        printf("FIX:  3 RespPayload-Frames empfangen, payload_count=%u, "
               "0x12 %dx gesendet, Namen korrekt uebernommen\n",
               app.payload_count, app.event_0x12_count);
    }

    // ---- Szenario 3: FIX -- Eintrag am oberen Rand (idx=UKFE_MAX_PAYLOADS-1) + Namen
    //      an der args[]-Obergrenze (39 Byte Nutzlast, passt in name[48] ohne Kuerzung) ----
    {
        AppState app = {0};
        char long_name[UKFE_RF_MAX_ARGS];   // 39 Zeichen + '\0', < name_cap(47)
        memset(long_name, 'A', sizeof(long_name) - 1);
        long_name[sizeof(long_name) - 1] = '\0';

        UkfeRfMessage f = make_payload_resp(UKFE_MAX_PAYLOADS - 1, long_name);
        handle_rf_response_new(&app, &f);

        assert(app.payload_count == UKFE_MAX_PAYLOADS);
        assert(strlen(app.payloads[UKFE_MAX_PAYLOADS - 1].name) == strlen(long_name));
        assert(strcmp(app.payloads[UKFE_MAX_PAYLOADS - 1].name, long_name) == 0);
        printf("FIX:  Randfall idx=%u (letzter gueltiger Slot) mit %zu-Byte-Namen "
               "korrekt uebernommen, payload_count=%u\n",
               UKFE_MAX_PAYLOADS - 1, strlen(long_name), app.payload_count);
    }

    // ---- Szenario 4: FIX -- kartenkontrollierter idx ausserhalb UKFE_MAX_PAYLOADS
    //      (args[0] ist ein uint8_t, 0..255 moeglich) wird sicher verworfen ----
    {
        AppState app = {0};
        UkfeRfMessage f = make_payload_resp(200, "out_of_range");
        handle_rf_response_new(&app, &f);   // darf NICHT ausserhalb von payloads[] schreiben

        assert(app.payload_count == 0);
        assert(app.event_0x12_count == 0);
        for(unsigned i = 0; i < UKFE_MAX_PAYLOADS; i++) assert(app.payloads[i].name[0] == '\0');
        printf("FIX:  idx=200 (> UKFE_MAX_PAYLOADS-1) sicher verworfen, kein Schreibzugriff, "
               "payload_count blieb bei %u\n", app.payload_count);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
