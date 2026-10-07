# SWD Probe — externe App von g3gg0 (ARM-SWD-Pinout-Scanner/Debug-Probe über GPIO)

## Was

Ich habe gezielt nach einer bislang komplett fehlenden Kategorie gesucht: Hardware-Debug-
Schnittstellen (JTAG/SWD) als Angriffs-/Auditfläche bei eingebetteten Zielgeräten — bisher deckt
mein GPIO/Serial-Bus-Cluster nur Bus-*Analyse* (UART Sniff, GPIO Logic Analyzer, i2c Tools, RS485
Modbus) und reinen Chip-*Dump* (SPI Flash Dump) ab, aber keinen Weg, eine unbekannte Platine
zuerst auf einen aktiven ARM-Debug-Port abzuklopfen. Fund: **SWD Probe**
(`g3gg0/flipper-swd_probe`, GPL-3.0, derselbe Autor wie der bereits katalogisierte GPIO Logic
Analyzer), vollständig gelesen (vier Quelldateien, `swd_probe_app.c`/`.h`, `adi.c`/`.h`,
`usb_uart.c`/`.h`, insgesamt ca. 4400 Zeilen).

Die App probiert ARM-SWD-Signale (Debug-Port für die meisten modernen Cortex-M-Mikrocontroller,
u. a. STM32) über acht wählbare Erweiterungsport-Pin-Paare per Bisektion durch und piept, sobald
ein gültiger SWD-`IDCODE` zurückkommt — zeigt dabei das erkannte JEP106-Herstellerkürzel
(eigene Datenbank in `jep106.c`) sowie den erkannten SWD-Pinout an. Nach erfolgreicher Erkennung
stellt sie eine vollwertige ARM-Debug-Interface-Implementierung bereit (`adi.c`, Debug-Port-/
Access-Port-Register-Zugriff, CoreSight-ROM-Table-Walk, Speicher lesen/schreiben über den
Memory-AP, Cortex-M-Core-Register über DCRSR/DCRDR, Haltepunkte setzen) — und eine eigene, kleine
Skriptsprache (`.swd`-Dateien von der SD-Karte, Befehle wie `halt`/`mem_dump`/`mem_write`/
`call`), um wiederkehrende Abläufe (z. B. Speicher nach Boot in eine Datei dumpen) ohne PC/
OpenOCD auszuführen. Zweiter virtueller COM-Port (parallel zur CLI, exakt dasselbe Muster wie
GPIO Logic Analyzer/UART Sniff) erlaubt zusätzlich eine Steuerung per Skript-Upload vom PC.

## Nutzen für mich

Deckt die bisher fehlende erste Phase eines Hardware-Assessments ab: bevor ich mit SPI Flash
Dump einen Flash-Chip anspreche oder mit GPIO Logic Analyzer/i2c Tools/UART Sniff/RS485 Modbus
einen bekannten Bus mitschneide, steht bei einer unbekannten Platine oft zuerst die Frage, ob
überhaupt ein offener Debug-Port (SWD) vorhanden ist, auf welchen zwei Pins er liegt und welcher
Chip dahintersteckt — ohne das muss man die Kontaktflächen blind durchprobieren oder ein
separates PC-Tool (OpenOCD + ST-Link/J-Link) mitbringen. Eigenständige Kategorie, keine
funktionale Überschneidung mit bestehenden Katalog-Einträgen; ergänzt die GPIO/Serial-Bus-
Sammlung um den Debug-Port-Kanal.

## API-87-Kompatibilität (mntm-012)

Alle acht referenzierten Erweiterungsport-Pins gegen den eigenen `mntm-012`-Tag-Stand geprüft
(`targets/f7/furi_hal/furi_hal_resources.c`):

```
gpio_ext_pc0 / pc1 / pc3 / pb2 / pb3 / pa4 / pa6 / pa7  -> alle acht unverändert vorhanden
```

Identische Pin-Menge wie GPIO Logic Analyzer (bereits verifiziert) — nur eine andere Nutzung
derselben acht Pins. `furi_hal_gpio_init()`/`_read()`/`_write()` (`furi_hal_gpio.h`) unverändert
vorhanden und signaturgleich.

Die zweite USB-CDC-Schnittstelle in `usb_uart.c` nutzt dieselbe Technik wie GPIO Logic Analyzer/
UART Sniff: `furi_hal_usb_set_config()` mit `usb_cdc_single`/`usb_cdc_dual`
(`Variable,+,usb_cdc_dual/_single,FuriHalUsbInterface` in `targets/f7/api_symbols.csv`),
`furi_hal_cdc_set_callbacks()`/`_send()`/`_receive()` (identische Signatur in Zeile 1419–1422
derselben Datei), `furi_hal_usb_unlock()` — alles unverändert vorhanden, API-Version exakt
`87.1` (Kopfzeile derselben CSV-Datei).

**Eine Besonderheit, die ich beim Gegenlesen gefunden und die bislang in keinem der Katalog-
Einträge aus demselben Autorenkreis (GPIO Logic Analyzer, UART Sniff) explizit vermerkt war:**
`usb_uart.c` öffnet die CLI-Sitzung über die *alte* API
(`Cli* cli = furi_record_open(RECORD_CLI); cli_session_open(cli, &cli_vcp); ...
cli_session_close(cli);`) — diese drei Symbole (`cli_session_open`, `cli_session_close`, der
globale `cli_vcp`) existieren in meinem `mntm-012`-Tag-Stand nicht mehr; die aktuelle
Momentum-CLI-Architektur arbeitet record-basiert über `CliVcp* cli_vcp =
furi_record_open(RECORD_CLI_VCP); cli_vcp_enable(cli_vcp); ... cli_vcp_disable(cli_vcp);`
(`applications/services/cli/cli_vcp.h`, nur `cli_vcp_enable`/`_disable` stehen in
`api_symbols.csv`). Ich habe per `grep` in einer frischen, unabhängigen Zweitkopie von
`g3gg0/flipper-logic_analyzer` bestätigt, dass dessen `usb_uart.c` exakt dasselbe alte Muster
verwendet (`cli_session_open(cli, &cli_vcp)`, Zeile für Zeile identisch) — SWD Probe ist also
kein Einzelfall, sondern folgt demselben, bei mir bereits als "CLI-Kompat-Shim (`cli_session_open/
close`, `cli_vcp`) für Alt-Apps" dokumentierten Muster (siehe `BACKLOG.md`, Abschnitt
"Kompatibilität"), über das bei mir firmwareseitig bereits 49 Alt-Apps mit genau dieser älteren
CLI-API laufen. Für den lokalen Bau heißt das: SWD Probe braucht **keinen** Patch für diesen
Teil, läuft aber nur über den bereits vorhandenen Kompat-Pfad, nicht über die aktuelle
`cli_vcp_enable`/`_disable`-API — beim eigenen Bau nicht versehentlich "reparieren"
(Signaturänderung würde den Shim-Pfad umgehen und echte Portierungsarbeit erzwingen, obwohl der
Shim das bereits abdeckt).

Übrige Abhängigkeiten (`gui/view_dispatcher.h`, `storage/storage.h`, `dialogs/dialogs.h`,
`notification/notification_messages.h`) sind Standard-Firmware-APIs, bereits bei mehreren
vorherigen Ports gegen denselben `mntm-012`-Stand verifiziert. `application.fam` zielt bereits
auf `f7` (kein `targets`-Eintrag nötig, Standardverhalten).

## Fund: ungeprüfte Dateibrowser-Pfadlänge → Heap-Buffer-Overflow (zwei Stellen)

`MAX_FILE_LENGTH` ist `128` (`swd_probe_app.h:48`), sowohl `ScriptContext.filename` als auch
`AppFSM.script_detected` sind `char[MAX_FILE_LENGTH]` — beide Felder liegen in
`malloc()`-allozierten Structs. An zwei Stellen landet ein vom Dateibrowser gelieferter Pfad
(`FuriString`, von `dialog_file_browser_show()` ohne jede Längenbegrenzung gefüllt — ein Flipper-
SD-Pfad kann durch verschachtelte Ordner deutlich länger als 127 Zeichen werden) ungeprüft per
`strcpy()` in eines dieser Felder:

```c
// swd_probe_app.c:2107, in swd_execute_script() — auch beim Laden per Dateibrowser erreicht
strcpy(ctx->script->filename, filename);

// swd_probe_app.c:2945, "Down"-Taste in ModePageScan/ModePageFound
const char* path = furi_string_get_cstr(result_path);
strcpy(ctx->script_detected, path);
```

Wähle ich über den eingebauten Dateibrowser ein `.swd`-Skript in einem tief genug verschachtelten
Ordner (Gesamtpfad ≥ 128 Zeichen inkl. NUL-Terminator), schreibt `strcpy()` über das Ende des
jeweiligen `malloc()`-Blocks hinaus — ein echter Heap-Buffer-Overflow, kein reiner
`furi_check()`-Abbruch. Die beiden anderen `strcpy()`/`strcat()`-Stellen im selben File
(`swd_scriptfunc_call()`, `swd_scriptfunc_mem_dump()`) sind bereits sauber mit vorherigen
Längenprüfungen bzw. `strncpy()` abgesichert — nur die beiden oben genannten, direkt aus dem
Dateibrowser gespeisten Stellen nicht.

## Host-Repro

`firmware-mining/repro-swd-probe-script-path-oob-write/repro.c` bildet beide `strcpy()`-Stellen
1:1 nach (gleiche Feldgröße `MAX_FILE_LENGTH = 128`, gleiche `malloc()`-Platzierung). Unter
`gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
verifiziert:

- **ALT**: Regressionsfall (kurzer Pfad) und Grenzfall (127 Zeichen, passt exakt) laufen sauber
  durch; ein 496 Byte langer, aus verschachtelten Ordnernamen zusammengesetzter Pfad löst in
  `execute_script_old()` einen von AddressSanitizer gemeldeten `heap-buffer-overflow`
  (`WRITE of size 497` auf eine 128-Byte-Region) aus — Fund reproduziert, Abbruch mit Exit-Code 1.
- **NEU** (`-DFIXED`): identischer Regressionsfall und Grenzfall weiterhin erfolgreich
  (byte-gleiches Ergebnis), derselbe 496-Byte-Pfad wird für beide Stellen sauber mit
  `"Path too long"` abgelehnt statt zu überlaufen — keine ASan-/UBSan-Meldung, keine
  Compiler-Warnung, Exit-Code 0, identisch unter `-O0` und `-O1`.

## Betroffene Datei

- `swd_probe_app.c` — `swd_execute_script()` (neue Längenprüfung vor dem bestehenden `strcpy()`)
  und der "Down"-Taste-Handler in `swd_message_process()` (bestehender `strcpy()` jetzt hinter
  einer Längenprüfung, sonst Fehlermeldung über `state_string` statt stillem Überlauf). Kein
  anderer Teil der App berührt.

## Port-Skizze

Patch (`git apply --check` vorab gegen eine frische, unabhängige Zweitkopie von
`g3gg0/flipper-swd_probe` verifiziert, real angewendet und byte-für-byte mit der Arbeitskopie
abgeglichen) in `firmware-mining/swd-probe-app-port/swd_probe_app_c.patch`:

```diff
 static bool swd_execute_script(AppFSM* const ctx, const char* filename) {
     bool success = true;

+    if(strlen(filename) >= MAX_FILE_LENGTH) {
+        DBG("Path too long '%s'", filename);
+        snprintf(ctx->state_string, sizeof(ctx->state_string), "Path too long");
+        return false;
+    }
+
     /* fetch current script and set as parent */
     ScriptContext* parent = ctx->script;
 [...]
                     if(dialog_file_browser_show(ctx->dialogs, result_path, preselected, &options)) {
                         const char* path = furi_string_get_cstr(result_path);
-                        strcpy(ctx->script_detected, path);
+                        if(strlen(path) < sizeof(ctx->script_detected)) {
+                            strcpy(ctx->script_detected, path);
+                        } else {
+                            DBG("Path too long '%s'", path);
+                            snprintf(
+                                ctx->state_string, sizeof(ctx->state_string), "Path too long");
+                        }
                     }
```

Beide Prüfungen spiegeln das bereits an anderer Stelle in derselben Datei verwendete Muster
(`swd_scriptfunc_call()`: Längencheck vor `strcat()`, Fehlermeldung über `swd_script_log()`/
`state_string` statt stillem Abbruch) — keine neue Fehlerbehandlungs-Abstraktion, nur dieselbe
bereits vorhandene Idee an den beiden bislang ungeschützten Stellen nachgezogen. Kein Einfluss
auf den eigentlichen SWD-Scan/die ADI-Implementierung (`adi.c` unberührt) oder die bereits
sicheren Skript-Interpreter-Pfade.

Lokal einzubringen beim eigenen Bau von `swd_probe.fap` (liegt nicht im eigenen
`mntm-012`-Firmware-Baum, sondern wird wie GPIO Logic Analyzer/SPI Flash Dump als externe App aus
dem Originalrepo `g3gg0/flipper-swd_probe` übernommen bzw. eigenständig gegen den eigenen
`mntm-012`-SDK-Stand gebaut — kein RogueMaster-Bündel nötig).

## Testhinweis (lokal, mit und ohne Hardware)

Rein logisch verifizierbar über den Host-Repro (siehe oben) — die Längenprüfung ist unabhängig
von SWD/Hardware. Wer zusätzlich am Gerät prüfen will: ein `.swd`-Skript in einem absichtlich
tief verschachtelten SD-Kartenordner ablegen, bis der volle Pfad (`/ext/.../name.swd`) 128
Zeichen oder mehr erreicht, dann über den Dateibrowser auswählen ("Down"-Taste im Scan-/
Found-Modus sowie direktes Ausführen) — muss jetzt "Path too long" in der Statuszeile zeigen und
die App darf nicht abstürzen/hängen bleiben. Regressionsfall mit einem kurzen Pfad (z. B.
`/ext/swd/test.swd`) erneut prüfen: muss weiterhin normal laden/ausführen. Eigentlicher
SWD-Scan/Debug-Betrieb (Pin-Erkennung, Speicherlesen) bleibt von diesem Fix unverändert und
braucht wie gehabt einen eigenen Cortex-M-Debug-Port (z. B. ein eigenes STM32-Nucleo-Board) zum
End-to-End-Test.

## Risiko

**Gering.** Zwei zusätzliche, rein additive Längenprüfungen vor bereits vorhandenen `strcpy()`-
Aufrufen, kein geänderter Kontrollfluss im Erfolgsfall (identisches Verhalten für jeden Pfad, der
schon vorher funktioniert hätte), keine neuen Abhängigkeiten. Der SWD-Scan/die ADI-Register-
Zugriffe selbst (`adi.c`) sind von diesem Fix nicht berührt — dort beim eigenen Einsatz weiterhin
gilt: nur an eigenen oder ausdrücklich freigegebenen Zielplatinen anschließen, ein offener
Debug-Port erlaubt vollen Speicher-/Register-Zugriff auf den Zielchip (passt in den
autorisierten Rahmen aus CONSTRAINTS Punkt 5, solange ausschließlich eigene/freigegebene
Hardware angeschlossen wird).

## Sicherheitsrahmen

Aktive Schnittstelle (im Gegensatz zu den bisher katalogisierten, rein passiven GPIO/Serial-
Funden UART Sniff/GPIO Logic Analyzer/i2c Tools) — ein erkannter, offener SWD-Port erlaubt
vollen Lese-/Schreibzugriff auf den Speicher und die Register des Zielchips, je nach Ziel auch
Firmware-Extraktion und Debug-Halt. Nur an eigenen Geräten oder im Rahmen autorisierter
Hardware-Pentests einsetzen (CONSTRAINTS Punkt 5) — bei Zielsystemen Dritter vorher die
Berechtigung klären, da ein offener Debug-Port in Produktivgeräten häufig als Secure-Boot-
Umgehung oder zur Schlüsselextraktion missbraucht werden kann.

## Quelle

`g3gg0/flipper-swd_probe`, Branch `master`, Commit `1beb139` (vollständiger, unabhängiger Klon
dieser Sitzung). Keine RogueMaster-Zwischenstufe nötig — eigenständiges Repo des Autors, der
auch den bereits katalogisierten GPIO Logic Analyzer pflegt.
