# Simultaneous UHF RFID Reader — Speicherleck an den vier `convert_to_hex_string()`-Aufrufstellen plus toter `strncpy()`-Nebenfund behoben

## Was
Ich habe den seit `simultaneous-rfid-reader-convert-to-hex-string-no-terminator-fix.md` offenen
Rückstellpunkt an diesem Projekt aufgegriffen: der dort gelieferte Patch behebt nur den fehlenden
Nullterminator in `view_read.c::convert_to_hex_string()` selbst; das an allen vier echten
`snprintf()`-Aufrufstellen fehlende `free()` auf den Rückgabewert sowie der tote `strncpy()`-Block
in `view_config.c:728–739` blieben dort ausdrücklich offen ("eigenständige Nacharbeit für eine
künftige Sitzung"). Beide hole ich hier nach.

### Vier Aufrufstellen: Rückgabewert wird übernommen und freigegeben
Bisher (alle vier identisch im Muster, nur Puffergröße und Quellpuffer unterschiedlich):
```c
char* tempBuffer = (char*)malloc(24);
snprintf(tempBuffer, 24, "%s", convert_to_hex_string(App->ApTempBuffer, 4));
```
Der Rückgabewert von `convert_to_hex_string()` wird nur als Argument an `snprintf()` übergeben,
nie in eine eigene Variable übernommen — ab dem Ende des Ausdrucks ist die Allokation
unerreichbar. Betroffen: `view_config.c:397` (Set-AP-Puffer, 24 Byte), `view_kill.c:107`
(Kill-Passwort, 12 Byte), `view_kill.c:215` (Kill-Bestätigungs-Passwort, 9 Byte), `view_lock.c:59`
(Access-Passwort, 8 Byte) — jede der vier Stellen bei jedem Öffnen des jeweiligen
Passwort-Eingabe-Menüs bzw. jeder Eingabe-Aktualisierung.

Fix (gleiches Muster an allen vier Stellen):
```c
char* tempBuffer = (char*)malloc(24);
char* HexStr = convert_to_hex_string(App->ApTempBuffer, 4);
snprintf(tempBuffer, 24, "%s", HexStr);
free(HexStr);
```
Reiner Aufrufstellen-Fix: `convert_to_hex_string()` selbst (Inhalt von `view_read.c`) bleibt
unverändert — dieser Patch ist unabhängig von, aber gedacht zur Anwendung zusammen mit dem bereits
vorliegenden `view_read_c.patch` (behebt den fehlenden Terminator in der Funktion selbst; ohne ihn
bleibt das in Fund 1 der Vorsitzung dokumentierte Overflow-Read bestehen, dieser Patch behebt nur
das davon unabhängige Leck).

### Nebenfund `view_config.c:728–739` — totes `strncpy()` entfernt
Beim Öffnen des "Set AP"-Menüpunkts stand zusätzlich:
```c
bool Redraw = false;
with_view_model(
    App->ViewRead,
    UHFReaderConfigModel * Model,
    {
        strncpy(
            convert_to_hex_string(App->ApTempBuffer, 4),
            furi_string_get_cstr(Model->SettingReadAp),
            App->ApInputBufferSize);
    },
    Redraw);
```
Der `strncpy()`-Zielzeiger ist ein frischer, nirgends gespeicherter Rückgabewert von
`convert_to_hex_string()` — die Anweisung beschreibt eine Allokation, die direkt danach
unerreichbar wird (zusätzliches Leck), und verändert `App->ApTempBuffer` selbst nicht. Der
eigentliche Wert kommt unverändert über den separat registrierten
`byte_input_set_result_callback(App->ApInput, ..., App->ApTempBuffer, App->ApInputBufferSize)`
direkt in `App->ApTempBuffer` — das Widget schreibt die vom Nutzer eingegebenen Bytes selbst in den
Puffer, unabhängig von dieser `strncpy`-Zeile. Fix: den gesamten wirkungslosen Block (Kommentar,
`Redraw`-Deklaration, `with_view_model`-Aufruf) ersatzlos entfernt — `byte_input_set_header_text()`
davor und `byte_input_set_result_callback()`/`view_set_previous_callback()`/
`view_dispatcher_switch_to_view()` danach bleiben unverändert und unmittelbar aufeinanderfolgend.

## Nutzen
- Schließt den in der Vorsitzung bewusst offen gelassenen Rückstellpunkt ("Offen, bewusst nicht
  Teil dieses Patches: das … Speicherleck an den fünf Aufrufstellen … sowie der tote `strncpy()`-
  Nebenfund") vollständig ab.
- Reine Anzeige-/Eingabe-Logik (Passwort-Hex-Darstellung in den Settings-Views), kein
  Lesegerät-Protokoll, kein Firmware-Kern — host-seitig ohne angeschlossenes YRM100-Modul
  vollständig nachvollziehbar und behebbar.
- Additiv/entfernend an klar abgegrenzten, bereits dokumentierten Fundstellen — kein neues
  Verhalten außer dem Wegfall der beiden Lecks.

## Betroffene Dateien
- `simultaneous_rfid_reader/views/view_config.c` — `uhf_reader_setting_6_text_updated()`
  (Leck-Fix) und `uhf_reader_setting_item_clicked()` (toter `strncpy()`-Block entfernt).
- `simultaneous_rfid_reader/views/view_kill.c` — `uhf_reader_kill_password_updated()` und
  `uhf_reader_kill_confirm_password_updated()` (je ein Leck-Fix).
- `simultaneous_rfid_reader/views/view_lock.c` — `uhf_reader_access_password_updated()`
  (Leck-Fix).
- `simultaneous_rfid_reader/views/view_read.c` — unverändert; `convert_to_hex_string()` selbst ist
  Gegenstand des separaten `view_read_c.patch` aus der Vorsitzung, nicht dieses Patches.

## Patch
Unified-Diff gegen die unveränderte Quelle in
[`simultaneous-rfid-reader-convert-to-hex-string-callsite-leak-fix/views.patch`](simultaneous-rfid-reader-convert-to-hex-string-callsite-leak-fix/views.patch)
(alle drei Dateien in einem Patch, da jede Änderung für sich betriebsunabhängig und klein ist).
Gegen zwei frische, unabhängige Zweitkopien verifiziert: `git apply --check`/real sowie
`patch -p1 --dry-run`/real beide erfolgreich, beide Ergebnisse je Datei per `md5sum`
byte-identisch zueinander und zur Referenzkopie, gegen die der Patch erstellt wurde.

## Testhinweis (ohne Hardware)
[`repro-simultaneous-rfid-reader-convert-to-hex-string-callsite-leak/repro.c`](repro-simultaneous-rfid-reader-convert-to-hex-string-callsite-leak/repro.c)
bildet alle vier echten Aufrufstellen mit ihren realen Zielpuffergrößen (24/12/9/8 Byte) sowie den
toten `strncpy()`-Block nach. Um das hier geprüfte Leck-Verhalten unabhängig vom bereits separat
dokumentierten Overflow-Read zu isolieren, verwenden die Nachbildungen die bereits vorgeschlagene
Terminator-Fix-Version von `convert_to_hex_string()` (nicht die unveränderte Originalfunktion) —
ohne sie würde der fehlende Terminator sofort einen Heap-Buffer-Overflow-Read auslösen und das
Leck-Verhalten selbst verdecken (siehe das vorige Repro für diesen separaten Fund).

```
$ gcc -std=c17 -O0 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_O0
$ gcc -std=c17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_O1
$ ASAN_OPTIONS=detect_leaks=1 ./repro_O0   # identisch bei ./repro_O1
[ABBRUCH/LECK] ALT view_config.c:397 (malloc(24)) beendete sich mit Exit-Code 1 (LeakSanitizer)
[OK, KEIN LECK] NEU view_config.c:397 (malloc(24)) lief sauber durch
[ABBRUCH/LECK] ALT view_kill.c:107 (malloc(12)) beendete sich mit Exit-Code 1 (LeakSanitizer)
[OK, KEIN LECK] NEU view_kill.c:107 (malloc(12)) lief sauber durch
[ABBRUCH/LECK] ALT view_kill.c:215 (malloc(9)) beendete sich mit Exit-Code 1 (LeakSanitizer)
[OK, KEIN LECK] NEU view_kill.c:215 (malloc(9)) lief sauber durch
[ABBRUCH/LECK] ALT view_lock.c:59 (malloc(8)) beendete sich mit Exit-Code 1 (LeakSanitizer)
[OK, KEIN LECK] NEU view_lock.c:59 (malloc(8)) lief sauber durch
    ApTempBuffer nach ALT-Block: DE AD BE EF (unveraendert erwartet)
[ABBRUCH/LECK] ALT Set-AP-Block (totes strncpy()) beendete sich mit Exit-Code 1 (LeakSanitizer)
    ApTempBuffer nach NEU (Block entfernt): DE AD BE EF (unveraendert erwartet)
[OK, KEIN LECK] NEU Set-AP-Block (entfernt) lief sauber durch
[OK] ApTempBuffer nach ALT und NEU byteidentisch -- Block war bereits im Original ohne Effekt auf den Puffer.
```
Zehn von zehn Prüfungen wie erwartet: alle vier ALT-Aufrufstellen melden unter LeakSanitizer ein
Leck, alle vier NEU-Aufrufstellen (mit `free()`) laufen lecksauber durch; der ALT-Set-AP-Block
meldet ebenfalls ein Leck, der entfernte NEU-Block nicht; `App->ApTempBuffer` ist nach ALT und NEU
byteidentisch — der Block war bereits im Original ohne Effekt auf den Puffer selbst, es wird also
kein Verhalten entfernt, nur das Leck. Identisches Ergebnis bei `-O0` und `-O1`, keine
Compiler-Warnungen außer einer bei `-Wextra` erwarteten `-Wstringop-truncation`-Warnung in der
Nachbildung des ALT-`strncpy()`-Aufrufs selbst (Quellstring länger als `ApInputBufferSize`,
identisch zur realen Aufrufstelle — kein Fehler in der neuen, hier geprüften Fix-Seite).

## Risiko
Gering: an den vier Leck-Fixen rein additiv (eine zusätzliche lokale Variable plus ein `free()`
pro Stelle, kein geändertes `snprintf()`-Verhalten). Am entfernten `strncpy()`-Block entfernend,
aber auf einen bereits als wirkungslos bestätigten Block begrenzt — Host-Repro bestätigt
`App->ApTempBuffer` vor/nach identisch. Keine der vier Leck-Fixe und keine der beiden
`strncpy()`-Varianten berühren den UART-/YRM100-Protokollpfad.

## Testhinweis (lokal, Hardware)
Patch gegen die eigene `Simultaneous-UHF-RFID-FlipperZero`-Kopie anwenden (zusammen mit dem
bereits dokumentierten `view_read_c.patch` für den vollständig lecksauberen und
overflow-freien Zustand), mit `ufbt`/`fbt` bauen und auf den Flipper flashen. Regressionsfall:
Öffnen von "Set AP"/"Set Kill Pwd"/Kill-Bestätigung/Access-Passwort-Eingabe und mehrfaches
Aktualisieren der jeweiligen Eingabe muss vor und nach dem Patch identisch funktionieren
(Anzeige, Navigation, gespeicherter Wert) — nur das Leck fällt weg, der entfernte Block war schon
vorher ohne sichtbaren Effekt.

## Quelle
- `haffnerriley/Simultaneous-UHF-RFID-FlipperZero` (`main`, Commit
  `1dfefed1c67078da8c416b7b93c02eaa25e220fa`): `simultaneous_rfid_reader/views/view_config.c`
  (Zeilen 397, 728–739), `view_kill.c` (Zeilen 107, 215), `view_lock.c` (Zeile 59).
- `G4MEOVER18/g4meover-ecosystem`:
  `ideas/simultaneous-rfid-reader-convert-to-hex-string-no-terminator-fix.md` (der Fund und der
  separate Terminator-Fix, auf dem dieser Nachzug aufbaut), `ideas/simultaneous-rfid-reader-app-status.md`
  (Gesamtstatus dieses Projekts).
