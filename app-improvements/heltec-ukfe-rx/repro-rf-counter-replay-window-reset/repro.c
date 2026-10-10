// Host-Repro: heltec-ukfe-rx fuehrt ZWEI unabhaengige Anti-Replay-Fenster
// (lastCounter fuer die 868-FSK-Strecke vom Flipper, enowCounter fuer die ESP-NOW-
// Strecke vom WROOM-Relay) -- beide als reines ESP32-RAM-Feld, Startwert 0. Jeder
// Neustart (Reset/Crash/Akku-leer/OTA-Update) setzt beide Fenster unabhaengig
// voneinander auf 0 zurueck. Nutzt das unveraenderte, plattformunabhaengige
// Protokoll aus ukfe_rf.c/.h (Original-Dateien 1:1 aus heltec-ukfe-rx uebernommen) --
// keine ESP32/RadioLib/Arduino-Abhaengigkeit.
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

// Baut ein gueltiges Wire-Frame fuer einen WifiDeauth-Befehl mit gegebenem Counter
// (wie es der Flipper per 868-FSK oder der WROOM-Relay per ESP-NOW senden wuerde).
static size_t build_wifi_deauth(uint32_t counter, uint8_t channel, uint8_t* out, size_t out_cap) {
    UkfeRfMessage m = {0};
    m.counter = counter;
    const uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
    ukfe_rf_make_wifi_deauth(&m, bssid, channel);
    return ukfe_rf_build_frame(SECRET, &m, out, out_cap);
}

// ---- Vorgeschlagene Fix-Logik (identisch zum bereits in usb-army-penetrator/
// heltec-v4-hub/lilygo-ukfe-rx/g4meover-wifi-relay verifizierten Muster, hier auf
// BEIDE unabhaengigen Fenster von heltec-ukfe-rx angewendet) ----
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

// Fuehrt dieselbe Vor-/Nach-Neustart-Replay-Pruefung fuer EIN Fenster aus (wird unten
// fuer das 868-Fenster und separat fuer das ESP-NOW-Fenster aufgerufen -- demonstriert,
// dass beide Fenster unabhaengig voneinander betroffen UND unabhaengig voneinander zu
// fixen sind: je ein eigener NVS-Key, keine gemeinsame Zustandsvariable).
static void run_scenario(const char* label) {
    uint8_t frame[UKFE_RF_MAX_FRAME];
    uint8_t captured_frame[UKFE_RF_MAX_FRAME];
    size_t  captured_len = 0;

    printf("== %s ==\n", label);

    // ---- Szenario A: IST-ZUSTAND (reines RAM-Feld, Startwert 0) ----
    {
        uint32_t last_counter = 0;
        int rejected = 0;
        for(uint32_t c = 1; c <= 50; c++) {
            size_t n = build_wifi_deauth(c, 6, frame, sizeof(frame));
            UkfeRfMessage msg;
            bool ok = ukfe_rf_parse_frame(SECRET, frame, n, &msg, &last_counter);
            if(!ok) rejected++;
            if(c == 25) {   // Angreifer schneidet dieses gueltige Frame per SDR/WiFi-Sniffer mit
                captured_len = n;
                memcpy(captured_frame, frame, n);
            }
        }
        printf("Szenario A (Sitzung vor Neustart): %d/50 abgelehnt (sollte 0 sein)\n", rejected);
        assert(rejected == 0);

        // Geraet stuerzt ab / Akku leer / versehentlicher Reset / OTA-Update -> setup()
        // laeuft erneut, das Fenster ist ein reines RAM-Feld und damit wieder 0.
        last_counter = 0;

        UkfeRfMessage replayed;
        bool accepted = ukfe_rf_parse_frame(SECRET, captured_frame, captured_len, &replayed, &last_counter);
        printf("BUG:  captured WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> %s\n",
               accepted ? "ANGENOMMEN (Replay erfolgreich!)" : "abgelehnt");
        assert(accepted);
    }

    // ---- Szenario B: FIX (persistierte Reserve mit Sicherheitsspanne, wie Preferences es koennte) ----
    {
        uint32_t persisted_floor = 0;   // simuliert den jeweiligen NVS-Key (anfangs leer)
        uint32_t last_counter;

        // -- "setup()" nach Erst-Inbetriebnahme --
        last_counter = persisted_floor;
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
                persisted_floor = new_floor;
                bumps++;
            }
            if(c == 25) { captured_len = n; memcpy(captured_frame, frame, n); }
        }
        printf("Szenario B (Sitzung vor Neustart, mit Fix): %d/50 abgelehnt, %u NVS-Schreibzugriffe (statt 50)\n",
               rejected, bumps);
        assert(rejected == 0);
        assert(bumps < 50);

        // -- Neustart: setup() laedt den persistierten Floor statt bei 0 zu beginnen --
        last_counter = persisted_floor;

        UkfeRfMessage replayed;
        bool accepted = ukfe_rf_parse_frame(SECRET, captured_frame, captured_len, &replayed, &last_counter);
        printf("FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> %s\n",
               accepted ? "ANGENOMMEN" : "abgelehnt (Replay verhindert)");
        assert(!accepted);

        // -- Legitime neue Befehle funktionieren nach dem Neustart weiterhin --
        size_t n2 = build_wifi_deauth(persisted_floor + 1, 11, frame, sizeof(frame));
        UkfeRfMessage fresh;
        bool fresh_ok = ukfe_rf_parse_frame(SECRET, frame, n2, &fresh, &last_counter);
        printf("FIX:  neuer, legitimer Befehl (Counter %u) nach Neustart -> %s\n",
               persisted_floor + 1, fresh_ok ? "angenommen" : "abgelehnt");
        assert(fresh_ok);
    }
}

int main(void) {
    // Beide Fenster sind laut Code-Kommentar in main.cpp bewusst getrennt ("SEPARATES
    // Anti-Replay-Fenster (WiFi)") -- hier je ein eigener Durchlauf, um zu zeigen, dass
    // der Fund (und der Fix) fuer beide unabhaengig gilt und keine gemeinsame Variable
    // o.ae. voraussetzt.
    run_scenario("868-FSK-Fenster (lastCounter, vom Flipper)");
    run_scenario("ESP-NOW-Fenster (enowCounter, vom WROOM-Relay)");

    // ---- Szenario C: beide Fenster sind unabhaengig -- ein Replay im einen Fenster
    // beeinflusst das andere nicht, weder im Ist-Zustand noch nach dem Fix. ----
    {
        uint8_t frame868[UKFE_RF_MAX_FRAME], frameEnow[UKFE_RF_MAX_FRAME];
        size_t n868 = build_wifi_deauth(5, 1, frame868, sizeof(frame868));
        size_t nEnow = build_wifi_deauth(5, 1, frameEnow, sizeof(frameEnow));  // gleicher Counter!

        uint32_t last868 = 0, lastEnow = 0;
        UkfeRfMessage m868, mEnow;
        bool ok868 = ukfe_rf_parse_frame(SECRET, frame868, n868, &m868, &last868);
        bool okEnow = ukfe_rf_parse_frame(SECRET, frameEnow, nEnow, &mEnow, &lastEnow);
        printf("Szenario C: identischer Counter (5) in beiden getrennten Fenstern -> 868=%s, ESPNOW=%s\n",
               ok868 ? "angenommen" : "abgelehnt", okEnow ? "angenommen" : "abgelehnt");
        assert(ok868 && okEnow);   // beide unabhaengig akzeptiert, kein Cross-Window-Lockout

        // Replay im 868-Fenster darf das ESP-NOW-Fenster nicht sperren.
        UkfeRfMessage replay;
        bool replay868 = ukfe_rf_parse_frame(SECRET, frame868, n868, &replay, &last868);
        assert(!replay868);       // im eigenen Fenster korrekt gesperrt
        size_t nEnow2 = build_wifi_deauth(6, 1, frameEnow, sizeof(frameEnow));
        bool okEnow2 = ukfe_rf_parse_frame(SECRET, frameEnow, nEnow2, &mEnow, &lastEnow);
        printf("Szenario C: Replay im 868-Fenster sperrt ESP-NOW-Fenster NICHT -> naechster ESPNOW-Befehl: %s\n",
               okEnow2 ? "angenommen" : "abgelehnt");
        assert(okEnow2);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
