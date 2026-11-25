# ProtoView — Toyota-TPMS-Dekoder: Längen-Vorab-Check mit veralteter Konstante

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): nach dem vollständig abgearbeiteten Vierer-Geschwisterkreis
Citroen/Ford/Renault/Schrader-EG53MA4 (identischer Faktor-2-Vorab-Check-Fehler in allen vieren)
nehme ich jetzt die bislang noch nicht logikgeprüfte `protocols/tpms/toyota.c` (92 Zeilen) vor —
laut eigenem Dateikommentar ein anderer Sync-Mechanismus (differenzielles Manchester statt
normales Manchester, mehrere alternative Sync-Muster statt eines einzigen).

`decode()` enthält auch hier einen Längen-Vorab-Check:

```c
if (numbits-6 < 64*2) return false; /* Ask for 64 bit of data (each bit
                                       is two symbols in the bitmap). */
```

Anders als bei den vier bereits behobenen Dateien ist hier der Faktor 2 bereits korrekt
angewendet (`64*2`, nicht `64`) — der Fehler liegt in der Basiskonstante selbst:

1. **Veraltete Konstante.** `64` entspricht dem Dateikopf-Kommentar ("Preamble + sync + 64 bits
   of data."), aber die tatsächliche Implementierung liest neun Bytes in `raw[9]` und verlangt
   drei Zeilen weiter unten `if (decoded < 8*9) return false;` — also 72 dekodierte Bits (9
   Bytes), nicht 64 (8 Bytes). Das Format wurde offenbar um ein neuntes Byte (ein CRC-Byte,
   geprüft über `crc8(raw,8,0x80,7) != raw[8]`) erweitert, ohne den Vorab-Check und den
   Dateikopf-Kommentar entsprechend anzupassen. Der tatsächliche Bedarf ist `8*9*2` (=144)
   Roh-Bitmap-Bits, nicht `64*2` (=128) — eine Lücke von 16 Rohbits (= genau 1 dekodiertes Byte),
   kleiner als bei den vier bereits behobenen Dateien (dort fehlte ein vollständiger Faktor 2),
   aber real und mit demselben strukturellen zweiten Mangel verbunden.
2. **Falscher Zeitpunkt.** Wie bei allen fünf bisher geprüften TPMS-Dateien läuft der Check,
   *bevor* die `for (j = 0; sync[j]; j++)`-Schleife `bitmap_seek_bits()` überhaupt aufruft, und
   sagt daher nichts über die nach einem spät gefundenen Sync-Treffer verbleibenden Bits aus.

**Unabhängige Zweitbestätigung innerhalb derselben Datei:** `info->pulses_count =
(off+8*9*2) - info->start_off;` (Zeile 76, nach dem Fix Zeile 84) verwendet bereits korrekt den
Faktor `8*9*2` für genau dieselbe Menge Rohbits — der Autor kannte den tatsächlichen Bedarf also
nachweislich, der Vorab-Check 24 Zeilen vorher wurde aber nicht aktualisiert, als das Format auf
9 Bytes erweitert wurde.

**Nebenbeobachtung (nicht separat behoben):** der Vorab-Check subtrahiert pauschal `6`, obwohl
der reale Sprung `off += strlen(sync[j])-2` für die beiden kürzeren Sync-Kandidaten
(`"00111100"`/`"00111101"`, je 8 Zeichen) `6`, für die beiden längeren
(`"001111100"`/`"001111101"`, je 9 Zeichen) aber `7` beträgt. Diese Ungenauigkeit bleibt
irrelevant, da der eigentliche Fix (Prüfung nach der Sync-Suche anhand des echten `off`) sie
automatisch mitbehebt, unabhängig davon, welcher der vier Kandidaten tatsächlich greift.

## Nutzen

Deckt einen bisher nicht dokumentierten, eigenständigen Fehler im sechsten und am Sync-Mechanismus
andersartigen TPMS-Decoder von ProtoView auf (differenzielles Manchester statt normales
Manchester, mehrere Sync-Kandidaten statt eines einzigen) — eine andere Fehler-Unterart derselben
Fehlerfamilie (veraltete Konstante statt fehlendem Faktor 2), was belegt, dass die Faktor-2-Lücke
in den vier Geschwisterdateien kein Einzelmuster war, sondern Teil eines umfassenderen,
wiederkehrenden Qualitätsproblems bei der Vorab-Längenprüfung über den gesamten TPMS-Cluster
hinweg. Ohne den Fix kann der Toyota-TPMS-Decoder ein als "erfolgreich" gemeldetes,
checksummenkorrektes Fahrzeugreifendruck-/Temperatur-Ergebnis anzeigen, bei dem im Extremfall
exakt das CRC-Byte — das einzige Feld, dessen Zweck es ist, Korruption zu erkennen — aus
Speicher jenseits des tatsächlich empfangenen Funksignals stammt.

## Betroffene Dateien

- `protocols/tpms/toyota.c` (`antirez/protoview`, Funktion `decode()`) — Fix dieser Sitzung.

## Fund im Detail

Wie bei allen bisher geprüften TPMS-Dateien reicht `decode_signal()` in `signal.c` an jeden
Decoder denselben per bloßem `malloc()` (ohne `memset`) angelegten, fest 4096 Byte großen
Scratch-Puffer durch. `convert_from_diff_manchester()` ist dabei an `numbytes` (die Byte-
*Kapazität* dieses Puffers) gebunden, nicht an `numbits` (die Anzahl tatsächlich abgetasteter
Bits) — liest also klaglos über das Ende des echten Empfangs hinaus, falls der aufrufende Decoder
das nicht selbst verhindert.

Konkretes Szenario (Host-Repro-Szenario 1): eine Aufnahme mit `numbits=134` (6 Bit
Sync-Rest-Sprung + 128 reale Nutzdaten-Rohbits, das entspricht exakt 8 der 9 Bytes). Der
bestehende Check prüft `134-6=128 < 128` → `false`, lässt die Dekodierung also durch, obwohl nach
dem Sync-Treffer nur 128 statt der tatsächlich benötigten 144 Rohbits real abgetastet wurden.
`convert_from_diff_manchester()` liest klaglos über `numbits` hinaus weiter und komplettiert das
fehlende 9. Byte (das CRC-Byte selbst) aus dem Scratch-Puffer-Rest — der dort zufällig vorhandene
Inhalt wird unverändert als CRC-Byte akzeptiert, sofern er (worst case) zufällig zum reinen
Zufallscheck passt. Host-Repro-Szenario 2 zeigt zusätzlich, wie bei den vier Geschwisterdateien,
dass ein naiver Fix, der nur die Konstante korrigiert (`64`→`72`), den Vorab-Check aber weiterhin
*vor* der Sync-Suche belässt, das Problem bei einem spät im Puffer liegenden Sync-Treffer nicht
vollständig behebt — dort ist sogar ein ganzes Byte plus ein Nachbarbyte (`raw[6]`/`raw[7]`, direkt
in die angezeigte Temperatur einfließend) kontaminierbar.

## Fix

Minimal-invasiv, eine neue Prüfung in `protocols/tpms/toyota.c`, keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik: nach `if (off == BITMAP_SEEK_NOT_FOUND) return
false;` (nachdem die tatsächliche Trefferposition bekannt ist) eine neue Prüfung einfügen:
`if (off + 8*9*2 > numbits) return false;` — korrekte Rohbit-Größenordnung (144 statt 128),
ausgewertet an der echten Sync-Position statt vorab unter der Annahme Offset 0. Der bestehende
Vorab-Check (`numbits-6 < 64*2`) bleibt unverändert als billiger Schnellausschluss vor der
Sync-Suche erhalten — er ist jetzt redundant-aber-harmlos, nicht mehr die einzige Absicherung.

Patch in `ideas/protoview-toyota-tpms-decoder-length-check-fix/toyota_c.patch`. Gegen zwei
frische, unabhängige Zweitkopien von `antirez/protoview`
(`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`) per `git apply --check`/`git apply` UND per
`patch -p1 --dry-run`/`patch -p1` verifiziert, Ergebnis laut `md5sum` in beiden Kopien und beiden
Prüfmethoden byte-identisch.

## Host-Repro

`ideas/repro-protoview-toyota-tpms-decoder-length-check-fix/repro.c`: alle von `decode()`
genutzten Bitmap-Primitiven (`bitmap_get`/`bitmap_set`/`bitmap_match_bits`/`bitmap_seek_bits`/
`convert_from_diff_manchester`) wörtlich aus `signal.c` übernommen, `crc8()` wörtlich aus `crc.c`,
`decode()`s Kernlogik in drei Varianten transkribiert: ALT (Original, fehlerhaft), MID (nur
Konstante korrigiert, `64`→`72`/`144` Rohbits, weiterhin vor der Sync-Suche ausgewertet) und NEU
(tatsächlicher Fix: nach der Sync-Suche, mit korrekter Konstante). Zusätzlich eine
differenziell-Manchester-Encoder-Hilfsfunktion (Umkehrung der Decodierregel aus
`convert_from_diff_manchester`), um kontrollierte Testaufnahmen zu bauen. Vier Szenarien:

- **Szenario 1** (Sync-Treffer bei Offset 0, Abbruch nach 8 von 9 realen Bytes — nur das
  CRC-Byte selbst fehlt real, worst case zufällig passend im Scratch-Speicher vorgefunden): ALT
  meldet fälschlich einen vollständigen, checksummenkorrekten Erfolg; MID und NEU lehnen hier
  beide korrekt ab — bei Offset 0 reicht eine reine Konstanten-Korrektur bereits aus, weil die
  Lücke hier nur 16 Rohbits beträgt.
- **Szenario 2** (Sync-Treffer erst bei Offset 250, Abbruch nach 6 von 9 realen Bytes, Rest mit
  `0xDE`/`0xAD`-Markerpaar in `raw[6]`/`raw[7]` plus passend berechnetem Checksummenbyte): ALT
  und **MID** melden beide fälschlich Erfolg mit demselben Marker — Beleg, dass eine reine
  Konstanten-Korrektur ohne Verschiebung des Checks hinter die Sync-Suche nicht ausreicht. Nur NEU
  lehnt korrekt ab.
- **Szenario 3** (vollständige, echte Aufnahme ohne jede Kontamination): ALT, MID und NEU
  dekodieren alle identisch und korrekt — Regressionsprobe, der Fix verändert das Verhalten für
  echte, vollständige Signale nicht.
- **Szenario 4** (grob zu kurze Aufnahme, nicht einmal das kürzeste 8-Bit-Sync-Muster passt
  hinein): alle drei Varianten lehnen korrekt ab.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: `19/19 Prüfungen bestanden`, `ALL CHECKS PASSED`, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden (zusätzlich zu den bereits
dokumentierten `furi_hal_subghz_load_preset()`-, B4B1-, KeeLoq-, Citroen-, Ford-, Renault- und
Schrader-EG53MA4-Fixes), dann `./fbt launch_app APPSRC=protoview` innerhalb des eigenen
`G4MEOVER-FW`-Baums. Funktionstest mit einem eigenen Toyota-TPMS-Reifendrucksensor (443,92 MHz
FSK, ~48µs Kurzpulslänge): "Tire ID"/"Pressure psi"/"Temperature C" müssen über mehrere Empfänge
desselben Sensors hinweg konstant und plausibel bleiben. Regressionsprobe: ein vollständig und
störungsfrei empfangenes Signal muss vor und nach dem Fix identisch dekodiert werden (siehe
Host-Repro-Szenario 3) — der Fix darf ausschließlich unvollständige/kontaminierte Aufnahmen
zusätzlich ablehnen, keine vorher funktionierenden Fälle.

## Risiko

Rein additiv (eine neue Prüfung), keine Änderung der Bit-Extraktions- oder
Signal-Erkennungslogik, kein neuer Sende-Pfad. Reine Empfangs-/Anzeigefunktion für fremde
TPMS-Sensordaten (Reifendruck/Temperatur/Tire-ID), kein Eingriff in Ziel-Hardware — fällt wie der
Rest von ProtoView unter reine passive Signalanalyse.

## Offen

Die noch nicht logikgeprüfte `protocols/tpms/schrader.c` (Basisvariante, andere
Sync-/Präambel-Strategie als die bisher sechs geprüften TPMS-Decoder) bleibt als letzte Datei im
TPMS-Cluster für künftige Sitzungen offen, ebenso die Bit-Edit-/Keyboard-Ansicht von ProtoView
selbst.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05), Datei `protocols/tpms/toyota.c`.
