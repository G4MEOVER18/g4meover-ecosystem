# UART Sniff — API-87-Status (aus dem b8fe671-Rückstellpunkt aufgegriffen)

Letzter noch offener Kandidat aus dem seit der Rayhunter-Client-Sitzung mitgeschleppten
Rückstellpunkt "aus Commit `b8fe671` weiterhin offen: `uart_sniff`, `ble_scanner`, `evil_ble`"
(bislang abgearbeitet aus derselben Commit-Reihe: SubGHz Jammer Detector, Rogue AP Detector,
H10301 Writer, Rayhunter Client). `ble_scanner`/`evil_ble` bleiben für eine künftige Sitzung
offen.

## Quelle
- Repo: `barkandbite/flipper_suite` (dieselbe Quelle wie alle bisher aus diesem Rückstellpunkt
  geprüften Funde), Pfad `uart_sniff/` — drei Dateien
  (`application.fam`, `uart_sniff.c`/`.h`, `uart_sniff_worker.c`/`.h`).
- `appid` `uart_sniff`, `fap_category` „GPIO", `fap_version` 1.0, `fap_author` „PentestSuite".

## Was
Passiver UART-Protokollsniffer über den Erweiterungsport (GPIO 13/14, USART oder LPUART
wählbar), sechs Baudraten (9600–230400), Live-Textbox-Ansicht mit 100-ms-Refresh in drei
Darstellungen (Hex/ASCII/beides, adressiert, 32 Zeilen à 8 Byte Fenster). Architektur: ISR
(`FuriHalSerialAsyncRxCallback`) schiebt jedes Byte non-blocking in einen 512-Byte
`FuriStreamBuffer`, ein eigener Worker-Thread drainiert diesen in einen mutex-geschützten
4-KB-Ringpuffer, aus dem die GUI die jeweils letzten 256 Byte für die Anzeige liest. Reiner
RX-Pfad, keine Sendefunktion.

## Nutzen für mich
Füllt eine bislang komplett fehlende Kategorie: Ich habe aktuell keine App für passive
UART-/Seriell-Bus-Analyse. Direkter Eigennutzen bei autorisierten Hardware-Assessments — viele
eingebettete Geräte (Debug-UARTs auf Platinen, serielle Bootloader, IoT-Sensor-Bussysteme)
lassen sich ohne Zusatzhardware über den vorhandenen Erweiterungsport abhören; ergänzt meine
bestehenden CAN-Bus-Funde (`canbus`/`can_bus_attack`) um die einfachere, weiter verbreitete
Bus-Art. Reine Ergänzung, keine Überschneidung mit vorhandenen Apps.

## API-87-Status
✅ Kompatibel, geprüft. Quelltext direkt per `curl` (nicht nur Zusammenfassung) gegen meinen
eigenen `mntm-012`-Header-Stand verifiziert (Momentum-Firmware-Tag lokal sparse ausgecheckt):
`furi_hal_serial_control_acquire()`/`_release()` mit `FuriHalSerialIdUsart`/`FuriHalSerialIdLpuart`
(`targets/f7/furi_hal/furi_hal_serial_control.h`, `furi_hal_serial_types.h`), sowie
`furi_hal_serial_init/deinit`, `_async_rx_start/_stop`, `_async_rx()` (nur aus dem ISR-Callback
aufgerufen, wie von der Header-Doku verlangt) — exakt dieselben Signaturen
(`targets/f7/furi_hal/furi_hal_serial.h`), keine Abweichung. `expansion_enable()`/
`expansion_disable()` (`applications/services/expansion/expansion.h`, RECORD_EXPANSION) werden
zusätzlich in exakt der von der Header-Doku vorgeschriebenen Reihenfolge aufgerufen
(`expansion_disable()` VOR `furi_hal_serial_control_acquire()` beim Start,
`expansion_enable()` NACH `furi_hal_serial_control_release()` beim Stop) — bei den beiden
zuvor geprüften Geschwister-Apps war diese Reihenfolge bereits korrekt, hier ebenso. GUI-Seite
nutzt ausschließlich `ViewDispatcher`/`Submenu`/`TextBox`/`VariableItemList`, keine
Sonderheader. Kein `requires`-Eintrag im `application.fam` über das Standard-GUI hinaus.

## Quelltext-Review (Speichersicherheit)
Sauber geschrieben: Ringpuffergröße ist eine Zweierpotenz (Masken- statt Modulo-Rechnung
korrekt), alle `snprintf()`-Aufrufe in der Anzeigeformatierung prüfen den Rückgabewert gegen
den verbleibenden Puffer und brechen kontrolliert ab, Capture-/Anzeigepuffer sind groß genug
für die deklarierten Konstanten. Keine Speicherzugriffsfehler gefunden.

## Gefundener Zustandsfehler (kein Speicherfehler, aber echtes Verhaltensproblem)
`sniff_view_back_cb()` (Zurück-Taste in der Sniff-Ansicht) liefert nur die Ziel-View-ID an
den `ViewDispatcher` zurück — anders als `app_navigation_cb()` (Zurück-Taste in der
Top-Level-Menüansicht, die App-Beenden auslöst) setzt sie `app->sniffing` nicht zurück und
ruft `uart_sniff_worker_free()` nicht auf. Der `ViewDispatcher` ruft den App-weiten
`app_navigation_cb()` laut eigenem Kommentar nur aus, wenn die aktuelle View selbst keine
weitere Zurück-Navigation anbietet (Top-Level) — die Sniff-View bietet mit `UartSniffViewMenu`
aber ein Ziel an, der App-weite Callback wird also für diesen Übergang gar nicht erreicht.
Folge: Zurück von der Sniff-Ansicht ins Menü beendet die Aufnahme nicht — ISR, Worker-Thread
und Ringpuffer laufen im Hintergrund unsichtbar weiter, solange die App offen bleibt (nur
vollständiges App-Beenden räumt auf). `UartSniffMenuStart` prüft vor dem erneuten Allozieren
zwar korrekt `if(!app->sniffing)` (kein Leck durch doppeltes Starten), aber genau dieselbe
Prüfung verhindert auch, dass eine im Menü zwischenzeitlich in den Settings geänderte
Baudrate/Kanalwahl beim erneuten Aufruf von "Start Sniff" übernommen wird — die
Einstellungsanzeige zeigt den neuen Wert, die tatsächlich noch laufende Aufnahme bleibt auf
dem alten Wert stehen, ohne dass das in der UI sichtbar würde. Für einen eigenen Port: entweder
`sniff_view_back_cb()`/eine explizite "Stop"-Aktion im Menü stoppt den Worker analog zu
`app_navigation_cb()`, oder das Start-Menüelement wird zu einem echten Toggle mit
Label-Anzeige des laufenden Zustands (Baudrate/Kanal), das bei laufender Aufnahme zuerst stoppt
und mit den aktuellen Settings neu startet.

## Aufwand
Gering für die Software (drei Dateien, reine externe `.fap`, keine Firmware-Änderung, keine
Zusatzhardware — nutzt ausschließlich den vorhandenen Erweiterungsport). Der oben beschriebene
Zustandsfehler ist beim Portieren mit wenigen Zeilen behebbar (siehe Fix-Skizze oben), kein
größerer Umbau nötig.

## Sicherheitsrahmen
Rein passiv (RX-only, sendet selbst nichts). Passt in den autorisierten Rahmen aus CONSTRAINTS
Punkt 5 — Analyse eigener Geräte/Platinen oder im Rahmen autorisierter Hardware-Assessments.
Wie bei jedem passiven Sniffer gilt: nur an eigenen oder ausdrücklich für den Test freigegebenen
Bussen einsetzen.

## Offen
- Lokale Bauprobe (`ufbt launch_app APPSRC=uart_sniff`) gegen die eigene `mntm-012`-Basis.
- Funktionstest mit einem bekannten UART-Signal (z. B. eigener ESP32/Heltec-Debug-UART bei
  bekannter Baudrate) zur Verifikation von Framing/Darstellung.
- Aus demselben `b8fe671`-Rückstellpunkt weiterhin offen: `ble_scanner`, `evil_ble`.
