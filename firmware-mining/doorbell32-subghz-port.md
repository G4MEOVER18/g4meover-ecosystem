# SubGHz: Doorbell32 als neues, eigenständiges Protokoll portiert

## Was

Unleashed-Firmware hat seit dem letzten Tag `unlshd-093` (aktuell weiterhin die neueste
Release-Markierung, keine neuere `unlshd-09x`/`unlshd-1xx` vorhanden) auf dem `dev`-Branch
mehrere neue, noch unveröffentlichte SubGHz-Protokolle angesammelt. Gegen den aktuellen
`dev`-Stand (Commit `af2e7649267c7ce820e9ba05cdedb74197c0ab60`, 2026-10-11) per
`git diff unlshd-093 af2e764` verglichen, um zu sehen, was seit dem letzten Release
tatsächlich neu ist, statt bereits bekannten Umfang zu wiederholen: 138 geänderte Dateien,
+7201/-802 Zeilen, darunter zwei komplett neue, eigenständige SubGHz-Protokolldateien
(`doorbell32.c/.h`, `keyfinder2.c/.h`), eine neue NFC-Protokollfamilie (`texkom`, 6 Dateien),
ein neues LFRFID-Werkzeugmodul (`lib/lfrfid/tools/hitags.c/.h`, 959+77 Zeilen) sowie
Erweiterungen an bereits von mir portierten Protokollen (`nord_ice.c`, `faac_slh.c`,
`prastel.c`, `keeloq.c`).

Von diesen neuen Kandidaten `doorbell32.c`/`.h` gewählt: mit 247+71 Zeilen die kleinste,
vollständig eigenständige Einheit (eine einzelne Protokolldatei + 2-Zeilen-Registrierung,
kein mehrteiliger Port wie `texkom`/`hitags`), ein reines statisches Protokoll ohne
Rolling-Code/Kryptografie — passt in eine Sitzung, rein host-verifizierbar (Encode/Decode,
kein Karten-/Luftschnittstellen-Zustand).

Doorbell32 ist laut dem eigenen Kommentar im Unleashed-Quelltext ein Princeton-artiges
1:3-PWM-Protokoll (dieselbe Bit-Kodierung wie mein bereits vorhandenes `princeton.c`), aber
mit 32 statt 24 Nutzbits und einem 14×Te- statt 31×Te-Guard — weshalb mein bestehender
Princeton-Decoder solche Rahmen zwar auf Bit-Timing-Ebene erkennen, aber nie vollständig
dekodieren würde (er synchronisiert auf einen 36×Te-Guard, der bei Doorbell32 nie auftritt;
`PRINCETON_GUARD_TIME_DEFALUT=30`, Toleranz skaliert mit, trifft 14×Te nicht). Vermutlich
batteriebetriebene Funktürklingeln/Glocken (Name und `SubGhzProtocolFlag_Sensors`-Flag bei
Unleashed deuten auf die "Sensoren"-Kategorie, in der Unleashed auch Wetterstationen/Klingeln
einordnet).

## Kompatibilitätsprüfung gegen `mntm-012`

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag-Commit
`b359f6fc32f282ce7217dd2d48a4e3478771852e`) geprüft, nicht nur gegen die Unleashed-Quelle
allein:

- `../blocks/const.h`, `decoder.h`, `encoder.h`, `generic.h`, `math.h` sowie
  `lib/subghz/types.h` (`SubGhzProtocolDecoder`/`SubGhzProtocolEncoder`-Vtables) sind bei mir
  strukturell identisch zur Unleashed-Quelle — alle von Doorbell32 verwendeten Feldnamen/
  Typsignaturen (`SubGhzBlockDecoder.{parser_step,te_last,decode_data,decode_count_bit}`,
  `SubGhzBlockGeneric.{protocol_name,data,data_count_bit}`, `level_duration_make/_reset`,
  `bit_read`, `DURATION_DIFF`, `subghz_protocol_blocks_add_bit`,
  `subghz_block_generic_deserialize_check_count_bit`) passen unverändert.
- **Fehlt bei mir, wie bei jedem bisher aus Unleashed portierten SubGHz-Protokoll bereits
  dokumentiert** (Jarolift/KeyFinder/Nord-ICE/Treadmill37/Allstar-Firefly/Ditec-GOL4/
  Superrollo): die gemeinsame `lib/subghz/protocols/common.c/.h`-Hilfsschicht
  (`subghz_protocol_decoder_common_alloc/_free/_reset/_get_hash_data/_serialize_te`,
  `subghz_protocol_encoder_common_alloc/_free/_stop/_yield`) sowie die beiden
  `SUBGHZ_ASSERT_DECODER_TE_LAYOUT`/`SUBGHZ_ASSERT_ENCODER_GENERIC_LAYOUT`-Makros, auf denen
  diese Hilfsschicht beruht (reine Offset-Sanity-Checks für ihre eigenen generischen
  Zugriffe — bei mir ohnehin gegenstandslos, da ich unten ausschließlich eigene,
  typkorrekte Zugriffsfunktionen schreibe statt generischer Offset-Casts). Gelöst nach dem
  bei den oben genannten sechs Ports bereits etablierten Muster: eigene
  `_alloc`/`_free`/`_reset`/`_get_hash_data`/`_stop`/`_yield`-Funktionen 1:1 nach dem Vorbild
  meines eigenen, bereits funktionierenden `princeton.c` (gleiche 1:3-PWM-Familie,
  `malloc`/`free` statt generischer Helfer, `.get_hash_data` kurze Form statt
  `.get_hash_data_long`, siehe dort `subghz_protocol_decoder_princeton_get_hash_data()` als
  Vorlage für die hier verwendete `subghz_protocol_blocks_get_hash_data()`-Variante).
- **`SubGhzProtocolFlag_Sensors` existiert in meinem `SubGhzProtocolFlag`-Enum nicht** (bei
  mir endet die Flag-Liste bei `SubGhzProtocolFlag_BinRAW`). Bewusst weggelassen statt
  erfunden — reine UI-Kategorisierung (Filterung in der "Sensoren"-Rubrik, die es bei mir in
  dieser Form nicht gibt), ohne jede Auswirkung auf Encode/Decode/Senden/Empfangen.
- `lib/subghz/SConscript` globbt `protocols/*.c*` rekursiv — keine Build-Datei-Änderung
  nötig, nur die zwei neuen Quelldateien plus die zwei Registrierungszeilen.
- Registrierung in `protocol_items.h`/`.c` 1:1 nach demselben Muster wie bei allen bisher
  portierten Protokollen (Include + Array-Eintrag ans Dateiende angehängt).

## Fund: Decoder braucht mindestens drei Wiederholungen, nicht zwei

Kein Fehler, den ich verändert habe — unverändertes, 1:1 aus Unleashed übernommenes
Verhalten, hier für meine eigene Basis gegengerechnet und dokumentiert (dieselbe Einordnung
wie bei den bereits dokumentierten GAP-Doppelnutzungs-Funden für Nord ICE/Treadmill37/
Allstar Firefly/Ditec GOL4, aber eine eigene, strengere Ausprägung):

Der Decoder verlässt `Reset` nur über eine LOW-Dauer, die auf das 14×Te-Guard-Zeitfenster
passt (`te_short_KONSTANTE × 14 ± te_delta × 14/2`). Jede Dauer innerhalb eines Rahmens
(Te oder 3×Te) liegt weit darunter — die allererste Übertragung eines Rahmens wird deshalb
auf Bit-Ebene vollständig verworfen (der Decoder bleibt die ganze Zeit in `Reset`), bis der
eigene Abschluss-Guard-Puls dieses ersten Rahmens zufällig genau auf das Guard-Fenster passt
und den Decoder in `SaveDuration` synchronisiert — bereit, den **zweiten** Rahmen bitweise
zu erfassen. Dieser zweite Rahmen vervollständigt zwar 32 Bit, wird aber ebenfalls nicht
gemeldet, weil `last_data` zu diesem Zeitpunkt noch `0` ist (der Vergleich
`last_data == decode_data && last_data` schlägt fehl). Erst beim **dritten** Rahmen stimmt
`last_data` (jetzt vom zweiten Rahmen gesetzt) mit `decode_data` überein — die erste
tatsächliche Meldung erfolgt also erst hier: `found_count == max(repeat-2, 0)` statt der bei
den anderen vier Protokollen dokumentierten `ceil(repeat/2)`- bzw. `floor(repeat/2)`-Klasse.
Per Host-Repro über 20000 Zufallsnutzlasten und `repeat` 1–6 bestätigt (siehe unten).

Praktische Auswirkung: die vom Encoder gesendeten `repeat=10` Wiederholungen liefern damit
real nur 8 auswertbare Rahmen beim Gegenempfänger (statt 9), und ein eigener Mitschnitt
braucht mindestens drei zusammenhängende, vollständige Rahmen, bevor meine Firmware das
Signal überhaupt als erkannt meldet — bei kurzen/abgeschnittenen Mitschnitten (z. B. nur
die letzten zwei Wiederholungen eines längeren Sendevorgangs erfasst) kann das dazu führen,
dass ein eigentlich vorhandenes Signal nicht decodiert wird, obwohl die Rohdaten vollständig
und korrekt sind.

## Betroffene Dateien

- `lib/subghz/protocols/doorbell32.c` (312 Zeilen) / `.h` (103 Zeilen) — neu, komplett
  eigenständig, keine Abhängigkeit zu einer bei mir fehlenden Datei.
- `lib/subghz/protocols/protocol_items.h` — ein neuer `#include "doorbell32.h"`.
- `lib/subghz/protocols/protocol_items.c` — ein neuer Array-Eintrag
  `&subghz_protocol_doorbell32,`.
- Nicht betroffen: `SConscript` (Rekursiv-Glob), `subghz_scene_set_type.c`/
  `subghz_custom_event.h` (Doorbell32 ist `SubGhzProtocolTypeStatic` ohne
  Hersteller-Varianten — wie `princeton.c` braucht es keinen "Add Manually"-Menüpunkt).

Alle vier Patches gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW`
(Tag-Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`) mit `git apply --check` verifiziert,
real angewendet, Ergebnis byte-für-byte gegen die separat editierte Arbeitskopie
abgeglichen — identisch, in `firmware-mining/doorbell32-subghz-port/`.

**Reihenfolge-Hinweis für die lokale Anwendung:** `protocol_items_c.patch`/
`protocol_items_h.patch` hängen wie bei allen bisher vorgeschlagenen SubGHz-Protokoll-Ports
(Jarolift/KeyFinder/Nord-ICE/Treadmill37/Allstar-Firefly/Ditec-GOL4/Superrollo) am selben
Dateiende (`revers_rb2`/`feron`/`roger`/`elplast`-Kontext) an — werden mehrere dieser noch
nicht angewendeten Patches gemeinsam übernommen, verschiebt jeder bereits angewendete
Patch diesen Kontext (derselbe, bereits mehrfach dokumentierte Versatz). Einzeln gegen die
unveränderte Basis angewendet funktioniert jeder Patch für sich; bei mehreren zusammen
Reihenfolge beachten oder den Kontext von Hand nachziehen.

## Host-Repro

`gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined
repro.c -o repro && ./repro` bei `-O0` und `-O1` in
`firmware-mining/repro-doorbell32-subghz-port/repro.c`. Transkribiert
`subghz_protocol_encoder_doorbell32_get_upload()` und
`subghz_protocol_decoder_doorbell32_feed()` zeilengetreu, ohne Flipper-/furi-Header. Tatsächliche
Ausgabe (identisch bei beiden Optimierungsstufen):

```
73378 checks passed -- ALL CHECKS PASSED
```

Sechs Testgruppen:
1. Encode→Decode-Rundlauf über 20000 Zufalls-32-Bit-Nutzlasten, `repeat` 1–6, Bit-Exaktheit
   jeder Meldung gegen die ursprünglichen Nutzdaten geprüft, `found_count == max(repeat-2,0)`
   bestätigt (siehe Fund oben).
2. Negativtest: zwei unterschiedliche, direkt aufeinanderfolgende Nutzlasten dürfen nie
   gemeldet werden (`last_data == decode_data`-Prüfung greift auch bei zwei für sich
   wohlgeformten Rahmen).
3. Negativtest: eine verfälschte Bit-Timing-Dauer (weder Kurz/Lang- noch Lang/Kurz-Muster),
   in einen durch einen vorangehenden guten Rahmen bereits synchronisierten Rahmen injiziert,
   bricht kontrolliert nach `Reset` ab, ohne einen falschen Rahmen zu melden, und
   resynchronisiert korrekt für nachfolgende gute Rahmen — zusätzlich differentiell gegen
   denselben Vier-Rahmen-Burst ganz ohne Verfälschung geprüft (muss genau eine Meldung mehr
   liefern: 2 statt 1), um auszuschließen, dass die Zählung nur durch Zufall der
   handgerechneten Erwartungswerte übereinstimmt.
4. Negativtest: ein zu kurzer LOW-Puls, wo `Reset` den 14×Te-Guard erwartet, wird nicht
   fälschlich als Guard akzeptiert; der tatsächliche Guard direkt danach schon.
5. `te`-Rekonstruktion: nach einer erfolgreichen Meldung muss der akkumulierte/geteilte
   `te`-Wert exakt (nicht nur näherungsweise) dem beim Encodieren verwendeten Te entsprechen
   (ganzzahlige Division ohne Rest, da der Encoder ausschließlich Vielfache von `te` sendet)
   — über 22 Te-Werte von 450 bis 600us geprüft, synchron zum Zeitpunkt des
   `base.callback`-Aufrufs erfasst (nicht erst danach gelesen): der echte Decoder setzt
   `instance->te` direkt nach diesem Callback unbedingt auf 0 zurück, um ihn für den
   nächsten Rahmen neu zu akkumulieren — das ist beabsichtigtes, bereits in meinem eigenen,
   produktiven `princeton.c` identisch vorhandenes Verhalten, kein Fehler.

Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identischer Output bei `-O0`/`-O1`.

## Risiko

Gering. Rein additiv (zwei neue Dateien, zwei Zweizeiler in bereits bestehenden Dateien),
keine Änderung an einer bestehenden Funktion, kein geteilter State mit einem anderen
Protokoll, keine Kryptografie. Das einzige verhaltensrelevante Detail (drei statt zwei
Wiederholungen bis zur ersten Meldung) ist unverändertes Upstream-Verhalten und oben als
Fund dokumentiert statt stillschweigend übernommen. Die beiden bewussten Abweichungen von
der Unleashed-Quelle (eigene `_alloc`/`_free`/`_reset`/`_get_hash_data`/`_stop`/`_yield`
statt der bei mir fehlenden `common.c`-Helfer; Weglassen von `SubGhzProtocolFlag_Sensors`)
sind beide rein strukturell/kategorisierend, ohne Einfluss auf Encode-/Decode-Korrektheit —
durch den Host-Repro oben direkt gegen die reale Bit-Timing-Logik verifiziert, nicht nur
gegen sich selbst.

## Testhinweis (lokal nachzuvollziehen)

1. `firmware-mining/repro-doorbell32-subghz-port/repro.c` host-kompilieren/ausführen (Befehl
   und tatsächliche Ausgabe siehe Host-Repro oben) — reine Logikverifikation ohne Hardware.
2. Die vier Patches gegen die eigene `mntm-012`-Basis anwenden (Reihenfolge-Hinweis oben
   beachten, falls weitere noch offene SubGHz-Port-Vorschläge gleichzeitig übernommen
   werden), mit `ufbt`/`fbt firmware_all` bauen.
3. Am Gerät: SubGHz → Lesen/Read RAW oder regulärer Scan bei 433,92 MHz AM650 (laut
   Namenskonvention wahrscheinlichste Frequenz für dieses Protokoll, lokal gegen die eigene,
   autorisierte Testklingel zu bestätigen) — eigenen Testsender (autorisierte eigene
   Funktürklingel) mindestens dreimal unmittelbar hintereinander auslösen und bestätigen,
   dass "Doorbell32" als Protokoll erkannt wird, mit den erwarteten 32 Bit und einem
   plausiblen `Te`-Wert in der Anzeige.
4. Gezielt Fund nachvollziehen: Signal speichern, dann über den Signal-Editor `Repeat=1`
   erneut senden und gegenprüfen, dass der eigene zweite Empfänger (zweiter Flipper oder
   SDR) **keinen** Treffer meldet; anschließend mit `Repeat=3` erneut senden und bestätigen,
   dass jetzt genau eine Meldung erscheint.
5. Encoder-Gegenprobe: das gespeicherte Signal erneut senden und mit einem unabhängigen
   zweiten Empfänger (SDR/zweiter Flipper) auf Bitmuster/Timing gegen die Aufzeichnung
   vergleichen.

## Quelle

- `github.com/DarkFlippers/unleashed-firmware`, `dev`-Branch, Commit
  `af2e7649267c7ce820e9ba05cdedb74197c0ab60` (2026-10-11):
  `lib/subghz/protocols/doorbell32.c`/`.h` (neu seit Tag `unlshd-093`).
- `github.com/G4MEOVER18/G4MEOVER-FW`, Tag-Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, als eigene Basis geprüft (alle verwendeten
  Hilfsstrukturen vorhanden, `common.c`-Hilfsschicht und `SubGhzProtocolFlag_Sensors`
  fehlen — siehe Kompatibilitätsprüfung oben).
- `lib/subghz/protocols/princeton.c` (eigene, bereits produktive Basis) als Vorlage für die
  eigenen `_alloc`/`_free`/`_reset`/`_get_hash_data`/`_stop`/`_yield`-Funktionen.
- API-87-Einordnung: reine neue SubGHz-Protokolldatei + zwei Registrierungszeilen, keine
  neuen HAL-/API-Aufrufe gegenüber dem bereits vorhandenen, produktiv genutzten
  SubGHz-Decoder-/Encoder-Pfad — kein Kompatibilitätsrisiko über das oben dokumentierte
  Strukturelle hinaus.
