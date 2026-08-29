// Host-Mini-Repro: ble_uart_is_connected() (applications/external/ble_scanner/ble_uart.c,
// RogueMaster-Bundle, Quelle barkandbite/flipper_suite) meldet den ESP32-Verbindungsstatus
// ueber ein reines Einweg-Flag ("connected"), das beim allerersten empfangenen UART-Byte auf
// true gesetzt wird und danach nie wieder zurueckgesetzt wird -- auch nicht, wenn seitdem
// keine einzige weitere Zeile mehr eingetroffen ist (ALT-Verhalten, siehe unten). Wird das
// ESP32-Board waehrend eines laufenden Scans getrennt (Wackelkontakt am Erweiterungsport,
// Board faellt aus dem Halter, Stromausfall auf dem Board), zeigt die App weiterhin
// "[scanning]" statt "[No ESP32]" an, obwohl keine neuen Geraete mehr erkannt werden koennen --
// bei einem AirTag-/Skimmer-Sweep vor Ort ein stiller Ausfall, der als vollstaendig
// abgeschlossener Scan missverstanden werden kann.
//
// Dieses Repro bildet ausschliesslich den Kontrollfluss von ble_uart_is_connected() nach (kein
// echtes UART/Furi noetig): ALT als reines "connected"-Flag, NEU mit zusaetzlicher
// Staleness-Pruefung gegen den Zeitpunkt der letzten empfangenen Zeile (last_rx_tick), exakt
// wie im Fix-Vorschlag beschrieben. furi_get_tick() wird durch einen einfachen simulierten
// Millisekunden-Zaehler ersetzt (die reale Funktion liefert monoton steigende Millisekunden-
// Ticks; fuer den hier relevanten Kontrollfluss reicht ein simulierter Zaehler).
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BLE_UART_STALE_TIMEOUT_MS 3000u

// ---- ALT (mntm-012 / RogueMaster-Quelle): reines Einweg-Flag ----
typedef struct {
    bool connected;
} UartOld;

static void old_on_line_received(UartOld* u) {
    u->connected = true; /* wie im Original: "if (!uart->connected) uart->connected = true;" */
}

static bool old_is_connected(UartOld* u) {
    return u->connected;
}

// ---- NEU (Fix): Flag + Staleness-Pruefung gegen last_rx_tick ----
typedef struct {
    bool connected;
    uint32_t last_rx_tick;
} UartNew;

static void new_on_line_received(UartNew* u, uint32_t now) {
    u->last_rx_tick = now;
    u->connected = true;
}

static bool new_is_connected(UartNew* u, uint32_t now) {
    if(!u->connected) return false;
    return (now - u->last_rx_tick) < BLE_UART_STALE_TIMEOUT_MS;
}

int main(void) {
    int all_ok = 1;

    UartOld old = {0};
    UartNew neu = {0};

    // t=0: ESP32 sendet die erste Zeile ("ESP32 board connected").
    old_on_line_received(&old);
    new_on_line_received(&neu, 0);

    printf("== t=0ms, erste Zeile empfangen ==\n");
    printf("  ALT: is_connected=%s\n", old_is_connected(&old) ? "true" : "false");
    printf("  NEU: is_connected=%s\n\n", new_is_connected(&neu, 0) ? "true" : "false");
    all_ok &= (old_is_connected(&old) == true);
    all_ok &= (new_is_connected(&neu, 0) == true);

    // t=1500ms: Scan laeuft normal weiter, alle 500ms trifft eine neue Zeile ein (Refresh-
    // Timer-Intervall der App). Beide Implementierungen melden weiterhin verbunden.
    for(uint32_t t = 500; t <= 1500; t += 500) {
        old_on_line_received(&old);
        new_on_line_received(&neu, t);
    }
    printf("== t=1500ms, letzte Zeile bei laufendem Scan ==\n");
    printf("  ALT: is_connected=%s\n", old_is_connected(&old) ? "true" : "false");
    printf("  NEU: is_connected=%s\n\n", new_is_connected(&neu, 1500) ? "true" : "false");
    all_ok &= (old_is_connected(&old) == true);
    all_ok &= (new_is_connected(&neu, 1500) == true);

    // ESP32 faellt aus (Wackelkontakt/Stromausfall) -- ab jetzt trifft keine weitere Zeile
    // mehr ein. App-Refresh-Timer fragt weiterhin alle 500ms is_connected() ab.
    uint32_t disconnect_at = 1500;

    printf("== ESP32 faellt bei t=%ums aus, keine weiteren Zeilen ==\n\n", disconnect_at);

    // t=2000ms (500ms nach Ausfall): unter dem 3000ms-Timeout, beide zeigen noch verbunden --
    // erwartetes, kurzes Toleranzfenster gegen einzelne verlorene/verzoegerte Zeilen.
    printf("== t=2000ms (500ms nach Ausfall, < %ums Timeout) ==\n", BLE_UART_STALE_TIMEOUT_MS);
    printf("  ALT: is_connected=%s\n", old_is_connected(&old) ? "true" : "false");
    printf("  NEU: is_connected=%s\n\n", new_is_connected(&neu, 2000) ? "true" : "false");
    all_ok &= (old_is_connected(&old) == true);
    all_ok &= (new_is_connected(&neu, 2000) == true);

    // t=5000ms (3500ms nach Ausfall): ALT zeigt weiterhin "[scanning]"/verbunden -- der
    // eigentliche Bug. NEU erkennt die Stille und faellt auf "[No ESP32]" zurueck.
    printf("== t=5000ms (3500ms nach Ausfall, > %ums Timeout) ==\n", BLE_UART_STALE_TIMEOUT_MS);
    printf("  ALT: is_connected=%s  <- Bug: bleibt \"verbunden\", obwohl seit 3.5s keine\n"
           "                           Zeile mehr eintraf\n",
           old_is_connected(&old) ? "true" : "false");
    printf("  NEU: is_connected=%s  <- faellt korrekt auf \"[No ESP32]\" zurueck\n\n",
           new_is_connected(&neu, 5000) ? "true" : "false");
    all_ok &= (old_is_connected(&old) == true); /* bestaetigt den Bug: ALT bleibt haengen */
    all_ok &= (new_is_connected(&neu, 5000) == false);

    // ESP32 wird wieder eingesteckt, neue Zeile bei t=5200ms -- NEU erkennt die Rueckkehr
    // sofort wieder (last_rx_tick wird bei jeder Zeile aufgefrischt).
    old_on_line_received(&old);
    new_on_line_received(&neu, 5200);
    printf("== t=5200ms, ESP32 wieder verbunden, neue Zeile trifft ein ==\n");
    printf("  ALT: is_connected=%s\n", old_is_connected(&old) ? "true" : "false");
    printf("  NEU: is_connected=%s\n\n", new_is_connected(&neu, 5200) ? "true" : "false");
    all_ok &= (old_is_connected(&old) == true);
    all_ok &= (new_is_connected(&neu, 5200) == true);

    printf(
        all_ok ? "ERGEBNIS: bestaetigt -- ALT meldet nach dauerhaftem ESP32-Ausfall "
                  "weiterhin \"verbunden\", NEU faellt nach %ums Stille korrekt auf "
                  "\"[No ESP32]\" zurueck und erkennt eine Rueckkehr sofort wieder.\n"
                : "ERGEBNIS: Abweichung von der Erwartung, siehe oben.\n",
        BLE_UART_STALE_TIMEOUT_MS);
    return all_ok ? 0 : 1;
}
