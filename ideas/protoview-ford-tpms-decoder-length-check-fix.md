# ProtoView — Ford-TPMS-Dekoder: Längen-Vorab-Check ist um Faktor 2 zu klein und prüft am falschen Zeitpunkt

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): nach der letzten Sitzung (Citroen-TPMS, kleinste der sechs
TPMS-Dateien) nehme ich jetzt `protocols/tpms/ford.c` (66 Zeilen) vor — bereits in der
Citroen-Sitzung als eine von drei Geschwisterdateien mit demselben Fehlermuster identifiziert,
aber noch nicht eigenständig zeilenweise geprüft und gefixt.

`decode()` enthält denselben Längen-Vorab-Check wie Citroen, nur mit anderen Konstanten:

```c
const char *sync_pattern = "010101010101" "0110";
uint8_t sync_len = 12+4; /* We just use 12 preamble symbols + sync. */
if (numbits-sync_len < 8*8) return false;
```

Dieser Check hat dieselben zwei sich verstärkenden Mängel wie beim Citroen-Fund:

1. **Falsche Größenordnung.** `8*8` (=64) ist die Anzahl der *dekodierten* Datenbits (8 Bytes),
   nicht die Anzahl der tatsächlich konsumierten *Roh*-Bitmap-Bits. Die Nutzdaten sind
   Manchester-kodiert (`convert_from_line_code(..., "01","10")`), was pro dekodiertem Bit zwei
   Roh-Bits benötigt — der tatsächliche Bedarf ist `8*8*2` (=128), nicht 64.
2. **Falscher Zeitpunkt.** Der Check läuft, *bevor* `bitmap_seek_bits()` die tatsächliche
   Sync-Position überhaupt kennt, und prüft daher nur das *gesamte* `numbits` unter der
   impliziten Annahme, der Sync-Treffer liege bei Offset 0. `bitmap_seek_bits(bits,numbytes,0,
   numbits,sync_pattern)` kann aber jede Position bis `numbits-sync_len` zurückgeben — über eine
   spätere Trefferposition sagt der Vorab-Check nichts aus.

**Unabhängige Zweitbestätigung innerhalb derselben Datei:** `info->pulses_count =
(off+8*8*2) - info->start_off;` (Zeile 42) verwendet bereits korrekt den Faktor `8*8*2` für genau
dieselbe Menge Rohbits — derselbe Autoren-Selbstwiderspruch wie bei Citroen (dort `8*10*2`),
Renault (`off+8*9*2` gegen `if (numbits-12 < 9*8)`) und Schrader-EG53MA4 (`off+10*8*2` gegen
`if (numbits-sync_len+8 < 8*10)`) — vier von sechs TPMS-Dateien zeigen dasselbe Muster, drei davon
jetzt eigenständig geprüft und gefixt (Citroen, jetzt Ford), Renault und Schrader-EG53MA4 bleiben
als eigener Rückstellpunkt offen.

**Zusätzlicher Nebenfund, derselbe Datei, derselbe Zeile:** `FURI_LOG_E(TAG, "Fort TPMS
preamble+sync found");` — Tippfehler im Protokollnamen ("Fort" statt "Ford"), auffällig auch
dadurch, dass dieselbe Datei zwei Zeilen später (`FURI_LOG_E(TAG, "Ford TPMS decoded bits: %lu",
decoded);`) den Namen korrekt schreibt. Anders als beim Citroen-Fund (dort eindeutig ein
Copy-Paste-Rest aus `renault.c`, da "Renault" dort wortwörtlich vorkommt) ist dies vermutlich ein
einfacher Tippfehler statt einer Querkontamination — ändert aber nichts an der Diagnose, dieselbe
Debug-Log-Zeile ist falsch.

## Nutzen

Deckt und behebt den bereits in der Citroen-Sitzung vorab bestätigten Faktor-2-Fund für die
zweite der drei betroffenen Geschwisterdateien, reduziert den offenen TPMS-Cluster-Rückstellpunkt
entsprechend. Ohne den Fix kann der Ford-TPMS-Decoder ein als "erfolgreich" gemeldetes,
CRC-korrektes Fahrzeugreifendruck-/Temperatur-/Flags-Ergebnis anzeigen, das teilweise aus Speicher
jenseits des tatsächlich empfangenen Funksignals stammt — irreführend für jede Auswertung eines
echten Empfangs.

## Betroffene Dateien

- `protocols/tpms/ford.c` (`antirez/protoview`, Funktion `decode()`) — Fix dieser Sitzung.
- Bereits behoben (vorherige Sitzung): `protocols/tpms/citroen.c`.
- Bestätigt, aber weiterhin nicht behoben: `protocols/tpms/renault.c`,
  `protocols/tpms/schrader_eg53ma4.c` (identisches Muster, siehe oben und bereits in der
  Citroen-Sitzung dokumentiert).

## Fund im Detail

`decode_signal()` in `signal.c` reicht an jeden Decoder denselben per bloßem `malloc()` (ohne
`memset`) angelegten, fest 4096 Byte großen Scratch-Puffer durch (bereits im KeeLoq- und
Citroen-Fund dokumentiert). `convert_from_line_code()` ist dabei an `numbytes` (die Byte-
*Kapazität* dieses Puffers) gebunden, nicht an `numbits` (die Anzahl tatsächlich abgetasteter
Bits) — liest also klaglos über das Ende des echten Empfangs hinaus, falls der aufrufende Decoder
das nicht selbst verhindert.

Konkretes Szenario: eine Aufnahme mit `numbits=96` (16 Sync-Bits + 80 reale Nutzdaten-Rohbits, das
entspricht 5 der 8 Bytes). Der bestehende Check prüft `96-16=80 < 64` → `false` ist falsch (`80` ist
nicht kleiner als `64`), lässt die Dekodierung also durch, obwohl nach dem Sync nur 80 statt der
tatsächlich benötigten 128 Rohbits real abgetastet wurden. `convert_from_line_code()` liest
klaglos über `numbits` hinaus weiter und komplettiert die fehlenden 3 Bytes aus dem
Scratch-Puffer-Rest — der dort zufällig vorhandene Inhalt wird unverändert als Teil von
"Flags"/"Moving" angezeigt, inklusive eines dafür nötigen, zufällig passenden additiven
CRC-Bytes aus derselben Quelle. Kein konstruierter Sonderfall: jede Aufnahme, bei der nach dem
Sync-Treffer weniger als 128 reale Rohbits folgen, aber die Gesamtlänge `numbits` noch über der
(zu niedrigen) Check-Schwelle liegt, ist betroffen — und das selbst dann, wenn der Sync-Treffer
(wie im Normalfall) direkt bei Offset 0 liegt.

Zusätzlich zeigt eine zweite Variante desselben Szenarios mit spätem Sync-Treffer (Offset 250
statt 0, bei insgesamt ausreichend langem `numbits=346`), dass ein naiver Fix, der nur den Faktor
korrigiert, den Vorab-Check aber weiterhin *vor* der Sync-Suche belässt, das Problem **nicht**
vollständig behebt: Er prüft weiterhin nur die Gesamtlänge, nicht die nach der tatsächlichen
(späten) Trefferposition verbleibenden Bits, und lässt daher dieselbe Kontamination ebenfalls
durch — identisch zum bereits beim Citroen-Fund gezeigten Muster.

## Fix

Minimal-invasiv, zwei Einzelstellen in `protocols/tpms/ford.c`, keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik:

1. **Haupt-Fund:** nach `off += sync_len;` (nachdem die tatsächliche Trefferposition bekannt
   ist) eine neue Prüfung einfügen: `if (off + 8*8*2 > numbits) return false;` — korrekte
   Rohbit-Größenordnung (128 statt 64), ausgewertet an der echten Sync-Position statt vorab unter
   der Annahme Offset 0. Der bestehende Vorab-Check (Zeile 20) bleibt unverändert als billiger
   Schnellausschluss vor der Sync-Suche erhalten — er ist jetzt redundant-aber-harmlos, nicht mehr
   die einzige Absicherung.
2. **Nebenfund (kosmetisch, gleiche Datei, gleicher Durchlauf):** `FURI_LOG_E(TAG, "Fort TPMS
   preamble+sync found");` → `"Ford TPMS preamble+sync found"` — reiner Tippfehler. Rein
   kosmetisch (reine Debug-Log-Ausgabe, keine Auswirkung auf Dekodierergebnis oder Anzeige), aber
   im selben Durchlauf behoben, da trivial und risikofrei.

Patch in `ideas/protoview-ford-tpms-decoder-length-check-fix/ford_c.patch`. Gegen zwei frische,
unabhängige Zweitkopien von `antirez/protoview` (`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`) per
`git apply --check`/`git apply` und separat per `patch -p1 --dry-run`/`patch -p1` verifiziert,
Ergebnisdatei in beiden Fällen laut `md5sum` byte-identisch.

## Host-Repro

`ideas/repro-protoview-ford-tpms-decoder-length-check-fix/repro.c`: alle von `decode()` genutzten
Bitmap-Primitiven (`bitmap_get`/`bitmap_set`/`bitmap_match_bits`/`bitmap_seek_bits`/
`convert_from_line_code`) wörtlich aus `signal.c` übernommen, `decode()`s Kernlogik in drei
Varianten transkribiert: ALT (Original, fehlerhaft), MID (nur Faktor korrigiert, weiterhin vor der
Sync-Suche ausgewertet) und NEU (tatsächlicher Fix: nach der Sync-Suche, mit korrektem Faktor).
Anders als bei Citroen (XOR-Checksumme) verwendet Ford eine additive Summe der ersten 7 Bytes
modulo 256 — die Hybrid-/Kontaminations-Konstruktion ist entsprechend angepasst (letztes
kontaminiertes Byte so gelöst, dass die Summe weiterhin passt), sonst identischer Aufbau wie beim
Citroen-Repro. Vier Szenarien:

- **Szenario 1** (Sync-Treffer bei Offset 0, Abbruch nach 5 von 8 realen Bytes, Rest aus
  Scratch-Speicher mit einem eindeutig fremden `0xDE`/`0xAD`-Markerpaar plus passend berechnetem
  CRC-Byte): ALT meldet fälschlich einen vollständigen, CRC-korrekten Erfolg, `raw[5]`/`raw[6]`
  sind exakt der Marker (Beweis: diese Bytes stammen nicht aus dem echten Signal). MID und NEU
  lehnen hier beide korrekt ab — bei Offset 0 reicht eine reine Faktor-Korrektur bereits aus.
- **Szenario 2** (identischer 5-von-8-Bytes-Schnitt, aber Sync-Treffer erst bei Offset 250, bei
  insgesamt ausreichend langem `numbits`): ALT und **MID** melden beide fälschlich Erfolg mit
  demselben Marker in `raw[5]`/`raw[6]` — Beleg, dass eine reine Faktor-Korrektur ohne
  Verschiebung des Checks hinter die Sync-Suche nicht ausreicht. Nur NEU lehnt korrekt ab.
- **Szenario 3** (vollständige, echte Aufnahme ohne jede Kontamination): ALT, MID und NEU
  dekodieren alle identisch und korrekt — Regressionsprobe, der Fix verändert das Verhalten für
  echte, vollständige Signale nicht.
- **Szenario 4** (grob zu kurze Aufnahme, nicht einmal das 16-Bit-Sync-Muster passt hinein): alle
  drei Varianten lehnen korrekt ab.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: `20/20 Prüfungen bestanden`, `ALL CHECKS PASSED`, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden (zusätzlich zu den bereits
dokumentierten `furi_hal_subghz_load_preset()`-, B4B1-, KeeLoq- und Citroen-Fixes), dann `./fbt
launch_app APPSRC=protoview` innerhalb des eigenen `G4MEOVER-FW`-Baums. Funktionstest: eigenen
Ford-TPMS-Reifendrucksensor (443,92 MHz FSK) empfangen, "Tire ID"/"Pressure psi"/
"Temperature C"/"Flags"/"Moving" müssen über mehrere Empfänge desselben Sensors hinweg konstant
und plausibel bleiben. Regressionsprobe: ein vollständig und störungsfrei empfangenes Signal muss
vor und nach dem Fix identisch dekodiert werden (siehe Host-Repro-Szenario 3) — der Fix darf
ausschließlich unvollständige/kontaminierte Aufnahmen zusätzlich ablehnen, keine vorher
funktionierenden Fälle.

## Risiko

Rein additiv/korrigierend (eine neue Prüfung, eine Log-String-Korrektur), keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik, kein neuer Sende-Pfad. Reine Empfangs-/
Anzeigefunktion für fremde TPMS-Sensordaten (Reifendruck/Temperatur/Flags/Tire-ID), kein Eingriff
in Ziel-Hardware — fällt wie der Rest von ProtoView unter reine passive Signalanalyse.

## Offen

Dieselbe Faktor-2-Vorab-Check-Schwäche in `protocols/tpms/renault.c` und
`protocols/tpms/schrader_eg53ma4.c` bleibt für künftige Sitzungen offen, ebenso die noch nicht
logikgeprüften `protocols/tpms/toyota.c`/`protocols/tpms/schrader.c` (andere Sync-Strategie, noch
nicht im Detail durchgesehen) und die Bit-Edit-/Keyboard-Ansicht von ProtoView selbst.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05), Datei `protocols/tpms/ford.c`.
