# Security+ 2.0: 86-Bit-Keypad-Frame fehlt komplett

## Was

Mein `lib/subghz/protocols/secplus_v2.c` (Momentum-Basis, Tag `mntm-012`) decodiert
ausschließlich das 62-Bit-Halbpaket, das Security+-2.0-Handsender senden. LiftMaster-/
Chamberlain-Funktastaturen (Keypads) desselben Protokolls senden stattdessen ein 86-Bit-
Rahmenformat mit eigenem Vorlauf, eigenem Sync-Gap und einer zusätzlichen 4-stelligen PIN
im Nutzdatenwort — dafür gibt es bei mir aktuell keinerlei Erkennung. Ein empfangenes
Keypad-Signal läuft komplett durch, ohne dass `secplus_v2` (oder ein anderer bei mir
registrierter Decoder) anspringt; es erscheint bestenfalls als unklassifiziertes RAW-Signal.

Unleashed-Firmware (`dev`, Commit `945234a76701d556346f131c88f29f0feeec4d52`) hat dieselbe
Datei um genau diese 86-Bit-Erkennung erweitert (86 statt 62 Bit, 22-Bit-Präambel `0x3C`,
eigener Vorlauf-Puls, breiteres Sync-Gap-Toleranzfenster, 18- statt 10-Bit-Mischpuffer,
zusätzliches 16-Bit-Datenfeld inkl. darin verpackter PIN, XOR-Prüfsumme über die acht
Nibbles gegen den Button-Wert). Das ist einer der drei in der letzten Prastel-Sitzung
zurückgestellten SubGHz-Kandidaten (`firmware-mining/prastel-42bit-rolling-code-port.md`).

Ich liefere in dieser Sitzung:
1. Diesen Port-Vorschlag mit vollständiger Analyse und konkreter Port-Skizze.
2. Den wiederverwendbaren, reinen Bit-Mathematik-Kern (Mix-/Order-Funktionen, Halbpaket-
   Encode/Decode, PIN-Extraktion) 1:1 aus Unleashed übernommen, umbenannt und eigenständig
   auf dem Host gegen sich selbst verifiziert (keine Hardware, keine Zeiger auf variable
   Puffer — reine Integer-Bit-Operationen, siehe „Verifikation" unten).

Die letzte, hardwarenahe Verdrahtung (Zustandsautomat in `feed()`, Struct-Erweiterung,
Registry-Feld `min_count_bit_for_found`, Präambel-/Header-Erkennung für den 86-Bit-Fall)
liefere ich bewusst **nicht** als fertigen Patch — dazu unten mehr unter „Risiko" und
„Port-Skizze". Das ist kein „klein & sicher"-Fund im Sinne von `WORKFLOW.md`, sondern ein
mehrteiliger Feature-Port; der verifizierte Kern plus die konkrete Skizze sollen die lokale
Fertigstellung leicht machen.

## Nutzen

- Security+-2.0-Funktastaturen sind bei LiftMaster/Chamberlain-Torantrieben weit verbreitet
  (dieselbe Produktfamilie, deren Handsender ich bereits über `secplus_v2` decodiere) —
  aktuell eine Erkennungslücke bei der SubGHz-Signalanalyse eigener/autorisierter Anlagen.
- Reine Erweiterung der Empfangs-/Analysefähigkeit für ein bereits unterstütztes,
  öffentlich dokumentiertes Protokoll (`github.com/argilo/secplus`, dieselbe Quelle, die
  schon im Datei-Header meines bestehenden `secplus_v2.c` referenziert ist); kein neues
  Angriffsprimitiv — die 62-Bit-Handsender-Seite desselben Protokolls decodiere ich bereits.
- Das Nutzdatenwort trägt bei einer Funktastatur die eingegebene 4-stellige PIN im Klartext
  (byte-vertauscht) — direkt aus dem ohnehin empfangenen Signal ablesbar, kein zusätzlicher
  Aufwand gegenüber dem, was mit jedem SDR/`rtl_433` ebenfalls möglich ist.

## Betroffene Dateien

- `lib/subghz/protocols/secplus_v2.c` — einzige betroffene Datei. Anders als beim
  Prastel-Port ist keine Änderung an einer registry- oder protokollfremden Datei nötig:
  86-Bit- und 62-Bit-Rahmen unterscheiden sich schon anhand der Bitzahl/Präambel, es gibt
  kein Kollisionsrisiko mit einem anderen Decoder wie beim Prastel/CAME-Fall.
- `lib/subghz/protocols/secplus_v2.h` — nur falls neue öffentliche Funktionen (z. B. ein
  PIN-Lesezugriff für eine eigene Anzeige/App) nach außen exportiert werden sollen; für die
  reine Erkennung im Timing-/Anzeige-Pfad selbst nicht zwingend.
- `protocol_items.c`/`.h`: **keine Änderung** — `secplus_v2` ist bereits registriert.

## Verifikation (auf dem Host, ohne Hardware)

Aus Unleasheds `secplus_v2.c` sind folgende Funktionen reine, zustandslose
Integer-Bit-Manipulation ohne Zeiger auf variable Puffer, ohne Logging-Abhängigkeit und
ohne Bezug zum Zustandsautomaten — genau der Teil, der sich unabhängig vom Rest des Ports
prüfen lässt:

- `mix_invet()` (bei mir bereits als `subghz_protocol_secplus_v2_mix_invet()` vorhanden,
  aber fest auf `uint16_t`/Maske `0x3FF` für den 62-Bit-Fall zugeschnitten) — Unleashed
  generalisiert auf `uint32_t` plus expliziten Masken-Parameter, um dieselbe Funktion für
  62 und 86 Bit zu nutzen. Für den Port lege ich bewusst eine **eigenständige** `_86`-Variante
  an (siehe „Risiko") statt die bestehende, im 62-Bit-Pfad safety-kritische Funktion
  umzubauen.
- `mix_order_decode()`/`mix_order_encode()` — dieselbe Überlegung, ebenfalls als `_86`-Variante.
- `decode_half_86()`/`encode_half_86()` — komplett neu (kein 62-Bit-Gegenstück bei mir).
- `pin_extract()`/`pin_apply()` — komplett neu.

Host-Repro in `firmware-mining/repro-secplus-v2-keypad-frame/repro.c`
(`gcc -O2 -Wall -Wextra -fsanitize=address,undefined`), Ergebnis `ALL CHECKS PASSED`:

1. `mix_invet_86()` ist für alle 9 vom `switch` behandelten `invert`-Werte eine Involution
   (zweimalige Anwendung liefert das Original zurück) — 9 Werte × 1000 zufällige
   18-Bit-Puffer.
2. `mix_order_decode_86(mix_order_encode_86(order, p))` liefert für alle 9 behandelten
   `order`-Werte wieder `p` — 9 Werte × 1000 zufällige Puffer.
3. `encode_half_86()` → `decode_half_86()` reproduziert `roll_array`/`fixed`/`data_part`
   bitgenau über 200 000 zufällige gültige Eingaben (Rollen-Digits als Ternärziffern
   `{0,1,2}`, `fixed` 20 Bit, `data_part` 16 Bit) — 0 Verwerfungen, 0 Abweichungen. Dabei
   analytisch hergeleitet und durch den Test bestätigt: die "Tail wiederholt Head"-
   Konsistenzprüfung in `decode_half_86()` hält für jede von `encode_half_86()` erzeugte
   Nutzlast automatisch (beide Digit-Gruppen stammen aus denselben vier
   `roll_array[0..3]`-Werten, siehe Kommentare im Repro).
4. Das Präambel-/Paket-ID-Bit (`data >> 62 == 1`) ist ein unbedingtes Gate — Löschen dieses
   Bits wird immer verworfen, unabhängig vom Rest der Nutzlast (1 gezielter Fall).
5. Zufällige Einzelbit-Verfälschung einer gültigen Nutzlast (5000 Versuche) reproduziert nie
   unbemerkt das Original — entweder wird verworfen, oder das Ergebnis weicht ab (die
   eigentliche Integritätsprüfung über beide Rahmenhälften, die 8-Nibble-XOR-Prüfsumme in
   `remote_controller_86()`, ist bewusst nicht Teil dieses isolierten Tests, siehe
   Repro-Kommentar).
6. `pin_extract(pin_apply(x, pin)) == pin` für den vollständigen 4-stelligen Bereich
   (0000–9999, 10 000 Werte, 0 Abweichungen); ein High-Halbwort > 9999 wird korrekt als
   „keine gültige PIN" (`SECPLUS_V2_PIN_UNSET`) erkannt statt als falsche PIN
   fehlinterpretiert.

Das deckt die Bit-Mathematik vollständig ab. **Nicht** durch diesen Host-Test abgedeckt
(erfordert die Zustandsautomat-Integration bzw. echte Hardware, siehe „Risiko"): Timing-
Erkennung der 86-Bit-Präambel/des breiteren Sync-Gaps aus echten Pegel-/Dauer-Ereignissen,
Verhalten bei gemischtem Empfang (62- und 86-Bit-Sender abwechselnd), die
8-Nibble-XOR-Prüfsumme über beide Hälften hinweg (`remote_controller_86()`-Äquivalent).

## Risiko

- **Niedrig** für den gelieferten, host-verifizierten Bit-Mathematik-Kern: rein additive
  neue statische Funktionen mit `_86`-Suffix, keine Änderung an einer bestehenden Funktion,
  keine Signaturänderung, kein geteilter State. Fehlerbild bei einem übersehenen Fehler in
  diesem Teil wäre eine falsch angezeigte PIN/Seriennummer, kein Speicherfehler (alle
  Operationen sind Bit-Arithmetik auf festen `uint32_t`/`uint64_t`-Werten).
- **Mittel-hoch** für die noch ausstehende Zustandsautomat-Integration — bewusst nicht als
  fertiger Patch geliefert:
  - `subghz_protocol_decoder_secplus_v2_feed()` (`lib/subghz/protocols/secplus_v2.c:722`
    in meiner Basis) gated die Paket-Fertigstellung aktuell hart auf
    `instance->decoder.decode_count_bit == subghz_protocol_secplus_v2_const.min_count_bit_for_found`
    (fest `62`, `SubGhzBlockConst` bei `secplus_v2.c:26`). Für 86-Bit-Rahmen muss diese
    Schwelle **pro Sync-Versuch** unterscheidbar sein (62 oder 86), sonst wird ein
    86-Bit-Rahmen entweder nie fertig erkannt oder ein 62-Bit-Rahmen fälschlich als
    unvollständiger 86-Bit-Rahmen behandelt. Das erfordert ein zusätzliches Zustandsfeld
    (z. B. `expected_count_bit` in `SubGhzProtocolDecoderSecPlus_v2`), das im `Reset`-Schritt
    abhängig davon gesetzt wird, welches der beiden Sync-Gap-Fenster (siehe unten) getroffen
    hat.
  - Der `Reset`-Schritt akzeptiert aktuell nur ein festes Gap-Fenster
    (`te_long * 130`, Toleranz `te_delta * 100`, siehe `secplus_v2.c:730`). Unleashed nutzt
    für 86-Bit-Rahmen ein zweites, breiteres Fenster (Mittelwert `te_long * 179`, Toleranz
    `te_long * 72`, s. `SECPLUS_V2_SYNC_86`/`_DELTA` in Unleasheds Datei) **und** zusätzlich
    einen 2-Ereignis-Vorlauf (ein `te_short`-High-Puls plus `te_long`-Low-Gap **vor** der
    eigentlichen Manchester-Präambel), den ein 62-Bit-Handsender nicht sendet. Beide Fenster
    müssen im `Reset`-Schritt parallel geprüft werden, ohne dass sich ihre Toleranzbereiche
    gegenseitig falsch positiv auslösen (in Unleasheds Werten überlappungsfrei: 130±100 vs.
    179±72 grenzen bei ca. 107–130 bzw. 130–251 in `te_long`-Einheiten aneinander — vor der
    Übernahme mit den eigenen `te_delta`-Werten gegenrechnen).
  - `subghz_protocol_secplus_v2_check_packet()` (`secplus_v2.c:709`) prüft aktuell
    `SECPLUS_V2_HEADER`/`_MASK` auf dem vollen 62-Bit-Wert. Für 86-Bit-Rahmen ist die
    Präambel nur 22 Bit breit (`0x3C` in den obersten 8 Bit dieser 22 Bit, plus 2-Bit
    Paket-ID direkt danach) — eine eigene Prüfung auf dem 86-Bit-Wert nötig, keine
    Wiederverwendung der bestehenden Maske.
  - Struct-Erweiterung: `secplus_packet_1_count_bit` (oder gleichwertig) nötig, damit eine
    gespeicherte erste Hälfte nicht fälschlich mit einer zweiten Hälfte anderer Bitbreite
    kombiniert wird (Unleashed führt genau dieses Feld für denselben Zweck ein, siehe
    Kommentar in dessen Struct-Definition).
  - Jede Abweichung von der bestehenden 62-Bit-Struct-Größe/-Reihenfolge fällt beim Bauen
    (`fbt`/`ufbt`) sofort als Compile-Fehler auf, sofern `SUBGHZ_ASSERT_DECODER_COMMON_LAYOUT`
    (bereits in meiner Basis vorhanden, `secplus_v2.c` nach der Struct-Definition) weiterhin
    verwendet wird.
- **Kein zusätzliches Missbrauchsrisiko** über das hinaus, was der bereits vorhandene
  62-Bit-Decoder ohnehin hat: reine Signalanalyse/-decodierung, kein automatisiertes
  Senden fremder Codes, keine Bruteforce-Komponente.

## Port-Skizze

1. `firmware-mining/repro-secplus-v2-keypad-frame/repro.c` als Referenz für die reinen
   Funktionen nehmen; die sechs `_86`-Funktionen (`mix_invet_86`, `mix_order_decode_86`,
   `mix_order_encode_86`, `decode_half_86`, `encode_half_86`, `pin_extract`/`pin_apply`)
   nach `lib/subghz/protocols/secplus_v2.c` übernehmen (FURI_LOG_E statt printf für die
   Fehlerfälle, wie im Rest der Datei üblich). Zusätzlich `subghz_protocol_secplus_v2_rolling_join_86()`
   ergänzen — analog zur bestehenden Interleave-Logik in
   `subghz_protocol_secplus_v2_remote_controller()` (`secplus_v2.c:292`), aber für 9-statt-9
   Ternärziffern in der von Unleashed dokumentierten Reihenfolge (siehe Kommentar in dessen
   `rolling_join()`).
2. Struct `SubGhzProtocolDecoderSecPlus_v2` (`secplus_v2.c:32`) um
   `uint8_t secplus_packet_1_count_bit`, `uint32_t secplus_data` (86-Bit-Datenwort) und
   ein Feld für die pro Sync erwartete Bitzahl erweitern.
3. `Reset`-Schritt in `feed()` (`secplus_v2.c:726`) um das zweite Sync-Gap-Fenster
   (86-Bit) erweitern, inkl. Setzen des neuen "erwartete Bitzahl"-Felds; dabei den
   2-Ereignis-Vorlauf einer Funktastatur berücksichtigen (Unleashed löst das über einen
   zusätzlichen Zähler vor dem eigentlichen Manchester-Sync — Details in dessen `feed()`).
4. `DecoderData`-Schritt: die feste `min_count_bit_for_found`-Prüfung
   (`secplus_v2.c:763`) durch einen Vergleich gegen das neue "erwartete Bitzahl"-Feld
   ersetzen.
5. `check_packet()` (`secplus_v2.c:709`) um den 86-Bit-Präambel-/Paket-ID-Fall erweitern
   (eigene Konstanten, eigener Zweig — bestehenden 62-Bit-Zweig unverändert lassen).
6. `subghz_protocol_secplus_v2_get_string()` (`secplus_v2.c:981`) um die PIN-Anzeige
   ergänzen (nur wenn ein 86-Bit-Rahmen erkannt wurde), analog zu Unleasheds
   `get_string()` (dessen Zeilen ca. 1419–1460).
7. `serialize()`/`deserialize()` (`secplus_v2.c:828`/`850`) um das optionale `"Pin"`-Feld
   und das 86-Bit-Datenwort erweitern, wie in Unleasheds `deserialize()` (Zeile ca. 940–960)
   vorgemacht.
8. Bauen: `ufbt` (bzw. `fbt firmware_all`) — jede verbleibende Struct-/Signaturabweichung
   fällt sofort als Compile-Fehler auf.
9. Funktionstest lokal: Signal einer echten Security+-2.0-Funktastatur empfangen (eigene/
   autorisierte Anlage) → Protokollname `secplus_v2` sollte anspringen (statt unklassifiziert
   zu bleiben), `86b` statt `62b` in der Bitzahl-Anzeige, PIN-Feld sichtbar bei
   PIN-Eingabe am Keypad. Regressionstest: ein bereits funktionierender 62-Bit-Handsender
   muss unverändert weiter decodieren (Sn/Btn/Cnt identisch zu vorher).

## Quelle

- Unleashed-Firmware, `dev`-Branch, Commit `945234a76701d556346f131c88f29f0feeec4d52`
  (2026-09-10, API 88.9): `lib/subghz/protocols/secplus_v2.c` (86-Bit-Erweiterung).
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/secplus_v2.c`/`.h`
  (Ausgangszustand — nur 62-Bit-Erkennung).
- `github.com/argilo/secplus`, `github.com/merbanan/rtl_433` (bereits im Datei-Header
  meiner bestehenden `secplus_v2.c` referenzierte Protokollquellen).
- Zurückgestellter Kandidat aus `firmware-mining/prastel-42bit-rolling-code-port.md`
  (dort als „Security+ 2.0 Keypad" vorgemerkt).
