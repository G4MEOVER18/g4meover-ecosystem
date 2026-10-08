# SubGHz: Nord ICE-Protokoll fehlt komplett

## Was

Aus der seit der Beninca-ARC-Sitzung offenen Restliste des `unlshd-093`-Vergleichs
(`allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` — alle bei
Unleashed vorhanden, bei mir nicht; `jarolift` inzwischen aufgegriffen) **`nord_ice.c`**
aufgegriffen. Nord ICE ist ein einfaches, **statisches** (nicht rollierendes) 33-Bit-
Fernbedienungsprotokoll ohne jede Kryptografie — anders als Jarolift/Beninca ARC bringt es keine
Chiffre und keinen Schlüsselspeicher mit, sondern ist reine Timing-Dekodierung plus
Bit-Extraktion (Seriennummer + Tastenfeld direkt aus dem festen 33-Bit-Rahmen). Der Reiz liegt
hier also nicht in neuer Krypto, sondern darin, dass es eine bislang komplett fehlende, einfache
Signalform (echtes Manchester-artiges Kurz/Lang-Timing mit GAP-Rahmung statt Header+Payload) zu
meinem SubGHz-Stack hinzufügt — ein deutlich kleinerer, risikoärmerer Port als die beiden
vorherigen KeeLoq-/AES-Protokolle.

Quelle bei Unleashed (Tag `unlshd-093`): `lib/subghz/protocols/nord_ice.c`/`.h`, laut
Datei-Kommentar "2026.03 - @xMasterX (MMX)".

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `nord_ice.c` benutzte HAL-/Lib-Symbol einzeln gegen meinen `mntm-012`-Tag-Stand
geprüft:

- `lib/subghz/blocks/{decoder,encoder,const,math,generic}.h` — **byteidentisch** zu Unleashed.
  `SubGhzBlockConst` (`te_short`/`te_long`/`te_delta`/`min_count_bit_for_found`),
  `SubGhzBlockDecoder` (`parser_step`/`te_last`/`decode_data`/`decode_count_bit`),
  `SubGhzBlockGeneric` (`data`/`data_count_bit`/`serial`/`btn`/`protocol_name`),
  `subghz_protocol_blocks_add_bit()`, `subghz_protocol_blocks_get_hash_data()`,
  `subghz_protocol_blocks_reverse_key()`, `subghz_block_generic_serialize()`,
  `subghz_block_generic_deserialize_check_count_bit()`, `DURATION_DIFF`-Makro — alles 1:1
  vorhanden, keine einzige abweichende Signatur oder Struct-Feldreihenfolge.
- `lib/subghz/protocols/base.h`/`lib/subghz/types.h` — `SubGhzProtocolDecoder`/`Encoder`-Vtables
  (`alloc`/`free`/`feed`/`reset`/`get_hash_data`/`serialize`/`deserialize`/`get_string` bzw.
  `alloc`/`free`/`deserialize`/`stop`/`yield`), `SubGhzProtocol` (`name`/`type`/`flag`/
  `encoder`/`decoder`/`filter`), `SubGhzProtocolTypeStatic`, alle `SubGhzProtocolFlag_*`-Werte —
  identisch.
- `lib/subghz/protocols/common.c`/`.h` — wie bei Beninca ARC/Jarolift bereits dokumentiert: bei
  mir **komplett nicht vorhanden** (Unleasheds PR-#1116-Flash-Spar-Refactor). `nord_ice.c` nutzt
  daraus `SUBGHZ_ASSERT_DECODER_COMMON_LAYOUT`/`_ENCODER_GENERIC_LAYOUT` (reine
  Compile-Zeit-`_Static_assert`-Prüfungen, keine Laufzeitlogik, hier komplett entfallen, da ich
  keine `*Common`-Cast-Struct verwende), `subghz_protocol_decoder_common_alloc/_free/_reset/
  _get_hash_data/_serialize` sowie `subghz_protocol_encoder_common_alloc/_free/_stop/_yield/
  _deserialize`. Anders als bei Jarolift/Beninca ARC betrifft das hier **ausschließlich**
  Boilerplate — keine Krypto, kein Keystore, kein `_global`-Override (Nord ICE hat gar keinen
  Zähler und keine Taste, die über das Editor-UI übersteuert werden könnte).
- `lib/flipper_format/flipper_format.h` — `flipper_format_read_uint32()` (für das optionale
  `"Repeat"`-Feld) ist bei mir vorhanden, exakt wie von Unleashed benutzt.

**Fazit:** Jedes von `nord_ice.c` verwendete Kern-Symbol ist 1:1 kompatibel; der einzige fehlende
Teil ist `common.*` selbst, das ich bewusst **nicht** übernehme — stattdessen portiere ich
`nord_ice.c` im bei mir etablierten Stil (eigene `_alloc/_free/_reset/_stop/_yield`-Funktionen
pro Protokoll, siehe `x10.c`/`princeton.c`), wie bereits bei Beninca ARC und Jarolift begründet.
Die Dekodier-/Encodier-**Logik selbst** (Timing-Konstanten, State-Machine, Bit-Extraktion) bleibt
dabei Zeile für Zeile unverändert zum Unleashed-Original — nur das drumherum liegende
Allokations-/Vtable-Boilerplate ist eigenständig nachgebaut.

## Fund (keiner)

Im Gegensatz zu den beiden vorherigen Ports (Beninca ARC: Zustandsmaschine-Gate-Nebenfund;
Jarolift: echter Dekodier-Bug im Seed-Feld) fiel beim Durchgehen von `check_remote_controller()`,
`get_upload()` und der `feed()`-Zustandsmaschine **kein** Fehler auf. Einzige
Dokumentationsungenauigkeit im Original-Kommentar: `// 26 bits for serial` neben
`instance->serial = (instance->data >> 15) << 9 | (instance->data & 0x1FF);` — der resultierende
Wert belegt tatsächlich 27 Bit (Bit 26..0), wovon das oberste Bit (ursprünglich Bit 32 des
33-Bit-Rahmens) in allen bekannten Beispielen immer `0` ist, also nur 26 Bit echte Entropie
trägt. Reine Kommentarpräzisierung, keine Funktionsänderung — im Port unverändert übernommen
(siehe Host-Repro Test 1 für die Gegenprobe an den vier echten Beispielwerten aus dem
Original-Kommentar).

Eine interessante, aber ebenfalls **keine eigenständige Fehlfunktion** darstellende Eigenschaft
der (unveränderten) `feed()`-Zustandsmaschine: das einzige GAP-Timing (7500 µs ± 1650 µs) dient
sowohl als Rahmenende des aktuellen Schlüssels als auch als Scharfschalt-Signal für den
nächsten — bei mehreren direkt aneinandergereihten Wiederholungen (`encoder.repeat`) wird dadurch
genau jede zweite Wiederholung tatsächlich dekodiert (die dazwischenliegende schaltet den
Decoder nur erneut scharf, ohne selbst einen vollständigen Treffer zu liefern). Das ist
unverändertes Original-Verhalten, nicht durch den Port verursacht, und für den eigentlichen
Zweck (ein Empfänger braucht nur einen einzigen vollständigen Treffer pro Sendevorgang) folgenlos
— per Host-Repro (Test 2) exakt nachvollzogen und als `found_count == ceil(repeat/2)` belegt,
siehe Testhinweis.

## Betroffene Dateien

- `lib/subghz/protocols/nord_ice.c`/`.h` — **neu**, angepasst an meinen Stil (siehe
  "Kompatibilitätsprüfung" oben): kein `common.h`, eigene `_alloc/_free/_reset/_stop/_yield`,
  eigene `subghz_protocol_decoder_nord_ice_get_hash_data()`/`_serialize()`/`_deserialize()`
  statt der `common.c`-Helfer. Timing-Konstanten, State-Machine (`feed()`), Bit-Extraktion
  (`check_remote_controller()`) und Encoder-Bitstuffing (`get_upload()`) unverändert aus
  Unleashed übernommen.
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag
  (`#include "nord_ice.h"` / `&subghz_protocol_nord_ice,`), jeweils als letzte Zeile vor der
  Listen-Klammer angefügt.
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h` (nicht Teil dieser
  Portierung, keine andere eigene Datei bräuchte sie).

## Risiko

**Sehr gering.** Beide Patch-Dateien (`nord_ice_c`/`_h`) sind neue, eigenständige Dateien — kein
bestehender Code wird verändert, kein bestehendes Protokoll ist betroffen. `protocol_items.{c,h}`
bekommen je eine einzelne zusätzliche Zeile. Keine Krypto, kein Keystore, kein gemeinsam
genutzter Zustand mit einem anderen Protokoll — das kleinste und am leichtesten zu
überprüfende der drei bisher in `firmware-mining/` dokumentierten SubGHz-Protokoll-Ports.
Einziger nicht host-verifizierbarer Teil: die Timing-/GAP-Erkennung in
`subghz_protocol_decoder_nord_ice_feed()` (feste Werte `te_short=300µs`, `te_long=800µs`,
`te_delta=150µs`, End-GAP `te_short*25 ± te_delta*11`) — unverändert aus Unleashed übernommen,
ohne eigene Hardware-Gegenprobe mit einem echten Nord-ICE-Sender nicht unabhängig bestätigt.

**Hinweis zur lokalen Anwendung mit dem bereits vorliegenden Jarolift-Vorschlag:** Beide Ports
hängen ihren jeweiligen `protocol_items.c`/`.h`-Eintrag an dieselbe Stelle (unmittelbar vor der
schließenden Klammer der Registry bzw. ans Dateiende der Includes) an. Wird der
Jarolift-Patch zuerst angewendet, verschiebt sich diese Anker-Zeile um eine Position, wodurch
`git apply` für `protocol_items_c.patch`/`_h.patch` dieses Ports einen Kontextfehler meldet
(selbst geprüft: gegen eine frische `mntm-012`-Zweitkopie mit zuerst angewendetem
Jarolift-`protocol_items`-Patch schlägt `git apply --check` hier fehl). Die `nord_ice.c`/`.h`-
Dateien selbst sind davon nicht betroffen (eigenständige neue Dateien, keine Überlappung) — bei
Konflikt reicht ein einzeiliges manuelles Nachtragen von `#include "nord_ice.h"` bzw.
`&subghz_protocol_nord_ice,` als jeweils neue letzte Zeile. Gegen eine saubere, noch unveränderte
`mntm-012`-Zweitkopie angewendet, ist jeder der vier Patches hier für sich genommen konfliktfrei
(siehe Testhinweis).

## Port-Skizze

1. `nord_ice.c`/`.h` wie in `nord_ice_c.patch`/`nord_ice_h.patch` nach
   `lib/subghz/protocols/` übernehmen.
2. `protocol_items.h` um `#include "nord_ice.h"`, `protocol_items.c` um
   `&subghz_protocol_nord_ice,` ergänzen (`protocol_items_c.patch`/`_h.patch`). Bei bereits
   angewendetem Jarolift-Patch: siehe Hinweis unter "Risiko", einzeilig von Hand nachtragen statt
   `git apply`.
3. Bauen: `ufbt` (bzw. `fbt firmware_all`) — einziger hier nicht nachbildbarer
   Verifikationsschritt (kein `fbt`-Toolchain-Zugriff in dieser Recherche-Umgebung, siehe
   CONSTRAINTS Punkt 7). Jede verbleibende Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf.
4. Kein Keystore-Eintrag, kein Schlüsselmaterial nötig — Nord ICE ist unverschlüsselt, der
   Build ist nach Schritt 3 sofort empfangs- und sendebereit.

## Testhinweis

1. `firmware-mining/repro-nord-ice-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`):
   ```
   Scenario 1 OK: 4/4 real samples -- same Serial, correct raw Btn field each
   Scenario 2 OK: 20000 trials, 35807 total repeats decoded, all bit-exact
   Scenario 3a OK: short trailing gap correctly rejected as end-of-key
   Scenario 3b OK: garbled bit timing aborts the key, no false decode
   ALL CHECKS PASSED
   ```
   Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis bei beiden
   Optimierungsstufen.
2. Patch-Anwendbarkeit selbst geprüft: alle vier Patches per `git apply --check` gegen eine
   frische, unabhängige `mntm-012`-Zweitkopie verifiziert, real angewendet, das Ergebnis
   byte-für-byte mit der separat editierten Arbeitskopie abgeglichen — identisch. Zusätzlich
   gegen eine dritte Zweitkopie mit bereits angewendetem Beninca-ARC- und Jarolift-
   `protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge ist konfliktfrei (fügt seinen Eintrag
   weiter vorn in der Liste ein), die Jarolift-Reihenfolge erzeugt den oben unter "Risiko"
   dokumentierten Kontext-Versatz am Dateiende.
3. Nach `ufbt`-Build und Flash: SubGHz → Rohfunkempfang → 433,92 MHz AM650 gegen einen echten
   Nord-ICE-Sender (eigenes/autorisiertes Gerät) — Protokoll sollte als "Nord ICE" erkannt
   werden, `Serial` sollte über mehrere Tastendrücke desselben Senders hinweg konstant bleiben,
   `Btn` je Taste unterschiedlich.
4. Gezielter Regressionstest für das Repeat/GAP-Verhalten (siehe "Fund"): eine Datei mit
   `Repeat` > 1 senden und am Empfänger (zweites eigenes Gerät oder Rohfunkmitschnitt)
   gegenprüfen, dass nicht jede einzelne Wiederholung einen eigenen Dekodier-Treffer auslöst,
   sondern nur jede zweite — unkritisch für den praktischen Betrieb (ein Treffer reicht), aber
   gut zu wissen, falls beim eigenen Timing-Debugging weniger Treffer als erwartet auftauchen.
