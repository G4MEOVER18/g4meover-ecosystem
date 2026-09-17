# NFC FeliCa Lite: dateikontrollierter Heap-Überlauf beim Laden über „Blocks total" — Unleashed vs. mntm-012

Quelle: Unleashed-Firmware-Commit `51a56fc4015299bf071a360633968ca489e455ee` ("NFC: reject
oversized FeliCa Lite dumps", PR #1106, gemerged 26.08.2026), letzter noch offener Kandidat aus der
zuvor vorgemerkten `unlshd-093`-Restliste (neben Stealth-Jiggler-Bounds #1111,
CUID-Dictionary-Zähler #1115, MIFARE-Classic-Schlüssel-Dictionary-Feature #1118,
Superrollo/GW60-SubGHz-Protokoll #1068, die weiterhin offen bleiben). Gegen Tag `mntm-012` geprüft:
`lib/nfc/protocols/felica/felica.c` unterscheidet sich vom hier zitierten Vorher-Stand nur um
`felica_data_format_version` (2 statt 3, bei mir fehlt die spätere Schlüsselversions-Erweiterung —
irrelevant für diesen Fund) — die betroffene `FelicaLite`-Ladeschleife selbst ist byteidentisch.
Der Fund ist bei mir vollständig vorhanden.

Wie beim bereits portierten EMV-`.nfc`-Loader-Fund
(`firmware-mining/emv-nfc-file-loader-length-bounds-fix.md`) ist das Bedrohungsmodell hier die
Datei, nicht die Karte: `felica_load()` wird beim Öffnen eines gespeicherten `.nfc`-Datensatzes aus
dem Dateisystem aufgerufen. Eine präparierte `.nfc`-Datei überschreibt Speicher, sobald sie geladen
wird — kein Kartenkontakt nötig.

## Was

`felica_load()` (`lib/nfc/protocols/felica/felica.c`), Zweig `FelicaLite`, liest zwei Werte direkt
aus der `.nfc`-Datei und verwendet sie ungeprüft als Schreiblänge in die heap-allokierte
`FelicaData` (`malloc(sizeof(FelicaData))` in `felica_alloc()`):

```c
// lib/nfc/protocols/felica/felica.c, felica_load(), Fall FelicaLite -- bei mir (mntm-012) so:
uint32_t blocks_total = 0;
uint32_t blocks_read = 0;
if(!flipper_format_read_uint32(ff, "Blocks total", &blocks_total, 1)) break;
if(!flipper_format_read_uint32(ff, "Blocks read", &blocks_read, 1)) break;
data->blocks_total = (uint8_t)blocks_total; // ungeprüfter uint32_t -> uint8_t-Cast
data->blocks_read = (uint8_t)blocks_read;

for(uint8_t i = 0; i < data->blocks_total; i++) {
    furi_string_printf(str_data_buffer, "Block %d", i);
    if(!flipper_format_read_hex(
           ff,
           furi_string_get_cstr(str_data_buffer),
           (&data->data.dump[i * sizeof(FelicaBlock)]), // Ziel: FelicaFSUnion.dump[504]
           sizeof(FelicaBlock))) {                       // 18 Byte je Iteration
        break;
    }
}
```

`data->data` ist eine `FelicaFSUnion` (`FelicaFileSystem fs` bzw. `uint8_t dump[sizeof(FelicaFileSystem)]`)
mit genau `FELICA_BLOCKS_TOTAL_COUNT` (28) Blöcken zu je `sizeof(FelicaBlock)` (18 Byte: `SF1`+`SF2`+
16 Byte Nutzdaten) — 504 Byte, eingebettet in `FelicaData` zwischen `blocks_read` und den
nachfolgenden Feldern `SimpleArray* systems`/`FelicaWorkflowType workflow_type` (gesamte Struct:
544 Byte in meiner Basis, per `malloc()` alloziert).

Der Datei-Wert `blocks_total` kommt als `uint32_t` ungeprüft aus `flipper_format_read_uint32()`.
Nach dem Cast auf `uint8_t` ist der wirksame Maximalwert 255 — bereits das schreibt in der
Schleife 255 × 18 = 4590 Byte in ein 504 Byte großes Feld, weit über das Ende der *gesamten*
`FelicaData`-Allokation hinaus (Heap-Speicherüberschreibung, nicht nur ein Übergriff auf
Nachbarfelder innerhalb derselben Struct). Zusätzlich fehlt die Prüfung `blocks_read <=
blocks_total`, die die eigentlich beabsichtigte Invariante ("nicht mehr Blöcke als gelesen
markiert, als insgesamt vorhanden sind") absichert — `blocks_read` selbst wird bei mir nur zur
Anzeige verwendet (`felica_render.c`, "Blocks Read: %u/%u"), ist also für sich genommen kein
zweiter Speicherfehler, aber Teil der von Unleashed korrigierten Dateninvariante.

## Nutzen

- Schließt eine Heap-Speicherüberschreibung, die allein durch das **Öffnen** einer präparierten
  `.nfc`-Datei mit `Device type: FeliCa` und `Data format version: 1` (FeliCa-Lite-Zweig) ausgelöst
  wird — kein NFC-Lesevorgang, kein Kartenkontakt nötig. Gleiches Bedrohungsmodell wie beim bereits
  portierten EMV-Loader-Fund: relevant, weil ich Captures zwischen eigenen Geräten austausche und
  testweise Dateien aus fremden Quellen (CTF-Material, Pentest-Testsets) öffne.
- Verhält sich für jede regulär von mir selbst gespeicherte FeliCa-Lite-Datei identisch zu vorher
  (siehe Testhinweis): `felica_save()` schreibt ohnehin nie mehr als 28 Blöcke, da `blocks_total`
  dort aus dem tatsächlich befüllten `data->blocks_total`-Feld stammt.
- Kleiner, klar abgegrenzter Fund direkt im Anschluss an den bereits abgeschlossenen
  Expansion-Callback-Sentinel-Fix (letzte Sitzung) — passt zur bisherigen `unlshd-093`-Reihe.

## Betroffene Dateien (Portumfang)

- `lib/nfc/protocols/felica/felica.c`
  - `felica_load()`, Zweig `FelicaLite`: nach dem Lesen von `"Blocks total"`/`"Blocks read"` und vor
    der Zuweisung an `data->blocks_total`/`data->blocks_read` eine Grenzprüfung einfügen
    (`blocks_total > FELICA_BLOCKS_TOTAL_COUNT || blocks_read > blocks_total` → `parsed = false;
    break;`, exakt das bestehende Fehlerpfad-Muster der Funktion, `felica_load()` gibt am Ende
    `return parsed;` zurück).

Keine Änderung an `felica.h` nötig — `FELICA_BLOCKS_TOTAL_COUNT` ist dort bereits definiert (28),
nur die Prüflogik beim Laden fehlt.

## Risiko

- Minimalster mögliche Umfang: eine Implementierungsdatei, vier neue Zeilen in einem bereits
  vorhandenen `do {} while(false)`-Fehlerpfad — keine Struktur-/API-Änderung, keine neuen Felder,
  kein neues Symbol.
- Verhaltensänderung im Fehlerfall: eine `.nfc`-Datei mit `Blocks total` > 28 oder `Blocks read` >
  `Blocks total` wird beim Laden jetzt vollständig abgelehnt (`felica_load()` gibt `false` zurück),
  statt Speicher zu überschreiben. Für alle von mir selbst über `felica_save()` erzeugten Dateien
  ändert sich nichts, da `blocks_total` dort bereits im gültigen Bereich (≤ 28) liegt — nur eine
  manuell manipulierte oder aus einer fremden Quelle stammende Datei kann den abgelehnten Fall
  überhaupt auslösen.
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-Abweichung als Compile-Fehler auf. `FELICA_BLOCKS_TOTAL_COUNT`
  ist bereits im selben Header definiert und an anderer Stelle in `felica.c`/`felica_poller.c`
  benutzt — keine neue Abhängigkeit.

## Port-Skizze

1. `lib/nfc/protocols/felica/felica.c` gegen meine mntm-012-Basis gegenlesen — bei der Prüfung für
   diesen Vorschlag bis auf die unabhängige `felica_data_format_version`-Differenz byteidentisch
   zum zitierten Vorher-Stand.
2. Patch `felica-lite-blocks-total-oob-fix/felica_c.patch` anwenden (`patch -p1 <
   felica-lite-blocks-total-oob-fix/felica_c.patch` aus dem Firmware-Wurzelverzeichnis).
3. Host-Repro (`repro.c`, unten) mit `gcc -std=c11 -O1 -g -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen — bereits von mir lokal ausgeführt, siehe
   Testhinweis.
4. Build: `ufbt` (bzw. `fbt firmware_all`).
5. Funktionstest lokal: ein vorhandenes, mit meinem eigenen Gerät gespeichertes FeliCa-Lite-`.nfc`-
   Capture erneut öffnen (Ergebnis unverändert: alle Blöcke wie vorher, kein abgelehntes Laden).
   Danach eine Kopie der Datei per Texteditor manipulieren (`Blocks total` auf z. B. `255` setzen,
   `Blocks read` beibehalten oder ebenfalls erhöhen) und versuchen zu laden — vor dem Fix bricht das
   reale Verhalten undefiniert ab (Heap-Speicherüberschreibung, potenziell Absturz/Fehlverhalten),
   nach dem Fix wird der Ladevorgang sauber mit `false` abgelehnt (NFC-App zeigt "Failed to
   load file" o. ä., kein Absturz).

## Testhinweis (lokal, ohne Hardware — reine Datei-/Speicherlogik)

Host-kompilierbares Mini-Repro
(`repro-felica-lite-blocks-total-oob-write/repro.c`) bildet die relevanten `FelicaData`-Felder
(Reihenfolge/Größen wie in `felica.h` bei mir) heap-alloziert wie in `felica_alloc()` nach. ALT ist
1:1 die bei mir vorhandene Logik, NEU die gebundene Fassung aus Commit `51a56fc4`. Jeder Testfall
läuft isoliert in einem `fork()`-Kindprozess (ein ASan-Abbruch eines Falls darf den Testlauf selbst
nicht beenden; da ASan auf dieser Maschine standardmäßig ohne `abort_on_error=1` per `exit(1)`
statt per Signal beendet, zählt die Erkennung sowohl Signal als auch einen von Null verschiedenen
Exitcode als erkannten Fund — beide Varianten habe ich direkt gegengeprüft, siehe Kommentar im
Repro). Gebaut/ausgeführt mit `gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined`:

```
sizeof(FelicaBlock) = 18, sizeof(dump) = 504, sizeof(FelicaData) = 544

== Regressionsfall: reale FeliCa-Lite-Karte (28 Bloecke, Maximum laut Protokoll) ==
  ALT, blocks_total=28: kein Absturz
  NEU, blocks_total=28: kein Absturz

== Angriffsfall: dateikontrollierter Ueberlaengen-Wert (255, Maximum nach uint8_t-Cast) ==
  ALT, blocks_total=255 (Datei-Wert, Ziel nur 504 Byte): ABSTURZ (von ASan/Signal erkannt)
  NEU, blocks_total=255 (Datei-Wert, Ziel nur 504 Byte): kein Absturz

OK: ALT ueberschreibt bei blocks_total=255 den gesamten heap-allokierten FelicaData-
Block weit ueber sein Ende hinaus (4590 Byte Schreibversuch in ein < 600 Byte grosses
Ziel), NEU (Grenzpruefung aus Commit 51a56fc4) lehnt den Ladevorgang vorher ab. Fuer den
protokollkonformen Regressionsfall (28 Bloecke) verhalten sich ALT und NEU identisch.
```

Repräsentativer ASan-Bericht für den Angriffsfall (separat mit vollem `stderr`, ohne
Fork-Isolation, gegengeprüft — bestätigt, dass der Schreibzugriff exakt am Ende der gesamten
544-Byte-`FelicaData`-Allokation liegt, nicht nur auf ein Nachbarfeld innerhalb der Struct
übergreift):

```
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x...
WRITE of size 1 at 0x... thread T0
0x... is located 0 bytes after 544-byte region [0x...,0x...) allocated by thread T0 here:
    #0 ... in malloc ...
```

Bestätigt: die ALT-Fassung überschreibt bei dateikontrolliertem `Blocks total` = 255 den
heap-allokierten `FelicaData`-Block über sein Ende hinaus; die NEU-Fassung lehnt den Ladevorgang
kontrolliert ab, bevor irgendetwas geschrieben wird. Für den protokollkonformen Regressionsfall
(28 Blöcke, das protokollbedingte Maximum) verhalten sich beide Fassungen identisch — kein
Funktionsverlust im Regelfall.

## Quelle

- `DarkFlippers/unleashed-firmware`, Commit `51a56fc4015299bf071a360633968ca489e455ee` (PR #1106,
  gemerged 26.08.2026): „NFC: reject oversized FeliCa Lite dumps" —
  `lib/nfc/protocols/felica/felica.c` (+ Unit-Test `felica_oversized_lite_file_test`, hier nicht
  mitportiert, da mein Debug-Unit-Test-Baum von diesem gegen `mntm-012` geprüften Ausschnitt
  unabhängig ist).
- Momentum-Firmware, Tag `mntm-012`: `lib/nfc/protocols/felica/felica.c` als Vergleichsbasis
  (bis auf die unabhängige `felica_data_format_version`-Differenz identisch zum zitierten
  Vorher-Stand).
- Damit ist die zuletzt vorgemerkte `unlshd-093`-Restliste um einen weiteren Punkt reduziert;
  offen bleiben Stealth-Jiggler-Bounds (#1111), CUID-Dictionary-Zähler (#1115),
  MIFARE-Classic-Schlüssel-Dictionary-Feature (#1118), Superrollo/GW60-SubGHz-Protokoll (#1068).
