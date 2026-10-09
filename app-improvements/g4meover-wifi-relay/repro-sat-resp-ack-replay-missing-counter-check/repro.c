// Host-Repro: g4meover-wifi-relay, on_espnow_recv() (Antworten der Satelliten)
// Nutzt die unveraenderten Original-Dateien ukfe_rf.c/ukfe_rf.h 1:1 aus
// g4meover-wifi-relay (laut eigenem Dateikommentar "plattformunabhaengig, ohne
// HW-Abhaengigkeit") -- keine ESP32/Arduino-Abhaengigkeit noetig.
//
// Zeigt:
//  Szenario 1 (Ist-Zustand): on_espnow_recv() ruft ukfe_rf_parse_frame(..., NULL) --
//    ein einmal mitgeschnittenes, gueltig signiertes Antwort-Frame eines Satelliten
//    wird JEDERZEIT erneut akzeptiert, beliebig oft, unabhaengig vom bisherigen
//    Counter-Verlauf.
//  Szenario 2 (Fix): sat_counter_slot() -- ein eigenes Anti-Replay-Fenster je
//    Absender-MAC. Dasselbe mitgeschnittene Frame wird abgelehnt; neue legitime
//    Frames vom selben UND von einem zweiten, unabhaengig zaehlenden Satelliten
//    werden weiterhin akzeptiert (keine Kollision zwischen den Zaehlerraeumen).
//  Szenario 3 (Fix, Tabelle voll): mehr als SAT_COUNTER_TABLE_SIZE unterschiedliche
//    Absender -- sat_counter_slot() faellt auf NULL zurueck statt einen weiteren
//    legitimen Satelliten grundlos abzuweisen oder einen Pufferueberlauf zu riskieren.
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ukfe_rf.h"

static const uint8_t SECRET[UKFE_RF_SECRET_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01
};

// Baut ein UkfeRfRespAck-Frame (so, wie ein Satellit es ueber send_espnow_ack() an
// den Relay schickt: args[0]=orig_cmd, args[1]=result).
static size_t build_ack(uint32_t counter, uint8_t orig_cmd, uint8_t result, uint8_t* out) {
    UkfeRfMessage m;
    m.cmd = UkfeRfRespAck;
    m.arg_len = 2;
    m.args[0] = orig_cmd;
    m.args[1] = result;
    m.counter = counter;
    return ukfe_rf_build_frame(SECRET, &m, out, UKFE_RF_MAX_FRAME);
}

// ---- Fix-Logik 1:1 aus main_cpp.patch (sat_counter_slot) ----
#define SAT_COUNTER_TABLE_SIZE 8u
typedef struct { uint8_t mac[6]; uint32_t counter; int used; } SatCounterEntry;
static SatCounterEntry s_satCounters[SAT_COUNTER_TABLE_SIZE];

static uint32_t* sat_counter_slot(const uint8_t mac[6]) {
    for (size_t i = 0; i < SAT_COUNTER_TABLE_SIZE; i++) {
        if (s_satCounters[i].used && memcmp(s_satCounters[i].mac, mac, 6) == 0)
            return &s_satCounters[i].counter;
    }
    for (size_t i = 0; i < SAT_COUNTER_TABLE_SIZE; i++) {
        if (!s_satCounters[i].used) {
            memcpy(s_satCounters[i].mac, mac, 6);
            s_satCounters[i].counter = 0;
            s_satCounters[i].used = 1;
            return &s_satCounters[i].counter;
        }
    }
    return NULL;
}

static const uint8_t MAC_SAT_A[6] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x01};
static const uint8_t MAC_SAT_B[6] = {0xBB, 0x00, 0x00, 0x00, 0x00, 0x02};

int main(void) {
    uint8_t frame[UKFE_RF_MAX_FRAME];
    UkfeRfMessage msg;

    // ================= Szenario 1: Ist-Zustand (NULL = kein Replay-Check) =================
    printf("Szenario 1 (Ist-Zustand, on_espnow_recv mit NULL):\n");

    // Satellit A quittiert fuenf nacheinander ausgefuehrte Befehle, Counter 1..5.
    size_t captured_len = 0;
    uint8_t captured[UKFE_RF_MAX_FRAME];
    int ok_count = 0;
    for (uint32_t c = 1; c <= 5; c++) {
        size_t n = build_ack(c, UkfeRfCmdWifiDeauth, 0, frame);
        assert(n > 0);
        bool ok = ukfe_rf_parse_frame(SECRET, frame, n, &msg, NULL);
        if (ok) ok_count++;
        if (c == 3) { memcpy(captured, frame, n); captured_len = n; }  // "vom Angreifer mitgeschnitten"
    }
    printf("  %d/5 reguläre ACKs akzeptiert (sollte 5 sein)\n", ok_count);

    // Angreifer spielt das bei Counter=3 mitgeschnittene Frame danach beliebig oft erneut ein --
    // obwohl der Relay zwischenzeitlich bereits Counter 4 und 5 gesehen hat.
    int replay_accepted = 0;
    for (int i = 0; i < 10; i++) {
        if (ukfe_rf_parse_frame(SECRET, captured, captured_len, &msg, NULL)) replay_accepted++;
    }
    printf("  BUG:  mitgeschnittenes ACK (Counter 3, orig_cmd=WifiDeauth) 10x erneut eingespielt "
           "-> %d/10 ANGENOMMEN (erwartet: 0 -- Relay reicht jedes Mal einen gefaelscht "
           "'frischen' Befehlsstatus an den Flipper-UART weiter)\n", replay_accepted);
    assert(ok_count == 5);
    assert(replay_accepted == 10);   // Bug: jede Wiederholung wird angenommen

    // ================= Szenario 2: Fix (sat_counter_slot, pro Absender-MAC) =================
    printf("\nSzenario 2 (Fix, eigenes Fenster je Absender-MAC):\n");
    memset(s_satCounters, 0, sizeof(s_satCounters));

    ok_count = 0;
    for (uint32_t c = 1; c <= 5; c++) {
        size_t n = build_ack(c, UkfeRfCmdWifiDeauth, 0, frame);
        if (ukfe_rf_parse_frame(SECRET, frame, n, &msg, sat_counter_slot(MAC_SAT_A))) ok_count++;
    }
    printf("  FIX:  %d/5 reguläre ACKs von Satellit A akzeptiert (sollte 5 sein)\n", ok_count);
    assert(ok_count == 5);

    // Dasselbe mitgeschnittene Frame (Counter 3) erneut einspielen -> jetzt abgelehnt.
    bool replay_ok = ukfe_rf_parse_frame(SECRET, captured, captured_len, &msg,
                                          sat_counter_slot(MAC_SAT_A));
    printf("  FIX:  dasselbe mitgeschnittene ACK (Counter 3) erneut eingespielt -> %s "
           "(erwartet: abgelehnt)\n", replay_ok ? "ANGENOMMEN" : "abgelehnt");
    assert(!replay_ok);

    // Satellit B (eigene MAC, eigener, unabhaengig bei 1 startender Zaehlerraum) sendet parallel --
    // darf NICHT durch Satellit As bereits hohen Stand (5) blockiert werden.
    size_t nb = build_ack(1, UkfeRfCmdWifiScan, 0, frame);
    bool sat_b_ok = ukfe_rf_parse_frame(SECRET, frame, nb, &msg, sat_counter_slot(MAC_SAT_B));
    printf("  FIX:  Satellit B, erstes ACK (eigener Counter 1, trotz Satellit-A-Stand 5) -> %s "
           "(erwartet: angenommen -- keine Kollision zwischen den Zaehlerraeumen)\n",
           sat_b_ok ? "angenommen" : "ABGELEHNT");
    assert(sat_b_ok);

    // Satellit A, neuer legitimer Befehl (Counter 6) -> weiterhin angenommen.
    size_t na6 = build_ack(6, UkfeRfCmdWifiStop, 0, frame);
    bool a6_ok = ukfe_rf_parse_frame(SECRET, frame, na6, &msg, sat_counter_slot(MAC_SAT_A));
    printf("  FIX:  Satellit A, neuer legitimer Befehl (Counter 6) -> %s (erwartet: angenommen)\n",
           a6_ok ? "angenommen" : "ABGELEHNT");
    assert(a6_ok);

    // ================= Szenario 3: Fix, Tabelle voll (Fallback-Pfad) =================
    printf("\nSzenario 3 (Fix, Tabelle voll -- Fallback-Pfad):\n");
    memset(s_satCounters, 0, sizeof(s_satCounters));
    uint8_t mac[6] = {0xCC, 0, 0, 0, 0, 0};
    int distinct_slots = 0;
    for (int i = 0; i < (int)SAT_COUNTER_TABLE_SIZE; i++) {
        mac[5] = (uint8_t)i;
        if (sat_counter_slot(mac) != NULL) distinct_slots++;
    }
    printf("  %d/%u Slots fuer %u unterschiedliche Absender belegt (sollte alle sein)\n",
           distinct_slots, SAT_COUNTER_TABLE_SIZE, SAT_COUNTER_TABLE_SIZE);
    assert(distinct_slots == (int)SAT_COUNTER_TABLE_SIZE);

    mac[5] = (uint8_t)SAT_COUNTER_TABLE_SIZE;  // (SAT_COUNTER_TABLE_SIZE+1)-ter, bisher unbekannter Absender
    uint32_t* overflow_slot = sat_counter_slot(mac);
    printf("  (SAT_COUNTER_TABLE_SIZE+1)-ter unbekannter Absender -> Slot == %s "
           "(erwartet: NULL -- faellt auf ungeprueften Pfad zurueck statt abzuweisen)\n",
           overflow_slot ? "gueltig" : "NULL");
    assert(overflow_slot == NULL);
    // Auch im Fallback bleibt das Frame integritaetsgeprueft (MAC+CRC) gueltig -- es wird nur
    // nicht gegen Replay geprueft, exakt das bisherige (dokumentierte) Verhalten.
    size_t nf = build_ack(1, UkfeRfCmdStatus, 0, frame);
    bool fallback_ok = ukfe_rf_parse_frame(SECRET, frame, nf, &msg, overflow_slot);
    printf("  Integritaet im Fallback-Pfad weiterhin geprueft (gueltiges Frame) -> %s "
           "(erwartet: angenommen)\n", fallback_ok ? "angenommen" : "ABGELEHNT");
    assert(fallback_ok);

    printf("\nAlle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
