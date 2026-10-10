# SubGHz: Treadmill37-Protokoll fehlt komplett

## Was

Aus der seit der Beninca-ARC-Sitzung offenen Restliste des `unlshd-093`-Vergleichs
(`allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` — alle bei
Unleashed vorhanden, bei mir nicht; `jarolift`/`keyfinder`/`nord_ice` inzwischen aufgegriffen)
**`treadmill37.c`** aufgegriffen: mit 281 Zeilen die kürzeste der beiden verbleibenden
Kandidaten (`allstar_firefly`, `ditec_gol4` bleiben offen). Treadmill37 (laut Unleashed-Kommentar
"QH-433") ist wie Nord ICE und KeyFinder ein einfaches, **statisches** (nicht rollierendes)
Fernbedienungsprotokoll ohne jede Kryptografie — 37 Bit, reine Timing-Dekodierung plus
Bit-Extraktion (Seriennummer + Zähler-/Tastenfeld direkt aus dem festen Rahmen). Architektonisch
nahezu baugleich zu Nord ICE (gleiche drei Decoder-Zustände, gleiches Kurz/Lang-Timing-Schema,
gleiche GAP-Rahmung) — nur mit anderen Timing-Konstanten (`te_short=300`, `te_long=900` statt
`800`) und einem anderen Bit-Layout (20 Bit Serial + 16 Bit Zähler/Taste + 1 Stop-Bit statt
26/6-Bit-Aufteilung).

Quelle bei Unleashed (Tag `unlshd-093`): `lib/subghz/protocols/treadmill37.c`/`.h`.

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `treadmill37.c` benutzte HAL-/Lib-Symbol einzeln gegen meinen `mntm-012`-Tag-Stand
geprüft (frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware`, Tag `mntm-012`):

- `lib/subghz/blocks/{decoder,encoder,const,math,generic}.h` — **byteidentisch** zu Unleashed.
  `SubGhzBlockConst` (`te_short`/`te_long`/`te_delta`/`min_count_bit_for_found`),
  `SubGhzBlockDecoder` (`parser_step`/`te_last`/`decode_data`/`decode_count_bit`),
  `SubGhzBlockGeneric` (`data`/`data_count_bit`/`serial`/`cnt`/`protocol_name` — `serial` als
  `uint32_t`, `cnt` als `uint32_t`, beide vorhanden und vom benötigten Typ),
  `subghz_protocol_blocks_add_bit()`, `subghz_protocol_blocks_get_hash_data()`,
  `subghz_protocol_blocks_reverse_key()`, `subghz_block_generic_serialize()`,
  `subghz_block_generic_deserialize_check_count_bit()`, `DURATION_DIFF`/`bit_read`-Makros,
  `level_duration_make()`/`level_duration_reset()` — alles 1:1 vorhanden, keine abweichende
  Signatur oder Struct-Feldreihenfolge.
- `lib/subghz/protocols/base.h`/`lib/subghz/types.h` — Decoder-/Encoder-Vtables, `SubGhzProtocol`,
  `SubGhzProtocolTypeStatic`, alle benötigten `SubGhzProtocolFlag_*`-Werte — identisch.
- `lib/subghz/protocols/common.c`/`.h` — wie bei Beninca ARC/Jarolift/KeyFinder/Nord ICE bereits
  dokumentiert: bei mir **komplett nicht vorhanden** (Unleasheds Flash-Spar-Refactor). Betrifft
  hier ausschließlich Boilerplate (`_common_alloc/_free/_reset/_stop/_yield/_get_hash_data/
  _serialize`) — kein Keystore, kein `_global`-Override (Treadmill37 hat wie Nord ICE keinen
  Zähler- oder Tasten-Override über das Editor-UI).
- `lib/flipper_format/flipper_format.h` — `flipper_format_read_uint32()` (optionales
  `"Repeat"`-Feld) vorhanden, exakt wie benutzt.

**Fazit:** Jedes von `treadmill37.c` verwendete Kern-Symbol ist 1:1 kompatibel; einzig fehlender
Teil ist `common.*` selbst, bewusst **nicht** übernommen — Port im etablierten eigenen Stil
(eigene `_alloc/_free/_reset/_stop/_yield`-Funktionen pro Protokoll statt der `common.c`-Helfer,
wie bei Beninca ARC/Jarolift/KeyFinder/Nord ICE begründet). Die Dekodier-/Encodier-**Logik
selbst** (Timing-Konstanten, State-Machine, Bit-Extraktion) bleibt dabei Zeile für Zeile
unverändert zum Unleashed-Original — nur das Allokations-/Vtable-Boilerplate ist eigenständig
nachgebaut, und der Datei-Kommentar wurde um die Autor-/Datumszeile sowie (siehe "Fund") um die
falsch ausgerichtete Bit-Tabelle bereinigt.

## Fund

Kein Dekodier-Bug. Zwei Punkte geprüft und eingeordnet:

1. **Kommentarungenauigkeit** (reine Dokumentation, keine Funktionsänderung — exakt dieselbe
   Fundklasse wie zuvor bei Nord ICE: dort 26 vs. tatsächlich 27 belegte Bit). Der
   Original-Kommentar zeigt die sechs Beispielwerte als 40-Bit-Strings auf, mit einem 23 Zeichen
   breiten "serial"-Block:
   ```
   //              serial                  button           stop
   // 1800001830 = 00011000000000000000000 0000110000011000 0
   ```
   Tatsächlich ist `min_count_bit_for_found = 37` (alle sechs Beispielwerte haben
   `bit_length() == 37`, keine 40), und `check_remote_controller()` liest `serial = data >> 17` —
   bei einem 37-Bit-Wert sind das **20 Bit**, nicht 23. Die drei überzähligen führenden Nullen im
   Kommentar sind reines Padding auf eine 40-Bit-Darstellung, kein zusätzliches, tatsächlich
   übertragenes Datenfeld (verifiziert: alle sechs Beispielwerte reduzieren sich korrekt auf
   20 Bit Serial + 16 Bit Zähler/Taste + 1 Stop-Bit = 37 Bit, siehe Host-Repro Test 1). Im Port
   durch eine auf die tatsächliche 37-Bit-Breite korrigierte Tabelle ersetzt (siehe
   `treadmill37.c` im Patch).
2. **GAP-Wiederholungs-Eigenschaft** (wie bei Nord ICE: keine eigenständige Fehlfunktion, nicht
   neu, hier nur gegengeprüft statt übersehen). Die `feed()`-Zustandsmaschine ist strukturell
   identisch zu der bereits portierten Nord ICE: das einzige GAP-Timing (hier `te_short*20 =
   6000 µs ± 600 µs`, bei Nord ICE `te_short*25 ± te_delta*11`) dient sowohl als Rahmenende des
   aktuellen Schlüssels als auch als Scharfschalt-Signal für den nächsten. Bei mehreren direkt
   aneinandergereihten Wiederholungen (`encoder.repeat`, Standard 3) wird dadurch genau jede
   zweite Wiederholung tatsächlich dekodiert (die dazwischenliegende schaltet den Decoder nur
   erneut scharf, ohne selbst einen vollständigen Treffer zu liefern) — unverändertes
   Original-Verhalten, nicht durch den Port verursacht, für den eigentlichen Zweck (ein Empfänger
   braucht nur einen einzigen vollständigen Treffer pro Sendevorgang) folgenlos. Exakt wie bei
   Nord ICE per Host-Repro (Test 2) als `found_count == ceil(repeat/2)` belegt — konsistent mit
   der dort bereits getroffenen Einordnung "Timing-Merkmal des Formats, kein Fehler".

## Betroffene Dateien

- `lib/subghz/protocols/treadmill37.c`/`.h` — **neu**, angepasst an meinen Stil (siehe
  "Kompatibilitätsprüfung" oben): kein `common.h`, eigene `_alloc/_free/_reset/_stop/_yield`,
  eigene `subghz_protocol_decoder_treadmill37_get_hash_data()`/`_serialize()`/`_deserialize()`
  statt der `common.c`-Helfer. Timing-Konstanten, State-Machine (`feed()`), Bit-Extraktion
  (`check_remote_controller()`) und Encoder-Bitstuffing (`get_upload()`) unverändert aus
  Unleashed übernommen; Datei-Kommentar um Autor-/Datumszeile bereinigt und um die unter "Fund"
  beschriebene, auf 37 Bit korrigierte Beispieltabelle ersetzt.
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag
  (`#include "treadmill37.h"` / `&subghz_protocol_treadmill37,`), jeweils als letzte Zeile vor
  der Listen-Klammer angefügt.
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h`.

## Risiko

**Sehr gering.** Beide Patch-Dateien (`treadmill37_c`/`_h`) sind neue, eigenständige Dateien —
kein bestehender Code wird verändert, kein bestehendes Protokoll ist betroffen.
`protocol_items.{c,h}` bekommen je eine einzelne zusätzliche Zeile. Keine Krypto, kein Keystore,
kein gemeinsam genutzter Zustand mit einem anderen Protokoll.

**Hinweis zur lokalen Anwendung mit den bereits vorliegenden Jarolift-/KeyFinder-/Nord-ICE-
Vorschlägen:** Diese drei Ports sowie dieser hängen ihren jeweiligen `protocol_items.c`/`.h`-
Eintrag an dieselbe Stelle (unmittelbar vor der schließenden Klammer der Registry bzw. ans
Dateiende der Includes) an. Selbst geprüft (frische `mntm-012`-Zweitkopie, je ein Kandidat zuerst
angewendet): mit zuvor angewendetem Jarolift-, KeyFinder- oder Nord-ICE-`protocol_items`-Patch
meldet `git apply --check` für diesen Treadmill37-Patch einen Kontextfehler (alle vier hängen an
derselben Anker-Zeile); mit zuvor angewendetem Beninca-ARC-Patch (fügt seinen Eintrag weiter vorn
in der Liste ein) ist dieser Patch dagegen konfliktfrei. Bei Konflikt reicht ein einzeiliges
manuelles Nachtragen von `#include "treadmill37.h"` bzw. `&subghz_protocol_treadmill37,` als
jeweils neue letzte Zeile. Gegen eine saubere, unveränderte `mntm-012`-Zweitkopie angewendet, ist
jeder der vier Patches hier für sich genommen konfliktfrei (siehe Testhinweis).

## Port-Skizze

1. `treadmill37.c`/`.h` wie in `treadmill37_c.patch`/`treadmill37_h.patch` nach
   `lib/subghz/protocols/` übernehmen.
2. `protocol_items.h` um `#include "treadmill37.h"`, `protocol_items.c` um
   `&subghz_protocol_treadmill37,` ergänzen (`protocol_items_c.patch`/`_h.patch`). Bei bereits
   angewendetem Jarolift-/KeyFinder-/Nord-ICE-Patch: siehe Hinweis unter "Risiko", einzeilig von
   Hand nachtragen statt `git apply`.
3. Bauen: `ufbt` (bzw. `fbt firmware_all`) — einziger hier nicht nachbildbarer
   Verifikationsschritt (kein `fbt`-Toolchain-Zugriff in dieser Recherche-Umgebung, siehe
   CONSTRAINTS Punkt 7). Jede verbleibende Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf.
4. Kein Keystore-Eintrag, kein Schlüsselmaterial nötig — Treadmill37 ist unverschlüsselt, der
   Build ist nach Schritt 3 sofort empfangs- und sendebereit.

## Testhinweis

1. `firmware-mining/repro-treadmill37-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`):
   ```
   Scenario 1 OK: 6/6 real samples -- same 20-bit Serial, correct Btn field each, stop bit 0
   Scenario 2 OK: 20000 trials, 35786 total repeats decoded, all bit-exact
   Scenario 3a OK: short trailing gap correctly rejected as end-of-key
   Scenario 3b OK: garbled bit timing aborts the key, no false decode
   ALL CHECKS PASSED
   ```
   Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis bei beiden
   Optimierungsstufen.
2. Patch-Anwendbarkeit selbst geprüft: alle vier Patches per `git apply --check` gegen eine
   frische, unabhängige `mntm-012`-Zweitkopie verifiziert, real angewendet, das Ergebnis
   byte-für-byte mit der separat editierten Arbeitskopie abgeglichen — identisch. Zusätzlich
   gegen vier weitere Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift-,
   KeyFinder- bzw. Nord-ICE-`protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge konfliktfrei,
   die anderen drei erzeugen den oben unter "Risiko" dokumentierten Kontext-Versatz am
   Dateiende.
3. Nach `ufbt`-Build und Flash: SubGHz → Rohfunkempfang → 433,92 MHz AM650 gegen einen echten
   Treadmill37-/QH-433-Sender (eigenes/autorisiertes Gerät) — Protokoll sollte als "Treadmill37"
   erkannt werden, `Serial` sollte über mehrere Tastendrücke desselben Senders hinweg konstant
   bleiben, `Btn` je Taste unterschiedlich.
4. Gezielter Regressionstest für das Repeat/GAP-Verhalten (siehe "Fund", Punkt 2): eine Datei mit
   `Repeat` > 1 senden und am Empfänger (zweites eigenes Gerät oder Rohfunkmitschnitt)
   gegenprüfen, dass nicht jede einzelne Wiederholung einen eigenen Dekodier-Treffer auslöst,
   sondern nur jede zweite — unkritisch für den praktischen Betrieb (ein Treffer reicht), aber
   gut zu wissen, falls beim eigenen Timing-Debugging weniger Treffer als erwartet auftauchen.
