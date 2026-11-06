// Host-Repro: ESP-NOW-ACK-Absender-MAC wird global gehalten und erst NACH dem
// blockierenden act()-Aufruf gelesen -- ein zweiter, waehrend act() eintreffender
// ESP-NOW-Frame ueberschreibt sie vorher. Bildet exakt die Variablen/den
// Kontrollfluss aus heltec-ukfe-rx/src/main.cpp (Zeilen ~423-442) und
// lilygo-ukfe-rx/src/main.cpp (Zeilen ~260-283) nach -- Mailbox-Felder,
// Reihenfolge der Zuweisungen, Ort der send_espnow_ack()-Aufrufe. act() selbst
// ist durch einen simulierten Verarbeitungs-Block ersetzt, in dem genau EIN
// zweiter ESP-NOW-Frame "eintreffen" kann (nachbildet, was auf echter Hardware
// waehrend eines mehrsekuendigen WiFi-Scans/BLE-Spams/HID-Ducky-Laufs passieren
// wuerde, wenn in dieser Zeit ein zweiter Hub/Relay-Sender dasselbe Geraet
// anspricht).
//
// gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0 -o repro repro.c && ./repro
// gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O1 -o repro repro.c && ./repro
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define UKFE_RF_MAX_FRAME 55

// ---- Mailbox-Felder, 1:1 wie in main.cpp (enowFlag/enowBuf/enowLen/enowSenderMac) ----
static int     enowFlag = 0;
static uint8_t enowBuf[UKFE_RF_MAX_FRAME];
static int     enowLen = 0;
static uint8_t enowSenderMac[6] = {0};

// Protokoll: der ACK-Zielempfaenger, wie send_espnow_ack() ihn tatsaechlich bekommen hat.
static uint8_t g_last_ack_mac[6];
static int     g_ack_sent = 0;

// Nachbildung von onEspNowRecv(): genau wie im echten Code -- droppt, solange
// die Mailbox noch belegt ist (enowFlag==true), uebernimmt sonst Frame+Sender-MAC.
static void on_espnow_recv(const uint8_t mac[6], const uint8_t* data, int len) {
    if(enowFlag) return;
    if(len <= 0 || len > UKFE_RF_MAX_FRAME) return;
    memcpy(enowSenderMac, mac, 6);
    memcpy(enowBuf, data, (size_t)len);
    enowLen = len;
    enowFlag = 1;
}

// Nachbildung von send_espnow_ack(): wir pruefen nur, welche MAC uebergeben wurde.
static void send_espnow_ack(const uint8_t mac[6], uint8_t orig_cmd, uint8_t result) {
    (void)orig_cmd; (void)result;
    memcpy(g_last_ack_mac, mac, 6);
    g_ack_sent++;
}

// --- ALT: 1:1 der Ist-Zustand aus main.cpp (ACK-MAC bleibt global, Lesen nach act()) ---
// during_act(): Hook, der waehrend des blockierenden act()-Aufrufs ausgefuehrt wird --
// simuliert, dass auf echter Hardware in dieser Zeit ein ESP-NOW-Interrupt feuert.
static void loop_iteration_ALT(void (*during_act)(void)) {
    if(!enowFlag) return;
    int len = enowLen;
    uint8_t frame[UKFE_RF_MAX_FRAME];
    memcpy(frame, enowBuf, (size_t)len);
    enowFlag = 0;                    // Puffer wieder freigeben -- WIE IM ECHTEN CODE
    // act(&msg) -- hier als Platzhalter fuer den blockierenden Befehl (WiFi-Scan/
    // BLE-Spam/HID-Ducky, alles mit mehreren hundert ms bis mehreren Sekunden Laufzeit)
    if(during_act) during_act();
    // send_espnow_ack(enowSenderMac, ...) -- liest die GLOBALE Variable JETZT, nachdem
    // act() (und der simulierte Interrupt darin) bereits gelaufen ist.
    send_espnow_ack(enowSenderMac, frame[0], 0);
}

// --- NEU: Fix -- ACK-MAC wird als lokale Kopie zusammen mit dem Frame gesichert ---
static void loop_iteration_FIX(void (*during_act)(void)) {
    if(!enowFlag) return;
    int len = enowLen;
    uint8_t frame[UKFE_RF_MAX_FRAME];
    memcpy(frame, enowBuf, (size_t)len);
    uint8_t ackMac[6];
    memcpy(ackMac, enowSenderMac, 6);   // <-- der Fix: Snapshot VOR dem Freigeben/act()
    enowFlag = 0;
    if(during_act) during_act();
    send_espnow_ack(ackMac, frame[0], 0);
}

static const uint8_t MAC_HUB_A[6]     = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05};
static const uint8_t MAC_HUB_B[6]     = {0xBB, 0x01, 0x02, 0x03, 0x04, 0x05};
static uint8_t frame_from_A[UKFE_RF_MAX_FRAME];
static uint8_t frame_from_B[UKFE_RF_MAX_FRAME];

// waehrend des act()-Aufrufs der ersten Nachricht trifft eine zweite, von einem
// ANDEREN Sender (z. B. einem zweiten Hub/Relay auf demselben Kanal) ein.
static void inject_second_frame_from_B(void) {
    on_espnow_recv(MAC_HUB_B, frame_from_B, UKFE_RF_MAX_FRAME);
}

static int mac_eq(const uint8_t* a, const uint8_t* b) { return memcmp(a, b, 6) == 0; }

static void reset_state(void) {
    enowFlag = 0; enowLen = 0;
    memset(enowBuf, 0, sizeof(enowBuf));
    memset(enowSenderMac, 0, sizeof(enowSenderMac));
    g_ack_sent = 0;
    memset(g_last_ack_mac, 0, sizeof(g_last_ack_mac));
}

int main(void) {
    memset(frame_from_A, 0xAA, sizeof(frame_from_A)); frame_from_A[0] = 0x01;
    memset(frame_from_B, 0xBB, sizeof(frame_from_B)); frame_from_B[0] = 0x02;

    // Szenario 1 (ALT, Ist-Zustand): Nachricht von A kommt an, act() laeuft, WAEHREND
    // act() trifft ein zweiter Frame von B ein -> der ACK fuer A's Befehl geht an B.
    {
        reset_state();
        on_espnow_recv(MAC_HUB_A, frame_from_A, UKFE_RF_MAX_FRAME);
        loop_iteration_ALT(inject_second_frame_from_B);
        assert(g_ack_sent == 1);
        int bug_reproduced = !mac_eq(g_last_ack_mac, MAC_HUB_A) && mac_eq(g_last_ack_mac, MAC_HUB_B);
        printf("Szenario 1 (ALT, A sendet, B kommt waehrend act() dazwischen): "
               "ACK ging an %s -- %s\n",
               mac_eq(g_last_ack_mac, MAC_HUB_A) ? "A (korrekt)" : "B (FALSCH)",
               bug_reproduced ? "Fund reproduziert" : "FEHLER: Fund NICHT reproduziert");
        assert(bug_reproduced);
    }

    // Szenario 2 (NEU, Fix): exakt derselbe Ablauf, aber mit lokalem ackMac-Snapshot --
    // der ACK fuer A's Befehl geht trotz des zwischenzeitlich eingetroffenen B-Frames
    // weiterhin korrekt an A.
    {
        reset_state();
        on_espnow_recv(MAC_HUB_A, frame_from_A, UKFE_RF_MAX_FRAME);
        loop_iteration_FIX(inject_second_frame_from_B);
        assert(g_ack_sent == 1);
        int fixed = mac_eq(g_last_ack_mac, MAC_HUB_A);
        printf("Szenario 2 (FIX, A sendet, B kommt waehrend act() dazwischen):    "
               "ACK ging an %s -- %s\n",
               fixed ? "A (korrekt)" : "B (FALSCH)",
               fixed ? "Fix bestaetigt" : "FEHLER: Fix wirkt nicht");
        assert(fixed);
        // der B-Frame darf dabei nicht verloren gehen -- er muss fuer die naechste
        // loop()-Iteration in der Mailbox stehen (wie im echten Code: enowFlag wird
        // beim Eintreffen waehrend act() wieder auf 1 gesetzt).
        assert(enowFlag == 1);
        assert(enowBuf[0] == frame_from_B[0]);
        assert(mac_eq(enowSenderMac, MAC_HUB_B));
        printf("  -> B's Frame bleibt fuer die naechste loop()-Iteration in der "
               "Mailbox erhalten (kein Datenverlust durch den Fix).\n");
    }

    // Szenario 3 (Regression, kein zweiter Sender): ohne Race verhaelt sich der Fix
    // identisch zum Ist-Zustand -- der alleinige Absender A bekommt seinen ACK.
    {
        reset_state();
        on_espnow_recv(MAC_HUB_A, frame_from_A, UKFE_RF_MAX_FRAME);
        loop_iteration_ALT(NULL);
        uint8_t alt_result[6]; memcpy(alt_result, g_last_ack_mac, 6);

        reset_state();
        on_espnow_recv(MAC_HUB_A, frame_from_A, UKFE_RF_MAX_FRAME);
        loop_iteration_FIX(NULL);
        uint8_t fix_result[6]; memcpy(fix_result, g_last_ack_mac, 6);

        int identical = mac_eq(alt_result, fix_result) && mac_eq(fix_result, MAC_HUB_A);
        printf("Szenario 3 (Regression, kein zweiter Sender): ALT und FIX liefern "
               "beide A -- %s\n", identical ? "keine Verhaltensaenderung" :
               "FEHLER: Verhaltensaenderung ohne Race");
        assert(identical);
    }

    printf("\nALLE CHECKS BESTANDEN (3 Szenarien, ALT reproduziert den Fund, FIX "
           "behebt ihn ohne Datenverlust und ohne Verhaltensaenderung im "
           "Nicht-Race-Fall)\n");
    return 0;
}
