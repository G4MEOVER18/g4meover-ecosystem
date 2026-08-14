# Infrared: uninitialisiertes `timings_cnt` bei Decoder-Alloc + falsche Bounds-Prüfung — Official-dev vs. mntm-012

## Was
Ich habe den aus der 2026-08-14-00:34-Sitzung vorgemerkten Kandidaten `5dc71870` (gleicher
`dev`-Durchlauf wie die bereits portierten PAC/Stanley-, ISO15693-, `bit_lib`-, Pyramid- und
FeliCa-Fixes) aufgegriffen: PR #4429 (@munzzyy, gemerged 27.07.2026) behebt zwei zusammengehörige
Bugs im gemeinsamen Infrarot-Decoder (`lib/infrared/encoder_decoder/common/infrared_common_decoder.c`),
der bei mir in der mntm-012-Basis unverändert vorhanden ist (Datei UND das relevante
`InfraredCommonDecoder`-Struct-Layout in `infrared_common_i.h` gegen Tag `mntm-012` gegengeprüft
— identisch zum hier zitierten Vorher-Stand).

**Teilbug 1 — `timings_cnt` bei Alloc nicht initialisiert:**
```c
// mntm-012 (fehlerhaft):
void* infrared_common_decoder_alloc(const InfraredCommonProtocolSpec* protocol) {
    ...
    InfraredCommonDecoder* decoder = malloc(alloc_size);
    decoder->protocol = protocol;
    decoder->level = true;
    return decoder;                       // <- timings_cnt bleibt Heap-Muell
}
```
`timings_cnt` (ein `uint8_t`-Feld) wird nirgends im Alloc-Pfad gesetzt. Der erste `decode()`-Aufruf
nach dem Alloc löst normalerweise sofort ein `infrared_common_decoder_reset(decoder)` aus
(`decoder->level` wurde in Alloc auf `true` gesetzt; trifft der erste gemeldete Flankenpegel
ebenfalls `true`, greift die Reset-Bedingung `if(decoder->level == level)`). Genau in diesem
allerersten Reset liegt die eigentliche Falle: `infrared_common_decoder_reset()` ruft zuerst
`infrared_common_decoder_reset_state()` auf und setzt `timings_cnt` **erst danach** explizit auf 0:
```c
// mntm-012, infrared_common_decoder_reset_state() (Auszug):
if(decoder->protocol->timings.preamble_mark == 0) {
    if(decoder->timings_cnt > 0) {
        decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 1);
    }
}
// ...
void infrared_common_decoder_reset(InfraredCommonDecoder* decoder) {
    infrared_common_decoder_reset_state(decoder);
    decoder->timings_cnt = 0;             // <- zu spaet, s.o.
}
```
`preamble_mark == 0` betrifft in meiner Codebasis ausschließlich **RC5**
(`lib/infrared/encoder_decoder/rc5/infrared_protocol_rc5.c`, einziges Protokoll mit
`.preamble_mark = 0`; NEC/RCA/Kaseikyo/Samsung/RC6/SIRC/Pioneer haben alle einen echten
`preamble_mark`-Wert). Bei RC5 liest `reset_state()` das noch nicht zurückgesetzte,
uninitialisierte `timings_cnt` und reicht es als Länge an `consume_samples()` weiter:
```c
static inline size_t consume_samples(uint32_t* array, size_t len, size_t shift) {
    furi_assert(len >= shift);
    len -= shift;
    for(size_t i = 0; i < len; ++i) {
        array[i] = array[i + shift];      // <- OOB Lesen (i+shift) UND Schreiben (i)
    }
    return len;
}
```
Mit `array = decoder->timings` (6-Element-Array, 24 Byte) und `len` gleich einem beliebigen
Heap-Müll-Wert > 6 ist das ein memmove-artiger Shift-Loop, der weit über das eigentliche Array
hinaus liest **und schreibt** — direkt angrenzende Struct-Felder (`message`, `state`,
`timings_cnt` selbst, `switch_detect`, `level`, das flexible `data[]`-Array) und je nach
Müllwert auch Speicher jenseits der eigentlichen Allokation.

**Teilbug 2 — Bounds-Prüfung vergleicht Byte-Größe statt Elementanzahl (unabhängig von Teilbug 1):**
```c
// mntm-012, infrared_common_decode():
decoder->timings[decoder->timings_cnt] = duration;
decoder->timings_cnt++;
furi_check(decoder->timings_cnt <= sizeof(decoder->timings));   // 24 (Byte!), nicht 6 (Elemente)
```
`sizeof(decoder->timings)` ist 24 (sechs `uint32_t`), die tatsächliche Kapazität ist aber 6
**Elemente**. Die Prüfung greift damit erst bei `timings_cnt == 25` statt bei `timings_cnt == 7` —
sie toleriert 19 Elemente (76 Byte) Overrun, bevor `furi_check()` das Gerät anhält, statt (durch
die Schreiben-vor-Prüfen-Reihenfolge im Code ohnehin nur) höchstens ein Element. Mit
host-seitigem Repro (siehe Testhinweis) numerisch bestätigt: 25 vs. 7, Faktor 19.

## Nutzen
- Teilbug 1 betrifft **jede** RC5-Fernbedienungs-Aufnahme/-Erkennung bei mir (Universal-Remote-
  Lernmodus, IR-Signal-Empfang/-Decode) — RC5 ist ein verbreitetes IR-Protokoll (u. a. viele
  Philips-/EU-Consumer-Geräte), kein präpariertes Signal nötig, der Bug triggert regulär beim
  allerersten Reset nach Decoder-Alloc.
- Teilbug 2 ist eine generelle Sicherheitsnetz-Verschärfung für **alle** IR-Protokolle, die über
  den gemeinsamen Decoder laufen (NEC/RCA/Kaseikyo/Samsung/RC6/SIRC/Pioneer/RC5) — reduziert das
  Zeitfenster für einen etwaigen Overrun in `decoder->timings[]` von 19 auf höchstens 1 Element,
  unabhängig davon, wodurch die Überschreitung ausgelöst wird.
- Winzige, rein additive Änderung (eine neue Zeile in Alloc, ein Makro-Tausch in einer bestehenden
  Prüfung) — kein API-Bruch, kein Verhaltensunterschied im regulären Erfolgsfall.
- Direkt gegen meine mntm-012-Basis anwendbar, da beide betroffenen Stellen seit dem mntm-012-Stand
  unverändert sind.

## Betroffene Dateien (lokal)
- `lib/infrared/encoder_decoder/common/infrared_common_decoder.c`:
  - `infrared_common_decoder_alloc()`: nach `decoder->level = true;` eine Zeile
    `decoder->timings_cnt = 0;` ergänzen, vor dem `return decoder;`.
  - `infrared_common_decode()`: `furi_check(decoder->timings_cnt <= sizeof(decoder->timings));`
    ersetzen durch `furi_check(decoder->timings_cnt <= COUNT_OF(decoder->timings));`
    (`COUNT_OF` ist bereits über `#include <core/common_defines.h>` verfügbar, das die Datei
    schon inkludiert).

## Risiko
- Sehr niedrig: eine zusätzliche Initialisierungszeile plus ein Makro-Tausch in einer bestehenden
  Prüfung, keine neue Logik, keine Signaturänderung. Der reguläre Erfolgspfad (`timings_cnt` wird
  in der Praxis durch `consume_samples()` laufend klein gehalten) verhält sich identisch zu
  vorher — beide Prüfungen greifen im Normalbetrieb nie.
- Vor dem Übernehmen `InfraredCommonDecoder`-Struct-Layout (`timings[6]`, Feld-Reihenfolge) sowie
  Signatur/Verhalten von `consume_samples()` in meiner mntm-012-Basis gegenlesen — bei mir
  identisch zum zitierten Stand (siehe Port-Skizze Schritt 2).
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Struct-Abweichung als Compile-Fehler auf.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich: den Kontrollfluss-/Speicherzugriffs-
  Bug (uninitialisiertes `timings_cnt` im RC5-Reset-Pfad, zu lasche Bounds-Prüfung) habe ich mit
  einem host-kompilierbaren Mini-Repro nachgestellt (siehe Testhinweis) — der reale RC5-Empfangs-
  pfad auf dem Gerät braucht einen echten Test mit einer eigenen RC5-Fernbedienung, lokal, nicht
  Teil dieses Repos.

## Port-Skizze
1. `lib/infrared/encoder_decoder/common/infrared_common_decoder.c` in meiner mntm-012-Basis
   öffnen: `infrared_common_decoder_alloc()` und `infrared_common_decode()`.
2. Gegenlesen, ob `InfraredCommonDecoder` (`infrared_common_i.h`, Feld `timings[6]` und
   `timings_cnt`) sowie `consume_samples()`/`infrared_common_decoder_reset_state()` weiterhin wie
   oben zitiert aussehen — falls Momentum den IR-Decoder-Stack strukturell verändert hat, Fix
   entsprechend anpassen statt blind zu übernehmen.
3. In `infrared_common_decoder_alloc()`: `decoder->timings_cnt = 0;` nach
   `decoder->level = true;` ergänzen.
4. In `infrared_common_decode()`: `sizeof(decoder->timings)` durch `COUNT_OF(decoder->timings)`
   in der `furi_check(...)`-Zeile ersetzen.
5. Host-seitiges Mini-Repro (`repro.c`, unten) mit
   `gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined` bauen und laufen lassen, um beide
   Teilbugs (ALT/NEU) zu bestätigen — bereits von mir lokal ausgeführt, siehe Testhinweis.
6. Build: `ufbt` (bzw. `fbt firmware_all`).
7. Funktionstest lokal: mit einer eigenen RC5-Fernbedienung (oder einem beliebigen anderen
   IR-Sender) im Universal-Remote-Lernmodus/IR-Empfang mehrfach hintereinander Signale
   aufnehmen/erkennen lassen (Erfolgsfall unverändert, kein Geräteabsturz/-neustart mehr beim
   allerersten RC5-Reset nach Decoder-Alloc, den es vorher potenziell auslösen konnte).

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) für beide Teilbugs, 1:1 aus
`infrared_common_decoder.c` übernommene Kontrollfluss-Logik (`consume_samples()`,
`infrared_common_decoder_reset()`/`_reset_state()`, Alloc, erster `decode()`-Aufruf):

- **Teilbug 1** (fork-isoliert, unter ASan/UBSan, deterministisches Müll-Muster `0x11` statt
  reales uninitialisiertes malloc()-Ergebnis, um nicht vom Zufall abzuhängen — plausibler,
  keineswegs extremer Müllwert):
  ```
  == Fall 1: uninitialisiertes timings_cnt bei Alloc (RC5-Reset-Pfad) ==
    ALT (mntm-012): ABSTURZ (ASan heap-buffer-overflow in consume_samples())
    NEU (Fix):      kein Absturz
  ```
  ASan meldet konkret einen `heap-buffer-overflow`-**Lesezugriff** in `consume_samples()`
  (0 Byte nach einem 48-Byte-Bereich), ausgelöst über
  `first_decode_call → decoder_reset → reset_state_rc5_branch → consume_samples`.

- **Teilbug 2** (reine Formel-/Zahlen-Demonstration, kein unsicherer Speicherzugriff nötig, um
  die Diskrepanz zwischen `sizeof()` und `COUNT_OF()` bei realer Array-Kapazität 6 zu zeigen):
  ```
  == Fall 2: sizeof() vs. COUNT_OF() in der Bounds-Pruefung ==
    echte Array-Kapazitaet:            6 Elemente
    ALT-Pruefung (`<= sizeof(timings)`): erster Abbruch bei timings_cnt == 25 (also 19 Elemente / 76 Byte ueber die Kapazitaet hinaus bereits geschrieben)
    NEU-Pruefung (`<= COUNT_OF(timings)`): erster Abbruch bei timings_cnt == 7 (1 Element / 4 Byte ueber die Kapazitaet hinaus)
    ERGEBNIS: bestaetigt (ALT toleriert 19x mehr Overrun als NEU, bevor furi_check greift)
  ```

```
GESAMTERGEBNIS: bestaetigt.
```
Gebaut/ausgeführt mit `gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined -o repro repro.c &&
./repro` (Exit-Code 0, jede Erwartung bestätigt).

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, Commit `5dc7187085b65bbe8c5f86d5e1232f2932b97f40`
  / PR #4429 (@munzzyy, gemerged 27.07.2026): „Infrared: initialize timings_cnt on decoder alloc
  and fix its bounds check" — `lib/infrared/encoder_decoder/common/infrared_common_decoder.c`
  (Commit-Message des Autors beschreibt exakt den hier verifizierten RC5-Reset-Pfad und die
  `sizeof()`/`COUNT_OF()`-Diskrepanz).
- Momentum-Firmware, Tag `mntm-012`: `lib/infrared/encoder_decoder/common/infrared_common_decoder.c`
  und `infrared_common_i.h` als Vergleichsbasis (Fix dort noch nicht vorhanden, Code identisch zum
  hier zitierten Vorher-Stand).
- Damit ist der seit PR #4369 verfolgte `dev`-Durchlauf (0dd3681f bis 9148d50c, 2026-08-04)
  vollständig abgearbeitet: alle sechs vorgemerkten Kandidaten (`c9c77f66`/FeliCa, `8713d1c4`/
  `bit_lib`, `9c173b46`/Pyramid, `2fe56050`/ISO15693, hier `5dc71870`/Infrared, sowie der bereits
  separat abgehandelte PAC/Stanley-Fund) sind jeweils als eigener Port-Vorschlag dokumentiert.
  Nächster Schritt: `dev` weiter ab `9148d50c` (2026-08-04) gegenprüfen.
