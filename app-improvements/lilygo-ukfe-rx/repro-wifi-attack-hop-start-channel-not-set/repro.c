// Host-Repro (reine Zustandslogik, keine Arduino-/ESP32-Abhaengigkeit): bildet den
// fehlenden Sofort-Kanalwechsel beim Start eines Hopping-Angriffs in
// lilygo-ukfe-rx/src/wifi_attack.cpp::wifi_attack_deauth()/wifi_attack_beacon() nach. Die
// Mock-Funktionen ersetzen esp_wifi_set_channel()/esp_wifi_80211_tx() 1:1 durch
// instrumentierte Varianten, die den aktuellen Funkkanal und den Kanal jedes tatsaechlich
// gesendeten Frames mitprotokollieren -- die Entscheidungslogik selbst (set_channel/
// wifi_attack_deauth/wifi_attack_beacon/wifi_attack_tick) ist Zeile fuer Zeile aus der
// echten Datei uebernommen, einmal unveraendert (ALT) und einmal mit dem Fix (NEU).
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// ---- Mock-Funk-Hardware ----------------------------------------------------
static uint8_t g_radio_channel  = 0;
static uint8_t g_last_tx_channel = 0xFF;   // Kanal, auf dem der letzte Burst tatsaechlich rausging
static int     g_tx_count        = 0;

static void mock_esp_wifi_set_channel(uint8_t ch) { g_radio_channel = ch; }
static bool mock_esp_wifi_80211_tx(void) {
    g_last_tx_channel = g_radio_channel;   // Frame geht immer auf dem *aktuellen* Funkkanal raus
    g_tx_count++;
    return true;
}

// ---- ab hier Zeile fuer Zeile wie src/wifi_attack.cpp (je ein ALT/NEU-Variantenpaar) ----
#define ATK_DEFAULT_MS   20000u
#define ATK_HOP_MS         180u
#define ATK_BURST          6

enum AtkMode { M_NONE, M_DEAUTH, M_BEACON };

static enum AtkMode s_mode      = M_NONE;
static uint8_t  s_fixedCh   = 0;
static uint8_t  s_hopCh     = 1;
static uint32_t s_untilMs   = 0;
static uint32_t s_lastHop   = 0;
static uint32_t s_txOk      = 0;
static uint32_t g_now       = 0;    // Ersatz fuer millis()
static int      g_fix_enabled = 0;  // 0 = ALT (Bug), 1 = NEU (Fix)

static void set_channel(uint8_t ch) {
    if(ch < 1) ch = 1;
    if(ch > 13) ch = 13;
    mock_esp_wifi_set_channel(ch);
}

static void send_deauth(void) {
    for(int i = 0; i < ATK_BURST; i++) if(mock_esp_wifi_80211_tx()) s_txOk++;
}
static void send_beacon_claim_channel(uint8_t claimed_ch) {
    // Entspricht send_beacon()'s "f[n++] = ch;" DS-Parameter-Tag: haelt fest, welchen Kanal
    // der Beacon-Frame-Inhalt selbst behauptet (unabhaengig vom tatsaechlichen Funkkanal).
    (void)claimed_ch;
    for(int i = 0; i < ATK_BURST; i++) if(mock_esp_wifi_80211_tx()) s_txOk++;
}

static void wifi_attack_deauth(uint8_t channel, uint32_t dur_ms) {
    s_fixedCh = channel;
    s_hopCh   = channel ? channel : 1;
    if(g_fix_enabled) {
        set_channel(s_hopCh);                 // NEU: immer sofort setzen
    } else {
        if(channel) set_channel(channel);     // ALT: nur im Festkanal-Fall
    }
    s_untilMs = g_now + (dur_ms ? dur_ms : ATK_DEFAULT_MS);
    s_lastHop = g_now;
    s_txOk = 0;
    s_mode = M_DEAUTH;
}

static void wifi_attack_beacon(uint32_t dur_ms) {
    s_fixedCh = 0;
    s_hopCh   = 1;
    if(g_fix_enabled) set_channel(s_hopCh);   // NEU
    // ALT: kein set_channel()-Aufruf hier
    s_untilMs = g_now + (dur_ms ? dur_ms : ATK_DEFAULT_MS);
    s_lastHop = g_now;
    s_txOk = 0;
    s_mode = M_BEACON;
}

static void wifi_attack_stop(void) {
    if(s_mode == M_NONE) return;
    s_mode = M_NONE;
    // (ESP-NOW-Kanal-Wiederherstellung hier fuer dieses Repro irrelevant, s. anderer Fund)
}

static void wifi_attack_tick(void) {
    if(s_mode == M_NONE) return;
    uint32_t now = g_now;
    if((int32_t)(now - s_untilMs) >= 0) { wifi_attack_stop(); return; }

    if((s_mode == M_BEACON || (s_mode == M_DEAUTH && s_fixedCh == 0)) &&
       (now - s_lastHop) >= ATK_HOP_MS) {
        s_hopCh = (s_hopCh % 13) + 1;
        set_channel(s_hopCh);
        s_lastHop = now;
    }

    if(s_mode == M_DEAUTH) {
        send_deauth();
    } else {
        uint8_t ch = s_fixedCh ? s_fixedCh : s_hopCh;
        send_beacon_claim_channel(ch);
    }
}

static void reset_state(uint8_t leftover_channel) {
    s_mode = M_NONE; s_fixedCh = 0; s_hopCh = 1;
    s_untilMs = 0; s_lastHop = 0; s_txOk = 0;
    g_last_tx_channel = 0xFF; g_tx_count = 0;
    g_now = 0;
    // Radio steht noch auf dem Kanal eines vorherigen Angriffs/Scans (z.B. Kanal 6 von
    // einem gerade zuvor laufenden Festkanal-Deauth) -- main.cpp ruft vor WifiDeauth/
    // BeaconSpam KEIN wifi_attack_stop() auf (siehe UkfeRfCmdWifiDeauth/BeaconSpam-Handler).
    g_radio_channel = leftover_channel;
}

static int checks = 0;
#define CHECK(cond, label) do { \
    checks++; \
    if(!(cond)) { fprintf(stderr, "FAIL [%s]\n", label); assert(cond); } \
    else fprintf(stdout, "ok  [%s]\n", label); \
} while(0)

int main(void) {
    // Szenario 1 (ALT): Beacon-Spam direkt nach einem Festkanal-Deauth auf Kanal 6 gestartet
    // (kein Stop zwischendurch -- main.cpp erzwingt das nicht).
    g_fix_enabled = 0;
    reset_state(6);
    wifi_attack_beacon(0);
    CHECK(g_radio_channel == 6, "ALT: Setup -- Radio steht noch auf Kanal 6 vom vorherigen Angriff");
    g_now += 1; wifi_attack_tick();   // allererster Beacon-Burst, noch vor dem ersten Hop-Tick
    CHECK(g_last_tx_channel == 6, "BUG ALT: erster Beacon-Burst geht auf Kanal 6 raus, nicht auf den behaupteten Kanal 1");
    CHECK(s_hopCh == 1, "ALT: DS-Parameter-Tag behauptet bereits Kanal 1 (claimed_ch==s_hopCh)");
    g_now += 1; wifi_attack_tick();
    CHECK(g_last_tx_channel == 6, "BUG ALT: zweiter Burst ebenfalls noch auf Kanal 6 (Fenster bis ATK_HOP_MS=180ms)");
    g_now += 200; wifi_attack_tick();   // jetzt greift der erste Hop
    CHECK(g_last_tx_channel == 2, "ALT: erst nach dem ersten Hop-Tick stimmen Funkkanal und Tag wieder zusammen");

    // Szenario 1b (ALT): derselbe Effekt bei hoppendem Deauth (channel=0) nach einem
    // vorherigen Festkanal-Deauth auf Kanal 11.
    g_fix_enabled = 0;
    reset_state(11);
    wifi_attack_deauth(0, 0);
    g_now += 1; wifi_attack_tick();
    CHECK(g_last_tx_channel == 11, "BUG ALT: hoppender Deauth sendet ersten Burst auf Kanal 11 statt Kanal 1");

    // Szenario 2 (NEU): derselbe Ablauf mit dem Fix -- Beacon-Spam.
    g_fix_enabled = 1;
    reset_state(6);
    wifi_attack_beacon(0);
    CHECK(g_radio_channel == 1, "FIX: set_channel(s_hopCh) beim Start raeumt den Leftover-Kanal sofort weg");
    g_now += 1; wifi_attack_tick();
    CHECK(g_last_tx_channel == 1, "FIX: erster Beacon-Burst geht korrekt auf Kanal 1 raus (passt zum Tag)");
    g_now += 200; wifi_attack_tick();
    CHECK(g_last_tx_channel == 2, "FIX: Hopping danach unveraendert (Kanal 2 nach erstem Hop)");

    // Szenario 2b (NEU): hoppender Deauth.
    g_fix_enabled = 1;
    reset_state(11);
    wifi_attack_deauth(0, 0);
    g_now += 1; wifi_attack_tick();
    CHECK(g_last_tx_channel == 1, "FIX: hoppender Deauth sendet ersten Burst korrekt auf Kanal 1");

    // Regression (NEU): Festkanal-Deauth bleibt exakt wie zuvor (kein Verhaltensunterschied).
    g_fix_enabled = 1;
    reset_state(11);
    wifi_attack_deauth(6, 0);
    CHECK(g_radio_channel == 6, "Regression FIX: Festkanal-Deauth setzt weiterhin sofort auf den Zielkanal 6");
    g_now += 1; wifi_attack_tick();
    CHECK(g_last_tx_channel == 6, "Regression FIX: erster Burst bleibt auf Zielkanal 6");

    printf("\nALL CHECKS PASSED (%d total)\n", checks);
    return 0;
}
