# SPI Flash Dump — externe App aus RogueMaster-Bündel (SPI-NOR-Flash-Auslesen über GPIO)

## Was

Ich habe gezielt nach einer bislang komplett fehlenden Kategorie gesucht — Hardware-/Firmware-
Extraktion über die bestehende GPIO/Serial-Bus-Sammlung hinaus (die bisher nur Bus-*Analyse*
abdeckt: UART Sniff, GPIO Logic Analyzer, i2c Tools, RS485 Modbus) — und **SPI Flash Dump**
(`applications/external/spi_flash_dump/`, Autor `PentestSuite & barkandbite`, also derselbe
Autorenkreis wie UART Sniff/SubGHz Jammer Detector/Rogue AP Detector, v1.0) gefunden und
vollständig gelesen (1933 Zeilen über 4 C-Dateien).

Die App liest SPI-NOR-Flash-Chips (Winbond W25Q, Macronix MX25L, GigaDevice GD25Q, Adesto
AT25SF, ISSI IS25LP, SST/Microchip SST25VF, Spansion/Cypress S25FL — 29 Einträge in einer
eingebauten JEDEC-ID-Datenbank) über bitgebangtes SPI auf vier Erweiterungsport-Pins aus, ohne
Firmware-eigene SPI-Peripherie zu belegen. Reiner Lesezugriff — Schreib-/Löschkommandos sind
laut eigenem README und eigener Prüfung des Quelltexts nicht implementiert. Einsatzzweck:
Firmware-Extraktion aus Routern/IoT-Geräten/eingebetteten Systemen zur Analyse (z. B. mit
`binwalk`/`strings` auf dem PC), Bootloader-Extraktion, Secret-/Key-Suche in Firmware-Dumps —
alles im eigenen README bereits korrekt als "nur mit Berechtigung" gerahmt. Füllt eine echte
Lücke: bislang hat das Ökosystem kein Werkzeug, das Daten *von einem fremden Chip auf die
SD-Karte* holt (alle bisherigen GPIO/Serial-Funde sind Sniffer/Scanner, kein Dump-Pfad).

Architektonisch ordentlich: Lese-Operation läuft in einem eigenen `FuriThread` (nicht im
UI-Thread), Ergebnis-Polling über einen 250-ms-Timer statt Busy-Wait, GPIO wird beim
App-Beenden konsequent zurück auf `GpioModeAnalog` gesetzt (`spi_worker_gpio_deinit()`), CRC32
der Dump-Datei wird im Worker-Thread selbst berechnet statt im Timer-Daemon (eigener Kommentar
begründet das explizit mit Vermeidung einer Blockade des Timer-Daemons) und die optionale
Verify-Operation liest unabhängig vom Dump nochmal direkt vom Chip, statt der Datei blind zu
vertrauen.

## API-87-Kompatibilität (mntm-012)

Alle vier verwendeten Erweiterungsport-Pins habe ich gegen den eigenen `mntm-012`-Tag-Stand
geprüft (`targets/f7/furi_hal/furi_hal_resources.h`):

```
gpio_ext_pb3  (CLK)   -> extern const GpioPin   (Zeile 90)
gpio_ext_pa4  (CS)    -> extern const GpioPin   (Zeile 91)
gpio_ext_pa6  (MISO)  -> extern const GpioPin   (Zeile 92)
gpio_ext_pa7  (MOSI)  -> extern const GpioPin   (Zeile 93)
```

Identische Symbole, kein Firmware-Patch nötig — dieselben vier Pins, über die bereits i2c Tools
(C0/C1, andere Pins) und GPIO Logic Analyzer (acht Kanäle) laufen, nur eine andere Teilmenge des
Erweiterungsport-Pintabellen-Blocks. `crc32_calc_file()` (`lib/toolbox/crc32_calc.h`) existiert
mit exakt der von `spi_worker.c` verwendeten Signatur
(`uint32_t crc32_calc_file(File*, FileCrcProgressCb, void*)`, Aufruf mit
`NULL, NULL` für Progress-Callback/Kontext). `application.fam` zielt bereits auf `f7`. Restliche
Abhängigkeiten (`gui/view_dispatcher.h`, `gui/modules/widget.h`, `gui/modules/
variable_item_list.h`, `storage/storage.h`, `notification/notification_messages.h`,
`datetime/datetime.h`) sind Standard-Firmware-APIs, bereits bei mehreren vorherigen Ports
gegen denselben `mntm-012`-Stand verifiziert.

## Fund: Hex-Viewer verdrahtet nur die Hälfte der im README versprochenen Navigation

Das eigene README beschreibt die Hex-Viewer-Steuerung so:

```
2. Navigate with Up/Down (one line) or Left/Right (one page)
3. Hold OK to jump to a specific offset (hex input)
```

`hex_viewer.c::hex_viewer_input_cb()` prüft aber ausschließlich `InputKeyUp` und
`InputKeyDown` — für jeden anderen Tastenwert (`InputKeyLeft`, `InputKeyRight`, `InputKeyOk`,
`InputKeyBack`) liefert die Funktion unbedingt `false`:

```c
static bool hex_viewer_input_cb(InputEvent* event, void* ctx) {
    View* view = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyUp) { /* ... scroll_offset--, geklemmt auf 0 ... */ return true; }
    if(event->key == InputKeyDown) { /* ... scroll_offset++, geklemmt auf max_offset ... */ return true; }
    return false;
}
```

Die im README dokumentierte Seitennavigation (Links/Rechts, eine ganze Bildschirmseite auf
einmal) existiert im Quelltext überhaupt nicht — bei einem vollständigen 4096-Byte-Preview
(1024 Zeilen à 4 Byte, 6 Zeilen pro Bildschirm sichtbar) bräuchte es ohne diese Funktion 1018
einzelne Tastendrücke auf Runter, um vom Anfang zum Ende zu scrollen. Der "Hold OK to jump to a
specific offset"-Eingabedialog ist ebenfalls nirgends verdrahtet — es gibt keine
`InputKeyOk`-Behandlung, keinen Hex-Eingabedialog, keine Verbindung zu `scroll_offset` dafür.
Das ist kein reines Dokumentationsversehen: der Scrollbalken in `hex_viewer_draw_cb()`
(`total_rows > ROWS_ON_SCREEN`-Fall) ist bereits für genau diesen Anwendungsfall ausgelegt
(zeigt die Position innerhalb eines langen Dumps), aber ohne Seitensprung bleibt er für den
eigentlichen Nutzungsfall (einen 16-MB-Dump nach interessanten Strings/Headern durchsuchen)
praktisch unbenutzbar langsam.

## Host-Repro

`firmware-mining/repro-spi-flash-dump-hex-viewer-page-scroll/repro.c` transkribiert die
`scroll_offset`-Arithmetik aus `hex_viewer_input_cb()` 1:1 für ALT (nur Up/Down) und NEU (dieser
Patch, zusätzlich Left/Right als Ein-Seiten-Sprung über `ROWS_ON_SCREEN` Zeilen — dieselbe
Einheit, die der Scrollbalken in `hex_viewer_draw_cb()` bereits verwendet). Unter
`gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1` verifiziert: 3150 Prüfungen, **ALLE CHECKS BESTANDEN**, keine ASan-/UBSan-Meldungen,
keine Compiler-Warnungen.

Sieben Szenarien:
1. Regression — Up/Down verhalten sich zwischen ALT und NEU über 111 verschiedene
   `data_len`-Werte und neun aufeinanderfolgende Tastendrücke identisch.
2. Fund — ALT reagiert auf Left/Right bei einem vollen 4096-Byte-Dump (1024 Zeilen) gar nicht:
   `scroll_offset` bleibt unverändert, `input_cb` liefert `false`.
3. Fix — NEU's Right springt exakt eine Seite (6 Zeilen) vor, klemmt am Ende auf `max_offset`
   statt zu überschießen.
4. Fix — NEU's Left springt exakt eine Seite zurück, klemmt am Anfang auf 0 statt zu
   unterlaufen.
5. Randfall — passt der Dump auf einen Bildschirm (`total_rows <= ROWS_ON_SCREEN`), bleiben
   Left/Right wirkungslose No-Ops (wie Up/Down es für diesen Fall schon immer waren), liefern
   aber weiterhin `true` (Taste wurde behandelt).
6. Randfall — wiederholtes Rechts über den vollen 4096-Byte-Preview (1000 Tastendrücke in
   Folge) bleibt durchgehend `<= max_offset` und landet exakt darauf, kein Überlauf.
7. Äquivalenz — unbehandelte Tasten (Ok/Back) liefern in ALT und NEU identisch `false`, über 20
   verschiedene `data_len`-Werte — der Fix fügt ausschließlich Left/Right hinzu, sonst nichts.

Beim Schreiben des Repros selbst einen eigenen Fehler gemacht und per ASan/UBSan-Lauf sofort
gefunden: die erste `CHECK()`-Fassung hat `assert(cond)` gefolgt von einem redundanten
`if(!(cond))` verwendet — bei einer seiteneffektbehafteten Bedingung (einem `input_cb()`-Aufruf
direkt in `CHECK(...)`) wertet das die Bedingung zweimal aus und löst die Taste doppelt aus.
Behoben, indem `CHECK()` die Bedingung jetzt in eine lokale Variable einmal auswertet, bevor sie
geprüft wird — betrifft nur das Repro selbst, nicht den eigentlichen Fix.

## Betroffene Datei

- `applications/external/spi_flash_dump/hex_viewer.c` — `hex_viewer_input_cb()`, zwei neue
  `if`-Zweige (`InputKeyLeft`/`InputKeyRight`). Kein anderer Teil der App berührt.

## Port-Skizze

Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet und
byte-für-byte mit der Arbeitskopie abgeglichen) in
`firmware-mining/spi-flash-dump-app-port/hex_viewer_c.patch`:

```diff
     if(event->key == InputKeyDown) {
         with_view_model(
             view,
             HexViewerModel * m,
             {
                 uint32_t total_rows = (m->data_len + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
                 if(total_rows > ROWS_ON_SCREEN && m->scroll_offset < total_rows - ROWS_ON_SCREEN) {
                     m->scroll_offset++;
                 }
             },
             true);
         return true;
     }

+    /* README advertises "Left/Right (one page)" navigation, but until this
+       fix neither key was wired up at all -- a page is ROWS_ON_SCREEN rows,
+       the same unit the scrollbar in the draw callback already uses. */
+    if(event->key == InputKeyLeft) {
+        with_view_model(
+            view,
+            HexViewerModel * m,
+            {
+                if(m->scroll_offset > ROWS_ON_SCREEN) {
+                    m->scroll_offset -= ROWS_ON_SCREEN;
+                } else {
+                    m->scroll_offset = 0;
+                }
+            },
+            true);
+        return true;
+    }
+
+    if(event->key == InputKeyRight) {
+        with_view_model(
+            view,
+            HexViewerModel * m,
+            {
+                uint32_t total_rows = (m->data_len + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
+                if(total_rows > ROWS_ON_SCREEN) {
+                    uint32_t max_offset = total_rows - ROWS_ON_SCREEN;
+                    if(m->scroll_offset + ROWS_ON_SCREEN < max_offset) {
+                        m->scroll_offset += ROWS_ON_SCREEN;
+                    } else {
+                        m->scroll_offset = max_offset;
+                    }
+                }
+            },
+            true);
+        return true;
+    }
+
     return false;
 }
```

Die Klemm-Logik spiegelt bewusst exakt das bereits vorhandene Up/Down-Muster (Addition/
Subtraktion, dann gegen 0 bzw. `max_offset` geklemmt statt zu über-/unterlaufen) — keine neue
Abstraktion, keine geänderte Struktur, kein Einfluss auf `hex_viewer_draw_cb()` oder
`hex_viewer_load_file()`. Der ebenfalls im README versprochene "Hold OK to jump to offset"-
Dialog bleibt bewusst außerhalb dieses Beitrags: das bräuchte eine eigene Hex-Eingabe-Ansicht
(neuer View, nicht nur zwei `if`-Zweige) und ist damit ein eigenständiger, größerer
Folgeschritt statt einer kleinen, abgeschlossenen Korrektur.

Lokal einzubringen beim eigenen Bau von `spi_flash_dump.fap` (liegt nicht im eigenen
`mntm-012`-Firmware-Baum, sondern wird wie NFC Canary/NFC Sniffer/GPIO Logic Analyzer als
externe App aus dem RogueMaster-Bündel übernommen bzw. eigenständig gegen den eigenen
`mntm-012`-SDK-Stand gebaut).

## Testhinweis (lokal, mit und ohne Hardware)

Rein logisch verifizierbar über den Host-Repro (siehe oben) — die Scroll-Arithmetik ist
unabhängig von SPI/Hardware. Wer zusätzlich am Gerät prüfen will: einen beliebigen SPI-NOR-
Flash-Chip wie im README beschrieben verkabeln (oder notfalls ohne Chip einen vorhandenen
`.bin`-Dump von mindestens 4 KB manuell nach `/ext/spi_dumps/` kopieren und direkt über "View
Dump" öffnen, falls die App das unterstützt — andernfalls einmal erfolgreich dumpen), dann im
Hex-Viewer Rechts drücken: muss jetzt sechs Zeilen vorspringen statt wirkungslos zu bleiben;
mehrfach Rechts bis zum Dateiende drücken: darf nicht über das Dateiende hinaus scrollen
(letzte sichtbare Zeile enthält noch Daten, keine reinen Leerzeilen); Links drücken: muss
symmetrisch sechs Zeilen zurückspringen und am Anfang auf Zeile 0 einrasten statt negativ zu
laufen; Regressionsfall Hoch/Runter (Ein-Zeilen-Schritt) auf beiden Bildschirmseiten erneut
prüfen.

## Risiko

**Sehr gering.** Reine Eingabe-/Scroll-Logik einer lokalen Anzeige-Ansicht, kein Einfluss auf
die SPI-Lese-/Verify-Operation, keine neuen Speicherzugriffe außerhalb des bereits
vorhandenen, fest 4096 Byte großen `HexViewerModel.data`-Puffers (Index bleibt über
`offset < m->data_len <= HEX_PREVIEW_SIZE` weiterhin identisch geklemmt wie vorher — die
Zeichenindizierung in `hex_viewer_draw_cb()` ist von diesem Patch nicht berührt). Einzige
beobachtbare Verhaltensänderung: Links/Rechts scrollen jetzt seitenweise, wie im eigenen README
versprochen, statt wirkungslos zu bleiben.

## Quelle

`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`, Commit `16cb41b9`, Pfad
`applications/external/spi_flash_dump/` (vollständiger, unabhängiger Klon dieser Sitzung).
Oberstrom-Repo laut `application.fam`/`fap_weburl`: `barkandbite/flipper_suite`.
