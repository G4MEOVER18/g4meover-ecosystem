# NFC Comparator — externe App aus RogueMaster-Bündel (Karte-vs-Datei-/Karte-vs-Ordner-Vergleich)

## Was
Ich habe den weiterhin offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster" (zuletzt
Mifare Fuzzer, siehe `firmware-mining/mifare-fuzzer-app-port.md`) fortgesetzt und **NFC
Comparator** (`applications/external/nfc_comparator/`, Autor `acegoal07`, v2.6) geprüft — mit
rund 3400 Zeilen über gut 60 C-/H-Dateien vollständig gelesen. Zwei der vier verbleibenden
Kandidaten aus demselben Rückstellpunkt (`uhf_rfid`, `simultaneous_rfid_reader`) bestätige ich
jetzt endgültig als Fremdhardware-Fall (beide `fap_category="GPIO"`, benötigen ein externes
YRM100-/M6E-Nano-/M7E-Hecto-UHF-Lesegerät über UART — kein reiner Antennen-Softwareport, wie
schon in früheren Sitzungen vermutet). NFC Comparator selbst nutzt ausschließlich die
eingebaute 13,56-MHz-Antenne über die Standard-`NfcDevice`-API (dasselbe Muster, das ich bei
Mifare Fuzzer, NFC Fuzzer und NFC/RFID Detector bereits gegen `mntm-012` verifiziert habe).

Die App vergleicht NFC-Karten/-Dumps auf zwei Wegen: **Compare** (zwei konkrete Ziele — Datei
gegen Datei, oder physische Karte gegen Datei/physische Karte — mit UID-/Protokoll-Abgleich
plus optional vollständigem Byte-für-Byte-Diff je Block/Feld für zehn Protokolle) und
**Finder** (eine Datei bzw. eine gescannte physische Karte gegen alle `.nfc`-Dateien unter
`/ext/nfc` durchsuchen und die beste Übereinstimmung melden). Damit ergänzt sie meinen
bestehenden NFC-Werkzeug-Bestand um eine mir fehlende Kategorie: Duplikatserkennung/
Integritätsabgleich zwischen eigenen Kartendumps, nützlich um zu prüfen, ob ein frisch
gespeicherter Klon wirklich identisch zum Original ist, oder ob sich eine bereits gesicherte
Zugangskarte über die Zeit geändert hat (z. B. nach einem Reset/Neuprogrammieren durch den
Betreiber).

## Fund: Finder-Suche zeigt bei einem echten Teiltreffer den falschen Dateinamen an
`nfc_comparator_finder_searcher_worker_task()`
(`lib/finder_searcher_worker/nfc_comparator_finder_searcher_worker.c`) durchläuft `/ext/nfc`
mit `dir_walk_read()` und schreibt den aktuellen Verzeichniseintrag dabei **direkt und
unbedingt** in `worker->compare_worker->nfc_card_path` — noch bevor geprüft wird, ob der
Eintrag überhaupt eine `.nfc`-Datei ist oder sich laden lässt. Das eigentliche
Vergleichsergebnis (`results.uid`/`.uid_length`/`.protocol`, `diff.count`/`.total`) wird
dagegen ausschließlich für Einträge aktualisiert, die diese Prüfungen bestehen und tatsächlich
verglichen werden — bei einem Nicht-`.nfc`-Eintrag (`continue` vor dem Vergleich) oder einer
`.nfc`-Datei, deren `nfc_device_load()` fehlschlägt (beschädigte/fremdformatige Datei), bleiben
Ergebnis/Diff komplett unverändert, während der Pfad trotzdem überschrieben wurde.

Konkret bricht das für die **Digital Finder Search** (`compare_type = Deep`, gesetzt in
`nfc_comparator_scene_digital_finder_select.c`) den bislang besten Teiltreffer, sobald danach
im Verzeichnis-Durchlauf noch mindestens ein weiterer Eintrag folgt, der entweder keine
`.nfc`-Erweiterung hat oder nicht ladbar ist (z. B. eine Unterordner-/Nicht-Karten-Datei am
Ende der alphabetischen Reihenfolge, oder eine defekte `.nfc`-Datei im selben Ordner): der am
Ende in `nfc_comparator_scene_finder_results.c` unter "Match found!"/"Partial match found!"
angezeigte Dateiname zeigt dann auf den zuletzt durchlaufenen (irrelevanten) Verzeichniseintrag
statt auf die Datei, deren UID-/Protokoll-/Diff-Werte tatsächlich angezeigt werden — ein
sicherheitsrelevanter Fehlanzeige-Fall, wenn man die Funktion nutzt, um festzustellen, welche
gespeicherte Zugangskarte einer physischen/digitalen Referenz am nächsten kommt (man greift
dann zur falschen Datei). Betroffen ist nur der Anzeigepfad; UID-/Diff-Werte selbst bleiben
korrekt, nur fälschlich einer anderen Datei zugeordnet. Die abschließende
Ergebnisauswahl-Logik (Zeilen 79–84 im Original: `tmp`-Kandidat gegen den zuletzt aktiven
`worker->compare_worker`-Zustand per `diff.count`-Vergleich) verhindert das nicht, weil sie nur
`diff.count`, nie den Pfad, auf Konsistenz prüft.

## Host-Repro
Mit einem eigenständigen Host-Repro nachgebildet (kein Flipper/keine Hardware nötig): die
Schleife aus `nfc_comparator_finder_searcher_worker_task()` reduziert auf reine
Kontrollfluss-Logik (Verzeichniseintrag → Erweiterung/Ladeprüfung → Ergebnis-/Pfad-Zuweisung),
mit drei Szenarien:

1. Echter Teiltreffer, danach eine irrelevante Nicht-`.nfc`-Datei im selben Ordner.
2. Echter Teiltreffer, danach eine `.nfc`-Datei, die nicht lädt (Ladefehler).
3. Regressionsfall: der beste Treffer ist zugleich der letzte untersuchte Eintrag (kein
   nachfolgender Eintrag mehr) — hier müssen ALT und NEU identisch sein.

ALT zeigt in Fall 1 und 2 den Dateinamen des jeweils letzten (irrelevanten) Eintrags bei
gleichzeitig korrektem `diff.count` des tatsächlichen Treffers; NEU zeigt in allen drei Fällen
den korrekten, zum `diff.count` passenden Dateinamen — Fall 3 bestätigt, dass sich am bereits
korrekten Verhalten nichts ändert. Siehe Testhinweis unten für die tatsächliche Ausgabe.

## Betroffene Dateien
- `lib/finder_searcher_worker/nfc_comparator_finder_searcher_worker.c` —
  `nfc_comparator_finder_searcher_worker_task()`, Fix wie unten.

## Risiko
Sehr gering. Reine Kontrollfluss-Änderung (ein zusätzlicher lokaler `FuriString*` statt
direktem Schreiben in das geteilte Worker-Feld), keine neue Allokationsgröße, kein API-Wechsel.
Im Erfolgsfall (Treffer ist der letzte untersuchte Eintrag, oder gar keine nachfolgenden
Einträge mehr) identisches Verhalten zu ALT — durch den Regressionsfall im Repro bestätigt.

## Port-Skizze / Fix
1. Vor der `while(dir_walk_read(...))`-Schleife einen lokalen
   `FuriString* candidate_path = furi_string_alloc();` anlegen (Lebensdauer wie das bereits
   vorhandene lokale `ext`).
2. `dir_walk_read(worker->dir_walk, worker->compare_worker->nfc_card_path, NULL)` durch
   `dir_walk_read(worker->dir_walk, candidate_path, NULL)` ersetzen.
3. Den "skip self"-Vergleich (`furi_string_cmpi(worker->compare_worker->nfc_card_path,
   worker->nfc_card_path)`) und `path_extract_ext_str(...)` entsprechend auf `candidate_path`
   umstellen.
4. `nfc_device_load(nfc_card_2, furi_string_get_cstr(candidate_path))` — ebenfalls
   `candidate_path` statt des Worker-Felds verwenden.
5. Direkt vor dem eigentlichen Vergleichsaufruf (`nfc_comparator_compare_worker_compare_cards(
   worker->compare_worker, worker->nfc_card_1, nfc_card_2)`) einmalig
   `furi_string_set(worker->compare_worker->nfc_card_path, candidate_path);` einfügen — Pfad und
   Vergleichsergebnis werden damit atomar zusammen gesetzt, nur wenn wirklich verglichen wurde.
6. Am Ende der Funktion `furi_string_free(candidate_path);` neben dem bestehenden
   `furi_string_free(ext);` ergänzen.
7. Übriger Kontrollfluss (Skip-self-Reset, Mismatch-Reset, `tmp`-Kandidatenlogik,
   Ergebnisauswahl am Ende) bleibt unverändert — der Fix betrifft ausschließlich, *wann* und
   *woher* `nfc_card_path` seinen Wert bekommt.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfc-comparator-finder-stale-path/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten:

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliches Ergebnis (lokal ausgeführt):
```
== trailing non-.nfc file after a real partial match ==
  OLD: path="readme.txt" diff=5/200
  NEW: path="access_card_A.nfc" diff=5/200
  expected: path="access_card_A.nfc" diff_count=5 -> OLD MISMATCH (bug), NEW matches

== trailing unloadable .nfc file after a real partial match ==
  OLD: path="corrupted.nfc" diff=12/200
  NEW: path="access_card_B.nfc" diff=12/200
  expected: path="access_card_B.nfc" diff_count=12 -> OLD MISMATCH (bug), NEW matches

== regression: best match is the last entry examined ==
  OLD: path="best_match.nfc" diff=3/200
  NEW: path="best_match.nfc" diff=3/200
  OLD == NEW, both "best_match.nfc": yes

CONFIRMED: OLD reports the wrong filename ... NEW keeps path and diff data in sync in all
three cases.
Exit-Code: 0
```

Für den lokalen Funktionstest an der echten App: in `/ext/nfc` mehrere gespeicherte Dumps
desselben Kartentyps mit leicht unterschiedlichem Inhalt ablegen, plus mindestens eine
beliebige Nicht-`.nfc`-Datei, deren Dateiname alphabetisch nach dem besten Treffer sortiert
(oder `dir_walk_set_recursive` mit einem Unterordner danach). "NFC Comparator" → Finder →
Digital Finder Search mit der Referenzdatei starten; vor dem Fix zeigt das Ergebnis den Namen
der zuletzt durchlaufenen, irrelevanten Datei bei korrektem Prozentwert, nach dem Fix den
tatsächlich verglichenen Dateinamen.

## Zusätzlich geprüft, kein Fund
- `nfc_comparator_compare_worker_compare_cards()` und alle zehn Protokoll-`compare_cards()`-
  Implementierungen (Felica, MfUltralight, MfClassic, St25tb, Type4Tag, Iso15693_3, Slix, Emv,
  MfPlus, MfDesfire): durchgängig dasselbe Muster (Typ-/Blockzahl-Gleichheit prüfen, sonst auf
  `NfcCompareWorkerType_Shallow` zurückfallen; Diff-Array via `simple_array_init()` pro Aufruf
  frisch angelegt; alle Schleifen zusätzlich hart gegen die jeweilige `_MAX_*`-Konstante
  begrenzt) — kein Speicherzugriffsfehler, kein Off-by-one gefunden.
- Zwei `static volatile bool reset_exit`-Flags (`nfc_comparator_scene_compare_results.c`,
  `nfc_comparator_scene_finder_results.c`) werden nach dem ersten Aufruf der "More"-Ansicht nie
  wieder auf `true` zurückgesetzt — geprüft, ob das zu sichtbar falschen Ergebnissen führt: alle
  Felder, die `nfc_comparator_compare_worker_compare_cards()` selbst setzt
  (`results.*`, `diff.count`, `diff.total`, `diff.indices`), werden bei jedem neuen Vergleich
  ohnehin vollständig neu geschrieben, unabhängig vom Reset. Einziges Feld, das dadurch
  zwischen zwei Compare-/Finder-Durchläufen "durchsickern" könnte, ist `nfc_card_path` selbst —
  und genau dieses Feld wird durch den oben portierten Fix jetzt immer korrekt mit den
  zugehörigen Ergebnisdaten zusammen gesetzt, wodurch dieses zweite, kleinere Problem als
  Nebeneffekt mit abgedeckt ist. Kein eigenständiger Zusatz-Fix nötig.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser
  Sitzung): `applications/external/nfc_comparator/`, `fap_author` "acegoal07", Original-Quelle
  `github.com/acegoal07/FlipperZero_NFC_Comparator`.
- API-87-Nachweis: `Next-Flip/Momentum-Firmware`, Tag `mntm-012`, Sparse-Checkout
  `lib/toolbox/dir_walk.h`, `lib/toolbox/simple_array.h`, `lib/nfc/nfc_device.h` — alle von der
  App verwendeten Funktionssignaturen (`dir_walk_read`, `simple_array_init`/`_get`/`_cget`/
  `_copy`/`_reset`, `nfc_device_get_uid`/`_get_protocol`/`_is_equal`/`_get_data`/`_load`) 1:1
  vorhanden, kein Firmware-Patch nötig.
- Eigener Fund: `lib/finder_searcher_worker/nfc_comparator_finder_searcher_worker.c`,
  `nfc_comparator_finder_searcher_worker_task()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfc-comparator-finder-stale-path/repro.c`).
- Restlicher NFC-Werkzeug-Cluster aus demselben Rückstellpunkt: `uhf_rfid` und
  `simultaneous_rfid_reader` jetzt als Fremdhardware-Fall abgeschlossen (siehe oben); `nfctools`
  (~11 200 Zeilen, größter verbleibender Kandidat) für eine künftige Sitzung offen.
