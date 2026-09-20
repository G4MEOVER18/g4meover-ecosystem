# LF-RFID Keri-Decoder: PSK-Skew-Korrektur beidseitig statt einseitig, plus Zwei-Rahmen-Konsistenzprüfung

## Was
Ich habe den letzten noch offenen Kandidaten aus dem Unleashed-`dev`-Fenster seit `unlshd-093`
aufgegriffen: Commit `946a52a64` ("merge some rfid related PRs", zusammengefasst aus mehreren
Einzel-PRs) enthält unter anderem OFW PR 4449 ("Keri tags now read correctly"), eine grundlegende
Überarbeitung von `lib/lfrfid/protocols/protocol_keri.c` — dem Decoder für das Keri-LF-RFID-Protokoll
(125 kHz, PSK1-moduliert). Bei mir (mntm-012) liegt diese Datei byte-identisch zum
Unleashed-Vorher-Stand vor (per Diff gegen einen frischen Klon von `DarkFlippers/unleashed-firmware`
bestätigt) — die Portierung ist eine reine Eins-zu-eins-Übernahme.

Bewusst nicht Teil dieses Vorschlags: die beiden anderen, im selben Commit zusammengefassten Änderungen
(`targets/f7/furi_hal/furi_hal_rfid.c` — Timer-Capture-Ursprung neu gezählt, ein reiner Hardware-Timing-Fix
ohne host-testbare Logik; `applications/main/nfc/nfc_app.c` — Dolphin-XP beim direkten Emulieren aus
Favoriten, kosmetisch) sowie die beiden Geschwister-Commits `22ce1d9ae`/`49238e1bd` (Chiptyp-Auswahl beim
Schreiben bzw. neuer Hitag-S-Klon-Schreibpfad) — für eine künftige Sitzung vorgemerkt.

## Fund: die alte Skew-Korrektur verstärkt den beschriebenen Hardware-Fehler statt ihn auszugleichen
Ein Keri-Rahmen trägt in seiner 33-Bit-Präambel 29 Nullen am Stück und ist dadurch nie DC-balanciert.
Laut dem Unleashed-Änderungshinweis führt das dazu, dass der PSK-Pegel-Diskriminator eine der beiden
Flankenrichtungen systematisch um ~135 µs zu lang und die andere um ~135 µs zu kurz meldet — mit der
Folge, dass **kein einziger echter Keri-Tag mehr gelesen wird, nicht einmal einer, den der Flipper
selbst gerade erst geschrieben hat**.

Bei mir versucht `protocol_keri_decoder_feed()` genau das mit einem fest verdrahteten
Korrekturzweig auszugleichen: liegt die gemessene Dauer im Mehrdeutigkeitsfenster
(`KERI_US_PER_BIT/4 < duration <= KERI_US_PER_BIT/2`), wird bei `level == true` pauschal `+120 µs`
addiert, bei `level == false` `-120 µs` subtrahiert — eine einzige, richtungsfeste Annahme. Trifft diese
Annahme zufällig die tatsächliche Skew-Richtung, gleicht sie sie aus. Trifft sie die **entgegengesetzte**
Richtung (die physikalisch genauso möglich ist wie die andere), addiert sie sich zum bereits vorhandenen
Hardware-Versatz statt ihn zu kompensieren — bei ~135 µs realem Versatz plus ~120 µs falsch gerichteter
Korrektur ergibt das ~255 µs, exakt eine volle Bitperiode (`KERI_US_PER_BIT`) zu viel, und der betroffene
Lauf wird durchgehend um ein Bit zu lang decodiert. Das deckt sich exakt mit der oben zitierten
Fehlerbeschreibung ("every run of ones decodes one bit long").

Zusätzlich validiert mein Decoder nur, dass nach der eigentlichen Nutzlast (32 Bit ab Bitindex 32) eine
**zweite Präambel** folgt (Bitindex 64) — er prüft aber nie, ob die Nutzlast des zweiten Rahmens mit der
des ersten übereinstimmt, weil sein Schieberegisterpuffer mit 13 Byte (104 Bit) dafür schlicht zu klein
ist (Bit 96 wäre die letzte benötigte Präambel-Bitposition, die eigentliche zweite Nutzlast läge bei Bit
96–127 und damit außerhalb des Puffers). Keri hat weder Parität noch Prüfsumme — die einzige verfügbare
Integritätsprüfung ist der Vergleich zweier aufeinanderfolgender Rahmen, und genau die fehlt mir.

## Betroffene Dateien
- `lib/lfrfid/protocols/protocol_keri.c` (einzige geänderte Datei, keine Header-/Aufrufstellen-Änderung
  nötig — `ProtocolKeri` bleibt ein in dieser Datei gekapselter opaquer Typ).

## Risiko
Gering. Reine Software-Logik im Decoder, keine Protokoll-Wire-Änderung, kein Einfluss auf das
Schreiben (der Encoder-Pfad `protocol_keri_encoder_start/_yield` und `protocol_keri_write_data` bleiben
unverändert). Der Puffer wächst von 13 auf 16 Byte je der jetzt sechs statt vier
Schieberegister-Instanzen (zwei zusätzliche Buffer-Paare für die jetzt beidseitige Korrektur) — reine
Stack-/Heap-Mehrkosten von wenigen Byte pro `ProtocolKeri`-Instanz, keine funktionale Nebenwirkung.

## Port-Skizze / Fix
1. `KERI_PREAMBLE_DATA_SIZE` entfällt; `KERI_ENCODED_DATA_SIZE` wächst von `(64/8)+5=13` auf
   `(64*2)/8=16` Byte, sodass der Puffer beide Rahmen (Präambel+Nutzlast je 64 Bit) gleichzeitig fassen
   kann.
2. `protocol_keri_can_be_decoded()` prüft zusätzlich `bit_lib_get_bits_32(data,32,32) ==
   bit_lib_get_bits_32(data, 64+32, 32)` — beide Rahmen müssen dieselbe 32-Bit-Nutzlast tragen, bevor ein
   Treffer akzeptiert wird.
3. Die beiden alten Zweitversuchs-Puffer (`corrupted_encoded_data`/`corrupted_negative_encoded_data`,
   mit der einseitigen ±120-µs-Korrektur) werden durch **vier** neue ersetzt
   (`high_long_*`/`high_short_*`, je Positiv-/Negativ-Polarität): der eine Zweig probiert die Korrektur
   in die eine Richtung, der andere in die entgegengesetzte — unabhängig davon, welche Richtung real
   zutrifft, deckt einer der beiden Zweige sie ab.
4. Gemeinsame Hilfsfunktion `protocol_keri_decoder_feed_pair()` fasst das wiederholte
   "Positiv-Polarität versuchen, sonst Negativ-Polarität versuchen, bei Erfolg speichern"-Muster
   zusammen (vorher vierfach dupliziert, jetzt zweimal aufgerufen).
5. `KERI_EDGE_SKEW_US = 120` bleibt als Korrekturbetrag erhalten (laut Fix-Kommentar unkritisch, solange
   der Rest innerhalb ±127 µs der Bitperiodentoleranz landet).

Der vollständige Unified-Diff (gegen meine `mntm-012`-Basiskopie mit `patch -p1 --dry-run` vorab
verifiziert, real angewendet und Byte-für-Byte gegen den echten Unleashed-Fix abgeglichen — identisch)
liegt in `firmware-mining/keri-lfrfid-skew-and-frame-check-port/protocol_keri_c.patch`.

## Host-Repro
`firmware-mining/repro-keri-lfrfid-skew-and-frame-check-port/repro.c` transkribiert beide
Decoder-Fassungen (ALT = meine aktuelle `mntm-012`-Version, NEU = die portierte Fassung) sowie die
tatsächlich benötigten `bit_lib`-Grundfunktionen (`push_bit`/`get_bit`/`set_bit`/`get_bits`/
`get_bits_32`, wortgleich aus `lib/bit_lib/bit_lib.c` übernommen) eigenständig, ohne Hardware- oder
Firmware-Abhängigkeit.

Testaufbau: ein synthetischer Zwei-Rahmen-Keri-Strom (Präambel + 32-Bit-Nutzlast, zweimal
hintereinander mit identischer Nutzlast, wie ein realer Tag ihn dauerhaft sendet) wird lauflängenkodiert
(Pegel + Dauer je zusammenhängendem Bit-Lauf) und mit einem pegelabhängigen Versatz versehen, der die
oben zitierte Fehlerbeschreibung direkt nachbildet (`level==true`-Läufe um X µs verlängert,
`level==false`-Läufe um X µs verkürzt, oder umgekehrt). Vier Prüfgruppen:

1. **Kein Versatz** (Regressionsnachweis): ALT und NEU decodieren beide korrekt die eingebettete ID.
2. **Versatz in einer Richtung** (`level==true` um 135 µs zu lang, entspricht der im Änderungshinweis
   zitierten Richtung): ALT decodiert nie die korrekte ID (0 von 4 Testrahmen erfolgreich), NEU
   decodiert korrekt.
3. **Versatz in der Gegenrichtung**: ALT decodiert hier zufällig korrekt (seine feste
   Korrekturannahme trifft in diesem Fall zu) — das bestätigt, dass ALTs Fix nur für eine von zwei
   physikalisch möglichen Richtungen funktioniert, während NEU in beiden Fällen korrekt decodiert.
4. **Direkter Unit-Test der Zwei-Rahmen-Prüfung** (ohne den Lauflängen-/Versatz-Umweg): zwei
   strukturell gültige Rahmen mit einem einzelnen abweichenden Bit in der zweiten Nutzlast (eine
   plausible Grenzfall-Aufnahme) — NEU lehnt korrekt ab, akzeptiert aber identische Nutzlasten; ALT kann
   diesen Fall konstruktionsbedingt gar nicht prüfen, da sein Puffer mit 104 Bit die zweite Nutzlast
   (Bit 96–127) nicht erreicht.

Unter `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined` (Ausrichtungsprüfung für den
unausgerichteten `uint32_t`-Lesezugriff in der Präambelprüfung bewusst mit `-fno-sanitize=alignment`
ausgenommen — dieses Muster ist wortgleich in ALT wie NEU vorhanden, nicht Teil des portierten Fixes,
und auf dem Zielsystem [STM32WB55/Cortex-M4] unkritisch, da unausgerichtete Normalzugriffe dort per
Vorgabe unterstützt werden): **9/9 Prüfungen bestanden, keine ASan-/UBSan-Meldungen**.

## Testhinweis (lokal nachzuvollziehen)
1. Patch anwenden: `patch -p1 < firmware-mining/keri-lfrfid-skew-and-frame-check-port/protocol_keri_c.patch`
   im Firmware-Wurzelverzeichnis, danach `fbt`/`ufbt` neu bauen und flashen.
2. Regressionstest zuerst: einen bereits vorhandenen, sicher lesbaren Keri-Tag (oder einen selbst
   beschriebenen T5577-Klon) einlesen — muss weiterhin korrekt gelesen werden wie vor dem Patch.
3. Eigentlicher Test (falls ein echter Keri-Tag vorhanden ist, der bei mir bisher **nicht** oder nur
   unzuverlässig gelesen wurde): erneut versuchen — sollte nach dem Patch zuverlässig lesen, sofern der
   beschriebene PSK-Skew die Ursache war.
4. Schreib-Rundlauf: einen gelesenen Keri-Tag auf einen eigenen T5577 schreiben, den beschriebenen
   T5577 anschließend mit demselben (jetzt gepatchten) Decoder zurücklesen — muss dieselbe ID liefern.

## Quelle
- `DarkFlippers/unleashed-firmware`, Branch `dev`, Commit `946a52a64` ("merge some rfid related PRs"),
  darin der `protocol_keri.c`-Anteil von OFW PR 4449 ("Keri tags now read correctly", @mfcarroll).
- Eigene Basis: `Next-Flip/Momentum-Firmware`, Tag `mntm-012`.
