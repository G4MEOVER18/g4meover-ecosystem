// Host-Repro: Katalog-getriebenes send_fn() (lora-ukfe/scenes/scene_menu.c) fuer
// den Menuepunkt "Trigger [V3]" (command_catalog.h, FN_PAYLOAD[0]) gegen den echten,
// unveraenderten rf/ukfe_rf.c/.h (Commit 0dba410a, verbatim in dieses Verzeichnis
// kopiert -- reine Protokoll-/Framing-Logik, keine Flipper-/furi-Abhaengigkeit,
// daher 1:1 host-kompilierbar ohne Nachbau).
//
// Zeigt:
//   1) BUG (ALT): send_fn() baut fuer JEDEN Katalog-Eintrag mit has_arg==true
//      pauschal einen 1-Byte-Arg-Frame (m.arg_len = 1, args[0] = fn->arg). Das
//      passt fuer UkfeRfCmdBeaconSpam/BleSpam/EvilPortal/HidPayload/HidDucky/
//      Jammer/PayloadRun ("args: uint8 ...", je 1 Byte laut rf/ukfe_rf.h), aber
//      NICHT fuer UkfeRfCmdTrigger = 0x02, dessen eigener Kommentar in
//      rf/ukfe_rf.h:24 "args: uint8 id, uint32 delay_ms (LE)" -- also 5 Byte --
//      verlangt. Der Katalog-Eintrag "Trigger [V3]" sendet dadurch einen Frame,
//      der als ukfe_rf-Frame selbst gueltig ist (MAC/CRC stimmen, ukfe_rf_parse_frame
//      akzeptiert ihn anstandslos), dessen Nutzlast fuer UkfeRfCmdTrigger aber um
//      die vollen 4 delay_ms-Bytes zu kurz ist -- ein Empfaenger, der dem in
//      rf/ukfe_rf.h dokumentierten Format folgt, kann delay_ms aus diesem Frame gar
//      nicht lesen. Kein Speicherfehler (memcpy kopiert exakt arg_len=1 Byte in den
//      Frame, es werden keine unbelegten Stack-Bytes uebertragen) -- rein ein
//      Wire-Format-Korrektheitsproblem, still, ohne jede Fehlermeldung.
//   2) FIX (NEU): send_fn() ruft fuer UkfeRfCmdTrigger gezielt das bereits
//      vorhandene, im gesamten Baum bislang an keiner einzigen Stelle aufgerufene
//      ukfe_rf_make_trigger() (rf/ukfe_rf.c:125) auf -- baut korrekt einen
//      5-Byte-Frame (id + delay_ms LE), delay_ms=0 (sofort ausloesen, analog zum
//      bereits produktiv genutzten ukfe_uart_send_trigger(app, id, 0) in
//      scenes/scene_payload_list.c).
//   3) Alle anderen Katalog-Eintraege (1-Byte- und 0-Byte-Args) bleiben unveraendert
//      -- der Fix ist ein einziger, gezielter Sonderfall vor dem generischen Pfad.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ukfe_rf.h"

// Test-Secret (16 Byte) -- beliebig, muss nur auf Sender-/Empfaengerseite
// dieses Repros identisch sein; hat keinen Bezug zum echten, geheimen
// RF_SECRET aus rf/secret.h (das bleibt ungepusht, siehe CONSTRAINTS.md).
static const uint8_t TEST_SECRET[UKFE_RF_SECRET_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01,
};

// Minimaler Ausschnitt aus command_catalog.h -- nur das fuer diesen Fund
// relevante FN_PAYLOAD[0]-Format (name/cmd/arg/has_arg), 1:1 aus der echten
// Datei uebernommen.
typedef struct {
    uint8_t cmd;
    uint8_t arg;
    bool    has_arg;
} UkfeFnMini;

static const UkfeFnMini TRIGGER_ENTRY = {UkfeRfCmdTrigger, 0, true};

// ---- ALT: send_fn() aus scenes/scene_menu.c vor dem Fix (Ist-Zustand) ----
static void send_fn_old(const UkfeFnMini* fn, UkfeRfMessage* m) {
    m->cmd = fn->cmd;
    m->arg_len = fn->has_arg ? 1 : 0;
    if(fn->has_arg) m->args[0] = fn->arg;
}

// ---- NEU: send_fn() mit dem UkfeRfCmdTrigger-Sonderfall ----
static void send_fn_new(const UkfeFnMini* fn, UkfeRfMessage* m) {
    if(fn->cmd == UkfeRfCmdTrigger) {
        ukfe_rf_make_trigger(m, fn->arg, 0);
    } else {
        m->cmd = fn->cmd;
        m->arg_len = fn->has_arg ? 1 : 0;
        if(fn->has_arg) m->args[0] = fn->arg;
    }
}

int main(void) {
    // ---- Szenario 1: BUG -- ALT baut fuer "Trigger [V3]" einen 1-Byte-Frame ----
    {
        UkfeRfMessage m = {0};
        send_fn_old(&TRIGGER_ENTRY, &m);
        m.counter = 1;

        assert(m.cmd == UkfeRfCmdTrigger);
        assert(m.arg_len == 1);   // BUG: Protokoll verlangt 5 (id + delay_ms)

        uint8_t frame[UKFE_RF_MAX_FRAME];
        size_t n = ukfe_rf_build_frame(TEST_SECRET, &m, frame, sizeof(frame));
        assert(n == UKFE_RF_HDR_OVERHEAD + 1);   // nur 1 Arg-Byte im Frame

        // Frame ist als ukfe_rf-Frame selbst voellig gueltig (MAC/CRC stimmen) --
        // der Fehler ist unsichtbar auf Protokollebene, faellt hier nicht auf.
        UkfeRfMessage parsed;
        bool ok = ukfe_rf_parse_frame(TEST_SECRET, frame, n, &parsed, NULL);
        assert(ok);
        assert(parsed.arg_len == 1);

        // Ein Empfaenger, der dem in ukfe_rf.h dokumentierten Format folgt
        // (id=args[0], delay_ms=LE32(args[1..4])), kann delay_ms aus diesem
        // Frame gar nicht lesen -- die dafuer noetigen 4 Byte wurden nie
        // uebertragen (n ist um genau 4 Byte kuerzer als beim korrekten Frame,
        // siehe Szenario 2).
        printf("BUG:  ALT sendet Trigger-Frame mit arg_len=%u (Frame %zu Byte) --\n"
               "      Protokoll (ukfe_rf.h:24) verlangt 5 Byte (id + delay_ms LE),\n"
               "      delay_ms fehlt auf der Leitung komplett.\n",
               parsed.arg_len, n);
    }

    // ---- Szenario 2: FIX -- NEU baut denselben Katalog-Eintrag korrekt (5 Byte) ----
    {
        UkfeRfMessage m = {0};
        send_fn_new(&TRIGGER_ENTRY, &m);
        m.counter = 1;

        assert(m.cmd == UkfeRfCmdTrigger);
        assert(m.arg_len == 5);   // FIX: id (1) + delay_ms (4, LE)
        assert(m.args[0] == TRIGGER_ENTRY.arg);
        assert(m.args[1] == 0 && m.args[2] == 0 && m.args[3] == 0 && m.args[4] == 0);

        uint8_t frame[UKFE_RF_MAX_FRAME];
        size_t n = ukfe_rf_build_frame(TEST_SECRET, &m, frame, sizeof(frame));
        assert(n == UKFE_RF_HDR_OVERHEAD + 5);   // exakt 4 Byte mehr als ALT

        UkfeRfMessage parsed;
        bool ok = ukfe_rf_parse_frame(TEST_SECRET, frame, n, &parsed, NULL);
        assert(ok);
        assert(parsed.arg_len == 5);
        uint8_t  id       = parsed.args[0];
        uint32_t delay_ms = (uint32_t)parsed.args[1] | ((uint32_t)parsed.args[2] << 8) |
            ((uint32_t)parsed.args[3] << 16) | ((uint32_t)parsed.args[4] << 24);
        assert(id == TRIGGER_ENTRY.arg);
        assert(delay_ms == 0);
        printf("FIX:  NEU sendet Trigger-Frame mit arg_len=%u (Frame %zu Byte) --\n"
               "      id=%u delay_ms=%u korrekt aus dem geparsten Frame rekonstruiert.\n",
               parsed.arg_len, n, id, delay_ms);
    }

    // ---- Szenario 3: FIX -- Rundlauf ueber mehrere id/delay_ms-Kombinationen,
    //      inklusive Randfaellen (id=0/255, delay_ms=0/1/UINT32_MAX/typische
    //      Werte), direkt ueber das bereits vorhandene ukfe_rf_make_trigger() ----
    {
        static const struct { uint8_t id; uint32_t delay_ms; } cases[] = {
            {0, 0}, {255, 0}, {1, 1}, {7, 250}, {42, 5000},
            {255, 0xFFFFFFFFu}, {0, 0xFFFFFFFFu},
        };
        for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            UkfeRfMessage m = {0};
            ukfe_rf_make_trigger(&m, cases[i].id, cases[i].delay_ms);
            m.counter = (uint32_t)i + 1;
            assert(m.arg_len == 5);

            uint8_t frame[UKFE_RF_MAX_FRAME];
            size_t n = ukfe_rf_build_frame(TEST_SECRET, &m, frame, sizeof(frame));
            assert(n == UKFE_RF_HDR_OVERHEAD + 5);

            UkfeRfMessage parsed;
            bool ok = ukfe_rf_parse_frame(TEST_SECRET, frame, n, &parsed, NULL);
            assert(ok);
            uint8_t  id       = parsed.args[0];
            uint32_t delay_ms = (uint32_t)parsed.args[1] | ((uint32_t)parsed.args[2] << 8) |
                ((uint32_t)parsed.args[3] << 16) | ((uint32_t)parsed.args[4] << 24);
            assert(id == cases[i].id);
            assert(delay_ms == cases[i].delay_ms);
        }
        printf("FIX:  %zu id/delay_ms-Kombinationen (inkl. Randfaellen 0/255 und "
               "0/UINT32_MAX) verlustfrei rundgelaufen.\n",
               sizeof(cases) / sizeof(cases[0]));
    }

    // ---- Szenario 4: Regression -- ein 1-Byte-Katalog-Eintrag (z. B.
    //      UkfeRfCmdPayloadRun, "args: uint8 idx") bleibt durch den Fix
    //      unveraendert, da send_fn_new() den generischen Pfad fuer jeden
    //      cmd != UkfeRfCmdTrigger unangetastet laesst ----
    {
        const UkfeFnMini payload_run = {UkfeRfCmdPayloadRun, 3, true};
        UkfeRfMessage m_old = {0}, m_new = {0};
        send_fn_old(&payload_run, &m_old);
        send_fn_new(&payload_run, &m_new);
        assert(m_old.cmd == m_new.cmd);
        assert(m_old.arg_len == m_new.arg_len && m_new.arg_len == 1);
        assert(m_old.args[0] == m_new.args[0] && m_new.args[0] == 3);

        const UkfeFnMini wifi_scan = {UkfeRfCmdWifiScan, 0, false};
        UkfeRfMessage m_old2 = {0}, m_new2 = {0};
        send_fn_old(&wifi_scan, &m_old2);
        send_fn_new(&wifi_scan, &m_new2);
        assert(m_old2.arg_len == 0 && m_new2.arg_len == 0);
        printf("REGR: 1-Byte- (PayloadRun) und 0-Byte-Eintraege (WifiScan) "
               "bleiben ALT==NEU identisch.\n");
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
