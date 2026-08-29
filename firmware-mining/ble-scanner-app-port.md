# BLE Scanner — externe App aus RogueMaster-Bündel (AirTag-/Skimmer-Erkennung)

## Was
Aus dem seit der Rayhunter-Client-Sitzung offenen Commit `b8fe671` ("Add 6 new security tools:
uart_sniff, subghz_jammer, ble_scanner, evil_ble, rogue_ap_detector, rayhunter_client") im
RogueMaster-Bündel habe ich `uart_sniff`, `subghz_jammer`, `rogue_ap_detector` und
`rayhunter_client` bereits abgearbeitet. Als nächsten Kandidaten aus derselben Commit-Reihe
habe ich **BLE Scanner** geprüft (`applications/external/ble_scanner/`, `fap_author`
„PentestSuite / barkandbite", Quelle laut `application.fam`/`fap_weburl`
`barkandbite/flipper_suite` — dieselbe Quelle wie die vier bereits geprüften Apps).

Die App scannt über ein per UART (115200 Baud, Erweiterungsport) angebundenes
ESP32-Board mit [Marauder-Firmware](https://github.com/justcallmekoko/ESP32Marauder) nach
BLE-Advertisements und zeigt sie in einer scrollbaren Liste (RSSI, MAC, Name) an. Sie erkennt
Apple-AirTags/Find-My-Zubehör heuristisch (Namens-Teilstring „airtag" oder bekannte Apple-OUI-
Präfixe), erlaubt RSSI-Filterung und drei Sortiermodi (RSSI/Zeit/MAC) und kann Scans optional
als TSV-Log auf die SD-Karte schreiben. Codebasis ist klar in drei Dateien getrennt
(`ble_scanner.c` GUI/Menü/Refresh-Timer, `ble_scanner_worker.c` Marauder-Zeilen-Parser/
Gerätetabelle/AirTag-Heuristik, `ble_uart.c` UART-Abstraktion) — dieselbe Architektur wie bei
Rogue AP Detector und Rayhunter Client.

Beim Quelltext-Review auf Speichersicherheit keine Zugriffsfehler gefunden: alle
String-Kopien laufen über `strncpy`/`snprintf` mit expliziter Zielgrößenangabe, `find_mac()`
prüft Länge und Struktur vor jedem Zugriff, `name_hash_mac()` erzeugt exakt
`BLE_SCANNER_MAC_LEN` (18) Byte, die Geräteliste ist auf `BLE_SCANNER_MAX_DEVICES` (64) fest
begrenzt mit Prüfung vor jedem Einfügen. Stattdessen einen **Zustandsfehler** gefunden: das
Verbindungsflag zum ESP32-Board (`ble_uart_is_connected()`) ist ein reines Einweg-Flag, das
beim ersten empfangenen UART-Byte auf `true` gesetzt wird und danach **nie wieder
zurückgesetzt wird** — auch nicht, wenn seither keine einzige weitere Zeile mehr eingetroffen
ist.

```c
// ble_uart.c, ALT:
struct BleUart {
    ...
    volatile bool connected;
    volatile bool running;
};

static void ble_uart_isr_rx_cb(...) { /* ISR, unveraendert */ }

static int32_t ble_uart_rx_worker(void* context) {
    ...
    if(byte == '\n') {
        line_buf[line_pos] = '\0';
        if(line_pos > 0 && strncmp(line_buf, "[BUF/", 5) != 0) {
            if(!uart->connected) {
                uart->connected = true;   // <- wird NIE wieder auf false gesetzt
                FURI_LOG_I(TAG, "ESP32 board connected");
            }
            if(uart->rx_callback) uart->rx_callback(line_buf, uart->rx_callback_ctx);
        }
        line_pos = 0;
    }
    ...
}

bool ble_uart_is_connected(BleUart* uart) {
    furi_assert(uart);
    return uart->connected;   // <- reines Einweg-Flag, keine Staleness-Pruefung
}
```

`ble_scanner.c` fragt `ble_uart_is_connected()` alle 500 ms im Refresh-Timer ab, um zwischen
„[No ESP32]" (Board nie erreicht) und „[scanning]"/„[stopped]" (Board erreicht) zu
unterscheiden. Fällt das ESP32-Board während eines laufenden Scans aus — Wackelkontakt am
Erweiterungsport, das Board rutscht aus dem Halter, Stromausfall auf dem Board — zeigt die
App weiterhin „[scanning]" an, obwohl keine einzige Zeile mehr eintrifft und damit auch keine
neuen Geräte mehr erkannt werden können. Bei einem AirTag-/Skimmer-Sweep vor Ort (der
namensgebende Anwendungsfall dieser App) ist das ein stiller Ausfall, der als vollständig
abgeschlossener, unauffälliger Scan missverstanden werden kann, statt als abgebrochene Messung
erkannt zu werden.

## Nutzen
- Deckt eine mir bislang fehlende Kategorie ab: **BLE-Rekognoszierung mit AirTag-/Tracker-
  Erkennung**. Ergänzt die drei bereits geprüften Detektoren derselben Quelle (SubGHz-
  Dauerträger, WiFi Evil Twin, Mobilfunk-IMSI-Catcher) um die vierte Funkschicht (Bluetooth Low
  Energy) auf derselben ESP32-Hardwarebasis.
- Reine passive Scan-/Anzeige-App ohne eigenen Sendebetrieb (README bestätigt dies explizit:
  „does not transmit BLE advertisements or interfere with Bluetooth communications") — passt
  uneingeschränkt in den defensiven Rahmen aus CONSTRAINTS Punkt 5.
- Direkter Eigennutzen bei autorisierten Assessments: unwillkommene Tracker/Skimmer im
  Testumfeld aufspüren, allgemeines BLE-Lagebild vor/während eines Einsatzes.
- Nutzt exakt dasselbe UART-Erweiterungsport-Muster wie meine bereits verifizierten Rogue AP
  Detector/Rayhunter-Client-Ports — kein zusätzliches API-Risiko.
- Der gefundene Zustandsfehler ist mit einer kleinen, lokal begrenzten Änderung behebbar (siehe
  Port-Skizze), die keine der drei anderen bereits geprüften `barkandbite`-Apps betrifft (jede
  hat ihre eigene `*_uart.c`-Kopie).

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/ble_scanner/application.fam`
- `applications_user/ble_scanner/ble_scanner.c` / `.h`
- `applications_user/ble_scanner/ble_scanner_worker.c` / `.h`
- `applications_user/ble_scanner/ble_uart.c` / `.h` — **mit dem unten beschriebenen Fix**
- `applications_user/ble_scanner/README.md`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung).

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen meinen eigenen `mntm-012`-Header-Stand verifiziert
  (Momentum-Firmware-Tag lokal sparse ausgecheckt: `targets/f7/furi_hal/furi_hal_serial.h`,
  `furi_hal_serial_control.h`, `furi_hal_serial_types.h`,
  `applications/services/expansion/expansion.h`, `furi/core/stream_buffer.h`). Alle
  verwendeten Signaturen 1:1 vorhanden: `expansion_enable()`/`expansion_disable()` (inklusive
  der im Header selbst dokumentierten Aufrufreihenfolge — `expansion_disable()` vor
  `furi_hal_serial_control_acquire()`, `expansion_enable()` nach
  `furi_hal_serial_control_release()` — die `ble_uart.c` exakt einhält),
  `furi_hal_serial_control_acquire()`/`_release()` mit `FuriHalSerialIdUsart`,
  `furi_hal_serial_init/deinit`, `_async_rx_start(handle, callback, context, report_errors)`/
  `_async_rx_stop`, `_async_rx()`, `_tx()`, `_tx_wait_complete()`,
  `furi_stream_buffer_alloc/free/send/receive` — identisches API-Muster wie bei meinen bereits
  verifizierten Rogue-AP-Detector-/Rayhunter-Client-Ports. GUI-Seite nutzt ausschließlich
  Standardheader (`gui/gui.h`, `gui/modules/submenu.h`, `gui/modules/variable_item_list.h`,
  `gui/view.h`, `gui/view_dispatcher.h`, `notification/notification_messages.h`,
  `storage/storage.h`), kein `requires`-Eintrag im `application.fam` über das Standard-GUI
  hinaus.
- **Hardwarekette ist die eigentliche Einstiegshürde**: benötigt ein ESP32-Board mit
  BLE-fähigem Chip (ESP32-WROOM oder ESP32-S3) und laufender Marauder-Firmware — das
  WiFi-only ESP32-S2-Dev-Board (falls das meine aktuelle Hardware ist) hat laut eigenem README
  keinen BT-Radio und liefert dauerhaft „Needs ESP32 with BLE radio". Vor lokalem Test klären,
  welches ESP32-Board ich einsetze.
- **Der gefundene Zustandsfehler selbst ist kein Speicher-/Sicherheitsproblem**, sondern eine
  Zuverlässigkeitslücke: die App wird durch den Fix nicht funktional verändert (Erfolgsfall
  identisch), lediglich der Reconnect-/Ausfall-Fall wird jetzt sichtbar statt stillschweigend
  ignoriert.
- **AirTag-Heuristik bleibt Best-Effort** (README benennt dies selbst): Namens-Teilstring plus
  neun feste Apple-OUI-Präfixe, keine Service-UUID-Auswertung (0xFD6F „Find My" wird von
  Marauders Text-Output laut Quellcode-Kommentar nicht dekodiert) — beim lokalen Test mit einem
  eigenen AirTag gegenprüfen, ob die tatsächlich gesendete MAC/der Name von der Heuristik
  erfasst wird.

## Port-Skizze
1. App-Ordner `ble_scanner` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/ble_scanner/`) nach `applications_user/ble_scanner/` in meiner
   Firmware kopieren.
2. In der kopierten `ble_uart.c` den Zustandsfehler beheben — Staleness-Prüfung gegen den
   Zeitpunkt der letzten empfangenen Zeile statt reinem Einweg-Flag:

   ```c
   /* Nach Ausfall des Boards gilt die Verbindung als getrennt, wenn seit diesem
    * Zeitraum keine Zeile mehr eintraf. Deutlich groesser als das 500ms-Refresh-
    * Intervall der App, damit eine einzelne verzoegerte/verlorene Zeile nicht
    * sofort einen Fehlalarm ausloest. */
   #define BLE_UART_STALE_TIMEOUT_MS 3000

   struct BleUart {
       ...
       volatile bool connected;
       volatile uint32_t last_rx_tick;   /* neu */
       volatile bool running;
   };

   /* im Zeilen-Dispatch-Block der Worker-Schleife: */
   if(line_pos > 0 && strncmp(line_buf, "[BUF/", 5) != 0) {
       uart->last_rx_tick = furi_get_tick();          /* neu */
       if(!uart->connected) {
           uart->connected = true;
           FURI_LOG_I(TAG, "ESP32 board connected");
       }
       if(uart->rx_callback) {
           uart->rx_callback(line_buf, uart->rx_callback_ctx);
       }
   }

   bool ble_uart_is_connected(BleUart* uart) {
       furi_assert(uart);
       if(!uart->connected) return false;
       return (furi_get_tick() - uart->last_rx_tick) < furi_ms_to_ticks(BLE_UART_STALE_TIMEOUT_MS);
   }
   ```

   Die Änderung bleibt vollständig in `ble_uart.c` (kein Header-/API-Signaturbruch, `ble_uart.h`
   und alle Aufrufer unverändert): ein zusätzliches privates Feld plus zwei Zeilen im
   Zeilen-Dispatch plus die um die Staleness-Prüfung erweiterte `ble_uart_is_connected()`.
   `furi_get_tick()` ist bereits über `<furi.h>` verfügbar (in `ble_scanner_worker.c` an
   anderer Stelle bereits genutzt); Tick-Überlauf ist durch die vorzeichenlose Subtraktion
   unkritisch, dasselbe Muster wie `last_seen_tick` in `BleScanDevice`.
3. `ufbt launch_app APPSRC=applications_user/ble_scanner` bzw.
   `fbt launch_app APPSRC=applications_user/ble_scanner` — deckt etwaige Header-Abweichungen
   als Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O2 -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um das ALT/NEU-Verhalten des
   Zustandsfehlers zu bestätigen — bereits von mir lokal ausgeführt, siehe Testhinweis.
5. Hardwarekette aufbauen: ESP32-Board (WROOM/S3, Marauder-Firmware mit BLE-Support) per UART
   an den Flipper-Erweiterungsport.
6. Funktionstest:
   - Scan starten, mit bekannten eigenen BLE-Geräten (Handy, Kopfhörer, eigener AirTag falls
     vorhanden) Erkennung/RSSI/Sortierung/RSSI-Filter durchgehen.
   - **Regressionstest für den Fix**: während eines laufenden Scans das ESP32-Board gezielt vom
     Erweiterungsport trennen — vor dem Fix bleibt „[scanning]" dauerhaft stehen, nach dem Fix
     wechselt die Anzeige nach `BLE_UART_STALE_TIMEOUT_MS` auf „[No ESP32]"; Board wieder
     verbinden und prüfen, dass die Anzeige sofort wieder auf „[scanning]" zurückspringt.
   - SD-Logging aktivieren, Scan durchführen, TSV-Datei unter `/ext/ble_scanner/` gegen die
     angezeigten Werte prüfen.
7. Bei erfolgreichem Test: zusammen mit Rogue AP Detector, Rayhunter Client und SubGHz Jammer
   Detector als vierte defensive Erkennungs-App führen — alle vier teilen sich dieselbe
   ESP32-Hardwarebasis.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) des Kontrollflusses von `ble_uart_is_connected()`:
ALT als reines Einweg-Flag, NEU mit der oben beschriebenen Staleness-Prüfung gegen einen
simulierten Millisekunden-Zähler (steht für `furi_get_tick()`). Simuliert einen normalen Scan
(alle 500 ms trifft eine Zeile ein), einen ESP32-Ausfall mitten im Scan (keine weiteren Zeilen)
und eine anschließende Rückkehr des Boards. Gebaut/ausgeführt mit
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined`:

```
== t=5000ms (3500ms nach Ausfall, > 3000ms Timeout) ==
  ALT: is_connected=true  <- Bug: bleibt "verbunden", obwohl seit 3.5s keine
                           Zeile mehr eintraf
  NEU: is_connected=false  <- faellt korrekt auf "[No ESP32]" zurueck

ERGEBNIS: bestaetigt -- ALT meldet nach dauerhaftem ESP32-Ausfall weiterhin "verbunden", NEU
faellt nach 3000ms Stille korrekt auf "[No ESP32]" zurueck und erkennt eine Rueckkehr sofort
wieder.
```

Vollständige Fallmatrix (Erstverbindung, laufender Scan mit regelmäßigen Zeilen, Ausfall
knapp unter und deutlich über dem Timeout, Rückkehr) bestätigt: ALT bleibt in jedem Fall nach
der Erstverbindung auf „verbunden" hängen, NEU unterscheidet korrekt zwischen kurzer Stille
(toleriert) und anhaltendem Ausfall (erkannt), und erkennt eine Rückkehr sofort wieder.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/ble_scanner/` (Teil von Commit `b8fe671`, „Add 6 new security
  tools").
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `barkandbite/flipper_suite` (dieselbe
  Quelle wie SubGHz Jammer Detector, Rogue AP Detector, Rayhunter Client und UART Sniff).
- Externe Referenz für die Scan-Hardware: `justcallmekoko/ESP32Marauder` (BLE-Scan-Kommando
  `scanbt`/`stopscan`, von der Flipper-App per UART angesteuert).
- Aus derselben Commit-Reihe (`b8fe671`) weiterhin offen: `evil_ble` — für eine künftige
  Sitzung vorgemerkt.
