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
      `ideas/subghz-fixed-code-bruteforce-konzept.md`. NFC-/RFID-seitige Konzepte (analog, z. B.
      Mifare-Classic-Nested-Attack-Automatisierung, in dieser Sitzung recherchiert aber noch nicht
      kuratiert) sowie weitere `.sub`-Konzepte bleiben für künftige Sitzungen offen.
- [x] Companion-Tool-Ideen: PC-Helfer für Flipper-Apps — Grundbaustein gefunden (SD Card Mounter,
      USB-MSC-Durchreichung der SD-Karte ohne Kartenausbau, Basis für `Vault`/
      `Capture-Dedup-Indexer`): `ideas/companion-tool-recherche.md`.
- [x] Raspberry-Pi-Deck-Stufe (baut auf obigem Fund auf): Pi-seitiger Gegenbaustein gefunden
      (**pi-usb-automount**, headless Auto-Mount/-Unmount ohne Hook) plus eigener Sync-Auslöser-
      Entwurf (systemd-Path-Unit + rsync-Skript, kopieren-only, Marker-Datei-Absicherung gegen
      die von SD Card Mounter bereits bekannte Korruptionsgefahr bei zu frühem Mount-Ende):
      `ideas/raspberry-pi-deck.md`. Lokale Umsetzung/Test mit Test-SD-Karte weiterhin offen.

## Kompatibilität (Kontext, lokal umgesetzt)
- CLI-Kompat-Shim (`cli_session_open/close`, `cli_vcp`) für Alt-Apps + cli_bridge — Firmware-seitig, lokal.
- 49 Alt-Apps laufen bereits unter `api_mismatch_autoload` (bewiesen per Symbol-Analyse).
