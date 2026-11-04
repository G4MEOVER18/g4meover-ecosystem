# Texkom (13,56-MHz-Zutrittsschlüssel): Protokollkern + Poller + Listener/Signal + Phase 4 + Phase 5 (vollständig, 5 von 5)

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
5. Phase 4a (diese Sitzung) — die bisher nicht erkannten Voraussetzungen von Phase 4 selbst
   geklärt und als eigene, additive/verhaltenserhaltende Patches ausgeliefert, bevor ich
   mich wie vorgesehen an `furi_hal_nfc_texkom.c` (jetzt Phase 4b) setze. Die ursprüngliche
   Port-Skizze ging davon aus, Phase 4 bestünde nur aus dieser einen Datei; die für diese
   Sitzung vorgesehene Zeile-für-Zeile-Registerprüfung hat stattdessen drei vorgelagerte
   Lücken offengelegt, ohne die `furi_hal_nfc_texkom.c` bei mir nicht einmal baut (siehe
   "Betroffene Dateien" und "Verifikation"):
   - `NfcTechTexkom`/`FuriHalNfcTechTexkom` (zwei neue Enum-Werte) und
     `nfc_texkom_poller_rx()` (neue Funktion) — exakt die beiden in Phase 2/3 schon benannten
     Anschlusspunkte, jetzt tatsächlich geliefert.
   - `furi_hal_nfc_transparent_mode_enter/_exit` — bei mir existierten diese Funktionen
     bislang nur ISO15693-Listener-lokal (`static`, in `furi_hal_nfc_iso15693.c`); Texkom
     braucht sie aber generisch (Poller *und* Listener). Verallgemeinert als Patch: Körper
     unverändert nach `furi_hal_nfc.c` gehoben, `furi_hal_nfc_iso15693.c` ruft jetzt die
     generische Fassung.
   - `lib/signal_reader/signal_reader.c` unterstützt freilaufendes Abtasten
     (`SignalReaderTriggerNone`, von Texkom für das unaufgefordert sendende Tag benötigt)
     bei mir zwar im Enum, aber `start()`/`stop()` schalten trotzdem immer in den
     EXTI-getriggerten Pfad — bei mir unbemerkt, weil `signal_reader` aktuell von keinem
     einzigen eigenen Aufrufer genutzt wird (siehe "Verifikation"). Patch macht den
     freilaufenden Pfad tatsächlich frei laufend.

   Alle drei sind rein additiv bzw. nachweislich verhaltenserhaltend (siehe "Verifikation"),
   gegen zwei bzw. drei frische, unabhängige Zweitkopien meiner eigenen Basis per
   `git apply --check`/`git apply`/`md5sum` bestätigt — identisch.

6. Phase 4b (diese Sitzung) — `furi_hal_nfc_texkom.c` selbst, unverändert übernommen
   (527 Zeilen, erneut per `md5sum` gegen den in Phase 4a bereits geprüften Stand
   bestätigt — unverändert seit letzter Sichtung). Mit der Registerlage bereits aus
   Phase 4a geklärt, war für diese Sitzung die noch offene Frage, ob die Datei neben der
   reinen Registerkonfiguration auch eigene, host-prüfbare Bit-/Byte-Logik enthält — sie
   tut: die Abtast-/Zeitmess-Zustandsmaschine (`furi_hal_nfc_texkom_feed()`,
   `_handle_impulse()`, `_cut_frame()`), die aus dem vom Analogfrontend digitalisierten
   Bitstrom die Lückenliste baut, ist reine, hardwareunabhängige Kontrolllogik — exakt
   dieselbe Kategorie wie Phase 2s Poller-Dispatch. Jetzt erstmals host-verifiziert
   (Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_furi_hal_texkom.c`),
   zusammen mit zwei kleineren, ebenfalls reinen Dispatch-Funktionen
   (`furi_hal_nfc_texkom_coding()`, `furi_hal_nfc_texkom_listener_tx()`s
   Rahmengrößen-/Codierungs-Dispatch, mit gemocktem `texkom_signal_tx()`). Damit ist von
   den fünf Schichten dieser Portierung nur noch die reine Registerkonfiguration selbst
   (acht Registerschreibvorgänge, davon sieben bereits in Phase 4a als wortwörtliche
   Wiederverwendung bestätigt) und der eine verbleibende Punkt (`RX_CONF3`) nicht
   host-prüfbar — siehe "Verifikation" und "Risiko".

`furi_hal_nfc_texkom.c` selbst liefere ich jetzt unverändert als fertig zum Übernehmen
bereite Datei (siehe "Betroffene Dateien") — mit `RX_CONF3` weiterhin als einzigem, nur an
Hardware zu klärendem Punkt (siehe "Risiko"). Die UI-Anbindung (Phase 5) weiterhin nicht
Teil dieser Sitzung.

## Update (Phase 5, diese Sitzung) — letzter Schritt: Protokollbaum-Registrierung und UI

Phase 5 schließt diese Portierung ab. Geliefert sind jetzt alle fünf geplanten Phasen:
`lib/nfc/protocols/nfc_protocol.c/.h` (neuer Basisknoten `NfcProtocolTexkom`),
`lib/nfc/protocols/nfc_device_defs.c/nfc_poller_defs.c/nfc_listener_defs.c` (Dispatch-Tabellenzeilen
auf die bereits in Phase 1-4b gelieferten Dateien), `lib/nfc/SConscript` (Export der drei
öffentlichen Header), sowie die UI-Anbindung selbst
(`applications/main/nfc/helpers/protocol_support/texkom/texkom.c/.h`, `texkom_render.c/.h`).

Beim Nachvollziehen der ursprünglich als "ein Enum-Wert, ein Baumeintrag, plus die
UI-Anbindung" benannten Port-Skizze (siehe "Port-Skizze", Punkt 6) sind zwei bislang nicht
benannte, aber tatsächlich nötige Anschlussstellen aufgefallen, ohne die ein Texkom-Tag die
App zum Absturz bringen bzw. überhaupt keine UI zeigen würde — siehe "Betroffene Dateien" für
den vollen Umfang:

1. `applications/main/nfc/application.fam` — jedes Protokoll-UI-Modul ist bei mir als
   eigenständiges Plugin (`App(appid="nfc_<protokoll>", apptype=FlipperAppType.PLUGIN, ...)`)
   registriert, das zu einer eigenen `.fal`-Datei unter `apps_data/nfc/plugins/` baut. Ohne
   einen eigenen `nfc_texkom`-Eintrag hier gäbe es nie eine `nfc_texkom.fal` — die unten
   genannte Laufzeit-Ladefunktion fände beim ersten Texkom-Tag schlicht keine Datei.
2. `applications/main/nfc/helpers/protocol_support/nfc_protocol_support.c` —
   `nfc_protocol_support_plugin_names[NfcProtocolNum]`, die Tabelle, die
   `nfc_protocol_support_alloc()` von der Enum-Konstante `NfcProtocolTexkom` auf den
   Datei-Namensteil `"texkom"` übersetzt (`furi_string_alloc_printf(APP_ASSETS_PATH("plugins/
   nfc_%s.fal"), protocol_name)`). Ohne einen eigenen Eintrag bleibt dieser Tabellenplatz ein
   stiller `NULL`-Zeiger (C initialisiert einen nicht benannten Eintrag eines designated-init-
   Arrays automatisch auf 0) — `protocol_name` wäre `NULL`, und `furi_string_alloc_printf()`
   mit einem `NULL`-`%s`-Argument ist undefiniertes Verhalten, in der Praxis ein Absturz beim
   ersten Lesen einer Texkom-Karte, nicht erst beim Senden/Emulieren.

Beide Lücken sind rein additiv (eine neue `App()`-Eintragung, eine neue Tabellenzeile) und
wurden gefunden, indem ich dem tatsächlichen Lade-Code in `nfc_protocol_support.c` gefolgt
bin, statt mich auf die ursprüngliche, zwei Jahre alte Port-Skizzen-Formulierung zu verlassen
— beide Stellen fehlten dort komplett.

**Bewusst nicht mitgenommen:** `nfc_protocol_support.c` selbst hat sich zwischen meinem
Firmware-Stand und dem aktuellen Unleashed-`dev`-Stand erheblich weiterentwickelt (Plugin-
Lade-Fehlerbildschirm `NFC_PROTOCOL_SUPPORT_SCENE_LOAD_FAILED`, `extra_scenes`-Dispatch,
`call_depth`-Wiedereintritts-Schutz, Datei-Umbenennungs-/Schatten-Datei-Fixes bei
`SceneSaveName`, u. a.) — alles unabhängig von Texkom, siehe die eigene `diff` dieser Sitzung
gegen den aktuellen `dev`-Stand. Nur die eine neue Tabellenzeile `[NfcProtocolTexkom] =
"texkom"` ist Texkom-spezifisch und Teil dieses Patches; der Rest bleibt außen vor, exakt das
Prinzip, das Phase 4a bereits für die unabhängige ISO15693-1oo4/1oo256-Änderung im selben
Unleashed-Diff angewendet hat. Ebenso unabhängig von Texkom und bewusst nicht mitgenommen:
`application.fam` selbst trägt inzwischen flächendeckend ein neues `fap_exclude_libs=["gcc"]`
auf praktisch jedem `App()`-Eintrag sowie diverse neue Parser-Plugins/Quellausschlüsse — davon
übernommen ist ausschließlich der neue `nfc_texkom`-Eintrag selbst.

**Nebenfund (nicht behoben, für eine künftige Sitzung vorgemerkt):** beim Diff-Abgleich dieser
Sitzung sind zwei von Texkom unabhängige `memcpy()`-Stellen aufgefallen, die die Elementzahl
statt der Bytezahl übergeben (`lib/nfc/nfc_scanner.c`,
`applications/main/nfc/helpers/nfc_detected_protocols.c` — beide kopieren ein
`NfcProtocol`-Array mit `memcpy(dst, src, count)` statt `memcpy(dst, src, count *
sizeof(NfcProtocol))`). Der aktuelle Unleashed-`dev`-Stand hat beide bereits auf die korrekte
Form gebracht; bei mir sind beide Stellen noch mit der alten, zu kurzen Kopie vorhanden — ein
potenzielles Unterlesen, falls `NfcProtocol` (ein `enum`, typischerweise 4 Byte) bei mehr als
einem erkannten Protokoll zu kurz kopiert. Reine Dokumentations-Randnotiz dieser Sitzung,
nicht Teil dieses Patches (kein Texkom-Bezug) — siehe "Offen" im zugehörigen Report.

Alle Dateien dieser Sitzung sind rein additiv (neue Zeilen/neue Dateien, keine Änderung an
bestehender Logik außerhalb der genannten Tabellenzeilen) und gegen zwei bzw. drei frische,
unabhängige Zweitkopien meiner eigenen Firmware-Basis verifiziert — siehe "Verifikation".

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

**Jetzt geliefert (Phase 4a, diese Sitzung) — additive/verhaltenserhaltende
Voraussetzungen für Phase 4b, keine eigene Protokolldatei:**
- `lib/nfc/nfc.h/.c` — neuer Enum-Wert `NfcTechTexkom` (Dispatch-Tabellen `nfc_tech_table`
  für Poller *und* Listener), neuer Enum-Wert `NfcEventTypeListenerTick` (bewusst ans Ende
  von `NfcEventType` angehängt, da exportierter Header — eine frühere Einfügestelle hätte
  die Werte einer bereits gebauten Anwendung verschoben) plus Weiterleitung im
  Listener-Event-Loop, neue Funktion `nfc_texkom_poller_rx()` (ein Aufruf von
  `furi_hal_nfc_poller_rx()`, Timeout-Fall auf `NfcErrorTimeout` abgebildet, Rest auf
  `nfc_process_hal_error()`, plus eine Zielpuffer-Größenprüfung vor `bit_buffer_copy_bits()`)
  — rein additiv, genau die beiden in Phase 2/3 bereits als "erst mit Phase 4 real
  implementiert" benannten Anschlusspunkte.
- `targets/furi_hal_include/furi_hal_nfc.h` — neuer Enum-Wert `FuriHalNfcTechTexkom`
  (`FuriHalNfcTech`) und `FuriHalNfcEventListenerTick` (`FuriHalNfcEvent`), die Gegenstücke
  der beiden vorigen auf der `furi_hal`-Schicht — ebenfalls rein additiv.
- `targets/f7/furi_hal/furi_hal_nfc_i.h`/`furi_hal_nfc.c` — neue, generische Funktionen
  `furi_hal_nfc_transparent_mode_enter()`/`_exit()` (Deklaration bzw. Definition) plus die
  neue Dispatch-Tabellenzeile `[FuriHalNfcTechTexkom] = &furi_hal_nfc_texkom`. Die beiden
  Funktionskörper sind unverändert aus den zwei weiter unten entfernten
  `furi_hal_nfc_iso15693_listener_transparent_mode_enter/_exit`-Funktionen übernommen —
  keine neue Logik, nur eine neue, höhere Sichtbarkeit für denselben Code (siehe
  "Verifikation").
- `targets/f7/furi_hal/furi_hal_nfc_tech_i.h` — neue `extern`-Deklaration
  `furi_hal_nfc_texkom` (Gegenstück zu `furi_hal_nfc_iso15693`/`furi_hal_nfc_felica` usw.),
  rein additiv.
- `targets/f7/furi_hal/furi_hal_nfc_iso15693.c` — die beiden oben genannten, bislang
  Listener-lokalen `static`-Funktionen entfernt, ihre zwei Aufrufstellen
  (`furi_hal_nfc_iso15693_listener_init/_deinit`) rufen jetzt die neue generische Fassung.
  Byte-für-Byte dieselbe Abfolge von `st25r3916_direct_cmd()`/
  `furi_hal_spi_bus_handle_(de)init()`/`furi_hal_nfc_(de)init_gpio_isr()` wie vorher, nur an
  anderer Stelle definiert — kein Verhaltensunterschied für ISO15693 (siehe "Verifikation").
  Eine im selben Unleashed-`dev`-Diff mitgekommene, zu Texkom unabhängige Funktionsgruppe
  (`furi_hal_nfc_iso15693_detect_mode/_force_1outof4/_force_1outof256`, ISO15693-Parser-
  Modusauswahl) nehme ich bewusst **nicht** mit — siehe "Risiko".
- `lib/signal_reader/signal_reader.c/.h` — `SignalReaderTriggerNone` existierte bei mir
  bereits als Enum-Wert, aber `signal_reader_start()`/`_stop()` haben ihn bislang ignoriert
  und immer den EXTI-getriggerten Pfad (Interrupt-Leitung, DMA-Resync-Kanal, Request-
  Generator) scharfgeschaltet — für ein Signal, das nie eine Rahmenflanke zum Resynchen hat
  (wie Texkoms frei sendender Tag), unnötig und laut neuem Kommentar bei einem 212-kHz-
  Subträger sogar potenziell überlastend (Tausende Anfragen pro Rahmen). Patch fügt ein
  `started_trigger`-Feld hinzu (merkt sich, was `start()` tatsächlich scharfgeschaltet hat,
  damit `stop()` exakt das wieder abschaltet und nicht einen zwischenzeitlich per
  `set_trigger()` geänderten Modus) und verzweigt `start()`/`stop()` jetzt tatsächlich nach
  `SignalReaderTriggerNone` (reiner `GpioModeInput` ohne Interrupt/Resync) gegenüber
  getriggert (unverändertes Verhalten). Keine Abhängigkeit zu einer bei mir fehlenden Datei.

**Jetzt geliefert (Phase 4b, diese Sitzung):**
- `targets/f7/furi_hal/furi_hal_nfc_texkom.c` (527 Zeilen, unverändert übernommen nach
  `firmware-mining/texkom-nfc-protocol-core-port/phase4b-furi-hal/`, md5
  `38c7f707f2d8c9156cdd726068cc9644` — identisch zum in Phase 4a bereits geprüften
  Stand) — Low-Level-RF: fährt den STM32WB55-NFC-Analogfrontend im "Transparent Mode"
  (digitaler Framing-Pfad umgangen, MISO/MOSI direkt verdrahtet), eigener 64-MHz-Timer
  für eine 500-ksps-Abtastrate beim Lesen, präkompiliertes Signal beim Senden. Laut
  Datei-Kopfkommentar hat ein früherer Versuch, stattdessen die IRQ-Leitung wie beim
  ISO15693-Listener abzutasten, beim Pollen kein einziges Flankenereignis erzeugt —
  Texkom braucht also nachweislich diesen eigenen Pfad, kein Wiederverwenden eines
  bestehenden. Die Registerkonfiguration selbst (`furi_hal_nfc_texkom_configure_rx()`)
  ist bereits vollständig in Phase 4a gegen die eigene ISO15693-/ISO14443A-Basis
  gegengerechnet (siehe dortige Tabelle) — unverändert, keine neue Prüfung hier nötig.
  Neu für diese Sitzung: die Abtast-/Zeitmess-Zustandsmaschine
  (`furi_hal_nfc_texkom_feed()`/`_handle_impulse()`/`_cut_frame()`), die aus dem vom
  Analogfrontend auf MISO gelieferten, mit 500 ksps abgetasteten Bitstrom die
  Lückenliste rekonstruiert, die der bereits host-verifizierte Poller (Phase 2) an
  `texkom_decode_intervals()` (Phase 1) weiterreicht — reine, hardwareunabhängige
  Kontrolllogik ohne jeden Registerzugriff, jetzt erstmals host-verifiziert (siehe
  "Verifikation"). Ebenfalls host-verifiziert: `furi_hal_nfc_texkom_coding()` (wählt
  TK17- vs. TK13-Signalcodierung anhand des Rahmen-Typbytes) und der
  Größen-/Codierungs-Dispatch in `furi_hal_nfc_texkom_listener_tx()` (`texkom_signal_tx()`
  selbst bleibt gemockt — bereits in Phase 3 host-verifiziert).
**Jetzt geliefert (Phase 5, diese Sitzung) — Protokollbaum-Registrierung und UI, letzter
Schritt dieser Portierung:**
- `applications/main/nfc/helpers/protocol_support/texkom/texkom.c/.h`,
  `texkom_render.c/.h` (165 Zeilen, unverändert übernommen nach `firmware-mining/
  texkom-nfc-protocol-core-port/phase5-ui/`) — UI-Anbindung (Lese-/Emulations-Szenen,
  Infoanzeige), reine Boilerplate nach demselben Muster wie `st25tb` (byte-für-byte
  dieselbe Struktur bis auf die zusätzliche `NfcProtocolFeatureEmulateFull`, siehe
  "Verifikation").
- `lib/nfc/protocols/nfc_protocol.c/.h` — Registrierung als neuer, eigenständiger
  Basisknoten `NfcProtocolTexkom` im Protokollbaum (Geschwisterknoten von
  ISO14443-3A/3B/Felica/ISO15693-3/ST25TB, keine Kindprotokolle) — rein additiv: ein neuer
  Enum-Wert, ein neuer Baumeintrag, keine Änderung an einem bestehenden Protokollknoten.
- `lib/nfc/protocols/nfc_device_defs.c/nfc_poller_defs.c/nfc_listener_defs.c` — je eine neue
  `#include` und eine neue Dispatch-Tabellenzeile (`[NfcProtocolTexkom] = &nfc_device_texkom`
  / `&nfc_poller_texkom` / `&nfc_listener_texkom`), die auf die bereits in Phase 1-3
  gelieferten, unveränderten Dateien zeigen. In `nfc_listener_defs.c` bewusst **nicht**
  mitgenommen: eine unabhängige, im selben Unleashed-Diff enthaltene Änderung
  (`[NfcProtocolMfPlus] = NULL` → `&mf_plus_listener`) — siehe "Risiko".
- `lib/nfc/SConscript` — die drei öffentlichen Header (`texkom.h`, `texkom_poller.h`,
  `texkom_listener.h`) zur `SDK_HEADERS`-Liste ergänzt, an derselben Stelle wie bei jedem
  anderen bereits integrierten Protokoll.
- `applications/main/nfc/application.fam` — neuer `App(appid="nfc_texkom", ...)`-Eintrag
  (Plugin-Baudefinition für `helpers/protocol_support/texkom/*.c`), ohne den nie eine
  `nfc_texkom.fal` gebaut würde. Dieser Anschlusspunkt stand in der ursprünglichen
  Port-Skizze (Punkt 6) noch nicht — siehe "Update (Phase 5)" oben.
- `applications/main/nfc/helpers/protocol_support/nfc_protocol_support.c` — eine neue
  Tabellenzeile `[NfcProtocolTexkom] = "texkom"` in `nfc_protocol_support_plugin_names[]`,
  der Dateiname-Baustein für den Laufzeit-Plugin-Lader. Ebenfalls nicht in der
  ursprünglichen Port-Skizze benannt — siehe "Update (Phase 5)" oben. Die übrige, seit
  meinem Firmware-Stand erheblich weiterentwickelte Datei (Plugin-Lade-Fehlerbildschirm,
  `extra_scenes`-Dispatch, Wiedereintritts-Schutz u. a.) ist bewusst **nicht** Teil dieses
  Patches — siehe "Risiko".

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
Phase 4b.

### Phase 4a — Register-/Abhängigkeitsprüfung und Anschlussstücke (diese Sitzung)

Für diese Sitzung war laut Port-Skizze eine auf `furi_hal_nfc_texkom.c` fokussierte
Register-/Timing-Prüfung gegen meine eigene `furi_hal_nfc_iso14443a.c`/
`furi_hal_nfc_iso15693.c` vorgesehen, bevor ein Patch für Phase 4 überhaupt sinnvoll
vorschlagbar wäre. Durchgeführt — mit einem Ergebnis, das die Port-Skizze selbst
verändert: die Datei hängt an drei Stellen von Code ab, der bei mir noch nicht existiert
und auch nicht Teil der ursprünglich als "Phase 4" benannten einen Datei ist. Ohne diese
drei Stellen würde `furi_hal_nfc_texkom.c` bei mir schlicht nicht bauen, unabhängig davon,
wie sorgfältig die Register-Werte selbst geprüft sind.

**Treiberbasis unverändert:** `lib/drivers/st25r3916.c/.h/_reg.c/.h` — alle vier Dateien
gegen den unten genannten Unleashed-`dev`-Stand per `diff -q` Byte-für-Byte identisch
bestätigt. Jedes einzelne von `furi_hal_nfc_texkom.c` verwendete Register-/Bit-Symbol
(`ST25R3916_REG_MODE_*`, `_STREAM_MODE_*`, `_RX_CONF1-4_*`, `_CORR_CONF1-2_*`,
`_AUX_MOD_*`, `_OP_CONTROL_rx_en`, `_CMD_TRANSPARENT_MODE`, `_CMD_UNMASK_RECEIVE_DATA`) ist
bei mir bereits vorhanden — keine einzige fehlende Registerdefinition, kein neues Register.

**Registerwerte gegen vorhandene Pfade gegengerechnet** (eigene
`furi_hal_nfc_iso15693.c`/`furi_hal_nfc_iso14443a.c`, Poller- *und* Listener-Konfiguration):

| Register | Texkom (Poller-RX) | ISO15693-Poller | ISO14443A-Poller | Befund |
|---|---|---|---|---|
| `MODE` (om/tr_am) | `om_subcarrier_stream\|tr_am_ook` | `om_subcarrier_stream\|tr_am_ook` | `om_iso14443a\|tr_am_ook` | **Exakt** wie ISO15693 — gleicher Betriebsmodus. |
| `STREAM_MODE` | `scf_sc212\|stx_106\|scp_1pulse` | `scf_sc424\|stx_106\|scp_8pulses` | — | `stx_106` gleich; `scf`/`scp` bewusst anders (212 statt 424 kHz, andere Kodierung) — deckt sich mit dem eigenen Dateikommentar. |
| `RX_CONF1` | `z12k\|h80\|lp_600khz` | `z12k\|h80\|lp_600khz` | `z600k` | **Exakt** wie ISO15693. |
| `RX_CONF2` | `agc6_3\|agc_m\|agc_en\|sqm_dyn` | identisch | identisch | **Exakt** wie beide. |
| `RX_CONF3` | `rg1_am1\|rg1_am0` (3/7 Dämpfung) | `0x00` (offen) | `0x00` (offen) | **Einzige echte Abweichung** — selbst im Dateikommentar als beim Hardware-Bringup empirisch ergänzt benannt (Digitizer lag ohne Dämpfung an einer Schiene fest). Nicht durch Quelltextvergleich verifizierbar, siehe "Risiko". |
| `RX_CONF4` | `0x00` | `0x00` | `0x00` | **Exakt**. |
| `CORR_CONF1` | `corr_s0\|corr_s1\|corr_s4` | identisch | `corr_s0\|corr_s4\|corr_s6` | **Exakt** wie ISO15693 (ISO14443A nutzt ein anderes Korrelatorprofil, erwartungsgemäß). |
| `CORR_CONF2` | `0x00` (`corr_s8` gelöscht) | `corr_s8` gesetzt | `0x00` | Spiegelbildlich zu ISO15693: `corr_s8` erweist sich so als der 212-/424-kHz-Auswahlbit, passend zu `scf_sc212` oben — in sich konsistent. |
| `AUX_MOD` | `dis_reg_am\|res_am` | identisch | — | **Exakt** wie ISO15693. |

Für die Listener-Seite (`furi_hal_nfc_texkom_listener_init`): `MODE` auf
`targ_targ\|om_targ_nfca\|tr_am_ook` gesetzt — **exakt** dieselben Bits wie
`furi_hal_nfc_iso15693_listener_init()`, passend zum eigenen Kommentar ("Passive target,
OOK... nur den Zielzustand setzen").

**Ergebnis:** von acht geprüften Registern ist eines (`RX_CONF3`) eine neue, empirisch
begründete Abweichung, alle anderen sind wortwörtliche Wiederverwendung der bereits in
meiner eigenen Firmware gebauten und (implizit über den produktiven ISO15693-Betrieb)
getesteten Konfiguration. Das deckt sich mit der ursprünglichen Risikoeinschätzung und
verschiebt sie nicht — `RX_CONF3` bleibt der eine Punkt, der nur an echter Hardware mit
Oszilloskop/Spektrumanalysator zu prüfen ist (siehe "Risiko").

**Drei vorgelagerte Abhängigkeitslücken gefunden** (keine davon in der Register-Tabelle
oben, da sie nicht in `furi_hal_nfc_texkom.c` selbst, sondern in von ihr vorausgesetztem
Code liegen):

1. `furi_hal_nfc_transparent_mode_enter/_exit` — Texkom ruft diese generisch für Poller
   *und* Listener auf. Bei mir existiert dieselbe Logik nur Listener-lokal unter anderem
   Namen (`furi_hal_nfc_iso15693_listener_transparent_mode_enter/_exit`, `static` in
   `furi_hal_nfc_iso15693.c`). Per `diff` bestätigt: die beiden Funktionskörper sind
   Zeichen-für-Zeichen identisch zu den neuen generischen Funktionen im Unleashed-`dev`-
   Stand — eine reine Hochzieh-Refaktorierung, keine neue Logik.
2. `NfcTechTexkom`/`FuriHalNfcTechTexkom`/`nfc_texkom_poller_rx()` — bereits in Phase 2/3
   als "kommt erst mit Phase 4" benannt, jetzt per `diff` gegen den exakten Unleashed-
   `dev`-Quelltext identifiziert und übernommen.
3. `signal_reader_start()`/`_stop()` behandeln `SignalReaderTriggerNone` nicht gesondert
   — obwohl der Enum-Wert bei mir bereits existiert, schalten beide Funktionen
   unverändert immer den EXTI-/DMA-Resync-Pfad scharf. Geprüft, ob das bei mir schon
   irgendwo auffällt: `signal_reader_alloc()` hat bei mir aktuell **keinen einzigen
   Aufrufer** außerhalb von `lib/signal_reader/` selbst (per `grep` über den gesamten
   eigenen Baum bestätigt, ebenso im Unleashed-`dev`-Stand außer `furi_hal_nfc_texkom.c`
   selbst) — die fehlende Abzweigung hat bislang niemanden gestört, weil niemand den
   freilaufenden Pfad bei mir je benutzt hat. Texkom wäre der erste tatsächliche
   Aufrufer. Ändern dieser Datei trägt deshalb kein Regressionsrisiko für einen
   bestehenden Aufrufer.

**Eine vierte, bewusst nicht übernommene Änderung:** derselbe Unleashed-`dev`-Diff bringt
in `furi_hal_nfc_iso15693.c`/`furi_hal_nfc.h` zusätzlich eine komplett unabhängige
ISO15693-Funktionsgruppe mit (`furi_hal_nfc_iso15693_detect_mode/_force_1outof4/
_force_1outof256`, Parser-Modusauswahl zwischen den beiden ISO15693-Kodierungen) sowie
eine Konstanten-Umbenennung (`FURI_HAL_NFC_ISO15693_BIT_LEN`). Beides hat nichts mit
Texkom zu tun — reine parallele Weiterentwicklung desselben Dateibestands seit meinem
letzten Firmware-Abgleich. Nicht mitgenommen, um diesen Patch auf Texkom beschränkt zu
halten.

**Verifikation der drei Patches:** je zwei bzw. drei frische, unabhängige Zweitkopien
meiner eigenen Firmware-Basis geklont (`G4MEOVER18/G4MEOVER-FW`, `main`, unverändert
`b359f6f`). Alle neun betroffenen Dateien nach der Bearbeitung per `diff` gegen den
Unleashed-`dev`-Stand verglichen — für jede der neun Dateien **exakt keine Abweichung**
außer der oben benannten, bewusst ausgeschlossenen vierten Änderung (nur in
`furi_hal_nfc_iso15693.c`/`furi_hal_nfc.h` relevant). Die neun Einzelpatches
(`firmware-mining/texkom-nfc-protocol-core-port/phase4a-connective-patches/`) sowohl
einzeln als auch als ein zusammengefasster Diff mit `git apply --check` gegen beide
Zweitkopien gegengeprüft, real angewendet, und alle neun Zieldateien per `md5sum` über
vier unabhängig angewendete Kopien (eine davon Datei-für-Datei statt als ein Block
angewendet) verglichen — durchgängig identisch. Diese Dateien enthalten keine neue
Bit-/Byte-Mathematik (reine Enum-Werte, Dispatch-Tabellenzeilen, eine Dispatch-Funktion,
eine Hochzieh-Refaktorierung, eine Pfadverzweigung) — anders als Phase 1-3 ist ein
Host-Repro mit synthetischen Testvektoren hier nicht der richtige Prüfmaßstab; der exakte
Quelltextabgleich gegen einen unabhängig geprüften, vorhandenen Referenzstand ist es.

### Phase 4b — Low-Level-RF-Datei: Abtast-/Zeitmess-Zustandsmaschine (diese Sitzung)

`furi_hal_nfc_texkom.c` gegen die eigene Basis geprüft: `md5sum` über zwei unabhängig
geladene Kopien bestätigt `38c7f707f2d8c9156cdd726068cc9644` — identisch zu der in
Phase 4a bereits einmal geprüften Prüfsumme, also seit der letzten Sichtung unverändert.
527 Zeilen vollständig gelesen, in drei Gruppen zerlegt:

1. **Reine Registerkonfiguration** (`furi_hal_nfc_texkom_configure_rx()`, die
   `MODE`/`OP_CONTROL`-Bits in `_listener_init()`) — bereits vollständig in Phase 4a
   Register-für-Register gegen die eigene ISO15693-/ISO14443A-Basis gegengerechnet
   (siehe dortige Tabelle); keine neue Prüfung hier, nur die erneute
   Prüfsummenbestätigung, dass sich seither nichts geändert hat.
2. **Reine Hardwareansteuerung ohne eigene Bit-/Byte-Logik**
   (`furi_hal_nfc_texkom_poller_init/_deinit/_rx`, `_listener_init/_deinit/_sleep`,
   `_wait_flags()`): Aufrufe von `signal_reader_*()`, `furi_hal_nfc_transparent_mode_*()`,
   `furi_thread_flags_*()` — delegiert vollständig an bereits vorhandene bzw. in Phase 4a
   verallgemeinerte Funktionen, ohne eigene Entscheidungslogik außer Fehlercode-Mapping
   (`NfcErrorTimeout`/`FuriHalNfcErrorBufferOverflow`/`FuriHalNfcErrorNone`) und dem
   `memcpy(rx_data, capture.gaps, capture.frame_count)` am Ende von `_poller_rx()` — Letzteres
   gegen `rx_data_size` geprüft, bevor kopiert wird (`if(capture.frame_count > rx_data_size)
   return FuriHalNfcErrorBufferOverflow;`), kein eigener Fund. Nicht host-testbar (echte
   `FuriHalSpiBusHandle`/`SignalReader`-Objekte nötig), aber auch keine eigene Logik, die
   das rechtfertigen würde.
3. **Eigene, hardwareunabhängige Bit-/Byte-Logik** — die drei Funktionen, die den von
   `SignalReader` gelieferten, bei 500 ksps abgetasteten Bitstrom in die Lückenliste
   überführen (`furi_hal_nfc_texkom_feed()`, `_handle_impulse()`, `_cut_frame()`), plus zwei
   kleinere reine Dispatch-Funktionen (`furi_hal_nfc_texkom_coding()`,
   `_listener_tx()`s Rahmengrößen-/Codierungs-Prüfung). Das ist die eine Stelle in dieser
   Datei, an der sich ein Portierungsfehler als falsch decodierte Lücken statt als
   offensichtlicher Hardware-Fehlschlag zeigen würde — und die einzige, die sich ohne
   Hardware tatsächlich prüfen lässt. Bislang (Phase 4a) nicht geprüft, weil die
   Register-fokussierte Sichtung dieser Sitzung galt; diese Sitzung hat sie nachgezogen.

**Fund:** keiner. Beim Lesen auf Speicherfehler/Off-by-one geprüft (gleiches Kriterium wie
bei Phase 1-3): `gaps[]` (127 Byte) wird ausschließlich über `gap_count < MAX_GAPS` vor dem
Schreiben geschützt; `since_impulse` ist ein `uint32_t`, das vor jedem Speichern in `gaps[]`
nachweislich nie über `FURI_HAL_NFC_TEXKOM_FRAME_GAP` (100, laut `_Static_assert` klein genug
für ein Byte) steigen kann, weil die Inkrement-Verzweigung selbst genau das verhindert; die
Fast-Path-Abkürzung für durchgängig-Low-Bytes (`feed()`s erste `if`) habe ich von Hand gegen
den langsamen Bit-für-Bit-Pfad durchgerechnet und für äquivalent befunden — jetzt zusätzlich
durch Szenario 3 unten bestätigt statt nur hergeleitet.

Host-Repro in
`firmware-mining/repro-texkom-nfc-protocol-core-port/repro_furi_hal_texkom.c` (die drei
Zustandsmaschinen-Funktionen sowie `_coding()` unverändert transkribiert;
`_listener_tx()` mit gemocktem `texkom_signal_tx()` transkribiert, da dessen eigene Logik
bereits in Phase 3 verifiziert ist; `FuriHalSpiBusHandle`/`SignalReader`/
`furi_thread_flags_*` bleiben vollständig außen vor — diese Funktionen rufen keine davon
auf), unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` bei `-O0`/`-O1`: **445 Prüfungen, alle bestanden**,
identischer Output beider Optimierungsstufen, keine Compiler-Warnungen, keine
Sanitizer-Befunde.

1. **TK13-förmiger Rahmen** (128 Impulse, zwei alternierende Periodenlängen, synthetisch
   als Low-/High-Bitfolge aufgebaut statt über `texkom_signal.c` codiert — bewusst
   entkoppelt von Phase 1/3s eigener Bitzuordnung, um ausschließlich diese Datei zu
   prüfen): die 127 rekonstruierten Lücken stimmen bit-exakt mit den vorgegebenen
   Periodenlängen überein.
2. **TK17-förmiger Rahmen** (65 Impulse, vier Periodenlängen): 64 Lücken bit-exakt.
3. **Fast-Path-/Slow-Path-Äquivalenz**: derselbe Bitstrom einmal als ein Block (reizt die
   `byte==0`-Abkürzung auf den langen Low-Läufen) und einmal Byte für Byte einzeln
   eingespeist (reizt denselben Abkürzungspfad, aber auf Einzelbytes, inklusive des
   frühen `continue` vor dem ersten Impuls) — beide Varianten liefern exakt denselben
   Endzustand.
4. **Overflow**: ein Zug, der über 127 Lücken hinaus weiterimpulst, ohne zu pausieren,
   wird beim endgültigen Pausen-Eintreffen verworfen (`frame_count` bleibt 0,
   `gap_count`/`overflow` werden von `cut_frame()` zurückgesetzt), `longest_train` meldet
   den Deckel (127), nicht die unbegrenzte Impulszahl.
5. **Müllzug gefolgt von gültigem Zug im selben `feed()`-Aufruf**: ein verworfener,
   10-Impulse-Zug direkt gefolgt von einem gültigen TK17-Zug im selben Puffer — der
   gültige Zug wird trotzdem korrekt erfasst, der Reset durch `cut_frame()` lässt keinen
   Zustand in den nächsten Zug durchsickern.
6. **Refraktärzeit absorbiert Nachschwingen**: ein einzelner Abtastwert mitten in einem
   Low-Lauf auf High gekippt (ein simuliertes Klingeln knapp nach einer Flanke, deutlich
   innerhalb der 6-Abtastwerte-Refraktärzeit) ändert die rekonstruierte Lückenliste nicht
   — zusätzlich gegen eine Mutante geprüft, die `FURI_HAL_NFC_TEXKOM_REFRACTORY` von 12 µs
   auf 6 µs halbiert: dieses Szenario (nicht nur die am Anfang von `main()` stehende
   Konstantenprüfung) schlägt dann tatsächlich fehl, ist also empfindlich für die reale
   Refraktärzeit und keine folgenlose Tautologie.
7. **Früher Rückgabepfad nach gültigem Schnitt**: nach einem gültigen Frame werden
   innerhalb desselben `feed()`-Aufrufs angehängte, selbst wieder gültig aussehende
   weitere Bytes nachweislich nie angefasst (andere Lückenwerte als der erste Zug bleiben
   unverändert in `gaps[]`); ein zweiter `feed()`-Aufruf auf demselben, bereits
   geschnittenen `capture` ändert ebenfalls nichts — spiegelt exakt die eigene
   `if(capture->frame_count) return;`-Absicherung im Reader-Callback.
8. **Aufteilung über mehrere `feed()`-Aufrufe**, wie es die echten 32-Byte-Blöcke des
   `SignalReader` tatsächlich tun: derselbe Zug einmal als ein Aufruf, einmal über
   mehrere 32-Byte-Häppchen verteilt — identisches Endergebnis.
9. **`furi_hal_nfc_texkom_coding()`**: TK17-Typbyte wählt TK17-Codierung, jedes andere
   Typbyte (TK13, TK15, MMBIT, Zufallswerte) wählt TK13-Codierung.
10. **`furi_hal_nfc_texkom_listener_tx()`-Dispatch** (mit gemocktem `texkom_signal_tx()`):
    korrekte Rahmengrößenprüfung (exakt 64 Bit akzeptiert, 63/65/0 Bit abgelehnt, ohne
    `texkom_signal_tx()` je zu erreichen) und korrekte Codierungswahl, die bei korrekter
    Länge durchgereichten Rahmen-Bytes unverändert.

**Mutationsproben zur Absicherung des Repros selbst** (keine Teil des gelieferten Repros,
nur zur Prüfung dieser Sitzung durchgeführt und verworfen): eine Mutante, die die
Fast-Path-Akkumulation von `+= BITS_IN_BYTE` auf `+= BITS_IN_BYTE - 1` ändert (ein
plausibler Transkriptionsfehler), lässt 402 der 445 Prüfungen scheitern; eine Mutante, die
`cut_frame()`s TK13-Lückenzahl-Vergleich um eins verschiebt, lässt gezielt die
TK13-bezogenen Szenarien scheitern. Eine dritte Mutante (Refraktärzeit-Vergleich von `<`
auf `<=` geändert, statt den Konstantenwert selbst) schlägt bei keiner der zehn Prüfungen
an — bewusst untersucht statt verschwiegen: die Grenze liegt exakt bei
`since_impulse == REFRACTORY`, einem Abstand, den kein reales Signal je erreicht (die
Refraktärzeit ist mit 12 µs bewusst unter dem kürzesten realen Lückenabstand, TK17s rund
28 µs, mehr als doppelt so klein gewählt — siehe Datei-Kommentar); ein echter
Portierungsfehler an dieser Stelle würde sich als grob falscher Lückenwert zeigen, den
Szenario 1/2/6 bereits auffangen, nicht als exakt-Grenzfall. Kein zusätzlicher Test dafür
ergänzt, um keine für das reale Signal irrelevante Grenzfallprüfung als Sicherheit
vorzutäuschen, die sie nicht ist.

**Nicht** durch diesen Host-Test abgedeckt: die tatsächliche Registerkonfiguration selbst
(Gruppe 1 oben, bereits durch den Phase-4a-Quelltextabgleich abgedeckt, nicht durch ein
Host-Repro mit synthetischen Vektoren), jede tatsächliche SPI-/GPIO-Ansteuerung (Gruppe 2),
und `RX_CONF3` (siehe "Risiko") — dafür bleibt echte Hardware mit
Spektrumanalysator/Oszilloskop weiterhin der einzige Prüfmaßstab.

### Phase 5 — Protokollbaum-Registrierung und UI (diese Sitzung, letzte Phase)

Zwei unterschiedliche Prüfmaßstäbe, exakt wie bei Phase 4a bereits etabliert:

**Quelltextabgleich** (die sechs reinen Registrierungsdateien, keine eigene Bit-/Byte-Logik):
`lib/nfc/protocols/nfc_protocol.c/.h`, `nfc_device_defs.c/nfc_poller_defs.c/
nfc_listener_defs.c`, `lib/nfc/SConscript`, `applications/main/nfc/application.fam`,
`applications/main/nfc/helpers/protocol_support/nfc_protocol_support.c`. Für jede der sechs
Dateien per `git diff` ein minimaler, auf exakt die Texkom-Zeilen beschränkter Patch
abgeleitet (`firmware-mining/texkom-nfc-protocol-core-port/phase5-registration/*.patch`).

Verifikation der Patches: drei unabhängig voneinander erzeugte Ergebnisse verglichen —
(1) die Patches von Hand gegen eine frische Kopie meiner eigenen Firmware-Basis
(`G4MEOVER18/G4MEOVER-FW`, `main`, unverändert `b359f6f`) abgeleitet, (2) dieselben sechs
Patches per `git apply` in einem Lauf gegen eine zweite, unabhängig geklonte Kopie
angewendet, (3) dieselben sechs Patches einzeln per `patch -p1` (unabhängiges Werkzeug von
`git apply`) gegen eine dritte, unabhängig geklonte Kopie angewendet. Alle sechs betroffenen
Dateien per `md5sum` über die drei Ergebnisse verglichen: **durchgängig byte-identisch**.
Zusätzlich die drei nicht nur additiven, sondern auch inhaltlich eigenständigen Dateien
(`nfc_protocol.c`, `nfc_device_defs.c`, `nfc_poller_defs.c`, `nfc_listener_defs.c`,
`SConscript`, `nfc_protocol_support.c`) nach dem Anwenden per `diff` gegen den aktuellen
Unleashed-`dev`-Stand verglichen: für `nfc_protocol.c/.h`, `nfc_device_defs.c`,
`nfc_poller_defs.c` und `SConscript` **exakt keine Abweichung** — vollständig identisch zum
Upstream-Stand. Für `nfc_listener_defs.c` genau eine verbleibende Abweichung, die bewusst
ausgeschlossene, von Texkom unabhängige `NfcProtocolMfPlus`-Listener-Zeile (siehe
"Betroffene Dateien" und "Risiko"). Für `nfc_protocol_support.c` eine erhebliche
verbleibende Abweichung — die seit meinem Firmware-Stand unabhängig gewachsene restliche
Datei (siehe "Betroffene Dateien") —, innerhalb der aber die eine neue, hier gelieferte
Tabellenzeile `[NfcProtocolTexkom] = "texkom"` Zeichen-für-Zeichen mit dem Upstream-Stand
übereinstimmt (per `grep`/Sichtprüfung der verbleibenden Diff-Ausgabe bestätigt). Für
`application.fam` ebenfalls eine erhebliche verbleibende Abweichung (siehe dieselbe
Begründung), mit demselben Befund: der neue `App(appid="nfc_texkom", ...)`-Block selbst ist
Zeichen-für-Zeichen identisch zum Upstream-Block, nur an einer anderen Stelle relativ zu den
unabhängig hinzugekommenen Parser-Plugin-Einträgen platziert.

Zusätzlich: `python3 -c "import ast; ast.parse(open('application.fam').read())"` bestätigt
gültige Python-Syntax der geänderten Datei (SCons-`.fam`-Dateien sind Python); für alle
sechs geänderten C-Dateien eine Klammer-/Parenthesen-Bilanzprüfung (`{`/`}` sowie `(`/`)`
jeweils gleich oft) als zusätzliche, einfache Sicherheitsnetz-Prüfung gegen eine
unvollständig kopierte Zeile.

**Host-Repro für die eine Stelle mit eigener Logik** (`applications/main/nfc/helpers/
protocol_support/texkom/texkom_render.c::nfc_render_texkom_info()` — reine
String-Formatierung, keine `furi_hal`/View/Widget-Abhängigkeit, anders als `texkom.c`
selbst, das nur Szenen-Boilerplate nach dem `st25tb`-Muster ist und keine eigene Logik
enthält). Host-Repro in `firmware-mining/repro-texkom-nfc-protocol-core-port/
repro_render.c`: die Funktion unverändert transkribiert, `FuriString` durch einen minimalen,
wachsenden Puffer-Ersatz ersetzt (implementiert exakt die vier tatsächlich genutzten Aufrufe
`alloc`/`cat_printf`/`get_cstr`/`free`), die Phase-1-Kernfunktionen
(`texkom_get_uid`/`texkom_is_crc_valid`/`texkom_get_expected_crc`/`texkom_update_crc`/
`texkom_make_blank`/`texkom_set_uid`, CRC-Varianten, Features-Tabelle) aus dem bereits in
`repro.c` verifizierten Bestand in diese eigenständige Übersetzungseinheit erneut mit
hineinkopiert, nicht neu geprüft — exakt die seit `repro_poller.c` etablierte Technik.

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` bei `-O0`/`-O1`: **6 Szenarien, alle bestanden**,
identischer Output beider Optimierungsstufen, keine Compiler-Warnungen, keine
Sanitizer-Befunde. Jedes Szenario vergleicht die tatsächliche Ausgabe Byte-für-Byte gegen
eine unabhängig mit `snprintf()` aufgebaute Erwartung (kein Teilstring-/Enthält-Vergleich):
TK13/TK17 mit gültiger CRC im vollen und im kurzen Format (prüft, dass `format_type`
tatsächlich nur den "Frame:"-Block schaltet und sonst nichts), TK13 mit absichtlich
verfälschtem CRC-Byte (prüft "Invalid" plus den exakten, unabhängig berechneten
"Expected CRC"-Wert), MMBIT (prüft, dass die UID-Zeile exakt die von `texkom_get_uid()`
selbst gemeldete Länge nutzt, nicht eine feste Konstante), sowie `TexkomTypeUnknown` (prüft
den einen Fall, in dem `crc_valid` bereits falsch ist, aber auch `texkom_get_expected_crc()`
selbst `false` liefert — und die "Expected CRC"-Zeile deshalb trotz ungültiger CRC
unterdrückt bleiben muss).

Gegenprobe zur Absicherung des Repros selbst: eine Mutante, die die reale
`if(!crc_valid && texkom_get_expected_crc(...))`-Bedingung auf `if(texkom_get_expected_crc(
...))` verkürzt (ein plausibler Transkriptionsfehler, der die "Expected CRC"-Zeile auch bei
gültiger CRC anzeigen würde), lässt das erste Szenario sofort mit einer sichtbar
abweichenden Ausgabe scheitern — das Repro ist also empfindlich für genau die Fehlerklasse,
die ein Transkriptionsfehler in dieser einen Datei wäre.

**Nicht** durch diesen Host-Test abgedeckt: `texkom.c` selbst (reine Szenen-Verdrahtung ohne
eigene Logik, siehe oben), das tatsächliche Laden der `.fal`-Datei durch den
`PluginManager`, und jede Interaktion mit echten `NfcApp`/`Widget`/`ViewDispatcher`-Objekten
— dafür bleibt ein lokaler Build-/Flash-Test weiterhin der einzige Prüfmaßstab (siehe
"Port-Skizze").

## Risiko

- **Niedrig** für den gelieferten, host-verifizierten Phase-1-Protokollkern: rein additive
  neue Dateien, keine Änderung an einer bestehenden Datei, kein geteilter Zustand, keine
  Registrierung in der Protokoll-Registry (die kommt erst mit Phase 2+). Reine
  Bit-/Byte-Mathematik ohne Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge.
- **Niedrig** für die gelieferte, host-verifizierte Phase-2-Poller-Kontrolllogik selbst:
  rein additive neue Dateien, keine Änderung an einer bestehenden Datei, keine eigene
  Bit-/Byte-Mathematik (reiner Dispatch auf den bereits verifizierten Phase-1-Kern). Die
  damals offene Integrationslücke (`NfcTechTexkom`, `nfc_texkom_poller_rx()`) ist mit
  Phase 4a jetzt geschlossen (siehe unten) — diese Datei compiliert bei mir damit, sobald
  nur noch Phase 4b fehlt.
- **Niedrig** für den gelieferten, host-verifizierten Phase-3-Listener und -Signalpreset
  selbst: rein additive neue Dateien, keine Änderung an einer bestehenden Datei, kein
  geteilter Zustand, der Signalpreset ist reine, differentiell gegen Phase 1 geprüfte
  Bit-/Byte-Mathematik ohne Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge,
  der Listener reiner Dispatch ohne eigene Logik. Die `NfcTechTexkom`-Lücke ist wie bei
  Phase 2 mit Phase 4a geschlossen; der damals offene Klärungspunkt — *wie* die rohen
  Byte, die der Listener an `nfc_listener_tx()` übergibt, auf dem Weg dorthin überhaupt
  an die für die Sendung nötige `TexkomCoding`/`TexkomSignalCoding` kommen — ist mit
  Phase 4b jetzt beantwortet: `furi_hal_nfc_texkom_listener_tx()` leitet die Codierung
  selbst aus dem Rahmen-Typbyte ab (`furi_hal_nfc_texkom_coding()`), bevor sie an
  `texkom_signal_tx()` übergeben wird — kein Fehler in den Phase-3-Dateien, nur eine
  Zuständigkeit, die strukturell erst in Phase 4b lag.
- **Niedrig** für die in dieser Sitzung gelieferten Phase-4a-Anschlussstücke (siehe
  "Verifikation"): zwei der drei Änderungsgruppen sind rein additiv (`lib/nfc/nfc.h/.c`,
  `furi_hal_nfc.h`, `furi_hal_nfc_tech_i.h`); die beiden Änderungen an bestehenden Dateien
  sind beide nachweislich verhaltenserhaltend für ihren jeweils einzigen bisherigen
  Verwender — die `transparent_mode`-Hochzieh-Refaktorierung in `furi_hal_nfc_iso15693.c`
  bewegt einen Byte-für-Byte identischen Funktionskörper nur an eine andere Stelle, und
  `signal_reader.c`s neue Pfadverzweigung betrifft einen Modus
  (`SignalReaderTriggerNone`), den bei mir aktuell kein einziger Aufrufer nutzt (siehe
  "Verifikation") — es gibt also niemanden, dessen Verhalten sich ändern könnte.
- **Niedrig** für die jetzt gelieferte, host-verifizierte Abtast-/Zeitmess-
  Zustandsmaschine und die beiden kleinen Dispatch-Funktionen aus Phase 4b (siehe
  "Verifikation"): unverändert übernommene, reine Bit-/Byte-Logik ohne Registerzugriff,
  keine Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge (`gaps[]` ist
  durchgängig durch `gap_count < MAX_GAPS` geschützt), kein eigener Fund beim Lesen.
- **Mittel** für die verbleibende, nicht host-prüfbare Hardwareansteuerung in derselben
  Datei (Phase 4b, `furi_hal_nfc_texkom.c`s Registerkonfiguration und
  SPI-/`SignalReader`-Aufrufe) — einziger Teil, der den STM32WB55-NFC-Analogfrontend
  direkt im "Transparent Mode" ansteuert (digitaler Framing-Pfad umgangen) — ein Fehler
  hier wirkt sich potenziell auf die HF-Erzeugung selbst aus, nicht nur auf die
  Dekodierung. Von acht geprüften Registern (Phase 4a) ist `RX_CONF3` (AM-Kanal-
  Dämpfung auf der Poller-RX-Seite, 3 von 7 Stufen) die einzige nicht aus einer bereits
  bei mir produktiv laufenden Konfiguration wiederverwendete, sondern eine neue, laut
  eigenem Quelltextkommentar beim Hardware-Bringup empirisch ergänzte Abweichung — nur
  dieser eine Wert ist nicht durch Quelltextvergleich abschließend bewertbar, sondern nur
  an echter Hardware mit einem Spektrumanalysator/Oszilloskop zu prüfen. Alle anderen
  Registerwerte sind bit-exakt gegen meine eigene, bereits gebaute und produktiv genutzte
  ISO15693-/ISO14443A-Konfiguration bestätigt (siehe Tabelle in "Verifikation" zu Phase
  4a). Richtigstellung zur vorherigen Fassung dieses Punkts: "Transparent Mode bereits für
  ISO14443A/ISO15693 genutzt" war so nicht korrekt — bei mir nutzt ihn bislang nur der
  ISO15693-*Listener*; ISO14443A verwendet an keiner Stelle `transparent_mode`
  (per `grep` über den gesamten eigenen Baum bestätigt, siehe "Verifikation" zu Phase 4a).
  Die ursprünglich als "Mittel-hoch" eingeschätzte Gesamtdatei ist damit jetzt in zwei
  klar getrennte Risiken aufgeteilt: die hier gelieferte Logik (niedrig, jetzt
  host-verifiziert) und die reine Hardwareansteuerung samt `RX_CONF3` (mittel, nur an
  Hardware zu klären) — kein pauschales Risiko mehr über die ganze Datei.
- **Niedrig** für die in dieser Sitzung gelieferte Phase-5-Registrierung/UI (siehe
  "Verifikation"): sechs der sieben Dateien sind rein additive Tabellenzeilen/
  `#include`s/ein neuer `App()`-Block in bereits bestehenden, generischen
  Registrierungsdateien — keine Änderung an einer bestehenden Protokollzeile. Die eine
  Datei mit eigener Logik (`texkom_render.c`) ist reine String-Formatierung ohne
  Zeiger-Arithmetik über eine selbst nicht kontrollierte Länge, jetzt host-verifiziert.
  `texkom.c` selbst enthält keine eigene Logik (reine Szenen-Verdrahtung, byte-für-byte
  nach demselben Muster wie `st25tb`, siehe "Betroffene Dateien"). Bewusst nicht
  mitgenommen, um den Patch auf Texkom beschränkt zu halten: die unabhängige
  `NfcProtocolMfPlus`-Listener-Änderung in `nfc_listener_defs.c` sowie der komplette, seit
  meinem Firmware-Stand unabhängig gewachsene Rest von `nfc_protocol_support.c` und
  `application.fam` (siehe "Betroffene Dateien" und "Update (Phase 5)") — dieselbe
  Abgrenzungstechnik wie bei der in Phase 4a ausgeschlossenen ISO15693-Änderung.
- **Ohne Phase 5 wäre die gesamte Portierung praktisch nutzlos gewesen**: ohne die
  Protokollbaum-Registrierung hätte mein Lesegerät einen erkannten Texkom-Tag nicht einmal
  dem richtigen Protokoll zugeordnet; ohne den `application.fam`-Plugin-Eintrag und die
  `nfc_protocol_support_plugin_names[]`-Zeile hätte selbst eine erfolgreiche Zuordnung
  beim ersten Laden der (nie gebauten) `nfc_texkom.fal` abgestürzt (siehe "Update
  (Phase 5)"). Phase 1-4b allein (host-verifizierte Protokoll-/Poller-/Listener-/
  Signal-/Register-Logik) wären ohne diese Sitzung totes, nie erreichbares Code bei mir
  geblieben.
- **Kein zusätzliches Missbrauchsrisiko**: Texkom kennt kein Authentifizierungsgeheimnis;
  Lesen und Emulieren eines bekannten/eigenen Schlüssels ist strukturell identisch zum
  bereits unterstützten 125-kHz-Zutrittsformat-Modell (EM-Marin/Cyfral/Hitag), nur auf
  13,56 MHz statt 125 kHz.

## Port-Skizze (mehrteilig, für künftige Sitzungen gestuft)

1. **Phase 1 (geliefert):** `texkom.c/.h/_i.h` unverändert nach
   `lib/nfc/protocols/texkom/` übernehmen, als neue Quelldateien in die
   `SConscript`/Build-Liste von `lib/nfc` aufnehmen (reine Quelldatei-Ergänzung, kein
   neues Ziel). Baut eigenständig, wird von nichts referenziert, bis Phase 2 beginnt.
2. **Phase 2 (geliefert):** `texkom_poller.c/.h/_defs.h/_i.h` unverändert
   nach `lib/nfc/protocols/texkom/` übernehmen, ebenfalls in die Build-Liste aufnehmen.
   Compiliert im eigenen Baum jetzt, sobald Phase 4a angewendet ist (liefert
   `NfcTechTexkom`-Enum-Wert und `nfc_texkom_poller_rx()`), ist aber bereits seit der
   eigenen Sitzung — unabhängig davon — host-seitig gegen eine feste, gemockte
   Lückenliste durchgetestet (siehe "Verifikation"), exakt die hier zuvor vorgesehene
   Technik.
3. **Phase 3 (geliefert):** `texkom_listener.c/.h/_defs.h/_i.h` plus
   `texkom_signal.c/.h` unverändert nach `lib/nfc/protocols/texkom/` bzw.
   `lib/digital_signal/presets/nfc/` übernehmen, in die jeweilige Build-Liste aufnehmen.
   Der Listener compiliert im eigenen Baum jetzt ebenfalls mit Phase 4a; der
   Signalpreset ist davon unabhängig und bräuchte nur ein `GpioPin`. Beide bereits seit
   der eigenen Sitzung — unabhängig davon — host-seitig verifiziert (siehe
   "Verifikation"), der Signalpreset per differentiellem Encode/Decode gegen Phase 1,
   exakt die seit Phase 1 vorgesehene Technik.
4. **Phase 4a (geliefert, diese Sitzung):** die neun Einzelpatches aus
   `firmware-mining/texkom-nfc-protocol-core-port/phase4a-connective-patches/` anwenden
   (`lib/nfc/nfc.h/.c`, `lib/signal_reader/signal_reader.h/.c`,
   `targets/furi_hal_include/furi_hal_nfc.h`, `targets/f7/furi_hal/furi_hal_nfc.c`,
   `furi_hal_nfc_i.h`, `furi_hal_nfc_tech_i.h`, `furi_hal_nfc_iso15693.c`). Macht Phase
   2/3 baubar, ohne selbst RF-Register anzufassen — siehe "Betroffene Dateien" und
   "Verifikation" für den vollen Umfang und die Abgrenzung von der unabhängigen
   ISO15693-1oo4/1oo256-Änderung im selben Upstream-Diff, die bewusst nicht mit
   übernommen ist.
5. **Phase 4b (geliefert, diese Sitzung):** `furi_hal_nfc_texkom.c` unverändert nach
   `targets/f7/furi_hal/` übernehmen, in die Build-Liste aufnehmen. Macht Phase 2/3
   zusammen mit Phase 4a vollständig baubar. Die eigene Abtast-/Zeitmess-
   Zustandsmaschine ist jetzt host-verifiziert (siehe "Verifikation"); offen bleibt nur
   noch `RX_CONF3` (AM-Kanal-Dämpfung), das sich nur an echter Hardware mit
   Spektrumanalysator/Oszilloskop bestätigen lässt — siehe "Risiko".
6. **Phase 5 (geliefert, diese Sitzung — letzte Phase):** die sieben Einzelpatches aus
   `firmware-mining/texkom-nfc-protocol-core-port/phase5-registration/` anwenden
   (`nfc_protocol.c/.h`, `nfc_device_defs.c/nfc_poller_defs.c/nfc_listener_defs.c`,
   `SConscript`, `application.fam`, `nfc_protocol_support.c`), dann die vier UI-Dateien aus
   `firmware-mining/texkom-nfc-protocol-core-port/phase5-ui/` unverändert nach
   `applications/main/nfc/helpers/protocol_support/texkom/` übernehmen. Registriert
   `NfcProtocolTexkom` im Protokollbaum, verdrahtet die Dispatch-Tabellen auf die bereits
   gebauten Phase-1-3-Dateien, und macht die UI tatsächlich ladbar (`application.fam`-
   Plugin-Eintrag plus Namenstabellen-Zeile — siehe "Update (Phase 5)" für die beiden
   Lücken, die die ursprüngliche Fassung dieses Punkts noch nicht benannt hatte). Mit
   Phase 5 ist diese Portierung vollständig; es gibt keine weitere Phase.
7. Bauen: `ufbt` (bzw. `fbt firmware_all`) — jede Struct-/Signaturabweichung fällt sofort
   als Compile-Fehler auf. Mit allen fünf Phasen angewendet ist dies jetzt der erste
   sinnvolle vollständige Build-Versuch dieser Portierung — jeder verbleibende
   Compile-Fehler an dieser Stelle wäre ein eigener Fund, den diese Sitzung übersehen hat.
8. Funktionstest lokal (ab Phase 2 mit Hardware nötig, Phase 1 und die Phase-5-
   Render-Logik bereits host-verifiziert): eigenen Texkom-Schlüssel (TK13/TK15/TK17/MMBIT,
   je nach Verfügbarkeit) lesen, Rahmen gegen ein unabhängiges Referenzwerkzeug
   (Proxmark3, falls vorhanden) vergleichen; Emulation gegen ein zweites eigenes
   Lesegerät testen; die Infoanzeige (Lese-Erfolg-Bildschirm, gespeicherte-Karte-Info)
   gegen einen bekannten Schlüssel inklusive eines absichtlich ungültigen CRC-Bytes
   prüfen (sollte "Invalid" plus den erwarteten CRC-Wert zeigen, wie in "Verifikation"
   host-seitig vorweggenommen). Besonders für `RX_CONF3` wichtig: erste Lesetests mit
   einem Spektrumanalysator/Oszilloskop am Antennenausgang begleiten, um die einzige
   noch nicht quelltextseitig bestätigte Registerabweichung tatsächlich zu validieren.

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
- Diese Sitzung (Phase 4a) erneut gegen einen frischen, unabhängigen `dev`-Stand geprüft,
  Commit `525dcce0e461d4f4b852a32cc6e3b4d075b50f17` (2026-10-20, per `git ls-remote`
  bestimmt): `targets/f7/furi_hal/furi_hal_nfc_texkom.c` (527 Zeilen, über zwei
  unabhängige Klone per `md5sum` identisch bestätigt, Prüfsumme
  `38c7f707f2d8c9156cdd726068cc9644`) vollständig gelesen. Zur Ableitung der drei
  Phase-4a-Patches zusätzlich gelesen und gegen die eigene Firmware-Basis verglichen:
  `lib/nfc/nfc.h/.c`, `targets/furi_hal_include/furi_hal_nfc.h`, `targets/f7/furi_hal/
  furi_hal_nfc.c/_i.h/_tech_i.h/_iso15693.c`, `lib/signal_reader/signal_reader.h/.c`,
  `lib/drivers/st25r3916.c/.h/_reg.c/.h` (alle vier Treiberdateien gegen die eigene Basis
  per `diff -q` Byte-für-Byte identisch bestätigt — keine Registeränderung seit meinem
  letzten Firmwarestand). Eigene Firmware-Basis zur Patch-Ableitung erneut über vier statt
  zwei unabhängige Klone geprüft (zwei zur Patch-Herleitung, zwei weitere ausschließlich
  zur `git apply`/`md5sum`-Gegenprobe) — unverändert `b359f6f`.
- Diese Sitzung (Phase 4b) `targets/f7/furi_hal/furi_hal_nfc_texkom.c` erneut über zwei
  unabhängige Abrufe geladen: Prüfsumme weiterhin `38c7f707f2d8c9156cdd726068cc9644`,
  527 Zeilen — identisch zum in Phase 4a geprüften Stand, keine Änderung seither.
- Diese Sitzung (Phase 5) gegen einen frischen `dev`-Stand geprüft, Commit
  `73ea4fca39816213414776731bdb081a3bc9ef76` (2026-11-04, per `git clone --branch dev`
  bestimmt): alle Phase-1-4b-Dateien (`lib/nfc/protocols/texkom/*`,
  `lib/digital_signal/presets/nfc/texkom_signal.{c,h}`,
  `targets/f7/furi_hal/furi_hal_nfc_texkom.c`) per `md5sum` erneut identisch zum in
  Phase 1-4b geprüften Stand bestätigt — seit Phase 4b keine Änderung. Neu gelesen und
  gegen die eigene Firmware-Basis verglichen: `applications/main/nfc/helpers/
  protocol_support/texkom/texkom.c/.h/texkom_render.c/.h`,
  `lib/nfc/protocols/nfc_protocol.c/.h`, `nfc_device_defs.c/nfc_poller_defs.c/
  nfc_listener_defs.c`, `lib/nfc/SConscript`, `applications/main/nfc/application.fam`,
  `applications/main/nfc/helpers/protocol_support/nfc_protocol_support.c` sowie, zum
  Vergleich derselben Baustelle, `applications/main/nfc/helpers/protocol_support/
  st25tb/st25tb.c` (eigene Basis, als nächstverwandtes bereits integriertes
  Basisprotokoll-Muster). Eigene Firmware-Basis (`G4MEOVER18/G4MEOVER-FW`, `main`,
  weiterhin unverändert `b359f6f`) über drei unabhängige Klone geprüft (einer zur
  Patch-Herleitung, zwei weitere ausschließlich zur `git apply`/`patch`/`md5sum`-
  Gegenprobe, siehe "Verifikation").
