# BACKLOG — priorisiert

Stand: initial. Wird laufend abgehakt und ergänzt; lokal wird gemerged/gebaut/geflasht.

## Firmware-Feature-Mining
- [x] Unleashed **unlshd-090** vs. mntm-012: **Telcoma/Cardin EDGE**-SubGHz-Protokoll fehlt mir —
      Port-Vorschlag in `firmware-mining/telcoma-cardin-edge-subghz.md`.
- [ ] Unleashed **unlshd-090+** vs. mntm-012: weitere neue/erweiterte **SubGHz-Protokolle** prüfen
      (unlshd-090 selbst brachte darüber hinaus primär Stabilitätsfixes, kein zweites neues
      Protokoll — bei künftigen unlshd-Releases erneut gegenprüfen).
- [x] RogueMaster App-Bündel (`applications/external/`, ~700 Apps) durchgesehen: erster
      konkreter Fund ist die **Wiegand Reader**-App (Physical-Access-Audit, mir bislang komplett
      fehlende Kategorie) — Port-Vorschlag in `firmware-mining/wiegand-reader-app.md`. Bündel ist
      groß genug, dass weitere Kategorien (CAN-Bus, PicoPass/iClass, Seader/SEOS) in künftigen
      Sitzungen einzeln geprüft werden.
- [x] RogueMaster-Kandidat **Access Audit** (`applications/external/access_audit`) geprüft:
      NFC/RFID/iCLASS-Risikoscoring für Zutrittskontroll-Medien, ergänzt die Wiegand-Reader-App
      um die Medienseite — Port-Vorschlag in `firmware-mining/access-audit-app.md`. Verbleibende
      RogueMaster-Kandidaten (CAN-Bus-Apps, PicoPass/Seader) weiterhin offen — größerer
      Portaufwand wegen Hardware-Modul- bzw. ASN.1/mbedTLS-Abhängigkeiten.
- [x] Official-dev seit mntm-012 (2025-12-31): erster konkreter Fund sind zwei zusammengehörige
      NFC-Stack-Buffer-Overflow-Fixes aus PR #4369 (DESFire-Dateisettings-Parser,
      MFUL-`FAST_READ`-Handler bei Emulation) — Port-Vorschlag in
      `firmware-mining/nfc-desfire-mful-stack-overflow-fixes.md`. Weitere Sicherheits-/
      Stabilitäts-Commits seit mntm-012 in künftigen Sitzungen einzeln weiter durchsehen (großer
      Zeitraum, ~7 Monate Commits).
- [x] Official-dev seit mntm-012, weiterverfolgt ab PR #4369 (`0dd3681f`) bis `9148d50c`
      (2026-08-04, 21 Commits): Fund ist PR #4427, LFRFID-PAC/Stanley-Decoder ohne
      NUL-Terminator (Stack-Über-/Unterlesen bei `hex_chars_to_uint8()`, tritt bei jeder
      erfolgreichen Kartenlesung potenziell auf) — Port-Vorschlag in
      `firmware-mining/lfrfid-pac-stanley-nul-terminator-fix.md`. Aus demselben Durchlauf für
      künftige Sitzungen vorgemerkt (noch nicht einzeln geprüft/portiert): `c9c77f66` (NFC
      FeliCa Poller-Fehlerprüfung), `5dc71870` (Infrared `timings_cnt`-Init/Bounds-Fix),
      `8713d1c4` (`bit_lib`-Lese-Bounds-Fix), `9c173b46` (LFRFID Pyramid-Parity-Bit-Fix).
      Danach weiter ab `9148d50c` gegenprüfen (Datum der jeweiligen Sitzung als neuer
      Startpunkt).
- [x] Aus demselben Durchlauf weiterverfolgt: `2fe56050` (NFC ISO15693) geprüft — deckt zwei
      getrennte Bugs ab: `READ_MULTI_BLOCKS` ignoriert bei mir (mntm-012,
      `ISO15693_3_LISTENER_BUFFER_SIZE = 256U`) Anfragen nach mehr als 63 Blöcken komplett
      (Emulations-Korrektheitsbug, kein Overflow möglich), und der ISO15693-Signal-Parser
      (`iso15693_parser.c`, Software-Bitbang-Empfangspfad, läuft bei jedem
      ISO15693/Vicinity-Lesevorgang) kann bei RF-Rauschen einen harten `furi_check`-Abbruch
      auslösen (echter DoS/Geräteneustart, mit Host-Repro unter ASan/UBSan verifiziert) — Port-
      Vorschlag in
      `firmware-mining/nfc-iso15693-read-multi-blocks-and-parser-overrun-fix.md`.
- [x] Aus demselben Durchlauf weiterverfolgt: `8713d1c4` (`bit_lib_get_bits()`) geprüft — liest
      bei unausgerichtetem Zugriff immer ein Byte hinter der aktuellen Position, auch wenn die
      angefragten Bits vollständig im aktuellen Byte liegen; bei Puffern ohne Sicherheitsabstand
      am Ende (Muster kommt in mehreren meiner LFRFID-Decoder vor, z. B. `noralsy_chksum()`) ein
      Lese-Overrun hinter der Allokation, mit Host-Repro unter ASan verifiziert. Betrifft
      indirekt alle LFRFID-/Teile der NFC-Protokoll-Decoder, die auf `bit_lib_get_bits()`
      aufbauen — Port-Vorschlag in
      `firmware-mining/bit-lib-get-bits-read-past-buffer-fix.md`. Weiterhin vorgemerkt aus
      demselben Durchlauf: `c9c77f66` (NFC FeliCa), `5dc71870` (Infrared `timings_cnt`),
      `9c173b46` (LFRFID Pyramid-Parity-Bit). Danach weiter ab `9148d50c` (2026-08-04)
      gegenprüfen.
- [x] Aus demselben Durchlauf weiterverfolgt: `9c173b46` (LFRFID Pyramid-Trailing-Wiegand-
      Parity-Bit) geprüft — `protocol_pyramid_add_wiegand_parity()` berechnet das odd-
      Paritätsbit über `protocol_pyramid_get_parity(source + length / 2, ...)`; `length / 2`
      ist als Bit-Offset gemeint, wirkt auf dem `uint8_t*` aber als Byte-Offset (12 Byte statt
      12 Bit) und liest damit 9+ Byte hinter dem nur 3 Byte großen `wiegand[]`-Puffer aus
      `protocol_pyramid_encode()` — Speicherzugriffsfehler (mit Host-Repro unter ASan als
      `heap-buffer-overflow ... 10 bytes after 3-byte region` verifiziert) UND falsches
      Paritätsbit für reale Kartendaten (50,0 % Falschrate über vollen FC- x CardNum-Raum bei
      fester Speicherfüllung verifiziert), das von jedem paritätsprüfenden Reader abgelehnt
      wird, ohne dass der eigene Decoder (prüft nur CRC-8) es bemerkt — Port-Vorschlag in
      `firmware-mining/lfrfid-pyramid-wiegand-parity-oob-read-fix.md`. Weiterhin offen aus
      demselben Durchlauf: `c9c77f66` (NFC FeliCa), `5dc71870` (Infrared `timings_cnt`). Danach
      weiter ab `9148d50c` (2026-08-04) gegenprüfen.
- [x] Aus demselben Durchlauf weiterverfolgt: `c9c77f66` (NFC FeliCa) geprüft —
      `felica_poller_state_handler_list_system()` dereferenziert den Response-Pointer von
      `felica_poller_list_system_code()` unbedingt, bevor der Fehlerstatus geprüft wird; die
      Funktion schreibt den Pointer nur im Erfolgsfall, sodass jeder Timeout/CRC-Fehler oder
      eine zu kurze Antwort eine uninitialisierte Pointer-Variable dereferenziert — betrifft den
      Standard-Lesepfad jeder FeliCa-Karte (Auth ist per Default übersprungen), mit
      Host-Repro (isolierte `fork()`-Kindprozesse, ungültige Sentinel-Adresse) als
      reproduzierbarer Absturz auf jedem der beiden Fehlerpfade verifiziert — Port-Vorschlag in
      `firmware-mining/nfc-felica-list-system-uninitialized-response-fix.md`.
- [x] Aus demselben Durchlauf weiterverfolgt (letzter noch offener Kandidat): `5dc71870`
      (Infrared `timings_cnt`) geprüft und ausgearbeitet — zwei zusammengehörige Bugs in
      `infrared_common_decoder.c`: `infrared_common_decoder_alloc()` initialisiert
      `timings_cnt` nicht, wodurch der allererste Reset nach Alloc bei RC5 (einziges Protokoll
      mit `preamble_mark == 0`) einen Heap-Müllwert als Länge an `consume_samples()` reicht
      (OOB Lesen UND Schreiben über das 6-Element-`timings[]`-Array hinaus, mit
      fork-isoliertem Host-Repro unter ASan als `heap-buffer-overflow`-Lesezugriff verifiziert);
      unabhängig davon vergleicht die Bounds-Prüfung in `infrared_common_decode()`
      `timings_cnt` gegen `sizeof(decoder->timings)` (24 Byte) statt `COUNT_OF(...)`
      (6 Elemente) und toleriert dadurch 19 statt höchstens 1 Element Overrun, bevor
      `furi_check()` greift (mit Repro numerisch bestätigt: 25 vs. 7) — Port-Vorschlag in
      `firmware-mining/infrared-timings-cnt-init-bounds-fix.md`. Damit ist der seit PR #4369
      verfolgte `dev`-Durchlauf (`0dd3681f` bis `9148d50c`, 2026-08-04) vollständig
      abgearbeitet — alle sechs vorgemerkten Kandidaten sind jeweils als eigener
      Port-Vorschlag dokumentiert.
- [x] `dev` weiter ab `9148d50c` (2026-08-04) gegenprüft: genau ein neuer Commit, `a55e3939`
      (PR #4439, "Fix crash when reading card with a zero-key application", Issue #3835) —
      DESFire-`furi_check`-Absturz bei Applikation/PICC mit `MaxKeys == 0`
      (`mf_desfire_poller_read_key_versions()` sowie `mf_desfire_load()`), tritt bei jedem
      normalen Lesevorgang einer betroffenen Karte auf, kein präparierter Dump nötig —
      Port-Vorschlag in `firmware-mining/nfc-desfire-zero-key-furi-check-crash-fix.md`. `dev`
      steht danach weiterhin exakt auf `a55e3939` — erneut gegenprüfen, sobald neue Commits
      vorliegen.
- [x] RogueMaster CAN-Bus-Kandidaten (`can_bus_attack`, `can_commander`, `can_tools`,
      `can_transceiver`, `canbus`, `canonivm`) geprüft: `can_bus_attack` (MCP2515-GPIO-Modul,
      Sniffer/UDS/OBD-II-PID/Replay/Modify/DoS-Test) deckt als erster Fund eine mir komplett
      fehlende Kategorie ab — Automotive-/CAN-Bus-Pentest — Port-Vorschlag in
      `firmware-mining/canbus-attack-app-port.md`. Restliche CAN-Kandidaten sowie
      PicoPass/Seader/SEOS weiterhin offen (größerer Portaufwand wegen ASN.1/mbedTLS bzw.
      weiterer CAN-Tools, die teils Überschneidungen zu `can_bus_attack` haben dürften — bei
      Bedarf in künftiger Sitzung einzeln gegenprüfen).
- [x] Aus denselben CAN-Kandidaten weiterverfolgt: `can_tools` geprüft (Einzeldatei-App,
      keine Hardware-/Treiberanbindung, DBC-Signal-Erstellung + manuelle Frame-Dekodierung) —
      überschneidet sich nicht mit `can_bus_attack` (dort Live-Sniffing/Injection über
      MCP2515, hier Hardware-unabhängige Offline-Auswertung), ergänzt es als Decoder für
      gesniffte Frames; Bit-Extraktion (Intel- und Motorola-Pfad, Vorzeichenerweiterung)
      am Code gegen die DBC-Spezifikation geprüft, kein Fehler gefunden — Port-Vorschlag in
      `firmware-mining/can-tools-dbc-decoder-app-port.md`. `canonivm` ist trotz CAN-ähnlichem
      Namen ein reiner Kamera-Intervalometer, kein Pentest-Kandidat. Verbleibend offen:
      `can_commander` (großer Funktionsumfang, deutliche Überschneidung mit `can_bus_attack`
      zu erwarten), `can_transceiver`, `canbus` (beide ebenfalls MCP2515-Sniff/Inject,
      vermutlich funktional redundant zu `can_bus_attack`) sowie weiterhin PicoPass/Seader/
      SEOS.
- [ ] NFC/RFID-Tooling-Deltas (Dictionaries, Parser, Magic-Support).

## App-Verbesserungen (reine Logik, ohne HW testbar)
- [x] **g4meover-magspoof**: Card-String→MagDevice-Parser (aus astro-cyberpaws-Fork, identisch in
      `zacharyweiss/magspoof_flipper`) geprüft — Track-2/3-Start-Sentinel-Bug in
      `mag_device_parse_card_string` gefunden und gefixt: Patch in
      `app-improvements/g4meover-magspoof/track2-3-start-sentinel-fix.md`.
- [x] **ProtoPirate**: zentrale Key-Parser-Hilfsfunktion `pp_parse_hex_u64_strict()`
      (`protocols/protocols_common.c`) geprüft — nimmt trotz `_strict`-Namen Hex-Strings mit
      überzähligen Zeichen nach der 16. Ziffer klaglos an (Rest wird stillschweigend
      ignoriert statt die Datei abzulehnen) — Fix in
      `app-improvements/ProtoPirate/hex-key-strict-trailing-garbage-fix.md`. Betrifft alle
      Key-lesenden Protokoll-Decoder gemeinsam (KIA/Ford/PSA/Fiat/Honda/Star Line).
- [x] **RollJam**: interlaced Jam+RX-Aufnahme (`rj_run_jam_capture_phase()`) geprüft —
      `rolljam_capture_start()` setzt bei *jedem* RX-Fenster-Wechsel `sig->count = 0`, statt
      nur beim ersten Eintritt der Phase, wodurch über mehrere Zyklen akkumulierte Flanken
      verworfen werden und Signale, die länger als ein einzelnes 80-ms-RX-Fenster sind, kaum
      je vollständig erfasst werden — Fix in
      `app-improvements/RollJam/interlaced-capture-buffer-reset-fix.md` (neue
      Pause-/Resume-Funktionen statt Reset bei jedem Zyklus).
- [x] **RollLab**: Signalanalyse (`rlab_analyze()`) geprüft — die Stille-Terminator-Flanke,
      mit der die RX-Callbacks das Ende einer Aufnahme markieren (`>= RLAB_SILENCE_US`,
      25 ms LOW), wird mit in `sig->buf[]` abgelegt und fließt ungefiltert in Min/Max/Avg/
      Gesamtdauer sowie die OOK-Bimodalitäts-Heuristik ein — verzerrt die im Analyze-Modus
      angezeigten Werte massiv (Max/Avg/Total viel zu hoch) und lässt die OOK-Erkennung bei
      den meisten realen Signalen fälschlich "non-OOK" melden — Fix in
      `app-improvements/RollLab/analyze-silence-terminator-stats-skew-fix.md` (Terminator-
      Flanke anhand desselben Kriteriums wie die RX-Callback-Bereitschaftsprüfung aus den
      Statistikschleifen ausschließen).
- [x] **RollForge**: gebündeltes RollJam-Tick-Modul (`module_rolljam.c`, nicht-blockierende
      `RJ_P1_JAM/RX`-`RJ_P2_JAM/RX`-Zustandsmaschine) geprüft — trägt denselben
      Puffer-Reset-Bug wie die eigenständige RollJam-App vor ihrem Fix (unabhängige
      Neuimplementierung, Fix wurde nicht mitgezogen): `rj_cap_start()` setzt bei jedem
      JAM→RX-Übergang `sig->count = 0`, statt nur beim ersten Eintritt der Phase — Fix in
      `app-improvements/RollForge/interlaced-capture-buffer-reset-fix.md` (neue
      `cap_started`-Zustand plus Pause-/Resume-Helfer, analog zur RollJam-Lösung, für die
      RollForge-eigene Tick-Architektur portiert). RollForges RollLab-/Jammer-Module und
      restliche eigene Apps (FlipperJammer/lora-ukfe) weiterhin offen.
- [x] **RollForge** Jammer-Modul (`module_jammer.c`) Tiefenaudit: `jm_input()` löst bei
      laufendem Jamming (`JM_JAMMING`) beim Frequenzwechsel (Hoch/Runter) einen
      verbotenen Doppel-Stop der CC1101-Treiber-Zustandsmaschine aus (direkter
      `subghz_devices_stop_async_tx()`-Aufruf im Input-Handler plus ein zweiter,
      interner Stop-Aufruf in `rollforge_rf_start_tx()`, da `app->rf_op` zu diesem
      Zeitpunkt noch auf `RF_JAMMING` steht) — reproduzierbarer Geräteabsturz bei
      jedem Frequenzwechsel während des Jammings, mit `assert()`-Modell der
      Treiber-Zustandsmaschine host-verifiziert — Fix in
      `app-improvements/RollForge/jammer-freq-switch-double-stop-crash-fix.md`.
      Restlicher Jammer-Modul-Code (Frequenzliste, Trägersignal-Erzeugung, Zeichnen)
      mitgeprüft, keine weiteren Probleme gefunden.
- [x] **RollForge** (`module_rolllab.c`, gebündeltes RollLab-Modul): trägt denselben
      Silence-Terminator-Stats-Skew-Bug wie die eigenständige RollLab-App vor ihrem Fix
      (`rl_analyze()` iteriert über die volle `sig->count` inkl. der ≥25-ms-Stille-Terminator-
      Flanke) — Fix in
      `app-improvements/RollForge/analyze-silence-terminator-stats-skew-fix.md`. Wegen
      RollForges einfacherer, verhältnisbasierter OOK-Heuristik (`max_us/min_us >= 2`, keine
      obere Schranke) kippt die Fehlerrichtung hier ins Gegenteil der eigenständigen App:
      statt falschem "non-OOK" meldet RollForge fälschlich "OOK" für Signale mit eng
      beieinanderliegenden (nicht-bimodalen) Pulsbreiten, mit Testprogramm verifiziert.
      RollForges Jammer-Modul sowie FlipperJammer/lora-ukfe weiterhin offen.
- [x] **FlipperJammer** (eigenständige App, bislang komplett ungeprüft): Tiefenaudit aller
      Module (`ble_jam.c`, `fuzzer.c`, `ir_jam.c`, `jammer_storage.c`, `nfc_jam.c`,
      `nrf24_jam.c`, `nrf24_spi.c`, `rfid_jam.c`, `spoofer.c`, `subghz_jam.c`, `usb_fuzz.c`,
      `wifi_jam.c`, `jammer_app.c`) — Fund im RFID-Spoofer-Teil von `spoofer.c`:
      `em4100_encode()` schreibt in beiden Zweigen ihrer Bitfall-Unterscheidung denselben
      Literalwert, wodurch die erzeugte Manchester-Zeitdauer-Sequenz komplett unabhängig von
      `facility`/`card_id` ist (zwei verschiedene Karten-IDs erzeugen byte-identische Puffer);
      zusätzlich fehlen die im Funktionskommentar selbst angekündigten 14 Zeilen-/
      Spaltenparitätsbits eines echten 64-Bit-EM4100-Frames komplett (nur 50 statt 64 Bit) —
      Fix in `app-improvements/FlipperJammer/em4100-encoder-data-independent-output-fix.md`
      (LevelDuration-basierte Neufassung mit explizitem Level pro Bit plus vollständigem
      Paritäts-Frame, mit Rundtrip-Decoder-Testprogramm verifiziert). `subghz_jam.c` bereits
      mit sauberer State-Machine ohne den in RollForges Jammer-Modul gefundenen
      Doppel-Stop-Bug; restliche Module ohne weitere Logikfehler dieser Kategorie. `lora-ukfe`
      weiterhin ungeprüft.
- [x] **lora-ukfe** (eigenständige App, Quelle identifiziert: `G4MEOVER18/lora-ukfe`,
      Flipper-Gegenstück zum Heltec-ESP32-LoRa-Agenten): neue RF-Console
      (`rf/rf_comm.c`, `rf/ukfe_rf.c`, CC1101-Funkkanal als Ersatz für die
      UART/JSON-Bridge) geprüft — der monoton steigende Anti-Replay-Rolling-Counter
      (`s_rf.counter`) lebt nur in einer statischen RAM-Variable und beginnt bei jedem
      App-Neustart wieder bei 0, während der Heltec-Agent seinen zuletzt akzeptierten
      Counter-Stand behält; da `ukfe_rf_parse_frame()` strikt `counter <= last_counter`
      ablehnt, verwirft der Heltec nach *jedem* Neustart so viele Kommandos, wie in der
      vorherigen Sitzung gesendet wurden — die Konsole sperrt sich bei jedem Neustart
      selbst aus, ohne sichtbare Fehlermeldung. `rf_comm_counter()` war laut eigenem
      Kommentar bereits "für Anzeige/Persistenz" vorgesehen, aber nirgends aufgerufen —
      Persistenz war offensichtlich geplant, nie angeschlossen. Fix in
      `app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md` (Counter-Reserve
      mit Sicherheitsspanne, auf SD persistiert, crash-sicher; reine Reserve-Logik mit
      Testprogramm gegen ein Modell der Heltec-seitigen Prüfung verifiziert). Restliche
      App (`json_parse.c`/UART-Fallback-Pfad, Szenen, LoRa/WiFi-Scan-Befehle)
      mitgeprüft, keine weiteren Logikfehler dieser Kategorie gefunden; die
      Heltec-Firmware selbst liegt außerhalb dieses Repos und wurde nicht geprüft.
- [x] **usb-army-penetrator** (eigenständiges ESP32/RP2040-"USB Army Knife"-Repo, bislang
      ungeprüft; laut Ökosystem-Karte die Heltec/ESP32-seitige Gegenstelle zur
      `lora-ukfe`-RF-Console): `UkfeRfControl` (`src/Comms/Radio/UkfeRfControl.cpp`/`.h`),
      die Empfängerseite desselben `ukfe_rf`-Protokolls, geprüft — schließt genau die in
      der vorherigen `lora-ukfe`-Sitzung offen gelassene Lücke ("Heltec-Firmware liegt
      außerhalb dieses Repos"). Fund: `_lastCounter` (RX-seitiges Anti-Replay-Fenster) ist
      ein reines RAM-Feld, das bei jedem ESP32-Neustart auf 0 zurückfällt, obwohl
      `begin()`/`loop()` bereits einen `Preferences&`-Parameter für genau diesen Zweck
      durchgereicht bekommen, ihn aber mit `(void)prefs;` verwerfen — im Unterschied zum
      TX-seitigen Fund (Verfügbarkeits-Bug/Selbstaussperrung) öffnet das hier das
      Anti-Replay-Fenster wieder: ein mitgeschnittenes, gültig signiertes Funkframe kann
      nach jedem Neustart erneut akzeptiert werden (Replay-Schwachstelle im eigenen
      Sicherheitsmechanismus), mit Host-Repro (echte `ukfe_rf_build_frame`/
      `ukfe_rf_parse_frame`-Funktionen, ASan/UBSan) verifiziert — Fix in
      `app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
      (dieselbe bereits verifizierte Reserve-mit-Sicherheitsspanne-Logik wie beim
      TX-seitigen Fix, hier auf den RX-seitigen Counter angewendet, über den bereits
      vorhandenen `Preferences`-Kanal persistiert). Restlicher `usb-army-penetrator`-Code
      (WiFi/BLE/USB-HID/Marauder-Module, Web-UI) weiterhin ungeprüft.
- [x] **usb-army-penetrator** Web-UI (`src/Comms/Web/WebServer.cpp`) geprüft: fünf
      Handler (`/runfile`, `/showimage`, `/download`, `/delete`, `handleUpload`/
      `/uploadFile`) nehmen einen client-gelieferten `filename` (Query-Parameter bzw.
      Multipart-`Content-Disposition`) komplett ungeprüft entgegen und reichen ihn direkt
      an die Storage-Schicht durch — kein `..`/Backslash/Steuerzeichen-Filter irgendwo im
      Repo (Storage-Abstraktion prüft ebenfalls nicht). Bei meiner flachen Root-Ablage
      (alle `.ds`-Payloads direkt unter `/`, Kategorisierung nur über Dateinamens-Präfix)
      ist ein `/`/`..` im Filename nie legitim — Path-Traversal-Lücke im eigenen
      Web-Control-Interface (Lesen/Löschen/Überschreiben beliebiger Dateien außerhalb des
      Payload-Verzeichnisses), relevant weil das Interface bei Einsätzen zwangsläufig
      zumindest kurzzeitig in fremden Netzsegmenten erreichbar sein kann. Fix (gemeinsame
      `isFilenameSafe()`-Prüfung vor allen fünf Aufrufstellen) in
      `app-improvements/usb-army-penetrator/webui-filename-path-traversal-fix.md`.
      Restlicher `usb-army-penetrator`-Code (WiFi/BLE/USB-HID/Marauder-Module)
      weiterhin ungeprüft.

## Ideen-Recherche
- [x] Kuratierte Liste starker Pentest-Apps (SubGHz/NFC/RFID/BadUSB/2.4G) mit API-87-Status:
      `ideas/pentest-app-katalog.md` (dieser Backlog-Eintrag war bereits erledigt, aber hier
      nie abgehakt worden — nachgetragen).
- [x] Nützliche `.sub`-/NFC-/RFID-**Konzepte** (keine echten Captures) + eigene App-Ideen: erster
      konkreter Fund ist das SubGHz-**Fixed-Code-Bruteforce**-Konzept (Coderaum-Erschöpfung mit
      Binärsuch-Ansatz statt naiver De-Bruijn-Kompression, die bei den meisten realen Protokollen
      wegen Stop-/Pilot-Bits nicht greift) — deckt Fixed-Code-Sender ab, eine Geräteklasse, die
      RollJam/RollForge/Telcoma-Cardin (alle auf Rolling Code ausgelegt) bewusst nicht abdecken;
      kein Firmware-Patch nötig, nutzt die vorhandene Stock-Sub-GHz-Sendefunktion:
      `ideas/subghz-fixed-code-bruteforce-konzept.md`. Weitere `.sub`-Konzepte bleiben für
      künftige Sitzungen offen.
- [x] NFC-seitiges Konzept (aus vorheriger Sitzung vorgemerkt): **Mifare-Classic-Nested-Attack-
      Automatisierung** — `AloneLiberty/FlipperNested` (card-only Nested/Static-Nested/Hard-
      Nested-Angriff, portiert aus Proxmark3 Iceman-Fork) schließt die Lücke zwischen Stock-
      Dictionary-/Darkside-Angriff (findet i. d. R. nur einen Sektorschlüssel) und einer
      vollständigen Sektorschlüssel-Prüfung ohne erneute Reader-Interaktion; benötigt zwingend
      mindestens einen bereits bekannten Schlüssel als Startpunkt, Schlüsselberechnung läuft
      PC-seitig über das Begleit-Skript `FlipperNestedRecovery` — kuratiert in
      `ideas/mifare-classic-nested-attack-konzept.md`. Weitere NFC-/RFID-Konzepte bleiben für
      künftige Sitzungen offen.
- [x] Companion-Tool-Ideen: PC-Helfer für Flipper-Apps — Grundbaustein gefunden (SD Card Mounter,
      USB-MSC-Durchreichung der SD-Karte ohne Kartenausbau, Basis für `Vault`/
      `Capture-Dedup-Indexer`): `ideas/companion-tool-recherche.md`.
- [x] Raspberry-Pi-Deck-Stufe (baut auf obigem Fund auf): Pi-seitiger Gegenbaustein gefunden
      (**pi-usb-automount**, headless Auto-Mount/-Unmount ohne Hook) plus eigener Sync-Auslöser-
      Entwurf (systemd-Path-Unit + rsync-Skript, kopieren-only, Marker-Datei-Absicherung gegen
      die von SD Card Mounter bereits bekannte Korruptionsgefahr bei zu frühem Mount-Ende):
      `ideas/raspberry-pi-deck.md`. Lokale Umsetzung/Test mit Test-SD-Karte weiterhin offen.
- [x] Aus dem Katalog-Auftrag "Metroflip / iButton-Brute / nRF24-Apps: API-87-Status
      verifizieren" (`ideas/pentest-app-katalog.md`) den ersten Kandidaten abgearbeitet:
      **Metroflip** (Transit-Karten-Parser, ~20 Betreiber, aktiv gepflegt) geprüft — reine
      Zusatz-App ohne Firmware-Patch, nutzt ausschließlich Standard-NFC-HAL-Protokolle
      (MIFARE Classic/DESFire/Ultralight, ISO14443-4, FeliCa, ST25TB) plus einen app-eigenen
      Calypso-Poller, geringes API-87-Kompatibilitätsrisiko — kuratiert in
      `ideas/metroflip-transit-card-parser-konzept.md`.
- [x] Aus demselben Auftrag weiterverfolgt: **iButton-Brute / nRF24-Apps** API-87-Status
      verifiziert — `W0rthlessS0ul/FZ_nRF24_jammer` (nRF24-Jammer, GPIO-`.fap`, aktiv gepflegt,
      letzter Commit 3 Tage vor dieser Sitzung) überwiegend ✅, Firmware-Familien-Releases
      (Momentum/RogueMaster/Unleashed) vorhanden, aber keine exakte mntm-Buildnummer belegt.
      Katalog-Fehlzuordnung korrigiert: `W0rthlessS0ul/iButton-brute` ist eigenständige
      Arduino-Nano-Firmware (separates Gerät), kein Flipper-App-Port-Kandidat. Echtes
      Flipper-natives Gegenstück identifiziert (`xMasterX/ibutton-fuzzer`, DS1990/Metakom/
      Cyfral-Fuzzer als `.fap`), aber seit 2023 unverändert — 🔧 Migration nötig, kein reiner
      Rebuild — kuratiert in `ideas/ibutton-brute-nrf24-jammer-api-status.md`.
- [x] Katalog-Kandidat **Subbrute / BruteForcer** (bislang ❓) geprüft: Quelle
      `DarkFlippers/flipperzero-subbrute` (App-ID `subghz_bruteforcer`, aktiv gepflegt, zuletzt
      2026-07-21) identifiziert und gegen `mntm-012` verifiziert — die App ist bereits Teil des
      Firmware-Baums (Momentum-Apps-Submodul, beim `mntm-012`-Tag gepinnter Commit `85b0cdb`
      enthält `subghz_bruteforcer/` bereits), kein Port nötig. Deckt On-Device CAME/NICE/
      Ansonic/Chamberlain/Linear/Unilarm/SMC5326/PT2260/PT2262/Holtek ab und ergänzt damit mein
      eigenes host-skriptbasiertes Bruteforce-Konzept aus einer früheren Sitzung — kuratiert in
      `ideas/subghz-bruteforcer-app-status.md`, Katalog-Status in `ideas/pentest-app-katalog.md`
      von ❓ auf ✅ korrigiert.

## Kompatibilität (Kontext, lokal umgesetzt)
- CLI-Kompat-Shim (`cli_session_open/close`, `cli_vcp`) für Alt-Apps + cli_bridge — Firmware-seitig, lokal.
- 49 Alt-Apps laufen bereits unter `api_mismatch_autoload` (bewiesen per Symbol-Analyse).
