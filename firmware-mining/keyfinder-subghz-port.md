# SubGHz: KeyFinder-Protokoll fehlt komplett

## Was

Aus der seit der Beninca-ARC-Sitzung offenen Restliste des `unlshd-093`-Vergleichs
(`allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` — alle bei
Unleashed vorhanden, bei mir nicht; `jarolift` und `nord_ice` inzwischen aufgegriffen)
**`keyfinder.c`** aufgegriffen. KeyFinder ist, wie Nord ICE, ein einfaches, **statisches**
(nicht rollierendes) Fernbedienungsprotokoll ohne jede Kryptografie — 24-Bit-Rahmen
(20 Bit Seriennummer + 4 Bit Tasten-/Geräte-ID), reine Kurz/Lang-Timing-Dekodierung. Von den vier
verbleibenden Kandidaten (`allstar_firefly`, `ditec_gol4`, `keyfinder`, `treadmill37`) ist
`keyfinder.c` mit 287 Zeilen die kürzeste, unkryptografische Quelldatei — der kleinste,
risikoärmste der vier.

Quelle bei Unleashed (Tag `unlshd-093`): `lib/subghz/protocols/keyfinder.c`/`.h`, laut
Datei-Kommentar "2026.03 - @xMasterX (MMX)".

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `keyfinder.c` benutzte HAL-/Lib-Symbol einzeln gegen meinen `mntm-012`-Tag-Stand
geprüft:

- `lib/subghz/blocks/{decoder,encoder,const,math,generic}.h` — **byteidentisch** zu Unleashed.
  `SubGhzBlockConst` (`te_short`/`te_long`/`te_delta`/`min_count_bit_for_found`),
  `SubGhzBlockDecoder` (`parser_step`/`te_last`/`decode_data`/`decode_count_bit`),
  `SubGhzBlockGeneric` (`data`/`data_count_bit`/`serial`/`btn`/`protocol_name`),
  `subghz_protocol_blocks_add_bit()`, `subghz_protocol_blocks_get_hash_data()`,
  `subghz_protocol_blocks_reverse_key()`, `subghz_block_generic_serialize()`,
  `subghz_block_generic_deserialize_check_count_bit()`, `DURATION_DIFF`-Makro — alles 1:1
  vorhanden, keine einzige abweichende Signatur oder Struct-Feldreihenfolge. `decoder.c`s
  `subghz_protocol_blocks_add_bit()` selbst ist byteidentisch (nur die hier nicht genutzte
  `get_hash_data()`-Hashfunktion unterscheidet sich zwischen Unleashed/Momentum — bereits aus
  früheren Ports bekannt, hier folgenlos, da KeyFinder keine eigene Hash-Logik mitbringt).
- `lib/subghz/protocols/base.h`/`lib/subghz/types.h` — `SubGhzProtocolDecoder`/`Encoder`-Vtables,
  `SubGhzProtocol` (`name`/`type`/`flag`/`encoder`/`decoder`), `SubGhzProtocolTypeStatic`, alle
  `SubGhzProtocolFlag_*`-Werte (`_433`/`_AM`/`_Decodable`/`_Load`/`_Save`/`_Send`) — identisch.
- `lib/subghz/protocols/common.c`/`.h` — wie bei Beninca ARC/Jarolift/Nord ICE bereits
  dokumentiert: bei mir **komplett nicht vorhanden** (Unleasheds PR-#1116-Flash-Spar-Refactor).
  `keyfinder.c` nutzt daraus `SUBGHZ_ASSERT_DECODER_COMMON_LAYOUT`/`_ENCODER_GENERIC_LAYOUT`
  (reine Compile-Zeit-Prüfungen, hier entfallen) sowie `subghz_protocol_decoder_common_alloc/
  _free/_get_hash_data/_serialize` und `subghz_protocol_encoder_common_alloc/_free/_stop/_yield/
  _deserialize`. Wie bei Nord ICE betrifft das ausschließlich Boilerplate — kein `_global`-
  Override (KeyFinder hat keinen Zähler, der Tasten-/ID-Wert ist Teil der festen 24-Bit-ID, nicht
  separat überschreibbar).
- `lib/flipper_format/flipper_format.h` — `flipper_format_read_uint32()` (für das optionale
  `"Repeat"`-Feld) ist bei mir vorhanden, exakt wie von Unleashed benutzt.

**Fazit:** Jedes von `keyfinder.c` verwendete Kern-Symbol ist 1:1 kompatibel; der einzige
fehlende Teil ist `common.*` selbst, das ich bewusst **nicht** übernehme — stattdessen portiere
ich `keyfinder.c` im bei mir etablierten Stil (eigene `_alloc/_free/_reset/_stop/_yield`-
Funktionen pro Protokoll, siehe `x10.c`/`princeton.c`), wie bereits bei Beninca ARC, Jarolift und
Nord ICE begründet.

## Fund: ein voller Durchlauf von Finish diskontiert das eigene Rahmenende, jede zweite
## direkt aufeinanderfolgende Wiederholung geht beim Dekodieren verloren

Beim Durchgehen von `check_remote_controller()`, `get_upload()` und der `feed()`-
Zustandsmaschine fiel folgendes auf: Nach dem 24. dekodierten Bit erwartet `feed()` vier
HIGH-Impulse der Länge `te_short` als Rahmenende-Markierung (`end_count` zählt sie, bei `== 4`
wird `parser_step = KeyFinderDecoderStepFinish` gesetzt). Der **vierte** dieser HIGH-Impulse
löst den Zustandswechsel zu `Finish` aus — der unmittelbar darauf folgende LOW-Impuls
(`te_short * 10`, die eigentliche Rahmenende-/Folgerahmen-Gap) wird dadurch erst im **nächsten**
`feed()`-Aufruf verarbeitet, und zwar bereits im `Finish`-Zweig. Dessen Code-Körper verwendet
seine `level`/`duration`-Parameter aber überhaupt nicht — er liest nur `decode_count_bit`,
übergibt den gefundenen Schlüssel an den Callback und setzt `parser_step` zurück auf `Reset`.
Der LOW-Impuls selbst, der exakt der Rahmen-Timing-Wert ist, den `Reset` normalerweise als
"Gap gefunden, nächsten Rahmen scharf schalten" erkennt, wird dadurch **ungeprüft verworfen**.

**Praktische Auswirkung, per Host-Repro belegt (Szenario 3):** Bei unmittelbar aufeinander
folgenden Wiederholungen (wie `subghz_protocol_encoder_common_yield()`/mein eigenes
`subghz_protocol_encoder_keyfinder_yield()` sie tatsächlich senden — der Upload-Puffer läuft
beim Wiederholen einfach wieder von Index 0 los, ohne zusätzliche Stille dazwischen) bleibt die
Dekodier-Zustandsmaschine nach dem Verwerfen dieses LOW-Impulses in `Reset` stehen, bis die
**nächste** Wiederholung mit ihrem **eigenen** Rahmenende-LOW-Impuls zufällig wieder in
`Reset`s Erkennungsfenster fällt. Das Ergebnis: von `N` unmittelbar aufeinanderfolgenden
Wiederholungen werden nur `ceil(N/2)` tatsächlich dekodiert (Wiederholung 1, 3, 5, … — jede
dazwischenliegende schaltet nur stillschweigend den Folge-Rahmen scharf, ohne selbst einen
Treffer zu liefern). Mechanisch ein anderer Fund als die bereits bei Nord ICE dokumentierte
GAP-Doppelnutzung, zufällig aber mit demselben resultierenden Muster.

**Einordnung:** Kein Speicherfehler, keine Sicherheitslücke. Wie bei Nord ICE bereits begründet,
praktisch folgenlos für den üblichen Anwendungsfall (ein Tastendruck eines echten KeyFinder-
Senders muss nur **einmal** erfolgreich dekodiert werden, um den Schlüssel zu speichern — das
gelingt bei jeder ungerade nummerierten Wiederholung). Anders als bei Nord ICE ist die Ursache
hier aber eine reine **Zustandsmaschinen-Falle** (ein Edge-Fall beim Verschieben der Fund-Logik
in einen eigenen Folgezustand) und nicht dem Timing-Format selbst inhärent — die Korrektur ist
entsprechend klein und risikoarm: ich verschiebe die Fund-Callback-/Reset-Logik eine Stufe
früher, direkt in den Zweig, der `end_count == 4` erkennt, statt sie einem separaten `Finish`-
Zustand zu überlassen. Dadurch bleibt der `KeyFinderDecoderStepFinish`-Zustand komplett
überflüssig (im Port entfernt) und der abschließende LOW-Impuls wird beim nächsten Aufruf bereits
korrekt von `Reset` ausgewertet — keine Zeile der eigentlichen Timing-Konstanten, Bit-Dekodierung
oder Rahmenlänge ändert sich. Host-Repro Szenario 4 belegt: mit der Korrektur werden alle `N`
Wiederholungen bitgenau dekodiert, unabhängig von `N` (1–5, wie `encoder.repeat` es im Original
zulässt). Szenario 5 belegt, dass die Korrektur die Validierung nicht aufweicht (ein verstümmelter
Rahmenende-Impuls bzw. ein verstümmeltes inneres Bit-Timing wird weiterhin vollständig
verworfen).

Die vier echten Tastendruck-Beispiele aus dem Original-Kommentar (`RED`/`PURPLE`/`GREEN`/`BLUE`,
`C396F` + jeweils andere 4-Bit-ID) decodieren mit `check_remote_controller()` unverändert
korrekt (Host-Repro Szenario 1) — der Fund betrifft ausschließlich das Wiederholungsverhalten,
nicht die Bit-Extraktion selbst.

## Betroffene Dateien

- `lib/subghz/protocols/keyfinder.c`/`.h` — **neu**, angepasst an meinen Stil (siehe
  "Kompatibilitätsprüfung" oben): kein `common.h`, eigene `_alloc/_free/_reset/_stop/_yield`,
  eigene `subghz_protocol_decoder_keyfinder_get_hash_data()`/`_serialize()`/`_deserialize()`
  statt der `common.c`-Helfer. Timing-Konstanten, Bit-Extraktion (`check_remote_controller()`)
  und Encoder-Bitstuffing (`get_upload()`) unverändert aus Unleashed übernommen; einzige
  inhaltliche Abweichung ist der oben beschriebene, durch Host-Repro Szenario 3/4 belegte
  Fund-Timing-Fix in `subghz_protocol_decoder_keyfinder_feed()` (mit Kommentar an der
  betroffenen Stelle dokumentiert, analog zum Jarolift-Seed-Byte-Fix).
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag
  (`#include "keyfinder.h"` / `&subghz_protocol_keyfinder,`), jeweils als letzte Zeile vor der
  Listen-Klammer angefügt.
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h` (nicht Teil dieser
  Portierung, keine andere eigene Datei bräuchte sie).

## Risiko

**Gering.** Beide neuen Dateien (`keyfinder_c`/`_h`) sind eigenständige neue Dateien — kein
bestehender Code wird verändert, kein bestehendes Protokoll ist betroffen. `protocol_items.{c,h}`
bekommen je eine einzelne zusätzliche Zeile. Keine Krypto, kein Keystore, kein gemeinsam
genutzter Zustand mit einem anderen Protokoll. Die einzige bewusste inhaltliche Abweichung vom
Unleashed-Original (der Fund-Timing-Fix) ist durch das Host-Repro in beiden Richtungen (Fehler
UND Korrektur bitgenau reproduziert) belegt und ändert nichts an Timing-Konstanten, Bit-Reihen-
folge oder Dateiformat — ein geladenes `.sub`-Schlüsselfile verhält sich identisch, nur das
Dekodierverhalten bei unmittelbar aufeinanderfolgenden Wiederholungen verbessert sich. Einziger
nicht host-verifizierbarer Teil: die Timing-/GAP-Erkennung in
`subghz_protocol_decoder_keyfinder_feed()` (feste Werte `te_short=400µs`, `te_long=1200µs`,
`te_delta=150µs`, End-Gap `te_short*10 ± te_delta*5`) — unverändert aus Unleashed übernommen,
ohne eigene Hardware-Gegenprobe mit einem echten KeyFinder-Sender nicht unabhängig bestätigt.

**Hinweis zur lokalen Anwendung mit den bereits vorliegenden Jarolift-/Nord-ICE-Vorschlägen:**
Alle drei Ports hängen ihren jeweiligen `protocol_items.c`/`.h`-Eintrag an dieselbe Stelle
(unmittelbar vor der schließenden Klammer der Registry bzw. ans Dateiende der Includes) an.
Selbst geprüft (frische, unabhängige `mntm-012`-Zweitkopien, je einzeln):

- Gegen eine **unveränderte** Zweitkopie: alle vier Patches hier konfliktfrei
  (`git apply --check` erfolgreich).
- Mit bereits angewendetem **Beninca-ARC**-`protocol_items`-Patch: konfliktfrei (Beninca ARC
  fügt seinen Eintrag weiter vorn in der Liste ein, wie bereits beim Nord-ICE-Port dokumentiert).
- Mit bereits angewendetem **Jarolift**- oder **Nord-ICE**-`protocol_items`-Patch: jeweils ein
  Kontextfehler am Dateiende (beide hängen ihren Eintrag an dieselbe Anker-Zeile), exakt das
  bereits zwischen Jarolift und Nord ICE dokumentierte Muster. Die `keyfinder.c`/`.h`-Dateien
  selbst sind davon nicht betroffen (eigenständige neue Dateien, keine Überlappung) — bei
  Konflikt reicht ein einzeiliges manuelles Nachtragen von `#include "keyfinder.h"` bzw.
  `&subghz_protocol_keyfinder,` als jeweils neue letzte Zeile.

## Port-Skizze

1. `keyfinder.c`/`.h` wie in `keyfinder_c.patch`/`keyfinder_h.patch` nach
   `lib/subghz/protocols/` übernehmen.
2. `protocol_items.h` um `#include "keyfinder.h"`, `protocol_items.c` um
   `&subghz_protocol_keyfinder,` ergänzen (`protocol_items_c.patch`/`_h.patch`). Bei bereits
   angewendetem Jarolift- oder Nord-ICE-Patch: siehe Hinweis unter "Risiko", einzeilig von Hand
   nachtragen statt `git apply`.
3. Bauen: `ufbt` (bzw. `fbt firmware_all`) — einziger hier nicht nachbildbarer
   Verifikationsschritt (kein `fbt`-Toolchain-Zugriff in dieser Recherche-Umgebung, siehe
   CONSTRAINTS Punkt 7). Jede verbleibende Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf.
4. Kein Keystore-Eintrag, kein Schlüsselmaterial nötig — KeyFinder ist unverschlüsselt, der
   Build ist nach Schritt 3 sofort empfangs- und sendebereit.

## Testhinweis

1. `firmware-mining/repro-keyfinder-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`):
   ```
   Scenario 1 OK: 4/4 real samples -- same 20-bit Serial, correct 4-bit ID each
   Scenario 2 OK: 20000 single-frame trials, buggy and fixed both decode exactly once
   Scenario 3 OK (bug confirmed): 5000 trials, Repeat=1..5 back-to-back, unmodified upstream
   feed() decodes 8914/14821 repeats (exactly ceil(repeat/2) each)
   Scenario 4 OK (fix verified): 5000 trials, Repeat=1..5 back-to-back, fixed feed() decodes
   all 15058 repeats, bit-exact
   Scenario 5a OK: mistimed end marker still rejected by the fixed feed()
   Scenario 5b OK: garbled interior bit timing still aborts cleanly
   ALL CHECKS PASSED
   ```
   Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis bei beiden
   Optimierungsstufen. (Die exakte Zahl bei Szenario 3/4 ist vom festen PRNG-Seed abhängig, das
   Muster `gefunden == ceil(repeat/2)` bzw. `gefunden == repeat` gilt deterministisch für jeden
   einzelnen der 5000 Versuche, siehe `assert()`s im Code.)
2. Patch-Anwendbarkeit selbst geprüft: alle vier Patches per `git apply --check` gegen eine
   frische, unabhängige `mntm-012`-Zweitkopie verifiziert, real angewendet, das Ergebnis
   byte-für-byte mit der separat editierten Arbeitskopie abgeglichen — identisch. Zusätzlich
   gegen separate Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift- bzw.
   Nord-ICE-`protocol_items`-Patch geprüft (siehe "Risiko" für die Ergebnisse je Kombination).
3. Nach `ufbt`-Build und Flash: SubGHz → Rohfunkempfang → 433,92 MHz AM650 gegen einen echten
   KeyFinder-Sender (eigenes/autorisiertes Gerät) — Protokoll sollte als "KeyFinder" erkannt
   werden, `Serial` sollte über mehrere Tastendrücke desselben Senders hinweg konstant bleiben,
   `ID` je Taste/Gerät unterschiedlich.
4. Gezielter Regressionstest für den Fund (siehe oben): eine Datei mit `Repeat` > 1 senden und
   am Empfänger (zweites eigenes Gerät oder Rohfunkmitschnitt) gegenprüfen, dass mit dem
   portierten (korrigierten) Code **jede** Wiederholung einen eigenen Dekodier-Treffer auslöst
   — zum Vergleich ließe sich dieselbe Prüfung gegen eine unveränderte Unleashed-Fassung
   gegenprüfen, dort sollte nur jede zweite Wiederholung einen Treffer liefern.
