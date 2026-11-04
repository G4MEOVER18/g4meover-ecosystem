# Simultaneous UHF RFID Reader — convert_to_hex_string() in den UI-Views ohne Nullterminator, Rückgabewert nirgends freigegeben

## Was
Ich habe den seit der letzten Sitzung offenen Rückstellpunkt zu diesem Projekt aufgegriffen: von
den zwölf UI-`views/*.c`-Dateien (`haffnerriley/Simultaneous-UHF-RFID-FlipperZero`, `main`-Branch,
Commit `1dfefed`, identisch zum bereits mehrfach bestätigten Stand) war bisher keine einzelne
geprüft. Alle zwölf (`view_about.c`, `view_config.c`, `view_delete.c`,
`view_delete_success.c`, `view_epc.c`, `view_epc_info.c`, `view_kill.c`, `view_lock.c`,
`view_read.c`, `view_saved.c`, `view_tag_actions.c`, `view_write.c`, zusammen 4620 Zeilen)
gelesen.

Fund in `view_read.c::convert_to_hex_string()` — einer zentralen, aus fünf Aufrufstellen in drei
weiteren Views heraus genutzten Formatierfunktion für Access-/Kill-Passwort-Anzeigen:

```c
// views/view_read.c, ALT:
char* convert_to_hex_string(uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        return " ";
    }
    FuriString* temp_str = furi_string_alloc();

    for(size_t i = 0; i < length; i++) {
        furi_string_cat_printf(temp_str, "%02X ", array[i]);
    }
    const char* furi_str = furi_string_get_cstr(temp_str);

    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(sizeof(char) * str_len);   // <- fehlt: +1 fuer den Terminator

    memcpy(str, furi_str, str_len);                       // <- kopiert das '\0' NICHT mit
    furi_string_free(temp_str);
    return str;
}
```

Das ist bis auf den Funktionsnamen identisch mit `convertToHexString()` aus `uhf_rfid/uhf_app.c`
(bereits dokumentiert in `firmware-mining/uhfrfid-hexstring-oob-read-fix.md`) — dieses Projekt
übernimmt den YRM100-Kern laut eigener vorheriger Einordnung direkt aus `uhf_rfid`, und dieselbe
Formatierfunktion ist dabei mit in die Views dieses Forks gewandert. Zwei Fehler, teils anders
ausgeprägt als im Original:

1. **Fehlender Nullterminator (jeder tatsächliche Aufruf).** `str_len` ist die Länge des
   aufgebauten Hex-Strings ohne Terminator (bei den hier stets festen 4-Byte-Puffern:
   `4 * 3 = 12` Zeichen, `"XX XX XX XX "` inkl. Trennleerzeichen nach jedem Byte). Die Allokation
   reserviert genau `str_len` Byte, `memcpy` kopiert genau `str_len` Byte — der Terminator aus
   `furi_str` wird nie übernommen. Alle fünf Aufrufstellen reichen den Rückgabewert direkt in
   `snprintf(tempBuffer, N, "%s", convert_to_hex_string(...))` weiter (`view_config.c:397`,
   `view_kill.c:107`, `view_kill.c:215`, `view_lock.c:59`) bzw. direkt als `strncpy()`-Ziel
   (`view_config.c:733`, siehe Nebenfund unten) — `%s` liest bis zum ersten `\0`-Byte und damit
   über die 12-Byte-Allokation hinaus: ein Heap-Buffer-Overflow-Read. Anders als bei `uhf_rfid`
   (dort tritt der Fehler nur bei nicht-leeren Bänken auf, aber praktisch bei jedem erfolgreichen
   Read) passiert das hier bei **jedem einzigen** der fünf Aufrufe, da `length` überall fest `4`
   ist (Access-/Kill-Passwort-Puffer, nie `0`) — der `array == NULL || length == 0`-Zweig wird in
   diesem Projekt nie erreicht.
2. **Rückgabewert wird an keiner der fünf Aufrufstellen freigegeben (Speicherleck).** Anders als
   bei `uhf_rfid` — dort führt ein unconditional `free()` im Leerfall zum Absturz auf
   Nicht-Heap-Speicher (String-Literal) — ruft hier niemand überhaupt `free()` auf den
   Rückgabewert auf. `snprintf(tempBuffer, N, "%s", convert_to_hex_string(...))` übergibt den
   Zeiger nur als Argument, ohne ihn in eine Variable zu übernehmen — die Allokation ist ab dem
   Ende des Ausdrucks unerreichbar. Das tritt bei jedem Öffnen der Access-/Kill-Passwort-Eingabe
   (Menüpunkt "Set AP"/"Set Kill Pwd"/Kill-Bestätigung) und jeder Passwort-Eingabe-Aktualisierung
   auf — ein kleines (12–13 Byte), aber bei wiederholter Nutzung dieser Menüs stetig wachsendes
   Leck.
3. **Nebenfund, dieselbe Funktion, `view_config.c:731–735`:** Beim Öffnen des
   "Set AP"-Menüpunkts steht
   ```c
   with_view_model(
       App->ViewRead, UHFReaderConfigModel * Model,
       {
           strncpy(
               convert_to_hex_string(App->ApTempBuffer, 4),
               furi_string_get_cstr(Model->SettingReadAp),
               App->ApInputBufferSize);   // ApInputBufferSize == 4
       },
       Redraw);
   ```
   Der `strncpy()`-Zielzeiger ist der frische, nirgends gespeicherte Rückgabewert von
   `convert_to_hex_string()` — die Anweisung beschreibt eine Allokation, die direkt im Anschluss
   unerreichbar wird, und tut sonst nichts: `App->ApTempBuffer` selbst wird hier **nicht**
   verändert. Der eigentliche Wert kommt stattdessen unverändert über den separat registrierten
   `byte_input_set_result_callback(App->ApInput, ..., App->ApTempBuffer, App->ApInputBufferSize)`
   direkt in `App->ApTempBuffer` (Zeilen 739–745) — das Widget schreibt die vom Nutzer
   eingegebenen Bytes selbst in den Puffer, unabhängig von der `strncpy`-Zeile davor. Die
   `strncpy`-Anweisung ist damit totes Füllcode mit Seiteneffekt (ein weiteres, zusätzliches
   12-Byte-Leck bei jedem Öffnen dieses Menüpunkts) statt einer wirksamen Zuweisung — kein
   Speicherfehler über die Puffergrenze hinaus, da `ApInputBufferSize` (4) kleiner als die
   (fehlerhaft berechnete) 12-Byte-Zielallokation ist, aber funktional wirkungslos und
   irreführend benannt ("Modify the value of the AP").

Mit einem eigenständigen Host-Repro unter AddressSanitizer/UBSan nachvollzogen (siehe
Testhinweis): Fund 1 löst einen `heap-buffer-overflow`-Lesefehler in der `snprintf("%s", ...)`-
Aufrufstelle mit dem jeweils realen Zielpuffer aus; Fund 2 bleibt auch nach einem reinen
Terminator-Fix an der Funktion bestehen und wird von LeakSanitizer unabhängig davon als Leck
markiert — der Funktions-Fix allein reicht nicht, jede Aufrufstelle muss den Rückgabewert
zusätzlich selbst übernehmen und freigeben.

## Nutzen
- Schließt den seit der vorigen Sitzung offenen Punkt zu diesem Projekt ("nur die zwölf UI-
  `views/*.c`-Dateien bleiben … noch offen") vollständig ab — alle zwölf Dateien jetzt
  erstgeprüft, mit einem eigenständigen, reproduzierbaren Fund.
- Bestätigt denselben, bereits bei `uhf_rfid` dokumentierten Formatierfehler als Fork-Ableger in
  einem zweiten, unabhängig genutzten Projekt — stärkt die bereits in
  `uhfrfid-hexstring-oob-read-fix.md` vermerkte Einschätzung, dass dieses `convertToHexString()`-
  Muster beim Weiterverbreiten (hier über einen Quelltext-Fork) mitwandert, ohne dass der
  ursprüngliche Fehler dabei auffällt.
- Betrifft ausschließlich reine Anzeige-/Eingabe-Logik (Passwort-Hex-Darstellung in den Settings-
  Views) — kein Lesegerät-Protokoll, kein Firmware-Kern, rein host-seitig ohne angeschlossenes
  YRM100-Modul nachvollziehbar und behebbar.
- Kleine, lokal begrenzte Änderung an einer einzigen Funktion (zwei Zeilen Allokationsgröße/Kopie
  plus der Leerfall) behebt Fund 1 vollständig; Fund 2 (Leck) bleibt als separater, noch offener
  Nachzug an den fünf Aufrufstellen (siehe Risiko/Offen) — bewusst nicht in diesem Patch
  mitgezogen, um die Änderung auf die eine, bereits durch Host-Repro abgesicherte Funktion
  begrenzt zu halten.

## Betroffene Dateien
- `simultaneous_rfid_reader/views/view_read.c` — `convert_to_hex_string()`, siehe Patch.
- `simultaneous_rfid_reader/views/view_config.c`, `view_kill.c`, `view_lock.c` — alle fünf
  Aufrufstellen profitieren vom Fix in `view_read.c`, bleiben selbst in diesem Patch unverändert
  (das Leck aus Fund 2/3 bleibt dort offen, siehe oben).
- `simultaneous_rfid_reader/views/view_about.c`, `view_delete.c`, `view_delete_success.c`,
  `view_epc.c`, `view_epc_info.c`, `view_saved.c`, `view_tag_actions.c`, `view_write.c` —
  mitgeprüft, kein weiterer Fund.

## Patch
Unified-Diff gegen die unveränderte Quelle in
[`simultaneous-rfid-reader-convert-to-hex-string-no-terminator-fix/view_read_c.patch`](simultaneous-rfid-reader-convert-to-hex-string-no-terminator-fix/view_read_c.patch).
Gegen zwei frische, unabhängige Zweitkopien verifiziert: `git apply --check`/real sowie
`patch -p1 --dry-run`/real beide erfolgreich, beide Ergebnisse per `md5sum` byte-identisch
zueinander und zur Referenzkopie, gegen die der Patch erstellt wurde.

## Testhinweis (ohne Hardware)
[`repro-simultaneous-rfid-reader-convert-to-hex-string-no-terminator/repro.c`](repro-simultaneous-rfid-reader-convert-to-hex-string-no-terminator/repro.c)
bildet die Größenrechnung von `convert_to_hex_string()` wortgetreu nach (der Zwischenstring, den
`furi_string_cat_printf()`/`furi_string_get_cstr()` im Original liefern, wird ohne FuriString
nachgebaut — für die geprüfte Größen-/Kopierlogik danach ohne Bedeutung) und spielt die reale
Aufrufstelle mit dem kleinsten der fünf Zielpuffer nach (`view_lock.c:57/59`,
`malloc(8)`/`snprintf(tempBuffer, 8, "%s", ...)`):

```
$ gcc -std=c17 -O0 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_O0
$ gcc -std=c17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_O1
$ ASAN_OPTIONS=detect_leaks=1 ./repro_O0   # identisch bei ./repro_O1
=== Szenario A, ALT (view_lock.c:59 -- snprintf(tempBuffer, 8, "%s", ...)) ===
==…==ERROR: AddressSanitizer: heap-buffer-overflow … READ of size 13 at … thread T0
    … in snprintf … in scenario_a_alt repro.c:91
0x… is located 0 bytes after 12-byte region […]
    … in convert_to_hex_string_alt repro.c:64
[ABBRUCH/LECK] Szenario A, ALT … beendete sich mit Exit-Code 1

=== Szenario A, NEU (Terminator-Fix allein, Aufrufstelle unveraendert: kein free()) ===
==…==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 13 byte(s) in 1 object(s) allocated from: … in convert_to_hex_string_neu repro.c:…
[ABBRUCH/LECK] Szenario A, NEU … beendete sich mit Exit-Code 1

=== Szenario B, NEU + korrigierter Aufrufer (Rueckgabewert wird freigegeben) ===
[OK] tempBuffer="DE AD B", Rueckgabewert freigegeben -- kein Leck
[KEIN ABBRUCH] Szenario B, NEU + korrigierter Aufrufer … lief sauber durch
```

Unter `gcc -std=c17 -Wall -Wextra` sowohl bei `-O0` als auch bei `-O1` (mit
`-fsanitize=address,undefined -fno-sanitize-recover=undefined`) identisches Verhalten, keine
Compiler-Warnungen. Drei Szenarien, jedes in einem eigenen `fork()`-Kindprozess (ein
ASan-/LeakSanitizer-Abbruch in einem Szenario beendet die übrigen nicht):
Szenario A/ALT bestätigt Fund 1 (Heap-Buffer-Overflow-Read, bricht ab); Szenario A/NEU bestätigt,
dass der reine Funktions-Fix Fund 2 **nicht** behebt (LeakSanitizer meldet weiterhin ein Leck,
weil die nachgebildete Aufrufstelle unverändert bleibt); Szenario B bestätigt, dass Funktions-Fix
plus `free()` an der Aufrufstelle zusammen vollständig sauber laufen (kein Overflow, kein Leck).
Zusätzlich sichtbar in Szenario B: der truncation-Effekt des realen `view_lock.c`-Zielpuffers
(`malloc(8)`) bleibt unverändert bestehen — `tempBuffer` zeigt `"DE AD B"` (7 Zeichen + Nullbyte,
der volle String `"DE AD BE EF "` ist 12 Zeichen lang) — das ist ein separates, bereits durch die
feste `malloc(8)`-Größe an dieser einen Aufrufstelle vorgegebenes Anzeige-Detail, kein
Speicherfehler, und nicht Gegenstand dieses Fixes.

## Risiko
- Niedrig für den eingereichten Patch: additive Erweiterung von `convert_to_hex_string()` um die
  fehlende Terminator-Byte in Allokation und Kopie sowie einen echten (statt literalen)
  Leerfall-Rückgabewert — Wire-/Anzeigeformat (`"XX XX XX XX "`) und alle fünf Aufrufstellen
  bleiben unverändert, kein Verhalten ändert sich außer dem Wegfall des Overflow-Reads.
- **Offen, bewusst nicht Teil dieses Patches:** das in Fund 2/3 beschriebene Speicherleck an den
  fünf Aufrufstellen (`view_config.c:397,733`, `view_kill.c:107,215`, `view_lock.c:59`) besteht
  nach diesem Patch unverändert weiter — jede Stelle müsste den Rückgabewert selbst in eine
  Variable übernehmen und nach dem `snprintf()`/Gebrauch freigeben. Praktisch harmlos auf dem
  Flipper selbst (wenige Byte pro Menü-Öffnung, kein Hochfrequenz-Pfad, App-Prozess endet beim
  Verlassen), aber eine für eine künftige Sitzung vorgemerkte eigenständige Nacharbeit.
- Der Nebenfund in `view_config.c:731–735` (totes `strncpy()` auf einen sofort verworfenen
  Rückgabewert) ist rein funktional wirkungslos, kein Speicherfehler über Puffergrenzen hinaus
  (Quelllänge `ApInputBufferSize` = 4 ist kleiner als die 12-Byte-Zielallokation) — für eine
  künftige Sitzung als eigenständige Aufräumarbeit vorgemerkt, nicht Teil dieses Patches.

## Quelle
- `haffnerriley/Simultaneous-UHF-RFID-FlipperZero` (`main`, Commit `1dfefed1c67078da8c416b7b93c02eaa25e220fa`):
  `simultaneous_rfid_reader/views/view_read.c` (`convert_to_hex_string()`),
  `simultaneous_rfid_reader/views/view_config.c` (Aufrufstellen Zeile 397, 733; `ApTempBuffer`/
  `ApInputBufferSize`-Deklaration Zeile 660–661; `byte_input_set_result_callback` Zeile 739–745),
  `simultaneous_rfid_reader/views/view_kill.c` (Aufrufstellen Zeile 107, 215),
  `simultaneous_rfid_reader/views/view_lock.c` (Aufrufstelle Zeile 59), sowie die übrigen acht
  UI-`views/*.c`-Dateien (mitgeprüft, kein weiterer Fund).
- `G4MEOVER18/g4meover-ecosystem`: `ideas/simultaneous-rfid-reader-app-status.md` (die beiden
  vorigen Funde an diesem Projekt), `firmware-mining/uhfrfid-hexstring-oob-read-fix.md` (derselbe
  Fehler im Ursprungsprojekt `uhf_rfid`, von dem dieses Projekt den YRM100-Kern übernimmt).
