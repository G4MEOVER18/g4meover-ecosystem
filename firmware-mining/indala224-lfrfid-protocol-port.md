# Indala224 (125-kHz-Zutrittsformat, PSK2, 224 Bit): neues LFRFID-Protokoll

## Was

Ich unterstütze aktuell nur die 26-Bit-Variante von Indala (`protocol_indala26.c`, Motorola/
Indala-FlexCard-Format). Daneben gibt es eine zweite, strukturell unabhängige Indala-Variante
mit 224 Bit Nutzdaten statt 26 — dieselbe Kartenfamilie (Motorola/Indala, Langstrecken-
Zutrittskarten), aber eigener Rahmen, eigene PSK2-Phasenkodierung, eigener Dateicode. Beide
Formate teilen sich keine Funktionen (unterschiedliche Datenstrukturen, unterschiedliche
Decoder, unterschiedliche T5577-Konfiguration) — Indala224 ist ein eigenständiges, bislang
komplett fehlendes LFRFID-Protokoll, kein Fix an der bereits vorhandenen 26-Bit-Variante.

Ich habe `lib/lfrfid/protocols/` zwischen meiner eigenen Basis (`G4MEOVER18/G4MEOVER-FW`,
`main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`, Momentum `mntm-012`) und Unleashed
(`DarkFlippers/unleashed-firmware`, `dev`-Branch, Commit
`a70a69d9d47c1e0a3b1f868aa25c4d0a7a5346d5`, 2026-10-20) vollständig gegenübergestellt
(Protokolldatei-Liste, nicht nur Stichproben), um einen echten Lückenfund statt einer
Wiederholung eines bereits mehrfach geprüften Clusters zu liefern. Einziger Unterschied bei
den LFRFID-Protokollen: `protocol_indala224.{c,h}` (278 Zeilen) fehlt bei mir komplett, dazu
eine neue, von sehr vielen bereits bei mir vorhandenen Protokollen (siehe "Risiko") genutzte
Helferdatei `lfrfid_write_fill.{c,h}` (72 Zeilen), auf die `protocol_indala224.c` für seinen
T5577-Schreibpfad zurückgreift.

Ich liefere in dieser Sitzung den vollständigen, unveränderten Protokollkern
(`protocol_indala224.c/.h`) plus die eine, ebenfalls unveränderte Helferdatei
(`lfrfid_write_fill.c/.h`, nur ihre T5577-Hälfte wird von Indala224 benutzt), bewusst **ohne**
den viel größeren, unabhängigen Umbau zu übernehmen, der dieselbe Helferdatei bei zwanzig
bereits vorhandenen Protokollen nachträglich einsetzt (siehe "Risiko") — dieser Port bleibt
auf Indala224 beschränkt.

## Nutzen

- Neue, bislang komplett fehlende Kartenvariante: Indala224 ist ein 125-kHz-PSK2-Format mit
  224 Bit Nutzdaten, verwendet u. a. in Motorola/Indala-Langstrecken-Zutrittskarten (Büro-/
  Werksgelände, oft als Ergänzung zu kurzen 26-Bit-Karten für höhere ID-Kapazität). Ohne
  diesen Port liest mein Gerät solche Karten nicht (die vorhandene Indala26-Decodierung
  erkennt eine andere Rahmenstruktur und andere Bitlänge, der Rahmen wird verworfen).
- Reine Lese-/Emulations-/Beschreibfähigkeit für eigene, autorisiert getestete Karten — kein
  neues Angriffsprimitiv: wie bei Indala26 liegt die ID im Klartext im Rahmen (Proxmark3-
  dokumentiertes Format, keine Verschlüsselung, kein Authentifizierungsgeheimnis), es gibt
  nichts zu "knacken".
- Datei-/Registrierungsaufwand ist minimal (zwei neue, unabhängige Dateien plus zwei
  Zweizeiler-Ergänzungen an bereits vorhandenen Dateien, siehe "Betroffene Dateien") — kein
  mehrteiliger Port wie Texkom, da Indala224 (anders als die meisten NFC-Protokollfamilien)
  keine eigene `furi_hal`-Schicht braucht: Lesen/Senden läuft über die bereits vorhandene,
  generische LFRFID-Analogfrontend-Pipeline (dieselbe, die auch Indala26 und alle anderen
  LFRFID-Formate bereits nutzen).

## Betroffene Dateien

**Jetzt geliefert, unverändert übernommen (siehe "Verifikation"):**
- `lib/lfrfid/protocols/protocol_indala224.c` (278 Zeilen) — vollständiger Protokollkern:
  Datenstruktur (`ProtocolIndala224`, 28-Byte-Nutzdaten), PSK2-Decoder
  (`decoder_feed`/`_feed_internal`/`_can_be_decoded`/`_check_preamble`/
  `_check_inverted_preamble`/`_decoder_save`), Encoder (`encoder_start`/`_encoder_yield`),
  Renderer (`render_data`/`_render_brief_data`), T5577-Schreibpfad (`write_data`), die
  abschließende `ProtocolBase`-Struktur (`protocol_indala224`).
- `lib/lfrfid/protocols/protocol_indala224.h` (4 Zeilen) — öffentliche Deklaration
  (`extern const ProtocolBase protocol_indala224`).
- `lib/lfrfid/protocols/lfrfid_write_fill.c` (nur die T5577-Hälfte, 17 von 44 Zeilen
  tatsächlich von Indala224 erreicht; die EM4305-Hälfte unverändert mitgeliefert, da die
  Datei als Ganzes unverändert übernommen wird, aber von Indala224 nicht benutzt) /
  `lfrfid_write_fill.h` (56 Zeilen) — geteilte Hilfsfunktion `lfrfid_write_fill_t5577()`:
  liest die Blockzahl aus dem `maxblock`-Feld des Konfigurationsworts, kopiert dann genau so
  viele 32-Bit-Blöcke aus dem codierten Datenpuffer in die T5577-Blockliste. Keine
  Abhängigkeit zu einer bei mir fehlenden Datei (nutzt nur `bit_lib_get_bits_32()`, bei mir
  bereits vorhanden und — bis auf den bereits anderswo dokumentierten, hier nicht
  ausgelösten `TODO FL-3534`-Lese-Overrun bei unausgerichtetem Zugriff, siehe
  `bit-lib-get-bits-read-past-buffer-fix.md` — unverändert).

**Jetzt geliefert, additive Zweizeiler an bereits vorhandenen Dateien (siehe
"connective-patches/", "Verifikation"):**
- `lib/lfrfid/protocols/lfrfid_protocols.h` — ein neuer Enum-Wert `LFRFIDProtocolIndala224`,
  bewusst ans Ende von `LFRFIDProtocol` angehängt (nicht alphabetisch/thematisch einsortiert)
  — exakt das in der Texkom-Sitzung (Phase 4a) begründete Muster: ein exportierter Enum, eine
  frühere Einfügestelle hätte die numerischen Werte aller nachfolgenden Protokolle
  verschoben.
- `lib/lfrfid/protocols/lfrfid_protocols.c` — ein neuer `#include "protocol_indala224.h"`
  sowie eine neue Registrierungszeile `[LFRFIDProtocolIndala224] = &protocol_indala224,` in
  `lfrfid_protocols[]`, ebenfalls ans Ende angehängt. Keine Änderung an einer bestehenden
  Zeile.

**Nicht Teil dieses Ports (siehe "Risiko"):** die begleitende Umbau-Welle, die
`lfrfid_write_fill_t5577()`/`_em4305()` bei zwanzig bereits bei mir vorhandenen Protokollen
(u. a. `protocol_indala26.c` selbst, AWID, Gallagher, Keri, Pyramid, Viking, Paradox,
Noralsy, Securakey, GProxII, FDX-A/B, HID/HID-Ex, IO-Prox-XSF, Idteck, Jablotron) nachträglich
anstelle ihres jeweils eigenen, bereits vorhandenen Inline-Codes einsetzt — reine
Verhaltensgleichheit vorausgesetzt, aber zwanzig unabhängige Dateiänderungen, die mit
Indala224 selbst nichts zu tun haben und diesen Port unnötig aufblähen würden.

## Risiko

- **Niedrig** für `protocol_indala224.c/.h`: reine, additive neue Dateien, keine Änderung an
  einer bestehenden Datei, kein geteilter Zustand mit einem anderen Protokoll (anders als
  der Name suggeriert, hat Indala224 keine Codeabhängigkeit zu `protocol_indala26.c` — beide
  Dateien sind komplett unabhängig). Reine Bit-/Byte-Logik ohne Zeiger-Arithmetik über eine
  selbst nicht kontrollierte Länge: `gaps`/Schieberegister-Puffer sind beide fest
  `INDALA224_ENCODED_DATA_SIZE` (32 Byte) groß, `decoder_save()`s höchster Lesezugriff
  (`bit_lib_get_bit(data_from, i+1)` bei `i`=223) liegt mit Index 224 von 256 möglichen
  innerhalb dieses Puffers. Kein eigener Fund beim Lesen (siehe "Verifikation").
- **Niedrig** für `lfrfid_write_fill.c/.h`: ebenfalls additive neue Dateien, keine Änderung
  an einer bestehenden Datei. Die einzige Rechenstelle, die bei falscher Übernahme stillschweigend
  falsche Daten statt eines Compile-/Laufzeitfehlers produzieren könnte (Blockindex-Offset
  `(block - 1) * 32`), ist durch den Host-Test abgedeckt (siehe "Verifikation") — eine
  Mutationsprobe mit genau diesem Off-by-one-Fehler lässt den Test zuverlässig fehlschlagen.
- **Niedrig** für die beiden Zweizeiler-Patches (`lfrfid_protocols.h/.c`): rein additiv, keine
  Änderung an einer bestehenden Zeile, Enum-Wert ans Ende angehängt (kein Verschieben
  bestehender Werte). Einzige Voraussetzung ist, dass `lfrfid_protocols[]` tatsächlich per
  designated initializer (`[Enum] = &protocol`) statt positionsbasiert befüllt wird — bei mir
  der Fall (siehe Quelltext oben), ein hinten angehängter Eintrag kann daher keine bereits
  vorhandene Zuordnung verschieben.
- **Kein zusätzliches Missbrauchsrisiko**: wie Indala26 kein Authentifizierungsgeheimnis,
  Lesen/Emulieren/Schreiben eines bekannten/eigenen Schlüssels ist strukturell identisch zum
  bereits unterstützten Modell.
- **Bewusst nicht übernommen, zur Risikoabgrenzung dokumentiert:** die oben unter "Betroffene
  Dateien" genannte Umbau-Welle an zwanzig bereits vorhandenen Protokolldateien. Eine
  nachträgliche Umstellung dieser zwanzig Dateien auf `lfrfid_write_fill_*()` wäre ein eigener,
  separater Beitrag (Nutzen: weniger duplizierter Inline-Code; Risiko: zwanzig Dateien, bei
  denen jede einzelne Umstellung für sich auf Verhaltensgleichheit zu prüfen wäre) — nicht
  Teil dieser Sitzung, da hier nur Indala224 als neues Protokoll geliefert wird.
- **Lokal zu prüfen (Hardware):** die tatsächliche T5577-Beschreibbarkeit einer Karte mit acht
  Blöcken (Konfiguration + 7 Datenblöcke) ist durch den Host-Test nur auf Byte-Ebene
  abgesichert (siehe "Verifikation") — der eigentliche RF/32-PSK2-Schreibvorgang selbst läuft
  über die bereits vorhandene, generische T5577-Schreibroutine (`t5577_write()`, von keiner
  Protokolldatei neu implementiert) und ist damit kein neues Risiko dieses Ports.

## Verifikation (auf dem Host, ohne Hardware)

`protocol_indala224.c` ist reine Bit-/Byte-Logik ohne jeden `furi_hal`-Bezug (anders als
Texkom hat Indala224 — wie jedes andere LFRFID-Protokoll — keine eigene Low-Level-RF-Schicht,
die generische LFRFID-Pipeline sampelt bereits fertige Pegel-/Dauer-Paare an). Ich habe die
gesamte Datei beim Lesen auf Speicherfehler/Off-by-one geprüft (gleiches Kriterium wie bei
jedem bisherigen Protokoll-Port) und **keinen eigenen Fund** identifiziert.

Host-Repro in `firmware-mining/repro-indala224-lfrfid-protocol-port/repro.c`
(`protocol_indala224.c` sowie die T5577-Hälfte von `lfrfid_write_fill.c` unverändert
transkribiert, `bit_lib_*`/`level_duration_*`/`ProtocolBase` unverändert aus der eigenen
Basis übernommen, `FuriString`/`furi_string_printf` auf einen minimalen, aber
verhaltensgleichen `vsnprintf`-Stand-in reduziert, `FURI_LOG_D` auf No-Op), unter
`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1`: **58 Prüfungen, alle bestanden**, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

Die beiden Haupt-Testvektoren (`INDALA224_TEST_DATA`, `INDALA224_ALT_TEST_DATA`) sind nicht
selbst konstruiert, sondern unverändert aus Unleasheds eigenem Unit-Test übernommen
(`applications/debug/unit_tests/tests/lfrfid/lfrfid_protocols.c`,
`test_lfrfid_protocol_indala224_roundtrip`/`_alternating_phase`), die laut dortigem Kommentar
gegen eine reale Proxmark3-Ausgabe verifiziert sind — ich vertraue dieser Herkunft aber nicht
blind, sondern habe sie durch eine eigene, unabhängige Repro-Umgebung (direkter Aufruf der
Protokollfunktionen statt über `protocol_dict`/`pulse_glue`, zusätzliche eigene
Negativ-/Grenzfall-Szenarien, ASan/UBSan bei zwei Optimierungsstufen, siehe unten) erneut
durchlaufen lassen.

1. **PSK2-Rundlauf über den öffentlichen Dispatch** (`encoder_start`/`_encoder_yield` → Pegel
   am Anfang jeder 32-Yield-Bitperiode abgetastet, exakt die Technik aus Unleasheds eigenem
   Test, da die PSK-Encoder-Ausgabe mit Dauer 1 pro Halbzyklus feiner ist als die vom Decoder
   erwartete Abtastgranularität → Lauflängen gebildet → `decoder_feed`), für beide
   Testvektoren: Rahmen bit-exakt wiederhergestellt. Je zweiter, unabhängiger Decode-
   Lebenszyklus auf derselben, bereits erfolgreich decodierten Instanz direkt im Anschluss
   ebenfalls erfolgreich und bit-exakt — kein Zustand bleibt zwischen zwei Decode-Aufrufen
   hängen.
2. **Phasenwechsel-Flag**: `INDALA224_ALT_TEST_DATA` (eine ungerade Zahl 1-Bits — genau das
   führende Präambel-Bit) erzeugt nachweislich eine zum ersten Rahmen invertierte
   PSK2-Startphase im zweiten Rahmen (`levels[0] != levels[224]`) — belegt, dass
   `_check_inverted_preamble()` tatsächlich den Pfad abdeckt, für den er geschrieben ist, statt
   nur ungetestet im Quelltext zu stehen.
3. **Müllzug gefolgt von gültigem Zug im selben, durchgehenden Bitstrom** (50 zufällige
   Pegelwechsel direkt vor einem gültigen Rahmen, in einem einzigen fortlaufenden Decoder-
   Lebenszyklus): der gültige Rahmen wird trotzdem korrekt erfasst — das Schieberegister lässt
   den vorherigen Müll beim Weiterschieben folgenlos herausfallen (dasselbe Muster wie bei
   Texkom Phase 4b).
4. **Rechnerisches Minimum, als Kontrollprobe für die beiden folgenden Negativfälle**: das
   256-Bit-Schieberegister wird bei `decoder_start()` mit Nullen vorbelegt; ein echtes Bit
   steht an Fensterposition 0 erst, nachdem exakt 256 reale Bit gepusht wurden (danach ist das
   allererste gepushte Bit bereits wieder herausgeschoben) — der einzige Moment, an dem
   Fensteranfang (Position 0, Rahmen 1) und Fensterende (Position 224, Rahmen 2 — 224 Bit
   später dieselbe Rahmengrenze) gleichzeitig auf echten Daten liegen, ist rechnerisch
   `Gesamtzahl gepushter Bits == 256`. Empirisch bestätigt: bei exakt 256 gepushten Bit
   decodiert ein unveränderter Strom bereits erfolgreich.
5. **Negativ, ein Bit zu kurz**: 255 gepushte Bit (ein Bit unter dem in (4) bestätigten
   Minimum) decodieren nie — die zweite Präambel-Prüfung kann ihr letztes benötigtes Bit nicht
   erreichen.
6. **Negativ, gekipptes Präambel-Leitbit**: bei exakt 256 gepushten Bit (siehe (4)) ein
   gekipptes erstes Bit — schlägt zuverlässig fehl, und da der Strom hier endet, gibt es keine
   spätere Wiederholung, die den Fehler "heilen" könnte.
7. **`write_data()` (T5577)**: `blocks_to_write == 8`; `block[0]` exakt die dokumentierte
   Konfiguration (PSK2, RF/32, 7 Datenblöcke); `block[1..7]` exakt die sieben 32-Bit-
   Big-Endian-Blöcke der 28 Quell-Byte, byteweise gegen die Quelldaten nachgerechnet (nicht
   nur gegen `lfrfid_write_fill_t5577()` selbst zurückgespiegelt).
8. **`render_data()`/`render_brief_data()`**: beide liefern für den Proxmark3-Referenzvektor
   exakt das im Quelltext festgelegte Hex-Zeilenlayout.

**Mutationsproben zur Absicherung des Repros selbst** (keine Teil des gelieferten Repros, nur
zur Prüfung dieser Sitzung durchgeführt und verworfen): eine Mutante, die in
`decoder_save()` die differentielle XOR-Verknüpfung durch eine AND-Verknüpfung ersetzt, lässt
bereits die allererste Rundlauf-Prüfung fehlschlagen; eine zweite Mutante, die in
`lfrfid_write_fill_t5577()` den Blockindex-Offset `(block - 1) * 32` durch `block * 32`
ersetzt (ein plausibler Transkriptionsfehler), lässt gezielt die `write_data()`-Prüfung
fehlschlagen, ohne eine der vorherigen Decoder-Prüfungen zu berühren. Beide Mutanten sind
nicht Teil des gelieferten Repros.

**Nicht** durch diesen Host-Test abgedeckt: das tatsächliche Abtasten eines Feldes über die
generische LFRFID-Analogfrontend-Pipeline selbst (dieselbe Pipeline, die bereits produktiv für
jedes andere LFRFID-Protokoll bei mir läuft — kein neues Risiko dieses Ports), sowie der
reale T5577-Schreibvorgang über `t5577_write()` (ebenfalls bereits vorhandene, generische
Funktion).

## Port-Skizze

1. `protocol_indala224.c/.h` unverändert nach `lib/lfrfid/protocols/` übernehmen, als neue
   Quelldatei in die `SConscript`/Build-Liste von `lib/lfrfid` aufnehmen (reine
   Quelldatei-Ergänzung, kein neues Ziel — exakt das Muster, mit dem bereits jedes vorhandene
   LFRFID-Protokoll in der Build-Liste steht).
2. `lfrfid_write_fill.c/.h` unverändert nach `lib/lfrfid/protocols/` übernehmen, ebenfalls in
   die Build-Liste aufnehmen.
3. Die beiden Patches aus `firmware-mining/indala224-lfrfid-protocol-port/connective-patches/`
   anwenden (`lfrfid_protocols.h/.c`) — macht `protocol_indala224.c` erst referenzierbar.
4. Bauen (`ufbt` bzw. `fbt firmware_all`) — jede Struct-/Signaturabweichung fällt sofort als
   Compile-Fehler auf; bei korrekter Übernahme sollte dieser Schritt ohne Fehler durchlaufen,
   da alle drei neuen/geänderten Dateien additiv sind und keine bestehende Signatur ändern.
5. Funktionstest lokal: eine eigene Indala224-Karte (falls verfügbar) lesen, Rahmen gegen ein
   unabhängiges Referenzwerkzeug (Proxmark3, `lf indala reader`) vergleichen; danach Emulation
   gegen ein zweites eigenes Lesegerät; danach Schreibversuch auf eine eigene, leere T5577-Karte
   und Rücklesen zur Bestätigung.

## Quelle

- Unleashed-Firmware, `dev`-Branch, Commit `a70a69d9d47c1e0a3b1f868aa25c4d0a7a5346d5`
  (2026-10-20): `lib/lfrfid/protocols/protocol_indala224.c/.h`,
  `lib/lfrfid/protocols/lfrfid_write_fill.c/.h`,
  `applications/debug/unit_tests/tests/lfrfid/lfrfid_protocols.c` (Testvektoren,
  `test_lfrfid_protocol_indala224_roundtrip`/`_alternating_phase`). Alle vier Quelldateien
  über zwei unabhängige Abrufmethoden (`git clone --branch dev` sowie
  `raw.githubusercontent.com` gegen den exakten, oben genannten Commit-Hash) geladen und per
  `md5sum` byte-für-byte identisch bestätigt.
- Eigene Firmware-Basis (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, Tag `v2.0.0`/Momentum `mntm-012`):
  `lib/lfrfid/protocols/` vollständig gegen Unleashed `dev` gegenübergestellt (Dateiliste,
  nicht nur Stichproben) — `protocol_indala224.{c,h}`/`lfrfid_write_fill.{c,h}` als einziger
  Unterschied identifiziert. `lib/lfrfid/protocols/lfrfid_protocols.h/.c`,
  `lib/lfrfid/tools/t5577.h`, `lib/bit_lib/bit_lib.c`, `lib/toolbox/protocols/protocol.h`,
  `lib/toolbox/level_duration.h`, `lib/lfrfid/protocols/protocol_indala26.c` (zum Abgleich,
  dass Indala224 keine Codeabhängigkeit zu Indala26 hat) gelesen, um die Patches und die
  Repro-Testumgebung exakt an den tatsächlichen Signaturen auszurichten. Die beiden Patches
  (`connective-patches/lfrfid_protocols_h.patch`/`_c.patch`) gegen zwei frische, unabhängige
  Zweitkopien dieser Basis mit `git apply --check`/`git apply` verifiziert, real angewendet,
  per `md5sum` abgeglichen — identisch.
- `protocol_indala224.h`/`.c` zitiert im Dateikommentar der T5577-Konfiguration das
  Proxmark3-Symbol `T55X7_INDALA_224_CONFIG_BLOCK` als Herkunft der Schreibkonfiguration.
