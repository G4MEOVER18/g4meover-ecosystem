// Host-Repro: ESP-NOW-Satelliten (heltec-ukfe-rx, lilygo-ukfe-rx) pruefen Hub-Broadcasts
// (heltec-v4-hub::hub_broadcast(), eigener s_txCounter-Zaehlerraum) und relayte
// Flipper-Frames (heltec-v4-hub::relay_frame(), Flipper-eigener s_flipperCounter-
// Zaehlerraum) mit EINEM gemeinsamen Anti-Replay-Fenster (enowCounter), obwohl beide
// Quellen unabhaengig voneinander zaehlen. Ein hoher Counter aus der einen Quelle kann
// damit kuenftige, eigentlich gueltige Frames der ANDEREN Quelle sperren, unabhaengig von
// einem Geraete-Neustart. Nutzt das unveraenderte, plattformunabhaengige Protokoll aus
// ukfe_rf.c/.h (Original-Dateien aus heltec-ukfe-rx, hier 1:1 uebernommen) -- keine
// ESP32/RadioLib/Arduino/ESP-NOW-Abhaengigkeit.
//
// Vorgeschlagener Fix: ein zusaetzliches Envelope-Byte VOR dem ukfe_rf-Frame (ausserhalb
// des MAC/CRC-geschuetzten Bereichs, reine Routing-Info), das der Hub beim Senden setzt
// (ESPNOW_ORIGIN_RELAY=0x00 / ESPNOW_ORIGIN_HUBCMD=0x01) und der Satellit beim Empfang
// ausliest, um zwei getrennte Anti-Replay-Fenster zu fuehren. Die Frame-Authentizitaet
// selbst (MAC+CRC) bleibt unveraendert allein Sache von ukfe_rf_parse_frame().
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

#define ESPNOW_ORIGIN_RELAY   0x00u
#define ESPNOW_ORIGIN_HUBCMD  0x01u

// Baut ein gueltiges Wire-Frame fuer einen bestimmten Counter/Befehl.
static size_t build(uint32_t counter, UkfeRfCmd cmd, uint8_t* out, size_t out_cap) {
    UkfeRfMessage m = {0};
    m.counter = counter;
    ukfe_rf_make_simple(&m, cmd);
    return ukfe_rf_build_frame(SECRET, &m, out, out_cap);
}

// ---- IST-ZUSTAND: ein einziges gemeinsames Anti-Replay-Fenster fuer alle ESP-NOW-Frames,
// unabhaengig davon, ob sie vom Hub selbst (hub_broadcast()) oder vom Flipper (relay_frame(),
// unveraendert weitergefunkt) stammen. Nachgestellt anhand von
// heltec-ukfe-rx/src/main.cpp:433 bzw. lilygo-ukfe-rx/src/main.cpp:269 (beide: EIN
// `enowCounter` fuer den gesamten ESP-NOW-Empfangspfad). ----
static bool satellite_accept_shared(uint32_t counter, uint8_t* frame, size_t n,
                                     uint32_t* shared_window) {
    UkfeRfMessage msg;
    return ukfe_rf_parse_frame(SECRET, frame, n, &msg, shared_window) && msg.counter == counter;
}

// ---- FIX: Envelope-Byte vor dem eigentlichen ukfe_rf-Frame. Baut die Senderseite
// (heltec-v4-hub::hub_broadcast()/relay_frame()) nach. ----
static size_t wrap_envelope(uint8_t origin, const uint8_t* frame, size_t frame_len,
                             uint8_t* out, size_t out_cap) {
    if(frame_len + 1 > out_cap) return 0;
    out[0] = origin;
    memcpy(out + 1, frame, frame_len);
    return frame_len + 1;
}

// Baut die Empfangsseite nach (onEspNowRecv()+loop(), gepatchte Fassung): liest das
// Envelope-Byte, verwirft unbekannte Ursprungs-Tags sofort (bevor ueberhaupt geparst
// wird) und reicht den inneren Frame unveraendert an ukfe_rf_parse_frame() weiter --
// mit dem zum Ursprung passenden, getrennten Anti-Replay-Fenster.
static bool satellite_accept_split(const uint8_t* envelope, size_t envelope_len,
                                    uint32_t* relay_window, uint32_t* hubcmd_window,
                                    uint8_t* out_origin, uint32_t* out_counter) {
    if(envelope_len <= 1 || envelope_len > UKFE_RF_MAX_FRAME + 1) return false;
    uint8_t origin = envelope[0];
    if(origin != ESPNOW_ORIGIN_RELAY && origin != ESPNOW_ORIGIN_HUBCMD) return false;
    uint32_t* win = (origin == ESPNOW_ORIGIN_HUBCMD) ? hubcmd_window : relay_window;
    UkfeRfMessage msg;
    if(!ukfe_rf_parse_frame(SECRET, envelope + 1, envelope_len - 1, &msg, win)) return false;
    *out_origin = origin;
    *out_counter = msg.counter;
    return true;
}

int main(void) {
    uint8_t frame[UKFE_RF_MAX_FRAME];
    uint8_t env[1 + UKFE_RF_MAX_FRAME];

    // ============ Szenario 1: IST-ZUSTAND -- gemeinsames Fenster, cross-source lockout ========
    {
        uint32_t shared = 0;

        // Hub meldet 5x "Status" ueber die Menue-Taste (hub_broadcast(), s_txCounter ab 100)
        int rejected_hub = 0;
        uint32_t hub_tx = 100;
        for(int i = 0; i < 5; i++) {
            hub_tx++;
            size_t n = build(hub_tx, UkfeRfCmdStatus, frame, sizeof(frame));
            if(!satellite_accept_shared(hub_tx, frame, n, &shared)) rejected_hub++;
        }
        printf("Szenario 1a: %d/5 Hub-Broadcasts abgelehnt (sollte 0 sein), "
               "gemeinsames Fenster steht bei %u\n", rejected_hub, shared);
        assert(rejected_hub == 0);

        // Der Flipper selbst hat (ueber viele RF-Console-Sitzungen) laengst einen viel
        // hoeheren Counter erreicht (eigener Zaehlerraum, s_flipperCounter) -- der Hub
        // relayt EINEN solchen Flipper-Befehl unveraendert per ESP-NOW weiter.
        uint32_t flipper_ctr = 50000;
        size_t n = build(flipper_ctr, UkfeRfCmdLoraScan, frame, sizeof(frame));
        bool relay_ok = satellite_accept_shared(flipper_ctr, frame, n, &shared);
        printf("Szenario 1b: relayter Flipper-Frame (Counter %u) akzeptiert -> %s, "
               "gemeinsames Fenster jetzt bei %u\n", flipper_ctr, relay_ok ? "ja" : "nein", shared);
        assert(relay_ok);
        assert(shared == flipper_ctr);

        // Der Hub broadcastet als naechstes erneut "Status" (naechster s_txCounter-Wert,
        // voellig unabhaengig vom Flipper-Zaehlerraum) -- wird trotzdem abgelehnt, weil das
        // gemeinsame Fenster inzwischen weit ueber dem naechsten Hub-eigenen Counter steht.
        hub_tx++;
        n = build(hub_tx, UkfeRfCmdStatus, frame, sizeof(frame));
        bool hub_ok = satellite_accept_shared(hub_tx, frame, n, &shared);
        printf("BUG:  naechster Hub-Broadcast (Counter %u, weit unter dem gemeinsamen "
               "Fenster %u durch den fremden Flipper-Counter) -> %s\n",
               hub_tx, flipper_ctr, hub_ok ? "akzeptiert" : "ABGELEHNT");
        assert(!hub_ok);   // Cross-Source-Lockout: kein Neustart noetig, reine Verwechslung der Raeume
    }

    // ============ Szenario 2: FIX -- Envelope-Byte trennt die beiden Zaehlerraeume ============
    {
        uint32_t relay_window = 0, hubcmd_window = 0;

        // Dieselbe Abfolge wie in Szenario 1: 5 Hub-Broadcasts, dann ein hoher relayter
        // Flipper-Frame, dann ein weiterer Hub-Broadcast.
        uint32_t hub_tx = 100;
        int rejected_hub = 0;
        for(int i = 0; i < 5; i++) {
            hub_tx++;
            size_t n = build(hub_tx, UkfeRfCmdStatus, frame, sizeof(frame));
            size_t en = wrap_envelope(ESPNOW_ORIGIN_HUBCMD, frame, n, env, sizeof(env));
            uint8_t origin; uint32_t ctr;
            if(!satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr))
                rejected_hub++;
        }
        printf("Szenario 2a (Fix): %d/5 Hub-Broadcasts abgelehnt (sollte 0 sein), "
               "Hub-Fenster=%u Relay-Fenster=%u\n", rejected_hub, hubcmd_window, relay_window);
        assert(rejected_hub == 0);
        assert(hubcmd_window == hub_tx);
        assert(relay_window == 0);   // vom Hub-Verkehr komplett unberuehrt

        uint32_t flipper_ctr = 50000;
        size_t n = build(flipper_ctr, UkfeRfCmdLoraScan, frame, sizeof(frame));
        size_t en = wrap_envelope(ESPNOW_ORIGIN_RELAY, frame, n, env, sizeof(env));
        uint8_t origin; uint32_t ctr;
        bool relay_ok = satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr);
        printf("Szenario 2b (Fix): relayter Flipper-Frame (Counter %u) akzeptiert -> %s, "
               "Relay-Fenster jetzt bei %u, Hub-Fenster unveraendert bei %u\n",
               flipper_ctr, relay_ok ? "ja" : "nein", relay_window, hubcmd_window);
        assert(relay_ok && origin == ESPNOW_ORIGIN_RELAY && ctr == flipper_ctr);
        assert(relay_window == flipper_ctr);
        assert(hubcmd_window == hub_tx);   // Hub-Fenster bleibt vom fremden Zaehlerraum unberuehrt

        // Der naechste Hub-Broadcast wird jetzt gegen sein EIGENES Fenster geprueft --
        // unabhaengig vom hohen Flipper-Counter -- und damit korrekt akzeptiert.
        hub_tx++;
        n = build(hub_tx, UkfeRfCmdStatus, frame, sizeof(frame));
        en = wrap_envelope(ESPNOW_ORIGIN_HUBCMD, frame, n, env, sizeof(env));
        bool hub_ok = satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr);
        printf("FIX:  naechster Hub-Broadcast (Counter %u) trotz hohem Relay-Fenster -> %s\n",
               hub_tx, hub_ok ? "akzeptiert" : "abgelehnt");
        assert(hub_ok && origin == ESPNOW_ORIGIN_HUBCMD && ctr == hub_tx);
    }

    // ============ Szenario 3: Replay bleibt INNERHALB eines Fensters weiterhin gesperrt =======
    {
        uint32_t relay_window = 0, hubcmd_window = 0;
        uint32_t hub_tx = 101;
        size_t n = build(hub_tx, UkfeRfCmdStatus, frame, sizeof(frame));
        size_t en = wrap_envelope(ESPNOW_ORIGIN_HUBCMD, frame, n, env, sizeof(env));
        uint8_t origin; uint32_t ctr;
        assert(satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr));

        // derselbe Frame (gleicher Counter, gleicher Origin) erneut eingespielt -> Replay
        bool replay_ok = satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr);
        printf("Szenario 3: Replay desselben Hub-Broadcasts -> %s (sollte abgelehnt sein)\n",
               replay_ok ? "akzeptiert" : "abgelehnt");
        assert(!replay_ok);
    }

    // ============ Szenario 4: unbekanntes/kaputtes Envelope-Byte wird sofort verworfen,
    // bevor MAC/CRC ueberhaupt geprueft werden (kein Crash, kein Fenster-Fortschritt) =========
    {
        uint32_t relay_window = 0, hubcmd_window = 0;
        size_t n = build(1, UkfeRfCmdStatus, frame, sizeof(frame));
        size_t en = wrap_envelope(0x42u /* weder RELAY noch HUBCMD */, frame, n, env, sizeof(env));
        uint8_t origin; uint32_t ctr;
        bool ok = satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr);
        printf("Szenario 4: unbekanntes Envelope-Byte (0x42) -> %s\n", ok ? "akzeptiert" : "verworfen");
        assert(!ok);
        assert(relay_window == 0 && hubcmd_window == 0);

        // Envelope mit Laenge 1 (nur das Tag-Byte, kein innerer Frame) -> ebenfalls verworfen.
        uint8_t tiny[1] = {ESPNOW_ORIGIN_RELAY};
        ok = satellite_accept_split(tiny, sizeof(tiny), &relay_window, &hubcmd_window, &origin, &ctr);
        printf("Szenario 4b: Envelope ohne inneren Frame (len=1) -> %s\n", ok ? "akzeptiert" : "verworfen");
        assert(!ok);
    }

    // ============ Szenario 5: Envelope aendert nichts an der Frame-Authentizitaet -- ein
    // manipuliertes Frame mit korrektem Envelope-Byte und korrektem Counter wird weiterhin
    // ueber MAC/CRC abgelehnt (ukfe_rf.c selbst unveraendert) =================================
    {
        uint32_t relay_window = 0, hubcmd_window = 0;
        size_t n = build(1, UkfeRfCmdStatus, frame, sizeof(frame));
        frame[9] ^= 0xFF;   // erstes Arg/MAC-Byte nach dem Header kippen (Faelschungsversuch)
        size_t en = wrap_envelope(ESPNOW_ORIGIN_HUBCMD, frame, n, env, sizeof(env));
        uint8_t origin; uint32_t ctr;
        bool ok = satellite_accept_split(env, en, &relay_window, &hubcmd_window, &origin, &ctr);
        printf("Szenario 5: manipuliertes Frame (gueltiges Envelope-Byte) -> %s "
               "(sollte ueber MAC/CRC verworfen werden)\n", ok ? "akzeptiert" : "verworfen");
        assert(!ok);
        assert(hubcmd_window == 0);   // kein Fenster-Fortschritt durch ein gefaelschtes Frame
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
