# Texkom (13,56-MHz-Zutrittsschlüssel): Protokollkern fehlt komplett

## Was

Ich unterstütze aktuell keine Texkom-Schlüssel (TK13/TK15/TK17/MMBIT) — 13,56-MHz-Tags,
die keinen der ISO-14443/15693-Standards implementieren: keine Anti-Kollision, kein
Befehlssatz. Im Feld lastmoduliert der Tag fortlaufend einen kurzen Impuls, die Nutzlast
steckt in den Pausen zwischen den Impulsen. Jede Variante trägt denselben 8-Byte-Rahmen
(`FF FF <Typ> <UID 4 Byte> <CRC>`, MMBIT als Ausnahme mit nur 3 UID-Byte, da der zweite
Rahmenbyte dort ebenfalls `FF` ist). Dieser Punkt stand seit der `unlshd-093`→`dev`-Diff-
Sichtung als größte, mehrteilige NFC-Protokollfamilie auf meiner Rückstellliste und wurde
seither in jeder Firmware-Feature-Mining-Sitzung zugunsten kleinerer, vollständig
abschließbarer Kandidaten zurückgestellt.

Ich habe die Quelle (Unleashed, `dev`-Branch, Commit `f62964cb8216c566cb3940992d0fea6bbe91d840`,
2026-10-16) jetzt vollständig gesichtet statt sie erneut zu verschieben: 18 Dateien,
insgesamt 2040 Zeilen, über fünf Schichten:

| Schicht | Dateien | Zeilen |
|---|---|---|
| Protokollkern (Codec, kein Hardwarebezug) | `lib/nfc/protocols/texkom/texkom.c/.h/_i.h` | 798 |
| Poller (Zustandsautomat, NFC-HAL) | `lib/nfc/protocols/texkom/texkom_poller*.{c,h}` (4 Dateien) | 167 |
| Listener (Emulation, NFC-HAL) | `lib/nfc/protocols/texkom/texkom_listener*.{c,h}` (4 Dateien) | 122 |
| Signalerzeugung (`DigitalSequence`-Preset) | `lib/digital_signal/presets/nfc/texkom_signal.c/.h` | 261 |
| Low-Level-RF (`furi_hal`, Transparent-Mode) | `targets/f7/furi_hal/furi_hal_nfc_texkom.c` | 527 |
| App-Anbindung (UI-Helfer) | `applications/main/nfc/helpers/protocol_support/texkom/*.{c,h}` (4 Dateien) | 165 |

Mit 798 von 2040 Zeilen trägt allein der Protokollkern knapp 40 % — und ist zugleich die
einzige Schicht ohne jeden Hardware-/`furi_hal`-Bezug: reine Datenstruktur, CRC-Prüfung,
Lade-/Speicherlogik (delegiert nur an bereits vorhandene generische `flipper_format_*`-
Helfer) und drei Intervall-Decoder (TK13, TK15, TK17), die ausschließlich mit `uint8_t`-
Lückenlisten rechnen — exakt das, was sich ohne Flipper-Hardware auf dem Host verifizieren
lässt. Ich liefere in dieser Sitzung deshalb, analog zum bereits etablierten Muster für
mehrteilige NFC-Protokollfamilien (MIFARE-Plus-/Ultralight-AES-Port):

1. Diesen Port-Vorschlag mit vollständiger Bestandsaufnahme aller fünf Schichten, Risiko
   und gestufter Port-Skizze für die verbleibenden Sitzungen.
2. Phase 1 — den vollständigen, unveränderten Protokollkern (`texkom.c/.h/_i.h`), als
   fertig zum Übernehmen bereite Dateien in `firmware-mining/texkom-nfc-protocol-core-port/`,
   host-verifiziert gegen eigene, aus der Header-Dokumentation abgeleitete Testvektoren
   (Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro.c`).

Poller, Listener, Signalerzeugung, Low-Level-RF und UI liefere ich bewusst **nicht** als
Patch — siehe "Risiko" und "Port-Skizze".

## Nutzen

- Schließt eine bislang komplett fehlende Kartenkategorie: Texkom-Schlüssel sind in
  Russland/GUS als Gegensprechanlagen-/Hausanlagen-Zutrittsschlüssel verbreitet — ein
  13,56-MHz-Gegenstück zu den bereits von mir unterstützten 125-kHz-Zutrittsformaten
  (EM-Marin, Cyfral, Hitag, Hitag S). Ohne diesen Port erkennt mein Lesegerät solche Tags
  überhaupt nicht (keine der unterstützten ISO-Familien antwortet, weil der Tag selbst
  nie auf einen Befehl wartet).
- Der jetzt gelieferte Protokollkern ist die gemeinsame Grundlage für Lesen *und*
  Emulation (Poller und Listener greifen beide auf dieselben `texkom_decode_intervals()`/
  `texkom_is_crc_valid()`/`texkom_make_blank()`-Funktionen zurück) — ihn jetzt schon
  host-seitig korrekt zu wissen, entkoppelt die beiden größeren, noch ausstehenden
  Zustandsautomaten-Sitzungen von der Frage "stimmt die Bit-/CRC-Logik überhaupt".
- Reine Lese-/Emulationsfähigkeit für eigene, autorisiert getestete Schlüssel — kein
  neues Angriffsprimitiv: Texkom kennt kein Authentifizierungsgeheimnis, die UID liegt
  im Klartext im Rahmen (wie bei den bereits unterstützten 125-kHz-Formaten), es gibt
  nichts zu "knacken".

## Betroffene Dateien (gestuft, siehe Port-Skizze)

**Jetzt geliefert (Phase 1, siehe Verifikation):**
- `lib/nfc/protocols/texkom/texkom.c` (614 Zeilen) — Datenstruktur (`TexkomData`,
  `NfcDeviceBase`-Vtable `nfc_device_texkom`), drei CRC-Varianten (TK13: eigenes CRC-8
  Poly `0x31`; TK17: Dallas/Maxim-CRC-8 über `maxim_crc8()`; MMBIT: genibbelter XOR),
  Lade-/Speicherlogik (`texkom_load/_save`, reine `flipper_format_*`-Wrapper), UID-Zugriff
  (`texkom_get_uid/_set_uid`, variantenabhängiger Offset/Länge), `texkom_make_blank()`
  sowie die drei Intervall-Decoder (`texkom_decode_tk13/_tk15/_tk17`) und ihr Dispatch
  (`texkom_decode_intervals`).
- `lib/nfc/protocols/texkom/texkom.h` (96 Zeilen) — öffentliche API (siehe oben),
  `TexkomType`-Enum, Frame-Konstanten.
- `lib/nfc/protocols/texkom/texkom_i.h` (88 Zeilen) — interne API (`TexkomCoding`-Enum,
  `texkom_update_crc`, `texkom_make_blank`, `texkom_decode_intervals`), von Poller/
  Listener (Phase 2/3) mitgenutzt.
- Keine Abhängigkeit zu einer bei mir fehlenden Datei: `nfc/protocols/nfc_device_base_i.h`,
  `nfc/nfc_common.h` (inkl. `NFC_UNIFIED_FORMAT_VERSION`), `one_wire/maxim_crc.h/.c` und
  `flipper_format/flipper_format.h` sind bei mir bereits vorhanden und — Byte-für-Byte
  per `diff` gegen Unleashed `dev` geprüft — identisch.

**Für künftige Sitzungen (Phase 2+, nicht Teil dieses Patches):**
- `lib/nfc/protocols/texkom/texkom_poller.c/.h`, `texkom_poller_defs.h`,
  `texkom_poller_i.h` (167 Zeilen) — Poller-Zustandsautomat; ruft `furi_hal_nfc_texkom_*`
  (Phase 4) zum Abtasten auf und reicht die Lückenliste an `texkom_decode_intervals()`
  (bereits Phase 1) weiter.
- `lib/nfc/protocols/texkom/texkom_listener.c/.h`, `texkom_listener_defs.h`,
  `texkom_listener_i.h` (122 Zeilen) — Emulationsseite; baut aus `TexkomData` ein
  Sendesignal über die `texkom_signal`-Presets (Phase 3) und `furi_hal_nfc_texkom_*`
  (Phase 4).
- `lib/digital_signal/presets/nfc/texkom_signal.c` (196 Zeilen) / `.h` (65 Zeilen) —
  `DigitalSequence`-Preset, das einen `TexkomData`-Rahmen in eine Impuls-Zeitfolge für die
  Emulation umsetzt (Kehrfunktion zu den Phase-1-Decodern).
- `targets/f7/furi_hal/furi_hal_nfc_texkom.c` (527 Zeilen) — Low-Level-RF: fährt den
  STM32WB55-NFC-Analogfrontend im "Transparent Mode" (digitaler Framing-Pfad umgangen,
  MISO/MOSI direkt verdrahtet), eigener 64-MHz-Timer für eine 500-ksps-Abtastrate beim
  Lesen, präkompiliertes Signal beim Senden. Laut Datei-Kopfkommentar hat ein früherer
  Versuch, stattdessen die IRQ-Leitung wie beim ISO15693-Listener abzutasten, beim
  Pollen kein einziges Flankenereignis erzeugt — Texkom braucht also nachweislich diesen
  eigenen Pfad, kein Wiederverwenden eines bestehenden.
- `applications/main/nfc/helpers/protocol_support/texkom/texkom.c/.h`,
  `texkom_render.c/.h` (165 Zeilen) — UI-Anbindung (Kartentyp-Icon, Infoanzeige), reine
  Boilerplate nach demselben Muster wie alle bereits bei mir integrierten NFC-Protokolle.
- `lib/nfc/protocols/nfc_protocol.c/.h` (bei mir bereits vorhanden, zu erweitern) —
  Registrierung als neuer, eigenständiger Basisknoten `NfcProtocolTexkom` im Protokollbaum
  (Geschwisterknoten von ISO14443-3A/3B/Felica/ISO15693-3/ST25TB, keine Kindprotokolle) —
  rein additiv: ein neuer Enum-Wert, ein neuer Baumeintrag, keine Änderung an einem
  bestehenden Protokollknoten.

## Verifikation (auf dem Host, ohne Hardware)

Phase 1 ist reine Bit-/Byte-Logik ohne Zeiger auf variable Puffer außerhalb der von der
jeweiligen Funktion selbst kontrollierten Länge und ohne jeden `furi_hal`-Bezug — ich habe
die gesamte Datei (614 Zeilen) gegen genau dieses Kriterium gelesen und **keinen eigenen
Fund** (keinen Speicherfehler, keine Off-by-one-Stelle) identifiziert: alle Feld-/
Array-Zugriffe sind an zur Übersetzungszeit bekannte Konstanten gebunden, die exakt den
tatsächlichen Puffergrößen entsprechen (`raw[128]`, `bits[64]`, `frame[8]`, jeweils mit
Schleifengrenzen, die exakt diese Größen treffen); `malloc()`-Größe und alle
`furi_check()`-Nullprüfungen sind durchgängig korrekt.

Host-Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro.c` (reine
Transkription der Datei, `furi_check()` → `assert()`, `maxim_crc8()` aus
`lib/one_wire/maxim_crc.c` unverändert mitkopiert — vorher Byte-für-Byte als identisch zu
meiner eigenen Basis bestätigt), unter
`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: **57 Prüfungen, alle bestanden**, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

1. **TK13-Rundlauf über den öffentlichen Dispatch** (`texkom_make_blank` → `texkom_set_uid`
   → eigene Rahmen-Bits in 127 synthetische Lücken mit klar getrennten Längen kodiert →
   `texkom_decode_intervals`) — Rahmen bit-exakt wiederhergestellt, Typ `TK13`.
2. **TK13→TK15-Rückstufung**: dieselbe Kodierung, aber mit eng beieinanderliegenden,
   überlappenden Lückenlängen auf der zweiten Rahmenhälfte — zwingt `texkom_decode_tk13()`
   über die globale Mittelwert-Schwelle zu einem gleichen Lückenpaar und damit zum
   dokumentierten Abbruch (explizit gegengeprüft: `decode_tk13()` schlägt tatsächlich
   fehl, nicht nur zufällig richtig); der Dispatch fällt daraufhin auf
   `texkom_decode_tk15()` (paarweiser Vergleich statt absoluter Schwelle) zurück und
   liefert denselben Rahmen bit-exakt zurück, jetzt als Typ `TK15` — die einzige Stelle
   der Datei, an der dieselben Bits je nach Zeitgenauigkeit unterschiedlich klassifiziert
   werden, deshalb als eigenes Szenario statt im Rundlauf mitgeprüft.
3. **TK17-Rundlauf über den öffentlichen Dispatch**: Zweibit-pro-Lückenpaar-Kodierung samt
   der bytelokalen Paar-Vertauschung von Hand invertiert, Rahmen bit-exakt
   wiederhergestellt, Typ `TK17`.
4. **MMBIT**: 3-Byte-UID, High-Nibble-only-CRC-Maske — eine Änderung am Low-Nibble
   (Nutzlast laut Kommentar) lässt `texkom_is_crc_valid()` wie dokumentiert gültig,
   eine Änderung am High-Nibble (Prüfsumme) macht sie ungültig; `texkom_set_uid()` weist
   eine falsche UID-Länge für die Variante zurück.
5. **Negativ, Preamble**: ein verändertes erstes Rahmenbit (`FF FF` → `7F FF`) wird
   abgelehnt; direkt im Anschluss decodiert derselbe unveränderte Rahmen weiterhin
   korrekt — kein Seiteneffekt durch den vorherigen Fehlschlag.
6. **Negativ, Typ-/Codierungs-Inkonsistenz**: ein Rahmen mit TK17-Typbyte, aber über die
   TK13-127-Lücken-Kodierung gesendet — die Bits werden bit-exakt decodiert (die Kodierung
   selbst kennt keine Typbytes), aber `texkom_type_from_frame()` verwirft die Kombination
   wie dokumentiert, und `texkom_decode_intervals()` insgesamt liefert `false`.
7. **Negativ, TK17-Decoder einzeln**: ein Lückenpaar-Anteil außerhalb von [10,90] sowie ein
   Lückenpaar mit Summe 0 werden beide ohne Division durch 0 (bereits im Original durch
   `if(total == 0) return false;` abgefangen) zurückgewiesen.
8. **CRC-Konsistenz** für alle drei bekannten Varianten plus `TexkomTypeUnknown`:
   `texkom_get_expected_crc()`/`texkom_is_crc_valid()`/`texkom_update_crc()` stimmen in
   jedem Fall überein; `TexkomTypeUnknown` hat erwartungsgemäß keine bekannte CRC.

**Nicht** durch diesen Host-Test abgedeckt (erfordert die noch nicht portierten
Poller-/Listener-/`furi_hal`-Schichten bzw. echte Hardware, siehe "Risiko"): das
tatsächliche Abtasten eines Feldes, die Signalerzeugung für die Emulation,
`texkom_load()`/`texkom_save()` selbst (reiner `flipper_format_*`-Durchgriff ohne eigene
Logik — dasselbe Muster, das bereits jedes andere bei mir portierte NFC-Protokoll nutzt,
daher kein neues Risiko).

## Risiko

- **Niedrig** für den gelieferten, host-verifizierten Phase-1-Protokollkern: rein additive
  neue Dateien, keine Änderung an einer bestehenden Datei, kein geteilter Zustand, keine
  Registrierung in der Protokoll-Registry (die kommt erst mit Phase 2+). Reine
  Bit-/Byte-Mathematik ohne Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge.
- **Mittel** für Poller/Listener (Phase 2/3): neue, aber in sich abgeschlossene
  Zustandsautomaten, die ausschließlich über die bereits verifizierten Phase-1-Funktionen
  mit dem Protokollkern sprechen — das übliche Risiko eines neuen NFC-Zustandsautomaten
  (falsch verdrahtete Übergänge zeigen sich meist erst an echter Hardware), aber ohne
  eigene Kryptografie oder Authentifizierung, die zusätzlich falsch sein könnte.
- **Mittel-hoch** für die Low-Level-RF-Schicht (Phase 4, `furi_hal_nfc_texkom.c`):
  einziger Teil, der den STM32WB55-NFC-Analogfrontend direkt im "Transparent Mode"
  ansteuert (digitaler Framing-Pfad umgangen) — ein Fehler hier wirkt sich potenziell auf
  die HF-Erzeugung selbst aus, nicht nur auf die Dekodierung, und ist nicht durch reine
  Quelltextprüfung abschließend bewertbar, sondern nur an echter Hardware mit einem
  Spektrumanalysator/Oszilloskop zur Kontrolle der tatsächlichen Feldcharakteristik. Die
  bei mir bereits vorhandenen Grundlagen dafür (`furi_hal_nfc_i.h`, `furi_hal_nfc_tech_i.h`,
  `lib/signal_reader/signal_reader.h`, Transparent-Mode bereits für ISO14443A/ISO15693
  genutzt) sind vorhanden, was eine künftige Portierung grundsätzlich machbar macht, aber
  die genauen Register-/Timing-Sequenzen müssen in einer eigenen, auf diese eine Datei
  fokussierten Sitzung sorgfältig gegen meine Basis gegengerechnet werden, bevor ein Patch
  sinnvoll ist — das ist nach meiner Einschätzung der eigentliche Grund, warum dieser
  Punkt bislang in jeder Sitzung zugunsten kleinerer, risikoärmerer Kandidaten
  zurückgestellt wurde.
- **Kein zusätzliches Missbrauchsrisiko**: Texkom kennt kein Authentifizierungsgeheimnis;
  Lesen und Emulieren eines bekannten/eigenen Schlüssels ist strukturell identisch zum
  bereits unterstützten 125-kHz-Zutrittsformat-Modell (EM-Marin/Cyfral/Hitag), nur auf
  13,56 MHz statt 125 kHz.

## Port-Skizze (mehrteilig, für künftige Sitzungen gestuft)

1. **Phase 1 (dieser Patch):** `texkom.c/.h/_i.h` unverändert nach
   `lib/nfc/protocols/texkom/` übernehmen, als neue Quelldateien in die
   `SConscript`/Build-Liste von `lib/nfc` aufnehmen (reine Quelldatei-Ergänzung, kein
   neues Ziel). Baut eigenständig, wird von nichts referenziert, bis Phase 2 beginnt.
2. **Phase 2:** `texkom_poller.c/.h/_defs.h/_i.h` — Poller-Zustandsautomat, ruft
   `furi_hal_nfc_texkom_rx_start()`/-ähnliche Funktionen (Phase 4) zum Abtasten; bis
   Phase 4 fertig ist, kann der Zustandsautomat selbst bereits gegen eine
   host-generierte, feste Lückenliste durchgetestet werden (dieselbe Technik wie im
   Phase-1-Host-Repro).
3. **Phase 3:** `texkom_listener.c/.h/_defs.h/_i.h` plus `texkom_signal.c/.h` —
   Emulationsseite; die Signalerzeugung (Kehrfunktion zu den Phase-1-Decodern) lässt sich
   ebenfalls host-seitig gegen die Phase-1-Decoder differenziell prüfen (encode → decode
   → muss den Ausgangsrahmen ergeben), bevor sie an echte Hardware geht.
4. **Phase 4:** `furi_hal_nfc_texkom.c` — eigene, auf diese eine Datei fokussierte
   Sitzung; Register-/Timing-Konstanten (64-MHz-Timer, 500-ksps-Abtastrate,
   Transparent-Mode-Konfiguration) Zeile für Zeile gegen meine bereits vorhandene
   `furi_hal_nfc_iso14443a.c`/`furi_hal_nfc_iso15693.c` (nutzen laut eigener Prüfung
   bereits denselben Transparent-Mode-Pfad) gegenrechnen, bevor ein Patch vorgeschlagen
   wird — höchstes Risiko dieser gesamten Portierung, siehe "Risiko".
5. **Phase 5:** `lib/nfc/protocols/nfc_protocol.c/.h` — `NfcProtocolTexkom` als neuen
   Basisknoten registrieren (ein Enum-Wert, ein Baumeintrag), plus
   `applications/main/nfc/helpers/protocol_support/texkom/*` (UI-Anbindung) — letzter
   Schritt, erst sinnvoll sobald Phase 1-4 lokal bauen.
6. Bauen nach jeder Phase: `ufbt` (bzw. `fbt firmware_all`) — jede Struct-/
   Signaturabweichung fällt sofort als Compile-Fehler auf.
7. Funktionstest lokal (ab Phase 2 mit Hardware nötig, Phase 1 bereits host-verifiziert):
   eigenen Texkom-Schlüssel (TK13/TK15/TK17/MMBIT, je nach Verfügbarkeit) lesen, Rahmen
   gegen ein unabhängiges Referenzwerkzeug (Proxmark3, falls vorhanden) vergleichen; nach
   Phase 3 Emulation gegen ein zweites eigenes Lesegerät testen.

## Quelle

- Unleashed-Firmware, `dev`-Branch, Commit `f62964cb8216c566cb3940992d0fea6bbe91d840`
  (2026-10-16): `lib/nfc/protocols/texkom/`, `lib/digital_signal/presets/nfc/
  texkom_signal.{c,h}`, `targets/f7/furi_hal/furi_hal_nfc_texkom.c`,
  `applications/main/nfc/helpers/protocol_support/texkom/`.
- Eigene Firmware-Basis (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, Tag `v2.0.0`/Momentum `mntm-012`):
  `lib/nfc/protocols/nfc_device_base_i.h`, `nfc_common.h`, `lib/one_wire/maxim_crc.{c,h}`,
  `lib/flipper_format/flipper_format.h` — alle gegen Unleashed `dev` Byte-für-Byte
  identisch bestätigt. `lib/nfc/protocols/nfc_protocol.c` als Integrationspunkt für
  Phase 5 identifiziert (Texkom dort bereits als eigenständiger Basisknoten,
  Geschwisterknoten von ISO14443-3A/3B/Felica/ISO15693-3/ST25TB).
- `texkom.h` selbst zitiert als Kodierungsquelle die Proxmark3-Funktion
  `TexcomTK13CRC` (TK13-CRC-8-Poly `0x31`).
