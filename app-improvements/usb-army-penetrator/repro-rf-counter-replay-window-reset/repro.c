// Host-Repro: RX-seitiger Anti-Replay-Counter (UkfeRfControl::_lastCounter) lebt nur im
// ESP32-RAM -> nach einem Geraete-Neustart wird der Replay-Schutz komplett zurueckgesetzt.
// Nutzt das unveraenderte, plattformunabhaengige Protokoll aus ukfe_rf.c/.h (Original-Dateien
// aus usb-army-penetrator, hier 1:1 uebernommen) -- keine ESP32/RadioLib/Flipper-Abhaengigkeit.
//
// Baut:  gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c ukfe_rf.c
// Laeuft: ./repro
#include "ukfe_rf.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const uint8_t SECRET[UKFE_RF_SECRET_LEN] = {
    0x47, 0x34, 0x4D, 0x45, 0x4F, 0x56, 0x45, 0x52,
    0xA5, 0x5A, 0xC3, 0x3C, 0x0F, 0xF0, 0x99, 0x66,
};

// Baut ein gueltiges Wire-Frame fuer einen bestimmten Counter (wie es der Flipper senden wuerde).
static size_t build(uint32_t counter, uint8_t* out, size_t out_cap) {
    UkfeRfMessage m = {0};
    m.counter = counter;
    ukfe_rf_make_simple(&m, UkfeRfCmdWifiDeauth);
    return ukfe_rf_build_frame(SECRET, &m, out, out_cap);
}

// ---- Vorgeschlagene Fix-Logik (identisch zum bereits verifizierten TX-seitigen Muster aus
// lora-ukfe/app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md, hier auf den RX-
// seitigen last_counter statt den TX-seitigen send-counter angewendet) ----
#define MARGIN 10u
static uint32_t counter_initial_reserve(uint32_t loaded, uint32_t margin) {
    return loaded + margin;
}
static bool counter_needs_bump(uint32_t counter, uint32_t reserved_up_to,
                                uint32_t margin, uint32_t* new_reserved_up_to) {
    if(counter + margin / 2 < reserved_up_to) return false;
    *new_reserved_up_to = counter + margin;
    return true;
}

int main(void) {
    uint8_t frame[UKFE_RF_MAX_FRAME];
    uint8_t captured_frame[UKFE_RF_MAX_FRAME];
    size_t  captured_len = 0;

    // ============ Szenario 1: IST-ZUSTAND (UkfeRfControl::_lastCounter, RAM-only) ============
    {
        uint32_t last_counter = 0;   // == UkfeRfControl::_lastCounter Startwert
        int rejected = 0;
        for(uint32_t c = 1; c <= 50; c++) {
            size_t n = build(c, frame, sizeof(frame));
            UkfeRfMessage msg;
            bool ok = ukfe_rf_parse_frame(SECRET, frame, n, &msg, &last_counter);
            if(!ok) rejected++;
            if(c == 25) { // Angreifer sniffed dieses gueltige Frame mit (z.B. per SDR)
                captured_len = n;
                memcpy(captured_frame, frame, n);
            }
        }
        printf("Szenario 1 (Sitzung vor Neustart): %d/50 abgelehnt (sollte 0 sein)\n", rejected);
        assert(rejected == 0);

        // ESP32 stuerzt ab / Stromausfall / Neuflash -> begin() laeuft erneut,
        // _lastCounter ist ein reines RAM-Feld und damit wieder 0.
        last_counter = 0;

        UkfeRfMessage replayed;
        bool accepted = ukfe_rf_parse_frame(SECRET, captured_frame, captured_len, &replayed, &last_counter);
        printf("BUG:  captured WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> %s\n",
               accepted ? "ANGENOMMEN (Replay erfolgreich!)" : "abgelehnt");
        assert(accepted); // demonstriert die Schwachstelle: alte, bereits benutzte Befehle wirken nach jedem Neustart wieder
    }

    // ============ Szenario 2: FIX (persistierte Reserve mit Sicherheitsspanne, wie Preferences es koennte) ============
    {
        uint32_t persisted_floor = 0;   // simuliert NVS-Key "ukfeRfCtr" (anfangs leer)
        uint32_t last_counter;

        // -- "begin()" nach Erst-Inbetriebnahme --
        last_counter = persisted_floor;                                   // geladener Stand
        persisted_floor = counter_initial_reserve(persisted_floor, MARGIN);
        // (NVS-Schreibzugriff hier)

        int rejected = 0;
        uint32_t bumps = 0;
        for(uint32_t c = 1; c <= 50; c++) {
            size_t n = build(c, frame, sizeof(frame));
            UkfeRfMessage msg;
            bool ok = ukfe_rf_parse_frame(SECRET, frame, n, &msg, &last_counter);
            if(!ok) { rejected++; continue; }
            uint32_t new_floor;
            if(counter_needs_bump(last_counter, persisted_floor, MARGIN, &new_floor)) {
                persisted_floor = new_floor;   // NVS-Schreibzugriff (selten, s. u.)
                bumps++;
            }
        }
        printf("Szenario 2 (Sitzung vor Neustart, mit Fix): %d/50 abgelehnt, %u NVS-Schreibzugriffe (statt 50)\n",
               rejected, bumps);
        assert(rejected == 0);
        assert(bumps < 50); // deutlich weniger Flash-Schreibzugriffe als ein Write pro Frame

        // -- Neustart: begin() laedt den persistierten Floor statt bei 0 zu beginnen --
        last_counter = persisted_floor;

        UkfeRfMessage replayed;
        bool accepted = ukfe_rf_parse_frame(SECRET, captured_frame, captured_len, &replayed, &last_counter);
        printf("FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> %s\n",
               accepted ? "ANGENOMMEN" : "abgelehnt (Replay verhindert)");
        assert(!accepted);

        // -- Legitime neue Befehle nach dem Neustart funktionieren weiterhin --
        size_t n2 = build(persisted_floor + 1, frame, sizeof(frame));
        UkfeRfMessage fresh;
        bool fresh_ok = ukfe_rf_parse_frame(SECRET, frame, n2, &fresh, &last_counter);
        printf("FIX:  neuer, legitimer Befehl (Counter %u) nach Neustart -> %s\n",
               persisted_floor + 1, fresh_ok ? "angenommen" : "abgelehnt");
        assert(fresh_ok);
        uint32_t new_floor;
        if(counter_needs_bump(last_counter, persisted_floor, MARGIN, &new_floor)) {
            persisted_floor = new_floor; // wie im echten loop() nach jedem akzeptierten Frame
        }

        // -- Crash-Sicherheit: Absturz zwischen zwei Bumps verliert nie einen bereits akzeptierten Counter --
        // persisted_floor ist per Konstruktion immer >= letzter akzeptierter Counter + MARGIN/2,
        // auch wenn der zuletzt *tatsaechlich erreichte* last_counter nie persistiert wurde.
        assert(persisted_floor >= fresh.counter);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
