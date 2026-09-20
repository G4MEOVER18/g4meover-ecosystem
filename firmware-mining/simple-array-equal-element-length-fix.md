# SimpleArray-Gleichheitsvergleich — Elementlänge statt Elementanzahl (Port aus Unleashed)

## Was
Ich habe aus dem verbleibenden Unleashed-`dev`-Fenster seit `unlshd-093` den letzten noch offenen
Kandidaten aufgegriffen: Commit `370ca57bc` ("Toolbox: fix SimpleArray equality length", PR #1107,
19.09.2026, API 88.11). Er betrifft `lib/toolbox/simple_array.c`, das generische
`simple_array_is_equal()` — dieselbe Hilfsfunktion, die bei mir u. a. `iso15693_3.c`,
`type_4_tag.c`, `felica.c` und `mf_desfire.c` zum Karten-/Emulationsvergleich nutzen.

Der Vergleich prüft neben Config-Zeiger und Elementanzahl den eigentlichen Inhalt per `memcmp()`.
Die Bytelänge dafür ist bei mir `other->count` — also die **Anzahl der Elemente**, nicht deren
Bytegröße. Für Byte-Arrays (`type_size == 1`, z. B. `block_data`/`block_security`/`ndef_data`)
ist das zufällig richtig. Für jeden anderen Elementtyp (`type_size > 1`, z. B. `FelicaSystem`,
`FelicaArea`, `MfDesfireApplication`) vergleicht `memcmp()` dadurch nur die ersten `count` Bytes
des insgesamt `count * type_size` Bytes großen Puffers — bei drei Elementen zu je 32 Byte werden
also nur 3 von 96 Byte tatsächlich verglichen, der Rest wird stillschweigend als gleich
angenommen.

Die naheliegende Korrektur (`other->count * instance->config->type_size` als Bytelänge) deckt
einen zweiten, bisher durch den ersten Fehler verdeckten Fall auf: `FelicaSystem` und
`MfDesfireApplication` (sowie `MfDesfireFileData`) besitzen selbst Zeiger auf eigene,
heap-allozierte `SimpleArray`-Unterlisten (`services`/`areas`/`public_blocks` bzw.
`key_versions`/`file_ids`/`file_settings`/`file_data`/`data`). Ein vollständiger, jetzt korrekt
langer `memcmp()` über den kompletten Elementspeicher vergleicht dann diese Zeigerwerte selbst,
nicht die Daten, auf die sie zeigen — zwei inhaltlich identische, aber unabhängig geladene/
kopierte Karten (unterschiedliche Heap-Adressen für ihre Unterlisten) würden nach reiner
Bytelängen-Korrektur fälschlich als **verschieden** gelten, obwohl sie vorher (dank des ersten
Fehlers) zufällig als gleich galten. Unleashed löst das, indem `felica_is_equal()` und
`mf_desfire_is_equal()` für ihre jeweilige „besitzende" Elementliste eine eigene, elementweise
Vergleichsfunktion statt des generischen `simple_array_is_equal()` bekommen, die rekursiv wieder
`simple_array_is_equal()` auf die (nicht besitzenden, reinen Byte-/Skalar-)Unterlisten anwendet.

## Nutzen
- Behebt eine reale, bereits ausgelieferte Fehlfunktion bei der NFC-FeliCa-Emulation: laut
  Unleashed-Changelog wurde bislang jedes von einem Lesegerät während der Emulation geschriebene
  Datenblock als „Karte hat sich geändert" gewertet (weil der Vergleich effektiv nur 1 Byte der
  `systems`-Liste prüfte) und entsprechend **jedes Mal** eine Shadow-Datei geschrieben —
  unabhängig davon, ob sich tatsächlich etwas geändert hat.
- Sobald ich die Bytelängen-Korrektur ohne die zugehörigen elementweisen Wrapper übernehmen würde,
  bekäme ich stattdessen den gegenteiligen Fehler: FeliCa- und MIFARE-DESFire-Kartenvergleiche
  (`felica_is_equal()`/`mf_desfire_is_equal()`, u. a. genutzt für den „hat sich die Karte seit dem
  letzten Lesen geändert"-Vergleich) würden zwei identische Karten nie mehr als gleich erkennen,
  sobald sie mindestens ein System bzw. eine Applikation besitzen — also eine neue, selbst
  eingeführte Regression. Der hier vorgeschlagene Port liefert deshalb beide Teile zusammen.
- Betrifft ausschließlich Elementtypen mit `type_size > 1`; die bei mir ebenfalls über
  `simple_array_is_equal()` verglichenen reinen Byte-Arrays (`iso15693_3`s `block_data`/
  `block_security`, `type_4_tag`s `ndef_data`, beide `type_size == 1`) sind von beiden
  Fehlerfällen nicht betroffen und bleiben unverändert korrekt (host-seitig als Regressionsfall
  mitgeprüft, siehe unten).
- Rein additiv: kein neues Protokoll, keine geänderte Datenstruktur, keine Änderung an
  `iso15693_3.c`/`type_4_tag.c` nötig.

## Betroffene Dateien (lokal)
Unified-Diff beiliegend: `firmware-mining/simple-array-equal-element-length-fix/simple_array_equal.patch`.
Acht zusammengehörige Stellen in fünf Dateien:

1. `lib/toolbox/simple_array.c` — `simple_array_is_equal()`: Bytelänge für `memcmp()` von
   `other->count` auf `other->count * instance->config->type_size` korrigiert.
2. `lib/toolbox/simple_array.h` — Dokumentationshinweis an `simple_array_is_equal()` ergänzt: der
   Vergleich ist bytebasiert, ein Elementtyp mit eigenem Zeiger auf eine Unterliste braucht eine
   eigene, elementbewusste Vergleichsfunktion.
3. `lib/nfc/protocols/felica/felica_i.c` — neue Funktionen `felica_system_is_equal()` (statisch,
   vergleicht `system_code`/`system_code_idx` sowie die drei Unterlisten per
   `simple_array_is_equal()`) und `felica_system_array_is_equal()` (öffentlich, iteriert
   elementweise über `felica_system_is_equal()`), platziert direkt nach `felica_system_copy()`.
4. `lib/nfc/protocols/felica/felica_i.h` — Deklaration von `felica_system_array_is_equal()`.
5. `lib/nfc/protocols/felica/felica.c` — `felica_is_equal()`: `simple_array_is_equal(data->systems, ...)`
   durch `felica_system_array_is_equal(data->systems, ...)` ersetzt.
6. `lib/nfc/protocols/mf_desfire/mf_desfire_i.c` — neue Funktionen
   `mf_desfire_file_data_array_is_equal()` (statisch, für `MfDesfireApplication::file_data`, deren
   Elemente selbst nur einen Zeiger auf eine Byte-Unterliste besitzen),
   `mf_desfire_application_is_equal()` (statisch, vergleicht `key_settings` per `memcmp()` sowie
   `key_versions`/`file_ids`/`file_settings` per `simple_array_is_equal()` und `file_data` per der
   neuen Funktion) und `mf_desfire_application_array_is_equal()` (öffentlich, elementweise über
   `mf_desfire_application_is_equal()`), platziert direkt nach `mf_desfire_application_copy()`.
7. `lib/nfc/protocols/mf_desfire/mf_desfire_i.h` — Deklaration von
   `mf_desfire_application_array_is_equal()`.
8. `lib/nfc/protocols/mf_desfire/mf_desfire.c` — `mf_desfire_is_equal()`:
   `simple_array_is_equal(data->applications, ...)` durch
   `mf_desfire_application_array_is_equal(data->applications, ...)` ersetzt.

Ich habe gegen meine eigene `mntm-012`-Basis geprüft, dass `FelicaSystem` bei mir kein
`key_version`-Feld führt (neuere Unleashed-Versionen haben eines für FeliCa-Lite-S ergänzt) —
`felica_system_is_equal()` vergleicht deshalb bewusst nur die bei mir tatsächlich vorhandenen
Felder. Kein Registry-Eintrag, keine neue Datei, keine Änderung an `iso15693_3.c`/`type_4_tag.c`
nötig (deren Elementtypen sind reine Byte-Arrays ohne eigene Unterlisten).

## Verifikation (Host, ohne Hardware)
`firmware-mining/repro-simple-array-equal-element-length-fix/repro.c` — vier Testgruppen, die die
Array-Buchhaltung (`data`/`count`/`config->type_size`) sowie beide Fassungen von
`simple_array_is_equal()` nachbilden, dazu je eine elementweise Wrapper-Funktion nach dem Vorbild
von `felica_system_array_is_equal()` und `mf_desfire_file_data_array_is_equal()`. Unter
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`: 4744/4744 Prüfungen bestanden,
keine ASan-/UBSan-Meldungen.

1. **Byte-Elemente (`type_size == 1`), Regression**: für Elementanzahlen 0–12 und je 20
   Zufallsinhalte stimmen ALT und NEU immer überein (beide „gleich" bei identischem Inhalt, beide
   „verschieden" bei einem invertierten Byte an jeder möglichen Position) — bestätigt, dass der
   Fix den bereits korrekten Fall (Bytelänge == Elementanzahl bei `type_size == 1`) nicht
   verändert.
2. **Breite Skalar-Elemente (`type_size == 8`, keine Unterlisten)**: für Elementanzahlen 1–10 mit
   identischem Inhalt stimmen ALT und NEU auf „gleich" überein; wird anschließend ein einzelnes
   Byte an jeder Position 0…(`n*8-1`) verändert, erkennt NEU **jede** dieser `n*8` Änderungen
   korrekt als „verschieden", während ALT jede Änderung ab Position `n` (außerhalb seines auf
   `n` Byte verkürzten Vergleichsfensters) fälschlich als „gleich" durchgehen lässt — der
   eigentliche, hier behobene Fund.
3. **Besitzende Elemente** (Skalarfeld + Zeiger auf eigene heap-allozierte Unterliste, nach dem
   Vorbild von `FelicaSystem`/`MfDesfireApplication`): über 50 Durchläufe mit je zwei unabhängig
   aufgebauten, inhaltlich identischen Kopien (unterschiedliche Heap-Adressen für die
   Unterlisten) liefert der reine, jetzt längenkorrekte `memcmp()`-Vergleich in **jedem** Fall
   fälschlich „verschieden" (das ist der von Unleashed beschriebene Auslöser für die
   FeliCa-Shadow-Datei bei jeder Emulation), während der elementweise Wrapper in **jedem** Fall
   korrekt „gleich" liefert. Drei Gegenproben bestätigen, dass der Wrapper nicht pauschal „gleich"
   liefert: unterschiedlicher Unterlisten-Inhalt, unterschiedliches Skalarfeld und
   unterschiedliche Elementanzahl werden je korrekt als „verschieden" erkannt.
4. **Datei-Elemente** (nur ein Zeiger auf eine Byte-Unterliste, nach dem Vorbild von
   `MfDesfireFileData`): dieselbe 3a/3b/3c-Struktur eine Ebene einfacher — bestätigt denselben
   Befund für den zweiten, in `mf_desfire_i.c` benötigten Wrapper
   (`mf_desfire_file_data_array_is_equal`).

Eine Verifikation an echter Hardware (insbesondere ob meine FeliCa-Emulation nach dem Port
tatsächlich seltener Shadow-Dateien schreibt) liegt mir nicht vor und ist nur lokal möglich.

## Risiko
- Niedrig: eine Ein-Zeilen-Korrektur in einer einzigen, generischen Funktion plus zwei neue,
  rein additive Wrapper-Funktionspaare, die jeweils nur an genau einer bestehenden Aufrufstelle
  (`felica_is_equal()`, `mf_desfire_is_equal()`) eingesetzt werden. Kein bestehender Code wird
  entfernt, keine Datenstruktur geändert.
- Die beiden unveränderten Nutzer von `simple_array_is_equal()` mit `type_size > 1`-artigen,
  aber nicht besitzenden Elementen gibt es bei mir nicht — `iso15693_3.c` und `type_4_tag.c`
  nutzen ausschließlich `type_size == 1`-Byte-Arrays (per Quellcode-Grep bestätigt) und sind vom
  Fix unberührt.
- Realistischer Fehlerfall bei einem Transkriptionsfehler wäre ein falsches Vergleichsergebnis
  (zu oft „gleich" oder zu oft „verschieden"), kein Speicherfehler — alle betroffenen Operationen
  sind Zeigervergleiche, `memcmp()` mit aus `count`/`type_size` berechneter fester Länge und
  einfache Schleifen über bereits vorhandene Elementanzahlen, kein zusätzliches
  Allokations-/Freigabe-Verhalten.
- Build (`fbt`/`ufbt`) deckt jede Signaturabweichung sofort als Compile-Fehler auf.
- Funktionale Prüfung des eigentlichen Nutzens (weniger überflüssige FeliCa-Shadow-Dateien bei
  Emulation, korrekt erkannte unveränderte DESFire-Karten) nur mit echter Hardware und
  entsprechenden Karten möglich.

## Port-Skizze
1. `firmware-mining/simple-array-equal-element-length-fix/simple_array_equal.patch` gegen die
   unveränderte `mntm-012`-Basis anwenden (`patch -p1 < simple_array_equal.patch` aus dem
   Firmware-Root, vorab mit `--dry-run` geprüft — bei mir sauber gegen eine frische Basiskopie
   verifiziert).
2. Build: `ufbt` (bzw. `fbt firmware_all`) — reine Funktionssignatur-/Vergleichsänderungen,
   sollte ohne weitere Anpassungen durchlaufen.
3. Funktionstest lokal:
   - Regression zuerst: eine gespeicherte FeliCa- oder MIFARE-DESFire-Karte erneut einlesen bzw.
     laden/vergleichen — muss weiterhin als unverändert erkannt werden.
   - Eigentlicher Fund (FeliCa): eine FeliCa-Standard-Karte emulieren, mit einem Lesegerät einen
     Block beschreiben, Emulation beenden — es sollte jetzt nur noch dann eine Shadow-Datei
     entstehen, wenn sich der Karteninhalt tatsächlich geändert hat, nicht bei jedem Emulationslauf.
   - MIFARE DESFire: eine Karte mit mindestens einer Applikation zweimal nacheinander lesen —
     muss als „unverändert" erkannt werden statt als zwei verschiedene Karten.
   - Nur an eigenen bzw. vertraglich freigegebenen Karten testen.

## Quelle
- Unleashed-Firmware, `dev`-Branch, Commit `370ca57bc30af7eb6dc6d600d17c49fe5a6a44e0` ("Toolbox:
  fix SimpleArray equality length", PR #1107, 19.09.2026, API 88.11):
  `lib/toolbox/simple_array.c`, `lib/nfc/protocols/felica/felica.c`,
  `lib/nfc/protocols/felica/felica_i.c`, `lib/nfc/protocols/felica/felica_i.h`,
  `lib/nfc/protocols/mf_desfire/mf_desfire.c`, `lib/nfc/protocols/mf_desfire/mf_desfire_i.c`,
  `lib/nfc/protocols/mf_desfire/mf_desfire_i.h`, `CHANGELOG.md`.
- Momentum-Firmware, Tag `mntm-012`: dieselben acht Dateien (Ausgangszustand — Fehler
  strukturell identisch vorhanden, `FelicaSystem` bei mir ohne `key_version`-Feld).
