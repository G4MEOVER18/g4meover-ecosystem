# CLI: `args_read_probably_quoted_string_and_trim()` erkennt eine fehlende schließende Anführung nicht — betrifft `loader open` und vier `subghz`-Befehle

Quelle: Unleashed-Firmware-Commit `6bf5e4f149bf54983d64aeb90749ae6ae39b0ddd` ("CLI: consistent
quoted path handling for loader and Sub-GHz", PR #4430, 30.07.2026), enthalten ab Tag
`unlshd-090`. Gefunden beim erneuten Gegenprüfen des Unleashed-Stands seit `unlshd-093` (offener
Punkt aus dem letzten Bericht).

## Was

`lib/toolbox/args.c`, `args_read_probably_quoted_string_and_trim()`: erkennt bei mir eine
fehlende schließende Anführung nicht als Fehler.

Die Funktion sucht ab Position 1 nach dem zweiten `"`:

```c
size_t second_quote_pos = furi_string_search_char(args, '\"', 1);

if(second_quote_pos == 0) {
    return false;
}
```

`furi_string_search_char()` (mlib-Rückgabe) liefert bei „nicht gefunden“ aber `FURI_STRING_FAILURE`
(`(size_t)-1`, also `SIZE_MAX`), nicht `0` — eine `0` kann als Ergebnis dieser Suche (Start-Index 1)
ohnehin nie auftreten. Der Vergleich `== 0` prüft damit einen Fall, der nie eintritt, und lässt den
tatsächlichen Fehlerfall (kein zweites `"` im String) durchfallen. Danach:

```c
furi_string_set_n(word, args, 1, second_quote_pos - 1);
furi_string_right(args, second_quote_pos + 1);
```

Kein Speicherfehler (per Host-Repro sowie Quellstudium von `m-string.h` verifiziert, siehe unten):
`m_string_set_n()` clamped `length` über `M_MIN(size(ref) - offset, length)`, das übernimmt hier
das reale Rest-Längen-Limit. `second_quote_pos + 1` läuft dagegen als `size_t` von `SIZE_MAX` auf
`0` über (Unsigned-Wraparound) — `furi_string_right(args, 0)` ist laut `m_string_right()` ein reines
No-op (Index `0 < size`, `memmove` kopiert den String auf sich selbst). Nettoeffekt bei fehlender
schließender Anführung: die Funktion liefert `true` statt `false`, `word` enthält den kompletten
Rest-String ab dem öffnenden `"` (ohne das `"` selbst) als vermeintlich gültigen Pfad, und `args`
bleibt komplett unkonsumiert (identisch zum Zustand vor dem Aufruf) stehen — ein stiller
Falsch-Erfolg statt der erwarteten Fehlermeldung.

Betroffen sind bei mir bereits drei Aufrufer dieser Funktion (`loader_cli.c` für den Anwendungsnamen,
`infrared_cli.c` und `lfrfid_cli.c` für Dateipfade — alle drei nutzen die Funktion unverändert zu
Unleasheds Vorher-Stand). Ein `loader open <name> "/ext/…` ohne schließende Anführung (z. B.
abgebrochene Shell-Eingabe, Copy-Paste-Fehler) liefert damit einen falschen Anwendungsnamen bzw.
eine falsche Argument-Zeichenkette statt der erwarteten Fehlermeldung „No application provided“.

Zusätzlich: `applications/main/subghz/subghz_cli.c` nutzt bei mir für `decode_raw`,
`tx_from_file`, `encrypt_keeloq` und `encrypt_raw` durchgehend die **unquoted** Variante
(`args_read_string_and_trim()`), die am ersten Leerzeichen abschneidet und Anführungszeichen nicht
entfernt — ein per Anführung übergebener Pfad mit Leerzeichen (`subghz decode_raw "/ext/subghz/Some
File.sub"`) wird dadurch auf `"/ext/subghz/Some` (samt öffnender Anführung als Teil des
„Dateinamens“) verkürzt und schlägt beim Öffnen fehl — still, mit einer irreführenden
„Datei nicht gefunden“-Meldung statt eines Hinweises auf falsche Quotierung.

Zusätzlich: `applications/services/loader/loader_cli.c`, `loader_cli_open()` übernimmt den nach dem
Anwendungsnamen verbleibenden Rest-String unverändert als Argument-String für die zu startende
Anwendung — bleibt dieser Rest selbst in Anführungszeichen (z. B. `loader open subghz
"/ext/subghz/Some File.sub"`), landen die Anführungszeichen wörtlich im Argument-String, den die
gestartete App (z. B. SubGHz beim Öffnen der Datei über `storage`) erhält, und das Öffnen schlägt
fehl.

## Betroffene Dateien

- `lib/toolbox/args.c` — `args_read_probably_quoted_string_and_trim()`: Vergleich von
  `second_quote_pos` gegen `FURI_STRING_FAILURE` statt `0`.
- `applications/services/loader/loader_cli.c` — `loader_cli_open()`: erkennt einen in
  Anführungszeichen stehenden Argument-Rest, entfernt die Anführung darum über dieselbe
  Toolbox-Funktion und hängt einen etwaigen weiteren unquoted Rest an; unverändert bleibt der
  bisherige Pfad für einen rein unquoted Argument-Rest.
- `applications/main/subghz/subghz_cli.c` — vier Aufrufstellen (`decode_raw`, `tx_from_file`,
  `encrypt_keeloq` zweimal für Quelle/Ziel, `encrypt_raw` zweimal für Quelle/Ziel) von
  `args_read_string_and_trim()` auf `args_read_probably_quoted_string_and_trim()` umgestellt —
  reiner Funktionsaustausch an bestehenden Aufrufstellen, keine Signaturänderung.
- `applications/debug/unit_tests/tests/args/args_test.c` — neuer Testfall
  `args_read_probably_quoted_string_test` (unquoted/quoted/leer-quoted/fehlende-Anführung), in die
  bestehende `run_minunit_test_toolbox_args()`-Suite eingehängt.

## Risiko

Gering. Die `args.c`-Änderung ist ein einzelner Vergleichsoperand (`0` → `FURI_STRING_FAILURE`) und
ändert das Verhalten ausschließlich für den zuvor bereits kaputten Fehlerfall (fehlende schließende
Anführung) — alle bislang funktionierenden Aufrufmuster (unquoted, korrekt quoted, leer-quoted)
bleiben laut Host-Repro (Test 1, Regression) byteidentisch. Die `subghz_cli.c`-Umstellung ersetzt an
vier Stellen eine bestehende Toolbox-Funktion durch eine andere mit identischer Signatur
(`bool(FuriString*, FuriString*)`); für jeden bisher funktionierenden unquoted Aufruf (kein
führendes `"`) fällt `args_read_probably_quoted_string_and_trim()` intern auf exakt
`args_read_string_and_trim()` zurück (siehe `else`-Zweig in `args.c`) — keine Verhaltensänderung für
bestehende Skripte/Makros ohne Anführungszeichen. Die `loader_cli.c`-Änderung ist additiv (neuer
Zweig nur bei erkanntem führenden `"` im Argument-Rest); der bisherige unquoted-Rest-Pfad bleibt im
`else`-Zweig unverändert.

## Host-Repro

`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro` in
`firmware-mining/repro-cli-quoted-path-handling-loader-subghz-fix/`. Bildet die drei tatsächlich
genutzten mlib-Primitive (`m_string_search_char`/`m_string_set_n`/`m_string_right`, Quellstudium
gegen `github.com/P-p-H-d/mlib`, `m-string.h`) in ihrer realen Semantik nach — insbesondere den
`M_MIN()`-Clamp in `m_string_set_n()` und den Index-0-Sonderfall (No-op) in `m_string_right()` — und
vergleicht ALT (eigene, unveränderte `args.c`-Fassung) gegen NEU (mit dem Patch) sowie gegen die
bisherige `subghz_cli.c`-Aufrufstelle. Tatsächliche Ausgabe (lokal ausgeführt):

```
== Test 1: uebereinstimmendes Verhalten (Regression) ==
  [5 Faelle je ALT/NEU, alle identisch]

== Test 2: fehlende schliessende Anfuehrung -- ALT vs. NEU ==
  ALT meldet bei 3/3 Faellen mit fehlender schliessender Anfuehrung faelschlich Erfolg, NEU bei 0/3

== Test 3: subghz-CLI-Aufrufe -- unquoted-Variante schneidet Pfade mit Leerzeichen ab ==
  VORHER (subghz, unquoted-Aufruf) word=""/ext/subghz/Some" rest="File.sub" 10 0"
  NACHHER (subghz, quoted-Aufruf)  word="/ext/subghz/Some File.sub" rest="10 0"

38/38 Checks bestanden
ALLE CHECKS BESTANDEN
```

Drei Testgruppen: (1) fünf Regressionsfälle (unquoted, quoted mit Leerzeichen, leer-quoted,
einzelnes quoted Wort, leere Eingabe) — ALT und NEU liefern in jedem Fall identisches `word`/`rest`;
(2) drei Varianten fehlender schließender Anführung — ALT meldet bei allen dreien fälschlich Erfolg
und lässt `args` unkonsumiert stehen, NEU meldet bei allen dreien korrekt Misserfolg; (3) direkter
Vergleich der alten `subghz_cli.c`-Aufrufstelle (`args_read_string_and_trim`, schneidet bei einem
per Anführung übergebenen Pfad mit Leerzeichen am ersten Leerzeichen ab, Anführung bleibt im
„Dateinamen“) gegen die neue (`args_read_probably_quoted_string_and_trim`, liefert den vollständigen
Pfad ohne Anführungszeichen). Unter `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`:
keine ASan-/UBSan-Meldungen.

## Betroffene Dateien (Diffs)

Vier Unified-Diffs gegen meine unveränderte mntm-012-Basis in
`firmware-mining/cli-quoted-path-handling-loader-subghz-fix/` — alle per `patch -p1 --dry-run` gegen
eine frische Basiskopie vorab verifiziert, real angewendet und Byte-für-Byte gegen die separat
editierte Fassung abgeglichen (identisch):

- `args_c.patch` (11 Zeilen) — der eine Vergleichsoperand in `args_read_probably_quoted_string_and_trim()`.
- `loader_cli_c.patch` (32 Zeilen) — Argument-Rest-Quotierung in `loader_cli_open()`.
- `subghz_cli_c.patch` (12 Zeilen) — vier Aufrufstellen in `subghz_cli.c` auf die quoted-Variante umgestellt.
- `args_test_c.patch` (39 Zeilen) — neuer Unit-Test `args_read_probably_quoted_string_test`.

## Testhinweis (lokal nachzuvollziehen)

1. `firmware-mining/repro-cli-quoted-path-handling-loader-subghz-fix/repro.c`
   host-kompilieren/ausführen (Befehl und tatsächliche Ausgabe siehe Host-Repro oben) — reine
   Logikverifikation ohne Hardware.
2. Alle vier Patches gegen eine frische `mntm-012`-Basis anwenden (`patch -p1 <
   firmware-mining/cli-quoted-path-handling-loader-subghz-fix/<datei>.patch`, Reihenfolge egal,
   unabhängig voneinander), `fbt` neu bauen.
3. `args_test_c.patch` bringt einen eigenständigen Unit-Test mit — falls die Unit-Test-App
   (`applications/debug/unit_tests`) im eigenen Build enthalten ist, direkt am Gerät ausführen und
   auf `toolbox_args_quoted_string_suite` als bestanden prüfen.
4. Am Gerät über die serielle CLI: `loader open subghz "/ext/subghz/Datei mit Leerzeichen.sub"`
   (Datei vorher unter genau diesem Namen ablegen) — vor dem Fix schlägt das Öffnen fehl
   (Anführungszeichen landen wörtlich im Pfad), nach dem Fix öffnet SubGHz die Datei korrekt.
5. `subghz decode_raw "/ext/subghz/Datei mit Leerzeichen.sub"` bzw. `tx_from_file` analog — vor dem
   Fix wird der Pfad am ersten Leerzeichen abgeschnitten (Fehlermeldung „Error open file“ mit
   sichtbar verstümmeltem Pfad inkl. Anführungszeichen), nach dem Fix wird die volle Datei geöffnet.
6. Regressionsprobe: bestehende, unquoted Aufrufe ohne Leerzeichen im Pfad
   (`loader open subghz`, `subghz decode_raw /ext/subghz/test.sub`) weiterhin unverändert
   funktionsfähig.
7. Absichtlich eine fehlende schließende Anführung testen (`loader open "subghz` ohne
   schließendes `"`) — vor dem Fix kein Fehlerhinweis (stattdessen ein falscher, verstümmelter
   Anwendungsname), nach dem Fix die erwartete Meldung „No application provided“.

## Quelle

- `github.com/DarkFlippers/unleashed-firmware`, Commit `6bf5e4f149bf54983d64aeb90749ae6ae39b0ddd`
  (PR #4430, "CLI: consistent quoted path handling for loader and Sub-GHz", Fixes #4248), enthalten
  ab Tag `unlshd-090` (per `git tag --contains` gegenpgeprüft) und damit auch weiterhin in
  `unlshd-093`.
- `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`, als eigene Basis geprüft: alle vier
  betroffenen Dateien vorhanden, die jeweils betroffenen Funktionen/Aufrufstellen
  (`args_read_probably_quoted_string_and_trim()`, `loader_cli_open()`, die vier
  `subghz_cli.c`-Aufrufstellen, `run_minunit_test_toolbox_args()`) byteidentisch zum
  Unleashed-Vorher-Stand.
- `github.com/P-p-H-d/mlib`, `m-string.h` (aktueller Stand), zur Verifikation der tatsächlichen
  `m_string_search_char`/`m_string_set_n`/`m_string_right`-Semantik (kein Speicherfehler trotz
  Unsigned-Wraparound, siehe Was-Abschnitt).
- API-87-Einordnung: reine interne CLI-/Toolbox-Logikänderung, keine neuen HAL-/API-Aufrufe, keine
  geänderte Signatur einer exportierten Funktion — kein Kompatibilitätsrisiko.
