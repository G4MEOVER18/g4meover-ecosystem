// Host-Repro: Hub-seitiger Broadcast-Counter (heltec-v4-hub::s_txCounter, hub_broadcast())
// lebt nur im ESP32-RAM -> nach einem Geraete-Neustart startet er wieder bei
// UKFE_TX_COUNTER_START=100, obwohl ein Satellit vom Hub bereits hoehere Counter-Werte
// akzeptiert haben kann. Nutzt das unveraenderte, plattformunabhaengige Protokoll aus
// ukfe_rf.c/.h (Original-Dateien aus heltec-v4-hub, hier 1:1 uebernommen) -- keine
// ESP32/RadioLib/Arduino-Abhaengigkeit.
//
// Anders als beim bereits dokumentierten RX-seitigen Fund (s_flipperCounter, siehe
// rf-counter-replay-window-reset-on-reboot-fix.md) geht es hier NICHT um eine
// Sicherheitsluecke (ein Angreifer gewinnt nichts durch den Reset), sondern um eine reine
// Selbstaussperrung: der Hub kann seine eigenen "Status"-Broadcasts an die Satelliten nach
// einem Neustart fuer eine unbestimmte Zahl an Versuchen nicht mehr zustellen, bis sein
// lokaler Counter den von den Satelliten zuletzt akzeptierten Stand wieder ueberholt hat --
// exakt dasselbe Muster wie app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md.
//
// Baut:  gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c ukfe_rf.c
// Laeuft: ./repro
#include "ukfe_rf.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define UKFE_TX_COUNTER_START 100u   // == s_txCounter-Initialisierung in main.cpp

static const uint8_t SECRET[UKFE_RF_SECRET_LEN] = {
    0x47, 0x34, 0x4D, 0x45, 0x4F, 0x56, 0x45, 0x52,
    0xA5, 0x5A, 0xC3, 0x3C, 0x0F, 0xF0, 0x99, 0x66,
};

// Baut ein gueltiges Wire-Frame fuer einen bestimmten Counter (wie es hub_broadcast() per
// ESP-NOW an die Satelliten senden wuerde).
static size_t build(uint32_t counter, uint8_t* out, size_t out_cap) {
    UkfeRfMessage m = {0};
    m.counter = counter;
    ukfe_rf_make_simple(&m, UkfeRfCmdStatus);
    return ukfe_rf_build_frame(SECRET, &m, out, out_cap);
}

// ---- Satelliten-seitige Pruefung aus ukfe_rf_parse_frame() (Anti-Replay-Regel selbst) ----
// Nachgestellt anhand von heltec-ukfe-rx/src/main.cpp (enowCounter) bzw. lilygo-ukfe-rx: beide
// nutzen ukfe_rf_parse_frame() mit einem persistenten last_counter fuer per ESP-NOW empfangene
// Frames.
static bool satellite_accept(uint32_t counter, uint8_t* frame, size_t n, uint32_t* last_counter) {
    UkfeRfMessage msg;
    return ukfe_rf_parse_frame(SECRET, frame, n, &msg, last_counter) && msg.counter == counter;
}

// ---- Vorgeschlagene Fix-Logik (identisches Muster zu rf-counter-restart-lockout-fix.md /
// rf-counter-replay-window-reset-on-reboot-fix.md, hier auf s_txCounter angewendet) ----
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

    // ============ Szenario 1: IST-ZUSTAND (static uint32_t s_txCounter = 100, RAM-only) ============
    {
        uint32_t sat_last = 0;             // Satellit hat vorher noch nie einen Hub-Broadcast gesehen
        uint32_t hub_tx = UKFE_TX_COUNTER_START;

        // Sitzung 1: Hub broadcastet 5x "Status" (z.B. ueber die Menue-Taste PG_STATUS/PG_SATS)
        int rejected_s1 = 0;
        for(int i = 0; i < 5; i++) {
            hub_tx++;                       // ++s_txCounter in hub_broadcast()
            size_t n = build(hub_tx, frame, sizeof(frame));
            if(!satellite_accept(hub_tx, frame, n, &sat_last)) rejected_s1++;
        }
        printf("Szenario 1 (Sitzung vor Neustart): %d/5 abgelehnt (sollte 0 sein), "
               "Satellit zuletzt akzeptiert: Counter %u\n", rejected_s1, sat_last);
        assert(rejected_s1 == 0);
        assert(sat_last == UKFE_TX_COUNTER_START + 5);

        // Hub stuerzt ab / Akku leer / versehentlicher Reset / OTA-Update -> setup() laeuft
        // erneut, s_txCounter ist ein reines RAM-Feld und damit wieder UKFE_TX_COUNTER_START.
        hub_tx = UKFE_TX_COUNTER_START;

        // Sitzung 2 nach Neustart: alle Broadcasts mit Counter <= sat_last werden abgelehnt,
        // bis hub_tx den vorherigen Stand (105) wieder ueberholt hat.
        int rejected_s2 = 0;
        for(int i = 0; i < 20; i++) {
            hub_tx++;
            size_t n = build(hub_tx, frame, sizeof(frame));
            if(!satellite_accept(hub_tx, frame, n, &sat_last)) rejected_s2++;
        }
        printf("BUG:  Sitzung 2 nach Neustart: %d/20 Status-Broadcasts vom Satelliten "
               "verworfen (Hub kann sich erst nach Aufholen wieder melden)\n", rejected_s2);
        assert(rejected_s2 == 5); // Counter 101..105 (<=105) abgelehnt, erst 106 wieder akzeptiert
    }

    // ============ Szenario 2: FIX (persistierte Reserve mit Sicherheitsspanne, wie Preferences
    // es unter dem NVS-Key "txCtr" im bereits etablierten Namespace "g4meoverv4" koennte) =======
    {
        uint32_t sat_last = 0;
        uint32_t persisted_floor = 0;   // NVS-Key "txCtr", anfangs leer (0)

        // -- "setup()" nach Erst-Inbetriebnahme: loadedTxFloor(0) <= 100 -> hub_tx bleibt 100 --
        uint32_t hub_tx = UKFE_TX_COUNTER_START;
        if(persisted_floor > hub_tx) hub_tx = persisted_floor;
        persisted_floor = counter_initial_reserve(hub_tx, MARGIN);
        // (NVS-Schreibzugriff hier)

        int rejected_s1 = 0;
        uint32_t bumps = 0;
        for(int i = 0; i < 5; i++) {
            hub_tx++;
            size_t n = build(hub_tx, frame, sizeof(frame));
            if(!satellite_accept(hub_tx, frame, n, &sat_last)) { rejected_s1++; continue; }
            uint32_t new_floor;
            if(counter_needs_bump(hub_tx, persisted_floor, MARGIN, &new_floor)) {
                persisted_floor = new_floor;   // NVS-Schreibzugriff (selten)
                bumps++;
            }
        }
        printf("Szenario 2 (Sitzung vor Neustart, mit Fix): %d/5 abgelehnt, %u NVS-"
               "Schreibzugriffe\n", rejected_s1, bumps);
        assert(rejected_s1 == 0);

        // -- Neustart: setup() laedt den persistierten Floor; da er > UKFE_TX_COUNTER_START,
        //    uebernimmt er ihn statt bei 100 zu beginnen --
        uint32_t loadedTxFloor = persisted_floor;
        hub_tx = UKFE_TX_COUNTER_START;
        if(loadedTxFloor > hub_tx) hub_tx = loadedTxFloor;
        persisted_floor = counter_initial_reserve(hub_tx, MARGIN);

        int rejected_s2 = 0;
        for(int i = 0; i < 20; i++) {
            hub_tx++;
            size_t n = build(hub_tx, frame, sizeof(frame));
            if(!satellite_accept(hub_tx, frame, n, &sat_last)) rejected_s2++;
        }
        printf("FIX:  Sitzung 2 nach Neustart: %d/20 Status-Broadcasts vom Satelliten "
               "verworfen\n", rejected_s2);
        assert(rejected_s2 == 0); // kein einziger Broadcast wird nach dem Neustart abgelehnt
    }

    // ============ Szenario 3: FIX uebersteht unsauberen Absturz (kein finales Speichern) =======
    {
        uint32_t sat_last = 0;
        uint32_t persisted_floor = 0;

        uint32_t hub_tx = UKFE_TX_COUNTER_START;
        persisted_floor = counter_initial_reserve(hub_tx, MARGIN);   // 110, gespeichert

        uint32_t max_counter_used = hub_tx;
        for(int i = 0; i < 30; i++) {                                 // ueberschreitet die Bump-Schwelle
            hub_tx++;
            max_counter_used = hub_tx;
            size_t n = build(hub_tx, frame, sizeof(frame));
            satellite_accept(hub_tx, frame, n, &sat_last);
            uint32_t new_floor;
            if(counter_needs_bump(hub_tx, persisted_floor, MARGIN, &new_floor)) {
                persisted_floor = new_floor;
            }
        }
        assert(persisted_floor > UKFE_TX_COUNTER_START + 10);
        assert(persisted_floor >= max_counter_used + MARGIN / 2);
        uint32_t storage_at_crash = persisted_floor;   // "Stromausfall" hier, kein weiteres Save

        // Neustart: laedt storage_at_crash (garantiert > max_counter_used == sat_last)
        uint32_t loadedTxFloor = storage_at_crash;
        hub_tx = UKFE_TX_COUNTER_START;
        if(loadedTxFloor > hub_tx) hub_tx = loadedTxFloor;
        assert(hub_tx > max_counter_used);

        hub_tx++;
        size_t n = build(hub_tx, frame, sizeof(frame));
        bool accepted = satellite_accept(hub_tx, frame, n, &sat_last);
        printf("FIX:  nach unsauberem Absturz bei Counter %u (Reserve war bei %u): erster "
               "Broadcast nach Neustart (Counter %u) sofort akzeptiert -> %s\n",
               max_counter_used, storage_at_crash, hub_tx, accepted ? "ja" : "nein");
        assert(accepted);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
