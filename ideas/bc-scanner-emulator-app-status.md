# BC Scanner (polarikus) — Barcode-Scanner-Emulator für Kassensystem-Tests

## Quelle

`github.com/polarikus/flipper-zero_bc_scanner_emulator`, aktueller Stand (Commit
`7f512ec`, 2026-10-11, `fap_version=(0, 5)`, GPL-3.0, 170 Sterne). Per `git clone` sowie
unabhängig per `curl` gegen `raw.githubusercontent.com` auf exakt diesen Commit geladen,
beide Methoden per `md5sum` byte-für-byte identisch bestätigt. Alle neun Quelldateien
(`bc_scanner_app.c/.h/_i.h`, `bc_scanner_script.c/.h`, vier `scenes/`, `views/bc_scanner_view.c`)
vollständig gelesen — kleines Projekt (rund 850 Zeilen). Gehört zu keinem der bereits
geprüften RogueMaster-/Unleashed-Bündel, eigenständiger Fund außerhalb bisheriger
Rückstellpunkte. Zusätzlich existiert eine separate Android-Begleit-App
(`flipper-zero_bc_scanner_emulator-Android-app`, nicht geprüft — reiner BLE/RPC-Client,
kein Flipper-Quelltext).

## Was

Lässt den Flipper über USB als COM-Port-Barcodescanner erscheinen (`furi_hal_usb_cdc`,
VID 1155/PID 22336, 19200 Baud) — für ein angeschlossenes Kassensystem/eine Kassen-Software
nicht von einem echten USB-Scanner zu unterscheiden. Zwei Betriebsarten: (1) Datei-Modus —
eine `.bc`-Textdatei mit einem beliebigen Barcode-Inhalt (auch GS1-Sonderzeichen) liegt auf
der SD-Karte, ein Tastendruck sendet sie zeichengetreu über den COM-Port; (2) RPC-Modus — eine
Android-Begleit-App verbindet sich per BLE, scannt einen Code mit der Handykamera und schickt
den Wert über das Flipper-RPC-Protokoll, die App reicht ihn unverändert an den COM-Port weiter
(kein `.bc`-Datei-Umweg nötig). Kein eigener USB-HID-Pfad (anders als beim eigenen BadUSB) —
rein COM-Port-/VCP-Emulation, exakt wie bei Kassensystem-Integrationen üblich, die einen
seriellen statt einen HID-Scanner erwarten.

## Nutzen

Neue Katalog-Kategorie: Scanner-*Emulation* für Eingabevalidierungstests an Kassen-/
Warenwirtschaftssystemen (POS) — Gegenstück auf der Eingabeseite zu den bestehenden
NFC-Tools-Funden (fremde NDEF-Tags vor dem Antippen prüfen) und zum eigenen BadUSB
(HID statt COM-Port). Erlaubt autorisierte Tests, ob eine Kasse überlange, mit Steuerzeichen
versehene oder ungültig strukturierte Barcode-Eingaben (z. B. manipulierte GS1-Felder,
Preis-/Mengen-Overrides in Payload-kodierten Formaten) korrekt zurückweist, statt sie
unvalidiert zu übernehmen — ohne einen echten Scanner anschaffen oder umprogrammieren zu
müssen. Der RPC-/BLE-Modus ergänzt das um Live-Tests mit beliebigen, vor Ort fotografierten
Codes, ohne jedes Mal eine neue `.bc`-Datei auf die SD-Karte kopieren zu müssen.

## Aufwand

Keiner (Firmware-seitig) — reiner `.fap`-Build gegen offizielle Flipper-APIs, kein
Firmware-Patch nötig (siehe API-87-Status). Für den Datei-Modus genügt eine Textdatei mit
`.bc`-Endung auf der SD-Karte. Für den RPC-/BLE-Modus zusätzlich die separate Android-App
(nicht geprüft) sowie ein Zielsystem mit COM-Port-Eingang (bei den meisten Kassensystemen
Standard, auch wenn der angeschlossene Scanner normalerweise HID ist — die meisten
Kassen-Software-Stacks unterstützen beide Transportarten parallel).

## API-87-Status

✅ Vollständig verifiziert gegen eine frische, unabhängige Zweitkopie von
`G4MEOVER18/G4MEOVER-FW` (`main`, `b359f6f`, `mntm-012`) — alle genutzten Symbole
byteidentisch vorhanden und exakt nach demselben Muster wie in der eigenen, bereits
gebauten `usb_uart_bridge`-App (`applications/main/gpio/usb_uart_bridge.c`) verwendet:
`furi_hal_usb_unlock()`/`_is_locked()`/`_set_config(&usb_cdc_single, NULL)`,
`furi_hal_serial_control_acquire(FuriHalSerialIdUsart)`/`_init()`/`_deinit()`/
`_control_release()`, `furi_hal_cdc_send()`, sowie `cli_vcp_disable()`/`_enable()` über
`RECORD_CLI_VCP` zum kontrollierten Verdrängen der CLI-VCP-Session während der Emulation.
Der RPC-Modus nutzt `rpc/rpc_app.h` (`RpcAppSystem`, `RpcAppSystemEvent`,
`RpcAppEventTypeSessionClose`/`_AppExit`/`_DataExchange`, `RpcAppSystemEventDataTypeBytes`,
`rpc_system_app_set_callback()`/`_send_started()`/`_send_exited()`/`_confirm()`) — jede
Signatur wörtlich identisch zum eigenen Firmware-Stand, dieselbe Nutzungsart wie im
offiziellen `rpc_debug_app`. `requires=["gui","dialogs","notification","rpc_start"]` aus
`application.fam` — alle vier App-IDs bei mir vorhanden (`rpc_start` u. a. von
`applications/services/expansion` selbst benötigt). Kein Rebuild-Hindernis, kein
API-Historien-Altlast-Fund wie bei anderen bereits geprüften USB-nahen Apps.

## Fund: Fortschrittsanzeige zeigt im Datei-Modus durchgehend 100 % statt echtem Sendefortschritt

In `bc_scanner_script.c` liest `bc_script_read_file()` (Zeile 99–108) jeweils höchstens
`FILE_BUFFER_LEN` (50) Byte aus der `.bc`-Datei und berechnet dabei `line_nb` komplett neu:

```c
static uint16_t bc_script_read_file(BarCodeScript* bc_script, File* script_file) {
    UNUSED(is_bc_end);
    bc_script->st.line_nb = 0;
    uint16_t ret = storage_file_read(script_file, bc_script->file_buf, FILE_BUFFER_LEN);
    if(storage_file_eof(script_file)) {
        bc_script->is_file_end = true;
    }
    bc_script->st.line_nb += ret;
    return ret;
}
```

`line_nb` wird dabei auf `0` zurückgesetzt und direkt wieder auf `ret` — die Größe genau
dieses einen 50-Byte-Häppchens — gesetzt, statt auf der beim Öffnen der Datei einmalig
ermittelten Gesamtgröße zu bleiben (`bc_scanner_worker()`, `bc_script->st.line_nb = size;`
beim Übergang in `BarCodeStateIdle`). Die Sendeschleife selbst setzt `line_cur` auf
denselben Wert:

```c
while(!bc_script->is_file_end) {
    bc_script->st.state = BarCodeStateRunning;
    uint16_t size = bc_script_read_file(bc_script, script_file);
    bc_script->st.line_cur = size;                          // == line_nb aus demselben Aufruf
    furi_hal_cdc_send(UART_PORT, bc_script->file_buf, size);
}
```

Der View-Layer (`views/bc_scanner_view.c:71`) berechnet die angezeigte Prozentzahl als
`(line_cur * 100) / line_nb`. Da beide Felder nach jedem Lesevorgang auf denselben
Häppchen-Wert gesetzt werden, ist dieser Quotient für jede Datei über 50 Byte bei **jedem**
einzelnen Lesevorgang exakt `100` — nie ein ansteigender Fortschritt wie `21 %, 42 %, 63 %, …`,
sondern eine durchgehend auf `100 %` stehende Anzeige, solange die Sendung noch läuft. Für
Dateien bis 50 Byte (ein einziger Lesevorgang deckt die ganze Datei ab) ist die Anzeige
zufällig korrekt — der Fehler betrifft ausschließlich Dateien, die über mehr als einen
50-Byte-Häppchen gesendet werden, beim gegebenen `UART_BAUD` von 19200 (≈ 1920 Byte/s) real
innerhalb von Millisekunden bis niedrigen Sekundenbereichen, je nach Dateigröße. Kein
Sicherheits- oder Zielrisiko (reiner Anzeigefehler auf dem eigenen Flipper-Display, der
tatsächlich über den COM-Port gesendete Byteinhalt ist unberührt), aber für ein Werkzeug,
dessen UI genau diese Fortschrittsanzeige als einzige Lebensanzeige während der Übertragung
zeigt, irreführend — der Nutzer kann anhand der Anzeige nicht erkennen, ob eine lange Sendung
noch läuft oder bereits fertig ist (beides zeigt `100 %`), ebenso wenig, wie weit sie
fortgeschritten ist, falls sie vorzeitig per "Stop" abgebrochen werden soll.

Gegengeprüft: der RPC-/BLE-Modus ist von diesem Fund **nicht** betroffen — dort gibt es
weder eine Fortschrittsanzeige noch einen `bc_script_read_file()`-Aufruf, jeder Barcode-Wert
wird in einem Stück über `bc_scanner_script_send_data()` gesendet. Ebenfalls gegengeprüft:
`scenes/bc_scanner_scene_file_select.c:28` ruft `scene_manager_next_scene()` mit dem
View-Enum-Wert `BarCodeAppViewWork` (aus `BarCodeAppView`, nicht aus dem eigentlich dafür
vorgesehenen `BarCodeScene`-Enum) auf — wirkt wie eine Typverwechslung zwischen den beiden
Enums, trifft aber nach der aktuellen Reihenfolge beider Enums zufällig denselben
Zahlenwert (`1`) wie die korrekte `BarCodeSceneWork`, navigiert also tatsächlich richtig.
Fragiler Code (bricht bei einer künftigen Umsortierung eines der beiden Enums lautlos),
aber beim aktuellen Stand kein eigenständiger Fund mit beobachtbarer Fehlwirkung.

## Fix-Skizze (additiv)

`line_nb` nur einmal beim Öffnen der Datei setzen und danach nicht mehr überschreiben;
`line_cur` statt mit der Größe des zuletzt gelesenen Häppchens mit der **kumulierten**
Anzahl bereits gesendeter Byte befüllen:

```c
// bc_script_read_file(): die line_nb = 0 / += ret - Zeilen entfernen, line_nb bleibt
// unverändert auf der beim Öffnen gesetzten Gesamtgröße.

// Sendeschleife in bc_scanner_worker():
uint32_t sent_total = 0;
while(!bc_script->is_file_end) {
    bc_script->st.state = BarCodeStateRunning;
    uint16_t size = bc_script_read_file(bc_script, script_file);
    sent_total += size;
    bc_script->st.line_cur = (uint16_t)sent_total;
    furi_hal_cdc_send(UART_PORT, bc_script->file_buf, size);
}
```

Zwei additive Einzeiler-Änderungen (eine entfernte Zeile in `bc_script_read_file()`, eine
neue Akkumulator-Variable plus angepasste Zuweisung in der Sendeschleife) — kein API- oder
Protokollwechsel, der tatsächlich über den COM-Port gesendete Byteinhalt bleibt unverändert.

## Testhinweis (lokal nachvollzogen)

Host-Repro (`ideas/repro-bc-scanner-progress-percent-overwrite/repro.c`) transkribiert
ausschließlich `bc_script_read_file()`, die Sendeschleife und die View-Formel
`(line_cur * 100) / line_nb` 1:1 (Feldnamen/-typen aus `BarCodeState` unverändert), mit
einer minimalen Mock-Storage (`uint32_t total_size`/`offset`), kein Flipper-/USB-/
Hardware-Zugriff nötig. Unter
`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`, sechs Prüfgruppen:

1. 237-Byte-Datei (fünf Häppchen `50/50/50/50/37`): ALT meldet bei **jedem** der fünf Lese-
   vorgänge `100 %`; NEU rampt `21/42/63/84/100 %`.
2. 150-Byte-Datei (exaktes Häppchen-Vielfaches, drei Lesevorgänge): ALT weiterhin durchgehend
   `100 %`; NEU rampt `33/66/100 %`.
3. 12-Byte-Datei (ein einziger, kurzer Lesevorgang): ALT und NEU stimmen überein (`100 %`) —
   bestätigt, dass der Fund ausschließlich Dateien über einem Häppchen betrifft.
4. Regressionsprüfung der View-Formel selbst anhand frei gewählter Werte (unverändert von
   der Fix-Skizze).
5. 65500-Byte-Datei (1310 Lesevorgänge, nahe der `uint16_t`-Obergrenze beider Felder): ALT
   bleibt durchgehend bei `100 %`, NEU endet exakt bei `100 %` ohne Überlauf in der
   Akkumulation.
6. Mutationsprobe: wird in NEU nur die Akkumulation wieder durch ein Überschreiben mit dem
   zuletzt gelesenen Häppchen ersetzt (ohne die `line_nb`-Reset-Zeilen erneut einzuführen),
   ergibt sich eine **andere**, ebenfalls falsche Anzeige (`21/21/21/21/15 %`, nie `100 %`,
   fällt beim letzten, kürzeren Häppchen sogar wieder ab) — bestätigt, dass die unter 1./2.
   geprüfte Rampe tatsächlich von der Akkumulations-Zeile abhängt und nicht zufällig
   durchläuft.

Ergebnis beider Durchläufe (`-O0`/`-O1`): **8 Prüfungen bestanden**, identischer Output
beider Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

Lokal zu tun: `.fap` gegen den eigenen `mntm-012`-Stand bauen (keine Firmware-Änderung
nötig, siehe API-87-Status), Fix in `bc_scanner_script.c` einbringen. Funktionstest: eine
`.bc`-Datei mit mehr als 50 Byte Inhalt (z. B. eine lange GS1-Zeile) über "Run" senden,
während der Übertragung die Prozentanzeige beobachten (vor dem Fix durchgehend `100 %`, nach
dem Fix ansteigend) und am COM-Port-Gegenstück (PC-Terminal mit passender Baudrate oder das
Zielkassensystem selbst) prüfen, dass der empfangene Byteinhalt in beiden Fällen identisch
und vollständig ist — der Fix ändert nur die Anzeige, nicht die Übertragung.

## Sicherheitsrahmen

Reine Eingabesimulation über einen emulierten COM-Port, kein Angriff auf ein Netzwerk oder
eine Mehrzahl von Zielen — ausschließlich für autorisierte Eingabevalidierungstests an einem
eigenen oder mit Erlaubnis zugänglichen Kassensystem. Keine Massen-/DoS-Funktion, kein
Umgehen von Erkennungsmechanismen. Passt unverändert in den defensiven Rahmen aus
`CONSTRAINTS.md` Punkt 5. Der gefundene Fehler ist ein reiner Anzeigefehler auf dem eigenen
Flipper-Display, kein Ziel-Sicherheitsrisiko.

## Nächster Schritt

Android-Begleit-App (`flipper-zero_bc_scanner_emulator-Android-app`) für eine künftige
Sitzung vormerken — eigener Quelltext (RPC-/BLE-Client), bislang nicht geprüft. Neue
Katalog-Kategorie "Barcode / POS-Testing" in `ideas/pentest-app-katalog.md` ergänzt.
