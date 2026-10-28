// Host-Repro (reine Zustandslogik, keine Arduino-/ESP32-Abhaengigkeit): bildet den
// Kanal-Wiederherstellungs-Fehler in lilygo-ukfe-rx/src/wifi_attack.cpp::wifi_attack_scan()
// nach. Die Mock-Funktionen ersetzen esp_wifi_set_channel()/esp_wifi_80211_tx()/
// WiFi.scanNetworks() 1:1 durch instrumentierte Varianten, die den aktuellen Funkkanal und
// jeden tatsaechlich gesendeten Deauth-Kanal mitprotokollieren -- die Entscheidungslogik
// selbst (set_channel/wifi_attack_deauth/wifi_attack_tick/wifi_attack_scan) ist Zeile fuer
// Zeile aus der echten Datei uebernommen.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// ---- Mock-Funk-Hardware ----------------------------------------------------
static uint8_t g_radio_channel = 0;
static int     g_set_channel_calls = 0;
static uint8_t g_last_tx_channel = 0xFF;   // Kanal, auf dem der letzte Deauth-Burst tatsaechlich rausging
static int     g_tx_ok_total = 0;

static void mock_esp_wifi_set_channel(uint8_t ch) {
    g_radio_channel = ch;
    g_set_channel_calls++;
}
// esp_wifi_80211_tx() sendet immer auf dem aktuell eingestellten Funkkanal -- das Frame
// selbst enthaelt keinen Kanal-Parameter, genau wie im echten ESP-IDF-Aufruf.
static bool mock_esp_wifi_80211_tx(void) {
    g_last_tx_channel = g_radio_channel;
    g_tx_ok_total++;
    return true;
}
// Simulierter Scan: wechselt waehrend des Scans selbst nichts, meldet nur n gefundene APs.
static int mock_scanNetworks(int fake_n) { return fake_n; }

// ---- ab hier Zeile fuer Zeile wie src/wifi_attack.cpp ----------------------
#define ATK_DEFAULT_MS   20000u
#define ATK_HOP_MS         180u
#define ATK_BURST          6

enum AtkMode { M_NONE, M_DEAUTH, M_BEACON };

static enum AtkMode s_mode      = M_NONE;
static uint8_t  s_espnowCh  = 1;
static uint8_t  s_bssid[6]  = {0};
static uint8_t  s_fixedCh   = 0;
static uint8_t  s_hopCh     = 1;
static uint32_t s_untilMs   = 0;
static uint32_t s_lastHop   = 0;
static uint8_t  s_scanCount = 0;
static uint32_t s_txOk      = 0;
static uint32_t g_now       = 0;    // Ersatz fuer millis()

static void set_channel(uint8_t ch) {
    if(ch < 1) ch = 1;
    if(ch > 13) ch = 13;
    mock_esp_wifi_set_channel(ch);
}

static void send_deauth(void) {
    for(int i = 0; i < ATK_BURST; i++) {
        if(mock_esp_wifi_80211_tx()) s_txOk++;
    }
}

static void wifi_attack_deauth(const uint8_t bssid[6], uint8_t channel, uint32_t dur_ms) {
    for(int i = 0; i < 6; i++) s_bssid[i] = bssid[i];
    s_fixedCh = channel;
    s_hopCh   = channel ? channel : 1;
    if(channel) set_channel(channel);
    s_untilMs = g_now + (dur_ms ? dur_ms : ATK_DEFAULT_MS);
    s_lastHop = g_now;
    s_txOk = 0;
    s_mode = M_DEAUTH;
}

static void wifi_attack_beacon(uint8_t mode, uint32_t dur_ms) {
    (void)mode;
    s_fixedCh = 0;
    s_hopCh   = 1;
    s_untilMs = g_now + (dur_ms ? dur_ms : ATK_DEFAULT_MS);
    s_lastHop = g_now;
    s_txOk = 0;
    s_mode = M_BEACON;
}

static void wifi_attack_stop(void) {
    if(s_mode == M_NONE) return;
    s_mode = M_NONE;
    set_channel(s_espnowCh);
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
        /* M_BEACON: fuer dieses Repro nicht relevant (keine send_beacon()-Instrumentierung) */
    }
}

// ---- ALT: unveraenderte wifi_attack_scan() ----
static uint8_t wifi_attack_scan_ALT(int fake_n) {
    int n = mock_scanNetworks(fake_n);
    s_scanCount = (n < 0) ? 0 : (n > 255 ? 255 : n);
    set_channel(s_espnowCh);
    return s_scanCount;
}

// ---- NEU: Fix aus wifi_attack_c_scan_channel.patch ----
static uint8_t wifi_attack_scan_NEU(int fake_n) {
    int n = mock_scanNetworks(fake_n);
    s_scanCount = (n < 0) ? 0 : (n > 255 ? 255 : n);
    set_channel(s_mode == M_NONE ? s_espnowCh : (s_fixedCh ? s_fixedCh : s_hopCh));
    return s_scanCount;
}

static void reset_state(void) {
    s_mode = M_NONE; s_espnowCh = 1; s_fixedCh = 0; s_hopCh = 1;
    s_untilMs = 0; s_lastHop = 0; s_scanCount = 0; s_txOk = 0;
    g_set_channel_calls = 0; g_last_tx_channel = 0xFF; g_tx_ok_total = 0;
    g_now = 0;
    // Entspricht setup(): esp_wifi_set_channel(ESPNOW_CHANNEL, ...) wird beim Boot
    // einmalig aufgerufen, bevor ueberhaupt ein Angriffsbefehl eintreffen kann.
    g_radio_channel = 0;
    mock_esp_wifi_set_channel(s_espnowCh);
    g_set_channel_calls = 0;   // Boot-Aufruf selbst nicht mitzaehlen
}

static int checks = 0;
#define CHECK(cond, label) do { \
    checks++; \
    if(!(cond)) { fprintf(stderr, "FAIL [%s]\n", label); assert(cond); } \
    else fprintf(stdout, "ok  [%s]\n", label); \
} while(0)

int main(void) {
    const uint8_t target[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};

    // Szenario 1 (ALT): Festkanal-Deauth auf Kanal 6, danach ein dazwischengefunkter Scan.
    reset_state();
    wifi_attack_deauth(target, 6, 0);
    g_now += 1;
    wifi_attack_tick();
    CHECK(g_last_tx_channel == 6, "ALT: erster Deauth-Burst auf Zielkanal 6");

    wifi_attack_scan_ALT(3);   // simulierter Scan-Befehl mitten im laufenden Angriff
    CHECK(g_radio_channel == s_espnowCh, "ALT: Scan setzt Radio auf ESP-NOW-Kanal zurueck");

    g_now += 1;
    wifi_attack_tick();   // naechster Deauth-Burst nach dem Scan
    CHECK(g_last_tx_channel == s_espnowCh, "BUG ALT: Deauth-Burst nach Scan faelschlich auf ESP-NOW-Kanal statt Kanal 6");
    CHECK(g_last_tx_channel != 6, "BUG ALT: Zielkanal 6 bleibt nach dem Scan dauerhaft verlassen");

    // Weitere Ticks (ohne erneuten Scan) zeigen: Festkanal-Modus (s_fixedCh!=0) hat kein
    // Hopping, das den Fehler je selbst korrigiert -- bleibt dauerhaft falsch.
    for(int i = 0; i < 5; i++) { g_now += 10; wifi_attack_tick(); }
    CHECK(g_last_tx_channel == s_espnowCh, "BUG ALT: bleibt auch nach weiteren Ticks auf dem falschen Kanal (kein Selbstheilung)");

    // Szenario 2 (NEU): derselbe Ablauf mit dem Fix.
    reset_state();
    wifi_attack_deauth(target, 6, 0);
    g_now += 1;
    wifi_attack_tick();
    CHECK(g_last_tx_channel == 6, "NEU: erster Deauth-Burst auf Zielkanal 6");

    wifi_attack_scan_NEU(3);
    CHECK(g_radio_channel == 6, "FIX: Scan stellt bei Festkanal-Deauth den Zielkanal 6 wieder her");

    g_now += 1;
    wifi_attack_tick();
    CHECK(g_last_tx_channel == 6, "FIX: Deauth-Burst nach dem Scan bleibt korrekt auf Kanal 6");

    // Regression A: Hopping-Deauth (channel=0) -- Fix darf den laufenden Hop-Stand nicht
    // verschlechtern, soll genau den aktuellen s_hopCh wiederherstellen.
    reset_state();
    wifi_attack_deauth(target, 0, 0);          // hoppend ab Kanal 1
    g_now += 1; wifi_attack_tick();            // Burst auf Kanal 1 (kein Hop noch ausgeloest)
    CHECK(g_last_tx_channel == 1, "Regression NEU: Hopping-Deauth startet auf Kanal 1");
    g_now += 200; wifi_attack_tick();          // >= ATK_HOP_MS -> ein Hop auf Kanal 2, danach Burst
    CHECK(s_hopCh == 2 && g_last_tx_channel == 2, "Regression NEU: Hopping-Deauth nach einem Hop auf Kanal 2");
    wifi_attack_scan_NEU(0);
    CHECK(g_radio_channel == 2, "FIX: Scan stellt bei Hopping-Deauth den aktuellen Hop-Kanal (2) wieder her, nicht Kanal 1 oder den ESP-NOW-Kanal");

    // Regression B: kein Angriff aktiv (M_NONE) -- Verhalten bleibt wie vor dem Fix.
    reset_state();
    wifi_attack_scan_NEU(0);
    CHECK(g_radio_channel == s_espnowCh, "Regression NEU: ohne laufenden Angriff weiterhin ESP-NOW-Kanal nach dem Scan");

    // Regression C: Beacon-Spam (immer hoppend, s_fixedCh==0) -- Fix restauriert s_hopCh.
    reset_state();
    wifi_attack_beacon(0, 0);
    g_now += 200; wifi_attack_tick();          // ein Hop ausgeloest (Beacon hoppt immer)
    uint8_t hop_before_scan = s_hopCh;
    wifi_attack_scan_NEU(2);
    CHECK(g_radio_channel == hop_before_scan, "Regression NEU: Beacon-Spam -- Scan stellt den aktuellen Hop-Kanal wieder her");

    printf("\nALL CHECKS PASSED (%d total)\n", checks);
    return 0;
}
