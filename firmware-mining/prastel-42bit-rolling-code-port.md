# Prastel 42-bit — von Fixed-Code auf Rolling-Code (Port aus Unleashed)

## Was
Ich habe gegen meine Basis mntm-012 (`lib/subghz/protocols/came.c`) geprüft, wie ich 42-Bit-
Prastel-Handsender aktuell behandle: `came.c` erkennt 42 Bit als einen weiteren Bit-Count neben
CAME-12/-24, Airforce-18 und Prastel-25 und zeigt die Rohdaten (und ihre Bitspiegelung) einfach als
statischen Fixed-Code an — keine Feld-Extraktion, kein Zähler, kein Rolling-Code-Handling. Ein
Replay sendet exakt dieselben 42 Bit erneut.

Unleashed-Firmware (dev, Commit `945234a`, Stand 2026-09-10, API 88.9) hat erkannt, dass das
42-Bit-Prastel-Format tatsächlich ein **Rolling Code** ist: die Nutzlast trägt eine Seriennummer,
einen Tasten-Index und einen 16-Bit-Zähler hinter einer eigenen Bit-Verwürflung (nicht nur die von
CAME bekannte Manchester-artige Kurz/Lang-Kodierung, sondern eine zusätzliche Byte-Ebenen-
Verschlüsselung mit einem LCG-artigen Zähler-Mechanismus). Sie haben das 42-Bit-Handling komplett
aus `came.c` entfernt und in ein eigenes `prastel.c`/`prastel.h` ausgelagert (`SubGhzProtocolType
Dynamic` statt `Static`).

Für mich bedeutet das: Prastel-Handsender, die ich bisher als (falsch klassifizierten) Fixed-Code
sehe, decodiere ich aktuell ohne Seriennummer/Taste/Zähler-Anzeige, und ein reiner Replay
funktioniert zwar (identische Bits), aber ich habe keine Analyse-/Zähler-Fähigkeiten wie bei
anderen Rolling-Code-Systemen (CAME Atomo, KeeLoq, …) — und falls eine Empfangsanlage den Zähler
tatsächlich prüft, wäre ein simpler Replay ohnehin nicht das, was ein echter Handsender sendet.

## Nutzen
- Ordnet ein bereits vorhandenes, aber falsch klassifiziertes Signalformat (Prastel 42 Bit) korrekt
  als Rolling Code ein, mit Seriennummer-/Tasten-/Zähler-Anzeige wie bei den anderen dynamischen
  Protokollen in meiner Basis (CAME Atomo, KeeLoq, Nice FloR-S, …).
- Reine Erkenntnis-/Analyse-Erweiterung für Torantriebs-Pentests (Prastel ist wie Cardin/Telcoma
  eine real verbreitete italienische Handsenderfamilie) — kein neues Sicherheits-/Missbrauchsrisiko
  über das hinaus, was jeder Rolling-Code-Dekoder in meiner Basis ohnehin hat (autorisierte Tests
  an eigenen bzw. vertraglich freigegebenen Toranlagen).
- Additiv: neue Datei-paar `prastel.c/.h`, plus eine einzeilige Entfernung aus einer bestehenden
  Bedingung in `came.c` (42 Bit wird dort nicht mehr als CAME akzeptiert, damit nicht beide
  Decoder auf dasselbe Signal anspringen) — berührt keinen anderen Protokoll-Handler.

## Betroffene Dateien (lokal)
Neu (fertig an meine Basis angepasst beigelegt, siehe unten):
- `lib/subghz/protocols/prastel.c` — `firmware-mining/prastel-42bit-rolling-code-port/prastel.c`
- `lib/subghz/protocols/prastel.h` — `firmware-mining/prastel-42bit-rolling-code-port/prastel.h`

Änderung (1 Zeile entfernen):
- `lib/subghz/protocols/came.c`, `subghz_protocol_decoder_came_feed()`,
  `CameDecoderStepSaveDuration`-Fall: aus der Akzeptanzbedingung
  ```c
  if((instance->decoder.decode_count_bit ==
      subghz_protocol_came_const.min_count_bit_for_found) ||
     (instance->decoder.decode_count_bit == AIRFORCE_COUNT_BIT) ||
     (instance->decoder.decode_count_bit == PRASTEL_25_COUNT_BIT) ||
     (instance->decoder.decode_count_bit == PRASTEL_42_COUNT_BIT) ||   // <- diese Zeile entfernen
     (instance->decoder.decode_count_bit == CAME_24_COUNT_BIT)) {
  ```
  die markierte Zeile streichen. Die restlichen `PRASTEL_42_COUNT_BIT`-Vorkommen in `came.c`
  (Encoder-`header_te`-Switch, `deserialize`-Bit-Count-Grenze, `get_string`-Namensanzeige) bewusst
  unverändert lassen — sie betreffen nur den manuellen "Add Manually"-Erzeugungspfad für CAME-
  artige Codes per Hand, mit dem weiterhin ein 42-Bit-CAME-artiger Fixed-Code erzeugt werden kann,
  falls das für Tests gewünscht ist; sie beeinflussen nicht, welcher Decoder ein empfangenes Signal
  zuerst für sich beansprucht. Genau dieselbe Aufteilung hat Unleashed selbst gewählt (siehe deren
  `came.c`, Kommentar `/* 42 bit Prastel is a rolling code and lives in prastel.c */` an exakt
  dieser Stelle).

Registry (2 Zeilen ergänzen, wie bei jedem neuen Protokoll):
- `lib/subghz/protocols/protocol_items.h`: `#include "prastel.h"` ergänzen.
- `lib/subghz/protocols/protocol_items.c`: `&subghz_protocol_prastel,` im Registry-Array
  `subghz_protocol_registry_items[]` ergänzen (Position unkritisch, an keiner anderen Stelle
  referenziert).

Optional (nur für manuelles Erzeugen im Menü, nicht zwingend fürs Decodieren/Replay):
- `applications/main/subghz/scenes/subghz_scene_set_type.c` — Menüeintrag „Prastel" (Rolling).
- `documentation/SubGHzSupportedSystems.md` — Protokoll-Eintrag in meiner eigenen Liste ergänzen
  (Rolling statt Static).

## Anpassung gegenüber der Unleashed-Quelle
Die Bit-Verwürflung selbst (`reverse_byte()`, `parity()`, `unscramble()`, `scramble()`, `pack()`/
`unpack()`) habe ich unverändert übernommen — dazu unten mehr unter „Verifikation". Alles
Drumherum (Struct-Layout, Alloc/Free, Decoder-Zustandsautomat, Custom-Button-Handling,
Serialize/Deserialize) musste ich umschreiben, weil Unleasheds aktuelle Quelle auf zwei Dingen
aufbaut, die in meiner Basis nicht existieren:
1. Einem gemeinsamen `common.h`-Decoder/Encoder-Alloc/Free/Reset/Serialize (Unleashed hat laut
   eigenem Changelog kürzlich 57 Protokolle auf diese gemeinsame Implementierung umgestellt,
   `SUBGHZ_ASSERT_*_LAYOUT`-Makros eingeschlossen) — meine Basis nutzt noch das ältere,
   pro-Protokoll ausgeschriebene Muster wie `came.c`/`came_atomo.c`. Ich bin bei diesem Muster
   geblieben, exakt wie schon beim Telcoma/Cardin-EDGE-Port.
2. Einer globalen Tasten-/Zähler-Override-Infrastruktur
   (`subghz_block_generic_global_button_override_get()`,
   `subghz_block_generic_global_counter_override_get()`,
   `subghz_block_generic_global.cnt_is_available` usw.) — in meiner Basis grep-negativ (nirgends
   vorhanden), vermutlich eine neuere Unleashed-UI-Funktion. Ich verwende stattdessen exakt das
   Zähler-Inkrement-Muster, das `came_atomo.c` in meiner eigenen Basis bereits nutzt
   (`furi_hal_subghz_get_rolling_counter_mult()`, Sentinel `-0x7FFFFFFF` = kein Override, sonst
   Standard-Inkrement um 1), und rufe `subghz_protocol_prastel_get_btn_code()` direkt auf, ohne den
   Override-Wrapper. Die „push protocol data to global variable"-Zeilen in Unleasheds
   `get_string()` (für dieselbe globale Zähler-/Tasten-UI) habe ich ebenfalls weggelassen, da das
   zugehörige Feature in meiner Basis fehlt.

Diese Anpassung ist notwendig, damit die Datei bei mir überhaupt kompiliert — ein reines
Kopieren der aktuellen Unleashed-Quelle würde an beiden Punkten mit unbekannten
Symbolen/Funktionen fehlschlagen.

## Verifikation (Host, ohne Hardware)
Die Bit-Verwürflung ist die einzige Stelle, an der ich beim Transkribieren einen eigenen Fehler
hätte einbauen können (Vorzeichen-/Cast-Fehler, vertauschte Bytes, falsche Schleifengrenzen), daher
habe ich sie vor dem eigentlichen Port eigenständig auf dem Host nachgebaut und geprüft, statt sie
blind zu übernehmen:

`firmware-mining/repro-prastel-42bit-rolling/repro.c` — `reverse_byte()`/`parity()`/
`unscramble()`/`scramble()`/`pack()`/`unpack()` byteidentisch aus der Unleashed-Quelle übernommen,
plus drei Prüfungen unter `gcc -O1 -Wall -Wextra -fsanitize=address,undefined`:
1. **Pack/Unpack-Inversität**: `pack(unpack(data)) == data` über 50 000 zufällige 42-Bit-Werte —
   0 Abweichungen.
2. **Runde-Trip auf einer legitimen Nutzlast**: `unpack→unscramble→scramble→pack` reproduziert
   exakt den Ausgangswert, wenn dieser selbst aus `scramble()` stammt (steht stellvertretend für
   eine echte Handsender-Übertragung) — über 50 000 zufällige Nutzlasten 0 Abweichungen.
3. **`gen_data()`-Zyklus** (genau der Pfad, den der Encoder beim erneuten Senden mit geändertem
   Zähler durchläuft): aus einer legitimen Nutzlast Seriennummer/Taste extrahieren, mit
   `gen_data()` einen neuen Zähler einsetzen, das Ergebnis erneut decodieren — Seriennummer und
   Taste müssen unverändert bleiben, der Zähler muss exakt dem neu gesetzten Wert entsprechen.
   5000 zufällige Basis-Nutzlasten × 1 zufälliger Zähler plus 4 Rand-Zähler (0, 1, 0xFFFE, 0xFFFF)
   an einer festen Basis — 0 Abweichungen von 5004 Prüfungen.

Eine anfängliche vierte Prüfung (`unscramble(scramble(p)) == p` bzw. die Umkehrrichtung direkt auf
zufälligen 7-Byte-Arrays, ohne den Umweg über `pack()`/`unpack()`) schlug bei ca. 50 % der Fälle
fehl. Das war aber kein Fund, sondern eine falsch gewählte Prüfrichtung: `pack()`/`unpack()`
verwerfen strukturell die untersten 2 Bit der 44-Bit-Zwischennutzlast, und `scramble()` überschreibt
Bit 2 von `p[6]` bewusst mit einer selbst berechneten Paritätsprüfung über die Zähler-Bytes, statt
sie aus der Eingabe zu übernehmen — beides Bits, auf die ein echter Aufrufer nie direkten Zugriff
hat, weil `unpack()` sie schon selbst kanonisiert. Die Prüfung wurde entsprechend durch die drei
oben genannten ersetzt, die exakt den tatsächlich genutzten Pfad nachbilden. Alles davon ist reine
Selbstkonsistenzprüfung meiner Transkription; eine Verifikation gegen einen echten Prastel-
Handsender-Mitschnitt liegt mir nicht vor und ist nur lokal an Hardware möglich.

## Risiko
- Niedrig-mittel: additive neue Datei plus eine Ein-Zeilen-Entfernung in einer bestehenden
  Bedingung (`came.c`). Kein bestehender State wird geteilt, keine Signaturänderung an von anderen
  Protokollen genutzten Funktionen.
- Die eine Änderung in `came.c` ist funktional notwendig, nicht nur kosmetisch: ohne sie würden
  `came.c` und `prastel.c` beide auf dasselbe 42-Bit-Signal ansprechen (identisches Timing/Header),
  was zu unvorhersagbarem Verhalten führen kann (welcher der beiden Callbacks zuerst feuert, hängt
  von der Registry-Reihenfolge ab) — deshalb habe ich sie in den Port aufgenommen statt nur
  `prastel.c/.h` beizulegen.
- Restrisiko bei der Portierung selbst: die Bit-Verwürflung wurde host-seitig gegen sich selbst
  geprüft (siehe oben), aber nicht gegen einen echten Handsender-Mitschnitt — sollte meine
  Transkription trotz der Prüfung einen Fehler enthalten, äußert sich das als falsche Seriennummer-
  /Zähler-Anzeige, nicht als Speicherfehler (alle Operationen sind Byte-Arithmetik auf festen
  7-Byte-Puffern, keine dynamische Länge, kein Overflow-Potential).
- Build (`fbt`/`ufbt`) deckt jede verbleibende Signaturabweichung als Compile-Fehler auf.
- Funktionale Prüfung nur mit einem echten Prastel-42-Bit-Handsender möglich (lokal, nicht Teil
  dieses Repos).
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko über das hinaus, was jeder SubGHz-Rolling-Code-
  Handler in meiner Basis ohnehin hat.

## Port-Skizze
1. `firmware-mining/prastel-42bit-rolling-code-port/prastel.c` und `.h` nach
   `lib/subghz/protocols/` kopieren.
2. In `lib/subghz/protocols/came.c`, `subghz_protocol_decoder_came_feed()`, die
   `(instance->decoder.decode_count_bit == PRASTEL_42_COUNT_BIT) ||`-Zeile aus der
   `CameDecoderStepSaveDuration`-Akzeptanzbedingung entfernen (siehe „Betroffene Dateien" oben für
   den genauen Kontext).
3. `protocol_items.h`: `#include "prastel.h"` ergänzen.
4. `protocol_items.c`: `&subghz_protocol_prastel,` im Registry-Array ergänzen.
5. Build: `ufbt` (bzw. `fbt firmware_all`) — deckt jede verbleibende Struct-/Signaturabweichung
   sofort als Compile-Fehler auf.
6. Funktionstest lokal: Empfang eines echten Prastel-42-Bit-Handsenders → prüfen, ob der neue
   Decoder anspringt (Protokollname „Prastel", nicht mehr „CAME"/unklassifiziert) und
   Seriennummer/Taste plausibel anzeigt; zwei aufeinanderfolgende Tastendrücke desselben Senders
   sollten denselben Seriennummer-/Tasten-Wert, aber unterschiedliche Zähler-Werte zeigen.
7. Optional: Senden testen (Replay bzw. mit inkrementiertem Zähler) an einer eigenen/autorisierten
   Toranlage; Menüeintrag für manuelles Erzeugen erst danach ergänzen.

## Quelle
- Unleashed-Firmware, `dev`-Branch, Commit `945234a76701d556346f131c88f29f0feeec4d52`
  (2026-09-10, API 88.9): `lib/subghz/protocols/prastel.c/.h`,
  `lib/subghz/protocols/came.c` (Vergleichsstelle), `CHANGELOG.md` (Ankündigung „Prastel 42 bit is
  now a rolling code").
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/came.c` (Ausgangszustand — 42 Bit dort
  als Fixed-Code registriert), `came_atomo.c` (Referenzstruktur für Alloc/Free/Custom-Button/
  Zähler-Muster ohne die neuere Unleashed-Infrastruktur), `lib/subghz/blocks/generic.h`,
  `custom_btn.h`/`custom_btn_i.h` (Signaturabgleich).
