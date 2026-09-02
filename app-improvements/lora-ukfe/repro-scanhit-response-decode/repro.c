// Host-Repro: RX-Dispatch fuer UkfeRfRespScanHit (0x90) in lora-ukfe/uart_comm.c,
// handle_rf_response(). Reine Formatierungs-/Decodierlogik (kein UART/Storage/GUI-
// Zugriff) -- bildet ALT (Ist-Zustand nach der Binaer-RX-Migration, Commit
// 0dba410a, wie bereits fuer RespPayload dokumentiert) und NEU (mit ergaenztem
// RespScanHit-Case) 1:1 aus dem Patch nach.
//
// Zeigt:
//   1) BUG (ALT): UkfeRfRespScanHit-Frames landen im generischen "default:"-Zweig
//      von handle_rf_response() -- die eigentliche Nutzlast (freq_or_ch, rssi)
//      wird nie decodiert, nur cmd/ctr/alen geloggt. Fuer jeden der fuenf
//      Scan-Befehle (LoRa/SubGHz/WiFi/BLE/RemoteID), die alle UkfeRfRespScanHit
//      als Trefferantwort verwenden, ist das Log damit inhaltlich wertlos.
//   2) FIX (NEU): RespScanHit decodiert args[0..3] als uint32 (LE) freq_or_ch und
//      args[4] als int8 rssi in eine aussagekraeftige Logzeile.
//   3) FIX bleibt robust bei einem zu kurzen Frame (arg_len < 5, aus welchem Grund
//      auch immer -- Hub-Bug, Frame-Fragment): decodiert dann NICHT aus
//      unbelegten/veralteten args[]-Bytes, sondern markiert die Zeile explizit als
//      unvollstaendig. args[] ist ein fixes 40-Byte-Array (UKFE_RF_MAX_ARGS) --
//      kein Speicherzugriffsfehler in beiden Faellen, nur ein Korrektheitsproblem
//      ohne den Bounds-Check.
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---- Konstanten/Typen 1:1 aus rf/ukfe_rf.h ----
#define UKFE_RF_MAX_ARGS 40u

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
    UkfeRfRespScanHit = 0x90,
};

// ---- App-Zustand (Ausschnitt aus LораUkfeApp, so viel wie fuer den Dispatch noetig) ----
typedef struct {
    char last_line[96];
    int  log_lines;   // Ersatz fuer ukfe_log_append()
} AppState;

// Minimaler strlcpy-Ersatz (kein furi/lib-Abhaengigkeit im Host-Repro).
static void strlcpy_local(char* dst, const char* src, size_t cap) {
    size_t n = strlen(src);
    if(n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// ---- ALT: handle_rf_response() vor dem Fix (Ist-Zustand) ----
// Ack/Status/Relayed/Payload behandelt (Payload-Fix bereits gelandet), alles
// andere (inkl. RespScanHit) faellt in den generischen default-Zweig.
static void handle_rf_response_old(AppState* app, const UkfeRfMessage* m) {
    char line[96];
    switch(m->cmd) {
    case UkfeRfRespAck:
    case UkfeRfRespStatus:
    case UkfeRfRespRelayed:
    case UkfeRfRespPayload:
        snprintf(line, sizeof(line), "[handled] cmd=0x%02X", m->cmd);
        break;
    default:
        snprintf(line, sizeof(line), "[RX] cmd=0x%02X ctr=%lu alen=%u",
            m->cmd, (unsigned long)m->counter, m->arg_len);
        break;
    }
    strlcpy_local(app->last_line, line, sizeof(app->last_line));
    app->log_lines++;
}

// ---- NEU: handle_rf_response() mit ergaenztem RespScanHit-Case ----
static void handle_rf_response_new(AppState* app, const UkfeRfMessage* m) {
    char line[96];
    switch(m->cmd) {
    case UkfeRfRespAck:
    case UkfeRfRespStatus:
    case UkfeRfRespRelayed:
    case UkfeRfRespPayload:
        snprintf(line, sizeof(line), "[handled] cmd=0x%02X", m->cmd);
        break;
    case UkfeRfRespScanHit: {
        // args: uint32 freq_or_ch (LE), uint8 rssi -- 5 Byte Mindestlaenge, sonst
        // waeren die decodierten Werte unbelegter/veralteter Inhalt von args[]
        // (fixes 40-Byte-Array, kein OOB, aber falsche Werte ohne den Check).
        if(m->arg_len >= 5) {
            uint32_t freq_or_ch = (uint32_t)m->args[0] | ((uint32_t)m->args[1] << 8) |
                ((uint32_t)m->args[2] << 16) | ((uint32_t)m->args[3] << 24);
            int8_t rssi = (int8_t)m->args[4];
            snprintf(line, sizeof(line), "[SCAN] freq/ch=%lu rssi=%d",
                (unsigned long)freq_or_ch, rssi);
        } else {
            snprintf(line, sizeof(line), "[SCAN] malformed alen=%u (need >=5)", m->arg_len);
        }
        break;
    }
    default:
        snprintf(line, sizeof(line), "[RX] cmd=0x%02X ctr=%lu alen=%u",
            m->cmd, (unsigned long)m->counter, m->arg_len);
        break;
    }
    strlcpy_local(app->last_line, line, sizeof(app->last_line));
    app->log_lines++;
}

static UkfeRfMessage make_scan_hit(uint32_t freq_or_ch, int8_t rssi) {
    UkfeRfMessage m = {0};
    m.cmd = UkfeRfRespScanHit;
    m.args[0] = (uint8_t)(freq_or_ch & 0xFF);
    m.args[1] = (uint8_t)((freq_or_ch >> 8) & 0xFF);
    m.args[2] = (uint8_t)((freq_or_ch >> 16) & 0xFF);
    m.args[3] = (uint8_t)((freq_or_ch >> 24) & 0xFF);
    m.args[4] = (uint8_t)rssi;
    m.arg_len = 5;
    return m;
}

int main(void) {
    // ---- Szenario 1: BUG -- ALT decodiert einen LoRa-Scan-Treffer (868350 kHz,
    //      -72 dBm) nicht, nur die generische Zeile ohne Nutzlast ----
    {
        AppState app = {0};
        UkfeRfMessage f = make_scan_hit(868350u, -72);
        handle_rf_response_old(&app, &f);

        assert(app.log_lines == 1);
        assert(strstr(app.last_line, "freq") == NULL);
        assert(strstr(app.last_line, "rssi") == NULL);
        printf("BUG:  ScanHit(freq=868350, rssi=-72) -> Log '%s' (keine Nutzlast "
               "sichtbar)\n", app.last_line);
    }

    // ---- Szenario 2: FIX -- derselbe Treffer wird korrekt decodiert ----
    {
        AppState app = {0};
        UkfeRfMessage f = make_scan_hit(868350u, -72);
        handle_rf_response_new(&app, &f);

        assert(strcmp(app.last_line, "[SCAN] freq/ch=868350 rssi=-72") == 0);
        printf("FIX:  ScanHit(freq=868350, rssi=-72) -> Log '%s'\n", app.last_line);
    }

    // ---- Szenario 3: FIX -- Randfall groesster uint32-Wert + kleinster int8-Wert ----
    {
        AppState app = {0};
        UkfeRfMessage f = make_scan_hit(0xFFFFFFFFu, -128);
        handle_rf_response_new(&app, &f);

        assert(strcmp(app.last_line, "[SCAN] freq/ch=4294967295 rssi=-128") == 0);
        printf("FIX:  Randfall freq_or_ch=UINT32_MAX, rssi=INT8_MIN -> Log '%s'\n",
               app.last_line);
    }

    // ---- Szenario 4: FIX -- zu kurzer Frame (arg_len=4, RSSI-Byte fehlt) wird
    //      NICHT aus unbelegten args[]-Bytes decodiert, sondern als unvollstaendig
    //      markiert ----
    {
        AppState app = {0};
        UkfeRfMessage f = make_scan_hit(12345u, 5);
        f.arg_len = 4;   // RSSI-Byte (args[4]) fehlt am Frame, obwohl im Puffer gesetzt
        handle_rf_response_new(&app, &f);

        assert(strcmp(app.last_line, "[SCAN] malformed alen=4 (need >=5)") == 0);
        printf("FIX:  arg_len=4 (< 5) sicher als unvollstaendig markiert statt "
               "veraltete args[]-Bytes zu decodieren -> Log '%s'\n", app.last_line);
    }

    // ---- Szenario 5: FIX -- komplett leerer Frame (arg_len=0) ----
    {
        AppState app = {0};
        UkfeRfMessage f = {0};
        f.cmd = UkfeRfRespScanHit;
        f.arg_len = 0;
        handle_rf_response_new(&app, &f);

        assert(strcmp(app.last_line, "[SCAN] malformed alen=0 (need >=5)") == 0);
        printf("FIX:  arg_len=0 sicher als unvollstaendig markiert -> Log '%s'\n",
               app.last_line);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
