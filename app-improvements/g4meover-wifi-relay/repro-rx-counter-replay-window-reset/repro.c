// Host-Repro: Flipper->Relay-Anti-Replay-Counter (g4meover-wifi-relay::s_rxCounter) lebt nur im
// ESP32-RAM -> nach einem Geraete-Neustart wird der Replay-Schutz komplett zurueckgesetzt.
// Nutzt das unveraenderte, plattformunabhaengige Protokoll aus ukfe_rf.c/.h (Original-Dateien
// aus g4meover-wifi-relay, hier 1:1 uebernommen) -- keine ESP32/Arduino/ESP-NOW-Abhaengigkeit.
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

// Baut ein gueltiges Wire-Frame fuer einen WifiDeauth-Befehl mit gegebenem Counter (wie es der
// Flipper per UART an den WROOM-Relay senden wuerde -- der Relay rebroadcastet dasselbe Frame
// unveraendert per ESP-NOW an alle Satelliten).
static size_t build_wifi_deauth(uint32_t counter, uint8_t channel, uint8_t* out, size_t out_cap) {
    static const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    UkfeRfMessage m = {0};
    m.counter = counter;
    ukfe_rf_make_wifi_deauth(&m, bssid, channel);
    return ukfe_rf_build_frame(SECRET, &m, out, out_cap);
}

// ---- Vorgeschlagene Fix-Logik (identisch zum bereits verifizierten Muster aus
// app-improvements/usb-army-penetrator/, app-improvements/heltec-v4-hub/ und
// app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md, hier auf
// g4meover-wifi-relay's s_rxCounter angewendet) ----
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

    // ============ Szenario 1: IST-ZUSTAND (static uint32_t s_rxCounter = 0, RAM-only) ============
    {
        uint32_t last_counter = 0;   // == s_rxCounter Startwert in main.cpp
        int rejected = 0;
        for(uint32_t c = 1; c <= 50; c++) {
            size_t n = build_wifi_deauth(c, 6, frame, sizeof(frame));
            UkfeRfMessage msg;
            bool ok = ukfe_rf_parse_frame(SECRET, frame, n, &msg, &last_counter);
            if(!ok) rejected++;
            if(c == 25) { // Angreifer sniffed dieses gueltige Flipper->Relay-Frame mit (UART ist
                          // physisch zugaenglich, ESP-NOW-Rebroadcast zusaetzlich unverschluesselt
                          // -- nur die ukfe_rf-MAC schuetzt den Inhalt)
                captured_len = n;
                memcpy(captured_frame, frame, n);
            }
        }
        printf("Szenario 1 (Sitzung vor Neustart): %d/50 abgelehnt (sollte 0 sein)\n", rejected);
        assert(rejected == 0);

        // Relay stuerzt ab / Stromausfall / versehentlicher Reset / OTA-Update -> setup() laeuft
        // erneut, s_rxCounter ist ein reines RAM-Feld und damit wieder 0.
        last_counter = 0;

        UkfeRfMessage replayed;
        bool accepted = ukfe_rf_parse_frame(SECRET, captured_frame, captured_len, &replayed, &last_counter);
        printf("BUG:  mitgeschnittenes WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> %s\n",
               accepted ? "ANGENOMMEN (Replay erfolgreich -> wird an alle Satelliten rebroadcastet!)" : "abgelehnt");
        assert(accepted); // demonstriert die Schwachstelle: ein zuvor gesendeter Angriffsbefehl
                          // kann nach jedem Neustart des Relays erneut an alle Satelliten
                          // weitergeleitet werden
    }

    // ============ Szenario 2: FIX (persistierte Reserve mit Sicherheitsspanne, wie Preferences es koennte) ============
    {
        uint32_t persisted_floor = 0;   // simuliert NVS-Key "rxctr" im Namespace "g4meover" (anfangs leer)
        uint32_t last_counter;

        // -- "setup()" nach Erst-Inbetriebnahme --
        last_counter = persisted_floor;                                   // geladener Stand
        persisted_floor = counter_initial_reserve(persisted_floor, MARGIN);
        // (NVS-Schreibzugriff hier)

        int rejected = 0;
        uint32_t bumps = 0;
        for(uint32_t c = 1; c <= 50; c++) {
            size_t n = build_wifi_deauth(c, 6, frame, sizeof(frame));
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

        // -- Neustart: setup() laedt den persistierten Floor statt bei 0 zu beginnen --
        last_counter = persisted_floor;

        UkfeRfMessage replayed;
        bool accepted = ukfe_rf_parse_frame(SECRET, captured_frame, captured_len, &replayed, &last_counter);
        printf("FIX:  dasselbe mitgeschnittene Frame (Counter 25) nach Neustart erneut gesendet -> %s\n",
               accepted ? "ANGENOMMEN" : "abgelehnt (Replay verhindert)");
        assert(!accepted);

        // -- Legitime neue Befehle vom Flipper funktionieren nach dem Neustart weiterhin --
        size_t n2 = build_wifi_deauth(persisted_floor + 1, 11, frame, sizeof(frame));
        UkfeRfMessage fresh;
        bool fresh_ok = ukfe_rf_parse_frame(SECRET, frame, n2, &fresh, &last_counter);
        printf("FIX:  neuer, legitimer Befehl (Counter %u) nach Neustart -> %s\n",
               persisted_floor + 1, fresh_ok ? "angenommen" : "abgelehnt");
        assert(fresh_ok);
        uint32_t new_floor;
        if(counter_needs_bump(last_counter, persisted_floor, MARGIN, &new_floor)) {
            persisted_floor = new_floor; // wie im echten scan_and_relay() nach jedem akzeptierten Frame
        }

        // -- Crash-Sicherheit: Absturz zwischen zwei Bumps verliert nie einen bereits akzeptierten Counter --
        // persisted_floor ist per Konstruktion immer >= letzter akzeptierter Counter + MARGIN/2,
        // auch wenn der zuletzt *tatsaechlich erreichte* last_counter nie persistiert wurde.
        assert(persisted_floor >= fresh.counter);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
