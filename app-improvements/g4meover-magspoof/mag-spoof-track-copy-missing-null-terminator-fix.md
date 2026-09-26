# g4meover-magspoof — Track-Kopie in `mag_spoof()` terminiert den Sendepuffer nicht

## Was
`helpers/mag_helpers.c`s zentrale Sendefunktion `mag_spoof()` bislang noch nicht auf die reine
Kopierlogik hin geprüft, mit der sie jeden Track aus seinem `FuriString` in einen schlichten
C-String überführt, bevor sie ihn an `mag_encode()` übergibt:

```c
char *data1, *data2, *data3;
data1 = malloc(furi_string_size(ft1) + 1);
data2 = malloc(furi_string_size(ft2) + 1);
data3 = malloc(furi_string_size(ft3) + 1);
strncpy(data1, furi_string_get_cstr(ft1), furi_string_size(ft1));
strncpy(data2, furi_string_get_cstr(ft2), furi_string_size(ft2));
strncpy(data3, furi_string_get_cstr(ft3), furi_string_size(ft3));
```

Fund: `strncpy(dst, src, n)` schreibt den Null-Terminator in `dst` **nur**, wenn
`strlen(src) < n` ist (dann wird der Rest bis `n` mit Nullen aufgefüllt). Hier ist
`n == furi_string_size(ft)`, also exakt `strlen(src)` — die Bedingung `strlen(src) < n` ist damit
**nie** erfüllt, und das extra Byte, das `malloc(furi_string_size(ft) + 1)` gerade für den
Terminator reserviert hat (`data[size]`), bleibt unverändert: uninitialisierter Heap-Inhalt, wie
`malloc()` ihn zurückgegeben hat.

`mag_encode()` (dieselbe Datei) liest den Puffer anschließend mit einer reinen
„lies bis zum ersten Nullbyte"-Schleife:

```c
for(int i = 0; *(data + i) != 0; i++) { ... }
```

Ist `data[size]` zufällig ungleich null, läuft diese Schleife über das Ende der
`size + 1`-Byte-Allokation hinaus weiter — ein Heap-Buffer-Over-Read unbestimmter Länge, der
irgendwann zufällig auf ein Nullbyte irgendwo im Heap trifft. Bis dahin werden fremde,
uninitialisierte bzw. von vorherigen Allokationen stammende Heap-Bytes als vermeintliche
Kartendaten in `mag_encode()`s Bit-Packing eingespeist — und damit tatsächlich über die
konfigurierte Sendemethode (GPIO/RFID-Spule/Piezo/CC1101) abgestrahlt. Kein theoretisches
Speicherproblem, sondern ein direkter, unkontrollierter Lesepfad über das Ende der eigenen
Allokation hinaus, bei **jedem** einzelnen Tastendruck auf „Send" (`mag_scene_emulate.c`) neu
ausgelöst — unabhängig von der Tracklänge, sogar bei leeren Tracks (`size == 0`:
`strncpy(dst, src, 0)` fasst `dst[0]` überhaupt nicht an).

Ob sich das auf echter Hardware sofort als Absturz zeigt, hängt vom Allokator-Zustand ab: direkt
nach dem Erststart kann eine frische, zufällig genullte Speicherseite den Fehler kurzzeitig
maskieren. Die App legt diese drei Track-Puffer aber bei **jedem** Emulationsversuch neu an
(`malloc`) und gibt sie am Ende von `mag_spoof()` wieder frei (`free`) — nach dem ersten „Send"
in einer Sitzung liefert ein Allokator, der Speicher wiederverwendet, für den zweiten Versuch
mit hoher Wahrscheinlichkeit denselben Speicherbereich zurück, jetzt mit den (nicht-null) Resten
des vorherigen Tracks darin. Das ist also kein Rand- sondern ein Wiederholungsfall des ganz
normalen Bedienablaufs (Karte lesen/laden → mehrfach „Send" drücken).

Mit einem eigenständigen Host-Repro (siehe Testhinweis) unter AddressSanitizer verifiziert: die
identische Kopierlogik löst sowohl bei der allerersten Allokation dieser Größe im Prozess als
auch bei einer nach einem vorherigen „Send" wiederverwendeten Allokation zuverlässig einen
`heap-buffer-overflow` in der Scan-Schleife aus.

## Nutzen
- Schließt einen unkontrollierten Heap-Buffer-Over-Read in der zentralen Sendefunktion, der bei
  praktisch jedem "Emulate" → "Send" auftreten kann, unabhängig von Tracklänge oder Kartentyp —
  im Gegensatz zum bereits dokumentierten `mag_encode()`-Stack-Overflow
  (`mag-encode-fixed-buffer-overflow-fix.md`, betrifft nur ungewöhnlich lange Tracks >73/102
  Zeichen) ist dieser Fund unabhängig von der Tracklänge und tritt bereits beim allerkleinsten
  bzw. leeren Track auf.
- Verhindert, dass fremde Heap-Inhalte (Reste vorheriger Track-Daten, anderer Heap-Allokationen)
  unbemerkt als Kartendaten codiert und über die aktive Sendemethode abgestrahlt werden.
- Minimal-invasiv: exakt ein zusätzliches `+ 1` je Aufruf, keine neue Variable, kein geänderter
  Datentyp, keine andere Aufrufstelle betroffen — behebt die Ursache direkt an der einzigen
  Stelle, an der die drei Puffer befüllt werden (nicht an den vielen Stellen, die `FuriString`s
  mit Track-Daten befüllen).
- Rein arithmetische Kopierlogik, vollständig ohne Hardware nachrechenbar (siehe Testhinweis).

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `helpers/mag_helpers.c` — `mag_spoof()`: die drei `strncpy()`-Aufrufe für `data1`/`data2`/
  `data3` bekommen `furi_string_size(ftN) + 1` statt `furi_string_size(ftN)` als dritten
  Parameter — genau die bereits für `malloc()` verwendete Größe, sodass `strncpy` den
  Null-Terminator innerhalb der Allokation immer mitschreibt (`strlen(src) < n` ist jetzt für
  jede Tracklänge erfüllt, auch für den leeren Track).

Keine Änderung an `mag_encode()`, an einer der drei Track-`FuriString`-Befüllstellen
(UART-Lesung, `.mag`-Datei laden, manuelle Eingabe) oder an einer anderen Datei nötig — der Fix
sitzt an der einzigen Stelle, die alle drei Wege gemeinsam durchlaufen, bevor gesendet wird.

## Patch
Siehe
`app-improvements/g4meover-magspoof/mag-spoof-track-copy-missing-null-terminator-fix/mag_helpers_c.patch`
(Unified-Diff gegen die unveränderte `zacharyweiss/magspoof_flipper`-Basis, `main`, Commit
`9e782404`, `git apply --check` vorab gegen eine frische Kopie verifiziert, real angewendet und
byte-für-byte mit der beabsichtigten Endfassung abgeglichen).

## Testhinweis (ohne Hardware)
Eigenständiges, hostseitiges Reproduktionsprogramm in
`app-improvements/g4meover-magspoof/repro-mag-spoof-track-copy-missing-null-terminator-fix/`
(`repro_old.c` = ALT, `repro_new.c` = NEU), das exakt die Kopier- und Scan-Logik aus
`mag_spoof()`/`mag_encode()` nachbildet (`malloc`+`strncpy` mit derselben Parameterwahl wie im
Original, anschließend dieselbe „lies bis zum ersten Nullbyte"-Schleife wie `mag_encode()`),
ohne Furi-/Flipper-Abhängigkeit. Je Variante zwei per `fork()`/`waitpid()` isolierte Testfälle:

1. **Allererste Allokation dieser Größe im Prozess.**
2. **Wiederverwendete Allokation nach einem vorherigen, gleich großen `malloc`+`free`** — bildet
   den realistischen Wiederholungsfall nach (zweiter „Send"-Versuch in derselben Sitzung),
   deterministisch über den glibc-`tcache` (unmittelbar aufeinanderfolgendes `free()`+`malloc()`
   derselben Größe liefert denselben Chunk zurück) statt sich auf Zufall zu verlassen.

AddressSanitizer füllt zusätzlich jede `malloc()`-Allokation selbst mit einem Nicht-Null-Muster
(`0xbe`), extra um genau diese Fundklasse (Vertrauen auf ein ungeprüft angenommenes Nullbyte) zu
erkennen — mit ASan ist deshalb bereits der allererste Fall betroffen, nicht erst der
wiederverwendete.

```sh
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro_old_O0 repro_old.c && ./repro_old_O0
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro_old_O1 repro_old.c && ./repro_old_O1
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro_new_O0 repro_new.c && ./repro_new_O0
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro_new_O1 repro_new.c && ./repro_new_O1
```

Bei mir lokal reproduziert, `-O0` und `-O1`:
- **ALT** (`repro_old.c`): beide Testfälle enden mit `AddressSanitizer: heap-buffer-overflow`
  (Kind-Prozess durch `SIGABRT` beendet) — die Scan-Schleife liest über das Ende der
  `size + 1`-Byte-Allokation hinaus, exakt an der Stelle, an der `mag_encode()` das fehlende
  Nullbyte erwartet hätte.
- **NEU** (`repro_new.c`): beide Testfälle laufen sauber durch (`child exit code 0`,
  `data1[size] == 0`, gescannte Länge entspricht exakt der echten Tracklänge) —
  `ALL CHECKS PASSED`, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen.

Funktionaler Test nach dem Patch, lokal auf dem Flipper:
1. `ufbt` bauen, App starten.
2. Eine Karte lesen oder aus `Saved` laden, in `Emulate` „Send" zweimal hintereinander drücken
   (deckt sowohl die erste als auch die wiederverwendete Allokation ab).
3. Regressionsprüfung: Sendeverhalten (Timing, codierte Bits) für normale, kurze Tracks bleibt
   für ASan/UBSan bereits unverändert bestätigt identisch — ein direkter Vergleich der
   gesendeten Bitfolge auf echter Hardware (z. B. mit einem Kartenleser oder Oszilloskop am
   gewählten Ausgabepin) ist der eigentliche Nachweis, dass kein Fremd-Byte mehr codiert wird.

## Risiko
- Niedrig: Änderung betrifft ausschließlich den dritten Parameter dreier `strncpy()`-Aufrufe
  innerhalb von `mag_spoof()`, keine neue Variable, kein geänderter Datentyp, keine andere
  Aufrufstelle.
- Die Zielpuffer sind bereits exakt `furi_string_size(ftN) + 1` Byte groß (`malloc()`,
  unverändert) — der Fix schreibt nur innerhalb dieser bereits reservierten Größe, es wird
  nichts größer alloziert und nichts an der bestehenden `mag_encode()`-Signatur geändert.
- Kein Einfluss auf den bereits separat dokumentierten `mag_encode()`-Stack-Overflow bei
  überlangen Tracks (`mag-encode-fixed-buffer-overflow-fix.md`) — beide Funde betreffen
  unterschiedliche Puffer (Quelle vs. Ziel) und schließen sich nicht gegenseitig ein; dieser Fix
  behebt nur den hier beschriebenen Quellpuffer-Terminierungsfehler.

## Quelle
- `zacharyweiss/magspoof_flipper` (main, `9e782404`): `helpers/mag_helpers.c` (`mag_spoof()`).
