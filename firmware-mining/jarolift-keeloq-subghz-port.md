# SubGHz: Jarolift-KeeLoq-Rollcode-Protokoll fehlt komplett

## Was

Aus der seit der Beninca-ARC-Sitzung offenen Restliste des `unlshd-093`-Vergleichs
(`allstar_firefly`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` — alle bei
Unleashed vorhanden, bei mir nicht) **`jarolift.c`** aufgegriffen. Jarolift ist ein
KeeLoq-basiertes Rollcode-Protokoll für Rollladen-/Markisenantriebe des österreichischen
Herstellers Jarolift (u. a. "TDEF"-Funkhandsender). Anders als Beninca ARC bringt es keine neue
Krypto-Primitive mit (es nutzt dieselbe `keeloq_common.c`, die `keeloq.c`/`star_line.c`/
`faac_slh.c` bei mir bereits verwenden) — der Reiz liegt hier in der zusätzlichen
8-Bit-"Grouping"/Kanal-Nutzlast (`data_2`, separat vom 64-Bit-KeeLoq-Rahmen übertragen), die
keines meiner bisherigen KeeLoq-Protokolle hat, und — wie sich beim Verifizieren
herausstellte — in einem echten Dekodier-Fehler im Unleashed-Original selbst (siehe "Fund"
unten).

Quelle bei Unleashed (Tag `unlshd-093`): `lib/subghz/protocols/jarolift.c`/`.h`, laut
Datei-Kommentar "01.2026 - @xMasterX (MMX) & d82k & Steffen (@bastelbudenbuben.de)" — eine der
neueren Dateien im `unlshd-093`-Vergleich.

## Kompatibilitätsprüfung (API 87.1 / f7)

Jedes von `jarolift.c` benutzte HAL-/Lib-Symbol einzeln gegen meinen `mntm-012`-Tag-Stand
geprüft:

- `lib/subghz/blocks/{decoder,encoder,const,math,custom_btn_i}.h` — **byteidentisch** zu
  Unleashed. Insbesondere `custom_btn_i.h` (dort deklariert: `subghz_custom_btn_set_original()`,
  `subghz_custom_btn_set_max()`) ist 1:1 vorhanden, `custom_btn.c`/`.h` ebenfalls byteidentisch
  — die komplette Tasten-Remap-Infrastruktur, die `jarolift.c` für seine
  Up/Down/Stop/Learn-Tasten nutzt, brauchte keine Anpassung.
- `lib/subghz/blocks/generic.h` — der gemeinsame Teil (`SubGhzBlockGeneric` mit `data`/
  `data_2`/`serial`/`btn`/`cnt`/`seed`/`data_count_bit`,
  `subghz_block_generic_serialize/_deserialize/_deserialize_check_count_bit`) ist identisch.
  Wie schon bei Beninca ARC dokumentiert fehlt mir die komplette
  `subghz_block_generic_global`-Override-Infrastruktur (Editor-UI für Zähler-/Tasten-Override).
  `jarolift.c` nutzt sie an zwei Stellen: im Encoder (`gen_data()`, Override von `btn`/`cnt`)
  und im Decoder-`get_string()` (Push von Zähler/Taste in die globale Struktur). Siehe
  "Risiko"/"Port-Skizze" zur Anpassung — exakt dasselbe Muster wie beim Beninca-ARC-Port.
- `lib/subghz/protocols/keeloq_common.h` — `subghz_protocol_keeloq_common_normal_learning()`,
  `_encrypt()`, `_decrypt()` sind alle bei mir vorhanden und **unverändert** (byteidentische
  Funktionskörper in `keeloq_common.c`, geprüft). Was fehlt: die
  `KEELOQ_LEARNING_NORMAL_JAROLIFT`-Konstante selbst — bei mir sind nur die Lerntypen 0–8
  definiert (`KEELOQ_LEARNING_UNKNOWN` … `_MAGIC_SERIAL_TYPE_3`), Unleashed hat seit mehreren
  neueren Protokoll-Ports zusätzlich 10–19 belegt (9 ist bei Unleashed selbst mit dem Kommentar
  "RESERVED" für Beninca ARC vorgemerkt).
- `lib/subghz/subghz_keystore.h`/`subghz_keystore_i.h` — **byteidentisch**.
  `subghz_environment_get_keystore()`, `subghz_keystore_get_data()`, `SubGhzKeyArray_t`,
  `M_EACH`-Iteration über `manufacture_code->type`/`->key` — alles 1:1 vorhanden.
- `lib/flipper_format/flipper_format.h` — `flipper_format_rewind/_read_hex/_update_hex/
  _insert_or_update_hex/_read_uint32` — alle bei mir vorhanden, exakt wie von Unleashed
  benutzt.
- `lib/subghz/protocols/common.c`/`.h` — wie bei Beninca ARC bereits dokumentiert: bei mir
  **komplett nicht vorhanden** (Unleasheds PR-#1116-Flash-Spar-Refactor). `jarolift.c` nutzt
  daraus `SUBGHZ_ASSERT_DECODER_COMMON_LAYOUT`/`_ENCODER_GENERIC_LAYOUT` (reine
  Compile-Zeit-`_Static_assert`-Prüfungen, keine Laufzeitlogik),
  `subghz_protocol_encoder_common_alloc/_free/_stop/_yield`,
  `subghz_protocol_decoder_common_free/_reset`, `subghz_protocol_decoder_common_serialize_data_2`
  und `subghz_protocol_common_append_data_2`.

**Fazit:** Bis auf die fehlende Lerntyp-Konstante, die fehlende `_global`-Override-
Infrastruktur und die fehlenden `common.*`-Helfer ist jedes von `jarolift.c` verwendete Symbol
1:1 kompatibel. Ich übernehme `common.*` bewusst **nicht** — stattdessen portiere ich
`jarolift.c` im bei mir etablierten Stil (eigene `_alloc/_free/_reset/_stop/_yield`-Funktionen
pro Protokoll, siehe `faac_slh.c`), wie bereits beim Beninca-ARC-Port begründet. Den
`subghz_protocol_decoder_common_serialize_data_2()`-Aufruf ersetze ich durch eine eigene
`subghz_protocol_decoder_jarolift_serialize()`, die exakt dieselben Bytes schreibt wie
Unleasheds `subghz_protocol_common_append_data_2()` (Byte-für-Byte-Transkription, siehe
Host-Repro Test 5). Den Zähler-Override in `gen_data()` ersetze ich durch das bei mir
etablierte direkte `furi_hal_subghz_get_rolling_counter_mult()`-Muster (identisch zu
`keeloq.c`/`star_line.c`/`faac_slh.c`/meinem Beninca-ARC-Port) — der Zweig, der ausgeführt
wird, wenn kein Override aktiv ist (bei mir immer der Fall), bleibt dabei bitidentisch zum
Unleashed-Original. Den Editor-UI-Push in `get_string()` lasse ich weg, wie schon beim
Beninca-ARC-Port.

## Kryptografische Prüfung

Reine Wiederverwendung des bei mir bereits mehrfach verifizierten KeeLoq-Kerns
(`subghz_protocol_keeloq_common_encrypt/_decrypt/_normal_learning`) — keine neue Primitive,
daher hier keine erneute FIPS-/Testvektor-Prüfung nötig. Stattdessen den **protokollspezifischen
Teil** geprüft: die Rahmen-Zusammensetzung (`hop_decrypted = (seed>>8)<<24 | (serial&0xFF)<<16 |
cnt`, `fix = btn<<60 | (serial&0xFFFFFFF)<<32 | encrypt(hop_decrypted, normal_learning(serial,
key))`, `data = reverse64(fix)`, `data_2 = reverse8(seed&0xFF)`) sowie ihr Gegenstück in
`subghz_protocol_jarolift_remote_controller()` über 50 000 zufällige
Serial/Button/Counter/Seed-Kombinationen mit einem synthetischen Test-Herstellerschlüssel
Encode→Decode durchgerechnet (kein echter Jarolift-Schlüssel verwendet oder benötigt — reine
Bit-Konsistenzprüfung der Rahmenlogik, Host-Repro Test 3). Dabei fiel der folgende Fund auf.

## Fund: Unleashed selbst verliert das untere Byte des Grouping-/Seed-Felds beim Dekodieren

Beim ersten Durchlauf von Test 3 (Encode→Decode-Rundlauf) schlug der Vergleich für **jede**
zufällige `seed`, deren unteres Byte nicht zufällig `0x00` war, fehl — `btn`/`serial`/`cnt`
kamen immer exakt zurück, nur `seed` war systematisch falsch. Das führte auf folgende Zeile in
Unleasheds eigener `subghz_protocol_jarolift_remote_controller()`:

```c
uint32_t group = subghz_protocol_blocks_reverse_key(instance->data_2, 8);
// ...
instance->seed = ((decrypt >> 24) << 8) | (group >> 8);
```

`data_2` wird beim Encodieren als `reverse8(seed & 0xFF)` erzeugt; `reverse8(reverse8(x))` ist
die Identität auf 8 Bit, also ist `group` nach dem Decodieren exakt wieder `seed & 0xFF` — ein
Wert, der ausschließlich in Bit 0–7 liegt. `group >> 8` ist für einen solchen Wert **immer 0**,
unabhängig vom tatsächlichen Inhalt. Richtig wäre `| group` ohne die zusätzliche
Rechtsschiebung — das untere Byte steht in `group` bereits an der richtigen Stelle (Bit 0–7),
genauso wie es das obere Byte `(decrypt >> 24)` an Bit 0–7 verlangt, bevor es per `<< 8` an
seine Zielposition Bit 8–15 geschoben wird.

**Praktische Relevanz: kein reiner Anzeigefehler.** `instance->generic.seed` ist nicht nur das
in `get_string()` angezeigte "Group"-Feld — es ist auch die einzige Quelle, aus der
`subghz_protocol_encoder_jarolift_get_upload()` beim erneuten Senden (egal ob unverändert
erneut gesendet oder mit Tasten-Remap über `subghz_custom_btn_set()`) `data_2` neu berechnet
(`subghz_protocol_jarolift_remote_controller()` wird in der Encoder-`deserialize()` direkt vor
`get_upload()` aufgerufen, siehe `jarolift.c`). Mit dem Unleashed-Original würde also **jedes**
geladene und erneut gesendete Jarolift-Signal, dessen echtes Grouping-Byte nicht zufällig schon
auf `0x00` endet, mit einem falschen (auf das untere Byte Null gesetzten) Kanal-/Grouping-Byte
gesendet — der Rollladen-/Markisenantrieb, der auf einen bestimmten Kanal eingelernt ist, würde
auf das erneut gesendete Signal nicht reagieren, obwohl Taste/Seriennummer/Zähler exakt stimmen.
Mit zufälligen Seeds bestätigt Test 4: die Unleashed-Formel trifft den echten Wert nur in
84 von 20 000 Fällen (≈ 1-zu-238, nahe am für ein zufälliges unteres Byte erwarteten 1-zu-256 —
die seltenen Treffer sind genau die Fälle, in denen das echte untere Byte ohnehin `0x00` war),
meine korrigierte Fassung in 20 000 von 20 000.

**Einordnung:** Kein Speicherfehler, keine Sicherheitslücke — eine funktionale Dekodier-/
Wiederaussendungs-Regression, die ausschließlich die Nutzbarkeit beim erneuten Senden
betrifft (Entschlüsselung von Taste/Seriennummer/Zähler bleibt korrekt, nur das separat
übertragene Grouping-Byte geht verloren). Ich übernehme die Korrektur (`| group` statt
`| (group >> 8)`) in meinen Port und dokumentiere die Abweichung vom Original direkt als
Kommentar an der betroffenen Stelle in `jarolift.c`.

## Betroffene Dateien

- `lib/subghz/protocols/jarolift.c`/`.h` — **neu**, angepasst an meinen Stil (siehe
  "Kompatibilitätsprüfung" oben): kein `common.h`, eigene `_alloc/_free/_reset/_stop/_yield`,
  eigene `subghz_protocol_decoder_jarolift_serialize()` statt
  `subghz_protocol_decoder_common_serialize_data_2()`, direktes
  `furi_hal_subghz_get_rolling_counter_mult()`-Muster statt `subghz_block_generic_global`,
  kein Editor-UI-Push in `get_string()`, plus der oben beschriebene Seed-Byte-Fix in
  `subghz_protocol_jarolift_remote_controller()`. KeeLoq-Kern (Encrypt/Decrypt/
  Normal-Learning) unverändert wiederverwendet, nicht dupliziert.
- `lib/subghz/protocols/keeloq_common.h` — um
  `#define KEELOQ_LEARNING_NORMAL_JAROLIFT 10u` ergänzt (bewusst **nicht** `9u`, das bereits der
  noch nicht übernommene `BENINCA_ARC_KEY_TYPE`-Vorschlag aus
  `firmware-mining/beninca-arc-aes-subghz-port/` für sich reserviert — ein Kommentar an der
  Definition verweist darauf, damit keine stille Typkollision entsteht, falls beide Patches
  irgendwann lokal angewendet werden).
- `lib/subghz/protocols/protocol_items.c`/`.h` — je ein zusätzlicher Include/Registry-Eintrag.
- Bewusst **nicht** portiert: `lib/subghz/protocols/common.c`/`.h` (nicht Teil dieser
  Portierung, keine andere eigene Datei bräuchte sie); `subghz_protocol_jarolift_create_data()`
  (optionale "Add Manually"-Hilfsfunktion, bei Unleashed selbst nicht im Header deklariert und
  von keiner Vtable referenziert); die Keystore-Ressourcendatei selbst (der tatsächliche
  Jarolift-Herstellerschlüssel ist Dateninhalt meiner eigenen `subghz_keystore`-Ressource, kein
  Code — exakt dieselbe Situation wie beim Beninca-ARC-Port).

## Risiko

**Gering.** Beide Patch-Dateien (`jarolift_c`/`_h`) sind neue, eigenständige Dateien — kein
bestehender Code wird verändert, kein bestehendes Protokoll ist betroffen.
`protocol_items.{c,h}` bekommen je eine einzelne zusätzliche Zeile, `keeloq_common.h` eine
einzelne zusätzliche Konstante mit Kollisionshinweis-Kommentar. Der KeeLoq-Kern wird
unverändert wiederverwendet (keine Modifikation an einer von mehreren Protokollen gemeinsam
genutzten Datei). Die Anpassungen gegenüber dem Unleashed-Original (eigenes Boilerplate statt
`common.h`, direktes Zähler-Inkrement statt `_global`-Override, kein Editor-UI-Push) sind durch
den Regressionsteil des Host-Repros als verhaltensgleich zum unveränderten Pfad belegt; die
einzige bewusste *inhaltliche* Abweichung ist der oben dokumentierte, durch Test 4 belegte
Seed-Byte-Fix. Einziger nicht host-verifizierbarer Teil: die Timing-/GAP-Erkennung in
`subghz_protocol_decoder_jarolift_feed()` (feste Werte `te_short=400µs`, `te_long=800µs`,
`te_delta=167µs`, Header-GAP `te_long*5 ± te_delta*6`) — unverändert aus Unleashed übernommen,
ohne eigene Hardware-Gegenprobe mit einem echten Jarolift-Sender nicht unabhängig bestätigt.

## Port-Skizze

1. `jarolift.c`/`.h` wie in `jarolift_c.patch`/`jarolift_h.patch` nach
   `lib/subghz/protocols/` übernehmen.
2. `keeloq_common.h` um die Lerntyp-Konstante ergänzen (`keeloq_common_h.patch`).
3. `protocol_items.h` um `#include "jarolift.h"`, `protocol_items.c` um
   `&subghz_protocol_jarolift,` ergänzen (`protocol_items_c.patch`/`_h.patch`).
4. Bauen: `ufbt` (bzw. `fbt firmware_all`) — einziger hier nicht nachbildbarer
   Verifikationsschritt (kein `fbt`-Toolchain-Zugriff in dieser Recherche-Umgebung, siehe
   CONSTRAINTS Punkt 7). Jede verbleibende Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf.
5. Vor dem ersten Empfangstest: eigenen Jarolift-Herstellerschlüssel
   (`KEELOQ_LEARNING_NORMAL_JAROLIFT`-Typ) in der eigenen `subghz_keystore`-Ressource ergänzen —
   ohne passenden Schlüssel liefert `subghz_protocol_keeloq_common_normal_learning()` zwar ein
   Ergebnis, aber die anschließende `((decrypt >> 16) & 0xFF) == (serial & 0xFF)`-Prüfung
   schlägt mit einem fremden/leeren Schlüssel praktisch immer fehl (kein Fehlzustand, lediglich
   kein Dekodier-Treffer).
6. Falls später der `BENINCA_ARC_KEY_TYPE`-Vorschlag ebenfalls angewendet wird: beide Patches
   sind unabhängig voneinander anwendbar (disjunkte Dateien, bis auf `keeloq_common.h` und
   `protocol_items.{c,h}`, wo beide nur additive, nicht überlappende Zeilen einfügen) — vorab
   per `git apply --check` in beiden Reihenfolgen geprüft, kein Konflikt.

## Testhinweis

1. `firmware-mining/repro-jarolift-keeloq-subghz-port/repro.c` host-kompilieren/ausführen:
   ```
   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o0 && ./repro_o0
   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
       repro.c -o repro_o1 && ./repro_o1
   ```
   Tatsächliche Ausgabe (identisch bei `-O0` und `-O1`):
   ```
   [1/6] KeeLoq cipher round trip: 20000 checks so far, 0 failures
   [2/6] normal_learning round trip: 40000 checks so far, 0 failures
   [3/6] encode->decode round trip (fixed seed expression): 50000/50000 exact matches
   [4/6] seed low-byte bug demonstration: fixed=20000/20000 exact, upstream(buggy)=84/20000 exact (expected ~1/256 by chance)
   [5/6] 'Data' hex field byte-packing round trip: 130003 checks so far, 0 failures
   [6/6] false-accept rate under wrong key: 729/200000 (0.00364, expected ~0.00391)

   ALL CHECKS PASSED: 130004 checks, 0 failures
   ```
   Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis bei beiden
   Optimierungsstufen.
2. Nach `ufbt`-Build und Flash: SubGHz → Rohfunkempfang → 433,92 MHz AM650 gegen einen echten
   Jarolift-Sender (eigenes/autorisiertes Gerät) — Protokoll sollte als "Jarolift" erkannt
   werden, `Sn`/`Btn`/`Cnt` sollten über mehrere Tastendrücke hinweg konsistent (Cnt
   monoton steigend) sein.
3. **Gezielter Regressionstest für den Seed-Fix:** eine aufgezeichnete Datei laden und erneut
   senden (unverändert, ohne Tasten-Remap) — vor dem Fix (Unleashed-Original) würde das
   gesendete Grouping-/Kanalbyte bei jedem Sender mit einem vom Null-Byte abweichenden echten
   Kanalwert nicht mehr mit dem ursprünglich aufgezeichneten Signal übereinstimmen und der
   Antrieb würde nicht reagieren; mit dem Fix sollte das erneut gesendete Signal den Antrieb
   wie beim Original-Sender auslösen. Ohne eigenen Jarolift-Antrieb nicht selbst nachstellbar —
   daher nur der Host-Repro-Nachweis (Test 3/4) als unabhängiger Beleg beigelegt.
