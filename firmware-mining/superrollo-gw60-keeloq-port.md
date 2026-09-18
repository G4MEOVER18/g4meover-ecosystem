# SubGHz: Superrollo (GW60) Rollladenfunk als KeeLoq-HCS361-Framing-Variante portiert

## Was
Letzten offenen Punkt der `unlshd-093`-Restliste aufgegriffen: `104d96f86` (PR #1068, "SubGHz:
Add Superrollo (GW60) roller-shutter protocol (KeeLoq HCS361)"). Superrollo-GW60-Rollladen-
Handsender sind laut Commit-Beschreibung regulärer HCS361-KeeLoq (gleiche Krypto, gleiches
Bit-PWM-Timing wie meine bereits vorhandenen KeeLoq-Handsender), nur mit eigenem Rahmen: eine
asymmetrische Präambel (Te HIGH / 2Te LOW statt der üblichen symmetrischen Te/Te-Präambel), ein
10×Te-Sync-Puls und ein 67-Bit-Frame (64 Nutzdaten-Bit + ein VLOW-Bit + 2-Bit-CRC statt der
üblichen 64+2 Statusbits). Kein neues Protokollmodul — Unleashed erweitert dafür ausschließlich
`lib/subghz/protocols/keeloq.c` (Decoder-Präambel-/Sync-Erkennung + Toleranzfenster, neuer
Encoder-Framing-Zweig) sowie drei kleine App-Dateien (neuer `SetType`-Menüpunkt "Add Manually").

Gegen meine `mntm-012`-Basis geprüft: `applications/main/subghz/helpers/subghz_custom_event.h`,
`applications/main/subghz/helpers/subghz_gen_info.c` und
`applications/main/subghz/scenes/subghz_scene_set_type.c` sind an der betroffenen Stelle
(`SetTypeDoorHan_433_92` gefolgt von `SetTypeBeninca433`/`"KL: Beninca 433MHz"`) byteidentisch zum
Unleashed-Vorher-Stand — Unleasheds Original-Hunks für diese drei Dateien sind bei mir 1:1
anwendbar. `lib/subghz/protocols/keeloq.c` dagegen ist bei mir strukturell deutlich abweichend
(eigener Stand ohne Unleasheds `bypass`-Variable/ältere `ProgMode`-Zwischenschritte, andere
Funktionsnamen — `subghz_protocol_encoder_keeloq_get_upload()` statt Unleasheds separater
`..._encode_to_timings()`-Hilfsfunktion, `te_delta = 140` statt `180`) — kein 1:1-Diff möglich,
wie schon beim MIFARE-Classic-Schlüssel-Dictionary-Feature und beim CUID-Dictionary-Zähler-Fund.
Denselben, hier bereits etablierten Weg gegangen: die zugrunde liegende Idee (asymmetrische
Präambel-Erkennung, 10×Te-Sync-Übergang, erweitertes Bit-Toleranzfenster, CRC-Framing im Encoder)
1:1 aus Unleasheds Commit übernommen, aber an meine eigene Funktions-/Zustandsautomat-Struktur
angepasst statt Unleasheds Diff blind zu übertragen.

## Fund/Einordnung: kein Sicherheitsfund, reine Feature-Portierung
Kein Fehler in meiner Basis — `keeloq.c` decodiert/encodiert bislang schlicht keine
Superrollo-GW60-Rahmen, weil Präambel-/Sync-Erkennung und Bit-Toleranzfenster auf die
symmetrische, 66-Bit-KeeLoq-Rahmenform zugeschnitten sind. Diese Sitzung liefert die fehlende
Framing-Unterstützung nach, exakt nach Unleasheds Prinzip.

## Betroffene Dateien
- `lib/subghz/protocols/keeloq.c` — eigener Diff (siehe unten), drei getrennte Änderungen:
  1. **Decoder, `KeeloqDecoderStepReset`**: neuer `else if`-Zweig — ein 10×Te-HIGH-Puls
     (`header_count > 2`) wird jetzt zusätzlich zum normalen Te-Präambelbit als Übergang nach
     `KeeloqDecoderStepCheckPreambula` akzeptiert (GW60s Sync-Puls tritt an die Stelle eines
     weiteren Präambelbits statt eines regulären Te-Bits).
  2. **Decoder, `KeeloqDecoderStepCheckPreambula`**: die LOW-Prüfung akzeptiert jetzt zusätzlich
     `2×Te` (bisher nur `Te`) — GW60s Präambel ist asymmetrisch (Te HIGH / 2Te LOW statt Te/Te).
  3. **Decoder, `KeeloqDecoderStepCheckDuration`**: das Toleranzfenster für die akzeptierte
     Gesamt-Bitzahl beim Frame-Ende wächst von `min_count_bit_for_found + 2` auf `+ 3` (64
     Nutzdaten-Bit + VLOW + 2-Bit-CRC = 67 statt bisher maximal 66).
  4. **Encoder, `subghz_protocol_encoder_keeloq_get_upload()`**: neuer, früh zurückkehrender
     Zweig für `manufacture_name == "Superrollo"` — eigenes Timing (`t = 450` statt der
     gemeinsamen `te_short = 400`, wie im Original), 9×(Te HIGH/2Te LOW)-Präambel, 10×Te-Sync,
     64 Nutzdaten-Bit (MSB zuerst) + VLOW(=1) + 2-Bit-CRC, Abschluss-HIGH + Guard-LOW.
  Alle drei Änderungen betreffen ausschließlich neue Zweige bzw. zusätzliche `||`-Bedingungen —
  der bestehende Kontrollfluss für alle anderen KeeLoq-Hersteller bleibt strukturell unverändert
  (siehe Risiko unten zum erweiterten Toleranzfenster, das *alle* Hersteller betrifft, nicht nur
  Superrollo).
- `applications/main/subghz/helpers/subghz_custom_event.h` — neuer Enum-Wert
  `SetTypeSuperrollo_433_92` (zwischen `SetTypeDoorHan_433_92` und `SetTypeBeninca433`, exakt wie
  im Original).
- `applications/main/subghz/helpers/subghz_gen_info.c` — neuer `case SetTypeSuperrollo_433_92:`
  (433,92 MHz, `keeloq.btn = 0x0A`, `keeloq.cnt = 0x03`, `keeloq.manuf = "Superrollo"`), 1:1 aus
  Unleashed übernommen.
- `applications/main/subghz/scenes/subghz_scene_set_type.c` — neuer Menüpunkt-Text
  `"KL: Superrollo 433MHz"` im "Add Manually"-Menü.
- Bewusst **nicht** portiert: die `CHANGELOG.md`-Zeile (projektspezifisch bei Unleashed), der
  `documentation/SubGHzSupportedSystems.md`-Eintrag (diese Datei existiert bei mir nicht — führe
  stattdessen hier Buch), sowie die `keeloq_mfcodes`-Ressourcendatei-Änderung (235 geänderte
  Zeilen laut Unleasheds Commit-Statistik) — das ist Unleasheds verschlüsselte
  Hersteller-Schlüsseldatenbank in einem bei mir strukturell/versionsmäßig nicht zwangsläufig
  identischen Format; der Normal-Learning-Schlüssel für "Superrollo" muss lokal separat geprüft
  und übernommen werden (siehe Testhinweis).

## Host-Repro
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro` in
`firmware-mining/repro-superrollo-gw60-keeloq-port/`. Bildet die reale
`subghz_protocol_decoder_keeloq_feed()`-Zustandsmaschine (gleiche Konstanten `te_short=400`,
`te_long=800`, `te_delta=140`, `min_count_bit_for_found=64`, gleiches
`subghz_protocol_blocks_add_bit()`-Schieberegisterverhalten) sowie die neue CRC-/Framing-Logik
aus dem Encoder-Zweig fast wörtlich nach — einmal als ALT (mein unveränderter Zustandsautomat)
und einmal als NEU (mit den drei Decoder-Änderungen aus `keeloq_c.patch`). Tatsächliche Ausgabe
(lokal ausgeführt):
```
== Test 1: CRC-Berechnung ==
  CRC-Werteverteilung ueber 200 Zufallsnutzlasten: 4/4 verschiedene Werte gesehen
== Test 2: Superrollo-GW60-Frame (ALT vs. NEU) ==
  ALT erkennt Superrollo-Frame in 0/500 Faellen
  NEU erkennt Superrollo-Frame in 500/500 Faellen (davon 500 mit korrekten 64 Nutzdaten-Bits)
== Test 3: Regression -- Standard-KeeLoq-Frame ==
  0/500 Standard-KeeLoq-Frames bei ALT und NEU abweichend erkannt

1206/1206 Checks bestanden -- ALL CHECKS PASSED
```
Vier Testgruppen: (1) die CRC-Berechnung ist deterministisch und deckt über 200
Zufallsnutzlasten alle vier möglichen 2-Bit-Werte ab (kein trivialer Immer-0/Immer-1-Fund — ein
Test, der eine falsch implementierte CRC aufgedeckt hätte); (2) 500 zufällige 64-Bit-Nutzlasten
werden über den neuen Encoder-Zweig zu einem synthetischen Superrollo-Pulsstrom kodiert und durch
beide Decoder-Varianten geschickt — ALT erkennt in **keinem** der 500 Fälle einen vollständigen
Frame (mein bisheriger Decoder kann GW60 nachweislich nicht dekodieren), NEU erkennt **alle** 500
korrekt und liefert dabei in jedem Fall exakt die ursprünglichen 64 Nutzdaten-Bit zurück
(Frame-Erkennung UND Bit-Korrektheit geprüft, nicht nur Erkennung allein); (3) Regressionstest —
500 zufällige 64-Bit-Nutzlasten über den bestehenden, unveränderten Standard-KeeLoq-Encoder-Zweig
(11×Te-Präambel, symmetrischer Sync, 66-Bit-Rahmen) kodiert: ALT und NEU erkennen in **allen**
500 Fällen identisch dieselben Nutzdaten — die drei Decoder-Änderungen ändern das
Erkennungsverhalten für bestehende Hersteller nachweislich nicht. Unter
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`: keine ASan-/UBSan-Meldungen.

## Betroffene Dateien (Diffs)
Vier Unified-Diffs gegen meine unveränderte `mntm-012`-Basis in
`firmware-mining/superrollo-gw60-keeloq-port/` — jeweils `patch --dry-run -p1` gegen eine frische
Basiskopie vorab verifiziert, real angewendet und Byte-für-Byte gegen die separat editierte
Fassung abgeglichen (identisch):
- `keeloq_c.patch` (93 Zeilen) — die vier oben beschriebenen Änderungen in
  `lib/subghz/protocols/keeloq.c`.
- `subghz_custom_event_h.patch` (10 Zeilen) — neuer `SetType`-Enum-Wert.
- `subghz_gen_info_c.patch` (19 Zeilen) — neuer `GenInfo`-Case.
- `subghz_scene_set_type_c.patch` (10 Zeilen) — neuer Menüpunkt-Text.

## Risiko
Gering bis moderat. Die drei neuen Zweige/Bedingungen (10×Te-Sync-Übergang in `Reset`,
`2×Te`-LOW-Alternative in `CheckPreambula`, Encoder-Framing-Zweig) sind ausschließlich additiv und
durch den `else if`/zusätzliche `||`-Bedingung bzw. `manufacture_name == "Superrollo"`-Gate von
allen anderen Herstellern entkoppelt — mit dem Regressionstest oben belegt. Eine Stelle betrifft
**alle** KeeLoq-Hersteller gemeinsam: das erweiterte Bit-Toleranzfenster
(`min_count_bit_for_found + 3` statt `+ 2`) im Decoder gilt herstellerunabhängig, weil der
Decoder beim Erkennen des Frame-Endes den Hersteller noch nicht kennt (der wird erst danach über
`subghz_protocol_keeloq_check_remote_controller()` anhand des entschlüsselten Inhalts bestimmt) —
das ist exakt Unleasheds eigener Ansatz, keine eigene Abweichung. Praktische Auswirkung: ein
Frame mit 67 statt bisher maximal 66 empfangenen Bits wird jetzt ebenfalls als vollständig
akzeptiert (die zusätzlichen Bits über die ersten 64 hinaus werden ohnehin nirgends ausgewertet,
weder vorher noch nachher — nur `decode_count_bit` wird weitergezählt, `decode_data` bleibt ab
Bit 64 unverändert), potenziell einen Bit-Fehlerfall knapp toleranter machend, aber keinen
bestehenden Erkennungsfall verändernd (Regressionstest oben deckt genau das ab). Kein
Speicherzugriff betroffen (alle Änderungen sind reine Zustandsübergänge/Zählervergleiche auf
bereits vorhandenen `uint16_t`/`uint32_t`-Feldern, keine neuen Puffer, keine geänderte
Puffergröße im Encoder-Zweig — der neue Frame ist mit 156 `LevelDuration`-Einträgen deutlich
kleiner als die bei der Encoder-Allokation reservierten 256).

## Testhinweis (lokal nachzuvollziehen)
1. `firmware-mining/repro-superrollo-gw60-keeloq-port/repro.c` host-kompilieren/ausführen (Befehl
   und tatsächliche Ausgabe siehe Host-Repro oben) — reine Logikverifikation ohne Hardware.
2. Die vier Patches gegen eine frische `mntm-012`-Basis anwenden (`patch -p1 <
   firmware-mining/superrollo-gw60-keeloq-port/<datei>.patch`, Reihenfolge egal, unabhängig
   voneinander), `fbt` neu bauen.
3. Vor dem Flashen den "Superrollo"-Normal-Learning-Schlüssel in meiner eigenen
   `subghz_keystore`-Ressource ergänzen/prüfen (bei mir eine eigene Datei/eigenes Format,
   unabhängig von Unleasheds `keeloq_mfcemfcodes`-Diff, siehe "Betroffene Dateien" oben) — ohne
   passenden Schlüssel liefert `subghz_protocol_keeloq_check_remote_controller()` beim Empfang
   zwar weiterhin einen erkannten *Frame*, aber keinen entschlüsselten, einem Hersteller
   zugeordneten Inhalt.
4. Am Gerät: SubGHz → 433,92 MHz AM650 → "Add Manually" → "KL: Superrollo 433MHz" auswählen,
   einen Testsender erzeugen/senden und mit einem zweiten Empfänger (zweiter Flipper oder
   SDR) gegenprüfen, dass Präambel/Sync/Bitmuster wie beschrieben (9×(Te/2Te), 10×Te-Sync,
   67 Bit) ankommen. Für den Empfangspfad: falls ein echter GW60-Handsender zum eigenen
   autorisierten Testaufbau gehört, dessen Signal empfangen und prüfen, dass es jetzt als
   vollständiger Frame erkannt wird (vorher: kein Treffer) — **keine fremden Aufnahmen/Captures
   in dieses Repo übernehmen**, rein lokaler Hardwaretest (CONSTRAINTS Punkt 2).
5. Regressionstest: einen bereits unterstützten KeeLoq-Handsender (z. B. DoorHan, Beninca) wie
   gewohnt senden/empfangen und bestätigen, dass sich am Verhalten nichts ändert.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `104d96f86` (PR #1068, "SubGHz: Add
  Superrollo (GW60) roller-shutter protocol (KeeLoq HCS361)"), Tag `unlshd-093`.
- `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`, als eigene Basis geprüft (alle vier
  betroffenen Dateien vorhanden, drei davon byteidentisch zum Unleashed-Vorher-Stand, `keeloq.c`
  strukturell abweichend — siehe "Was" oben).
- API-87-Einordnung: reine SubGHz-Protokoll-/App-Erweiterung, keine neuen HAL-/API-Aufrufe
  gegenüber dem bereits vorhandenen, produktiv genutzten KeeLoq-Pfad — kein
  Kompatibilitätsrisiko.
- Damit ist die `unlshd-093`-Restliste (Expansion-Callback-Sentinel #1108, FeliCa-Lite-Bounds
  #1106, Stealth-Jiggler-Bounds #1111, CUID-Dictionary-Zähler #1115,
  MIFARE-Classic-Schlüssel-Dictionary-Feature #1118, Superrollo/GW60 #1068) vollständig
  abgearbeitet.
