# ProtoView — Renault-TPMS-Dekoder: Längen-Vorab-Check ist um Faktor 2 zu klein, prüft am falschen Zeitpunkt und subtrahiert eine zur Praambel-/Sync-Länge inkonsistente Konstante

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): nach Citroen und Ford nehme ich jetzt `protocols/tpms/renault.c`
(123 Zeilen) vor — die dritte der drei bereits in der Citroen-Sitzung als strukturell identisch
identifizierten Geschwisterdateien, jetzt eigenständig zeilenweise geprüft und gefixt.

`decode()` enthält denselben Längen-Vorab-Check wie Citroen/Ford, mit eigenen Konstanten:

```c
const char *sync_pattern = "01010101010101010110";
uint8_t sync_len = 12+4; /* -- Anmerkung: so bei Citroen/Ford, bei Renault steht nur */
if (numbits-12 < 9*8) return false;
...
uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
if (off == BITMAP_SEEK_NOT_FOUND) return false;
info->start_off = off;
off += 20; /* Skip preamble. */
```

(Die zweite Zeile ist zur Klarstellung ergänzt — Renault hat keine eigene `sync_len`-Variable,
der Vorab-Check subtrahiert die Konstante `12` direkt.)

Dieser Check hat dieselben zwei sich verstärkenden Mängel wie bei Citroen/Ford, plus einen
dritten, hier erstmals auftretenden, unabhängigen Mangel:

1. **Falsche Größenordnung.** `9*8` (=72) ist die Anzahl der *dekodierten* Datenbits (9 Bytes),
   nicht die Anzahl der tatsächlich konsumierten *Roh*-Bitmap-Bits. Die Nutzdaten sind
   Manchester-kodiert (`convert_from_line_code(..., "01","10")`), was pro dekodiertem Bit zwei
   Roh-Bits benötigt — der tatsächliche Bedarf ist `9*8*2` (=144), nicht 72.
2. **Falscher Zeitpunkt.** Der Check läuft, *bevor* `bitmap_seek_bits()` die tatsächliche
   Sync-Position überhaupt kennt, und prüft daher nur das *gesamte* `numbits` unter der
   impliziten Annahme, der Sync-Treffer liege bei Offset 0.
3. **Inkonsistente Konstante (neu, nur bei Renault).** Der Vorab-Check subtrahiert `12`, aber
   `strlen(sync_pattern)` ist `20` (`"01010101010101010110"`), und genau `20` wird zwei Zeilen
   später tatsächlich als Sprungweite verwendet (`off += 20; /* Skip preamble. */`). Die `12`
   stimmt mit keiner der beiden tatsächlich relevanten Größen überein — selbst ein Fix, der nur
   den Faktor korrigiert (`9*8` → `9*8*2`), aber die `12` unverändert ließe, würde immer noch 8
   Roh-Bits weniger verlangen als tatsächlich nötig. Dieser dritte Mangel ist unabhängig von (1)
   und (2): er besteht auch dann, wenn man sich (hypothetisch) nur auf den Gesamtlängen-Vorab-Check
   beschränkt und den Zeitpunkt (2) ignoriert.

**Unabhängige Zweitbestätigung innerhalb derselben Datei:** `info->pulses_count =
(off+8*9*2) - info->start_off;` (Zeile 54) verwendet bereits korrekt den Faktor `8*9*2` für genau
dieselbe Menge Rohbits — derselbe Autoren-Selbstwiderspruch wie bei Citroen (`8*10*2`) und Ford
(`8*8*2`). Damit sind jetzt alle drei in der Citroen-Sitzung identifizierten Geschwisterdateien
eigenständig geprüft und gefixt (Citroen, Ford, jetzt Renault); Schrader-EG53MA4 (`off+10*8*2`
gegen `if (numbits-sync_len+8 < 8*10)`, laut Citroen-Sitzung ebenfalls betroffen) bleibt als
eigener Rückstellpunkt offen.

Die beiden Debug-Log-Zeilen (`"Renault TPMS preamble+sync found"`, `"Renault TPMS decoded
bits: %lu"`) sind — anders als bei Citroen/Ford — korrekt benannt, kein Tippfehler-Nebenfund in
dieser Datei.

## Nutzen

Deckt und behebt den bereits in der Citroen-Sitzung vorab bestätigten Faktor-2-Fund für die
dritte und letzte der drei ursprünglich identifizierten Geschwisterdateien, reduziert den offenen
TPMS-Cluster-Rückstellpunkt entsprechend, und dokumentiert zusätzlich einen eigenständigen,
Renault-spezifischen dritten Mangel (inkonsistente Konstante), der beim bloßen Kopieren eines
Citroen/Ford-artigen Fixes übersehen werden könnte. Ohne den Fix kann der Renault-TPMS-Decoder ein
als "erfolgreich" gemeldetes, CRC-korrektes Fahrzeugreifendruck-/Temperatur-/Tire-ID-/
Unknown1-Unknown2-Ergebnis anzeigen, das teilweise aus Speicher jenseits des tatsächlich
empfangenen Funksignals stammt.

## Betroffene Dateien

- `protocols/tpms/renault.c` (`antirez/protoview`, Funktion `decode()`) — Fix dieser Sitzung.
- Bereits behoben (vorherige Sitzungen): `protocols/tpms/citroen.c`, `protocols/tpms/ford.c`.
- Bestätigt, aber weiterhin nicht behoben: `protocols/tpms/schrader_eg53ma4.c` (identisches
  Grundmuster, siehe oben, bereits in der Citroen-Sitzung dokumentiert).

## Fund im Detail

`decode_signal()` in `signal.c` reicht an jeden Decoder denselben per bloßem `malloc()` (ohne
`memset`) angelegten, fest 4096 Byte großen Scratch-Puffer durch (bereits im KeeLoq-, Citroen- und
Ford-Fund dokumentiert). `convert_from_line_code()` ist dabei an `numbytes` (die Byte-*Kapazität*
dieses Puffers) gebunden, nicht an `numbits` (die Anzahl tatsächlich abgetasteter Bits) — liest
also klaglos über das Ende des echten Empfangs hinaus, falls der aufrufende Decoder das nicht
selbst verhindert.

Konkretes Szenario: eine Aufnahme mit `numbits=116` (20 Sync-/Präambel-Bits + 96 reale
Nutzdaten-Rohbits, das entspricht 6 der 9 Bytes). Der bestehende Check prüft `116-12=104 < 72` →
`false` ist falsch (`104` ist nicht kleiner als `72`), lässt die Dekodierung also durch, obwohl
nach dem Sync nur 96 statt der tatsächlich benötigten 144 Rohbits real abgetastet wurden.
`convert_from_line_code()` liest klaglos über `numbits` hinaus weiter und komplettiert die
fehlenden 3 Bytes (Unknown1, Unknown2, CRC) aus dem Scratch-Puffer-Rest — der dort zufällig
vorhandene Inhalt wird unverändert als Teil von "Unknown1"/"Unknown2" angezeigt, inklusive eines
dafür nötigen, zufällig passenden CRC8-Bytes aus derselben Quelle.

Zusätzlich zeigt eine zweite Variante desselben Szenarios mit spätem Sync-Treffer (Offset 250
statt 0, bei insgesamt ausreichend langem `numbits=366`), dass ein Fix, der sowohl den Faktor
*als auch* die inkonsistente Konstante korrigiert (`numbits-20 < 9*8*2`), aber den Vorab-Check
weiterhin *vor* der Sync-Suche belässt, das Problem **nicht** vollständig behebt: Er prüft
weiterhin nur die Gesamtlänge, nicht die nach der tatsächlichen (späten) Trefferposition
verbleibenden Bits, und lässt daher dieselbe Kontamination ebenfalls durch — identisch zum bereits
bei Citroen/Ford gezeigten Muster, hier zusätzlich mit korrigierter Konstante nachgewiesen.

## Fix

Minimal-invasiv, eine neue Prüfung in `protocols/tpms/renault.c`, keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik: nach `off += 20; /* Skip preamble. */` (nachdem die
tatsächliche Trefferposition bekannt ist) eine neue Prüfung einfügen: `if (off + 9*8*2 > numbits)
return false;` — korrekte Rohbit-Größenordnung (144 statt 72), ausgewertet an der echten
Sync-Position statt vorab unter der Annahme Offset 0, und unabhängig von der inkonsistenten
`12`-Konstante des bestehenden Vorab-Checks. Der bestehende Vorab-Check (Zeile 35) bleibt
unverändert als billiger Schnellausschluss vor der Sync-Suche erhalten — er ist jetzt
redundant-aber-harmlos, nicht mehr die einzige Absicherung.

Patch in `ideas/protoview-renault-tpms-decoder-length-check-fix/renault_c.patch`. Gegen zwei
frische, unabhängige Zweitkopien von `antirez/protoview` (`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`)
per `git apply --check`/`git apply` und separat per `patch -p1 --dry-run`/`patch -p1` verifiziert,
Ergebnisdatei in beiden Fällen laut `md5sum` byte-identisch zur jeweils dritten, unabhängig von
Hand bearbeiteten Kopie.

## Host-Repro

`ideas/repro-protoview-renault-tpms-decoder-length-check-fix/repro.c`: alle von `decode()`
genutzten Bitmap-Primitiven (`bitmap_get`/`bitmap_set`/`bitmap_match_bits`/`bitmap_seek_bits`/
`convert_from_line_code`) sowie `crc8()` wörtlich aus `signal.c`/`crc.c` übernommen, `decode()`s
Kernlogik in drei Varianten transkribiert: ALT (Original, fehlerhaft, `12`-Konstante), MID (Faktor
UND Konstante korrigiert — `9*8*2` statt `9*8`, `20` statt `12` — aber weiterhin vor der Sync-Suche
ausgewertet) und NEU (tatsächlicher Fix: nach der Sync-Suche, mit korrektem Faktor). Anders als bei
Citroen (XOR) und Ford (additive Summe) verwendet Renault eine echte CRC8 (Polynom 7,
Initialisierung 0) über die ersten 8 Bytes — die Hybrid-/Kontaminations-Konstruktion ist
entsprechend angepasst (letztes kontaminiertes Byte, die CRC selbst, neu berechnet, sodass die
Hybrid-Nachricht weiterhin CRC-gültig ist), sonst identischer Aufbau wie bei den vorherigen
Repros. Vier Szenarien:

- **Szenario 1** (Sync-Treffer bei Offset 0, Abbruch nach 6 von 9 realen Bytes, Rest aus
  Scratch-Speicher mit einem eindeutig fremden `0xDE`/`0xAD`-Markerpaar plus neu berechnetem
  CRC-Byte): ALT meldet fälschlich einen vollständigen, CRC-korrekten Erfolg, `raw[6]`/`raw[7]`
  sind exakt der Marker (Beweis: diese Bytes stammen nicht aus dem echten Signal). MID lehnt hier
  korrekt ab — bei einem byte-ausgerichteten Schnitt an Offset 0 reicht die kombinierte
  Faktor-/Konstanten-Korrektur bereits aus (anders als bei Ford, wo MID dort schon reichte, zeigt
  dies für Renault, dass auch die zusätzliche Konstanten-Korrektur in diesem Fall greift).
- **Szenario 2** (identischer 6-von-9-Bytes-Schnitt, aber Sync-Treffer erst bei Offset 250, bei
  insgesamt ausreichend langem `numbits`): ALT und **MID** melden beide fälschlich Erfolg mit
  demselben Marker in `raw[6]`/`raw[7]` — Beleg, dass selbst eine Korrektur von Faktor UND
  Konstante ohne Verschiebung des Checks hinter die Sync-Suche nicht ausreicht. Nur NEU lehnt
  korrekt ab.
- **Szenario 3** (vollständige, echte Aufnahme ohne jede Kontamination): ALT, MID und NEU
  dekodieren alle identisch und korrekt — Regressionsprobe, der Fix verändert das Verhalten für
  echte, vollständige Signale nicht.
- **Szenario 4** (grob zu kurze Aufnahme, nicht einmal das 20-Bit-Sync-Muster passt hinein): alle
  drei Varianten lehnen korrekt ab.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: `20/20 Prüfungen bestanden`, `ALL CHECKS PASSED`, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden (zusätzlich zu den bereits
dokumentierten `furi_hal_subghz_load_preset()`-, B4B1-, KeeLoq-, Citroen- und Ford-Fixes), dann
`./fbt launch_app APPSRC=protoview` innerhalb des eigenen `G4MEOVER-FW`-Baums. Funktionstest:
eigenen Renault-TPMS-Reifendrucksensor (443,92 MHz FSK) empfangen, "Tire ID"/"Pressure kpa"/
"Temperature C"/"Flags"/"Unknown1"/"Unknown2" müssen über mehrere Empfänge desselben Sensors
hinweg konstant und plausibel bleiben. Regressionsprobe: ein vollständig und störungsfrei
empfangenes Signal muss vor und nach dem Fix identisch dekodiert werden (siehe
Host-Repro-Szenario 3) — der Fix darf ausschließlich unvollständige/kontaminierte Aufnahmen
zusätzlich ablehnen, keine vorher funktionierenden Fälle.

## Risiko

Rein additiv (eine neue Prüfung), keine Änderung der Bit-Extraktions- oder
Signal-Erkennungslogik, kein neuer Sende-Pfad. Reine Empfangs-/Anzeigefunktion für fremde
TPMS-Sensordaten (Reifendruck/Temperatur/Tire-ID/Unknown-Felder), kein Eingriff in
Ziel-Hardware — fällt wie der Rest von ProtoView unter reine passive Signalanalyse.

## Offen

Dieselbe Faktor-2-Vorab-Check-Schwäche in `protocols/tpms/schrader_eg53ma4.c` bleibt für künftige
Sitzungen offen, ebenso die noch nicht logikgeprüften `protocols/tpms/toyota.c`/
`protocols/tpms/schrader.c` (andere Sync-Strategie, noch nicht im Detail durchgesehen) und die
Bit-Edit-/Keyboard-Ansicht von ProtoView selbst.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05), Datei `protocols/tpms/renault.c`.
