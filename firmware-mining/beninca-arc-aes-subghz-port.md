# SubGHz: Beninca-ARC-AES-Rollcode-Protokoll fehlt komplett

## Was

Die `unlshd-090`-Restliste aus dem Backlog aufgegriffen ("neue SubGHz-Protokolle listen →
Port-Vorschläge") und gegen den tatsächlich neuesten Unleashed-Tag (`unlshd-093`, aktueller
als `unlshd-090`) geprüft: `lib/subghz/protocols/` dort gegen meine `mntm-012`-Basis
verglichen. Sieben Protokolle sind bei Unleashed vorhanden, bei mir nicht:
`allstar_firefly`, `beninca_arc`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`,
`treadmill37` (`prastel` und `telcoma_edge` sind die beiden anderen fehlenden Dateien, aber
bereits als eigene Port-Vorschläge dokumentiert — `prastel-42bit-rolling-code-port.md`,
`telcoma-cardin-edge-subghz.md` — und damit kein neuer Fund).

Ausgewählt: **`beninca_arc.c`** — "Beninca ARC", ein Rollcode-Protokoll für Torantriebe/
Schiebetore des italienischen Herstellers Beninca. Grund der Auswahl: als einziges der sieben
fehlenden Protokolle verwendet es **AES-128** statt der bei mir bereits mehrfach vorhandenen
KeeLoq-/StarLine-Krypto (`keeloq.c`, `star_line.c`, `faac_slh.c` nutzen alle `keeloq_common.c`)
— eine komplett neue Krypto-Primitive für meinen SubGHz-Stack, kein weiterer
KeeLoq-Abkömmling. `jarolift` (ebenfalls rolling code, aber KeeLoq-basiert über
`keeloq_common.h`) wäre die naheliegende Alternative gewesen, bringt aber keine neue
Primitive mit.

Quelle bei Unleashed: Commit `db2dc8f64` ("subghz: add beninca arc protocol",
2026-01-21, @xMasterX/@zero-mega), seitdem nur zwei Folgekommits (Typo-Fix, Programmier-Modus-
Fix), unverändert im Tag `unlshd-093`. Drei Dateien: `beninca_arc.c`/`.h` (das Protokoll
selbst) und zwei **neue Hilfsmodule**, die bei Unleashed von mehreren neueren Protokollen
gemeinsam genutzt werden und bei mir ebenfalls vollständig fehlen:
- `aes_common.c`/`.h` — eine eigenständige AES-128-Implementierung (SBox, ShiftRows,
  MixColumns, KeySchedule, `aes128_encrypt`/`_decrypt`, `aes_key_expansion`) plus
  `reverse_bits_in_bytes()`. Ausschließlich von `beninca_arc.c` genutzt (keine andere
  Unleashed-Datei bindet `aes_common.h` ein) — reine Zusatzdatei, keine Kollision mit
  irgendetwas Bestehendem.
- `common.c`/`.h` — ein durch PR #1116 ("SubGHz + Storage: share more duplicated bodies
  (-4,892 B of flash)") eingeführter Satz gemeinsamer Decoder-/Encoder-Vtable-Funktionen
  (`subghz_protocol_decoder_common_alloc/_free/_reset/_get_hash_data/_serialize...`,
  Encoder-Äquivalente), den mehrere neuere Protokolle per `offsetof`-Static-Assert
  (`SUBGHZ_ASSERT_DECODER_COMMON_LAYOUT`) gegen ihre eigene Struct-Reihenfolge abgleichen.
  Reiner Flash-Spar-Refactor von Unleashed, **kein** Bestandteil meiner `mntm-012`-Basis.

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `beninca_arc.c` benutzte HAL-/Lib-Symbol einzeln gegen meinen `mntm-012`-Tag-Stand
geprüft (Headerdiff, nicht nur Namensgleichheit):

- `lib/subghz/blocks/{decoder,encoder,const,math,custom_btn_i}.h` — **byteidentisch** zu
  Unleashed (keine Diff-Zeilen). `subghz_protocol_blocks_add_bit()`, `bit_read()`,
  `DURATION_DIFF`, `level_duration_make/_reset`, `subghz_custom_btn_get/_get_original/
  _set_original/_set_max` — alle Signaturen und Struct-Layouts (`SubGhzBlockDecoder`,
  `SubGhzProtocolBlockEncoder`) passen 1:1.
- `lib/subghz/blocks/generic.h` — der bei Unleashed UND bei mir vorhandene Teil
  (`SubGhzBlockGeneric`-Struct mit `data`/`data_2`/`serial`/`btn`/`cnt`/`seed`/
  `data_count_bit`, `subghz_block_generic_serialize/_deserialize/
  _deserialize_check_count_bit`) ist identisch. Unleashed hat **zusätzlich** eine komplette
  `subghz_block_generic_global`-Infrastruktur (Counter-/Button-Override für eine
  Momentum/Unleashed-eigene Editor-UI: `cnt_is_available`, `btn_is_available`,
  `subghz_block_generic_global_counter_override_get/_set`, `..._button_override_get/_set`,
  `endless_tx`) ergänzt, die bei mir **komplett fehlt**. `beninca_arc.c` nutzt das in zwei
  Stellen: im Encoder beim Zähler-/Tasten-Override, im Decoder-`get_string()` beim Push von
  Zähler/Taste in die globale Struktur für die Editor-UI. Siehe "Risiko"/"Port-Skizze" zur
  Anpassung.
- `lib/subghz/subghz_keystore.h`/`subghz_keystore_i.h` — **byteidentisch**.
  `subghz_environment_get_keystore()`, `subghz_keystore_get_data()`, `SubGhzKeyArray_t`,
  `M_EACH`-Iteration über `manufacture_code->type`/`->key` — alles 1:1 vorhanden.
- `lib/subghz/types.h` — die von `beninca_arc.c` genutzten Flags/Typen
  (`SubGhzProtocolTypeDynamic`, `SubGhzProtocolFlag_433/_AM/_Decodable/_Load/_Save/_Send`)
  sind bei mir unverändert vorhanden. Meine `SubGhzProtocol`-Struct hat zusätzlich ein
  `.filter`-Feld (`SubGhzProtocolFilter`-Enum), das Unleashed inzwischen entfernt hat —
  unkritisch, bei einem Designated-Initializer-Literal bleibt es einfach auf 0, wie es
  `faac_slh.c` bei mir bereits genauso handhabt (kein `.filter` gesetzt).
- `lib/subghz/protocols/base.h` (`SubGhzProtocolDecoder`-Vtable) — meine Version hat
  zusätzlich `get_hash_data_long`/`get_string_brief`-Felder, die Unleashed entfernt hat.
  Gegenprobe in `subghz_protocol_decoder_base_get_hash_data_long()`/
  `_get_hash_data()` (`protocols/base.c`): beide Pfade fallen bei `NULL` ohne Crash auf die
  jeweils andere Variante zurück bzw. liefern 0 — ein Protokoll, das wie meine Portierung
  nur `.get_hash_data` setzt, funktioniert unverändert, kein Null-Pointer-Risiko.
- `lib/flipper_format/flipper_format.h` — `flipper_format_rewind/_read_hex/_write_hex/
  _update_hex/_read_uint32/_insert_or_update_hex` — alle bei mir vorhanden, exakt wie von
  Unleashed benutzt.

**Fazit:** Bis auf die beiden fehlenden Hilfsdateien (`aes_common.*`, `common.*`) und die
fehlende `subghz_block_generic_global`-Infrastruktur ist jedes von `beninca_arc.c` verwendete
Symbol 1:1 kompatibel. `aes_common.*` kann unverändert übernommen werden (reine Zusatzdatei,
keine Fremdabhängigkeit). `common.*` übernehme ich bewusst **nicht** — stattdessen portiere
ich `beninca_arc.c` im bei mir etablierten Stil (jedes Protokoll definiert seine eigenen
`_alloc/_free/_reset/_stop/_yield`-Funktionen direkt, siehe `faac_slh.c`/`keeloq.c`), was dem
in `superrollo-gw60-keeloq-port.md` begründeten Prinzip folgt: Idee übernehmen, Struktur an
die eigene Basis anpassen statt den fremden Diff blind zu übertragen. Den
Counter-/Tasten-Override-Aufruf ersetze ich durch das bei mir bereits überall verwendete
direkte Muster `instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult()` (siehe
`keeloq.c:191`, `star_line.c:136`, `faac_slh.c:242` — exakt dasselbe Muster, `_global`-Variante
existiert dort schlicht nicht). Den reinen Editor-UI-Push in `get_string()`
(`subghz_block_generic_global.cnt_is_available = true` etc.) lasse ich ebenfalls weg — eine
bei mir nicht vorhandene UI-Komfortfunktion, keine Kernfunktionalität des Decoders.

## Kryptografische Prüfung

Die komplette AES-128-Implementierung aus `aes_common.c` (SBox/Inv-SBox, `ShiftRows`/
`_inv`, `MixColumns`/`_inv` inkl. `gf_mul2()`-GF(2⁸)-Multiplikation, 11-Runden-KeySchedule mit
`RotWord`/`SubWord`/`Rcon`) Zeile für Zeile gegen den FIPS-197-Algorithmus nachgerechnet
(ShiftRows-Permutation, MixColumns-Koeffizienten `{2,3,1,1}`/Inverse `{14,11,13,9}`,
Rcon-Tabelle `01,02,04,...,36`) — **keine Abweichung gefunden**. Auf dem Host zusätzlich
gegen den offiziellen FIPS-197-Beispielvektor (Key `000102...0e0f`, Klartext
`00112233...ccddeeff`, erwarteter Chiffretext `69c4e0d8...70b4c55a`) verifiziert: Encrypt
und Decrypt liefern exakt den erwarteten Wert (siehe Host-Repro Test 1). Das ist eine von
Unleashed unabhängige Quelle, kein Vergleich nur gegen die eigene Transkription.

Die protokollspezifische Schlüsselableitung (`get_subghz_protocol_beninca_arc_aes_key()` —
ein 64-Bit-Herstellerschlüssel aus dem Keystore wird in zwei 8-Byte-Hälften zerlegt, Hälfte 2
wird Dword-vertauscht und byteweise gespiegelt an Hälfte 1 angehängt) sowie Encrypt/Decrypt
der 16-Byte-Nutzlast (Serial, Button, 40-Bit-"Mini-Counter", 32-Bit-Rollcode-Zähler, fixer
16-Bit-Seed `0xAA55`, davor/danach bitweise gespiegelt per `reverse_bits_in_bytes()`) über
50 000 zufällige Encrypt→Decrypt-Rundläufe geprüft: alle Felder kommen exakt zurück (Host-Repro
Test 3).

## Fund: Rand­fall in der Decoder-Zustandsmaschine (geringe praktische Relevanz)

Kein Speicherfehler (kein OOB, kein Overflow, kein Double-Free) — die Puffer sind durchgehend
fest dimensioniert und werden vollständig beschrieben, bevor sie gelesen werden; die
Upload-Größe (3 Pakete × 258 `LevelDuration`-Einträge = 774) passt in den reservierten
800-Einträge-Puffer. Der Fund liegt stattdessen in der Erfassungslogik für die erste
64-Bit-Rahmenhälfte in `subghz_protocol_decoder_beninca_arc_feed()`
(`BenincaARCDecoderHighLevel`):

```c
if ((decode_count_bit == min_count_bit_for_found/2) && (decode_data != 0)) {
    generic.data = decode_data;   // erste Haelfte uebernehmen
    decode_data = 0;
}
```

Das `decode_data != 0`-Gate entscheidet die Übernahme der ersten Hälfte nicht allein über den
Bitzähler, sondern verlangt zusätzlich, dass die erfassten 64 Bit nicht zufällig exakt Null
sind. Sind sie es doch (bei echtem AES-Chiffretext ca. 1-zu-2⁶⁴, praktisch nie, siehe Test 4
unten), bleibt `generic.data` unverändert auf dem Wert eines **vorherigen, unabhängigen**
Pakets stehen, während `generic.data_2` trotzdem mit der neuen zweiten Hälfte überschrieben
wird und der volle Callback ausgelöst wird — eine stille Vermischung von Alt- und Neu-Daten
unter einem einzigen "vollständiges Paket empfangen"-Signal, ohne Fehleranzeige.

**Einordnung:** Die Eintrittswahrscheinlichkeit aus echtem Funkempfang ist verschwindend
gering — AES-Chiffretext ist pseudozufällig, über 200 000 echte Verschlüsselungen mit
zufälligen Nutzdaten trat `data == 0` kein einziges Mal auf (Host-Repro Test 4). Das ist kein
Krypto- oder Speicherfehler, sondern eine unnötige Abhängigkeit der Zustandsmaschine vom
Dateninhalt statt allein vom Bitzähler — der Zweig für die zweite Hälfte
(`else if (decode_count_bit == min_count_bit_for_found)`) hat genau dieses Problem nicht, weil
er ausschließlich zählerbasiert ist. Ich dokumentiere und behebe es trotzdem defensiv (siehe
Patch), stufe es aber bewusst **nicht** als Sicherheitslücke ein — dafür fehlt ein praktikabler
Weg, einen Angreifer gezielt einen Chiffretext mit erster Hälfte exakt Null erzeugen zu lassen,
ohne bereits den AES-Schlüssel zu kennen (und dann gäbe es ohnehin größere Probleme).

## Betroffene Dateien

- `lib/subghz/protocols/aes_common.c`/`.h` — **neu**, unverändert aus Unleashed übernommen
  (reine AES-128-Primitive + `reverse_bits_in_bytes()`, keine Fremdabhängigkeit außer
  `<furi.h>`/`<stdint.h>`, bereits vorhanden).
- `lib/subghz/protocols/beninca_arc.c`/`.h` — **neu**, angepasst an meinen Stil (siehe
  "Kompatibilitätsprüfung" oben): kein `common.h`, eigene `_alloc/_free/_reset/_stop/_yield`,
  direktes `furi_hal_subghz_get_rolling_counter_mult()`-Muster statt
  `subghz_block_generic_global`, kein Editor-UI-Push in `get_string()`, plus der oben
  beschriebene `half_captured`-Fix in der Decoder-Zustandsmaschine. Krypto-Kern (Encrypt/
  Decrypt/Schlüsselableitung) unverändert übernommen.
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag,
  direkt neben `faac_slh` (ebenfalls Keystore-basiertes dynamisches Protokoll).
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h` (Unleasheds
  Flash-Spar-Refactor, siehe oben — nicht Teil dieser Portierung, keine andere eigene Datei
  bräuchte sie); `subghz_protocol_beninca_arc_create_data()` (optionale Hilfsfunktion für eine
  "Add Manually"-Menüszene, bei Unleashed selbst nicht im Header deklariert und von keiner
  Vtable referenziert — reine Komfortfunktion, kein Bestandteil von Decoder/Encoder); die
  Keystore-Ressourcendatei selbst (der tatsächliche `BENINCA_ARC_KEY_TYPE`-Herstellerschlüssel
  ist Dateninhalt meiner eigenen `subghz_keystore`-Ressource, kein Code — exakt dieselbe
  Situation wie beim "Superrollo"-Normal-Learning-Schlüssel in
  `superrollo-gw60-keeloq-port.md`).

## Risiko

**Gering.** Alle sechs Patch-Dateien sind rein additiv bzw. (bei `protocol_items.{c,h}`)
eine einzelne zusätzliche Zeile — kein bestehender Code wird verändert, kein bestehendes
Protokoll ist betroffen. Die beiden neuen Hilfsdateien (`aes_common.*`) haben keine
Fremdabhängigkeit und werden von keiner anderen Datei eingebunden außer `beninca_arc.c`
selbst. Die Anpassungen gegenüber dem Unleashed-Original (eigenes Boilerplate statt
`common.h`, direktes Zähler-Inkrement statt `_global`-Override, `half_captured`-Flag statt
`decode_data != 0`) sind beim Kryptokern unverändert (siehe "Kryptografische Prüfung") und bei
der Zustandsmaschine durch den Regressionstest (Host-Repro Test 6, 20 000 zufällige Rahmen)
als verhaltensgleich zum Original belegt, außer im oben beschriebenen Randfall. Einziger nicht
host-verifizierbarer Teil: die GAP-/Timing-Erkennung in `BenincaARCDecoderStart` (feste
Werte `te_short=300µs`, `te_long=600µs`, `te_delta=155µs`, GAP `te_long*16 ± te_delta*15`) —
unverändert aus Unleashed übernommen, aber ohne eigene Hardware-Gegenprobe mit einem echten
Beninca-ARC-Sender nicht unabhängig bestätigt (siehe Testhinweis).

## Port-Skizze

1. `aes_common.c`/`.h` unverändert nach `lib/subghz/protocols/` übernehmen (Patch
   `aes_common_c.patch`/`aes_common_h.patch`).
2. `beninca_arc.c`/`.h` wie in `beninca_arc_c.patch`/`beninca_arc_h.patch` übernehmen.
3. `protocol_items.h` um `#include "beninca_arc.h"`, `protocol_items.c` um
   `&subghz_protocol_beninca_arc,` ergänzen (`protocol_items_c.patch`/`_h.patch`).
4. Bauen: `ufbt` (bzw. `fbt firmware_all`) — das ist der einzige noch fehlende
   Verifikationsschritt, den ich hier nicht nachbilden kann (kein `fbt`-Toolchain-Zugriff in
   dieser Recherche-Umgebung, siehe CONSTRAINTS Punkt 7 "Bauen/Flashen/Hardware-Test passiert
   lokal"). Jede verbleibende Struct-/Signaturabweichung fällt sofort als Compile-Fehler auf.
5. Vor dem ersten Empfangstest: eigenen `BENINCA_ARC_KEY_TYPE`(=9)-Herstellerschlüssel in der
   eigenen `subghz_keystore`-Ressource ergänzen/prüfen (siehe "Betroffene Dateien" oben) — ohne
   passenden Schlüssel liefert `get_subghz_protocol_beninca_arc_aes_key()` einen Nullschlüssel
   und Encrypt/Decrypt laufen zwar durch, liefern aber keine mit einem echten Sender
   übereinstimmenden Klartextfelder.
6. Optional, nicht Teil dieses Beitrags: `subghz_protocol_beninca_arc_create_data()` plus einer
   eigenen "Add Manually"-Menüszene für manuelle Testsignal-Erzeugung (analog zum bereits
   etablierten Muster für andere KeeLoq-Hersteller in `subghz_scene_set_type.c`).

## Testhinweis

1. `firmware-mining/repro-beninca-arc-aes-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`):
   ```
   == Test 1: AES-128 gegen den offiziellen FIPS-197-Testvektor ==
     Verschluesselung stimmt mit FIPS-197-Vektor ueberein: ja
     Entschluesselung liefert wieder den Klartext: ja
   == Test 2: AES-128 Rundlauf ueber 20000 zufaellige (Key, Block)-Paare ==
     20000/20000 Rundlaeufe liefern exakt den Original-Block zurueck
   == Test 3: Beninca-ARC Encrypt/Decrypt-Rundlauf (Protokollfelder) ==
     50000/50000 Encrypt->Decrypt-Rundlaeufe liefern Serial/Btn/Cnt/Seed/Mc exakt zurueck
   == Test 4: Wie oft sind die ersten 64 Bit echten Chiffretexts tatsaechlich 0? ==
     0/200000 echte Verschluesselungen haben data==0 (erste Rahmenhaelfte) -- erwartungsgemaess
     praktisch nie (Wahrscheinlichkeit ca. 1/2^64)
   == Test 5: Fund -- ALT verliert die erste Rahmenhaelfte, wenn deren 64 Bit zufaellig
   komplett 0 sind ==
     ALT: generic.data_2 = 1122334455667788 (korrekt), generic.data blieb bei deadbeefdeadbeef
     (Altwert eines fruiheren Pakets statt 0)
     NEU:  generic.data   = 0000000000000000 (korrekt als 0 erfasst, half_captured=1)
   == Test 6: Regression -- normale (nicht-pathologische) 128-Bit-Rahmen verhalten sich bei
   ALT und NEU identisch ==
     20000/20000 zufaellige Rahmen: ALT und NEU liefern identisches generic.data/data_2 (Fix
     aendert das Normalverhalten nicht)

   20013/20013 Checks bestanden -- ALL CHECKS PASSED
   ```
   Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, bei beiden Optimierungsstufen
   identisches Ergebnis.
2. Die sechs Patches gegen eine frische `mntm-012`-Basis anwenden (Reihenfolge egal,
   unabhängig voneinander): `git apply firmware-mining/beninca-arc-aes-subghz-port/*.patch`
   (bereits hier gegen eine unabhängige, frische Zweitkopie per `git apply --check` vorab
   verifiziert, real angewendet und byte-für-byte mit der separat editierten Fassung
   abgeglichen — identisch), danach `ufbt`/`fbt firmware_all` bauen.
3. Herstellerschlüssel in der eigenen Keystore-Ressource ergänzen (siehe "Port-Skizze" Schritt
   5), dann am Gerät: SubGHz → 433,92 MHz AM650 → Empfang. Mit einem echten Beninca-ARC-Sender
   (eigene/autorisierte Anlage) gegenprüfen, dass `beninca_arc` anspringt und die Felder
   Sn/Btn/Cnt/Mc/Fx plausible, mit einem zweiten Empfänger (zweiter Flipper oder SDR)
   gegengeprüfte Werte zeigen. **Keine fremden Aufnahmen/Captures in dieses Repo übernehmen**
   (CONSTRAINTS Punkt 2) — rein lokaler Hardwaretest.
4. Falls ein zweiter Beninca-ARC-Sender mit bekanntem Zählerstand verfügbar ist: Senden über
   den portierten Encoder und Rückempfang mit demselben Decoder gegenprüfen (Sn/Btn/Cnt müssen
   nach einem Encode→Decode-Durchlauf am Gerät exakt übereinstimmen, analog zum Host-Repro
   Test 3, aber über echten Funk statt in-Prozess).

## Quelle

- `github.com/DarkFlippers/flipperzero-firmware` (Unleashed), Tag `unlshd-093`,
  `lib/subghz/protocols/beninca_arc.c`/`.h` + `aes_common.c`/`.h`. Ursprungscommit
  `db2dc8f64` ("subghz: add beninca arc protocol", 2026-01-21, @xMasterX/@zero-mega), seitdem
  nur zwei Folgekommits (Typo-Fix, Programmier-Modus-Fix). `common.c`/`.h` eingeführt durch
  PR #1116 ("SubGHz + Storage: share more duplicated bodies"), bewusst nicht übernommen
  (siehe "Betroffene Dateien").
- `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`, als eigene Basis geprüft — alle
  betroffenen Header byteidentisch oder mit abwärtskompatiblen Erweiterungen (siehe
  "Kompatibilitätsprüfung"), `common.c`/`.h`, `aes_common.c`/`.h` und `beninca_arc.c`/`.h`
  selbst fehlen vollständig.
- AES-128-Referenz: FIPS-197 (NIST), Anhang B/C.1 (Testvektor Key
  `000102030405060708090a0b0c0d0e0f` / Klartext `00112233445566778899aabbccddeeff` /
  Chiffretext `69c4e0d86a7b0430d8cdb78070b4c55a`) — unabhängige Quelle zur Verifikation der
  transkribierten AES-Implementierung, siehe Host-Repro Test 1.
- Damit ist die `unlshd-090`-Restliste ("neue SubGHz-Protokolle listen → Port-Vorschläge") für
  diese Sitzung abgearbeitet; `allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`,
  `nord_ice`, `treadmill37` bleiben als weitere, noch nicht geprüfte Kandidaten aus demselben
  Vergleich offen (alle nutzen ebenfalls das bei mir fehlende `common.h` — gleiche
  Anpassungsarbeit wie hier nötig, `jarolift` zusätzlich mit KeeLoq-Krypto statt AES).
