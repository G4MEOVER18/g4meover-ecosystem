# ProtoView — Citroen-TPMS-Dekoder: Längen-Vorab-Check ist um Faktor 2 zu klein und prüft am falschen Zeitpunkt

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): nach Oregon2, B4B1 und KeeLoq ist der TPMS-Cluster
(Renault/Toyota/Schrader/Citroen/Ford) der letzte noch offene Rückstellpunkt unter den nativ
unterstützten Protokoll-Decodern. Diesen Durchlauf nehme ich den kleinsten der sechs
TPMS-Dateien (`protocols/tpms/citroen.c`, 64 Zeilen) zeilenweise vor.

`decode()` enthält einen Längen-Vorab-Check, der den Dekodiervorgang vorzeitig abbrechen soll,
wenn nicht genug Signal übrig ist:

```c
uint32_t sync_len = 17;
const char *sync_pattern = "10101010101010110";
if (numbits-sync_len < 8*10) return false; /* Expect 10 bytes. */
```

Dieser Check hat zwei sich verstärkende Mängel:

1. **Falsche Größenordnung.** `8*10` (=80) ist die Anzahl der *dekodierten* Datenbits (10 Bytes),
   nicht die Anzahl der tatsächlich konsumierten *Roh*-Bitmap-Bits. Die Nutzdaten sind
   Manchester-kodiert (`convert_from_line_code(..., "01","10")`), was pro dekodiertem Bit zwei
   Roh-Bits benötigt — der tatsächliche Bedarf ist `8*10*2` (=160), nicht 80. Das ist ein anderer
   Fehler als der bereits dokumentierte KeeLoq-Fund (dort hebt sich `-sync_len+sync_len`
   algebraisch auf und macht den Check komplett wirkungslos) — hier wird `sync_len` korrekt
   verwendet, nur der Nutzdaten-Term ist exakt um die Hälfte zu klein.
2. **Falscher Zeitpunkt.** Der Check läuft, *bevor* `bitmap_seek_bits()` die tatsächliche
   Sync-Position überhaupt kennt, und prüft daher nur das *gesamte* `numbits` unter der
   impliziten Annahme, der Sync-Treffer liege bei Offset 0. `bitmap_seek_bits(bits,numbytes,0,
   numbits,sync_pattern)` kann aber jede Position bis `numbits-sync_len` zurückgeben — über eine
   spätere Trefferposition sagt der Vorab-Check nichts aus.

**Unabhängige Zweitbestätigung innerhalb derselben Datei:** `info->pulses_count =
(off+8*10*2) - info->start_off;` (Zeile 44) verwendet bereits korrekt den Faktor `8*10*2` für
genau dieselbe Menge Rohbits — der Autor kannte den Faktor 2 also nachweislich, hat ihn aber im
Vorab-Check sechs Zeilen vorher nicht angewendet. Zusätzliche Gegenprobe im selben Quellbaum:
`protocols/tpms/ford.c` (`if (numbits-sync_len < 8*8) return false;` vs. eigene
`pulses_count`-Zeile `off+8*8*2`), `protocols/tpms/renault.c` (`if (numbits-12 < 9*8) return
false;` vs. `off+8*9*2`) und `protocols/tpms/schrader_eg53ma4.c` (`if (numbits-sync_len+8 <
8*10) return false;` vs. `off+10*8*2`) zeigen alle dasselbe Muster — derselbe
Faktor-2-Vorab-Check-Fehler in mindestens drei weiteren Dateien, jeweils mit derselben
Selbstwiderlegung über die eigene `pulses_count`-Zeile. Diese drei bleiben als eigener
Rückstellpunkt für künftige Sitzungen offen; diese Sitzung behebt ausschließlich
`citroen.c`.

## Nutzen

Deckt einen bisher nicht dokumentierten Fehler im letzten noch offenen Protokoll-Decoder-Cluster
von ProtoView auf und behebt ihn für die kleinste/einfachste der sechs TPMS-Dateien, reduziert
den offenen Rückstellpunkt entsprechend und liefert zugleich den Beleg (über drei unabhängige
Geschwisterdateien), dass es sich um ein wiederkehrendes Muster handelt statt um einen
Einzelfall — als Vorlage für die verbleibenden drei betroffenen Dateien. Ohne den Fix kann der
Citroen-TPMS-Decoder ein als "erfolgreich" gemeldetes, checksummenkorrektes Fahrzeugreifendruck-
/Temperatur-Ergebnis anzeigen, das teilweise aus Speicher jenseits des tatsächlich empfangenen
Funksignals stammt — irreführend für jede Auswertung eines echten Empfangs.

## Betroffene Dateien

- `protocols/tpms/citroen.c` (`antirez/protoview`, Funktion `decode()`) — Fix dieser Sitzung.
- Bestätigt, aber nicht in dieser Sitzung behoben: `protocols/tpms/ford.c`,
  `protocols/tpms/renault.c`, `protocols/tpms/schrader_eg53ma4.c` (identisches Muster, siehe
  oben).

## Fund im Detail

`decode_signal()` in `signal.c` reicht an jeden Decoder denselben per bloßem `malloc()` (ohne
`memset`) angelegten, fest 4096 Byte großen Scratch-Puffer durch (bereits im KeeLoq-Fund
dokumentiert). `convert_from_line_code()` ist dabei an `numbytes` (die Byte-*Kapazität* dieses
Puffers) gebunden, nicht an `numbits` (die Anzahl tatsächlich abgetasteter Bits) — liest also
klaglos über das Ende des echten Empfangs hinaus, falls der aufrufende Decoder das nicht selbst
verhindert.

Konkretes Szenario: eine Aufnahme mit `numbits=129` (17 Sync-Bits + 112 reale Nutzdaten-Rohbits,
das entspricht 7 der 10 Bytes). Der bestehende Check prüft `129-17=112 < 80` → `false`, lässt die
Dekodierung also durch, obwohl nach dem Sync nur 112 statt der tatsächlich benötigten 160 Rohbits
real abgetastet wurden. `convert_from_line_code()` liest klaglos über `numbits` hinaus weiter und
komplettiert die fehlenden 3 Bytes aus dem Scratch-Puffer-Rest — der dort zufällig vorhandene
Inhalt (z. B. Reste einer vorherigen Dekodierung) wird unverändert als "Temperature"/"Battery"
angezeigt, inklusive eines dafür nötigen, zufällig passenden XOR-Checksummenbytes aus derselben
Quelle. Kein konstruierter Sonderfall: jede Aufnahme, bei der nach dem Sync-Treffer weniger als
160 reale Rohbits folgen, aber die Gesamtlänge `numbits` noch über der (zu niedrigen)
Check-Schwelle liegt, ist betroffen — und das selbst dann, wenn der Sync-Treffer (wie im
Normalfall) direkt bei Offset 0 liegt.

Zusätzlich zeigt eine zweite Variante desselben Szenarios mit spätem Sync-Treffer (Offset 250
statt 0, bei insgesamt ausreichend langem `numbits=379`), dass ein naiver Fix, der nur den Faktor
korrigiert, den Vorab-Check aber weiterhin *vor* der Sync-Suche belässt, das Problem **nicht**
vollständig behebt: Er prüft weiterhin nur die Gesamtlänge, nicht die nach der tatsächlichen
(späten) Trefferposition verbleibenden Bits, und lässt daher dieselbe Kontamination ebenfalls
durch.

## Fix

Minimal-invasiv, zwei Einzelstellen in `protocols/tpms/citroen.c`, keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik:

1. **Haupt-Fund:** nach `off += sync_len;` (nachdem die tatsächliche Trefferposition bekannt
   ist) eine neue Prüfung einfügen: `if (off + 8*10*2 > numbits) return false;` — korrekte
   Rohbit-Größenordnung (160 statt 80), ausgewertet an der echten Sync-Position statt vorab unter
   der Annahme Offset 0. Der bestehende Vorab-Check (Zeile 20) bleibt unverändert als billiger
   Schnellausschluss vor der Sync-Suche erhalten — er ist jetzt redundant-aber-harmlos, nicht
   mehr die einzige Absicherung.
2. **Nebenfund (kosmetisch, gleiche Datei, gleicher Durchlauf):** `FURI_LOG_E(TAG, "Renault TPMS
   preamble+sync found");` → `"Citroen TPMS preamble+sync found"` — offensichtlicher
   Copy-Paste-Rest aus `renault.c` (dessen eigene, korrekte Log-Zeile denselben Wortlaut
   verwendet). Rein kosmetisch (reine Debug-Log-Ausgabe, keine Auswirkung auf Dekodierergebnis
   oder Anzeige), aber im selben Durchlauf behoben, da trivial und risikofrei.

Patch in `ideas/protoview-citroen-tpms-decoder-length-check-fix/citroen_c.patch`. Gegen zwei
frische, unabhängige Zweitkopien von `antirez/protoview` (`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`)
per `git apply --check`/`git apply` und separat per `patch -p1 --dry-run`/`patch -p1` verifiziert,
Ergebnisdatei in beiden Fällen laut `md5sum` byte-identisch.

## Host-Repro

`ideas/repro-protoview-citroen-tpms-decoder-length-check-fix/repro.c`: alle von `decode()`
genutzten Bitmap-Primitiven (`bitmap_get`/`bitmap_set`/`bitmap_match_bits`/`bitmap_seek_bits`/
`convert_from_line_code`) wörtlich aus `signal.c` übernommen, `decode()`s Kernlogik in drei
Varianten transkribiert: ALT (Original, fehlerhaft), MID (nur Faktor korrigiert, weiterhin vor
der Sync-Suche ausgewertet) und NEU (tatsächlicher Fix: nach der Sync-Suche, mit korrektem
Faktor). Vier Szenarien:

- **Szenario 1** (Sync-Treffer bei Offset 0, Abbruch nach 7 von 10 realen Bytes, Rest aus
  Scratch-Speicher mit einem eindeutig fremden `0xDE`/`0xAD`-Markerpaar plus passend berechnetem
  Checksummenbyte): ALT meldet fälschlich einen vollständigen, checksummenkorrekten Erfolg,
  `raw[7]`/`raw[8]` sind exakt der Marker (Beweis: diese Bytes stammen nicht aus dem echten
  Signal). MID und NEU lehnen hier beide korrekt ab — bei Offset 0 reicht eine reine
  Faktor-Korrektur bereits aus.
- **Szenario 2** (identischer 7-von-10-Bytes-Schnitt, aber Sync-Treffer erst bei Offset 250, bei
  insgesamt ausreichend langem `numbits`): ALT und **MID** melden beide fälschlich Erfolg mit
  demselben Marker in `raw[7]`/`raw[8]` — Beleg, dass eine reine Faktor-Korrektur ohne
  Verschiebung des Checks hinter die Sync-Suche nicht ausreicht. Nur NEU lehnt korrekt ab.
- **Szenario 3** (vollständige, echte Aufnahme ohne jede Kontamination): ALT, MID und NEU
  dekodieren alle identisch und korrekt — Regressionsprobe, der Fix verändert das
  Verhalten für echte, vollständige Signale nicht.
- **Szenario 4** (grob zu kurze Aufnahme, nicht einmal das 17-Bit-Sync-Muster passt
  hinein): alle drei Varianten lehnen korrekt ab.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: `20/20 Prüfungen bestanden`, `ALL CHECKS PASSED`, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden (zusätzlich zu den bereits
dokumentierten `furi_hal_subghz_load_preset()`-, B4B1- und KeeLoq-Fixes), dann `./fbt launch_app
APPSRC=protoview` innerhalb des eigenen `G4MEOVER-FW`-Baums. Funktionstest: eigenen
Citroen-TPMS-Reifendrucksensor (433,92 MHz FSK) empfangen, "Tire ID"/"Pressure kpa"/
"Temperature C"/"Battery"/"Repeat" müssen über mehrere Empfänge desselben Sensors hinweg
konstant und plausibel bleiben. Regressionsprobe: ein vollständig und störungsfrei empfangenes
Signal muss vor und nach dem Fix identisch dekodiert werden (siehe Host-Repro-Szenario 3) — der
Fix darf ausschließlich unvollständige/kontaminierte Aufnahmen zusätzlich ablehnen, keine
vorher funktionierenden Fälle.

## Risiko

Rein additiv/korrigierend (eine neue Prüfung, eine Log-String-Korrektur), keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik, kein neuer Sende-Pfad. Reine Empfangs-/
Anzeigefunktion für fremde TPMS-Sensordaten (Reifendruck/Temperatur/Batterie/Tire-ID), kein
Eingriff in Ziel-Hardware — fällt wie der Rest von ProtoView unter reine passive Signalanalyse.

## Offen

Dieselbe Faktor-2-Vorab-Check-Schwäche in `protocols/tpms/ford.c`, `protocols/tpms/renault.c`
und `protocols/tpms/schrader_eg53ma4.c` (siehe "Fund im Detail") bleibt für künftige Sitzungen
offen, ebenso die noch nicht logikgeprüften `protocols/tpms/toyota.c`/`protocols/tpms/schrader.c`
(andere Sync-Strategie, noch nicht im Detail durchgesehen) und die Bit-Edit-/Keyboard-Ansicht
von ProtoView selbst.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05), Datei `protocols/tpms/citroen.c`.
