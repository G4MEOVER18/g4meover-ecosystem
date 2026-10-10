# SubGHz: Allstar-Firefly-Protokoll fehlt komplett

## Was

Aus der seit der Beninca-ARC-Sitzung offenen Restliste des `unlshd-093`-Vergleichs
(`allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` — alle bei
Unleashed vorhanden, bei mir nicht; `jarolift`/`keyfinder`/`nord_ice`/`treadmill37` inzwischen
aufgegriffen) **`allstar_firefly.c`** aufgegriffen: der letzte verbleibende Kandidat ohne
Kryptografie (`ditec_gol4` bleibt als einziger offen, 675 Zeilen, laut vorherigem Sachstand
„deutlich umfangreicher"). Allstar Firefly (Supertex-ED-9-Encoder-IC, Torantrieb-Fernbedienung
318ALD31K) ist ein **statisches** (nicht rollierendes) 18-Bit-Protokoll ohne jede Kryptografie —
anders als Jarolift/KeyFinder/Nord-ICE/Treadmill37 aber kein reines Serial+Zähler/Taste-Layout,
sondern ein 9-stelliger **trinärer** DIP-Schaltercode (jede Position `+`/`0`/`-`, je zwei Bit),
wie er bei mechanischen Drehschaltern mit drei Rastpositionen üblich ist.

Quelle bei Unleashed (Tag `unlshd-093`): `lib/subghz/protocols/allstar_firefly.c`/`.h`.

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `allstar_firefly.c` benutzte HAL-/Lib-Symbol einzeln gegen meinen `mntm-012`-Tag-Stand
geprüft (frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware`, Tag `mntm-012`):

- `lib/subghz/blocks/{decoder,encoder,const,math,generic}.h` — **byteidentisch** zu Unleashed
  bis auf den bereits bei den vier vorherigen Ports dokumentierten Wegfall der
  `SubGhzBlockGenericGlobal`-Struktur (Editor-UI-Zähler-/Tasten-Override, `endless_tx`) — hier
  **ohne jede Relevanz**, da Allstar Firefly weder Zähler noch Taste hat (reiner statischer
  DIP-Code, kein Editor-UI-Override-Pfad in `allstar_firefly.c` vorhanden). `SubGhzBlockConst`
  (`te_short`/`te_long`/`te_delta`/`min_count_bit_for_found`), `SubGhzBlockDecoder`
  (`parser_step`/`te_last`/`decode_data`/`decode_count_bit`), `subghz_protocol_blocks_add_bit()`,
  `subghz_protocol_blocks_get_hash_data()`, `subghz_protocol_blocks_reverse_key()`,
  `subghz_block_generic_serialize()`, `subghz_block_generic_deserialize_check_count_bit()`,
  `DURATION_DIFF`/`bit_read`-Makros, `level_duration_make()`/`level_duration_reset()` — alles 1:1
  vorhanden, keine abweichende Signatur oder Struct-Feldreihenfolge. `SubGhzBlockGeneric` wird
  hier nur für `data`/`data_count_bit`/`protocol_name` benutzt (kein `serial`/`cnt`/`btn` — siehe
  "Was"), alle drei vorhanden.
- `lib/subghz/protocols/base.h`/`lib/subghz/types.h` — Decoder-/Encoder-Vtables, `SubGhzProtocol`,
  `SubGhzProtocolTypeStatic`, die benutzten `SubGhzProtocolFlag_315/_AM/_Decodable/_Load/_Save/
  _Send`-Werte — alle vorhanden und unverändert. Zwei bereits bei Beninca ARC dokumentierte,
  folgenlose strukturelle Abweichungen gegenprüft und bestätigt weiterhin folgenlos:
  - Meine `SubGhzProtocolDecoder`-Vtable hat zusätzlich `get_hash_data_long`/`get_string_brief`
    (Unleashed hat sie entfernt) — bei einem Designated-Initializer-Literal, das wie meine
    Portierung nur `.get_hash_data` setzt, bleiben beide einfach `NULL`
    (`subghz_protocol_decoder_base_get_hash_data_long()`/`_get_hash_data()` fallen laut
    `protocols/base.c` bei `NULL` ohne Crash auf die jeweils andere Variante zurück bzw. liefern
    0 — kein Nullpointer-Risiko).
  - Meine `SubGhzProtocol`-Struct hat zusätzlich ein `.filter`-Feld (`SubGhzProtocolFilter`-Enum),
    das Unleashed entfernt hat — bleibt bei einem Literal, das es nicht setzt, einfach auf `0`,
    genau wie es `faac_slh.c` bei mir bereits handhabt (kein `.filter` gesetzt).
  - Zusätzlich neu gegenprüft (bei den vier vorherigen Ports nicht einzeln aufgeführt, hier
    wegen der entfernten `SubGhzProtocolFlag_*`-Werte explizit genannt): mein
    `SubGhzProtocolFlag`-Enum hat `_ReversRB2`/`_Alarms`/`_Sensors`/`_Princeton`/`_NiceFlorS`
    (Bit 11–15) entfernt (jetzt als eigenes `SubGhzProtocolFilter`-Enum geführt) —
    `allstar_firefly.c` benutzt keinen dieser fünf Werte, nur `_315`/`_AM`/`_Decodable`/`_Load`/
    `_Save`/`_Send`, alle unverändert vorhanden.
- `lib/subghz/protocols/common.c`/`.h` — wie bei Beninca ARC/Jarolift/KeyFinder/Nord-ICE/
  Treadmill37 bereits dokumentiert: bei mir **komplett nicht vorhanden** (Unleasheds
  Flash-Spar-Refactor). Betrifft hier ausschließlich Boilerplate (`_common_alloc/_free/_reset/
  _stop/_yield/_get_hash_data/_serialize`, `subghz_protocol_encoder_common_deserialize()`) — kein
  Keystore, kein `_global`-Override.
- `SUBGHZ_ASSERT_DECODER_COMMON_LAYOUT`/`SUBGHZ_ASSERT_ENCODER_GENERIC_LAYOUT` — in
  `common.h` definierte reine Compile-Zeit-Sanity-Makros (prüfen, dass die eigene Decoder-/
  Encoder-Struct mit dem `common.c`-Layout übereinstimmt); da ich `common.h` wie bei den vier
  vorherigen Ports nicht übernehme, entfallen beide Makro-Aufrufe ersatzlos — sie prüfen nur
  Kompatibilität zu einer Infrastruktur, die ich gar nicht benutze.
- `lib/flipper_format/flipper_format.h` — `flipper_format_read_uint32()` (optionales
  `"Repeat"`-Feld im Encoder) vorhanden, exakt wie benutzt.

**Fazit:** Jedes von `allstar_firefly.c` verwendete Kern-Symbol ist 1:1 kompatibel; einzig
fehlender Teil ist `common.*` selbst, bewusst **nicht** übernommen — Port im etablierten eigenen
Stil (eigene `_alloc/_free/_reset/_stop/_yield`-Funktionen statt der `common.c`-Helfer, wie bei
Beninca ARC/Jarolift/KeyFinder/Nord-ICE/Treadmill37 begründet). Die Dekodier-/Encodier-**Logik
selbst** (Timing-Konstanten, State-Machine, Bit-Extraktion, DIP-Anzeige) bleibt dabei Zeile für
Zeile unverändert zum Unleashed-Original.

## Fund

Kein Dekodier-Bug. Dieselbe GAP-Wiederholungs-Eigenschaft wie bei Nord ICE und Treadmill37
bereits dokumentiert — keine eigenständige Fehlfunktion, hier nur für diese Implementierung
(eigene Timing-Konstanten: `te_short=600`, `te_long=4000`, `te_delta=300`, GAP = `te_short*50 =
30000 µs ± te_delta*5 = 1500 µs`) gegengeprüft statt übersehen.

Die `feed()`-Zustandsmaschine ist strukturell identisch zu Nord ICE/Treadmill37: das einzige
GAP-Timing dient sowohl als Rahmenende des aktuellen Schlüssels als auch als Scharfschalt-Signal
für den nächsten. Nach einem vollständig dekodierten Rahmen geht der Decoder in
`AllstarFireflyDecoderStepReset` zurück und wartet dort erneut auf ein GAP (`!level` mit
~30000 µs), um in `SaveDuration` zu wechseln. Bei mehreren direkt aneinandergereihten
Wiederholungen (laut Unleashed-Header-Kommentar „20 frames per keypress", eigener Encoder-Default
`repeat=5`) wird das GAP, das Rahmen N beendet, bereits dabei verbraucht — die Bits von Rahmen
N+1 laufen dadurch komplett in `Reset` (werden ignoriert), bis Rahmen N+1s **eigenes** Rahmenende
ein neues GAP liefert, das dann Rahmen N+2 scharf schaltet. Ergebnis: von `repeat` direkt
aufeinanderfolgenden Rahmen wird nur jeder zweite tatsächlich dekodiert
(`found_count == ceil(repeat/2)`, per Host-Repro Test 2 über 20000 Zufallswerte und
Wiederholungen 1–5 bestätigt) — unverändertes Original-Verhalten, für den eigentlichen Zweck
(ein Empfänger braucht nur einen einzigen vollständigen Treffer pro Tastendruck) folgenlos,
konsistent mit der bei Nord ICE/Treadmill37 bereits getroffenen Einordnung „Timing-Merkmal des
Formats, kein Fehler".

Zusätzlich gegenprüft (reine Beobachtung, kein Fund, da unverändertes Original-Verhalten ohne
praktische Auswirkung): die Bit-Extraktion selbst arbeitet unabhängig von der
2-Bit-DIP-Symbol-Gruppierung — jedes HIGH/LOW-Paar wird für sich als Bit 0 oder 1 gewertet, ohne
Kenntnis der darüberliegenden `+`/`0`/`-`-Struktur. Eine durch Störung entstandene, nie vom
Encoder erzeugte Bitfolge `01` in einer 2-Bit-Gruppe würde `get_string()`s drei
`SHOW_DIP_P`-Zeilen an dieser Stelle alle drei `_` anzeigen lassen (keine der drei
`check_dip`-Werte `0b11`/`0b10`/`0b00` passt) — unverändertes Original-Anzeigeverhalten, keine
Fehlfunktion, nur eine optische Nichtübereinstimmung mit der physischen DIP-Schalterstellung bei
bereits gestörtem Empfang.

## Betroffene Dateien

- `lib/subghz/protocols/allstar_firefly.c`/`.h` — **neu**, angepasst an meinen Stil (siehe
  "Kompatibilitätsprüfung" oben): kein `common.h`, keine `SUBGHZ_ASSERT_*`-Makros, eigene
  `_alloc/_free/_reset/_stop/_yield`, eigene
  `subghz_protocol_decoder_allstar_firefly_get_hash_data()`/`_serialize()`/`_deserialize()`
  statt der `common.c`-Helfer. Timing-Konstanten, State-Machine (`feed()`), DIP-Anzeige
  (`get_string()`) und Encoder-Bitstuffing (`get_upload()`) unverändert aus Unleashed
  übernommen; Datei-Kommentar um Autor-/Datumszeile bereinigt.
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag
  (`#include "allstar_firefly.h"` / `&subghz_protocol_allstar_firefly,`), jeweils als letzte
  Zeile vor der Listen-Klammer angefügt.
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h`.

## Risiko

**Sehr gering.** Beide Patch-Dateien (`allstar_firefly_c`/`_h`) sind neue, eigenständige
Dateien — kein bestehender Code wird verändert, kein bestehendes Protokoll ist betroffen.
`protocol_items.{c,h}` bekommen je eine einzelne zusätzliche Zeile. Keine Krypto, kein Keystore,
kein gemeinsam genutzter Zustand mit einem anderen Protokoll.

**Hinweis zur lokalen Anwendung mit den bereits vorliegenden Jarolift-/KeyFinder-/Nord-ICE-/
Treadmill37-Vorschlägen:** Diese vier Ports sowie dieser hängen ihren jeweiligen
`protocol_items.c`/`.h`-Eintrag an dieselbe Stelle (unmittelbar vor der schließenden Klammer der
Registry bzw. ans Dateiende der Includes) an. Selbst geprüft (frische `mntm-012`-Zweitkopie, je
ein Kandidat zuerst angewendet): mit zuvor angewendetem Jarolift-, KeyFinder-, Nord-ICE- oder
Treadmill37-`protocol_items`-Patch meldet `git apply --check` für diesen Allstar-Firefly-Patch
einen Kontextfehler (alle fünf hängen an derselben Anker-Zeile); mit zuvor angewendetem
Beninca-ARC-Patch (fügt seinen Eintrag weiter vorn in der Liste ein) ist dieser Patch dagegen
konfliktfrei. Bei Konflikt reicht ein einzeiliges manuelles Nachtragen von
`#include "allstar_firefly.h"` bzw. `&subghz_protocol_allstar_firefly,` als jeweils neue letzte
Zeile. Gegen eine saubere, unveränderte `mntm-012`-Zweitkopie angewendet, ist jeder der fünf
Patches hier für sich genommen konfliktfrei (siehe Testhinweis).

## Port-Skizze

1. `allstar_firefly.c`/`.h` wie in `allstar_firefly_c.patch`/`allstar_firefly_h.patch` nach
   `lib/subghz/protocols/` übernehmen.
2. `protocol_items.h` um `#include "allstar_firefly.h"`, `protocol_items.c` um
   `&subghz_protocol_allstar_firefly,` ergänzen (`protocol_items_c.patch`/`_h.patch`). Bei
   bereits angewendetem Jarolift-/KeyFinder-/Nord-ICE-/Treadmill37-Patch: siehe Hinweis unter
   "Risiko", einzeilig von Hand nachtragen statt `git apply`.
3. Bauen: `ufbt` (bzw. `fbt firmware_all`) — einziger hier nicht nachbildbarer
   Verifikationsschritt (kein `fbt`-Toolchain-Zugriff in dieser Recherche-Umgebung, siehe
   CONSTRAINTS Punkt 7). Jede verbleibende Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf.
4. Kein Keystore-Eintrag, kein Schlüsselmaterial nötig — Allstar Firefly ist unverschlüsselt,
   der Build ist nach Schritt 3 sofort empfangs- und sendebereit.

## Testhinweis

1. `firmware-mining/repro-allstar-firefly-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`):
   ```
   Scenario 1 OK: header DIP example 0x30ABC decodes back to "+--000++-"
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
   gegen fünf weitere Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift-,
   KeyFinder-, Nord-ICE- bzw. Treadmill37-`protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge
   konfliktfrei, die anderen vier erzeugen den oben unter "Risiko" dokumentierten
   Kontext-Versatz am Dateiende.
3. Nach `ufbt`-Build und Flash: SubGHz → Rohfunkempfang → 318 MHz AM650 gegen einen echten
   Allstar-Firefly-318ALD31K-Sender (eigenes/autorisiertes Gerät) — Protokoll sollte als
   "Allstar Firefly" erkannt werden, die drei DIP-Zeilen sollten exakt der physischen
   9-Positionen-Schalterstellung des Senders entsprechen.
4. Gezielter Regressionstest für das Repeat/GAP-Verhalten (siehe "Fund"): eine Datei mit
   `Repeat` > 1 senden und am Empfänger (zweites eigenes Gerät oder Rohfunkmitschnitt)
   gegenprüfen, dass nicht jede einzelne Wiederholung einen eigenen Dekodier-Treffer auslöst,
   sondern nur jede zweite — unkritisch für den praktischen Betrieb (ein Treffer reicht), aber
   gut zu wissen, falls beim eigenen Timing-Debugging weniger Treffer als erwartet auftauchen.
