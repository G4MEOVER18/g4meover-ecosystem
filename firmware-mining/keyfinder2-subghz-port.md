# SubGHz: KeyFinder2 als neues, eigenständiges Protokoll portiert

## Was

Unleashed-Firmware hat seit dem letzten Tag `unlshd-093` (aktuell weiterhin die neueste
Release-Markierung, keine neuere `unlshd-09x`/`unlshd-1xx` vorhanden) auf dem `dev`-Branch
mehrere neue, noch unveröffentlichte SubGHz-Protokolle angesammelt — bereits in der
Doorbell32-Sitzung gegen den Stand `af2e764` (2026-10-11) aufgelistet: zwei neue
eigenständige SubGHz-Protokolldateien (`doorbell32.c/.h`, bereits portiert; `keyfinder2.c/.h`,
hier aufgegriffen), eine neue NFC-Protokollfamilie (`texkom`), ein neues
LFRFID-Werkzeugmodul (`hitags`) sowie Erweiterungen an bereits portierten Protokollen. Gegen
den `dev`-Branch erneut geprüft: weiterhin derselbe Commit `af2e764`, kein neuerer Stand
vorhanden — keine Wiederholung, echte Fortsetzung derselben Restliste.

`keyfinder2.c/.h` gewählt (283+60 Zeilen): die kleinere der beiden verbleibenden
eigenständigen Einheiten aus diesem Diff-Batch (`texkom`/`hitags` sind mehrteilige Ports, wie
schon bei Doorbell32 zurückgestellt), ein reines statisches Protokoll ohne
Rolling-Code/Kryptografie — passt in eine Sitzung, rein host-verifizierbar.

KeyFinder2 ist laut eigenem Kommentar im Unleashed-Quelltext ein Schlüsselfinder-Fob-Protokoll:
ein 11-Bit-Rahmen `btn(3) | 10011 | btn(3)` — der 3-Bit-Tastencode zuerst in drei breiten
Symbolen (1500/2500 µs, ±350 µs Toleranz), danach eine feste 5-Bit-Konstante und derselbe
Tastencode erneut, diesmal in drei schmalen Symbolen (350/1020 µs, ±150 µs Toleranz, dieselben
Timings wie das bereits bei mir vorhandene `princeton.c`, nur andere Bitbreite). Die Konstante
wird beim Dekodieren nicht geprüft — jedes beliebige 5-Bit-Muster an dieser Stelle wird
akzeptiert, solange die Timings passen (unverändertes Original-Verhalten, siehe Fund unten).
Rahmen wiederholen sich lückenlos direkt hintereinander (kein Gap, keine explizite
Rahmenende-Markierung).

## Kompatibilitätsprüfung gegen `mntm-012`

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag-Commit
`b359f6fc32f282ce7217dd2d48a4e3478771852e`, derselbe Stand wie bei allen bisherigen
SubGHz-Ports dieser Sitzungsreihe) geprüft:

- `../blocks/const.h`, `decoder.h` (`SubGhzBlockDecoder.{parser_step,te_last,decode_data,
  decode_count_bit}`), `encoder.h` (`SubGhzProtocolBlockEncoder`), `generic.h`
  (`SubGhzBlockGeneric.{protocol_name,data,data_count_bit,btn}`,
  `subghz_block_generic_deserialize_check_count_bit()`), `math.h` (`bit_read`,
  `DURATION_DIFF`) sowie `lib/subghz/types.h` (`SubGhzProtocolDecoder`/
  `SubGhzProtocolEncoder`-Vtables) sind bei mir strukturell identisch zur Unleashed-Quelle —
  jedes von KeyFinder2 verwendete Feldnamen/Typsignatur passt unverändert.
- `blocks/decoder.c`: `subghz_protocol_blocks_add_bit()` und
  `subghz_protocol_blocks_get_hash_data()` (kurze Form, `uint8_t`-Rückgabe) byteidentisch
  vorhanden.
- **Fehlt bei mir, wie bei jedem bisher aus Unleashed portierten SubGHz-Protokoll bereits
  dokumentiert** (Jarolift/KeyFinder/Nord-ICE/Treadmill37/Allstar-Firefly/Ditec-GOL4/
  Doorbell32/Superrollo): die gemeinsame `lib/subghz/protocols/common.c/.h`-Hilfsschicht.
  Anders als Doorbell32 (das bei mir eigene `te`/`last_data`-Felder nach Vorbild von
  `princeton.c` braucht) ist KeyFinder2 hier der einfachere Fall: das Original selbst trägt
  weder ein `te`- noch ein `last_data`-Feld (keine gemessene Timing-Mittelung, keine
  Rahmen-Konsistenzprüfung, siehe unten) — mein Ersatz für `common.c` ist dadurch nur
  `_alloc`/`_free`/`_reset`/`_get_hash_data`/`_serialize` ohne jeden Zusatzzustand, 1:1 nach
  dem bei `common.c` selbst nachgelesenen Verhalten
  (`subghz_protocol_decoder_common_alloc/_free/_reset/_get_hash_data/_serialize`, Zeile für
  Zeile gegen `af2e764`s `common.c` abgeglichen, siehe Fix unten).
- `SubGhzProtocolFlag_Sensors` existiert in meinem `SubGhzProtocolFlag`-Enum nicht (bei mir
  endet die Flag-Liste bei `SubGhzProtocolFlag_BinRAW`, bereits bei Doorbell32 dokumentiert).
  Bewusst weggelassen statt erfunden — reine UI-Kategorisierung, ohne jede Auswirkung auf
  Encode/Decode/Senden/Empfangen.
- `lib/subghz/SConscript` globbt `protocols/*.c*` rekursiv — keine Build-Datei-Änderung nötig.
- Registrierung in `protocol_items.h`/`.c` 1:1 nach demselben Muster wie bei allen bisher
  portierten Protokollen (Include + Array-Eintrag ans Dateiende angehängt, gegen den
  unveränderten Stand nach dem zuletzt dokumentierten Eintrag `elplast`).

## Fund: kein Speicherfehler, zwei dokumentierte Verhaltensunterschiede zu den Geschwister-Ports

Keine Fehler, die ich verändert habe — unverändertes, 1:1 aus Unleashed übernommenes
Verhalten (verifiziert gegen den tatsächlichen Quelltext von `keyfinder2.c`, nicht nur gegen
den eigenen Kommentar), hier für meine eigene Basis gegengerechnet und dokumentiert, weil
beide Punkte das Gegenteil der bisher für Doorbell32/Ditec-GOL4/Nord-ICE/Treadmill37/
Allstar-Firefly dokumentierten GAP-Doppelnutzungs- bzw. Zwei-Rahmen-Bestätigungs-Funde sind:

1. **Keine Rahmen-Konsistenzprüfung vor dem Melden.** Anders als `princeton.c`/`doorbell32.c`
   (dort `last_data == decode_data`, erst der zweite von zwei identischen Rahmen wird
   gemeldet) meldet KeyFinder2 jeden einzelnen Rahmen, sobald `decode_count_bit ==
   min_count_bit_for_found` (11) erreicht ist — ohne jede Prüfung gegen einen vorherigen
   Rahmen. Host-Repro (Szenario 1, "Single-frame"-Teil): ein einzelner, nicht wiederholter
   11-Bit-Rahmen wird bereits beim ersten Durchlauf gemeldet, bit-exakt. Praktische Folge:
   ein zufälliger Störimpuls, der rein zufällig auf die Timing-Toleranzen aller 11 Bits
   passt, würde sofort als "empfangener Tastendruck" gemeldet, ohne die bei den anderen
   eigenen SubGHz-Protokollen übliche zweite Bestätigung. Nicht verändert — unverändertes
   Originalverhalten, aber beim künftigen Störfestigkeits-Vergleich zwischen meinen
   portierten Protokollen zu berücksichtigen.
2. **Keine Rahmenanfangs-Synchronisation über ein Gap/Guard-Fenster.** Anders als
   `princeton.c`/`doorbell32.c`/`ditec_gol4.c` (dort muss `Reset` erst eine lange
   LOW-Guard-Dauer sehen, bevor es den ersten Rahmen überhaupt beginnt zu erfassen — das
   erste Repeat eines Bursts geht dadurch bei jenen Protokollen immer verloren) erkennt
   KeyFinder2s `Reset`-Zustand direkt die erste HIGH-Flanke eines Rahmens, sofern ihre Dauer
   auf die breite Kurz- oder Lang-Vorlage passt — keine vorherige Gap-Erkennung nötig. Host-
   Repro (Szenario 1): bei lückenlos aneinandergereihten Wiederholungen wird **jede** davon
   gemeldet (`found_count == repeat`, nicht `repeat-2` wie bei Doorbell32 oder `ceil(repeat/2)`
   wie bei den GAP-Doppelnutzungs-Fällen). Diese zwei Eigenschaften sind zwei Seiten derselben
   Designentscheidung: ohne Gap-Synchronisation gibt es auch keinen natürlichen Anker für eine
   Zwei-Rahmen-Bestätigung.

Beide Punkte sind reine Dokumentation des vom Originalhersteller so entworfenen Verhaltens
(ich habe nichts repariert, weil nichts kaputt ist) — wichtig für den künftigen
Hardware-Testhinweis unten, da dieses Protokoll empfindlicher auf einzelne Störimpulse
reagieren dürfte als seine bereits portierten Geschwister.

## Fix / Port

Additiver Port (keine Änderung an bestehendem Code): `keyfinder2.c`/`.h` 1:1 aus Unleashed
übernommen, `common.c`-Aufrufe durch eigene `_alloc`/`_free`/`_reset`/`_get_hash_data`/
`_serialize`-Funktionen ersetzt (exakt das Verhalten, das die fehlenden `common.c`-Funktionen
für dieses Layout hätten), `SubGhzProtocolFlag_Sensors` aus dem Flag-Feld entfernt. Die
Dekoder-/Encoder-Logik selbst (Zustandsautomat, Bit-Timing, `check_remote_controller()`,
`get_string()`) ist unverändert übernommen.

Betroffene Dateien (alle additiv, keine bestehende Datei verändert außer den beiden
Registrierungsstellen):

- `lib/subghz/protocols/keyfinder2.c` — neu (Patch: `keyfinder2_c.patch`)
- `lib/subghz/protocols/keyfinder2.h` — neu (Patch: `keyfinder2_h.patch`)
- `lib/subghz/protocols/protocol_items.h` — ein `#include` angehängt (Patch:
  `protocol_items_h.patch`)
- `lib/subghz/protocols/protocol_items.c` — ein Array-Eintrag angehängt (Patch:
  `protocol_items_c.patch`)

Alle vier Patches gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW`
(`b359f6f`) mit `git apply --check` verifiziert, real angewendet, Ergebnis per `diff`
byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

## Host-Repro

`../repro-keyfinder2-subghz-port/repro.c`: transkribiert Encoder (`get_upload()`) und
Dekoder-Zustandsautomat (`feed()`, fünf Zustände) verbatim, ohne jeden Flipper-/furi-Header.
Fünf Szenarien (80016 Einzelprüfungen):

1. Encode→Decode-Rundlauf über 20000 zufällige 11-Bit-Werte, Wiederholungszahl 1-6,
   lückenlos aneinandergereiht: `found_count == repeat` (jede Wiederholung wird gemeldet,
   inkl. der allerersten), bit-exakt; zusätzlich ein einzelner, nicht wiederholter Rahmen
   für sich allein.
2. Negativ: ein verfälschtes Timing im breiten Header-Teil bricht zurück zu `Reset` ab, ohne
   Fehlmeldung, und der direkt folgende, wohlgeformte Rahmen dekodiert trotzdem sofort
   (keine Resync-Rahmen nötig).
3. Negativ: dasselbe im schmalen Trailer-Teil.
4. Grenzfall: der Wechsel vom breiten in den schmalen Bit-Paar-Modus erfolgt exakt nach dem
   3. dekodierten Bit, nie früher oder später (pro Bit einzeln geprüft).
5. Encoder-Puffergröße: 11 Bit → 22 Slots, bleibt innerhalb der 32 allozierten Slots.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: alle 80016 Prüfungen bestanden, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine ASan-/UBSan-Meldungen.

## Risiko

Rein additiv (zwei neue Dateien, zwei Einzeiler-Ergänzungen in bereits existierenden
Registrierungsdateien) — kein bestehender Protokoll-Code wird verändert. Statisches
Protokoll ohne Rolling-Code, keine kryptografische Komponente. Einziges reales Risiko: ein
Namens-/Registrierungskonflikt mit einem zwischenzeitlich eigenständig hinzugefügten
Protokoll gleichen Namens — beim Anwenden per `git apply --check` gegen den tatsächlichen
lokalen Stand vor dem Mergen auszuschließen.

## Testhinweis (lokal)

1. Patches in dieser Reihenfolge anwenden: `keyfinder2_h.patch`, `keyfinder2_c.patch`,
   `protocol_items_h.patch`, `protocol_items_c.patch`.
2. Mit `ufbt`/`fbt` bauen, auf eigenem Gerät flashen.
3. Mit einem eigenen KeyFinder2-kompatiblen Fob (oder einem per `subghz_tx`/`.sub`-Datei
   synthetisch erzeugten 11-Bit-Testsignal, Timings wie oben) senden/empfangen testen:
   korrekte Dekodierung, korrekte Tastenfarben-Anzeige (`subghz_protocol_keyfinder2_color()`).
4. Gegenprobe zum dokumentierten Fund: ein Einzelimpuls (kein Wiederholpaar) sollte bereits
   als "empfangen" gemeldet werden — im Gegensatz zu den bereits vorhandenen
   Doorbell32/Princeton-artigen Protokollen, bei denen ein Einzelimpuls folgenlos verloren
   geht. Keine Hardware-Abhängigkeit über die ohnehin vorhandene SubGHz-Pipeline hinaus.
