# Texkom (13,56-MHz-Zutrittsschlüssel): Protokollkern + Poller + Listener/Signal (Phase 1-3 von 5)

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
2. Phase 1 (bereits in einer früheren Sitzung geliefert) — den vollständigen, unveränderten
   Protokollkern (`texkom.c/.h/_i.h`), als fertig zum Übernehmen bereite Dateien in
   `firmware-mining/texkom-nfc-protocol-core-port/`, host-verifiziert gegen eigene, aus der
   Header-Dokumentation abgeleitete Testvektoren
   (Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro.c`).
3. Phase 2 (vorherige Sitzung) — den vollständigen, unveränderten Poller
   (`texkom_poller.c/.h/_defs.h/_i.h`), ebenfalls fertig zum Übernehmen in
   `firmware-mining/texkom-nfc-protocol-core-port/`, host-verifiziert gegen den bereits
   host-verifizierten Phase-1-Protokollkern mit einer gemockten `nfc_texkom_poller_rx()`
   anstelle der noch nicht portierten Phase-4-`furi_hal`-Schicht
   (Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_poller.c`).
4. Phase 3 (diese Sitzung) — den vollständigen, unveränderten Listener
   (`texkom_listener.c/.h/_defs.h/_i.h`) sowie den vollständigen, unveränderten
   `DigitalSequence`-Signalpreset (`texkom_signal.c/.h`), ebenfalls fertig zum Übernehmen
   in `firmware-mining/texkom-nfc-protocol-core-port/`. Beide host-verifiziert — der
   Signalpreset differentiell gegen den bereits host-verifizierten Phase-1-Decoder
   (encode → gemessene Lückenliste → decode → muss den Ausgangsrahmen ergeben, exakt die
   seit Phase 1 vorgesehene Technik), der Listener gegen ein gemocktes
   `Nfc`/`BitBuffer`/`nfc_listener_tx()` (Repro in
   `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_listener.c`).

Low-Level-RF und UI liefere ich bewusst **nicht** als Patch — siehe "Risiko" und
"Port-Skizze".

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
- `lib/nfc/protocols/texkom/texkom_poller.c` (104 Zeilen), `texkom_poller.h` (23 Zeilen),
  `texkom_poller_defs.h` (13 Zeilen), `texkom_poller_i.h` (27 Zeilen) — zusammen 167
  Zeilen, Poller-Zustandsautomat: `alloc/free/set_callback/get_data/run/detect`, reicht
  jede erfolgreich abgetastete Lückenliste an `texkom_decode_intervals()` (Phase 1)
  weiter. Unverändert gegen einen frischen `dev`-Stand übernommen (siehe "Quelle").
  Nutzt zwei Symbole, die bei mir noch nicht existieren und erst mit Phase 4 real
  implementiert sind — `NfcTechTexkom` (neuer `NfcTech`-Enum-Wert, `nfc/nfc.h`) und
  `nfc_texkom_poller_rx()` (neue Funktion, `nfc/nfc.c`, intern nur ein Aufruf von
  `furi_hal_nfc_poller_rx()` plus `bit_buffer_copy_bits()`) — ohne diese beiden
  Anschlusspunkte compiliert diese Datei nicht im eigenen Baum, siehe "Risiko" und
  "Port-Skizze". Die Poller-eigene Kontrolllogik selbst (welches Ereignis abgetastet
  wird, wann der Callback aufgerufen wird, was bei Fehler/Timeout mit den vorherigen
  Daten passiert) ist davon unabhängig und bereits jetzt host-verifiziert (siehe
  "Verifikation").
- `lib/nfc/protocols/texkom/texkom_listener.c` (72 Zeilen), `texkom_listener.h` (17 Zeilen),
  `texkom_listener_defs.h` (12 Zeilen), `texkom_listener_i.h` (19 Zeilen) — zusammen 120
  Zeilen, Emulations-Zustandsautomat: `alloc/free/set_callback/get_data/run`. `run()`
  reagiert ausschließlich auf `NfcEventTypeListenerTick`, kopiert dabei `data->frame`
  (die rohen 8 Rahmen-Byte, unabhängig von Variante/Codierung) in einen eigenen
  `BitBuffer` und übergibt ihn unverändert an die generische `nfc_listener_tx()`.
  Bemerkenswert und für Phase 4 vorgemerkt (siehe "Risiko"): diese Datei ruft
  `texkom_signal_tx()` **nicht** selbst auf und übergibt auch nirgends eine
  `TexkomCoding`/`TexkomSignalCoding` — die beiden hier gelieferten Dateien
  (Listener, Signalpreset) sind in diesem Entwicklungsstand strukturell unabhängig
  und treffen sich erst innerhalb der noch nicht portierten Phase-4-Schicht
  (`furi_hal_nfc_texkom.c`), die aus den rohen Bytes, die `nfc_listener_tx()`
  entgegennimmt, offenbar selbst wieder die Codierung ableiten muss (zum Beispiel über
  denselben Rahmen-Typbyte-Blick, den `texkom_type_from_frame()` in Phase 1 schon nutzt) —
  unverändert gegen einen frischen `dev`-Stand übernommen (siehe "Quelle").
- `lib/digital_signal/presets/nfc/texkom_signal.c` (196 Zeilen) / `.h` (65 Zeilen) —
  `DigitalSequence`-Preset, das einen 8-Byte-Rahmen in eine Impuls-Zeitfolge für die
  Emulation umsetzt (Kehrfunktion zu den Phase-1-Decodern): zwei fest kodierte Symbol-Sätze
  (TK13: zwei Lückenlängen je nach Bitwert; TK17: vier Lückenpaare je nach Zweibit-Wert),
  je ein Abschlusssymbol pro Codierung. Nimmt einen `GpioPin` direkt entgegen (kein
  `Nfc`-Objekt) — bestätigt, dass diese Schicht von der Low-Level-RF-Schicht (Phase 4)
  aus aufgerufen wird, nicht vom Listener selbst. Unverändert gegen einen frischen
  `dev`-Stand übernommen (siehe "Quelle").
- Keine Abhängigkeit zu einer bei mir fehlenden Datei für beide: `nfc/protocols/
  nfc_listener_base.h`, `nfc/nfc_listener.h`, `digital_signal/digital_sequence.h`,
  `furi_hal_resources.h` sind bei mir bereits vorhanden.

**Für künftige Sitzungen (Phase 4+, nicht Teil dieses Patches):**
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
Listener-/`furi_hal`-Schichten bzw. echte Hardware, siehe "Risiko"): das tatsächliche
Abtasten eines Feldes selbst, die Signalerzeugung für die Emulation,
`texkom_load()`/`texkom_save()` (reiner `flipper_format_*`-Durchgriff ohne eigene Logik —
dasselbe Muster, das bereits jedes andere bei mir portierte NFC-Protokoll nutzt, daher
kein neues Risiko).

### Phase 2 — Poller (diese Sitzung)

`texkom_poller.c` enthält reine Kontrolllogik ohne eigene Bit-/Byte-Mathematik: es ruft
einmal `nfc_texkom_poller_rx()` auf, reicht das Ergebnis unverändert an
`texkom_decode_intervals()` (Phase 1) weiter und entscheidet anhand von Erfolg/Fehler, ob
der Callback ausgelöst wird. Die einzige Hardware-Grenze der Datei ist genau dieser eine
Aufruf — für den Host-Test durch einen pro Szenario fest programmierten Mock ersetzt
(entweder ein erfolgreicher Capture mit einer festen Lückenliste oder `NfcErrorTimeout`),
exakt die in der vorherigen Sitzung in der Port-Skizze vorgesehene Technik.

Host-Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_poller.c`
(`texkom_poller.c/.h/_i.h` unverändert transkribiert, `furi_assert()` → `assert()`,
`FURI_LOG_D` auf No-Op; der bereits in `repro.c` verifizierte Phase-1-Protokollkern ist
zur eigenständigen Übersetzungseinheit erneut mit hineinkopiert, nicht neu geprüft),
unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` bei `-O0`/`-O1`: **46 Prüfungen, alle bestanden**,
identischer Output beider Optimierungsstufen, keine Compiler-Warnungen, keine
Sanitizer-Befunde.

1. **Alloc/Free-Lebenszyklus**: `nfc_config()` wird beim Allozieren genau einmal mit
   `(nfc, NfcModePoller, NfcTechTexkom)` aufgerufen; frische Daten sind blank
   (`TexkomTypeUnknown`); `general_event`/`texkom_event` sind bereits beim Allozieren
   korrekt verdrahtet, bevor je ein Capture-Versuch stattfand; Alloc/Free sind ausgeglichen
   (kein Leck, kein Doppel-Free — ASan hätte beides gemeldet).
2. **Event-Filter**: ein Ereignis mit einem anderen Typ als `NfcEventTypePollerReady` lässt
   `run()`/`detect()` sofort zurückkehren (`NfcCommandContinue`/`false`), ohne
   `nfc_texkom_poller_rx()` oder den Callback je aufzurufen.
3. **TK13 Ende-zu-Ende über `run()`**: ein erfolgreicher Capture löst den Callback genau
   einmal aus, mit korrektem `NfcGenericEvent` (`protocol`/`instance`/`event_data`);
   `get_data()` spiegelt den dekodierten Rahmen; `run()`s Rückgabewert ist exakt das, was
   der Callback zurückgegeben hat (nicht hartkodiert) — belegt, dass das Kommando
   durchgereicht, nicht synthetisiert wird.
4. **TK17 über `detect()`**: derselbe Erfolgsfall, aber über `detect()` — liefert `true`,
   `get_data()` spiegelt ihn, und anders als `run()` wird der Callback dabei nie
   aufgerufen (kein Callback-Konzept in `detect()`).
5. **Erfolgreicher Capture, aber von keinem Decoder akzeptierte Daten** (alle Lücken
   gleich lang — reißt bereits `texkom_average_gaps()`s `max<=min`-Schutz): `read()`
   schlägt fehl, `run()` bleibt bei `NfcCommandContinue`, `detect()` bei `false`, ohne den
   Callback aufzurufen und ohne den vorherigen erfolgreichen Decode anzutasten.
6. **`NfcErrorTimeout` bricht ab, bevor der Lückenpuffer überhaupt angesehen wird**: der
   Mock liefert einen Timeout, während der Puffer noch gültig aussehende, decodierbare
   Lücken aus einem früheren erfolgreichen Aufruf enthält — `get_data()` bleibt exakt
   unverändert, belegt, dass die `if(error != NfcErrorNone) return false;`-Prüfung
   tatsächlich zuerst greift statt in eine erneute Dekodierung der Altdaten
   durchzufallen.
7. **Alle vier Varianten (TK13, TK15-Kodierung, TK17, MMBIT) Ende-zu-Ende über
   `run()`→Callback→`get_data()`**, jeweils mit denselben, bereits in `repro.c`
   verifizierten Vektoren — der Poller behandelt keine Variante gesondert, er ruft immer
   nur den einen Decode-Dispatch auf.
8. **Zeiger-Identität von `general_event`/`texkom_event` bleibt über mehrere
   aufeinanderfolgende erfolgreiche `run()`-Aufrufe stabil** (einmalig beim Allozieren
   verdrahtet, nie pro Aufruf neu gebaut) — ein Dauerfeld-Tag sendet denselben Rahmen
   wiederholt, der Poller baut dafür nicht jedes Mal ein neues Event-Objekt.

### Phase 3 — Listener und Signalpreset (diese Sitzung)

Beide gelieferten Dateien sind, wie in "Betroffene Dateien" benannt, in diesem
Entwicklungsstand strukturell unabhängig voneinander: `texkom_listener.c` reicht nur die
rohen Rahmen-Byte an die generische `nfc_listener_tx()` weiter, ohne `texkom_signal_tx()`
je selbst aufzurufen oder eine Codierung zu übergeben; `texkom_signal.c` nimmt einen
`GpioPin` direkt entgegen, nicht das `Nfc`-Objekt des Listeners. Beide host-verifiziert,
aber als zwei getrennte Testgruppen, weil das exakt die tatsächliche Code-Struktur ist —
keine künstliche Kopplung, die es im portierten Code so nicht gibt.

**Signalpreset, differentiell gegen den Phase-1-Decoder:** `texkom_signal_tx()` ist die
Kehrfunktion zu den in Phase 1 bereits host-verifizierten Dekodierfunktionen — ein Rahmen
hineingegeben, muss nach dem erneuten Decodieren der erzeugten Lückenfolge byte-exakt
wieder herauskommen. Die eine Hardware-Grenze der Datei (`DigitalSignal`/
`DigitalSequence`, `GpioPin`) ist für den Host-Test durch einen Mock ersetzt, der nur
`(Pegel, Dauer-in-Takt-Einheiten)` pro hinzugefügter Periode aufzeichnet — die reale
`TEXKOM_SIGNAL_FC_64`-Skalierung (Takt-Einheiten auf die von `DigitalSignal` erwartete
10-ps-Auflösung) bleibt im transkribierten Code unverändert; der Mock rechnet sie beim
Aufzeichnen nur zurück, da für den Soll-Ist-Vergleich mit dem Decoder (der auf
`uint8_t`-Takt-Einheiten arbeitet) ausschließlich die Verhältnisse zwischen den Lücken
zählen, nicht die reale GPIO-Zeitbasis.

Dabei ergab das Zusammenlesen von `texkom_signal.c` (diese Sitzung) mit dem bereits
bekannten `texkom_i.h`-Kommentar "TK13 sends 128 impulses, TK17 sends 65" (Phase 1,
seinerzeit als gegebene Tatsache übernommen) eine Erklärung, die ich vorher nicht hatte:
`texkom_signal_fill()` hängt an das TK17-Abschlusssymbol (anders als an das TK13-Symbol)
einen zusätzlichen Impuls *vor* die Park-Periode. Genau dieser eine zusätzliche Impuls ist
es, der die letzte Lücke des letzten TK17-Symbolpaars überhaupt erst messbar macht (sie
bekommt dadurch ein definiertes Ende statt im unbegrenzten Sendepausenbereich zu verlaufen)
— weshalb TK17 alle 64 Lücken verwertet (`TEXKOM_TK17_INTERVALS`), während TK13 ohne dieses
zusätzliche Abschlusssymbol die letzte der 128 Lücken verwerfen muss (`TEXKOM_TK13_INTERVALS
= 127`). Beide Zahlen waren seit Phase 1 als Konstanten bekannt; der Grund für ihre
Asymmetrie ergab sich erst jetzt aus dem Signalpreset.

Host-Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_listener.c`,
Szenarien 1-5 (Signalpreset):

1. **TK13-Rundlauf**, 8 UIDs (Null, Eins, sechs gemischte Muster):
   `make_blank`+`set_uid` → `texkom_signal_tx(Tk13)` → die ersten 127 Low-Perioden als
   gemessene Lücken → `texkom_decode_intervals()` → Rahmen und Typ (`TexkomTypeTk13`, nicht
   `Tk15` — saubere Zeiten bleiben auf dem Schwellwert-Pfad) stimmen exakt.
2. **TK17-Rundlauf**, dieselben 8 UIDs: `texkom_signal_tx(Tk17)` → die ersten 64
   Low-Perioden → decodiert → Rahmen und Typ (`TexkomTypeTk17`) stimmen exakt.
3. **MMBIT-Rundlauf**, 8 Drei-Byte-UIDs: MMBIT nutzt dieselbe TK13-Codierung
   (`texkom_features[Mmbit].coding`) → `texkom_signal_tx(Tk13)` → dieselbe 127-Lücken-
   Extraktion → decodiert → Rahmen und Typ (`TexkomTypeMmbit`) stimmen exakt; da
   `set_uid()` dabei auch die CRC neu setzt, wäre ein falsch mitkodiertes CRC-Byte
   ebenfalls aufgefallen.
4. **Negativ** (prüft das Testgeschirr selbst): eine aufgezeichnete Lücke wird vor dem
   Decodieren auf einen bewusst mehrdeutigen Wert (exakt zwischen Kurz- und Langschwelle)
   gesetzt — `texkom_decode_intervals()` muss ablehnen; ein zweiter, unveränderter
   Rundlauf direkt danach muss unverändert gelingen (kein Zustand bleibt zwischen Aufrufen
   hängen).
5. **Alloc/Free-Lebenszyklus**: zwei unabhängige `TexkomSignal`-Instanzen, verschachtelt
   gesendet (inklusive einem zweiten Sendevorgang auf derselben Instanz), in umgekehrter
   Reihenfolge freigegeben — ASan hätte ein Leck, ein Doppel-Free oder eine
   Use-after-free-Referenz auf eine registrierte Signal-Instanz gemeldet.

Szenarien 6-8 (Listener, gemocktes `Nfc`/`BitBuffer`/`nfc_listener_tx()`):

6. **Alloc/Free**: `nfc_config()` wird beim Allozieren genau einmal mit
   `(nfc, NfcModeListener, NfcTechTexkom)` aufgerufen; `get_data()` liefert exakt den beim
   Allozieren übergebenen Datenzeiger zurück, unverändert; `set_callback()` übernimmt
   Callback und Kontext unverändert.
7. **`run()` bei `NfcEventTypeListenerTick`** sendet die 8 Rahmen-Byte byte-exakt genau
   einmal über `nfc_listener_tx()` (unabhängig von Variante/Codierung — diese Schicht sieht
   keine von beiden); ein Ereignis eines anderen Typs sendet nicht; der Rückgabewert ist in
   beiden Fällen `NfcCommandContinue`.
8. **`run()` bei einem gemeldeten Sendefehler** (`NfcErrorTimeout`) sendet trotzdem genau
   einmal, protokolliert nur (No-Op-Mock für `FURI_LOG_E`) und liefert unverändert
   `NfcCommandContinue` — kein Wiederholungs- oder Abbruchpfad, den ein Test übersehen
   könnte, weil die Quelle selbst keinen hat.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: **178 Prüfungen, alle bestanden**, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde. Gegenprobe zur
Absicherung des Repros selbst: eine mutierte Kopie mit vertauschter Null-/Eins-Symbolwahl
in `texkom_signal_fill()` lässt 53 der 178 Prüfungen scheitern (die Rundlauf- und
Lebenszyklus-Szenarien, die tatsächlich eine korrekte Bitzuordnung voraussetzen) — das
Repro ist also empfindlich für genau die Fehlerklasse, die ein Portierungsfehler in dieser
Datei wäre. Eine zweite, geringwertigere Mutation (TK13-Langlücke um 1 Takt-Einheit
verändert, bei unveränderter Kurzlücke) lässt dagegen alle 178 Prüfungen weiterhin
bestehen — erwartungsgemäß, da `texkom_average_gaps()` seine Schwellwerte selbst aus den
gemessenen Lücken herleitet und daher gegen eine konsistent falsche, aber in sich
proportionale absolute Taktrate unempfindlich ist; dieselbe Selbstkalibrierung, die eine
echte Marke bei leicht abweichender Taktrate noch lesbar macht. Beide Gegenproben
bestätigen, dass dieses Repro echte Bitzuordnungsfehler fängt, ohne bei belanglosen
Konstantenabweichungen falsch auszuschlagen — keine der beiden Mutationen ist Teil des
gelieferten Repros, nur zur Absicherung dieser Sitzung durchgeführt und verworfen.

**Nicht** durch diesen Host-Test abgedeckt: die tatsächliche GPIO-Zeitbasis
(`TEXKOM_SIGNAL_FC_64`-Skalierung auf echte 10-ps-Perioden), `digital_sequence_transmit()`
selbst (reine Hardware-Ausgabe, kein eigener Code in `texkom_signal.c`), sowie alles, was
über `nfc_listener_tx()` hinaus tatsächlich Hochfrequenz erzeugt — das ist durchgängig
Phase 4.

## Risiko

- **Niedrig** für den gelieferten, host-verifizierten Phase-1-Protokollkern: rein additive
  neue Dateien, keine Änderung an einer bestehenden Datei, kein geteilter Zustand, keine
  Registrierung in der Protokoll-Registry (die kommt erst mit Phase 2+). Reine
  Bit-/Byte-Mathematik ohne Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge.
- **Niedrig** für die gelieferte, host-verifizierte Phase-2-Poller-Kontrolllogik selbst:
  rein additive neue Dateien, keine Änderung an einer bestehenden Datei, keine eigene
  Bit-/Byte-Mathematik (reiner Dispatch auf den bereits verifizierten Phase-1-Kern).
  **Mittel** bleibt nur die Integration in den eigenen Baum: `NfcTechTexkom` (neuer
  `NfcTech`-Enum-Wert) und `nfc_texkom_poller_rx()` (neue Funktion in `nfc/nfc.c`) existieren
  bei mir noch nicht — beide sind trivial additiv (ein Enum-Wert, eine
  Dispatch-Tabellenzeile, eine Wrapper-Funktion), ihre tatsächliche Implementierung ruft
  aber direkt in die noch nicht portierte Phase-4-`furi_hal`-Schicht hinein, landet also
  sinnvollerweise erst zusammen mit Phase 4.
- **Niedrig** für den gelieferten, host-verifizierten Phase-3-Listener und -Signalpreset
  selbst: rein additive neue Dateien, keine Änderung an einer bestehenden Datei, kein
  geteilter Zustand, der Signalpreset ist reine, differentiell gegen Phase 1 geprüfte
  Bit-/Byte-Mathematik ohne Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge,
  der Listener reiner Dispatch ohne eigene Logik. **Mittel** bleibt die Integration: wie
  bei Phase 2 fehlt `NfcTechTexkom`; zusätzlich noch offen (siehe "Betroffene Dateien"),
  *wie* die Phase-4-Schicht, die `nfc_listener_tx()`s rohe Byte letztlich entgegennimmt,
  an die für die Sendung nötige `TexkomCoding`/`TexkomSignalCoding` kommt, da der Listener
  selbst keine mitführt — das ist ein offener Klärungspunkt für Phase 4, keiner, der sich
  aus dem hier gelieferten Code selbst beantworten lässt, und kein Fehler in den beiden
  jetzt gelieferten Dateien (beide sind für sich genommen vollständig und korrekt, wie die
  Verifikation zeigt).
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

1. **Phase 1 (geliefert):** `texkom.c/.h/_i.h` unverändert nach
   `lib/nfc/protocols/texkom/` übernehmen, als neue Quelldateien in die
   `SConscript`/Build-Liste von `lib/nfc` aufnehmen (reine Quelldatei-Ergänzung, kein
   neues Ziel). Baut eigenständig, wird von nichts referenziert, bis Phase 2 beginnt.
2. **Phase 2 (geliefert, diese Sitzung):** `texkom_poller.c/.h/_defs.h/_i.h` unverändert
   nach `lib/nfc/protocols/texkom/` übernehmen, ebenfalls in die Build-Liste aufnehmen.
   Compiliert im eigenen Baum erst, sobald die zwei kleinen Phase-4-Anschlusspunkte
   existieren (`NfcTechTexkom`-Enum-Wert, `nfc_texkom_poller_rx()`), ist aber bereits
   jetzt — unabhängig von Phase 4 — host-seitig gegen eine feste, gemockte Lückenliste
   durchgetestet (siehe "Verifikation"), exakt die hier zuvor vorgesehene Technik.
3. **Phase 3 (geliefert, diese Sitzung):** `texkom_listener.c/.h/_defs.h/_i.h` plus
   `texkom_signal.c/.h` unverändert nach `lib/nfc/protocols/texkom/` bzw.
   `lib/digital_signal/presets/nfc/` übernehmen, in die jeweilige Build-Liste aufnehmen.
   Der Listener compiliert im eigenen Baum erst mit demselben fehlenden
   `NfcTechTexkom`-Anschlusspunkt wie Phase 2; der Signalpreset ist davon unabhängig und
   bräuchte nur ein `GpioPin`. Beide bereits jetzt — unabhängig von Phase 4 — host-seitig
   verifiziert (siehe "Verifikation"), der Signalpreset per differentiellem Encode/Decode
   gegen Phase 1, exakt die seit Phase 1 vorgesehene Technik.
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
- Vorherige Sitzung erneut gegen einen frischen, unabhängigen `dev`-Stand geprüft, Commit
  `b510f625dd5a8962d9129b6bb17dc8a73c90fa07` (2026-10-18): `texkom.c/.h/_i.h`
  (Phase 1, bereits übernommen) per `diff` Byte-für-Byte weiterhin identisch zum oben
  genannten Stand bestätigt — keine Änderung seit der letzten Sichtung. Die
  Poller-Dateien (Phase 2) stammen von diesem Stand:
  `lib/nfc/protocols/texkom/texkom_poller.c/.h/_defs.h/_i.h`. Zusätzlich
  `lib/nfc/nfc.h/.c` als Fundort von `NfcTechTexkom`/`nfc_texkom_poller_rx()` (Phase-4-
  Anschlusspunkte, siehe "Betroffene Dateien") sowie `lib/nfc/protocols/nfc_generic_event.h`/
  `nfc_poller_base.h` (gemeinsame, bei mir bereits unverändert vorhandene
  Abstraktionsschicht, über die jeder Poller — nicht nur Texkom — an das `Nfc`-Objekt
  angebunden wird) gelesen, um die Testumgebung für `repro_poller.c` exakt an den
  tatsächlichen Signaturen auszurichten.
- Diese Sitzung erneut gegen einen frischen, unabhängigen `dev`-Stand geprüft, Commit
  `3c6d6fa34f0be567d49a61a4c4c04dc2eb98f2d7` (2026-10-19, per `git ls-remote` auf den
  aktuellen `dev`-Zweigkopf bestimmt): `texkom.c/.h/_i.h` (Phase 1) und
  `texkom_poller.c/.h/_defs.h/_i.h` (Phase 2) per `diff` Byte-für-Byte weiterhin identisch
  zu den oben genannten Ständen bestätigt — keine Änderung seit der letzten Sichtung. Die
  Listener-/Signalpreset-Dateien (Phase 3, neu in dieser Sitzung) stammen von diesem
  Stand: `lib/nfc/protocols/texkom/texkom_listener.c/.h/_defs.h/_i.h`,
  `lib/digital_signal/presets/nfc/texkom_signal.{c,h}` — jeweils zweimal unabhängig
  geladen und per `diff`/`md5sum` byte-für-byte identisch bestätigt.
- Eigene Firmware-Basis (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, Tag `v2.0.0`/Momentum `mntm-012`):
  `lib/nfc/protocols/nfc_device_base_i.h`, `nfc_common.h`, `lib/one_wire/maxim_crc.{c,h}`,
  `lib/flipper_format/flipper_format.h` — alle gegen Unleashed `dev` Byte-für-Byte
  identisch bestätigt. `lib/nfc/protocols/nfc_protocol.c` als Integrationspunkt für
  Phase 5 identifiziert (Texkom dort bereits als eigenständiger Basisknoten,
  Geschwisterknoten von ISO14443-3A/3B/Felica/ISO15693-3/ST25TB).
- `texkom.h` selbst zitiert als Kodierungsquelle die Proxmark3-Funktion
  `TexcomTK13CRC` (TK13-CRC-8-Poly `0x31`).
