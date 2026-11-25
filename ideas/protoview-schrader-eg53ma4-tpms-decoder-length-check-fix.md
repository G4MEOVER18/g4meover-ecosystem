# ProtoView — Schrader-EG53MA4-TPMS-Dekoder: Längen-Vorab-Check ist um Faktor 2 zu klein und wird am falschen Zeitpunkt ausgewertet

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): von den drei in der Citroen-Sitzung vorab identifizierten
Geschwisterdateien mit demselben Fehlermuster (Ford, Renault bereits behoben) nehme ich jetzt die
letzte, `protocols/tpms/schrader_eg53ma4.c` (67 Zeilen), vor — bislang nur anhand des
Autoren-Selbstwiderspruchs in `pulses_count` zitiert, aber noch nicht eigenständig zeilenweise
geprüft und gefixt.

`decode()` enthält denselben Längen-Vorab-Check wie Citroen/Ford/Renault, nur mit anderen
Konstanten:

```c
const char *sync_pattern = "010101010101" "01100101";
uint8_t sync_len = 12+8; /* We just use 12 preamble symbols + sync. */
if (numbits-sync_len+8 < 8*10) return false;
```

Dieser Check hat dieselben zwei sich verstärkenden Mängel wie bei Citroen/Ford/Renault:

1. **Falsche Größenordnung.** `8*10` (=80) ist die Anzahl der *dekodierten* Datenbits (10 Bytes),
   nicht die Anzahl der tatsächlich konsumierten *Roh*-Bitmap-Bits. Die Nutzdaten sind
   Manchester-kodiert (`convert_from_line_code(..., "01","10")`), was pro dekodiertem Bit zwei
   Roh-Bits benötigt — der tatsächliche Bedarf ist `10*8*2` (=160), nicht 80.
2. **Falscher Zeitpunkt.** Der Check läuft, *bevor* `bitmap_seek_bits()` die tatsächliche
   Sync-Position überhaupt kennt, und prüft daher nur das *gesamte* `numbits` unter der
   impliziten Annahme, der Sync-Treffer liege bei Offset 0. `bitmap_seek_bits(bits,numbytes,0,
   numbits,sync_pattern)` kann aber jede Position bis `numbits-sync_len` zurückgeben — über eine
   spätere Trefferposition sagt der Vorab-Check nichts aus.

Anders als bei Renault ist die hier subtrahierte Konstante (`sync_len-8` = 12, äquivalent zur
Schreibweise `-sync_len+8`) tatsächlich **konsistent** mit der zwei Zeilen später real
verwendeten Sprungweite (`off += sync_len-8; /* Skip preamble, not sync that is part of the
data. */`) — diese Datei hat also nur die beiden oben genannten Mängel, nicht Renaults
eigenständigen dritten (inkonsistente Konstante).

**Unabhängige Zweitbestätigung innerhalb derselben Datei:** `info->pulses_count =
(off+10*8*2) - info->start_off;` (Zeile 47) verwendet bereits korrekt den Faktor `10*8*2` für
genau dieselbe Menge Rohbits — derselbe Autoren-Selbstwiderspruch wie bei Citroen (`8*10*2`),
Ford (`8*8*2`) und Renault (`8*9*2`) — damit sind jetzt alle vier der ursprünglich in der
Citroen-Sitzung identifizierten Geschwisterdateien eigenständig geprüft und gefixt.

## Nutzen

Schließt den TPMS-Vorab-Check-Rückstellpunkt für die vierte und letzte der vier in der
Citroen-Sitzung identifizierten Geschwisterdateien vollständig ab. Ohne den Fix kann der
Schrader-EG53MA4-TPMS-Decoder ein als "erfolgreich" gemeldetes, Checksummen-korrektes
Fahrzeugreifendruck-/Temperatur-Ergebnis anzeigen, das teilweise aus Speicher jenseits des
tatsächlich empfangenen Funksignals stammt — irreführend für jede Auswertung eines echten
Empfangs. Konkret betroffen wären die beiden angezeigten Messwerte "Pressure kpa" und
"Temperature C" (siehe Host-Repro unten); die Tire-ID bleibt in meinem Testszenario unabhängig
davon echt, da sie in den Bytes vor der Kontamination liegt — das ist aber Zufall der jeweiligen
Abschneide-Position, nicht Eigenschaft des Fixes.

## Betroffene Dateien

- `protocols/tpms/schrader_eg53ma4.c` (`antirez/protoview`, Funktion `decode()`) — Fix dieser
  Sitzung.
- Bereits behoben (vorherige Sitzungen): `protocols/tpms/citroen.c`, `protocols/tpms/ford.c`,
  `protocols/tpms/renault.c`. Damit ist der in der Citroen-Sitzung identifizierte
  Vierer-Geschwisterkreis vollständig abgearbeitet.
- Weiterhin offen, anderer Sync-Mechanismus, noch nicht im Detail durchgesehen:
  `protocols/tpms/toyota.c`, `protocols/tpms/schrader.c` (Basis-Schrader, nicht EG53MA4).

## Fund im Detail

`decode_signal()` in `signal.c` reicht an jeden Decoder denselben per bloßem `malloc()` (ohne
`memset`) angelegten, fest 4096 Byte großen Scratch-Puffer durch (bereits im KeeLoq-, Citroen-,
Ford- und Renault-Fund dokumentiert). `convert_from_line_code()` ist dabei an `numbytes` (die
Byte-*Kapazität* dieses Puffers) gebunden, nicht an `numbits` (die Anzahl tatsächlich
abgetasteter Bits) — liest also klaglos über das Ende des echten Empfangs hinaus, falls der
aufrufende Decoder das nicht selbst verhindert. Der nachgelagerte Check `if (decoded < 10*8)
return false;` schützt davor nicht: `decoded` zählt nur, wie viele Bits `convert_from_line_code()`
erfolgreich als "01"/"10" erkannt hat, unabhängig davon, ob diese Bits aus dem echten Empfang oder
aus dem Scratch-Speicherrest stammen — solange dort zufällig gültige Manchester-Muster liegen (wie
im hier konstruierten Repro), erreicht `decoded` trotzdem die volle Soll-Zahl.

Konkretes Szenario: eine Aufnahme mit `numbits=124` (20 Sync-Bits, davon 8 bereits Teil der
Nutzdaten, plus 112 reale Nutzdaten-Rohbits, das entspricht 7 der 10 Bytes). Der bestehende Check
prüft `124-20+8=112 < 80` → `false` ist falsch (`112` ist nicht kleiner als `80`), lässt die
Dekodierung also durch, obwohl nach dem Sync nur 112 statt der tatsächlich benötigten 160 Rohbits
real abgetastet wurden. `convert_from_line_code()` liest klaglos über `numbits` hinaus weiter und
komplettiert die fehlenden 3 Bytes aus dem Scratch-Puffer-Rest — der dort zufällig vorhandene
Inhalt wird unverändert als "Pressure kpa"/"Temperature C" angezeigt, inklusive eines dafür nötigen,
zufällig passenden additiven Checksummen-Bytes aus derselben Quelle. Kein konstruierter
Sonderfall: jede Aufnahme, bei der nach dem Sync-Treffer weniger als 160 reale Rohbits folgen, aber
die Gesamtlänge `numbits` noch über der (zu niedrigen) Check-Schwelle liegt, ist betroffen — und
das selbst dann, wenn der Sync-Treffer (wie im Normalfall) direkt bei Offset 0 liegt.

Zusätzlich zeigt eine zweite Variante desselben Szenarios mit spätem Sync-Treffer (Offset 250
statt 0, bei insgesamt ausreichend langem `numbits=374`), dass ein naiver Fix, der nur den Faktor
korrigiert, den Vorab-Check aber weiterhin *vor* der Sync-Suche belässt, das Problem **nicht**
vollständig behebt: Er prüft weiterhin nur die Gesamtlänge, nicht die nach der tatsächlichen
(späten) Trefferposition verbleibenden Bits, und lässt daher dieselbe Kontamination ebenfalls
durch — identisch zum bereits bei Citroen/Ford/Renault gezeigten Muster.

## Fix

Minimal-invasiv, eine Einzelstelle in `protocols/tpms/schrader_eg53ma4.c`, keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik: nach `off += sync_len-8; /* Skip preamble, not sync
that is part of the data. */` (nachdem die tatsächliche Trefferposition bekannt ist) eine neue
Prüfung einfügen: `if (off + 10*8*2 > numbits) return false;` — korrekte Rohbit-Größenordnung
(160 statt 80), ausgewertet an der echten Sync-Position statt vorab unter der Annahme Offset 0.
Der bestehende Vorab-Check (Zeile 25) bleibt unverändert als billiger Schnellausschluss vor der
Sync-Suche erhalten — er ist jetzt redundant-aber-harmlos, nicht mehr die einzige Absicherung.

Patch in
`ideas/protoview-schrader-eg53ma4-tpms-decoder-length-check-fix/schrader_eg53ma4_c.patch`. Gegen
zwei frische, unabhängige Zweitkopien von `antirez/protoview`
(`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`) per `git apply --check`/`git apply` und separat per
`patch -p1 --dry-run`/`patch -p1` verifiziert, Ergebnisdatei in beiden Fällen laut `md5sum`
byte-identisch.

## Host-Repro

`ideas/repro-protoview-schrader-eg53ma4-tpms-decoder-length-check-fix/repro.c`: alle von
`decode()` genutzten Bitmap-Primitiven (`bitmap_get`/`bitmap_set`/`bitmap_match_bits`/
`bitmap_seek_bits`/`convert_from_line_code`) wörtlich aus `signal.c` übernommen, `decode()`s
Kernlogik in drei Varianten transkribiert: ALT (Original, fehlerhaft), MID (nur Faktor korrigiert,
weiterhin vor der Sync-Suche ausgewertet) und NEU (tatsächlicher Fix: nach der Sync-Suche, mit
korrektem Faktor). Wie Ford verwendet Schrader-EG53MA4 eine additive Summe (hier der ersten 9
Bytes) modulo 256 statt einer echten CRC — die Hybrid-/Kontaminations-Konstruktion ist entsprechend
aufgebaut, diesmal gezielt auf die beiden angezeigten Messwert-Bytes (`raw[7]` Pressure, `raw[8]`
Temperature) statt auf Flags/Moving wie bei Ford. Vier Szenarien:

- **Szenario 1** (Sync-Treffer bei Offset 0, Abbruch nach 7 von 10 realen Bytes, Rest aus
  Scratch-Speicher mit einem eindeutig fremden `0xDE`/`0xAD`-Markerpaar plus passend berechnetem
  Checksummen-Byte): ALT meldet fälschlich einen vollständigen, Checksummen-korrekten Erfolg,
  `raw[7]`/`raw[8]` sind exakt der Marker (Beweis: diese Bytes stammen nicht aus dem echten
  Signal), während `raw[4..6]` (Tire ID) unverändert aus der echten Nachricht stammen. MID und NEU
  lehnen hier beide korrekt ab — bei Offset 0 reicht eine reine Faktor-Korrektur bereits aus.
- **Szenario 2** (identischer 7-von-10-Bytes-Schnitt, aber Sync-Treffer erst bei Offset 250, bei
  insgesamt ausreichend langem `numbits`): ALT und **MID** melden beide fälschlich Erfolg mit
  demselben Marker in `raw[7]`/`raw[8]` — Beleg, dass eine reine Faktor-Korrektur ohne
  Verschiebung des Checks hinter die Sync-Suche nicht ausreicht. Nur NEU lehnt korrekt ab.
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
dokumentierten `furi_hal_subghz_load_preset()`-, B4B1-, KeeLoq-, Citroen-, Ford- und
Renault-Fixes), dann `./fbt launch_app APPSRC=protoview` innerhalb des eigenen
`G4MEOVER-FW`-Baums bauen. Funktionstest: eigenen Schrader-EG53MA4-TPMS-Reifendrucksensor
(443,92 MHz OOK, 100µs Pulslänge, z. B. in bestimmten Open-Fahrzeugen verbaut) empfangen, "Tire
ID"/"Pressure kpa"/"Temperature C" müssen über mehrere Empfänge desselben Sensors hinweg konstant
und plausibel bleiben. Regressionsprobe: ein vollständig und störungsfrei empfangenes Signal muss
vor und nach dem Fix identisch dekodiert werden (siehe Host-Repro-Szenario 3) — der Fix darf
ausschließlich unvollständige/kontaminierte Aufnahmen zusätzlich ablehnen, keine vorher
funktionierenden Fälle.

## Risiko

Rein additiv (eine neue Prüfung), keine Änderung der Bit-Extraktions- oder
Signal-Erkennungslogik, kein neuer Sende-Pfad. Reine Empfangs-/Anzeigefunktion für fremde
TPMS-Sensordaten (Reifendruck/Temperatur), kein Eingriff in Ziel-Hardware — fällt wie der Rest
von ProtoView unter reine passive Signalanalyse.

## Offen

Mit diesem Fix ist der in der Citroen-Sitzung identifizierte Vierer-Geschwisterkreis
(Citroen/Ford/Renault/Schrader-EG53MA4) vollständig abgearbeitet. Weiterhin offen: die noch nicht
logikgeprüften `protocols/tpms/toyota.c`/`protocols/tpms/schrader.c` (andere Sync-Strategie,
Basis-Schrader statt EG53MA4-Variante) sowie die Bit-Edit-/Keyboard-Ansicht von ProtoView selbst.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05), Datei `protocols/tpms/schrader_eg53ma4.c`.
