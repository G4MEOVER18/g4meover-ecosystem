# SubGHz: Ditec GOL4-Protokoll fehlt komplett

## Was

Aus der seit der Beninca-ARC-Sitzung offenen Restliste des `unlshd-093`-Vergleichs
(`allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` — alle bei
Unleashed vorhanden, bei mir nicht; die anderen fünf inzwischen aufgegriffen) den letzten
verbleibenden Kandidaten **`ditec_gol4.c`** aufgegriffen (675 Zeilen). Ditec GOL4 ist, anders als
die bereits portierten statischen Protokolle (Nord ICE, Treadmill37, Allstar Firefly) oder das
KeeLoq-basierte Jarolift, ein **eigenes, proprietäres Rolling-Code-Schema**: ein 54-Bit-Rahmen
(Seriennummer 32 Bit, 4-Bit-Tastencode, 16-Bit-Zähler) wird über eine Kette aus Bit-Rotation,
Bit-Spiegelung, einer festen XOR-Maskierung und einem 8-Bit-LCG (`x' = 21·x + 1 mod 256`,
analytisch invertierbar, siehe unten) verschleiert — kein KeeLoq, kein AES, kein Schlüsselspeicher,
für eine Supertex-GOL4-Funkfernbedienung (Ditec-Torantriebe).

Quelle bei Unleashed (Tag `unlshd-093`): `lib/subghz/protocols/ditec_gol4.c`/`.h`, laut
Datei-Kommentar "2025 - 2026.02 - @xMasterX (MMX) & @zero-mega".

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `ditec_gol4.c` benutzte HAL-/Lib-Symbol einzeln gegen eine frische, unabhängige
Zweitkopie von `Next-Flip/Momentum-Firmware` (Tag `mntm-012`) geprüft:

- `lib/subghz/blocks/{decoder,encoder,const,math,generic,custom_btn_i}.h` — **byteidentisch** zu
  Unleashed. `SubGhzBlockConst`/`SubGhzBlockDecoder`/`SubGhzBlockGeneric`,
  `subghz_protocol_blocks_add_bit()`, `subghz_protocol_blocks_get_hash_data()`,
  `subghz_block_generic_serialize()`, `subghz_block_generic_deserialize_check_count_bit()`,
  `DURATION_DIFF`/`bit_read`-Makros, `level_duration_make()`/`level_duration_reset()` — alles
  1:1 vorhanden. `custom_btn_i.h` (`subghz_custom_btn_get/_get_original/_set_original/_set_max`,
  `SUBGHZ_CUSTOM_BTN_*`) ebenfalls 1:1 vorhanden, inklusive `custom_btn.c`.
- `lib/subghz/protocols/base.h`/`lib/subghz/types.h` — Decoder-/Encoder-Vtables,
  `SubGhzProtocol`, `SubGhzProtocolTypeDynamic`, alle benötigten `SubGhzProtocolFlag_*`-Werte —
  identisch.
- `furi_hal_subghz_get_rolling_counter_mult()` — vorhanden, exakt wie von Jarolift/KeyFinder
  bereits genutzt.
- `lib/subghz/protocols/common.c`/`.h` — wie bei Beninca ARC/Jarolift/KeyFinder/Nord ICE/
  Treadmill37/Allstar Firefly bereits dokumentiert: bei mir **komplett nicht vorhanden**
  (Unleasheds Flash-Spar-Refactor). Betrifft hier Boilerplate
  (`_common_alloc/_free/_stop/_yield/_get_hash_data/_serialize`), identisch zum bei mir
  etablierten Ersatzmuster.
- **Fehlt zusätzlich, anders als bei den fünf vorherigen Ports:** `subghz_block_generic_global`
  (Struct + `_counter_override_get`/`_button_override_get`/die `cnt_is_available`-Felder für die
  Signal-Editor-Zähler-/Tasten-Override-UI). `ditec_gol4.c` ist bei Unleashed das **einzige**
  dieser sechs Protokolle, das diese Infrastruktur tatsächlich aktiv aufruft (nicht nur
  auskommentiert wie bei Nord ICE/Treadmill37) — exakt dasselbe Muster, das bereits beim
  Jarolift-Port dokumentiert und dort durch den unveränderten Rückfall auf den
  Standard-Zähler-Pfad ersetzt wurde (siehe `jarolift-keeloq-subghz-port.md`, Abschnitt
  "Kompatibilitätsprüfung"). Hier identisch gehandhabt (siehe "Port-Skizze").

**Fazit:** Jedes von `ditec_gol4.c` verwendete Kern-Symbol ist 1:1 kompatibel; die beiden
fehlenden Teile (`common.*`, `subghz_block_generic_global`) werden nach demselben bereits
etablierten Muster ersetzt. Die Dekodier-/Encodier-**Logik selbst** (Rolling-Code-Transform,
Timing-Konstanten, State-Machine) bleibt dabei Zeile für Zeile unverändert zum
Unleashed-Original — mit einer einzigen Ausnahme, siehe Fund 2.

## Kryptografische Prüfung

Keine etablierte, extern verifizierte Primitive (anders als Jarolift/Beninca ARC) — ein
eigenständiges, proprietäres Verschleierungsschema, daher hier die **Rundlauf-Korrektheit** der
beiden Transform-Hälften direkt geprüft statt Testvektoren gegen eine Referenzimplementierung:

- **LCG-Invertierbarkeit:** `gol4_lcg_step(x, n)` wendet `x' = 21·x + 1 mod 256` `n`-mal an.
  `gol4_lcg_inverse(target, n)` berechnet `gol4_lcg_step(target, 256-n)` — korrekt, weil
  `x ↦ 21·x+1 mod 256` eine Permutation von `Z/256` ist (21 ist ungerade, also invertierbar
  mod 256) und ihre Ordnung ein Teiler von 256 ist, wodurch 256 Anwendungen stets zur Identität
  zurückführen; `256-n` Vorwärtsschritte sind damit exakt die Umkehrung von `n` Schritten.
  Einzeln gegengerechnet statt nur angenommen (Host-Repro, Teil von Szenario A).
- **Gesamter Rundlauf:** `gol4_rolling_encode()` → `gol4_rolling_decode()` über 50 000 zufällige
  7-Byte-Rahmen (Byte 2 auf `btn&0x0F` eingeschränkt — der einzige Bereich, den
  `encode_key()` dort tatsächlich hineinschreibt) bit-exakt auf allen sieben Bytes außer dem
  oberen Nibble von Byte 2 (das `decode_key()` selbst nie zurückliest, siehe unten) — Host-Repro
  Szenario A, 0 Abweichungen in 50 000 Durchläufen.
- **`raw[2]==0 → 0xF0`-Sonderfall** (Taste 0 = "Prog"-Modus, laut Decoder-eigenem
  Kommentar zwei reale Beispiele mit `Btn:0`): nach dem vollen Rundlauf kommt der volle Byte-Wert
  deterministisch als `0xF0` zurück, nicht als `0x00` — harmlos, weil `decode_key()`
  ausschließlich `decrypted[2]&0x0F` in `instance->btn` zurückliest (`0xF0&0x0F == 0x00&0x0F
  == 0`). Einzeln verifiziert, Host-Repro Szenario A2.
- **Vier reale Beispiele** aus dem Decoder-eigenen Kommentar (`0xCCB2F8320ED66`/`...37832104A6`/
  `...3B8320DB4E`/`...3D8320E855`, Tasten 1/2/4/8 einer realen Fernbedienung, als 54-Bit-Binärwert
  direkt aus dem Kommentar übernommen statt über die dort separat gelistete, mehrdeutige
  "Decoded array"-Zeilenliste — deren Anzahl nicht zu den Roh-Beispielen passt, 5 "Regular"- und
  2 "Programming"-Einträge gegen nur 4 bzw. 1 Rohwert, daher bewusst nicht für eine
  1:1-Zuordnung verwendet): alle vier dekodieren zur selben Seriennummer `0xCC093483`, mit dem
  jeweils dokumentierten Tastennibble — Host-Repro Szenario B.

Keine eigene Schwäche in der Verschleierung selbst gefunden oder gesucht (das ist kein
Sicherheits-Audit des Schemas, nur eine Korrektheitsprüfung des Ports); beide folgenden Funde
betreffen das Drumherum, nicht die Transform-Mathematik.

## Fund 1: Decoder benötigt mindestens zwei Wiederholungen — eine einzelne Übertragung wird nie dekodiert

Dieselbe Fundklasse wie bereits bei Nord ICE/Treadmill37/Allstar Firefly dokumentiert (ein GAP
dient gleichzeitig als Rahmenende- und Scharfschalt-Signal), hier aber mit einem spürbar
**schlechteren** Ergebnis, weil `ditec_gol4.c`s Zustandsmaschine das Rahmenende anders erkennt:

Die Reset-Stufe (`DitecGOL4DecoderStepReset`) erkennt das führende GAP eines Rahmens (`te_long*22`,
~24 200 µs) direkt und sauber. Das Rahmenende wird dagegen **nicht** dort erkannt, sondern in der
`CheckDuration`-Stufe, deren `level==false`-Zweig den zuletzt gespeicherten `te_last`-Wert gegen
`te_long*20` (~22 000 µs) prüft — und zwar unabhängig vom aktuell übergebenen `duration`-Wert.
Nach dem letzten Bit eines Rahmens steht die Maschine in `SaveDuration`; das nächste GAP (vom
**nächsten** Rahmen) wird dort als `te_last` gespeichert (fälschlich als wäre es ein normales
Bit-Low), die Maschine geht nach `CheckDuration`. Der darauf folgende Start-Bit-Puls des nächsten
Rahmens (`level==true`) passt zu keinem der beiden Bit-Muster (weil `te_last` noch das GAP ist) —
es gibt dafür keinen `else`-Zweig, die Maschine bleibt unverändert in `CheckDuration` stehen. Erst
das **erste Datenbit** des nächsten Rahmens (`level==false`) trifft auf den gap-Vergleich, der
wegen des immer noch gespeicherten GAP-`te_last`-Werts zuschlägt — und löst damit **verzögert**
den Abschluss des **vorherigen** Rahmens aus (Callback + Zähler-Reset), while der gerade erst
begonnene nächste Rahmen dabei verworfen wird (`parser_step = Reset`, aber ohne dass dessen
eigenes GAP je gesehen wurde — alle seine Bits laufen bis zum Ende seines eigenen letzten Bits
ins Leere, bis irgendwann das übernächste GAP wieder regulär erkannt wird).

**Empirisch nachgemessen** (Host-Repro Szenario C, encoder-get_upload() → decoder-feed(),
`repeat`=1..6 echte Rahmen hintereinander, exakt wie `subghz_protocol_encoder_ditec_gol4_yield()`
sie erzeugen würde):

| repeat | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| gefundene Rahmen | **0** | 1 | 1 | 2 | 2 | 3 |

Formel: `found_count == floor(repeat/2)`, **nicht** `ceil(repeat/2)` wie bei den drei bereits
dokumentierten einfacheren Fällen (Nord ICE/Treadmill37/Allstar Firefly, dort genügt eine einzige
`level==false`-Prüfung mit demselben Wert, der gerade als `duration` ankommt, statt eine
verzögerte Prüfung gegen einen stehengebliebenen `te_last`). **`repeat=1` — eine Taste einmal
gedrückt, keine Wiederholung gesendet — wird von diesem Decoder nie dekodiert.** Praktisch
eingeordnet: reale GOL4-Handsender senden laut Encoder-Default (`repeat=4` in
`subghz_protocol_encoder_ditec_gol4_alloc()`) üblicherweise mehrfach pro Tastendruck, sodass im
Normalbetrieb trotzdem mindestens ein Treffer zu erwarten ist — ein per Rohfunk mitgeschnittenes
Signal mit nur einer einzigen Übertragung (z. B. ein verkürzter Mitschnitt oder ein Sender mit
`Repeat`=1) würde dagegen **gar nicht** erkannt, auch nicht beim erneuten Abspielen der
Aufzeichnung selbst — ein über die bisherigen drei Funde hinausgehender, praktisch relevanterer
Fall. Unverändertes Original-Verhalten (identisch bei Unleashed selbst); ich übernehme die
State-Machine unverändert (siehe "Betroffene Dateien") und dokumentiere die Formel als Kommentar
im Decoder-Code.

## Fund 2: Signed-Shift-UB in `serial_to_display()` — behoben

`serial_to_display()` baut die angezeigte Seriennummer so zusammen:

```c
return (uint32_t)((s[0] << 24) | (s[4] << 16) | (s[1] << 8) | s[3]);
```

`s[0]` ist `uint8_t`, wird für die Schiebeoperation nach `int` befördert; für jeden Wert
`s[0] >= 0x80` überläuft `s[0] << 24` einen 32-Bit-`int` mit gesetztem Vorzeichenbit —
Undefined Behavior nach C-Standard, auch wenn jeder gängige Compiler/ABI (inkl. der hier
verwendeten ARM-Toolchain) in der Praxis einfach das erwartete Bitmuster liefert. Nicht
theoretisch: **alle vier** realen Beispiele im Decoder-eigenen Kommentar beginnen mit
`s[0]==0xCC` — jede echte Dekodierung dieser Fernbedienungsfamilie triggert die UB. Unter
`-fsanitize=undefined` bricht der Ausdruck für `s[0]=0xCC` hart ab (Host-Repro Szenario F,
isoliert in einem `fork()`-Kindprozess, selbe Technik wie
`firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c`, damit der Abbruch eines Falls nicht
den ganzen Testlauf beendet).

**Fix** (eine Zeile, additiv, jedes Byte vor dem Schieben nach `uint32_t` gecastet):

```c
return ((uint32_t)s[0] << 24) | ((uint32_t)s[4] << 16) | ((uint32_t)s[1] << 8) | (uint32_t)s[3];
```

Mathematisch identisches Ergebnis für jeden Eingabewert (Host-Repro Szenario F: beide Fassungen
liefern für `s[0]=0x4C` — oberstes Bit nicht gesetzt, keine UB in der Originalfassung — exakt
denselben Wert; für `s[0]=0xCC` liefert die gefixte Fassung `0xCC093483`, die Originalfassung
bricht ab statt ein Ergebnis zu liefern). In meinen Port übernommen (siehe `ditec_gol4.c`,
`serial_to_display()`).

## Betroffene Dateien

- `lib/subghz/protocols/ditec_gol4.c`/`.h` — **neu**, angepasst an meinen Stil: kein
  `common.h`, keine `SUBGHZ_ASSERT_*`-Makros, eigene `_alloc/_free/_reset/_stop/_yield`, eigene
  `subghz_protocol_decoder_ditec_gol4_get_hash_data()`/`_serialize()`/`_deserialize()` statt der
  `common.c`-Helfer (identisches Muster zu den fünf vorherigen Ports). `subghz_block_generic_global`
  entfernt: `subghz_protocol_ditec_gol4_encode_key()` nimmt (wie beim Jarolift-Port) immer den
  Standard-Zähler-Pfad statt des Editor-UI-Override; `subghz_protocol_decoder_ditec_gol4_get_string()`
  lässt die drei `subghz_block_generic_global.*`-Zuweisungen (Editor-UI-Push) ersatzlos weg —
  beides als Kommentar an der jeweiligen Stelle dokumentiert. `serial_to_display()` auf die
  UB-freie Fassung aus Fund 2 umgestellt. Rolling-Code-Transform
  (`gol4_bit_reverse/_parity`, `gol4_lcg_step/_inverse`, `gol4_decode_rotate_and_bitrev`,
  `gol4_decode_lcg_xor`, `gol4_encode_lcg_xor`, `gol4_encode_bitrev_and_rotate`), Timing-Konstanten,
  State-Machine (`feed()`), Encoder-Bitstuffing (`get_upload()`) und Tasten-Remap
  (`subghz_protocol_ditec_gol4_get_btn_code()`) unverändert aus Unleashed übernommen.
  `subghz_protocol_ditec_gol4_create_data()` (für eine künftige "Add Manually"-Anbindung) mit
  übernommen, aber — wie im Original, wo sie nur in `public_api.h` deklariert ist, nicht in
  `ditec_gol4.h` — hier ebenfalls bewusst nicht in der eigenen `.h` deklariert; die
  "Add Manually"-UI-Verdrahtung selbst (`subghz_txrx_create_protocol_key.c`) ist nicht Teil
  dieses Ports (siehe "Port-Skizze").
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag
  (`#include "ditec_gol4.h"` / `&subghz_protocol_ditec_gol4,`), jeweils als letzte Zeile vor der
  Listen-Klammer angefügt.
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h`, `subghz_block_generic_global`
  (siehe oben), die "Add Manually"-Anbindung in
  `applications/main/subghz/plugins/add_manually/subghz_txrx_create_protocol_key.c`.

## Risiko

**Sehr gering.** Beide Patch-Dateien (`ditec_gol4_c`/`_h`) sind neue, eigenständige Dateien —
kein bestehender Code wird verändert, kein bestehendes Protokoll ist betroffen. `protocol_items.{c,h}`
bekommen je eine einzelne zusätzliche Zeile. Kein Keystore, kein mit einem anderen Protokoll
gemeinsam genutzter Zustand. Die einzige inhaltliche Änderung gegenüber dem Unleashed-Original
ist der additive UB-Fix in `serial_to_display()` (Fund 2) — mathematisch ergebnisgleich, siehe
oben. Fund 1 (Decoder-Zustandsmaschine) wird bewusst **nicht** verändert, um die Original-Logik
nicht zu verzweigen; rein dokumentiert.

**Hinweis zur lokalen Anwendung mit den bereits vorliegenden Jarolift-/KeyFinder-/Nord-ICE-/
Treadmill37-/Allstar-Firefly-Vorschlägen:** Dieselbe bereits fünffach dokumentierte
Anker-Zeilen-Kollision in `protocol_items.{c,h}` (alle sechs Ports hängen ihren Eintrag an
dieselbe Stelle an) — selbst erneut geprüft: mit zuvor angewendetem Jarolift-, KeyFinder-,
Nord-ICE-, Treadmill37- oder Allstar-Firefly-`protocol_items`-Patch meldet `git apply --check`
für diesen Ditec-GOL4-Patch einen Kontextfehler; mit zuvor angewendetem Beninca-ARC-Patch (fügt
seinen Eintrag weiter vorn ein) ist er dagegen konfliktfrei. Bei Konflikt reicht ein einzeiliges
manuelles Nachtragen von `#include "ditec_gol4.h"` bzw. `&subghz_protocol_ditec_gol4,` als jeweils
neue letzte Zeile. Gegen eine saubere, unveränderte `mntm-012`-Zweitkopie angewendet, ist jeder
der vier Patches hier für sich genommen konfliktfrei (siehe Testhinweis).

## Port-Skizze

1. `ditec_gol4.c`/`.h` wie in `ditec_gol4_c.patch`/`ditec_gol4_h.patch` nach
   `lib/subghz/protocols/` übernehmen.
2. `protocol_items.h` um `#include "ditec_gol4.h"`, `protocol_items.c` um
   `&subghz_protocol_ditec_gol4,` ergänzen (`protocol_items_c.patch`/`_h.patch`). Bei bereits
   angewendetem Jarolift-/KeyFinder-/Nord-ICE-/Treadmill37-/Allstar-Firefly-Patch: siehe Hinweis
   unter "Risiko", einzeilig von Hand nachtragen statt `git apply`.
3. Bauen: `ufbt` (bzw. `fbt firmware_all`) — einziger hier nicht nachbildbarer
   Verifikationsschritt (kein `fbt`-Toolchain-Zugriff in dieser Recherche-Umgebung, siehe
   CONSTRAINTS Punkt 7). Jede verbleibende Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf.
4. Kein Keystore-Eintrag, kein Schlüsselmaterial nötig — der Build ist nach Schritt 3 sofort
   empfangs- und sendebereit.
5. Nicht Teil dieses Ports (optionale künftige Erweiterung): `ditec_gol4_create_data()` ist mit
   übernommen, aber ohne Anbindung an die "Add Manually"-UI-Szene — wer eine GOL4-Fernbedienung
   ohne eigenen Mitschnitt rein aus Seriennummer/Taste/Zähler anlegen will, müsste
   `subghz_txrx_create_protocol_key.c` zusätzlich um einen eigenen Fall erweitern (siehe
   Unleashed-Original, `applications/main/subghz/plugins/add_manually/subghz_txrx_create_protocol_key.c:473`).

## Testhinweis

1. `firmware-mining/repro-ditec-gol4-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`, bis auf die eine erwartete,
   Szenario F zugehörige UBSan-Zeile aus dem isolierten Kindprozess):
   ```
   repro.c:70:29: runtime error: left shift of 204 by 24 places cannot be represented in type 'int'
   Scenario A OK: 50000 trials, rolling-code encode<->decode round trip bit-exact
   Scenario A2 OK: btn==0 (Prog mode) round trip recovers byte2=0xF0 (not 0x00) as the encoder's own special case intends, btn nibble 0x0 either way
   Scenario B OK: 4/4 doc-comment samples decode to the same Serial (0xCC093483), correct Btn nibble each
   Scenario C repeat=1 -> found_count=0 (ceil(repeat/2)=1)
   Scenario C repeat=2 -> found_count=1 (ceil(repeat/2)=1)
   Scenario C repeat=3 -> found_count=1 (ceil(repeat/2)=2)
   Scenario C repeat=4 -> found_count=2 (ceil(repeat/2)=2)
   Scenario C repeat=5 -> found_count=2 (ceil(repeat/2)=3)
   Scenario C repeat=6 -> found_count=3 (ceil(repeat/2)=3)
   Scenario D OK: too-short leading gap correctly rejected
   Scenario E OK: corrupted bit timing does not produce a false frame
   Scenario F OK: serial_to_display_orig(s[0]=0xCC) aborts under UBSan (signed left-shift overflow); serial_to_display_fixed(s[0]=0xCC) survives, result 0xCC093483; both agree for s[0]=0x4C (top bit clear)
   ALL SCENARIOS PASSED
   ```
   Keine Compiler-Warnungen, identisches Ergebnis bei beiden Optimierungsstufen.
2. Patch-Anwendbarkeit selbst geprüft: alle vier Patches per `git apply --check` gegen eine
   frische, unabhängige `mntm-012`-Zweitkopie verifiziert, real angewendet, das Ergebnis
   byte-für-byte mit der separat editierten Arbeitskopie abgeglichen — identisch. Zusätzlich
   gegen fünf weitere Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift-,
   KeyFinder-, Nord-ICE- bzw. Treadmill37-`protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge
   konfliktfrei, die anderen vier erzeugen den oben unter "Risiko" dokumentierten Kontext-Versatz
   am Dateiende.
3. Nach `ufbt`-Build und Flash: SubGHz → Rohfunkempfang → 433 MHz AM650 gegen einen echten
   Ditec-GOL4-Handsender (eigenes/autorisiertes Gerät) — Protokoll sollte als "Ditec GOL4"
   erkannt werden, Seriennummer/Taste sollten über mehrere Tastendrücke hinweg konstant bleiben,
   der Zähler sollte pro Tastendruck hochzählen.
4. Gezielter Regressionstest für Fund 1: ein eigenes, zuvor gespeichertes `.sub`
   (`Repeat`-Feld manuell auf `1` gesetzt) erneut senden und am Empfänger (zweites eigenes Gerät
   oder Rohfunkmitschnitt) gegenprüfen, dass dabei **kein** Treffer erscheint; danach `Repeat`
   wieder auf `2`+ setzen und gegenprüfen, dass dann wieder `floor(repeat/2)` Treffer erscheinen.
   Unkritisch für den praktischen Betrieb mit einem echten Handsender (sendet laut Encoder-Default
   mehrfach), aber relevant, falls beim eigenen Timing-/Mitschnitt-Debugging mit künstlich
   verkürzten Einzelmitschnitten gearbeitet wird.
5. Gezielter Regressionstest für Fund 2: eine echte GOL4-Fernbedienung mit Serial-Präfix `0xCC`
   (wie in allen vier Kommentar-Beispielen) empfangen und die angezeigte Seriennummer gegen den
   per Host-Repro berechneten Wert abgleichen — mit dem additiven Fix identisch zur
   Original-Formel (nur UB-frei), kein beobachtbarer Unterschied im Feld erwartet.
