# BACKLOG — priorisiert

Stand: initial. Wird laufend abgehakt und ergänzt; lokal wird gemerged/gebaut/geflasht.

## Firmware-Feature-Mining
- [x] Unleashed **unlshd-090** vs. mntm-012: **Telcoma/Cardin EDGE**-SubGHz-Protokoll fehlt mir —
      Port-Vorschlag in `firmware-mining/telcoma-cardin-edge-subghz.md`.
- [x] Unleashed **unlshd-090+** vs. mntm-012: weitere neue/erweiterte **SubGHz-Protokolle** geprüft
      — Stand `dev`, Commit `945234a` (2026-09-10, API 88.9, noch kein neuer `unlshd-`-Tag seit
      090). CHANGELOG nennt vier Kandidaten: Nice O-Code (neues Protokoll, 52/72 Bit), Security+
      2.0 Keypad-Unterstützung (86-Bit-Erweiterung des bereits vorhandenen `secplus_v2`), neue
      KeeLoq-Hersteller (JCM Tech Gen2/Stagnoli/Telcoma/SEA/2× Wisniowski — Hersteller-Lernschlüssel,
      nicht quelltextlich gegen echte Aufnahmen verifizierbar, daher zurückgestellt) und **Prastel
      42 Bit ist jetzt Rolling Code statt Fixed Code** (aus `came.c` in ein eigenes `prastel.c`
      ausgelagert). Prastel als ersten, am klarsten abgeschlossenen Fund bearbeitet: bei mir läuft
      42-Bit-Prastel noch komplett als CAME-Fixed-Code (`came.c`, keine Feld-Extraktion, kein
      Zähler) — Port-Vorschlag inkl. an meine Basis angepasstem `prastel.c/.h` (mein Stand hat
      weder Unleasheds neue `common.h`-Decoder/Encoder-Infrastruktur noch die globale Tasten-/
      Zähler-Override-Infrastruktur; stattdessen an `came_atomo.c` orientiert) sowie host-seitiger
      Verifikation der übernommenen Bit-Verwürflung (Pack/Unpack-Inversität, Round-Trip auf
      legitimen Nutzlasten, `gen_data()`-Zyklus — je 0 Abweichungen) in
      `firmware-mining/prastel-42bit-rolling-code-port.md` /
      `firmware-mining/repro-prastel-42bit-rolling/repro.c`. Verbleibend zurückgestellt: Nice
      O-Code (neues Protokoll + eigene Installer-Code-Recovery-App, größerer Portaufwand), Security+
      2.0 Keypad (tief in den bestehenden `secplus_v2.c`-Zustandsautomat verwoben, kein sauber
      isolierbares Diff), neue KeeLoq-Hersteller (Schlüsselmaterial-Fragen, siehe oben) — für
      künftige Sitzungen einzeln aufgreifbar.
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
- [x] Restliche RogueMaster-CAN-Kandidaten (`can_commander`, `can_transceiver`, `canbus`)
      geprüft: `can_transceiver` laut eigenem README/`notes.txt` unvollständig (durchweg
      `TODO`-Menüpunkte) — reine Teilmenge von `can_bus_attack`, kein Portkandidat.
      `can_commander` hängt an einem eigenständigen kommerziellen ESP32-Board mit eigener,
      externer Firmware (kein reiner MCP2515-Softwareport) — für künftige Sitzung als größerer,
      separater Prüfpunkt vorgemerkt. `canbus` (Quelle `ElectronicCats/flipper-MCP2515-CANBUS`,
      MIT, aktiv gepflegt, gleiches MCP2515-Pinout wie `can_bus_attack`) ist ein echter Fund —
      bringt SLCAN2CAN-PC-Tool-Bridge, Auto-Baudrate-Erkennung, CSV-Log-Export/-Replay und
      gestufte DTC-Ansicht mit, die `can_bus_attack` fehlen — Port-Vorschlag in
      `firmware-mining/canbus-electroniccats-app-port.md`. Damit ist der seit der
      `can_bus_attack`-Sitzung offene CAN-Bus-Kandidatenthread abgeschlossen; weiterhin offen:
      PicoPass/Seader/SEOS (ASN.1/mbedTLS-Aufwand), `can_commander` (Fremdgerät).
- [x] Unleashed **unlshd-091** vs. **unlshd-090** (letzter zuvor geprüfter Stand) gegengeprüft:
      Fund ist Commit `4e8b746f` (PR #1078), eine unbegrenzte Kaskadenstufen-Schleife in der
      ISO14443-3A-Kollisionsauflösung — bei mir (mntm-012) überschreibt ein hostiler/emulierter
      Tag, der bei jeder Kaskadenstufe erneut `0x88` als erstes NFCID-Byte liefert, `data->uid[10]`
      unbegrenzt (Heap-Speicherzugriffsfehler mit tag-kontrolliertem Inhalt, betrifft jeden
      NFC-A-Lesevorgang), mit Host-Repro unter ASan als `heap-buffer-overflow` beim 4.
      Kaskadenschritt verifiziert — Port-Vorschlag in
      `firmware-mining/iso14443-3a-cascade-uid-overflow.md` (Fix: Kaskadenstufen-Obergrenze,
      unabhängig von der separaten, nicht mitportierten 0x88-vs-SAK-Korrektheitsfrage aus
      demselben Unleashed-Umfeld). `unlshd-091` selbst ist bei künftigen Sitzungen erneut gegen
      spätere Releases zu prüfen (weitere OFW-Merge-Commits in diesem Fenster bereits über die
      eigene Official-dev-Mining-Reihe bis `a55e3939` abgedeckt, siehe oben).
- [x] Aus demselben `unlshd-091`-Fenster weiterverfolgt: `38ba133f` (0x88-vs-SAK-
      Korrektheitsfrage, bewusst nicht mit dem Kaskadenstufen-Fix mitportiert) geprüft —
      die Kollisionsauflösung entscheidet bei mir über `nfcid[0] == 0x88`
      (Kaskadierungs-Tag-Byte), ob eine weitere Kaskadenstufe folgt, statt über das dafür
      vorgesehene SAK-Kaskadenbit (`sak & 0x04`); eine echte, finale 4-Byte-UID, die
      zufällig mit `0x88` beginnt (nicht spezifikationskonform, aber laut Unleashed-Issue
      #1074 auf manchen MIFARE-Classic-/Magic-Karten real vorhanden, hardware-verifiziert
      von @mishamyte), wird dadurch fälschlich als unvollständig behandelt und schlägt
      fehl bzw. liefert eine falsche UID. Reiner Korrektheitsbug, kein Speicherfehler —
      unabhängig vom bereits portierten Kaskadenstufen-Limit anwendbar (das bleibt
      notwendig, da SAK genauso tag-kontrolliert ist wie `nfcid[0]`) — Port-Vorschlag in
      `firmware-mining/iso14443-3a-sak-cascade-bit-fix.md`, inkl. kombinierter Fassung
      beider Fixes. `26318954`/`15e208c0` (EMV-TLV-Längenprüfungen) aus demselben Fenster
      bleiben für eine künftige Sitzung offen.
- [x] Aus demselben `unlshd-091`-Fenster weiterverfolgt: `15e208c0` (EMV-TLV-Längenprüfungen im
      Poller) geprüft und ausgearbeitet — `emv_decode_tlv_tag()` verwendet die kartenkontrollierte
      TLV-Länge (`tlen`, aus der Kartenantwort gelesen, bis 255) an mehreren Stellen ungeprüft als
      `memcpy()`-Länge in fest dimensionierte Felder (u. a. `track_1_equiv[80]`, PAN, AID,
      Applikationsname, Log-Format) bzw. löst bei Überschreitung einen `furi_check()`-Absturz aus
      statt den Tag abzulehnen; `emv_parse_tag()` liest zudem mehrere Header-Bytes ohne
      Längenprüfung, OOB-Lesezugriff bei verkürzten Antworten. Betrifft jeden EMV-Lesevorgang, bei
      mir gegen `mntm-012` als vollständig vorhanden verifiziert (Datei identisch zum
      Unleashed-Vorher-Stand). Eigener Zusatzfund: die Track-2-ASCII-Konvertierung
      (`track_2_equiv[41]`, derselbe Bug-Typ) steht im Unleashed-Vorher-Stand hinter einem
      `LOGS_RELEASE_BUILD`-Guard und existiert dort in einem Release-Build gar nicht — bei mir
      (Momentum) fehlt dieser Guard, der Codepfad ist bei mir in jedem Build aktiv, macht diesen
      Fall für mich schwerwiegender als für Unleashed selbst. Mit Host-Repro (ASan,
      fork-isolierte Testfälle für die beiden schwerwiegendsten Stellen) als
      `stack-buffer-overflow` bei kartenkontrolliertem Maximalwert verifiziert, identisches
      Verhalten für realistische Werte bestätigt — Port-Vorschlag in
      `firmware-mining/emv-tlv-length-bounds-fix.md`. Verwandter, bewusst nicht mitportierter
      Fund aus demselben Fenster: `26318954` (PR #1056, dateikontrollierte statt
      kartenkontrollierte Längen beim Laden einer `.nfc`-Datei) — eigenständiger Angriffsvektor,
      für eine künftige Sitzung vorgemerkt.
- [x] Aus demselben `unlshd-091`-Fenster den zuletzt offenen Restfund aufgegriffen: `26318954`/
      `2631895419075d7df8d71be7e3d93465fcc6a491` (PR #1056, "[NFC] Bound file-controlled lengths
      in the EMV .nfc loader") geprüft und ausgearbeitet — `emv_load()` (Datei-Laden, nicht der
      Poller) verwendet drei unbegrenzte `strcpy()`s in Namensfelder fester Breite sowie
      `PAN length`/`AID length` als datei-kontrollierten `uint32_t`-Wert ungeprüft als
      `read_hex()`-Schreiblänge in `pan[10]`/`aid[16]` — eine präparierte `.nfc`-Datei
      überschreibt beim bloßen Öffnen Heap-Speicher, kein Kartenkontakt nötig (eigenständiger
      Angriffsvektor gegenüber dem bereits portierten kartenkontrollierten Poller-Fix). Bei mir
      gegen `mntm-012` als vollständig vorhanden verifiziert (`emv.c` byteidentisch zum
      Unleashed-Vorher-Stand). Mit Host-Repro (ASan, fork-isolierte Testfälle, heap-allokiertes
      Zielfeld wie in `emv_alloc()`) für alle drei Fälle als `heap-buffer-overflow` bei
      dateikontrolliertem Überlängen-Wert verifiziert, identisches Verhalten für reguläre, selbst
      gespeicherte Dateien bestätigt — Port-Vorschlag in
      `firmware-mining/emv-nfc-file-loader-length-bounds-fix.md`. Damit ist das gesamte
      `unlshd-091`-EMV-Absicherungsfenster (Poller + Loader) abgeschlossen.
- [ ] NFC/RFID-Tooling-Deltas (Dictionaries, Parser, Magic-Support). Dictionary-Teilaspekt
      (Classic/Ultralight-C) unten separat abgehakt; Magic-Support-Teilaspekt geprüft: App
      `nfc_magic` (Gen1A/1B/Gen2/Gen4 GTU) ist bereits im `mntm-012`-Firmware-Baum enthalten
      (identischer Submodul-Commit wie Subbrute/PicoPass/Seader), kein Portaufwand, von der
      eigenen Haupt-NFC-App selbst referenziert — Details in `ideas/nfc-magic-app-status.md`,
      Katalog-Zeile ergänzt. Gen3-(APDU)/Gen4-GDM-Blanks bleiben unabgedeckt (neuere
      Community-Version könnte das nachrüsten, noch nicht geprüft). Parser-Teilaspekt weiterhin
      offen (unklar spezifiziert, in künftiger Sitzung zu konkretisieren).
- [ ] MIFARE Plus / Ultralight AES: schlüsselbasierte Authentifizierung (`MfPlusKey`,
      `MfUltralightAesKey`, Auth-Poller-Zustände) fehlt mir komplett — nur beim
      `nfc-unified-key-dictionary.md`-Vorschlag als Lücke entdeckt, noch nicht selbst als
      Firmware-Mining-Fund bearbeitet. Eigenes, größeres Portierungsvorhaben (neue Datenfelder in
      `MfPlusData`/`MfUltralightData`, neue Poller-Logik), vermutlich über mehrere
      Unleashed-Versionen gewachsen statt in einem Commit — Quellsuche noch offen.
- [x] Unleashed `unlshd-092` vs. `unlshd-091` (letzter zuvor geprüfter Stand): großer,
      gesquashter RAM-Reduktions-Commit `2b4411f23` (PR #1073) geprüft. Daraus den in sich
      abgeschlossenen Teilschritt "ein gemeinsamer Schlüssel-Dictionary-Manager statt Duplikat je
      Kartentyp" herausgelöst und geprüft: zehn fast identische MIFARE-Classic-/Ultralight-C-
      Schlüsselverwaltungsszenen lassen sich durch fünf generische Szenen plus eine kleine
      Tabelle (`nfc_key_dict.c/.h`) ersetzen — Korrektheits-Fix beim Löschen (Classic las bisher
      aus einem veralteten Snapshot statt den Schlüssel frisch aus der Datei) und ein
      App-weites Byte-Input-Datenleck zwischen den beiden Dictionaries (zuvor eingetippter
      Schlüssel schimmert im nächsten Dictionary durch) inklusive. Beim Abgleich gegen mntm-012
      festgestellt: die Plus-/Ultralight-AES-Hälfte des Original-Features ist bei mir nicht
      portierbar, da `MfPlusKey`/`MfUltralightAesKey` und die zugehörige schlüsselbasierte
      Authentifizierung in meiner Basis komplett fehlen (nur UID/Version/Security-Level für
      Plus, keine Auth-Poller-Logik) — eigenständiges, größeres Firmware-Feature, separat
      vorgemerkt. Port-Vorschlag bewusst auf die bei mir vorhandene Classic-/Ultralight-C-Hälfte
      beschränkt: `firmware-mining/nfc-unified-key-dictionary.md`.
- [x] Aus Commit `b8fe671` ("Add 6 new security tools") im RogueMaster-Bündel weiterverfolgt
      (bislang abgearbeitet: `subghz_jammer`/SubGHz Jammer Detector, `rogue_ap_detector`):
      **Rayhunter Client** geprüft (Quelle `barkandbite/flipper_suite`, dieselbe wie die beiden
      vorherigen Funde) — Statusanzeige für den EFF-Rayhunter-IMSI-Catcher-Detektor auf einem
      Orbic-RC400L-Hotspot, per UART/ESP32-WiFi-Bridge angebunden. Dritte defensive
      Erkennungskategorie (nach SubGHz-Dauerträger und WiFi-Evil-Twin) auf derselben
      ESP32-Hardwarebasis. API-87-Kompatibilität gegen den eigenen `mntm-012`-Header-Stand
      verifiziert (identisches `expansion_*`/`furi_hal_serial_*`-Muster wie Rogue AP Detector);
      Quelltext-Review ohne Speicherzugriffsfehler (durchgängig `strncpy`/`snprintf` mit
      expliziter Zielgröße, Teardown-Reihenfolge in `rh_app_free()` verhindert Use-after-free
      durch einen noch laufenden UART-Callback) — Port-Vorschlag in
      `firmware-mining/rayhunter-client-app-port.md`. Aus derselben Commit-Reihe weiterhin offen:
      `uart_sniff`, `ble_scanner`, `evil_ble`.
- [x] Aus demselben `b8fe671`-Rückstellpunkt weiterverfolgt (`uart_sniff` inzwischen separat als
      Ideen-Recherche abgearbeitet): **BLE Scanner** geprüft (Quelle `barkandbite/flipper_suite`,
      dieselbe wie die vier bereits geprüften Apps) — BLE-Advertisement-Scanner mit
      AirTag-/Find-My-Heuristik über ESP32/Marauder, vierte defensive Erkennungskategorie
      (Bluetooth Low Energy) auf derselben ESP32-Hardwarebasis. API-87-Kompatibilität gegen den
      eigenen `mntm-012`-Header-Stand verifiziert (identisches `expansion_*`/
      `furi_hal_serial_*`/`furi_stream_buffer_*`-Muster wie die drei vorherigen Funde);
      Quelltext-Review ohne Speicherzugriffsfehler. Stattdessen ein Zustandsfehler gefunden:
      `ble_uart_is_connected()` ist ein reines Einweg-Flag, das nach dem ersten empfangenen
      UART-Byte nie wieder zurückgesetzt wird — fällt das ESP32-Board während eines laufenden
      Scans aus, zeigt die App dauerhaft „[scanning]" statt „[No ESP32]", ein stiller Ausfall bei
      einem AirTag-/Skimmer-Sweep vor Ort. Mit Host-Repro (simulierter Tick-Zähler) verifiziert:
      ALT bleibt nach Erstverbindung dauerhaft „verbunden" hängen, NEU (Staleness-Prüfung gegen
      den Zeitpunkt der letzten Zeile) erkennt sowohl den Ausfall nach 3 s Stille als auch eine
      sofortige Rückkehr — Port-Vorschlag in `firmware-mining/ble-scanner-app-port.md`. Aus
      derselben Commit-Reihe weiterhin offen: `evil_ble`.
- [x] Letzter aus dem `b8fe671`-Rückstellpunkt noch offene Kandidat geprüft: **Evil BLE**
      (Quelle `barkandbite/flipper_suite`, dieselbe wie die fünf bereits geprüften Apps) —
      BLE-Device-Cloning-Gegenstück zu BLE Scanner: Ziel per ESP32/Marauder-Scan auswählen,
      MAC+Advertisement-Payload über die native `furi_hal_bt_extra_beacon_*`-API klonen. UART-Layer
      identisch robust zu BLE Scanner (durchgängig `strncpy`/`snprintf` mit expliziter
      Zielgröße, Geräteliste fest auf 32 begrenzt). Gefunden wurde ein Logikfehler in der
      Klon-Payload-Erzeugung: `evil_ble_start_clone()` behandelt für Zielgeräte ohne echten Namen
      den vom Scanner selbst erzeugten Anzeige-Platzhalter `"(AA:BB:CC:DD:EE:FF)"` ungeprüft als
      echten, sendefertigen Namen und broadcastet ihn als „Complete Local Name"-Feld — obwohl der
      Scanner-Code exakt dieselbe Prüfung (`name[0] != '\0' && name[0] != '('`) an zwei anderen
      Stellen bereits selbst verwendet, nur an dieser dritten Stelle fehlt sie. Für den
      Testzweck der App (Identitätsvalidierung über Name/MAC hinaus prüfen) macht das
      namenlose Zielgeräte-Klone sofort als Fälschung erkennbar. Mit Host-Repro (exakter
      Byte-Konstruktionscode des Fallback-Zweigs, vier Fälle inkl. Regression für den
      Normalfall mit echtem Namen) verifiziert — Port-Vorschlag inkl. Fix in
      `firmware-mining/evil-ble-app-port.md`. Damit ist die gesamte `b8fe671`-Commit-Reihe
      („Add 6 new security tools") jetzt vollständig abgearbeitet.
- [x] Restlichen NFC-Werkzeug-Cluster weiterverfolgt (Rückstellpunkt aus der
      `mfdesfire_auth`-Sitzung): **NFC Sniffer** (`applications/external/nfc_sniffer`, Quelle
      `kdiller713/public-FlipperApps`) geprüft — echte, funktionierende App (passiver
      NFC-Kommando-Logger im Listener-Modus, ISO14443-3A/ISO15693-3/FeliCa, Log auf SD).
      Gefunden: `nfc_sniffer_callback()` in `ui/log_scene.c` schreibt vor der eigentlichen
      Hex-Umwandlung einen zusätzlichen NUL-Terminator ein Byte hinter das korrekt bemessene
      Stack-Array — ein Ein-Byte-Stack-Overwrite bei jedem einzelnen, nicht-leeren erfassten
      Kommando (Normalbetrieb, kein Sonderfall), mit Host-Repro unter ASan als
      `dynamic-stack-buffer-overflow` verifiziert; die vorab geschriebene Zeile ist zugleich
      überflüssig, da der letzte Schleifendurchlauf den Terminator ohnehin korrekt setzt —
      Fix ist die ersatzlose Streichung dieser einen Zeile. API-87-Kompatibilität gegen den
      eigenen `mntm-012`-Header-Stand verifiziert (`lib/nfc/nfc.h`, `lib/toolbox/bit_buffer.h`)
      — Port-Vorschlag inkl. Fix in `firmware-mining/nfc-sniffer-app-port.md`. Restlicher
      Cluster weiterhin offen: `chameleon`, `nfc_relay`, `mifare_fuzzer`, `nfc_rfid_detector`,
      `nfctools`, `uhf_rfid`, `nfc_fuzzer`, `nfc_canary`, `nfc_comparator`,
      `simultaneous_rfid_reader`.
- [x] Restlichen NFC-Werkzeug-Cluster weiterverfolgt: **NFC Fuzzer**
      (`applications/external/nfc_fuzzer`, Quelle `barkandbite/flipper_suite` — dieselbe Quelle
      wie mein bereits verifizierter UART-Sniff-/Rogue-AP-Detector-/H10301-Writer-Fund) geprüft
      — Reader-/Tag-Protokollfuzzer, 11 Profile × 4 Mutationsstrategien, 3404 Zeilen über 3
      Quelldateien. Quelltext trägt bereits Fix-Kommentare (`Issue 2/6/7/8`) für die üblichen
      Speicher-/Nebenläufigkeitsfehler dieses App-Musters — beim eigenen Review kein weiterer
      Speicherzugriffsfehler gefunden (alle Testfall-Generatoren gegen `NFC_FUZZER_MAX_PAYLOAD_LEN`
      und ihre `uint8_t`-Längenfelder einzeln nachgerechnet). Stattdessen ein
      Architekturfehler gefunden: Profil "ISO15693 Fuzzing" sendet seine fuzzten
      ISO15693-Antwort-Bytes durchgehend über eine ISO14443-3A-Listener-Session statt über
      einen tatsächlichen `NfcProtocolIso15693_3`-Listener — kein einziges `Iso15693_3`-API-
      Symbol im gesamten App-Quellbestand verwendet (per Volltextsuche verifiziert), macht das
      Profil gegen echte ISO15693-Reader wirkungslos, ohne dass die App das erkennbar meldet.
      Die übrigen 10 Profile korrekt gegen die jeweils passende NFC-HAL-Session verdrahtet.
      API-87-Kompatibilität gegen den eigenen `mntm-012`-Header-Stand verifiziert, inkl. der für
      die Fix-Skizze benötigten `iso15693_3_alloc`/`_set_uid`/`NfcProtocolIso15693_3`/
      `Iso15693_3ListenerEventTypeCustomCommand`-Signaturen — Port-Vorschlag inkl. Fix-Skizze in
      `firmware-mining/nfc-fuzzer-app-port.md`. Restlicher Cluster weiterhin offen: `chameleon`,
      `nfc_relay`, `mifare_fuzzer`, `nfc_rfid_detector`, `nfctools`, `uhf_rfid`, `nfc_canary`,
      `nfc_comparator`, `simultaneous_rfid_reader`.
- [x] Restlichen NFC-Werkzeug-Cluster weiterverfolgt: **NFC/RFID Detector**
      (`applications/external/nfc_rfid_detector`, Quelle `SkorP`) geprüft — mit 407 Zeilen
      kleinster verbleibender Cluster-Kandidat, reine passive Feld-Anwesenheitserkennung
      (NFC 13,56 MHz / LF-RFID 125 kHz, keine Karten-/Reader-Interaktion). UI-/Decay-Logik
      (`views/nfc_rfid_detector_view_field_presence.c`) sauber implementiert, kein Fund.
      Fund in `nfc_rfid_detector_app_i.c`: `nfc_rfid_detector_app_field_presence_start()`
      verwirft den `FuriHalNfcError`-Rückgabewert von `furi_hal_nfc_acquire()` und ruft
      unconditional `furi_hal_nfc_field_detect_start()` auf; schlägt `acquire()` mit
      `FuriHalNfcErrorBusy` fehl (Mutex bereits von einem anderen Thread gehalten), bleibt der
      SPI-Bus-Handle unmarkiert, und die nachfolgende SPI-Registerschreibung löst
      `furi_check(handle->bus->current_handle == handle)` aus — Hard-Fault/Reboot statt eines
      rückgabewertbasierten Fehlerpfads; derselbe ungeprüfte Zustand lässt auch den Exit-Pfad
      (`furi_hal_nfc_release()`, `furi_check(furi_hal_nfc_is_mine())`) crashen. Gegen
      `targets/f7/furi_hal/furi_hal_nfc.c`/`furi_hal_spi.c` (mntm-012) als exaktes
      Implementierungsverhalten verifiziert, mit Host-Repro (fork-isolierte Testfälle, ASan/UBSan)
      bestätigt: Absturz ausschließlich im Busy-Fall, Normalfall unberührt — Port-Vorschlag inkl.
      Fix (Statusfeld `nfc_acquired`, alle drei Hardware-Funktionen entsprechend abgesichert) in
      `firmware-mining/nfc-rfid-detector-app-port.md`. Restlicher Cluster weiterhin offen:
      `chameleon`, `nfc_relay`, `mifare_fuzzer`, `nfctools`, `uhf_rfid`, `nfc_canary`,
      `nfc_comparator`, `simultaneous_rfid_reader`.
- [x] Restlichen NFC-Werkzeug-Cluster weiterverfolgt: **NFC Canary**
      (`applications/external/nfc_canary`, Quelle `antitree`) geprüft — mit 1608 Zeilen
      vollständig gelesen, reiner passiver Skimming-/Interrogations-Detektor über das
      ST25R3916-Feld-Detektor-Bit (Vibration/Ton/LED-Alarm, Event-Log auf SD). Architektur
      sauber (asymmetrisches Flanken-Latch statt symmetrischer Entprellung, geprüfter
      `furi_hal_nfc_acquire()`-Rückgabewert, sauberer Feld-Teardown beim Beenden); der geplante
      "Decoy"-Listener-Modus (Protokoll-/Kommando-Erfassung) ist im Quelltext vorbereitet, aber
      selbst korrekt aus dem Einstellungsmenü entfernt, solange er nicht verdrahtet ist — kein
      Fund. Fund liegt im Standardwert der Ton-Alarmschwelle (`settings.c`,
      `min_tier_sound = TierAlarm`): der ausgelieferte Sentinel-only-Build (v0.2) erreicht bei
      einer echten Erkennung nie `TierAlarm` (nur bis `TierWarn`), wodurch der Ton-Kanal mit
      Standardeinstellung dauerhaft stumm bleibt — die Test-Audition im Menü (`alert_test()`)
      injiziert `TierAlarm` jedoch direkt und täuscht dabei Funktionsfähigkeit vor. Mit
      Host-Repro (`gcc -O1 -Wall -Wextra -fsanitize=address,undefined`) bestätigt: ALT 0/2 echte
      Erkennungsstufen lösen Ton aus, NEU (Fix auf `TierWarn`, zusammen mit Vibration) 1/2 —
      Port-Vorschlag inkl. Fix in `firmware-mining/nfc-canary-app-port.md`. Dabei zwei weitere
      Kandidaten geprüft und zurückgestellt: `chameleon` (reiner Fernsteuerungsclient für
      externes Chameleon-Ultra-Gerät, Fremdhardware wie `can_commander`) und `nfc_relay`
      (benötigt zwei eigene Flipper-Geräte). Restlicher Cluster weiterhin offen: `mifare_fuzzer`,
      `nfctools`, `uhf_rfid`, `nfc_comparator`, `simultaneous_rfid_reader` (letztere zwei
      vermutlich ebenfalls Fremdhardware-Fall, externes UHF-Lesegerät nötig).
- [x] Restlichen NFC-Werkzeug-Cluster weiterverfolgt: **Mifare Fuzzer**
      (`applications/external/mifare_fuzzer`, Quelle `spheeere98`/`Sil333033`) geprüft — mit
      1503 Zeilen kleinster verbleibender Kandidat, vollständig gelesen. UID-Fuzzing gegen
      NFC-Reader (Test Values/Random Values/Load-from-File) über die eingebaute Antenne, keine
      Zusatzhardware. Fund: `MifareFuzzerEventStartAttack`-Handler alloziert bei jedem
      UID-Wechsel ein `Iso14443_3aData`, das nie wieder freigegeben wird (`iso14443_3a_free()`
      fehlt), obwohl die eigene `mifare_fuzzer_worker_set_nfc_data()` direkt daneben das
      korrekte alloziert-und-wieder-freigegeben-Muster für ihre interne Kopie zeigt — betrifft
      jeden laufenden Angriff, nicht nur eine Fehleingabe. Mit Host-Repro
      (`gcc -O1 -Wall -Wextra -fsanitize=address,undefined`, LeakSanitizer) bestätigt: ALT 700
      Byte/50 Zyklen geleakt, NEU 0 Lecks — Port-Vorschlag inkl. Fix in
      `firmware-mining/mifare-fuzzer-app-port.md`. Zusätzlich eine unbegrenzte
      Zeilenauswahl-Schleife im Dateimodus gefunden und mit eigenem Host-Repro bestätigt (hängt
      sich bei einer gültige-Zeile-losen `.txt`-Datei endlos auf), Fix dafür bewusst auf eine
      künftige Sitzung zurückgestellt, um den gelieferten Patch auf eine Änderung zu begrenzen.
      Restlicher Cluster weiterhin offen: `nfctools`, `uhf_rfid`, `nfc_comparator`,
      `simultaneous_rfid_reader`.
- [x] Restlichen NFC-Werkzeug-Cluster weiterverfolgt: **NFC Comparator**
      (`applications/external/nfc_comparator`, Quelle `acegoal07`) geprüft — mit rund 3400
      Zeilen über gut 60 Dateien vollständig gelesen, Karte-vs-Karte-/Karte-vs-Datei-Vergleich
      (UID/Protokoll/Byte-Diff über zehn Protokolle) plus Ordner-Suche nach der besten
      Übereinstimmung unter `/ext/nfc`, über die eingebaute Antenne. Alle zehn
      protokollspezifischen Diff-Implementierungen fehlerfrei. Fund in
      `nfc_comparator_finder_searcher_worker_task()`: `dir_walk_read()` schreibt den aktuellen
      Verzeichniseintrag unbedingt in das geteilte `nfc_card_path`-Feld, noch bevor geprüft ist,
      ob der Eintrag überhaupt eine `.nfc`-Datei ist bzw. lädt — folgt auf einen echten
      Teiltreffer ein solcher irrelevanter/nicht ladbarer Eintrag, zeigt die Digital-Finder-
      Suche am Ende dessen Dateinamen bei gleichzeitig korrektem Diff-Wert des tatsächlichen
      Treffers an (Fehlzuordnung Anzeigepfad↔Diffdaten). Mit Host-Repro (zwei Fehlerfälle plus
      Regressionsfall: bester Treffer ist der letzte untersuchte Eintrag) verifiziert — Port-
      Vorschlag inkl. Fix in `firmware-mining/nfc-comparator-app-port.md`. `uhf_rfid` und
      `simultaneous_rfid_reader` dabei endgültig als Fremdhardware-Fall (externes
      YRM100-/M6E-/M7E-UHF-Lesegerät) bestätigt und abgeschlossen. Restlicher Cluster nur noch
      `nfctools` (~11 200 Zeilen, größter verbleibender Kandidat).
- [ ] Restlichen NFC-Werkzeug-Cluster begonnen: **NFC Tools** (`applications/external/nfctools`,
      Autor `wakdev`, ~11 200 Zeilen) — in dieser Sitzung nur den NDEF-Kern
      (`core/nfc_tools_ndef.c`, 1080 Zeilen) vollständig geprüft. Fund: `ndef_fill_record()`
      extrahiert das vCard-Namensfeld im strukturierten Anzeigepfad mit unbegrenztem
      `strstr((const char*)payload, "FN:")` auf einem nicht NUL-terminierten, exakt auf die
      Tag-Datenlänge allozierten Heap-Puffer — bei fehlendem `"FN:"` liest die Funktion über das
      Allokationsende hinaus (Absturz-/Leak-Risiko), bei außerhalb gefundenem Muster unterläuft
      zusätzlich die Längenberechnung. Die Nachbarfunktion `ndef_decode_vcard()` in derselben
      Datei löst dasselbe Problem bereits korrekt mit einer auf `len` begrenzten Suche. Mit
      Host-Repro (ASan: `heap-buffer-overflow` exakt am Allokationsende) bestätigt — Port-
      Vorschlag inkl. Fix in `firmware-mining/nfctools-app-port.md`. Restlicher Umfang der App
      (Mifare-Classic-/DESFire-/ICODE-/NTAG-Lesepfade, ~35 Schreib-Szenen, Sektor-Analyse,
      Passwort-Entfernung, Tag-Formatierung/-Sperre — ~10 100 Zeilen) für künftige Sitzungen
      offen.
      Fortsetzung: `core/nfc_tools_mfc.c` (687 Zeilen, MAD-NDEF-Lese-/Schreibpfad, Format,
      Memory-Dump), `core/nfc_tools_ntag.c` (110 Zeilen, Format/Dump), `core/nfc_tools_app.c`
      (120 Zeilen, App-Lifecycle) sowie zur Kontextprüfung `scenes/read/nfc_tools_scene_read_tag.c`
      und `scenes/other/nfc_tools_scene_sector_analysis.c` vollständig gelesen — kein
      Speicherzugriffsfehler in diesen Dateien selbst, aber beim Nachverfolgen des DESFire-NDEF-Pfads
      (`nfc_tools_desfire_read_ndef()`, aus `nfc_tools_scene_read_tag.c` bei jedem DESFire-Scan
      automatisch aufgerufen) Fund in `core/nfc_tools_desfire.c` (1060 Zeilen, jetzt vollständig
      gelesen): `t4t_read_binary()` begrenzt die aus der R-APDU übernommene Antwortlänge (`dlen`)
      nirgends auf das vom Aufrufer angeforderte `length`/die tatsächliche Puffergröße — eine
      bösartige oder fehlerhafte ISO14443-4A-Karte (Proxmark3/ChameleonMini/zweiter Flipper mit
      Testfirmware) kann mit bis zu 256 Nutzdatenbytes antworten und damit sowohl feste
      Stack-Puffer (`cc[CC_SIZE]`=15 Bytes an vier Aufrufstellen, `old_len_raw`/`len_bytes`=2 Bytes)
      als auch den Heap-Puffer der NDEF-Nutzdaten (`ndef_buf`) überschreiben; erreichbar bereits beim
      automatischen NDEF-Lesepfad ohne Extra-Klick. Mit Host-Repro (ASan:
      `stack-buffer-overflow` exakt am Pufferende bei simulierter 200-Byte-Antwort auf eine
      15-Byte-Anfrage) bestätigt — Ein-Zeilen-Fix (Klammerung von `dlen` auf `length`) in derselben
      Funktion behebt alle sechs Aufrufstellen gleichzeitig, siehe
      `firmware-mining/nfctools-desfire-readbinary-oob-write.md`. Restlicher Umfang
      (`core/nfc_tools_icode.c`, ~35 Schreib-Szenen, Passwort-Entfernung) für künftige Sitzungen
      offen.
- [x] NFC-Werkzeug-Cluster weiterverfolgt: **NFC Tools** — `core/nfc_tools_icode.c` (635 Zeilen,
      ICODE/ISO15693-Format-/Dump-/NDEF-Schreibpfad) vollständig gelesen. Fund in
      `iso15693_write_ndef_blocks()`: das CC.MLEN-Feld (Kapazitätsangabe der kurzen NFC-Forum-
      Type-5-Tag-Capability-Container, ein Byte, max. 2040 Byte darstellbar) wird aus der vollen
      Tag-Kapazität als 32-Bit-Wert berechnet und danach ungeprüft auf `uint8_t` gecastet — bei
      Tags, deren Kapazität (`(block_count-1)*block_size`) über 2040 Byte liegt (spec-konform
      erreichbar, z. B. 256 Blöcke × 9+ Byte), läuft der Wert unbemerkt um und hinterlässt eine
      falsche CC, unabhängig von der Größe der tatsächlich geschriebenen NDEF-Nachricht selbst;
      die Funktion meldet dabei weiterhin Erfolg. Mit eigenständigem Arithmetik-Repro (mehrere
      Fälle inkl. zweier Regressionsfälle an/unter der Grenze) verifiziert — Port-Vorschlag inkl.
      Fix in `firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md`. Ein zunächst vermuteter
      zweiter Fund (Blockindex-Umlauf im Dump-Pfad bei `block_count > 256`) wurde beim Gegenprüfen
      gegen die eigene, nicht-erweiterte ISO15693-3-Blockadressierung (bereits als 256-Block-Limit
      in `firmware-mining/nfc-iso15693-read-multi-blocks-and-parser-overrun-fix.md` dokumentiert)
      als nicht erreichbar verworfen. Damit ist `core/nfc_tools_icode.c` vollständig geprüft;
      restlicher Cluster-Umfang (~35 Schreib-Szenen, Passwort-Entfernung) für künftige Sitzungen
      offen.
- [x] NFC-Werkzeug-Cluster weiterverfolgt: **NFC Tools** — "NFC Commands" (rohe APDU-Befehle an
      eine erkannte Karte senden, `scenes/other/nfc_tools_scene_nfc_commands_run.c` +
      `_input.c`, zusammen 363 Zeilen) vollständig gelesen, dazu zum Quervergleich erneut
      `nfc_tools_scene_set_password_write.c`, `_remove_password_write.c`, `nfc_tools_scene_format_tag.c`
      und `nfc_tools_scene_lock_tag.c`. Fund: "NFC Commands" prüft `app->detected_protocol` vor
      dem Poller-Start nirgends — anders als alle vier genannten Geschwister-Funktionen, die
      unpassende Protokolle explizit mit "Incompatible tag"/"Unsupported tag: %s" abfangen.
      Stattdessen entscheidet ein einziger Bool-Helfer (`protocol_is_iso4a()`) ohne Fehlerzweig,
      welcher der beiden Poller (ISO14443-3A/-4A) den Befehl bekommt, und kennt dabei nur 3 der 6
      echten ISO14443-4A-Familienmitglieder aus dem eigenen Firmware-Protokollbaum
      (`lib/nfc/protocols/nfc_protocol.c`): `Ntag4xx` (NTAG424 DNA u. ä.), `Type4Tag` und `Emv`
      (Bankkarten) fehlen und laufen über den falschen (3A-)Poller; echte Fremdprotokolle
      (Felica/ISO15693/SLIX/ST25TB/ISO14443-3B/-4B) erhalten keine Fehlermeldung statt eines
      klaren Abbruchs. Mit Host-Repro (Kontrollfluss-Vergleich über alle 15 `NfcProtocol`-Werte
      gegen die echte, aus `nfc_protocol.c` übernommene Baumstruktur) bestätigt — Port-Vorschlag
      inkl. Fix (Nutzung der bereits vorhandenen Firmware-API `nfc_protocol_has_parent()`) in
      `firmware-mining/nfctools-commands-missing-protocol-guard.md`. Restlicher Cluster-Umfang
      (~35 Schreib-Szenen, Passwort-Entfernungs-Input) für künftige Sitzungen offen.
- [x] NFC-Werkzeug-Cluster weiterverfolgt: Rückstellpunkt "Passwort-Entfernungs-Input"
      aufgegriffen — `nfc_tools_scene_remove_password_input.c`/`_set_password_input.c` (reine
      Tastatur-Eingabe-Wrapper, kein Fund) sowie beide zugehörigen Write-Szenen vollständig
      gelesen. Fund in `nfc_tools_remove_password_write_cb()`
      (`scenes/other/nfc_tools_scene_remove_password_write.c`): die beiden Schreibschritte
      (CONFIG-Seite deaktiviert den Passwortschutz komplett, PASSWORD-Seite setzt den
      Standardwert zurück) laufen in dieser Reihenfolge — bricht die RF-Verbindung nach dem
      CONFIG-, aber vor dem PASSWORD-Schritt ab (Tag zu früh vom Feld genommen), meldet die App
      "Password error" (Fehlschlag), obwohl der Passwortschutz auf dem Tag zu diesem Zeitpunkt
      bereits vollständig deaktiviert ist (AUTH0 = 0xFF wirkt unabhängig vom PASSWORD-Inhalt) —
      derselbe Fundtyp wie die bereits dokumentierten RollForge-OTG-/RollJam-B-Meldungswidersprüche,
      hier auf ein fremdes App-Feature angewendet. Geschwisterfunktion `set_password_write_cb()`
      von derselben Zweistufigkeit betroffen, aber unkritisch (Abbruch nach CONFIG lässt PASSWORD
      auf Standardwert stehen, ein zweiter Versuch mit der dort fest verdrahteten
      Standard-Authentifizierung bleibt möglich). Mit Host-Repro (Zustandsmodell aller drei
      Abbruchstellen, ALT vs. vorgeschlagene Reihenfolge) verifiziert: ALT widerspricht sich in
      1 von 3 Fällen, NEU in keinem — Port-Vorschlag inkl. Fix (Vertauschen der beiden
      Schreibaufrufe) in `firmware-mining/nfctools-remove-password-order-fix.md`. Restlicher
      Cluster-Umfang (~35 Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/etc.,
      Tag-Info-/QR-Code-/NDEF-Detail-Lesepfade) weiterhin für künftige Sitzungen offen.
- [x] NFC-Werkzeug-Cluster weiterverfolgt: den zentralen NDEF-Schreib-Worker geprüft, über den alle
      ~35 Schreib-Szenen am Ende laufen — `scenes/write/nfc_tools_scene_write_scan.c`
      (`nfc_tools_ndef_write_worker()`) sowie zum Quervergleich `core/nfc_tools_ndef.c`
      (`nfc_tools_ndef_build()`, alle NDEF-Typen) vollständig gelesen. Fund in der
      MFUL/NTAG-Kapazitätsprüfung: der CC-Lesevorgang (Seite 3) bekommt — anders als jeder der vier
      nachfolgenden Schreibpfade in derselben Funktion — keinen Wiederholungsversuch; schlägt er
      transient fehl, bleibt die Kapazität beim Fallback-Wert 48 Byte, der direkt im Anschluss als
      wäre er die echte, gemessene Kapazität in der Meldung "Tag too small!" ausgegeben wird — ein
      Kommunikationsfehler wird so als Kapazitätsurteil mit einer erfundenen Zahl gemeldet, derselbe
      Fundtyp wie der bereits dokumentierte "Remove Password"-Reihenfolgen-Fund. Mit Host-Repro
      (Zustandsmodell aller vier Fälle: transienter CC-Lesefehler, dauerhafter CC-Lesefehler, echte
      kleine Kapazität, ausreichende Kapazität) verifiziert: ALT meldet in 2 von 4 Fällen eine
      falsche Ursache, NEU (CC-Read mit demselben bereits vorhandenen Retry-Makro wie die
      Seiten-Writes) in keinem — Port-Vorschlag inkl. Fix in
      `firmware-mining/nfctools-write-scan-cc-read-no-retry-tag-too-small.md`. Restlicher
      Cluster-Umfang: `nfc_tools_scene_felica_write.c`/`_felica_write_input.c` (FeliCa-Schreibpfad),
      Tag-Info-/QR-Code-Detail-Lesepfade sowie `chameleon`/`nfc_relay` (Fremdgerät/Zweitgerät)
      weiterhin für künftige Sitzungen offen.
- [x] `uhf_rfid` erneut aufgegriffen: bei der NFC-Comparator-Sitzung nur als Fremdhardware-Fall
      (externes YRM100-Modul) bestätigt und ohne vollständigen Quelltext-Review abgeschlossen —
      diesmal alle 15 C-Dateien/beide Header-Verzeichnisse vollständig gelesen. Fund in
      `uhf_app.c::convertToHexString()` (durchläuft jede EPC-/TID-/USER-Bank-Anzeige): die
      Allokation reserviert `strlen(furi_str)` Byte ohne Platz für den Nullterminator und
      `memcpy` kopiert das `\0` nicht mit — ein Heap-Buffer-Overflow-Read bei jedem nachgelagerten
      Umgang mit dem Rückgabewert als C-String (`widget_add_string_multiline_element()`), tritt
      bei jedem erfolgreichen Read mit nicht-leerer Bank auf. Zusätzlicher Fund im selben
      Funktionskörper: bei Bank-Größe 0 gibt die Funktion ein String-Literal zurück, das beide
      Aufrufstellen anschließend per `free()` freigeben (Freigabe von Nicht-Heap-Speicher). Mit
      Host-Repro (vier isolierte Szenarien unter AddressSanitizer/UBSan: ALT/NEU × Normalfall/
      Leerfall) bestätigt: ALT bricht in beiden Fehlerfällen ab, NEU in keinem — Port-Vorschlag
      inkl. Fix in `firmware-mining/uhfrfid-hexstring-oob-read-fix.md`. Restlicher NFC-Werkzeug-
      Cluster nach wie vor nur `nfctools` (Fortsetzung laufend) sowie die bestätigten
      Fremdgerät-Fälle `chameleon`/`nfc_relay`.
- [x] NFC-Werkzeug-Cluster weiterverfolgt: Rückstellpunkt "Tag-Info-/QR-Code-/NDEF-Detail-
      Lesepfade" aufgegriffen — `nfc_tools_scene_tag_info.c`/`nfc_tools_scene_ndef_qr_code.c` ohne
      Fund, Fund im dahinterliegenden Encoder `helpers/qrcode/nfc_tools_qrcodegen.c`: Version 6/7
      sind bei ECC-L in Wirklichkeit zweiblöckig (Reed-Solomon), der Encoder behandelt sie
      fälschlich als einen einzigen Block und hängt eine zu kurze, falsch berechnete ECC-Folge an —
      jeder NDEF-Wert mit 107-127 Zeichen (der praktisch einzige erreichbare Fall, `value[128]`)
      wählt Version 6 und erzeugt dadurch einen strukturell falschen, mit einem echten QR-Decoder
      (`pyzbar`/`zbar`, nicht nur gegen sich selbst) verifiziert zu 100 % unlesbaren Code. Fix
      (korrigierte Datenkapazitätstabelle + Blockaufteilung/Interleaving) mit vollständigem
      Host-Repro-Sweep (137 Testfälle, Längen 1-140) verifiziert: ALT 31 Fehlschläge, NEU 0 —
      Port-Vorschlag inkl. Fix in `firmware-mining/nfctools-qr-code-v6-corrupt-fix.md`. Dabei einen
      zweiten, unabhängigen Fehler in der Version-7-Alignment-Pattern-Platzierung gefunden (nur 3
      von 6 nötigen Mustern gezeichnet) und bewusst nicht mitgefixt, da Version 7 über den
      einzigen Aufrufer ohnehin nie erreichbar ist und ein Teilfix diesen Pfad im eigenen Sweep
      nicht repariert hat — Version 7 stattdessen deaktiviert (Versionsauswahl auf v≤6 begrenzt),
      Version 6 allein deckt die volle erreichbare Werte-Kapazität bereits ab. Restlicher
      Cluster-Umfang (~35 Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/etc.) weiterhin für
      künftige Sitzungen offen.
- [x] NFC-Werkzeug-Cluster weiterverfolgt: Rückstellpunkt "~35 Schreib-Szenen für Kontakt/
      Bitcoin/WLAN/Social/etc." aufgegriffen — alle 34 Dateien unter `scenes/write/` vollständig
      gelesen (reine Tastatur-Eingabe-Wrapper ohne eigene Kodierlogik, kein Fund) sowie
      `core/nfc_tools_search.c`/`core/nfc_tools_social.c` (statische Präfix-/Suffix-Tabellen,
      kein Fund). Beim Nachverfolgen, wo die dort gesammelten Felder tatsächlich in NDEF-Payloads
      umgesetzt werden, Fund im gemeinsamen URL-Encoder-Helfer `url_encode()`
      (`core/nfc_tools_ndef.c:529`, genutzt von `NdefTypeBitcoin`s `message`-Feld und
      `NdefTypeSearch`s Suchbegriff): Off-by-eins in der Schleifenbedingung (`j + 4 < dest_size`
      statt `j + 4 <= dest_size`) verlangt ein Byte mehr Pufferreserve als nötig — bei den exakt
      worst-case-dimensionierten Zielpuffern beider Aufrufstellen (`raw_len * 3 + 1`) bricht die
      Kodierung dadurch vorzeitig ab und schneidet das Ende des Strings stillschweigend ab (bei
      kurzen/einzeichigen Eingaben bis auf einen leeren String), ohne Fehlermeldung. Mit
      Host-Repro (neun Fälle inkl. Regressionsanker gegen einen unabhängigen Referenz-Encoder)
      unter ASan/UBSan verifiziert — Port-Vorschlag inkl. Fix in
      `firmware-mining/nfctools-url-encode-off-by-one-truncation-fix.md`. Reiner
      Datenkorrektheitsfehler (Payload-Länge und -Inhalt bleiben konsistent, nur inhaltlich
      verstümmelt), keine Speichersicherheitsfrage. Damit ist der Schreib-Szenen-Rückstellpunkt
      abgeschlossen; restlicher Cluster-Umfang nur noch `chameleon`/`nfc_relay` (Fremdgerät/
      Zweitgerät, außerhalb reiner Quelltextprüfung).
- [x] Einen der drei aus der Prastel-Sitzung zurückgestellten SubGHz-Kandidaten aufgegriffen:
      **Security+ 2.0 Keypad-Unterstützung** — mein `secplus_v2.c` decodiert bisher nur das
      62-Bit-Halbpaket, das Handsender senden; LiftMaster-/Chamberlain-Funktastaturen desselben
      Protokolls senden stattdessen ein 86-Bit-Rahmenformat (eigener Vorlauf, breiteres
      Sync-Gap, 18- statt 10-Bit-Mischpuffer, zusätzliches 16-Bit-Datenfeld inkl. darin
      verpackter PIN, XOR-Prüfsumme über acht Nibbles gegen den Button-Wert) — dafür gibt es bei
      mir aktuell keine Erkennung, ein Keypad-Signal bleibt komplett unklassifiziert. Unleashed
      (`dev`, Commit `945234a`) hat dieselbe Datei genau um diese 86-Bit-Erkennung erweitert.
      Port-Vorschlag mit vollständiger Analyse (Struct-/Zustandsautomat-Anforderungen anhand
      exakter Zeilen meiner eigenen Basisdatei) in
      `firmware-mining/secplus-v2-keypad-frame-port.md`; den wiederverwendbaren, reinen
      Bit-Mathematik-Kern (Mix-/Order-Funktionen als eigenständige `_86`-Varianten statt Umbau
      der bestehenden, im 62-Bit-Pfad genutzten Funktionen, Halbpaket-Encode/Decode,
      PIN-Extraktion) 1:1 aus Unleashed übernommen und mit Host-Repro
      (`firmware-mining/repro-secplus-v2-keypad-frame/repro.c`, `gcc -O2 -Wall -Wextra
      -fsanitize=address,undefined`) verifiziert: Encode/Decode-Rundlauf über 200 000 zufällige
      Nutzlasten (0 Abweichungen), Mix-Invertierung/-Reihenfolge je Involution/Inverse für alle 9
      gültigen Werte, PIN-Rundlauf über den vollständigen 4-stelligen Bereich (0000-9999, 0
      Abweichungen), Präambel-Gate sowie Einzelbit-Verfälschung (5000 Versuche) korrekt
      verworfen bzw. nie unbemerkt akzeptiert. Die Zustandsautomat-Verdrahtung in `feed()`
      (86-Bit-Sync-Gap-Fenster, dynamische Bitzahl-Schwelle, 86-Bit-Präambelprüfung) liefere ich
      bewusst nicht als fertigen Patch, sondern als konkrete Schritt-für-Schritt-Skizze mit
      exakten Zeilenankern — kein „klein & sicher"-Fund, sondern ein mehrteiliger Feature-Port.
      Damit von den drei zurückgestellten Kandidaten noch offen: Nice O-Code (neues Protokoll),
      neue KeeLoq-Hersteller (Lernschlüssel, nicht quelltextlich verifizierbar).
- [x] Rückstellpunkt "uhf_rfid-Szenen-Code (reine UI-Ablaufsteuerung, nicht mitgeprüft)"
      aufgegriffen — alle 14 Szenen-C-Dateien vollständig gelesen. Fund: in 7 von 9
      Nicht-Wurzel-Szenen (`read_tag`, `write_tag`, `read_tag_success`, `write_tag_success`,
      `delete`, `saved_menu`, `settings`) wird `SceneManagerEventTypeBack` entweder gegen das
      falsche Struct-Feld geprüft (`event.event` statt `event.type`, in drei Szenen) oder gar
      nicht behandelt (in vier Szenen) — `on_event()` liefert dadurch für die physische
      Zurück-Taste `consumed = false`, was laut `view_dispatcher.c` unmittelbar
      `view_dispatcher_stop()` auslöst: die Taste beendet die gesamte App statt eine Ebene
      zurückzuspringen, u. a. auf den beiden zentralen Read-/Write-Tag-Bildschirmen. Gegen die
      korrekt implementierten Nachbarszenen (`device_info`, `tag_menu`) sowie die
      Input-Callbacks von Widget/Popup/Submenu/VariableItemList/TextInput im eigenen
      `mntm-012`-Baum verifiziert (nur Popup mit gesetztem Callback und TextInput fangen die
      Taste bereits ab, daher `save_success`/`delete_success`/`save_name` nicht betroffen;
      `module_info` ist Wurzelszene, dort ist App-Ende bei Zurück das erwartbare Verhalten).
      Mit Host-Repro (alle sieben `on_event()`-Funktionen ALT/NEU nachgebaut, gegen ein echtes
      Zurück-Ereignis sowie eine Regressionsmatrix aus allen zehn bereits gültigen
      Ereigniscodes je Szene geprüft) bestätigt — Port-Vorschlag inkl. Fix in
      `firmware-mining/uhfrfid-back-button-exits-app-fix.md`. Damit ist der komplette
      `uhf_rfid`-Quelltext (Core + Scenes) jetzt vollständig geprüft.
- [x] Quellsuche zum offenen Punkt "MIFARE Plus / Ultralight AES: schlüsselbasierte
      Authentifizierung fehlt komplett" (weiter oben als offen vermerkt) abgeschlossen:
      Unleashed (`dev`, Commit `945234a`) hat seit meinem `mntm-012`-Pin-Stand sowohl volle
      MIFARE-Plus-SL3-AES-Authentifizierung (Sektor-Keys, Secure Messaging, Admin-Keys,
      Konfigurationsblöcke, plus eine komplett neue Listener-/Emulationsseite) als auch
      MIFARE-Ultralight-AES/MF0AES20 (3-Pass-Auth, CMAC-Secure-Messaging) bekommen — grob
      3000+ Zeilen über ~25 Dateien, klar zu groß für einen einzelnen Patch. Vollständige
      Bestandsaufnahme (betroffene Dateien gestuft in sechs Phasen, Risiko, Port-Skizze) in
      `firmware-mining/mfplus-mfultralight-aes-auth-port.md`; den einzigen bereits jetzt
      eigenständig portierbaren Teil — die reinen AES-128-Kryptografie-Primitive beider
      Kartenfamilien (`mf_plus_crypto.c/.h`, `mf_ultralight_aes_crypto.c/.h`, 427 Zeilen) —
      1:1 übernommen und mit Host-Repro gegen drei unabhängige Referenzen verifiziert
      (FIPS-197-AES-Vektor, alle vier RFC-4493-CMAC-Vektoren, 4000 Fälle Differenzfuzz gegen
      OpenSSL `EVP_MAC`) in
      `firmware-mining/repro-mfplus-mfultralight-aes-crypto/repro.c`, `ALL CHECKS PASSED`
      unter ASan/UBSan. Zustandsautomat-Verdrahtung (Poller/Listener), Struct-Erweiterungen
      und UI bewusst nicht mitgeliefert — mehrteiliger Feature-Port, für künftige Sitzungen
      gestuft vorgemerkt (Phase 2-6 in der Port-Skizze).
- [x] MIFARE-Plus-/Ultralight-AES-Port **Phase 2** geliefert: `MfPlusData`-Struct-Erweiterung um
      die komplette SL3-Nutzlast (Blöcke, Sektor-/Admin-Schlüssel, Konfigurationsblöcke,
      Originalitätssignatur, alle "bekannt"-Bitmasken) plus deren vollständige Serialisierung
      (`mf_plus_sl3_data_save`/`_load`, Rückwärtskompatibilität zu Alt-Dumps ohne "Data format
      version"-Schlüssel, explizite Ablehnung eines unbekannten künftigen Formats) und zwölf
      neue Zugriffsfunktionen (`mf_plus_merge_update` inklusive) — reines Speicher-/
      Dateiformat, ohne Hardware nachrechenbar, MIFARE-Ultralight-AES bewusst auf eine eigene
      Sitzung verschoben. Als Unified-Diffs gegen meine `mntm-012`-Basis in
      `firmware-mining/mfplus-phase2-sl3-struct-serialization/` (`patch --dry-run` gegen frische
      Basiskopien vorab verifiziert), mit zehn Host-Fällen (Geometrie, Masken, Reset, Copy/
      Is-Equal, voller Save→Load-Rundlauf inkl. Basiskarten-UID, Nicht-SL3-Rundlauf, Legacy-Dump
      ohne Formatversion, Ablehnung einer künftigen Formatversion, Merge-Update, korruptes Feld)
      unter ASan/UBSan verifiziert (`ALL CHECKS PASSED`) in
      `firmware-mining/repro-mfplus-phase2-sl3-struct-serialization/repro.c` — Details inkl.
      Testhinweis in `firmware-mining/mfplus-phase2-sl3-struct-serialization.md`. Damit von den
      sechs Phasen der ursprünglichen Skizze zwei erledigt (Krypto-Kern, Struct/Serialisierung);
      Phase 3 (Poller-Zustandsautomat) braucht echte Hardware zum Testen und bleibt offen.
- [x] Bei der letzten Sitzung bewusst verschobenen Rest aufgegriffen: MIFARE-Ultralight-AES-Port
      **Phase 2** geliefert (analog zur bereits gelieferten MIFARE-Plus-SL3-Phase-2) —
      `MfUltralightData` bekommt die optionale 48-Byte-UL-AES-Originalitätssignatur
      (`aes_signature`/`aes_signature_present`) plus deren Serialisierung, einen neuen
      Kartentyp-Eintrag `MfUltralightTypeUltralightAES` (60 Seiten,
      `ReadVersion|Authenticate|ReadCounter|IncCounter`) sowie `mf_ultralight_aes_get_key()`
      (liest den wiederhergestellten DataProtKey aus den vorhandenen Kartenseiten, mit
      Byte-Umkehrung) — reines Speicher-/Dateiformat, ohne Hardware nachrechenbar. Dabei
      Unleasheds eigene Ladelogik für die neue Signatur korrigiert: ihr direktes
      `aes_signature_present = flipper_format_read_hex(...)` ohne vorheriges Nullen hätte bei
      mir (kein Zero-Init in `mf_ultralight_alloc()`) für jede Nicht-UL-AES-Karte einen
      nicht-deterministischen `mf_ultralight_is_equal()` ergeben (Allocator-Altinhalt bleibt im
      Puffer stehen, obwohl das Presence-Flag korrekt `false` wird) — bei mir stattdessen der
      Puffer vor dem optionalen Lesevorgang immer genullt. Bewusst nicht mitgezogen (siehe
      Port-Dokument): Unleasheds zeitgleiche, unabhängige NTAG210/212-Typ-Erkennungs-
      Umstellung, die gelockerte `detect_protocol()`-ATQA-Prüfung, `is_pwd_pack_read()`
      sowie die UL-AES-Schreibbereichs-Anpassung (gehört zum künftigen Schreibfluss). Als
      Unified-Diffs gegen meine `G4MEOVER-FW`-Basis in
      `firmware-mining/mfultralight-aes-phase2-struct-serialization/` (`patch --dry-run` vorab
      gegen frische Basiskopien verifiziert, real angewendet und Byte-für-Byte gegen die
      host-getestete Fassung abgeglichen), mit acht Host-Testgruppen (Reset, Copy, Rundlauf
      mit/ohne Signatur, ALT-vs-NEU-Nachweis der oben beschriebenen Korrektur, Schlüssel-
      Byte-Umkehrung inkl. FIPS-197-Referenzschlüssel, Typ-Verdrahtung, Regression eines
      unveränderten Kartentyps) unter ASan/UBSan verifiziert (`ALL CHECKS PASSED`, keine
      LeakSanitizer-Funde) in
      `firmware-mining/repro-mfultralight-aes-phase2-struct-serialization/repro.c` — Details
      inkl. Testhinweis in `firmware-mining/mfultralight-aes-phase2-struct-serialization.md`.
      Damit haben beide Kartenfamilien aus der ursprünglichen Skizze ihre Phase 2 erledigt;
      Phase 3 (Poller-Zustandsautomaten für beide Familien) braucht echte Hardware und bleibt
      offen, ebenso die für UL-AES oben gelisteten, unabhängigen Unleashed-Funde.
- [x] Einen der vier bei der letzten UL-AES-Phase-2-Sitzung zurückgestellten, unabhängigen
      Unleashed-Funde aufgegriffen: neuer Helfer `mf_ultralight_is_pwd_pack_read()` — meldet,
      ob die echten PWD-/PACK-Seiten erfasst wurden (`pages_read >= pwd_page + 2`), im
      Unterschied zu den maskierten Nullseiten, die mein bereits vorhandener
      `try_default_pass()`-Poller-Handler nach einem fehlgeschlagenen Default-Passwort-Versuch
      per `pages_read -= 2` zurückrechnet. Rein additiv, keine bestehende Signatur geändert,
      kein Aufrufer eingebaut (genau wie bei Unleashed zu diesem Diff-Zeitpunkt selbst). Mit
      Host-Repro gegen vier Seitenzahl-Zustände über zwei unabhängige Kartentypen (UL11,
      NTAG213) sowie einer direkten Gegenprobe zur bestehenden, unveränderten
      `mf_ultralight_is_all_data_read()` verifiziert: deren Inhaltsheuristik
      (`pass != 0 || pack != 0`) liest ein reales `PWD=0/PACK=0` fälschlich als "nicht
      vollständig gelesen", der neue Helfer (nur `pages_read`, unabhängig vom Seiteninhalt)
      korrekt — als eigenständiger Robustheitsvorteil dokumentiert, das Umstellen von
      `is_all_data_read()` selbst aber bewusst nicht mitgeliefert (eigenständige
      Verhaltensänderung an bestehendem, benutztem Code, eigene Sitzung wert). Dabei einen
      eigenen, unabhängigen Fund in der wiederverwendeten Host-Test-Infrastruktur der letzten
      Sitzung gemacht und in der eigenen Stub-Kopie behoben: `FURI_PACKED` war dort nicht
      definiert, wodurch `MfUltralightConfigPages` auf dem Host ungepackt (falsche Größe/
      Feldoffsets) statt wie in der echten Firmware gepackt kompiliert wurde — für die bereits
      gelieferten Ports folgenlos (deren Tests fassen dieses Struct nicht an), für die neue,
      feldgenaue Testgruppe aber notwendig. Als Unified-Diffs gegen meine unveränderte
      `main`-Basis in `firmware-mining/mf-ultralight-is-pwd-pack-read-helper/`
      (`mf_ultralight.h.patch`, `mf_ultralight.c.patch` — `patch --dry-run -p1` gegen frische
      Basiskopien verifiziert, real angewendet und Byte-für-Byte gegen die host-getestete
      Fassung abgeglichen), mit vier Host-Testgruppen unter ASan/UBSan (`ALL CHECKS PASSED`)
      in `firmware-mining/repro-mf-ultralight-is-pwd-pack-read/repro.c` — Details inkl.
      Testhinweis in `firmware-mining/mf-ultralight-is-pwd-pack-read-helper.md`. Von den vier
      ursprünglich zurückgestellten UL-Funden sind damit zwei offen: Typ-Erkennungs-Umbau
      (NTAG210/212), gelockerte `detect_protocol()`-ATQA-Prüfung sowie die
      `get_write_end_page()`-Schreibbereichs-Anpassung (gehört zum künftigen Schreibfluss).
- [x] **MIFARE Ultralight/NTAG: SAK/ATQA-Gate vor `mf_ultralight_poller_detect()`s Sonden-Read +
      `detect_protocol()`-ATQA-Lockerung** — beim genaueren Nachgehen des beim UL-AES-Phase-2-Port
      zurückgestellten `detect_protocol()`-Funds stellt sich heraus, dass Unleashed diese Funktion
      inzwischen als Gate vor dem bislang bedingungslosen Sonden-`READ` in
      `mf_ultralight_poller_detect()` einbaut (Fund per sparse `git clone` des `dev`-Branchs, da
      der Aufruf über reinen Rohdatei-Zugriff auf vermutete Dateipfade nicht zu finden war). Ohne
      dieses Gate liest meine Basis bei jedem ISO14443-3A-Tag im `Ready`-Zustand unauthentifiziert
      Seite 0 und stuft einen magischen MIFARE-Classic-Klon, der diesen `READ` beantwortet, dabei
      fälschlich als Ultralight ein, bevor die Classic-Erkennung überhaupt zum Zug kommt. Beide
      zusammengehörigen Änderungen geliefert: das Gate selbst sowie die bereits notierte
      ATQA-Low-Byte-Lockerung in `detect_protocol()` (ohne sie würde das neue Gate die
      4-Byte-UID-Ultralight-Klone aussperren, die meine ungegatete Basis bisher zufällig weiterhin
      erkannt hat). Als Unified-Diffs gegen meine unveränderte `main`-Basis in
      `firmware-mining/mfultralight-detect-protocol-atqa-gate/` (`detect_protocol_c.patch`,
      `poller_gate.patch` — `patch --dry-run -p1` gegen frische Basiskopien verifiziert, real
      angewendet und Byte-für-Byte gegen die host-getestete Fassung abgeglichen), mit fünf
      Host-Testgruppen unter ASan/UBSan (`ALL CHECKS PASSED`, inkl. Kontrollfluss-Modell für die
      auf dem Host nicht kompilierbare Poller-Funktion und Gegenprobe gegen die reale, ungepatchte
      Basis-Zeile) in `firmware-mining/repro-mfultralight-detect-protocol-atqa-gate/repro.c` —
      Details inkl. Testhinweis in `firmware-mining/mfultralight-detect-protocol-atqa-gate.md`.
      Von den vier ursprünglich zurückgestellten UL-Funden ist damit nur noch der
      Typ-Erkennungs-Umbau (NTAG210/212) offen; die `get_write_end_page()`-Schreibbereichs-
      Anpassung gehört zum künftigen Schreibfluss und wurde separat vorgemerkt.
- [x] Letzten der vier beim UL-AES-Phase-2-Port zurückgestellten, unabhängigen Unleashed-Funde
      aufgegriffen: **NTAG210/212-Typ-Erkennung + echter `Unknown`-Fallback** in
      `mf_ultralight_get_type_by_version()`. Bei mir kollidiert `storage_size` bei zwei Werten
      zwischen zwei Kartenfamilien (`0x0B`/`0x00`: MF0UL11 vs. NTAG210; `0x0E`: MF0UL21 vs.
      NTAG212) — meine Basis kennt jeweils nur die Ultralight-Seite, eine echte NTAG210/212-Karte
      wird also als UL11/UL21 mit falschen EV1-Zähler-Fähigkeiten fehlerkannt. Zusätzlich bleibt
      bei einem GetVersion-antwortenden, aber nicht modellierten IC (unbekannter Klon, künftiger
      Chip) `type` bei mir am Anfangswert `MfUltralightTypeOrigin` stehen — verwechselt mit "kein
      GetVersion, Original-Ultralight". Unleashed löst beides in einem Umbau: `switch` über
      `storage_size` mit den beiden Kollisionsfällen zusätzlich über das `prod_type`-Familien-
      Nibble unterschieden (`0x03` Ultralight, `0x04` NTAG; jeder andere `storage_size`-Wert bleibt
      unverändert allein über `storage_size` bestimmt), plus echtem `default: return
      MfUltralightTypeUnknown;` statt stillem Nicht-Setzen — dieselbe Ersetzung auch im
      NTAG-I2C-Zweig bei unbekanntem `minor`/`storage_size`. Drei neue, rein additive
      Feature-Tabellen-Einträge (NTAG210: Layout wie UL11 ohne dessen EV1-Zähler; NTAG212: Layout
      wie UL21 inkl. Dynamic Lock; Unknown: exakt derselbe `feature_set`, den der alte implizite
      Origin-Fallback schon hatte). Dabei geprüft und als eigenständiges, von diesem Patch
      geerbtes (nicht neu eingeführtes) Risiko dokumentiert: `mf_ultralight_get_write_end_page()`s
      `furi_assert()`-Positivliste für den Schreib-auf-Blanko-Ablauf führt NTAG210/212 weder bei
      mir noch im aktuellen Unleashed-Stand — nach diesem Patch würde ein Schreibversuch auf einen
      korrekt als NTAG210/212 erkannten Dump dort abstürzen, während er vor dem Patch (mit falschem
      Kartentyp UL11/UL21) durchlief. Als Unified-Diffs gegen meine unveränderte `main`-Basis in
      `firmware-mining/mfultralight-ntag210-212-type-detect/` (`mf_ultralight_h.patch`,
      `mf_ultralight_c.patch` — `patch --dry-run -p1` gegen frische Basiskopien verifiziert, real
      angewendet und Byte-für-Byte gegen die host-getestete Fassung abgeglichen), mit sieben
      Host-Testgruppen unter ASan/UBSan (`ALL CHECKS PASSED`, inkl. ALT-vs-NEU-Vergleich gegen
      eine unveränderte Kopie meiner bisherigen Funktion und Feature-Tabellen-Prüfung) in
      `firmware-mining/repro-mfultralight-ntag210-212-type-detect/repro.c` — dieselbe Testdatei
      kompiliert gegen die unveränderte Basis mangels der neuen Enum-Werte gar nicht erst, ein noch
      direkterer Beleg als ein reiner Laufzeit-Fehlschlag. Details, Risiko-Begründung inkl.
      Patch-Reihenfolge-Hinweis (falls der separate UL-AES-Phase-2-Vorschlag ebenfalls lokal
      angewendet werden soll — beide fügen ihre neuen Enum-Werte an derselben Stelle ein) und
      Testhinweis in `firmware-mining/mfultralight-ntag210-212-type-detect.md`. Damit sind alle
      vier ursprünglich beim UL-AES-Phase-2-Port zurückgestellten Funde abgearbeitet.
- [x] Official-dev seit `a55e3939` gegengeprüft: nur drei neue, sicherheitsirrelevante Commits
      (Lizenz-Check-Deaktivierung im PVS-Tooling, PR-Template, ein Include-Tippfehler in
      `furi_hal_bt.c`, eine Doku-Ergänzung) — kein Portkandidat. Unleashed dafür zwischenzeitlich
      um **unlshd-093** ergänzt (55 neue Commits seit `unlshd-092`); daraus `cc2d85278`
      (PR #1105, "SubGhz: guard final upload buffer write") geprüft — betrifft
      `subghz_protocol_blocks_get_upload_from_bit_array()` in `lib/subghz/blocks/encoder.c`,
      bei mir byteidentisch zum Unleashed-Vorher-Stand vorhanden und aktuell von zwei
      Protokoll-Encodern genutzt (`bin_raw.c`, `chamberlain_code.c`). Zwei voneinander
      unabhängige Off-by-eins-Schreibzugriffe hinter das Ende des vom Aufrufer exakt
      `max_size_upload` Elemente großen `upload[]`-Arrays gefunden: eine zu lasche
      Schleifen-Prüfung (`size_upload > max_size_upload` statt `>=`) sowie ein völlig
      ungeprüfter abschließender Flush-Schreibzugriff nach der Schleife, der bei genau
      `max_size_upload` gesammelten Einträgen still einen weiteren, nicht mehr gültigen Eintrag
      schreibt, ohne dass je ein `furi_crash()` greift. Bei `bin_raw.c` (Protokoll "BinRAW", für
      nicht klassifizierte SubGHz-Aufnahmen, `upload[]` mit 2560 Elementen) mit einer
      geladenen `.sub`-Datei erreichbar, deren Bitmuster genügend Pegelwechsel enthält — keine
      Sendehardware nötig. Mit Host-Repro (ALT/NEU-Vergleich der beiden real kompilierten
      Funktionen, fork-isolierte Testfälle je Grenzfall, `furi_crash()` auf einen
      unterscheidbaren Exit-Code gestubbt) unter ASan/UBSan verifiziert: ALT löst in beiden
      Fällen exakt an der erwarteten Zeile einen `heap-buffer-overflow`-Schreibzugriff aus, NEU
      löst stattdessen sauber den (bestehenden bzw. neu ergänzten) Guard aus, ein realistischer
      24-Bit-Regressionsfall bleibt bei ALT und NEU byte-für-byte identisch — Port-Vorschlag
      (1:1 Unleasheds Fix übernommen) in
      `firmware-mining/subghz-encoder-upload-buffer-overflow.md`. Aus derselben
      `unlshd-093`-Reihe für künftige Sitzungen vorgemerkt: `51a56fc40` (NFC: reject oversized
      FeliCa Lite dumps, #1106), `4b3c614c8` (F7: reject sentinel for expansion callbacks,
      #1108), `c742dfe6c` (HID: bound stealth jiggler movement, #1111), `cc3a1bc79` (NFC: CUID
      dictionary pass end marker, #1115 — verwandt zum bereits gelieferten
      `nfc-unified-key-dictionary.md`-Port), `95c35fb82` (NFC: MIFARE-Classic-Schlüssel in
      Nutzer-Dictionary speichern, #1118 — Feature), `104d96f86` (SubGHz: Superrollo/GW60,
      KeeLoq HCS361, #1068 — neues Protokoll, eigener Portaufwand).
- [x] Aus derselben `unlshd-093`-Restliste weiterverfolgt: `4b3c614c8` (PR #1108, "F7: reject
      sentinel for expansion callbacks") geprüft — `furi_hal_serial_control_set_expansion_callback()`
      in `targets/f7/furi_hal/furi_hal_serial_control.c` prüft `serial_id <= FuriHalSerialIdMax`
      statt `<`, lässt also den reinen Sentinel-Wert `FuriHalSerialIdMax` selbst durch; der
      nachgelagerte Handler indiziert damit ungeprüft `handles[FuriHalSerialIdMax]`, ein Element
      hinter dem nur `FuriHalSerialIdMax` Elemente großen `handles[]`-Array. Da dieses Array nur das
      erste Feld einer größeren Struct ist, führt der Zugriff zu keinem Fault, sondern überschreibt
      still das direkt danach liegende Feld `queue` — den `FuriMessageQueue*`, von dem der
      Serial-Control-Worker-Thread selbst abhängt. Copy-Paste-Fehler aus der Nachbarfunktion
      (Logging-ID-Prüfung, dieselbe Datei), die ihr eigenes `<=` korrekt behält, da dort
      `FuriHalSerialIdMax` als "Logging deaktivieren"-Sentinel tatsächlich gültig ist — der
      Expansion-Callback-Setter hat keine solche Semantik, deaktiviert wird nur über
      `callback == NULL`. Bei mir (`mntm-012`) Datei byteidentisch zum Unleashed-Vorher-Stand
      verifiziert. Erreichbarkeit eingeordnet: die vier Aufrufstellen im eigenen `expansion`-Dienst
      übergeben ausschließlich bereits geprüfte IDs, die Lücke ist nur über die öffentliche
      HAL-Funktion selbst erreichbar (API-Vertragsfehler bei Aufrufer-Fehlgebrauch, z. B. in einer
      eigenen Erweiterungsport-App). Mit Host-Repro (rekonstruierte Struct-Feldreihenfolge inkl.
      `offsetof`-Beleg, dass `queue` lückenlos auf `handles[]` folgt; fork-isolierte ALT/NEU-Aufrufe)
      unter ASan/UBSan verifiziert: ALT lässt den Sentinel durch und korrumpiert `queue` sichtbar,
      NEU weist ihn per `furi_check` ab, bevor `queue` je berührt wird, zwei Regressionsfälle (echte
      IDs 0/1) unverändert in beiden Varianten — Port-Vorschlag inkl. 1:1 übernommenem Fix (`<=` →
      `<`) in `firmware-mining/expansion-callback-sentinel-oob-fix.md`, Patch gegen meine
      unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab verifiziert, real angewendet und
      Byte-für-Byte abgeglichen) in
      `firmware-mining/expansion-callback-sentinel-oob-fix/furi_hal_serial_control_c.patch`, Repro in
      `firmware-mining/repro-expansion-callback-sentinel-oob-fix/repro.c`. Aus derselben
      `unlshd-093`-Restliste weiterhin offen: FeliCa-Lite-Dateilade-Bounds (#1106),
      Stealth-Jiggler-Bounds (#1111), CUID-Dictionary-Zähler (#1115),
      MIFARE-Classic-Schlüssel-Dictionary-Feature (#1118), Superrollo/GW60-SubGHz-Protokoll (#1068).
- [x] Aus derselben `unlshd-093`-Restliste weiterverfolgt: `51a56fc40` (PR #1106, "NFC: reject
      oversized FeliCa Lite dumps") geprüft — `felica_load()` in
      `lib/nfc/protocols/felica/felica.c`, Zweig `FelicaLite`, liest `"Blocks total"`/`"Blocks
      read"` als `uint32_t` direkt aus der `.nfc`-Datei und weist sie ungeprüft (nach Cast auf
      `uint8_t`, wirksamer Maximalwert 255) `data->blocks_total`/`data->blocks_read` zu; die
      anschließende Ladeschleife schreibt `data->blocks_total` × `sizeof(FelicaBlock)` (18 Byte)
      in `data->data.dump[]`, ein nur 504 Byte großes Feld (`FELICA_BLOCKS_TOTAL_COUNT` = 28
      Blöcke) innerhalb der heap-allokierten, insgesamt 544 Byte großen `FelicaData`
      (`malloc()` in `felica_alloc()`) — bei `blocks_total` = 255 ein 4590-Byte-Schreibversuch,
      weit über das Ende der gesamten Allokation hinaus. Bei mir (`mntm-012`) bis auf die
      unabhängige `felica_data_format_version`-Differenz (2 statt 3) byteidentisch zum
      Unleashed-Vorher-Stand vorhanden — kein Kartenkontakt nötig, allein das Öffnen einer
      präparierten `.nfc`-Datei mit `Device type: FeliCa` löst die Überschreibung aus, gleiches
      Bedrohungsmodell wie beim bereits portierten EMV-`.nfc`-Loader-Fund. Mit Host-Repro
      (nachgebildete `FelicaData`-Feldreihenfolge/-Größen, heap-alloziert wie in
      `felica_alloc()`, fork-isolierte ALT/NEU-Fälle; da ASan hier standardmäßig ohne
      `abort_on_error=1` per `exit(1)` statt per Signal beendet, zählt sowohl Signal als auch ein
      von Null verschiedener Exitcode als erkannter Fund — direkt gegengeprüft) unter ASan/UBSan
      verifiziert: ALT überschreibt bei `blocks_total` = 255 die gesamte Allokation über ihr Ende
      hinaus (separat mit vollem `stderr` bestätigt: Schreibzugriff exakt "0 bytes after
      544-byte region"), NEU (1:1 Unleasheds Grenzprüfung übernommen) lehnt den Ladevorgang vorher
      kontrolliert ab, ein realistischer 28-Blöcke-Regressionsfall bleibt bei ALT und NEU
      identisch — Port-Vorschlag in `firmware-mining/felica-lite-blocks-total-oob-fix.md`, Patch
      gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab verifiziert, real
      angewendet und Byte-für-Byte abgeglichen) in
      `firmware-mining/felica-lite-blocks-total-oob-fix/felica_c.patch`, Repro in
      `firmware-mining/repro-felica-lite-blocks-total-oob-write/repro.c`. Aus derselben
      `unlshd-093`-Restliste weiterhin offen: Stealth-Jiggler-Bounds (#1111),
      CUID-Dictionary-Zähler (#1115), MIFARE-Classic-Schlüssel-Dictionary-Feature (#1118),
      Superrollo/GW60-SubGHz-Protokoll (#1068).
- [x] Aus derselben `unlshd-093`-Restliste weiterverfolgt: `c742dfe6c` (PR #1111, "HID: bound
      stealth jiggler movement") geprüft — `hid_mouse_jiggler_stealth_timer_callback()` in
      `applications/system/hid_app/views/hid_mouse_jiggler_stealth.c` berechnet `move_x`/`move_y`
      als `int` im Bereich `-1000`..`1000`, übergibt sie aber an
      `hid_hal_mouse_move(Hid*, int8_t dx, int8_t dy)` — eine wertebereichsverletzende, implizite
      Konvertierung an der Aufrufgrenze, die auf dem realen GCC/ARM-Pfad still auf die unteren
      8 Bit kappt statt zu clampen. Bei mir (`mntm-012`) in den relevanten Zeilen byteidentisch
      zum Unleashed-Vorher-Stand (umgebende Datei weicht an anderen, für diesen Fund irrelevanten
      Stellen ab — ältere UI-Feinheiten, die bei mir fehlen; die drei betroffenen Zeilen gesondert
      byteweise gegengeprüft: identisch). Rein funktionaler Fund (kein Speicherfehler, der
      `int8_t`-Parameter bleibt über seinen vollen Wertebereich gültig) — läuft aber dem Zweck der
      "Stealth"-Variante zuwider: die tatsächlich gesendete Bewegung kann durch die Kappung jeden
      Wert im vollen `int8_t`-Bereich annehmen, unabhängig vom beabsichtigten Wert. Mit Host-Repro
      (verbatim ALT-/NEU-Ausdrücke aus beiden Quellzuständen, reale
      `hid_hal_mouse_move`-Parametersignatur als Kappungsgrenze, vollständige Domänenüberdeckung
      beider Fassungen statt Stichprobe) unter ASan/UBSan verifiziert: ALT weicht bei 1745 von
      2001 möglichen Zügen (87,2 %, analytisch hergeleitet und exakt gegengeprüft) vom
      beabsichtigten Wert ab (Beispiele: beabsichtigt +1000 kommt als -24 an, -1000 als +24, +900
      als -124), NEU weicht bei keinem der 255 möglichen Züge ab — der Fix beseitigt den Fund
      vollständig, verkleinert ihn nicht nur. Zum Vergleich den normalen (nicht-stealth)
      Mouse-Jiggler geprüft: verwendet bereits eine feste, `int8_t`-taugliche Schrittweite
      (`MOUSE_MOVE_SHORT`), vom selben Fund nicht betroffen — Port-Vorschlag in
      `firmware-mining/hid-jiggler-stealth-move-bounds-fix.md`, Patch (1:1 Unleasheds Fix
      übernommen) gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab
      verifiziert, real angewendet und Byte-für-Byte abgeglichen) in
      `firmware-mining/hid-jiggler-stealth-move-bounds-fix/hid_mouse_jiggler_stealth_c.patch`,
      Repro in `firmware-mining/repro-hid-jiggler-stealth-move-bounds-fix/repro.c`. Aus derselben
      `unlshd-093`-Restliste weiterhin offen: CUID-Dictionary-Zähler (#1115),
      MIFARE-Classic-Schlüssel-Dictionary-Feature (#1118), Superrollo/GW60-SubGHz-Protokoll
      (#1068).
- [x] Aus derselben `unlshd-093`-Restliste weiterverfolgt: `cc3a1bc79` (PR #1115, "NFC: keep the
      CUID dictionary pass end marker out of the sector counter") geprüft — bei mir existiert
      Unleasheds betroffene Datei (`mf_classic_extra_scenes.c`) nicht; meine NFC-App hat eine
      eigene, älter strukturierte MF-Classic-Dict-Attack-Szene
      (`applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c`) mit eigenem,
      Bitmap-basiertem CUID-Dictionary-Ansatz — kein 1:1-Diff übernehmbar. Die zugrunde liegende
      Fehlerklasse aber unabhängig gefunden und exakt identisch: `current_key_idx / 2` (Anzahl
      durchprobierter Schlüssel A/B je Sektor) läuft im `NextSector`-Handler eine Sektornummer
      über die letzte echte hinaus, sobald der CUID-Pass zu Ende ist — dieser Marker-Wert, den der
      Poller (`mf_classic_poller_handler_next_sector()` in
      `lib/nfc/protocols/mf_classic/mf_classic_poller.c`) als Ende-Signal zurückliest, wurde bei
      mir ungeprüft auch in das reine Anzeigefeld `current_sector` übernommen (z. B. Anzeige
      "16" auf einer 1K-Karte mit nur 16 gültigen Sektoren 0–15 — exakt der von Unleashed im
      PR-Titel genannte Fall). Zusätzlich setzt `nfc_scene_mf_classic_dict_attack_prepare_view()`
      bei jedem der drei Dictionary-Phasenwechsel (CUID → User → System) `dict_keys_total`/
      `dict_keys_current` zurück, nirgends aber `current_sector` — ein Reset existiert bei mir nur
      beim Verlassen der gesamten Szene (`on_exit`), sodass der Marker-Wert bis zum ersten echten
      Sektorwechsel der nächsten Phase sichtbar stehen bleibt. Kein Speicherfehler (`current_sector`
      ist `uint8_t`, der Marker-Wert bleibt für jeden Kartentyp im gültigen `uint8_t`-Bereich),
      rein ein UI-Korrektheitsfehler, reproduzierbar bei jedem vollständigen CUID-Dictionary-Lauf
      über eine eigene, bereits gespeicherte CUID-Dictionary-Datei. Mit Host-Repro (verbatim
      ALT-/NEU-Ausdrücke, reale Feldtypen aus `nfc_app_i.h`, vollständiger Sweep über alle drei
      echten MIFARE-Classic-Sektorzahlen — Mini 5, 1K 16, 4K 40 — statt nur der 1K-Stichprobe aus
      dem Commit-Titel, plus separater Phasenübergangs-Check) unter ASan/UBSan verifiziert: ALT
      erreicht bei allen drei Kartentypen den ungültigen Marker-Wert und lässt ihn beim
      simulierten Phasenwechsel unverändert stehen, NEU bleibt im gesamten Sweep aller drei
      Kartentypen durchgehend im gültigen Bereich und setzt beim Phasenwechsel korrekt auf 0
      zurück; ein Regressionsblock bestätigt, dass der unveränderte Nicht-CUID-Zweig von der
      Änderung unberührt bleibt — `ALL CHECKS PASSED` (344 Checks), keine ASan-/UBSan-Meldungen,
      identisches Ergebnis unter `-O0` gegengeprüft. Eigener Fix nach Unleasheds Prinzip (Marker
      weiterhin unbedingt ans Poller-Event publizieren, aber nur bei `next_sector <
      sectors_total` in die Anzeige übernehmen; Anzeige-Sektor bei jedem Phasenwechsel auf 0
      zurücksetzen), an meine tatsächliche Codestruktur angepasst — Port-Vorschlag in
      `firmware-mining/mf-classic-cuid-dict-sector-cursor-fix.md`, Patch gegen meine unveränderte
      `mntm-012`-Basis (`patch --dry-run -p1` vorab verifiziert, real angewendet und
      Byte-für-Byte abgeglichen) in
      `firmware-mining/mf-classic-cuid-dict-sector-cursor-fix/nfc_scene_mf_classic_dict_attack_c.patch`,
      Repro in `firmware-mining/repro-mf-classic-cuid-dict-sector-cursor-fix/repro.c`. Aus
      derselben `unlshd-093`-Restliste weiterhin offen: MIFARE-Classic-Schlüssel-
      Dictionary-Feature (#1118), Superrollo/GW60-SubGHz-Protokoll (#1068).
- [x] Aus derselben `unlshd-093`-Restliste weiterverfolgt: `95c35fb82` (PR #1118, "NFC: Save
      recovered MIFARE Classic keys to the user dictionary") geprüft und portiert — im
      Unleashed-Original auf die eigene, generische `nfc_key_dict.c/.h`-Infrastruktur aus PR
      #1073 aufgesetzt, die ich bei mir nicht habe (eigener Port-Vorschlag dafür,
      `firmware-mining/nfc-unified-key-dictionary.md`, bislang nur dokumentiert, nicht
      angewendet, und selbst dort schon auf Classic/Ultralight-C beschränkt, da mir
      `MfPlusKey`/`MfUltralightAesKey` fehlen). Feature stattdessen direkt gegen meine
      unveränderte MIFARE-Classic-Szenenstruktur verdrahtet: neuer Menüpunkt "Save Keys to
      Dictionary" neben "Show Keys" im Read- und Saved-Menü (nur sichtbar bei
      `key_a_mask | key_b_mask != 0`), neue Szene sammelt gefundene Schlüssel maskenbasiert
      (deckt CUID-Dictionary, Nested, MFKey32, Handeingabe und Dump-Import über einen
      gemeinsamen Pfad ab), filtert in je einem Durchlauf gegen System- und Nutzer-Dictionary
      und hängt neue Schlüssel an — schließt insbesondere die CUID-Sackgasse (per-UID-Schlüssel
      einer statisch-verschlüsselten Karte überleben jetzt einen Klon-auf-Magic-Tag-Zyklus).
      Bonus-Fund beim Nachvollziehen: `keys_dict_add_key_str()` fügt bei mir (byteidentisch zum
      Unleashed-Vorher-Stand) jeden neuen Schlüssel per `stream_insert_string()` ein statt ihn
      anzuhängen, obwohl zuvor ans Dateiende gesprungen wird — kopiert dabei unnötig die gesamte
      Datei in eine Scratch-Datei und zurück, quadratisch für einen Aufrufer, der mehrere
      Schlüssel in einer Schleife schreibt (MFKey32, jetzt zusätzlich dieser Import mit bis zu 80
      Schlüsseln); von Unleashed im selben PR behoben (`stream_write_string()`), 1:1 übernommen.
      Mit Host-Repro (drei Kernalgorithmen nahezu wörtlich extrahiert, Array-Nachbau der
      `KeysDict`-Iterationsschnittstelle statt echter Dateien, sieben Testgruppen: Masken- statt
      Byte-basiertes Sammeln inkl. Dedup, echter Allnull-Schlüssel vs. nicht gefundener
      Allnull-Slot, voller 4K-Sweep exakt an der 80er-Obergrenze, Überlauf-Schutz, Abgleich
      gegen zwei getrennte Dictionaries, leeres Dictionary, voller Zahlenabgleich
      Kandidaten=Neu+Bekannt) unter ASan/UBSan verifiziert: 14 von 14 Checks bestanden. Als drei
      Unified-Diffs gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab
      verifiziert, real angewendet und Byte-für-Byte abgeglichen) plus der vollständigen neuen
      Szenendatei in `firmware-mining/mf-classic-save-keys-to-dict-port/`, Repro in
      `firmware-mining/repro-mf-classic-save-keys-to-dict/repro.c`, Details in
      `firmware-mining/mf-classic-save-keys-to-dict-port.md`. Aus derselben
      `unlshd-093`-Restliste weiterhin offen: Superrollo/GW60-SubGHz-Protokoll (#1068, neues
      Protokoll, eigener Portaufwand) — damit letzter noch offener Punkt dieser Restliste.
- [x] Letzten offenen Punkt der `unlshd-093`-Restliste geliefert: `104d96f86` (PR #1068,
      "SubGHz: Add Superrollo (GW60) roller-shutter protocol (KeeLoq HCS361)") geprüft und
      portiert. Kein neues Protokollmodul — regulärer HCS361-KeeLoq mit eigenem Rahmen
      (asymmetrische Te/2Te-Präambel, 10×Te-Sync, 67-Bit-Frame = 64 Nutzdaten-Bit + VLOW +
      2-Bit-CRC statt der üblichen 64+2 Statusbits), ausschließlich in
      `lib/subghz/protocols/keeloq.c` plus drei kleinen App-Dateien für den neuen "Add
      Manually"-Menüpunkt. Drei der vier Dateien
      (`subghz_custom_event.h`/`subghz_gen_info.c`/`subghz_scene_set_type.c`) bei mir an der
      betroffenen Stelle byteidentisch zum Unleashed-Vorher-Stand, Unleasheds Original-Hunks 1:1
      anwendbar; `keeloq.c` dagegen strukturell abweichend (eigener Stand ohne Unleasheds
      `bypass`/ältere `ProgMode`-Zwischenschritte, andere Funktionsnamen, `te_delta=140` statt
      `180`) — kein 1:1-Diff möglich, dieselbe zugrunde liegende Idee (asymmetrische
      Präambel-Erkennung im Decoder, 10×Te-Sync-Übergang, Toleranzfenster
      `min_count_bit_for_found+2`→`+3`, CRC-Framing im Encoder) 1:1 aus Unleashed übernommen,
      aber an meine eigene Zustandsautomat-/Encoder-Struktur angepasst. Mit Host-Repro (reale
      Decoder-Zustandsmaschine samt Konstanten nachgebaut, CRC-/Framing-Logik aus dem
      Encoder-Zweig fast wörtlich extrahiert, vier Testgruppen: CRC-Werteabdeckung über 200
      Zufallsnutzlasten, 500 synthetische Superrollo-Frames ALT-vs-NEU inkl. Bit-Korrektheit,
      500 Standard-KeeLoq-Frames als Regressionstest) unter ASan/UBSan verifiziert: ALT erkennt
      0/500 Superrollo-Frames, NEU 500/500 mit durchgehend korrekten 64 Nutzdaten-Bit, 0/500
      Abweichungen bei bestehenden KeeLoq-Herstellern — ALL CHECKS PASSED (1206 Checks). Als
      vier Unified-Diffs gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab
      verifiziert, real angewendet und Byte-für-Byte abgeglichen) in
      `firmware-mining/superrollo-gw60-keeloq-port/`, Repro in
      `firmware-mining/repro-superrollo-gw60-keeloq-port/repro.c`, Details inkl. Hinweis zum
      separat zu prüfenden Normal-Learning-Schlüssel (Unleasheds `keeloq_mfcodes`-Ressourcendiff
      bewusst nicht mitübernommen, eigenes Format) in
      `firmware-mining/superrollo-gw60-keeloq-port.md`. Damit ist die gesamte
      `unlshd-093`-Restliste (Expansion-Callback-Sentinel #1108, FeliCa-Lite-Bounds #1106,
      Stealth-Jiggler-Bounds #1111, CUID-Dictionary-Zähler #1115,
      MIFARE-Classic-Schlüssel-Dictionary-Feature #1118, Superrollo/GW60 #1068) abgearbeitet.
- [x] Unleashed `dev` seit `unlshd-093` weiterverfolgt (noch kein neuer Tag): **MFC-Poller
      `mf_classic_poller_free()` liest uninitialisierten Speicher als Zeiger**, wenn der Poller
      noch im `DetectType`-State (vor dem ersten `Start`-Durchlauf, der `mode_ctx` erst nullt)
      freigegeben wird — `nfc_poller_list_free()` ruft `free()` unbedingt, unabhängig vom
      internen State. Bei mir strukturell identisch vorhanden (`mf_classic_poller_alloc()` nutzt
      `malloc()`, kein `calloc()`). Quelle: Unleashed-Commits `093d77e5` (PR #4362, "Fix BusFault
      in Write to Initial Card") + Nachbesserung `de0e3cf3` (PR #4400, DictAttackCUID im Gate
      ergänzt), ursprünglich aus der Official-Firmware übernommen. Port fügt ein neues
      `instance->mode`-Feld hinzu (getrennt vom bereits vorhandenen, nur intern für
      CUID-Unterscheidung genutzten `dict_attack_ctx.mode`) und geht dabei einen Schritt über
      Unleasheds eigenen Fix hinaus: `mf_classic_poller_alloc()` initialisiert das neue Feld
      explizit auf `MfClassicPollerModeRead`, was Unleashed selbst nach beiden Commits noch nicht
      tut (dort bliebe das neue Feld bei einer Freigabe vor `Start` seinerseits uninitialisiert).
      Mit Host-Repro (reale Struct-Layouts inkl. `offsetof`-Verifikation nachgebaut,
      Tracking-Allokator statt echtem `free()` für deterministische Invalid-Free-Erkennung
      unabhängig vom Heap-Layout, drei Testgruppen: 200 Poison-Bytemuster für den
      `DetectType`-Freigabefall — ALT 199/199 mit Invalid-Free, NEU 0/200 —, Regressionstest
      einer echten Dict-Attack-Session, alle drei Dict-Attack-Modi vs. Read/Write) unter
      ASan/UBSan verifiziert: ALL CHECKS PASSED. Als zwei Unified-Diffs gegen meine unveränderte
      `mntm-012`-Basis (`patch --dry-run -p1` vorab verifiziert, real angewendet und
      Byte-für-Byte abgeglichen) in
      `firmware-mining/mf-classic-poller-free-before-start-uninit-pointer-fix/`, Repro in
      `firmware-mining/repro-mf-classic-poller-free-before-start-uninit-pointer-fix/repro.c`,
      Details in `firmware-mining/mf-classic-poller-free-before-start-uninit-pointer-fix.md`.
- [x] Unleashed-Commit `6bf5e4f` (PR #4430, ab Tag `unlshd-090`): **CLI-Quotierung** —
      `args_read_probably_quoted_string_and_trim()` in `lib/toolbox/args.c` prüft eine fehlende
      schließende Anführung falsch (`second_quote_pos == 0` statt `== FURI_STRING_FAILURE`,
      Letzteres ist `(size_t)-1` und kann als Ergebnis dieser Suche nie `0` sein) — meldet bei mir
      dadurch fälschlich Erfolg statt Fehler und lässt `args` unkonsumiert stehen (betrifft
      `loader_cli.c`/`infrared_cli.c`/`lfrfid_cli.c`, alle drei nutzen die Funktion bereits
      unverändert). Kein Speicherfehler (per Quellstudium von `mlib`/`m-string.h` sowie Host-Repro
      verifiziert: `m_string_set_n()` clamped über `M_MIN`, `m_string_right(args, 0)` ist ein
      No-op). Zusätzlich nutzen bei mir vier `subghz`-CLI-Befehle
      (`decode_raw`/`tx_from_file`/`encrypt_keeloq`/`encrypt_raw`) weiterhin die unquoted Variante
      und schneiden einen in Anführung übergebenen Pfad mit Leerzeichen am ersten Leerzeichen ab;
      `loader open` ließ Anführungszeichen wörtlich im Argument-String der gestarteten App stehen.
      Vier Unified-Diffs (`args_c.patch`, `loader_cli_c.patch`, `subghz_cli_c.patch`,
      `args_test_c.patch`) gegen meine unveränderte `mntm-012`-Basis, `patch --dry-run -p1` vorab
      verifiziert, real angewendet und Byte-für-Byte abgeglichen, in
      `firmware-mining/cli-quoted-path-handling-loader-subghz-fix/`. Host-Repro (mlib-Primitive
      `search_char`/`set_n`/`right` semantiktreu nachgebaut, drei Testgruppen: Regression
      ALT==NEU, ALT meldet bei 3/3 Fällen fehlender Anführung fälschlich Erfolg vs. NEU 0/3,
      unquoted- vs. quoted-`subghz`-Aufruf) unter ASan/UBSan: ALLE CHECKS BESTANDEN, in
      `firmware-mining/repro-cli-quoted-path-handling-loader-subghz-fix/repro.c`, Details in
      `firmware-mining/cli-quoted-path-handling-loader-subghz-fix.md`.
- [x] Unleashed `dev` seit dem zuletzt geprüften Stand (`c74f6b1d5`, iButton-Write-Target-Fund,
      bewusst zurückgestellt) gegengeprüft: zwei neue Commits, `8fdb70dd3` (PR #1112, "HID:
      improve Mouse Jiggler timers and Bluetooth controls") und `9effa4b44` (PR #1149, "RFID:
      enter cards by facility code and card number, read HID formats and Casi-Rusco badges").
      Letzterer ist mit 1131 Zeilen über 18 Dateien (neue LFRFID-Manual-Entry-/HID-Format-
      Infrastruktur) für den hier üblichen Sitzungsumfang erneut zu groß — für eine künftige,
      eigens dafür vorgesehene Sitzung vorgemerkt, wie schon `c74f6b1d5`. Aus `8fdb70dd3`
      (bündelt mehrere, teils voneinander abhängige Jiggler-Fixes über vier Dateien) den einen
      unabhängig portierbaren Teilfund herausgelöst: die Registrierungsreihenfolge von
      BLE-Advertising-Start und Verbindungsstatus-Callback in `hid_ble_app()`
      (`applications/system/hid_app/hid.c`) — bei mir (`mntm-012`) byteidentisch zum
      Unleashed-Vorher-Stand. Eine bereits gekoppelte Gegenstelle kann sich verbinden, bevor der
      Callback registriert ist; das Verbindungsereignis geht dann verloren, und das
      Verbindungssymbol jeder BLE-HID-Ansicht (inkl. beider Mouse-Jiggler-Varianten, die es
      bereits unabhängig von diesem Fund anzeigen) bleibt bis zum nächsten unabhängigen
      Statuswechsel fälschlich auf "getrennt" stehen. Mit Host-Repro (Bt-Service-Callback-Slot
      und Advertising-Start-Ereignis minimal nachgebaut, drei Gruppen: kein Race ohne bereits
      gekoppelte Gegenstelle, Ereignis geht bei ALT verloren/kommt bei NEU an, Regression
      späterer gewöhnlicher Verbindungswechsel) unter ASan/UBSan verifiziert: ALLE CHECKS
      BESTANDEN. Zwei-Zeilen-Patch (reine Vertauschung, kein neuer Code) gegen meine
      unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab verifiziert, real angewendet und
      Byte-für-Byte abgeglichen — identisch) in
      `firmware-mining/hid-ble-status-callback-registered-after-advertising-fix/hid_c.patch`,
      Host-Repro in
      `firmware-mining/repro-hid-ble-status-callback-registered-after-advertising/repro.c`,
      Details in `firmware-mining/hid-ble-status-callback-registered-after-advertising-fix.md`.
      Restlicher Umfang von `8fdb70dd3` (Stop stoppt den Jiggler-Timer nicht wirklich, Verlassen
      der Ansicht setzt `running` nicht zurück, Stealth-Neubewaffnung-Race, Stealth-Bewegung kann
      0 sein, Jiggler wartet über BLE nicht auf die Verbindung vor dem Senden) bewusst
      zurückgestellt — baut sinnvoll auf dem bereits vorgeschlagenen, bei mir noch nicht
      angewendeten `hid-jiggler-stealth-move-bounds-fix.md` auf, für eine künftige Sitzung
      vorgemerkt.

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
- [x] **usb-army-penetrator** Marauder-Modul (`src/Attacks/Marauder/Marauder.cpp`)
      geprüft: das Ein-Slot-Kommando-Postfach (`nextMarauderCommandToRun`/
      `marauderActivated`) zwischen `ESP32Marauder::run()` und `::loop()` hat
      zwei Probleme — überschreibt kommentarlos einen noch nicht verarbeiteten
      Befehl, wenn vor dem nächsten `loop()`-Durchlauf ein zweiter `run()`-
      Aufruf eintrifft (betrifft konkret den Übertemperatur-Notstopp
      `run("stopscan")` aus `EvilCrowWind::loop()`, der so gegen einen
      zeitgleichen Web-UI-/Ducky-`MARAUDER`-Befehl verlieren kann), UND wird
      ohne jede Synchronisierung sowohl aus dem Arduino-`loop()`-Task als auch
      aus dem eigenständigen AsyncTCP-Task (`/marauder`-Webhandler, siehe
      `platformio.ini` `CONFIG_ASYNC_TCP_RUNNING_CORE=1`) gelesen/geschrieben —
      unsynchronisierter `std::string`-Zugriff über zwei FreeRTOS-Tasks hinweg,
      mit Host-Repro unter ThreadSanitizer als Data-Race auf beiden Statics
      bestätigt (Kommandoverlust zusätzlich deterministisch ohne Threading
      demonstriert). Fix (kritischer Abschnitt via `portMUX_TYPE` um alle drei
      Zugriffsstellen) in
      `app-improvements/usb-army-penetrator/marauder-command-mailbox-race-and-loss-fix.md`.
      Restlicher `usb-army-penetrator`-Code (WiFi/BLE/USB-HID) weiterhin
      ungeprüft.
- [x] **usb-army-penetrator** Serial-Agent-Modul (`src/Attacks/Agent/Agent.cpp`)
      geprüft: das Poll-Intervall für die `RequestAgentStatus`-Sonde
      (`agentPollingIntervalInSec`) ist als **`bool`** deklariert statt als
      Sekunden-Ganzzahl, obwohl das zugehörige Setting als `Int16` registriert,
      per `getInt()` gelesen (Default 15) und in `loop()` mit `* 1000` zu einem
      Millisekunden-Fenster verrechnet wird. Die `bool`-Zuweisung faltet jeden
      Sekundenwert auf 0/1: Default und alle Nutzerwerte ≠ 0 (30/60/5 …)
      kollabieren einheitlich auf 1000 ms (Poll alle 1 s statt N s, Setting
      wirkungslos), der Wert 0 ergibt ein 0-ms-Fenster (Dauerpoll in jedem
      `loop()`-Durchlauf) — mit Host-Repro (Modell aus Deklaration, `getInt()`
      und `Intervall*1000`, Assertions) verifiziert. Fix (Typkorrektur der
      einen Deklaration auf `int16_t`) in
      `app-improvements/usb-army-penetrator/agent-polling-interval-bool-type-fix.md`.
      Restlicher `usb-army-penetrator`-Code (WiFi/BLE/USB-HID) weiterhin
      ungeprüft.
- [x] **usb-army-penetrator** WiFi-Modul (`src/Devices/WiFi/HardwareWiFi.cpp`)
      und dessen Aufrufer (`EvilCrowWind::loop()`, `Extensions.h`
      `WIFI_OFF`/`WIFI_ON`) geprüft: Zustandsmaschine und
      `desiredWiFiState`-Zugriffe laufen ausschließlich im Arduino-Haupt-`loop()`
      (`main.cpp`: `Board.loop()` vor `WiFi.loop()` vor `Ducky.loop()`), keine
      der Marauder-Race-Kategorie entsprechende Cross-Task-Schreibstelle
      gefunden. Beim Weiterverfolgen des davon genutzten Pcap-Capture-Pfads
      (`src/Utilities/PcapBuffer.cpp`, aufgerufen aus
      `src/Devices/USB/USBNCM.cpp`, naheliegender Wiederverwendungspunkt für
      eine künftige WiFi-/BLE-Aufzeichnung) Fund: `PcapBuffer::append()`
      übernimmt jede Paketlänge ungeprüft, obwohl der im Konstruktor
      geschriebene Pcap-Header `MAX_PACKET_LEN` (2324) als Snaplen zusagt —
      ein Paket `len > MAX_PACKET_LEN` erzeugt einen `incl_len`, der die
      eigene Snaplen-Zusage verletzt (Format-Verstoß, kein Speicherfehler),
      und `orig_len` trägt bei Kappung fälschlich denselben Wert wie
      `incl_len` statt der echten Ursprungslänge — mit Host-Repro (Nachbau
      von Header-/Record-Schreiblogik plus Rückparser) für `len = 5000`
      verifiziert. Fix (Clamp auf `MAX_PACKET_LEN` plus separates
      `origLength`-Feld) in
      `app-improvements/usb-army-penetrator/pcapbuffer-snaplen-truncation-fix.md`.
      Restlicher `usb-army-penetrator`-Code (Ethernet CH9120/W5500, TFT/Touch,
      USB-HID-Detailpfade) weiterhin ungeprüft.
- [x] **usb-army-penetrator** USB-HID-Detailpfad (`src/Devices/USB/USBHID.cpp`,
      `src/Attacks/Ducky/DuckyPayload.cpp`) geprüft: `USBHID::IsQueueEmpty()`
      prüft nur die Tastatur-Queue (`eventsToProcess`), nicht die beiden
      anderen HID-Queues (`cdcEventsToProcess` für Medientasten,
      `mouseEventsToProcess` für Maus) derselben Klasse. Einziger Aufrufer
      ist das Yield-Gate in `DuckyPayload::loop()` ("warte, bis alle HID-
      Events verarbeitet sind, bevor die nächste Skriptzeile läuft") — für
      Skripte, die nur `MOUSE_JIGGLE`/`MOUSE_MOVE` oder Medientasten-Zeilen
      nutzen (Extensions.h), bleibt das Gate dauerhaft blind, der Interpreter
      rückt pro Tick eine Zeile vor unabhängig vom Rückstand, während
      `USBHID::loop()` nur 1 Report/Tick sendet — bei einer Folge von
      `MOUSE_JIGGLE`-Zeilen ohne `DELAY` (typisches Anti-Idle-Jiggler-Muster,
      auch über `REPEAT` erzeugbar) wächst der Rückstand in
      `mouseEventsToProcess` linear mit der Skriptlänge, ohne Schranke —
      mit Host-Repro (Modell beider `loop()`-Gates) als linear wachsenden
      vs. auf ≤2 begrenzten Rückstand verifiziert. Fix (eine Zeile,
      `IsQueueEmpty()` prüft alle drei Queues) in
      `app-improvements/usb-army-penetrator/hid-queue-empty-mouse-cdc-blindspot-fix.md`.
      Restlicher `usb-army-penetrator`-Code (Ethernet CH9120/W5500,
      TFT/Touch, WiFi/BLE-Detailpfade) weiterhin ungeprüft.
- [x] **usb-army-penetrator** Storage-Schicht (`src/Devices/Storage/HardwareStorage.cpp`,
      `readLineFromFile()`) geprüft, nachdem `Ethernet` (CH9120/W5500) sich als nirgends
      instanziierter, unfertiger Feature-Stub herausstellte (kein `Devices::Ethernet` verdrahtet,
      `writeData()` ohne `return` — kein sinnvolles Fix-Ziel) und `TFT::convertStringToColor()`s
      Hex-Zweig aktuell an keiner einzigen Aufrufstelle mit echtem Nutzereingabe-String erreicht
      wird (nur hartkodierte Named-Colors). Fund: `readStringUntil('\n')` (Arduino-Core)
      konsumiert den Zeilentrenner, hängt ihn aber nie an den zurückgegebenen String an — die
      beiden bestehenden Sonderfälle (`ret == "\n"`/`ret == "\r\n"`) sind daher toter Code, der
      nie greift, während der reale Fall (CRLF-Quelldatei hinterlässt ein trailing `'\r'' auf
      **jeder** Zeile, nicht nur auf Leerzeilen) unbehandelt bleibt — betrifft jede von SD
      geladene, Windows-zeilenendenkodierte `.dss`-Payload (`AUTORUN.dss` und hochgeladene
      Skripte), mit Host-Repro (Buffer-Nachbau von `readStringUntil`) als sichtbares,
      unsichtbar mitgetipptes `\r` in `STRING`-Argumenten verifiziert — Fix in
      `app-improvements/usb-army-penetrator/readlinefromfile-crlf-trailing-cr-fix.md`.
      Restlicher `usb-army-penetrator`-Code (WiFi/BLE-Detailpfade, restlicher Ducky-Extensions-
      Parsing-Code) sowie die beiden identifizierten, aber bewusst zurückgestellten Stubs
      (Ethernet CH9120/W5500 unverdrahtet, TFT-Hex-Farbparser unerreicht) weiterhin offen.
- [x] **ProtoPirate** Storage-Schicht (`helpers/protopirate_storage.c`,
      `protopirate_storage_copy_key_2()`) geprüft, nachdem die letzte Sitzung den
      `pp_parse_hex_u64_strict`-Fix in derselben Datei geliefert hatte. Fund: das Feld
      `"Key_2"` wird bei mir protokollabhängig in vier verschiedenen Formen geschrieben
      (Ford V1: Hex/8 Byte, PSA: String, Kia V6/Honda V1: u32, Chrysler V0: Hex/2 Byte), die
      Kopierfunktion beim Speichern eines Captures (Verlauf UND manuelles Save, beide über
      `protopirate_storage_write_capture_data()`) probiert davon aber nur drei Formen durch
      (Hex/8, String, u32) — der Chrysler-V0-Fall (Hex/2) fällt durch alle drei Versuche,
      ohne dass das als Fehler erkannt wird (jeder Versuch behandelt "Feld nicht in dieser
      Form vorhanden" korrekt als fehlerfreien Fall für ein optionales Feld), und `"Key_2"`
      fehlt danach komplett in der gespeicherten Datei. Trifft direkt
      `subghz_protocol_encoder_chrysler_v0_deserialize()` (`protocols/chrysler_v0.c:370-373`),
      die `"Key_2"` beim Laden zwingend verlangt und ohne das Feld sofort mit
      `SubGhzProtocolStatusError` abbricht — jedes über mich gespeicherte Chrysler-V0-Capture
      ist danach nicht mehr ladbar/emulierbar, obwohl der Empfang selbst korrekt war. Mit
      Host-Repro (getyptes Feld-Modell für alle vier real vorkommenden `"Key_2"`-Formen)
      verifiziert: ALT verliert `Key_2` ausschließlich im Chrysler-V0-Fall, die anderen drei
      Formate sind bereits heute korrekt. Fix (ein zusätzlicher Hex/2-Fallback-Versuch, nach
      demselben Muster wie der bereits vorhandene `"Key_3"`-Hex-oder-u32-Fallback in
      derselben Datei) in `app-improvements/ProtoPirate/key2-hex2-lost-on-save-fix.md`.
      Restlicher `ProtoPirate`-Code (übrige Protokoll-Decoder/-Encoder, PSA-Bruteforce-Kernel
      in `protocols/psa_crypto_bf.c`/`psa_bf_core.c` — reverse-engineerte Krypto-Konstanten,
      ohne verifizierte Referenzimplementierung nicht risikofrei zu bewerten) weiterhin
      ungeprüft.
- [x] **ProtoPirate** `helpers/raw_file_reader.c` (RAW-Decode-Feature, aus einer früheren Sitzung
      als offener `raw_file_reader.c`-Rückstellpunkt vorgemerkt) geprüft: `raw_file_reader_load_chunk()`
      fordert pro Chunk pauschal `RAW_READER_BUFFER_SIZE` (512) Werte an, ohne zu wissen, wie
      viele Werte die aktuelle `RAW_Data:`-Zeile tatsächlich enthält — `flipper_format_read_int32()`
      liest aber strikt zeilengebunden und scheitert komplett, sobald eine Zeile vor Erreichen der
      angeforderten Menge endet. Dass `RAW_READER_BUFFER_SIZE` zufällig exakt
      `SUBGHZ_DOWNLOAD_MAX_SIZE` (512) der Stock-Firmware entspricht, verdeckt den Bug nur für
      Aufnahmen aus der eigenen "Read RAW"-Funktion; jede andere, laut Formatdoku ebenso gültige
      Zeilenaufteilung (kleinere Zeilen, z. B. eine gezielt gebaute Testdatei oder eine Aufnahme
      eines anderen Tools) lässt "Sub Decode" bereits beim allerersten Chunk abbrechen und
      fälschlich "No ProtoPirate protocol detected" melden, ohne dass auch nur ein Sample
      verarbeitet wurde. Mit Host-Repro (Modell des zeilengebundenen Lese-/Skip-Verhaltens von
      `flipper_format_read_int32()`, vier Zeilenaufteilungen) verifiziert: ALT liest nur die
      Stock-kompatible Aufteilung vollständig, NEU (Chunk-Größe an die tatsächliche nächste
      Zeilenlänge gekoppelt, per neuem Peek-Helfer auf der bereits vorhandenen
      Zeilen-Zähl-Funktion) liest alle vier Fälle vollständig — Fix in
      `app-improvements/ProtoPirate/raw-file-reader-chunk-line-boundary-fix.md`.
- [x] **FlipperJammer** (bislang als "restliche eigene Apps offen" vorgemerkt):
      `jammer_hw_detect()` (`jammer_app.c`) geprüft — der 5V-OTG-Boost bleibt in den
      beiden vom Code selbst benannten Standardfällen (externer CC1101 gefunden;
      NRF24-Multiboard braucht 5V) dauerhaft an, auch nach dem Beenden der App: die
      vier Modul-eigenen OTG-Ownership-Flags (`nrf24_jam.c`/`ble_jam.c`/
      `subghz_jam.c`/`scene_nrf24_diag.c`) übernehmen kein OTG, das sie bereits
      eingeschaltet vorfinden, und `jammer_hw_detect()` selbst hinterlässt kein
      App-weites Flag dafür — `jammer_app_free()` hat gar keinen OTG-Aufräumcode.
      Batterie-Drain-Bug, mit Host-Repro (Ownership-Zustandsmodell) verifiziert —
      Fix (neues `hw_detect_left_otg_on`-Flag + Cleanup in `jammer_app_free()`) in
      `app-improvements/FlipperJammer/hw-detect-otg-leak-after-exit-fix.md`.
      `lora-ukfe` (eigene App, bislang nur `rf-counter-restart-lockout-fix.md`)
      weiterhin offen.
- [x] **usb-army-penetrator** Ducky-Extensions-Restcode (`src/Attacks/Ducky/Extensions.h`,
      bislang als "restlicher Ducky-Extensions-Parsing-Code" vorgemerkt) geprüft: die
      dynamische Konstante für `#_FILE_INDEX_FILE_NAME_` prüft vor dem Indizieren von
      `curListOfFiles[curFileIndexVariable]` nur, ob die Dateiliste nicht leer ist — nicht,
      ob `curFileIndexVariable` selbst innerhalb der Liste liegt. `curFileIndexVariable`
      wird aus der frei beschreibbaren Skriptvariable `$FILE_INDEX` gesetzt; erkennt
      `FILE_INDEX_VALID()` einen außerhalb liegenden Wert (u. a. der reguläre Abbruchfall
      jeder `LOAD_DS_FILES_FROM_SD()`-Zählschleife, sobald `$FILE_INDEX` die Listengröße
      erreicht) korrekt als ungültig, bleibt `curFileIndexVariable` trotzdem auf diesem
      Wert stehen. Da der zugrunde liegende `DuckScriptInterpreter`
      (`DuckyInterpreter::getLineAndProcess()`) jede registrierte dynamische Konstante bei
      *jeder* Skriptzeile aufruft, unabhängig davon, ob die Zeile
      `#_FILE_INDEX_FILE_NAME_` referenziert, greift `operator[]` (undefiniertes
      Verhalten bei ungültigem Index) ab diesem Punkt bei jeder weiteren Zeile des
      laufenden Skripts daneben — Absturz oder ein aus Heap-Nachbarspeicher gelesener
      String, der über die normale Konstantenersetzung in Skriptzeilen landen kann, mit
      Host-Repro (Nachbau der Closure plus der Interpreter-Aufrufstelle) für den
      Schleifenende- und den Negativ-Index-Fall verifiziert — Fix (Indexgrenzenprüfung
      direkt in der Closure) in
      `app-improvements/usb-army-penetrator/file-index-filename-const-oob-read-fix.md`.
      Restlicher `usb-army-penetrator`-Code (Ethernet CH9120/W5500 unverdrahteter Stub,
      TFT-Hex-Farbparser unerreicht, WiFi/BLE-Detailpfade) weiterhin offen.
- [x] **ProtoPirate** `protocols/protocol_items.c` (Timing-Tuner-Referenztabelle,
      `protocol_timings[]`/`protopirate_get_protocol_timing()`) geprüft, nachdem der letzte
      `ProtoPirate`-Fund (RAW-Decode-Chunkgröße) im selben Verzeichnisbaum lag: der
      Tabelleneintrag für das VAG-Protokoll trägt den Namen `"VW"`, der real registrierte
      Decoder-Name ist aber `VAG_PROTOCOL_NAME` = `"VAG"` (`protocols/vag.h`/`vag.c`) — weder
      `strcmp` noch `strstr` noch eine der vier Alias-Regeln stellt die Verbindung her, die
      Suche liefert für `"VAG"` durchgehend `NULL`, obwohl die korrekten Referenzwerte
      (500/1000µs Manchester, 80 Bit) bereits unter dem falschen Namen in der Tabelle stehen.
      `timing_tuner_rx_callback()` (`scenes/protopirate_scene_timing_tuner.c`) schaltet bei
      `NULL` den gesamten Referenzvergleich (Soll/Ist, Toleranzbewertung, Short/Long/Tolerance-
      Anzeige) für den gematchten Capture ab — für VAG-Signale ist damit die Kernfunktion des
      Timing Tuners unbenutzbar, alle 23 anderen real registrierten Decoder-Namen lösen bereits
      korrekt auf. Mit Host-Repro (Tabellensuche 1:1 nachgebaut, alle 24 real registrierten
      Namen einzeln gegen ALT- und NEU-Tabelle geprüft) verifiziert — Fix (Namenskorrektur des
      einen Tabelleneintrags auf `VAG_PROTOCOL_NAME`) in
      `app-improvements/ProtoPirate/timing-tuner-vag-name-mismatch-fix.md`. Dabei auch
      festgestellt: `"Mitsubishi V0"` hat gar keinen Tabelleneintrag (fehlende Zeile statt
      Namens-Divergenz) — bewusst nicht mitgefixt, für künftige Sitzung vorgemerkt, sobald
      verifizierte Referenz-Timings für Mitsubishi V0 vorliegen. Restlicher `ProtoPirate`-Code
      (einzelne Protokoll-Decoder/-Encoder unter `protocols/` außer den bereits geprüften
      Dateien, `scenes/`, `views/`, PSA-Bruteforce-Kernel weiterhin bewusst zurückgestellt)
      weiterhin ungeprüft.
- [x] **lora-ukfe** (bislang kaum geprüft, letzter Fund war der RF-Counter-Persistenz-Bug):
      nach der Binär-RX-Migration (`0dba410a`, JSON-Zeilenparser → `ukfe_rf`-Frame-Scanner)
      `handle_rf_response()` in `uart_comm.c` gegen alle in `rf/ukfe_rf.h` definierten
      Response-Typen abgeglichen — `UkfeRfRespPayload` (0x84, Payload-Listeneinträge) hat
      keinen eigenen `case`, fällt in den generischen `default`-Log-Zweig. Per `grep -rn
      ukfe_json_handle` verifiziert: der alte JSON-Handler `handle_payload_list()`
      (`json_parse.c`), der bisher einzige Ort, der `app->payloads[]`/`app->payload_count`
      befüllt und Custom-Event `0x12` sendet, hat keinen Aufrufer mehr im Baum — die
      Payload-Liste-Szene (`scenes/scene_payload_list.c`) bleibt dadurch dauerhaft leer
      (`"(keine — Liste laden)"`), unabhängig von der Hub-Antwort. Mit Host-Repro (ALT/NEU
      1:1 aus `handle_rf_response()`, vier Szenarien inkl. Randfall größter gültiger Index
      und kartenkontrolliertem Index außerhalb `UKFE_MAX_PAYLOADS`) unter ASan/UBSan
      verifiziert — Fix (ergänzter `case UkfeRfRespPayload:`, bounds-geprüft) in
      `app-improvements/lora-ukfe/payload-list-response-dispatch-fix.md`.
      `UkfeRfRespScanHit` (0x90) bleibt aus demselben Grund unbehandelt, für künftige
      Sitzung vorgemerkt. Restlicher `lora-ukfe`-Code (`command_catalog.h`, restliche
      `scenes/`) weiterhin fast ungeprüft.
- [x] **lora-ukfe** weiterverfolgt: den aus der vorigen Sitzung vorgemerkten
      `UkfeRfRespScanHit`-Fund (0x90, "args: uint32 freq_or_ch, uint8 rssi") geprüft —
      fällt wie zuvor `UkfeRfRespPayload` in den generischen `default`-Log-Zweig von
      `handle_rf_response()`. Anders als bei der Payload-Liste gibt es dafür (noch)
      keine eigene Konsumenten-Scene (per `grep -rn "ScanHit\|0x90"` verifiziert: kommt
      nur in der Enum-Definition vor) — kein Zustandsverlust, aber die Nutzlast
      (Frequenz/Kanal, RSSI) aller fünf Scan-Trefferantworten (LoRa/SubGHz/WiFi/BLE/
      RemoteID) bleibt im Log unlesbar. Mit Host-Repro (ALT/NEU 1:1 aus
      `handle_rf_response()`, fünf Szenarien inkl. Randfall `UINT32_MAX`/`INT8_MIN` und
      zu kurzem Frame `arg_len < 5`) unter ASan/UBSan verifiziert — Fix (ergänzter
      `case UkfeRfRespScanHit:`, bounds-geprüft gegen unbelegte `args[]`-Restbytes) in
      `app-improvements/lora-ukfe/scanhit-response-decode-fix.md`. Restlicher
      `lora-ukfe`-Code (`command_catalog.h`, restliche `scenes/`) weiterhin fast
      ungeprüft; eine dedizierte Scan-Ergebnis-Ansicht (analog
      `scene_payload_list.c`) wäre ein eigenständiger, größerer Folgebeitrag.
- [x] **ProtoPirate** `protopirate_history.c` (bislang nicht Teil der bereits geprüften
      Storage-/Parser-Funde) geprüft: `protopirate_history_add_to_history()` prüft
      Verlaufsgröße/Auto-Purge-Eviction vor statt nach der Duplikat-Entprellung
      (`code_last_hash_data`/`< 500 ms`-Fenster) — bei vollem Verlauf mit aktiviertem
      Auto-Purge verdrängt ein einzelner Tastendruck einer Fernbedienung (die dieselbe
      Bitfolge über die Druckdauer mehrfach sendet, jede Dekodierung ein eigener
      Aufruf) zwei echte, unabhängige Verlaufseinträge statt nur des einen, der für den
      einen tatsächlich neuen Eintrag nötig wäre — stiller, dauerhafter Datenverlust
      (samt zugehöriger Capture-Datei auf der SD-Karte) ohne Fehlermeldung. Mit
      Host-Repro (Kontrollfluss-Nachbau, Größen-/Reihenfolge-Buchhaltung) verifiziert —
      Fix (Entprellung vor die Limit-Prüfung gezogen) in
      `app-improvements/ProtoPirate/history-purge-before-debounce-item-loss-fix.md`.
      Restlicher `ProtoPirate`-Code (einzelne Protokoll-Decoder/-Encoder, `scenes/`,
      `views/`) sowie PSA-Bruteforce-Kernel weiterhin größtenteils ungeprüft.
- [x] **g4meover-magspoof** `mag_state.c::mag_state_load()` geprüft: `MagState state;` ist beim
      Deklarieren uninitialisiert; wenn `MAG_STATE_PATH` fehlt (erster App-Start, frische
      SD-Karte) oder die Zustandsdatei einen ungültigen/veralteten Header hat, wird der komplette
      feldsetzende Block übersprungen, und `mag_state_gpio_is_valid(&state)` liest
      `pin_input`/`pin_output`/`pin_enable` unbedingt vor jeder Initialisierung — mit
      eigenständigem Host-Repro unter Valgrind Memcheck als „uninitialised value(s)"-Fund
      verifiziert. Aktuelle Auswirkung eingegrenzt: der nachfolgende `if(!loaded_from_file)`-Block
      überschreibt die Pins in jedem geprüften Fall ohnehin wieder mit den korrekten Defaults,
      reine UB ohne derzeit beobachtbaren Effekt, aber fragil bei künftigen Refactors — Fix
      (`MagState state = {0};`) in
      `app-improvements/g4meover-magspoof/state-load-uninitialized-gpio-validity-check-fix.md`.
- [x] **RollForge** `module_rolljam.c` (`rj_tick()`, Übergang `RJ_P2_RX` → `RJ_REPLAY`)
      geprüft: die Statuszeile `"Replaying A... (B saved)"` wird gesetzt, bevor
      `rollforge_save_sig()` für Signal B je aufgerufen wurde — B wird laut `grep` im
      gesamten Modul ausschließlich manuell über `[v]` in `RJ_DONE` gespeichert. Da
      `[Back]` während des anschließenden Replays jederzeit bedienbar ist und
      `rj_exit()`/`rj_rf_clean()` keinerlei Speicheraufruf enthalten, führt der
      plausibelste Bedienschritt ("B ist ja schon gespeichert") zu unwiderruflichem
      Verlust von Signal B — genau dem Wert, den der RollJam-Angriff eigentlich liefern
      soll. Mit Host-Repro (Kontrollfluss-Nachbau inkl. Abbruchpfad, vier Szenarien:
      sofortiger Abbruch ALT/NEU, Regressionsfall ohne Abbruch, simulierter SD-Fehler)
      verifiziert — Fix (Speicherung von B an den `RJ_P2_RX`→`RJ_REPLAY`-Übergang
      vorgezogen, Statuszeile spiegelt jetzt den tatsächlichen Rückgabewert) in
      `app-improvements/RollForge/rj-b-premature-saved-claim-fix.md`. Rest von
      `module_rolljam.c`/`module_rolllab.c`/`module_jammer.c` bereits in früheren
      Sitzungen geprüft; `rollforge_rf.c`/`rollforge_storage.c`/`rollforge_app.c`
      (Lifecycle/Ext-Int-Umschaltung) weiterhin nur überflogen, nicht Zeile für Zeile
      geprüft — für künftige Sitzung vorgemerkt.
- [x] **RollForge** Lifecycle-/Ext-Int-Umschaltungs-Rückstellpunkt aufgegriffen:
      `rollforge_rf.c`/`rollforge_storage.c`/`rollforge_app.c` jetzt vollständig Zeile für
      Zeile geprüft. Fund in `rollforge_rf.c`: `rf_power_on()` beansprucht das Eigentum an
      der externen CC1101-OTG-5V-Versorgung (`s_otg_by_app = true`) allein danach, ob die
      Schiene am Ende der Funktion an ist — nicht danach, ob RollForge sie selbst
      eingeschaltet hat. War "5V on GPIO" bereits im Power-Menü aktiv (oder von einer
      anderen Erweiterungsplatinen-App), beansprucht RollForge trotzdem Eigentum und
      schaltet die fremde Versorgung in `rf_power_off()` wieder ab — im ungünstigsten Fall
      (kein EXT-Board angeschlossen) bereits augenblicklich beim bloßen App-Start, da
      `rollforge_rf_init()` bei fehlendem EXT-Board sofort `rf_power_off()` aufruft.
      Gegenstück zum bereits gefundenen FlipperJammer-OTG-Leck (dort: eigene Versorgung
      bleibt fälschlich an; hier: fremde Versorgung wird fälschlich abgeschaltet). Mit
      Host-Repro (`furi_hal_power_is_otg_enabled/enable_otg/disable_otg` als einzelner
      globaler Zustand gemockt, drei Szenarien inkl. zweier Regressionsfälle) verifiziert —
      Fix (frühzeitige Rückkehr in `rf_power_on()`, wenn die Versorgung bereits an ist) in
      `app-improvements/RollForge/otg-power-false-ownership-claim-fix.md`. Zusätzlich
      geprüft, kein Fund: `rollforge_rf_switch()` (OTG bewusst unangetastet),
      `rollforge_save_sig()` (Grenzen/Vorzeichenumwandlung der RAW-Werte),
      Hauptschleifen-Geräte-Guard in `rollforge_app.c` (Lücke von einer Iteration beim
      Fehlen von CC1101 vorhanden, aber alle drei `_enter()`-Funktionen fassen nur
      Status-Felder an, kein Hardwarezugriff, und der interne CC1101 ist auf echter
      Hardware ohnehin immer vorhanden — rein theoretischer Pfad, kein Fix nötig). Damit
      ist die Lifecycle-/Ext-Int-Umschaltungs-Rückstellpunkt-Gruppe für RollForge
      abgeschlossen.
- [x] **g4meover-magspoof** Hardware-Lesepfad/Sendefunktion aufgegriffen (`mag.c`
      App-Lifecycle bereits mitgeprüft, kein Fund — die OTG-5V-Ownership-Behandlung in
      `mag_app()` prüft `otg_was_enabled` korrekt vor dem Aktivieren und respektiert damit
      fremdes Eigentum, im Gegensatz zum zuvor gefundenen RollForge-Fall).
      `helpers/mag_helpers.c::mag_encode()` (die zentrale Bit-Pack-Funktion hinter
      `mag_spoof()`) geprüft: die aufrufenden Sendefunktionen legen für jeden Track feste
      64/128-Byte-Stack-Puffer an (Kommentar geht von "68 chars max track 1" aus),
      `mag_encode()` kennt diese Kapazität aber nicht und läuft bis zum Nullterminator der
      Eingabe durch — ab 73 Zeichen (Track 1, 7 Bit/Zeichen) bzw. 102 Zeichen (Track 2/3,
      5 Bit/Zeichen) schreibt `add_bit()` bereits hinter die Allokation. Bei mir über drei
      unabhängige Wege erreichbar: UART-Kartenlesung (bis 4096 Byte Sammelpuffer,
      `mag_scene_read.c`), geladene `.mag`-Datei (`flipper_format_read_string()` ohne
      Längenprüfung, `mag_device_load_data()`), manuelle Eingabe ("Enter track data (WIP)",
      bis 149 Zeichen laut `MAG_TEXT_STORE_SIZE`). `mag_spoof()` wird bei jedem
      "Emulate"-Vorgang direkt aufgerufen — kein Sonderfall, sondern der reguläre
      Absturzpfad für jede Eingabe über der stillschweigend angenommenen Zeichengrenze. Mit
      eigenständigem Host-Repro der reinen Bit-Pack-Logik unter AddressSanitizer verifiziert
      (100-Zeichen-Track-1 löst zuverlässig `stack-buffer-overflow` in `add_bit()`/
      `mag_encode()` aus) — Fix (neuer Kapazitätsparameter, Eingabe wird ab der
      Pufferkapazität sauber gekappt statt zu überlaufen, LRC-Byte bleibt reserviert) in
      `app-improvements/g4meover-magspoof/mag-encode-fixed-buffer-overflow-fix.md`.
- [x] **ProtoPirate** `protocols/porsche_touareg.c` geprüft (bislang unter "restlicher
      ProtoPirate-Code" vorgemerkt, erster konkreter Fund in diesem Protokoll): der
      empfangene 16-Bit-Rolling-Zähler steckt verschlüsselt in `pkt[4..7]` und wird beim
      Empfang durch reines Brute-Force-Ausprobieren zurückgewonnen
      (`porsche_cayenne_parse_data()`), die Such-Obergrenze ist aber fest auf `256` begrenzt
      statt auf den vollen `uint16_t`-Wertebereich des Zählerfelds (0–65535), den die
      Sender-Seite (`subghz_protocol_encoder_porsche_cayenne_get_upload()`, dieselbe Datei)
      bereits unbegrenzt nutzt — jeder reale Sender überschreitet einen Zählerstand von 256
      nach spätestens 256 Tastendrücken, danach bleibt `instance->generic.cnt` bei jedem
      empfangenen Frame stillschweigend auf dem Initialwert `0` stehen (`Cnt:0000`), ohne
      dass die App das als Fehlschlag erkennbar macht; Serial/Button werden davon unabhängig
      weiterhin korrekt angezeigt. Mit Host-Repro (`porsche_cayenne_compute_frame()` 1:1
      übernommen, zwölf realistische Zählerstände beidseits der 256er-Grenze erzeugt und
      zurückgesucht) verifiziert: ALT liefert für 7 von 12 Fällen (`cnt` > 256) `Cnt:0000`
      statt des echten Werts, NEU (Suchbereich auf `65535` erweitert, Schleifentyp auf
      `uint32_t` angepasst, um den sonst bei `try_cnt == 65535` auftretenden
      `uint16_t`-Überlauf/Endlosschleife zu vermeiden) liefert in allen zwölf Fällen den
      korrekten Zähler — Fix in
      `app-improvements/ProtoPirate/porsche-cayenne-counter-bruteforce-range-fix.md`.
      Restlicher `ProtoPirate`-Code (übrige Protokoll-Decoder/-Encoder unter `protocols/`,
      `scenes/`, `views/`, PSA-Bruteforce-Kernel) weiterhin größtenteils ungeprüft.
- [x] **ProtoPirate** `protocols/fiat_v1.c` (Fiat/Lancia, Magneti-Marelli-BSI-Protokoll)
      vollständig geprüft: der 13-Byte-Rohframe wird beim Live-Empfang durchgehend korrekt in
      `raw_data[]` gehalten, aber für Speichern/Laden zusätzlich verlustbehaftet über ein
      `uint32_t extra_data` (40 Bit hineingeschoben, nur 32 passen — die zuerst eingeschobenen
      8 Bit, Byte 8, gehen dabei unwiderruflich verloren) sowie einen beim Laden zusätzlich auf
      32 Iterationen gedeckelten Wiederherstellungs-Loop (`i < extra_bits && i < 32`, obwohl
      `extra_bits` korrekt 40 beträgt — Byte 12, das CRC-Byte, wird dadurch nie
      wiederhergestellt) geführt — jede gespeicherte Aufnahme verliert beim Neuladen Byte 8 und
      Byte 12 (beide werden `0x00`), `get_string()` zeigt danach ein falsches erstes
      Payload-Byte und praktisch immer `CRC:[FAIL]` statt der ursprünglich gültigen Aufnahme.
      Mit Host-Repro (Bit-Packing/Wiederherstellung 1:1 aus der Datei, 13-Byte-Testframe durch
      den vollen Empfangen→Speichern→Laden-Zyklus geschickt) verifiziert: ALT verliert 2 von 13
      Byte, NEU (Speichern/Laden auf ein direktes 5-Byte-Hex-Feld `ExtraRaw` umgestellt, analog
      zum `TailRaw`-Muster in `ford_v2.c`) liefert einen byte-exakten Rundlauf — Fix in
      `app-improvements/ProtoPirate/fiat-v1-extra-raw-save-load-corruption-fix.md`.
      Restlicher `ProtoPirate`-Code (`fiat_v0`, `chrysler_v0`, `honda_static`/`honda_v1`,
      `kia_v0`–`v7`, `land_rover_v0`, `mazda_v0`, `star_line`, `subaru`, `vag.c`,
      PSA-Bruteforce-Kernel) weiterhin größtenteils ungeprüft.
- [x] **ProtoPirate** `protocols/fiat_v0.c` (Fiat/Lancia, älteres 71-Bit-Protokoll)
      vollständig geprüft: `subghz_protocol_decoder_fiat_v0_deserialize()` ist ein reiner
      Ein-Zeilen-Durchreicher zur Stock-Hilfsfunktion
      `subghz_block_generic_deserialize_check_count_bit()`, die laut ihrem Vertrag nur
      `generic.data`/`generic.data_count_bit` aus `"Key"`/`"Bit"` füllt — die drei
      eigenen Decoder-Felder `hop`/`fix`/`endbyte`, die
      `subghz_protocol_decoder_fiat_v0_get_string()` als einzige Anzeigequelle für
      Key/Hop/Sn/EndByte nutzt, bleiben nach dem Laden einer gespeicherten Aufnahme auf
      ihrem `calloc()`-Nullwert stehen. Jede über "Saved" geöffnete Fiat-V0-Datei zeigt
      dadurch ausnahmslos Nullen statt der echten Werte, obwohl `generic.data` selbst
      bereits korrekt geladen ist und Senden (eigener Encoder-Deserialize-Pfad)
      unbetroffen bleibt. Dieselbe Fehlerklasse wie der zuvor gefundene
      `fiat_v1`-Speicher/Lade-Bug ("Decoder-`deserialize()` vergisst protokolleigene,
      von `get_string()` benötigte Felder"), hier erstmals für `fiat_v0` bestätigt. Mit
      Host-Repro (Modell der Stock-Hilfsfunktion, beide Deserialize-Fassungen, drei
      Testfälle inkl. Randfällen `0x00000000`/`0xFFFFFFFF`) unter
      `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` verifiziert — Fix (Hop/Fix aus
      `generic.data` zurückrechnen, EndByte per selbem `"EndByte"`/`"Btn"`-Fallback lesen
      wie im bereits vorhandenen Encoder-Pfad) in
      `app-improvements/ProtoPirate/fiat-v0-decoder-deserialize-missing-field-restore-fix.md`.
      Restlicher `ProtoPirate`-Code (`chrysler_v0`, `honda_static`/`honda_v1`,
      `kia_v0`–`v1`, `kia_v3`–`v7`, `land_rover_v0`, `mazda_v0`, `star_line`, `subaru`,
      `vag.c`, PSA-Bruteforce-Kernel) weiterhin größtenteils ungeprüft. Beim Durchsehen
      der übrigen Decoder außerdem festgestellt: derselbe Ein-Zeilen-Durchreicher-
      Deserialize ohne Nachbearbeitung findet sich mutmaßlich auch in weiteren noch
      ungeprüften Dateien (`kia_v2.c` diese Sitzung neu bestätigt, siehe eigener Eintrag
      unten) — als eigenständiger, übergreifender Prüfpunkt für künftige Sitzungen
      vorgemerkt, Datei für Datei zu bestätigen statt pauschal anzunehmen.
- [x] **ProtoPirate** `protocols/kia_v2.c` (Kia/Hyundai, 53-Bit-Rolling-Code-Protokoll)
      vollständig geprüft: derselbe Ein-Zeilen-Durchreicher-Deserialize wie bei
      `fiat_v0`/`fiat_v1` bestätigt. `kia_protocol_decoder_v2_deserialize()` ruft nur die
      Stock-Hilfsfunktion `subghz_block_generic_deserialize_check_count_bit()` auf, die
      ausschließlich `generic.data`/`generic.data_count_bit` aus `"Key"`/`"Bit"` füllt —
      `generic.serial`/`btn`/`cnt`, die einzige Datenquelle von
      `kia_protocol_decoder_v2_get_string()` für `Sn:`/`Btn:`/`Cnt:`, bleiben nach dem
      Laden einer gespeicherten Aufnahme unverändert (Instanz kommt aus `malloc()` ohne
      Nullinit). Jede über "Saved" geöffnete Kia-V2-Datei zeigt dadurch ausnahmslos
      `Sn:00000000 Btn:0 Cnt:000` statt der echten Werte, während `Key:`/`CRC:` (direkt
      aus `generic.data` berechnet) bereits korrekt sind und Senden (eigener
      Encoder-Deserialize-Pfad) unbetroffen bleibt. Mit Host-Repro (Modell der
      Stock-Hilfsfunktion, beide Deserialize-Fassungen, drei Testfälle inkl. Randfällen
      `serial=btn=cnt=0`/Maximalwerten, Zielwerte unabhängig rückwärts in `generic.data`
      kodiert) unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` verifiziert —
      Fix (`serial`/`btn`/`cnt` mit derselben Formel wie im Live-Decode-Pfad
      `kia_protocol_decoder_v2_feed()` aus `generic.data` zurückrechnen) in
      `app-improvements/ProtoPirate/kia-v2-decoder-deserialize-missing-field-restore-fix.md`.
- [x] **ProtoPirate** `protocols/chrysler_v0.c` geprüft (nächster Kandidat aus der Liste
      des ungeprüften restlichen ProtoPirate-Codes): anders als `fiat_v0`/`fiat_v1`/`kia_v2`
      ruft `subghz_protocol_decoder_chrysler_v0_deserialize()` bereits
      `chrysler_v0_decode_packet()` (dieselbe Funktion wie im Live-Commit-Pfad) auf und
      rechnet `seed`/`decoded_button`/`check_ok`/`plain_a`/`plain_b`/`cnt`/`sn_b` korrekt aus
      dem geladenen `generic.data`+`Key_2` zurück, mit zusätzlichem, konsistentem
      Direkt-Laden von `Plain_A`/`Plain_B`, falls in der Datei vorhanden — kein Fund dieser
      Fehlerklasse. Beim Weiterlesen der Datei Fund in `protocols/land_rover_v0.c`
      (eigener Eintrag direkt im Anschluss).
- [x] **ProtoPirate** `protocols/land_rover_v0.c` (81-Bit Land-Rover-Rolling-Code-Protokoll)
      geprüft: `subghz_protocol_decoder_land_rover_v0_get_string()`s `Tail:`-Anzeige
      rekonstruiert den angezeigten Wert nicht aus `instance->tail`/`instance->extra_bit`,
      sondern rät ihn ausschließlich aus Bit 15 von `instance->tail` auf einen von zwei fest
      kodierten Werten (`0x1FFFF`/`0x0FFFF`) und ignoriert `extra_bit` komplett. Für die
      beiden einzigen Zustände, die ein erfolgreicher Live-Empfang erreichen kann
      (`tail∈{0xFFFF,0x7FFF}`, `extra_bit=1`), trifft das zufällig den korrekten Wert — beim
      Laden einer Datei übernimmt `deserialize()` `Tail`/`ExtraBit` aber direkt aus den
      Datei-Feldern, ohne `land_rover_v0_validate_frame()`s Rückgabewert zu prüfen (die
      `check_ok`/`tail_ok`-Anzeige daneben ist bereits korrekt), sodass eine Datei mit
      abweichendem Tail-Wert oder `extra_bit=0` (bearbeitete/beschädigte Testdatei, im
      eigenen autorisierten Testrahmen ein regulärer Anwendungsfall) einen irreführenden,
      vom tatsächlichen Inhalt losgelösten `Tail:`-Wert anzeigt. Mit Host-Repro (beide
      Formeln 1:1 isoliert, sieben Fälle inkl. der zwei kanonischen Zustände als
      Regressionsanker) unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined`
      verifiziert — Fix (`Tail:` direkt aus `(tail<<1)|extra_bit` berechnen) in
      `app-improvements/ProtoPirate/land-rover-v0-tail-display-formula-fix.md`. Restlicher
      `ProtoPirate`-Code (`honda_static`/`honda_v1`, `kia_v0`–`v1`, `kia_v3`–`v7`, `mazda_v0`,
      `star_line`, `subaru`, `vag.c`, PSA-Bruteforce-Kernel) weiterhin größtenteils
      ungeprüft.
- [x] **ProtoPirate** `protocols/honda_static.c`, `protocols/honda_v1.c`, `protocols/kia_v0.c`,
      `protocols/kia_v1.c` und `protocols/subaru.c` vollständig geprüft (nächste Kandidaten
      aus der Liste des ungeprüften restlichen ProtoPirate-Codes): in allen fünf berechnet
      `get_string()` die angezeigten Sn/Btn/Cnt-Werte bei jedem Aufruf frisch aus
      `generic.data` (bzw. ruft dieselbe Rückrechnungsfunktion auf, die auch der
      Live-Decode-Pfad nutzt), sodass ein unvollständiges `deserialize()` dort folgenlos
      bleibt — kein Fund dieser Fehlerklasse. `mazda_v0.c` zusätzlich geprüft: dessen
      `deserialize()` lässt die internen `instance->serial`/`button`/`count`-Kopien bei
      fehlenden Serial/Btn/Cnt-Dateifeldern auf `0` stehen, aber sowohl `get_string()` als
      auch `serialize()` rufen vorher `mazda_v0_decode_key()` auf und überschreiben damit
      diese Kopien korrekt aus `generic.data` — selbstheilend, kein extern sichtbarer Fund.
      Dabei Fund in `protocols/kia_v3_v4.c`: `kia_protocol_decoder_v3_v4_get_string()` liest
      `generic.serial`/`btn`/`cnt` dagegen direkt und ungeprüft ohne Rückrechnung, während
      `kia_protocol_decoder_v3_v4_deserialize()` beim Laden nur `Encrypted`/`Decrypted`/
      `KIAVersion`/`CRC` restauriert und diese drei Felder nie setzt — nach dem Laden zeigt
      die Detailansicht beliebigen `malloc()`-Speicherinhalt statt der echten Werte (schlimmer
      als der bereits behobene `kia_v2`-Fund, der zumindest verlässlich `0` zeigte, da
      `kia_protocol_decoder_v3_v4_alloc()` `malloc()` statt `calloc()` verwendet). Mit
      Host-Repro (`pp_reverse_bits8()`/`pp_u64_to_bytes_be()` 1:1 übernommen, synthetischer
      Frame aus Ziel-`serial`/`btn`/`encrypted` rückwärts aufgebaut, ALT/NEU gegen
      absichtlich „vergifteten" Vor-Zustand getestet statt gegen `0`, vier Fälle inkl.
      Randfällen) unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` verifiziert —
      Fix (dieselbe Rückrechnung wie `kia_v3_v4_process_buffer()` in `deserialize()`
      nachziehen) in
      `app-improvements/ProtoPirate/kia-v3-v4-decoder-deserialize-missing-field-restore-fix.md`.
      Restlicher `ProtoPirate`-Code (`kia_v5`–`v7`, `star_line`, `vag.c`,
      PSA-Bruteforce-Kernel) weiterhin größtenteils ungeprüft.
- [x] **ProtoPirate** `protocols/kia_v5.c` geprüft (nächster Kandidat aus der Liste des
      ungeprüften restlichen ProtoPirate-Codes): `kia_protocol_decoder_v5_deserialize()`
      liest ausschließlich `generic.data`/`data_count_bit`, während
      `kia_protocol_decoder_v5_get_string()` `instance->yek`/`generic.serial`/`btn`/`cnt`/
      `instance->crc` direkt (ohne Rückrechnung) ausgibt — fünf statt der bei `kia_v3_v4`
      gefundenen drei betroffenen Felder, da `Yek:` und `CRC:` hier zusätzlich betroffen
      sind. `kia_protocol_decoder_v5_alloc()` nutzt `malloc()` statt `calloc()`, die
      Detailansicht zeigt nach dem Laden also beliebigen Speicherinhalt statt der echten
      Werte. Mit eigenständigem Host-Repro (`mixer_decode()`/`build_keystore_from_mfkey()`/
      `pp_reverse_bits8()` 1:1 übernommen, `get_kia_v5_key()` durch festen Test-Schlüssel
      ersetzt, ALT/NEU gegen vergifteten Vor-Zustand statt `0` getestet, vier Fälle inkl.
      Randfällen, NEU zusätzlich gegen unabhängigen Nachbau der Live-Decode-Rückrechnung
      verglichen) unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` verifiziert —
      Fix (dieselbe Rückrechnung wie `KiaV5DecoderStepData` in `deserialize()` nachziehen,
      CRC aus dem bereits von `serialize()` geschriebenen Feld lesen) in
      `app-improvements/ProtoPirate/kia-v5-decoder-deserialize-missing-field-restore-fix.md`.
      `protocols/kia_v6.c` und `protocols/kia_v7.c` im selben Durchgang geprüft, kein Fund:
      `kia_v6`s `deserialize()` liest `Key_2`–`Key_4`/`Fx` sowie über
      `pp_encoder_read_fields()` `Serial:`/`Btn:`/`Cnt:` bereits vollständig zurück;
      `kia_v7`s `get_string()` ruft vor jeder Ausgabe `kia_v7_decode_key_decoder()` auf und
      rechnet Sn/Btn/Cnt/CRC bei jedem Aufruf frisch aus `generic.data` zurück
      (Selbstheilungs-Muster wie `mazda_v0.c`). Restlicher `ProtoPirate`-Code (`star_line`,
      `vag.c`, PSA-Bruteforce-Kernel) weiterhin größtenteils ungeprüft.
- [x] **ProtoPirate** `protocols/star_line.c` und `protocols/vag.c` geprüft (nächste
      Kandidaten aus der Liste des ungeprüften restlichen ProtoPirate-Codes): beide
      selbstheilend, kein Fund der bisherigen Fehlerklasse. `star_line.c`s
      `get_string()`/`serialize()` rufen vor jeder Ausgabe
      `subghz_protocol_star_line_check_remote_controller()` auf, die `serial`/`btn`
      deterministisch aus `generic.data` zurückrechnet und `cnt` über denselben
      KeeLoq-Entschlüsselungspfad wie beim Live-Empfang neu bestimmt (identisches
      Verhalten bei fehlendem Herstellerschlüssel wie beim Live-Empfang, keine
      Lade-spezifische Abweichung). `vag.c`s `deserialize()` ruft nach dem Einlesen von
      Key1/Key2/Type bereits selbst `vag_parse_data()` auf (dieselbe Funktion, die auch
      `get_string()`/`serialize()` bei noch nicht entschlüsseltem Zustand aufrufen) —
      Serial/Cnt/Btn/`decrypted`-Status werden vollständig aus Key1/Key2/Type
      zurückgerechnet, keine der drei betroffenen Felder hängt von unbestimmtem
      `malloc()`-Speicherinhalt ab (`alloc()` nutzt bereits `calloc()`). Stattdessen dabei
      den seit mehreren Sitzungen mitgeschleppten Rückstellpunkt "`scher_khan.c`s
      `malloc()`-ohne-`furi_check()`-Muster als übergreifender Prüfpunkt" aufgegriffen und
      vollständig abgearbeitet: systematische Prüfung aller `= malloc(` in `protocols/*.c`
      ergab neun statt der vermuteten zwei bis drei betroffenen Allokationsfunktionen ohne
      NULL-Absicherung (`ford_v0.c` Encoder+Decoder, `kia_v1.c` Encoder+Decoder, `kia_v2.c`
      Encoder+Decoder, `scher_khan.c` Decoder, `subaru.c` Encoder+Decoder) gegenüber elf
      bereits korrekt abgesicherten Allokationsfunktionen im selben Baum. Mit Host-Repro
      (identisches Strukturmuster nachgebaut, `malloc()`-Stub simuliert Speichermangel,
      ALT/NEU je in eigenem `fork()`-Kindprozess unter `gcc -O0 -Wall -Wextra` ohne ASan,
      damit das reale Signal statt ASan's abgefangenem Bericht sichtbar wird) verifiziert:
      ALT endet unkontrolliert per `SIGSEGV`, NEU (mit `furi_check(instance)` direkt nach
      `malloc()`, wie an den elf bereits korrekten Stellen) kontrolliert mit eigenem
      Exit-Code vor jedem Schreibzugriff — Port-Vorschlag inkl. Fix (Ein-Zeilen-Ergänzung an
      allen neun Stellen) in
      `app-improvements/ProtoPirate/alloc-malloc-null-check-hardening.md`. Restlicher
      `ProtoPirate`-Code (PSA-Bruteforce-Kernel) weiterhin offen.
- [x] **g4meover-magspoof** aus früherer Sitzung vorgemerkten Folgepunkt aufgegriffen
      (`mag-encode-fixed-buffer-overflow-fix.md`, Risiko-Abschnitt: „sichtbare Warnung
      (Popup/Log) beim Kappen sinnvoll"): `mag_encode()` um einen optionalen
      `out_truncated`-Ausgabeparameter erweitert, `mag_spoof()` verknüpft die drei
      Track-Flags in `mag->last_tx_truncated` und loggt sie (`FURI_LOG_W`), die
      Emulate-Szene löst bei gesetztem Flag zusätzlich zum bestehenden Cyan-Blink eine
      `sequence_error`-Rückmeldung aus (bereits an anderer Stelle in diesem Ökosystem gegen
      `mntm-012` verifiziert). Mit Host-Repro (reiner Bit-Pack-Nachbau, identisch zum bereits
      kapazitätsbegrenzten `mag_encode()`-Stand) verifiziert: ALT liefert für eine 72- und
      eine 73-Zeichen-Track-1-Eingabe identischen Bitcount/identische Bitfolge (ebenso 101
      vs. 102 Zeichen bei Track 2/3) — aus dem Rückgabewert allein nicht unterscheidbar; NEU
      setzt `truncated` exakt an der bereits bekannten Kappungsgrenze, Bitausgabe bleibt
      dabei unverändert, Regressionsfall (40 Zeichen, `NULL` vs. `&truncated`) bestätigt
      keinen Verhaltensunterschied bei normal langen Tracks — Port-Vorschlag inkl. Fix in
      `app-improvements/g4meover-magspoof/track-truncation-visible-warning-fix.md`. Damit ist
      der zuletzt offene `g4meover-magspoof`-Rückstellpunkt abgeschlossen.
- [x] **ProtoPirate**: den beim `VW`/`VAG`-Timing-Tuner-Fix selbst vermerkten offenen Punkt
      aufgegriffen ("`Mitsubishi V0` hat überhaupt keinen Eintrag in `protocol_timings[]`,
      zurückgestellt bis verifizierte Referenz-Timings vorliegen") — beim erneuten Draufschauen
      festgestellt, dass diese Zurückstellung zu vorsichtig war: alle 24 vorhandenen
      Tabellenzeilen sind bereits 1:1 Kopien des `SubGhzBlockConst` des jeweiligen Decoders
      selbst (z. B. `VW`/`VAG`: 500/1000/120µs — identisch zu `vag.c`), keine separat
      recherchierten Werte. Mitsubishi V0 (real registriert über
      `protocols/plugins/protopirate_fm_plugin.c`, FM-Registry) hat mit
      `subghz_protocol_mitsubishi_const` (`protocols/mitsubishi_v0.c`) dieselben Werte bereits
      im Quelltext (250/500/100µs, 80 Bit) — Fix (neue Tabellenzeile plus fehlender
      `mitsubishi_v0.h`-Include) in
      `app-improvements/ProtoPirate/mitsubishi-v0-timing-tuner-missing-entry-fix.md`. Mit
      Host-Repro (Tabellensuche + Alias-Fallback 1:1 nachgebaut, alle 25 real registrierten
      Decoder-Namen aus beiden Registries einzeln gegen ALT/NEU geprüft) unter
      `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALT liefert für
      `"Mitsubishi V0"` `NOT FOUND`, NEU liefert die korrekten Referenzwerte, alle 23 übrigen
      Namen (sowie der separate, hier bewusst unveränderte `"VW"`/`VAG`-Fund) bleiben
      unverändert. Dabei zusätzlich `ford_v0.c` und `mitsubishi_v0.c` (Encoder/Decoder-
      Deserialize) sowie `aut64.c`/`keys.c`/`protocols_common.c` aus dem weiterhin offenen
      Rückstellpunkt "restlicher ProtoPirate-Code" auf das bereits bestätigte
      Deserialize-Bug-Muster geprüft: kein neuer Fund (beide lesen Serial/Btn/Cnt über
      `pp_encoder_read_fields()` aus stets unbedingt geschriebenen Serialize-Feldern zurück,
      strukturell identisch zum bereits als „gut" bestätigten `kia_v6`-Muster; `aut64.c` ist
      eine in sich geschlossene, bereits defensiv validierte Kryptoprimitive ohne
      Deserialize-Bezug). Restlicher `ProtoPirate`-Code (PSA-Bruteforce-Kernel; `ford_v1.c`/
      `ford_v2.c`/`ford_v3.c` nur auf dieses eine Bug-Muster geprüft, nicht auf sonstige
      Korrektheit) weiterhin offen.
- [x] **ProtoPirate**: den zuletzt offenen Rückstellpunkt "`ford_v1.c`/`ford_v2.c`/`ford_v3.c`
      nur auf das Deserialize-Bug-Muster geprüft, nicht auf sonstige Korrektheit" aufgegriffen —
      alle drei Dateien vollständig gelesen. `ford_v1.c`/`ford_v2.c` (CRC16-/Paritäts-/
      Interleave-Descramble- bzw. Sync-Wort-/Struktur-Validierungslogik): alle Feld-
      Transformationen über die jeweilige Encoder-Gegenrichtung exakt umkehrbar (mit
      unabhängigem Host-Repro über je 200 000 zufällige Serial/Btn/Cnt/Tail-Werte bitgenau
      bestätigt), kein Fund; zwei unerreichbare/inaktive Codestellen ohne Funktionsauswirkung
      notiert (`ford_v1.c`s Preset-Hop-Zweig, feste `0`-Stub-Funktion; `weak`-Symbol-
      Zähler-Override mit Standard `false`). `ford_v3.c`: toter Zweig in
      `FordV3DecoderStepData` gefunden — Copy-Paste aus `FordV3DecoderStepReset`, prüft die
      bereits als ungültig erkannte `duration` ein zweites Mal auf "kurz", kann dadurch nie
      auslösen (bewiesen, keine Empfangslücke, da der reguläre `Reset`-Fall den nächsten Puls
      ohnehin korrekt abfängt) — Fix (Entfernen des toten Zweigs) in
      `app-improvements/ProtoPirate/ford-v3-unreachable-preamble-restart-check-fix.md`. Mit
      Host-Repro (erschöpfende Duration-Sweep 0-20000 plus randomisierte ALT-vs-NEU-
      Zustandsäquivalenz über 200 000 gefütterte Pulse) unter
      `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` verifiziert: toter Zweig in keinem
      Fall erreicht, ALT/NEU-Zustand nach jedem Puls identisch. Damit ist der Rückstellpunkt
      abgeschlossen; restlicher `ProtoPirate`-Code (PSA-Bruteforce-Kernel) weiterhin offen.
- [x] **g4meover-magspoof**: `mag_device_load_data()` (`mag_device.c`) geprüft — übernimmt den
      Dateinamen (ohne Endung) per bloßem `strncpy(dev_name, ..., MAG_DEV_NAME_MAX_LEN)` in ein
      `char[23]`-Feld einer nur `malloc()`'ten (nicht zero-initialisierten) Struktur; bei 22+
      Zeichen langen Dateinamen bleibt `dev_name` ohne Terminierung. Beim App-Start mit
      Dateipfad-Argument (`mag.c`, Favoriten/„Open with") fehlt der reparierende
      `mag_device_set_name()`-Folgeaufruf, der beim interaktiven „Load file"-Pfad zufällig
      terminiert (per Selbst-Kopie-Nachrechnung als unkritisch bewiesen) — jeder spätere
      `%s`-Zugriff auf `dev_name` (Emulate-/Delete-Confirm-/Saved-Info-Szene,
      `mag_device_delete()`-Fallback) liest dann über die 23-Byte-Struktur hinaus. Mit
      Host-Repro unter ASan verifiziert (`heap-buffer-overflow`, „0 bytes after 23-byte
      region") — Fix in
      `app-improvements/g4meover-magspoof/mag-device-load-data-dev-name-oob-read-fix.md`
      (`strlcpy` auf volle Puffergröße statt `strncpy`, behebt beide Aufrufpfade an der
      Quelle).
- [x] **ProtoPirate**: ersten Schritt in den bislang zurückgestellten Rückstellpunkt
      "PSA-Bruteforce-Kernel" gemacht — bewusst nicht an den reverse-engineerten
      Krypto-Konstanten selbst (`psa_crypto_bf.c`/`psa_bf_core.c`, weiterhin ohne verifizierte
      Referenzimplementierung nicht risikofrei bewertbar), sondern an der reinen
      Orchestrierungsschicht drumherum (`scenes/plugins/protopirate_psa_bf_plugin.c`,
      `helpers/protopirate_psa_bf_host.c`). Fund: der Hintergrund-Bruteforce-Thread setzt
      `state->status` auf einen Endwert, bevor er das zugehörige
      `ProtoPirateCustomEventPsaBruteforceComplete`-Event verschickt; ein unabhängig
      laufender periodischer Tick kann denselben Statuswechsel zuerst bemerken und
      `bf_finish_and_show_result()` zuerst aufrufen — eine echte, unsynchronisierte
      Wettlaufsituation zwischen beiden Pfaden. Der Sub-Decode-spezifische
      `subdecode_signal_info_refresh()`-Aufruf sitzt aber nur im Custom-Event-Zweig; gewinnt
      der Tick bei `NOT_FOUND`/`CANCELLED` das Rennen, gibt er `g_bf_state` bereits frei,
      wodurch der später verarbeitete Custom-Event-Zweig seinen `if(g_bf_state)`-Wächter
      nicht mehr passiert und der Refresh ausbleibt — der Sub-Decode-Szenenzustand/die
      Empfänger-Statuszeile bleibt dann auf dem Stand vor dem Bruteforce-Versuch hängen. Mit
      Host-Repro (Zustandsautomatik 1:1 nachgebaut, alle 12 Kombinationen aus Ergebnis ×
      Kontext × Ereignisreihenfolge) verifiziert: ALT verliert den Refresh in exakt den 2
      vorhergesagten Fällen, NEU (Refresh-Aufruf von der Custom-Event-Aufrufstelle in
      `bf_finish_and_show_result()` selbst verlagert) liefert ihn in allen 6
      Sub-Decode-Fällen zuverlässig, 0 Regressionen im `ReceiverInfo`-Kontext, keine
      Doppel-Freigabe in allen 24 simulierten Durchläufen — Fix in
      `app-improvements/ProtoPirate/psa-bf-subdecode-refresh-race-fix.md`. Dabei
      `helpers/protopirate_psa_bf_host.c` vollständig gelesen (Plugin-Lade-/
      Entlade-Infrastruktur, durchgehend `NULL`-abgesichert), kein Fund; einen anfänglich
      vermuteten zweiten Fund (verschwindende Erfolgs-Schaltfläche im `ReceiverInfo`-Kontext
      bei Doppelaufruf) beim Nachrechnen als nicht zutreffend verworfen (alle drei
      tatsächlichen Aufrufstellen übergeben durchgehend `NULL`, die vorhandene
      Ersatzlogik liefert daher bei jedem Aufruf dieselbe Schaltfläche). Restlicher
      PSA-Bruteforce-Kernel (`psa.c`, `psa_crypto.c`, die reverse-engineerten
      Krypto-Konstanten selbst) weiterhin offen.
- [x] **ProtoPirate**: den zuletzt offenen Rückstellpunkt "`protocols/psa.c` (1443 Zeilen,
      eigentlicher PSA-Protokoll-Decoder/-Encoder) in dieser Sitzung nicht geprüft" aufgegriffen
      — vollständig gelesen. Fund in `subghz_protocol_decoder_psa_feed()`: der Decoder läuft
      über zwei alternative Zustandspfade (`State2`/`State4`, je nach erkanntem
      Präambel-Timing), die beide dieselben 64+16 Bit über strukturell identische
      Manchester-Schleifen decodieren und in denselben Abschlusstest
      (`decode_count_bit != PSA_KEY2_BITS`) laufen. `State2` deckelt den Zähler-Inkrement
      bewusst auf `< PSA_KEY2_BITS` (80), sodass zusätzliche, nach dem 80. Datenbit
      eintreffende Manchester-dekodierbare Flanken (physikalisch plausibles Jitter/Rauschen
      vor der eigentlichen Rahmenende-Lücke) folgenlos bleiben; `State4` erhöht denselben
      Zähler bei jeder erfolgreich dekodierten Flanke unbedingt, ohne diesen Deckel — schon
      eine einzige zusätzliche Flanke lässt den Zähler auf 81 statt 80 laufen und der
      anschließende Gleichheitstest verwirft ein andernfalls vollständig korrektes,
      empfangenes 80-Bit-Signal komplett, ohne jede Fehlermeldung. Mit Host-Repro (Zähler-Logik
      beider Zustände 1:1 nachgebaut, 0-5 zusätzliche Störflanken nach einem vollständigen
      80-Bit-Signal sowie 5 echte Kurz-Signal-Regressionsfälle) unter
      `gcc -O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALT verliert das Signal
      bei jeder der fünf Störflanken-Mengen (1-5), NEU (Deckel analog State2 in den
      State4-Bit-Dekodierzweig übernommen) sowie der unveränderte State2-Pfad akzeptieren
      dasselbe Signal in allen Fällen korrekt; alle fünf Kurz-Signal-Regressionsfälle bleiben
      unverändert korrekt verworfen — Port-Vorschlag inkl. Fix (Ein-Zeilen-Deckel, mirror-genau
      zu State2) in
      `app-improvements/ProtoPirate/state4-decode-bit-count-uncapped-frame-loss-fix.md`. Dabei
      Encoder/Decoder-Symmetrie (`psa_second_stage_xor_encrypt`/`_decrypt`,
      `psa_calculate_checksum`, `psa_extract_fields_mode23` vs. `psa_build_buffer_mode23`),
      Upload-Pulszahl-Rechnung (`psa_encoder_build_upload()`, exakt 325 = `PSA_UPLOAD_CAPACITY`
      im worst case) sowie beide Deserialize-Pfade (Flipper-Format-Feldreihenfolge, jeder
      Lesezugriff ruft vorab selbst `flipper_format_rewind()`) algebraisch bzw. strukturell
      geprüft, kein weiterer Fund. Restlicher PSA-Bruteforce-Kernel (`psa_crypto.c`s reine
      Puffer-Auf-/Abbau-Helfer, die reverse-engineerten Krypto-Konstanten selbst) weiterhin
      offen.
- [x] **ProtoPirate**: letzten offenen Teil des PSA-Bruteforce-Rückstellpunkts aufgegriffen —
      `protocols/psa_crypto.c`s reine Puffer-Auf-/Abbau-Helfer vollständig gelesen. Fund an der
      Stelle, an der ein Bruteforce-Fund in Anzeige-/Speicherfelder zerlegt wird
      (`psa_crypto_bf.c`, `psa_bf_fill_state_from_buffer()`): die Funktion ist für die
      BF1-Erfolgsprüfung geschrieben (8-Bit-CRC allein in `buffer[9]`, `buffer[8]` echte
      Zähler-Nutzlast), wird aber unverändert auch für die BF2-Erfolgsprüfung wiederverwendet,
      deren `expected_crc = dec_v1 & 0xFFFF` **beide** Bytes (`buffer[8]` und `buffer[9]`) als
      16-Bit-Prüfsumme verwendet. Für einen BF2-Fund verunreinigt `decrypted_counter` deshalb
      das obere CRC-Byte als Zähler-Nutzlast, und `decrypted_crc` verliert dasselbe Byte —
      beide Felder landen unverändert in Verlaufsanzeige (`apply_success_to_history()`) und
      `.sub`-Datei (`host_patch_flipper_format_on_success()`, `FF_CNT`/`"CRC"`). Mit Host-Repro
      (20 synthetische BF2-Funde, `dec_v1`s untere 16 Bit jeweils exakt auf das reale CRC16
      gesetzt, reale BF2-Erfolgsprüfung vorab gegengeprüft; zusätzlich 16 BF1-Regressionsfälle)
      unter `gcc -O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALT liefert in
      19/20 BF2-Fällen ein falsches CRC und einen verunreinigten Counter (einzige Ausnahme: der
      entartete Alles-Null-Fall), NEU (eigene BF2-Zerlegungsfunktion, nur am BF2-Aufrufort
      eingesetzt) in 0/20, alle 16 BF1-Regressionsfälle bleiben unverändert korrekt — Fix in
      `app-improvements/ProtoPirate/psa-bf2-counter-crc-buffer8-conflation-fix.md`. Dabei
      `psa_crypto_setup_byte_buffer()`, `psa_crypto_prepare_tea_data()`,
      `psa_crypto_unpack_tea_result_to_buffer()` (Rundlauf-Symmetrie), `psa_crypto_tea_crc()`
      sowie `psa_crypto_crc16_bf2()` (gegen unabhängige Referenzimplementierung im Repro
      nachgerechnet) geprüft, kein weiterer Fund. Damit ist der PSA-Bruteforce-Rückstellpunkt
      quelltextlich vollständig abgearbeitet — offen bleiben ausschließlich die
      reverse-engineerten TEA-Krypto-Konstanten selbst, weiterhin ohne verifizierte
      Referenzimplementierung bewusst zurückgestellt.
- [x] **lora-ukfe**: den zuletzt offenen Rückstellpunkt "`command_catalog.h`, restliche
      `scenes/`" aufgegriffen — `command_catalog.h` selbst (reine Daten) sowie alle bis dahin
      ungeprüften Scenes (`scene_handlers.c`, `scene_mode_menu.c`, `scene_menu.c`,
      `scene_status.c`, `scene_settings.c`, `scene_log.c`, `scene_payload_list.c`) gelesen. Fund
      in `scene_menu.c`, `send_fn()`: baut für jeden Katalog-Eintrag mit `has_arg==true`
      pauschal einen 1-Byte-Arg-Frame — passt für sieben der acht `has_arg`-Einträge (je 1 Byte
      laut `rf/ukfe_rf.h`), aber nicht für `UkfeRfCmdTrigger` (`FN_PAYLOAD[0]`, "Trigger [V3]"),
      dessen eigener Kommentar in `rf/ukfe_rf.h:24` 5 Byte (`uint8 id, uint32 delay_ms` LE)
      verlangt. Der Katalog-Menüpunkt sendet dadurch einen für sich genommen gültigen (MAC/CRC
      korrekt), aber um die vollen 4 `delay_ms`-Bytes zu kurzen `ukfe_rf`-Frame — still, ohne
      Fehlermeldung. Die korrekte Baufunktion existiert bereits (`ukfe_rf_make_trigger()`,
      `rf/ukfe_rf.c:125`), wird aber im gesamten Baum nirgends aufgerufen — derselbe
      Unverdrahtet-Fund-Muster wie schon bei `rf_comm_counter()` und dem `UkfeRfRespPayload`-
      Dispatch aus den letzten beiden Sitzungen. Mit Host-Repro gegen den echten, unveränderten
      `rf/ukfe_rf.c`/`.h` (verbatim kopiert, reine Protokolllogik ohne `furi`-Abhängigkeit,
      direkt host-kompilierbar) über echten `ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()` mit
      Test-Secret verifiziert: ALT liefert einen 16-Byte-Frame (`arg_len=1`), NEU einen
      korrekten 20-Byte-Frame (`arg_len=5`), 7 `id`/`delay_ms`-Randfall-Kombinationen
      verlustfrei rundgelaufen, ein 1-Byte- und ein 0-Byte-Katalog-Eintrag als Regression
      ALT==NEU bestätigt — Fix (gezielter `UkfeRfCmdTrigger`-Sonderfall vor dem unveränderten
      generischen Pfad, nutzt die bereits vorhandene `ukfe_rf_make_trigger()`) in
      `app-improvements/lora-ukfe/catalog-trigger-arg-format-mismatch-fix.md`, Repro in
      `app-improvements/lora-ukfe/repro-catalog-trigger-arg-format-mismatch/repro.c`. Unter
      `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`: alle vier Szenarien
      bestanden, keine ASan-/UBSan-Meldungen. Der Nachbar-Eintrag `FN_PAYLOAD[1]`
      ("Payload-Run [V3]") hat dasselbe feste `arg=0` (kann praktisch nur Index 0 auslösen),
      aber kein Protokollfehler — bewusst nicht mitgefixt, als Bedienungs-Rückstellpunkt
      vermerkt. `lora-ukfe`-Code damit bis auf UI-seitige Vervollständigung (id-/delay_ms-Eingabe
      im Katalog-Menü, `Payload-Run`-Index-Auswahl) vollständig geprüft.
- [x] **RollJam**: den bereits vorgeschlagenen Puffer-Reset-Fix im aktuellen Stand von
      `G4MEOVER18/RollJam` bestätigt (`rolljam_capture_pause()`/`_resume()` bereits im
      Hauptzweig übernommen) und die gesamte App (623 Zeilen über `rolljam_app.c`/`.h`,
      `rolljam_rf.c`/`.h`, `rolljam_states.h`) erneut vollständig durchgesehen. Fund:
      `rj_draw_cb()` zeichnet auf dem `ROLLJAM_IDLE`- und dem `ROLLJAM_DONE`-Bildschirm je
      einen `elements_button_left(canvas, "Exit")`-Hinweis, aber `rj_input_cb()` wertet
      `InputKeyLeft` an keiner Stelle aus — einzig die separate Hardware-Back-Taste
      (`InputKeyBack`) löst tatsächlich `app->abort = true` aus. Der angezeigte Hinweis ist
      auf beiden Bildschirmen wirkungslos. Fix (neuer `InputKeyLeft`-Zweig direkt nach dem
      bestehenden `InputKeyBack`-Zweig, aktiv nur in `IDLE`/`DONE`, nach demselben Muster wie
      der bestehende zustandsabhängige `InputKeyRight`-Zweig) in
      `app-improvements/RollJam/exit-button-left-key-unhandled-fix.md`, Unified-Diff gegen die
      unveränderte Basis (`git apply --check` vorab gegen eine frische Kopie verifiziert, real
      angewendet und byte-für-byte abgeglichen — identisch) in
      `app-improvements/RollJam/exit-button-left-key-unhandled-fix/rolljam_app_c.patch`. Mit
      Host-Repro (reine Zustands-/Flag-Entscheidungslogik von `rj_input_cb()` nachgebaut, ALT
      wortgleich der aktuellen Funktion) unter ASan/UBSan verifiziert: 22 Prüfungen — Left-Taste
      in `IDLE`/`DONE` (ALT wirkungslos, NEU setzt `abort`), Regression Left-Taste in den vier
      übrigen Zuständen, Regression Back-/Right-Taste in allen sechs Zuständen, Regression
      Ok-Taste-Zustandsübergänge, Regression `InputTypeLong` — ALL CHECKS PASSED, in
      `app-improvements/RollJam/repro-exit-button-left-key-unhandled/repro.c`. RF-Layer
      (`rolljam_rf.c`: Jam-/Capture-/Replay-Callbacks, OTG-Power-Handling, Signalpuffer-Packing,
      `rolljam_replay_done()`) bei dieser Sitzung ebenfalls durchgesehen, kein weiterer Fund.

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
- [x] Katalog-Zeile **nRF24 Sniffer/Mousejack** (bislang vage "🔧 | Modul") geprüft: Original-
      Upstream `mothball187/flipperzero-nrf24` seit 2022 unverändert und ohne `application.fam`,
      relevante Quelle ist die von `xMasterX` weitergepflegte, manifest-aktuelle Fassung im
      RogueMaster-Baum (`nrf24sniff`/`nrf24mousejacker`); dabei vier zusätzliche, bislang nicht
      erfasste nRF24-Apps gefunden (`nrf24channelscanner` — Upstream inzwischen gelöscht,
      `nrf24scan`, `nrf24batch`, `nrf24monitor`). Alle nutzen denselben stabilen
      `furi_hal_spi_bus_handle_external`-Mechanismus, den ich beim CAN-Bus-Port bereits gegen
      mntm-012 verifiziert habe; ein GitHub-Issue im `Next-Flip/Momentum-Firmware`-Repo belegt
      reale Installation von `nrf24sniff.fap` auf Momentum via App-Katalog — kuratiert in
      `ideas/nrf24-sniffer-mousejack-api-status.md`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` präzisiert und um die vier Zusatz-Apps ergänzt.
- [x] Seit der ersten RogueMaster-CAN-Bus-Sitzung mitgeschleppter Rückstellpunkt **PicoPass/
      Seader** ("größerer Portaufwand wegen ASN.1/mbedTLS-Abhängigkeiten") endlich konkret
      geprüft statt weiter zurückgestellt: `applications/external` in
      `Next-Flip/Momentum-Firmware` ist ein Submodul auf `Next-Flip/Momentum-Apps.git`, beim Tag
      `mntm-012` exakt auf denselben Commit (`85b0cdb4e7dec…`) gepinnt, den ich bereits für den
      Subbrute-Fund verifiziert hatte — `picopass/` (App-ID `picopass`, v1.22, Autor `bettse`,
      Ziel `f7`, `asn1`/`loclass` als private Libs im eigenen App-Ordner mitgebaut, kein
      Firmware-Patch nötig) und `seader/` (eigenständige App, GPLv3, ebenfalls `bettse`) liegen
      dort bereits vollständig vor. `mntm-012`-Changelog belegt zusätzlich aktive
      Momentum-spezifische Pflege durch `bettse` selbst ("Removed wiegand plugin, support for
      changing CN during emulation"). PicoPass liest/schreibt/emuliert HID-iClass ohne
      Zusatzhardware über die eingebaute NFC-Antenne; Seader deckt zusätzlich iClass SE/
      Desfire EV1-EV2/Seos ab, benötigt dafür aber zwingend einen externen HID-SAM-Adapter
      (Fremdhardware, analog zum bereits zurückgestellten `can_commander`). Damit ist der
      unterstellte große Eigenportaufwand hinfällig — kuratiert in
      `ideas/picopass-seader-api-status.md`, Katalog in `ideas/pentest-app-katalog.md` um eigene
      PicoPass-/Seader-Zeilen ergänzt und Access-Audit-Zeile entsprechend verknüpft.
- [x] RogueMaster-Bündel-Durchsicht fortgesetzt: vollständige Verzeichnisliste von
      `applications/external` diesmal per Sparse-Checkout direkt gegen den echten Baum geprüft
      (762 App-Ordner, statt aus Sekundärquellen zu zitieren). Kandidat `badge_audit`
      (ViperDroid, NFC-Zutrittsmedien-Auditor mit Clone-Resistance-Score) geprüft, weil er stark
      an den bereits vorliegenden `access_audit`-Port-Vorschlag erinnert — Feature-für-Feature-
      Vergleich ergab vollständige Überschneidung (Access Audit deckt zusätzlich LF-RFID und
      HID-iCLASS ab, Badge Audit nur NFC), kein eigener Portaufwand gerechtfertigt — kuratiert in
      `ideas/badge-audit-app-status.md`, Katalog-Zeile Access Audit entsprechend ergänzt. Beim
      Durchsehen zusätzlich `keeloq_decryptor` (Sub-GHz-Keystore-Decryptor, entschlüsselt die
      firmwareeigene, verschlüsselte KeeLoq-Hersteller-Schlüsseldatenbank per Secure-Enclave-
      Schlüssel) bewusst **nicht** aufgenommen: extrahiert fremde Hersteller-Rolling-Code-
      Masterschlüssel statt eigene Geräte zu testen, passt nicht in den defensiven/autorisierten
      Rahmen aus CONSTRAINTS Punkt 5 — als Ablehnungsgrund hier dokumentiert, damit er bei
      künftigen Durchsichten nicht erneut aufgegriffen wird.
- [x] Mitgeschleppter Rückstellpunkt aus der RogueMaster-Bündel-Durchsicht (`subghz_jammer_detect`)
      konkret geprüft: eigentliche Quelle ist das eigenständige Repo `barkandbite/flipper_suite`
      (MIT, aktiv gepflegt, letzter Commit an diesem Pfad 2026-05-04). Die App nutzt durchgehend
      dieselbe SubGHz-Devices-Abstraktionsschicht (`subghz_devices_init/get_by_name(
      SUBGHZ_DEVICE_CC1101_INT_NAME)/begin/reset/idle/load_preset/set_frequency/set_rx/get_rssi/
      sleep/end/deinit`), die ich bei RollJam/RollForge bereits gegen mntm-012 verifiziert habe —
      erste **defensive** Gegenkategorie zu meinen bisher rein offensiven Jammern (RollForge/
      FlipperJammer): RSSI-Dauerträger-Erkennung über vier Standardbänder, ausschließlich
      eingebaute CC1101-Antenne, kein Firmware-Patch nötig — kuratiert in
      `ideas/subghz-jammer-detector-app-status.md`, Katalog-Zeile in `ideas/pentest-app-katalog.md`
      ergänzt. Geschwisterfund `rogue_ap_detector` (WiFi-Evil-Twin-Erkennung, zwingend externes
      ESP32-Devboard) sowie `h10301_writer` und der restliche NFC-Werkzeug-Cluster bleiben aus
      demselben Rückstellpunkt weiterhin offen.
- [x] Letzter aus dem `b8fe671`-Rückstellpunkt noch offene Kandidat (nach Rayhunter Client)
      geprüft: **UART Sniff** (`barkandbite/flipper_suite`, dieselbe Quelle) — passiver
      UART-Protokollsniffer über den Erweiterungsport, füllt eine bislang komplett fehlende
      Kategorie (passive serielle Bus-Analyse). API-87-Kompatibilität per `curl`-Rohquelltext
      gegen den eigenen `mntm-012`-Header-Stand verifiziert (`furi_hal_serial_control_*`,
      `furi_hal_serial_*`, `expansion_enable/disable` inkl. korrekter Aufrufreihenfolge) —
      identisch zu den bereits geprüften Geschwister-Apps, keine Abweichung. Quelltext-Review
      ohne Speicherzugriffsfehler; gefunden wurde stattdessen ein Zustandsfehler ohne
      Speicherbezug: die Zurück-Taste aus der Sniff-Ansicht ins Menü beendet die laufende
      Aufnahme nicht (nur vollständiges App-Beenden räumt Worker/ISR auf), wodurch zusätzlich
      eine zwischenzeitlich in den Settings geänderte Baudrate/Kanalwahl bei erneutem "Start
      Sniff" stillschweigend ignoriert wird — kuratiert inkl. Fix-Skizze für den eigenen Port in
      `ideas/uart-sniff-api-status.md`, Katalog um neue Kategorie „GPIO / Serial Bus" ergänzt.
      Aus demselben Rückstellpunkt weiterhin offen: `ble_scanner`, `evil_ble`.
- [x] Beide aus der SubGHz-Jammer-Detector-Sitzung zurückgestellten Rückstellpunkte aufgegriffen:
      **Rogue AP Detector** (dieselbe Quelle `barkandbite/flipper_suite`, WiFi-Evil-Twin-/
      Rogue-AP-Erkennung über externes ESP32-Marauder-Board) und **H10301 Writer**
      (`Zacvr/h10301_writer`, Serien-/Auto-Inkrement-Schreiben von HID-Prox-H10301-Credentials
      auf T5577). Beide gegen den eigenen `mntm-012`-Header-Stand (Momentum-Firmware-Tag lokal
      sparse ausgecheckt) verifiziert statt nur am Quelltext geschätzt: Rogue AP Detector nutzt
      exakt die vorhandenen `expansion_enable/disable`-, `furi_hal_serial_control_acquire/
      release`- und `furi_hal_serial_*`-Signaturen (Erweiterungsport-UART, wie vom eigenen
      README als API-87.1-Ziel angegeben); H10301 Writer nutzt exakt die vorhandene
      `lfrfid_worker_*`-/`protocol_dict_*`-API inkl. `LFRFIDProtocolH10301`-Enum-Eintrag — beide
      ✅ ohne Anpassungsbedarf. Kuratiert in
      `ideas/rogue-ap-detector-h10301-writer-api-status.md`, Katalogzeilen in
      `ideas/pentest-app-katalog.md` ergänzt (Rogue AP Detector unter WiFi, H10301 Writer unter
      Physical Access Control). Restlicher NFC-Werkzeug-Cluster aus demselben
      RogueMaster-Bündel-Rückstellpunkt weiterhin offen.
- [x] Restlichen NFC-Werkzeug-Cluster aufgegriffen: vollständige Liste aller 34 NFC/RFID-
      Ordner unter `applications/external` per Sparse-Checkout gezogen (Branch `420`, Commit
      `6a8a4863`). Kandidat `mfdesfire_auth` geprüft (Name/Beschreibung erinnerten an das offene
      MIFARE-Plus/Ultralight-AES-Vorhaben, betrifft aber die andere Kartenfamilie DESFire
      DES/2K3DES) — kompletten Quellbestand (520 Zeilen, 7 Dateien) gelesen: keine einzige
      NFC-HAL-Aufrufstelle im gesamten Repo, die beworbene "Legacy Authetication" zeigt nur eine
      3-Sekunden-Ladeanimation ohne Kartenzugriff, der zuvor eingegebene Schlüssel/IV wird nie
      gelesen — unfertiger UI-Platzhalter ohne Funktion, kein Port-Kandidat, siehe
      `ideas/mfdesfire-auth-app-status.md`. Restlicher Cluster (u. a. `nfc_sniffer`, `chameleon`,
      `nfc_relay`, `mifare_fuzzer`, `nfc_rfid_detector`, `nfctools`, `uhf_rfid`) weiterhin offen.
- [x] `uhf_rfid` (YRM100-/M100-UHF-RFID-Modul, Fremdhardware am Erweiterungsport) aus demselben
      Cluster aufgegriffen — reiner Quelltext-Check braucht die Hardware nicht. Den gesamten
      Nicht-Szenen-Kern (721 Zeilen, vier Dateien) gelesen: `uhf_buffer.c` implementiert den
      UART-Empfangspuffer als echten Ringpuffer (überschreibt bei Kapazitätsüberlauf das älteste
      Byte, schiebt `head` weiter), während der einzige Aufrufer (`uhf_module.c`,
      `setup_and_send_rx()` und fünf weitere Stellen) ihn durchgehend als einfachen linearen
      Block ab Index 0 behandelt — sobald eine einzelne Modul-Antwort die 250-Byte-Kapazität
      übersteigt (z. B. kombinierte TID+USER-Bank-Lesung oder Multi-Tag-Inventur), liest die App
      bis zu 49 Byte hinter das Ende der Heap-Allokation hinaus, mit AddressSanitizer als
      `heap-buffer-overflow` bestätigt; unterhalb der Kapazitätsgrenze (Regressionsfälle 40 B und
      exakt 250 B) fehlerfrei — Fix in
      `firmware-mining/uhf-rfid-ring-buffer-oob-read-fix.md` (Kapazitätsprüfung vor jedem
      Ringpuffer-Schreibzugriff, wodurch `head` für den real genutzten
      Sammle-bis-Reset-Anwendungsfall dauerhaft 0 bleibt). Expansion-/5V-OTG-Handling in
      `uhf_app.c` sowie `uhf_tag.c`/`uhf_worker.c` mitgeprüft, kein weiterer Fund. Szenen-Code
      (reine UI-Ablaufsteuerung) sowie restlicher Cluster (`chameleon`, `nfc_relay` — beide
      zwingend Fremd-/Zweitgerät; NFC-Tools-Schreib-Szenen/Passwort-Entfernung) weiterhin offen.
- [x] NFC-Tools-Cluster weiterverfolgt: **FeliCa Write Block**
      (`scenes/write/nfc_tools_scene_felica_write.c` + `_felica_write_input.c`, 358 Zeilen)
      vollständig gelesen. Eingabevalidierung (34-Zeichen-Format, Blockbereich 00–13) und
      Frame-Aufbau (32-Byte-WRITE-WITHOUT-ENCRYPTION-Rahmen, Puffergrößen exakt nachgerechnet)
      ohne Fund. Fund: `nfc_tools_felica_write_worker()` wartet einmalig 8 s auf das
      Poller-Ergebnis, ohne den Rückgabewert von `furi_event_flag_wait()` zu prüfen — anders als
      alle fünf übrigen Poller-/Schreib-Worker derselben App (`write_scan`, `format_tag`,
      `lock_tag`, `nfc_commands_run`, `set_password_write`, `remove_password_write`), die
      vorab eine eigene `NfcScanner`-Erkennungsphase mit geprüftem Timeout und der Meldung
      `NTS_ERR_NO_TAG` ("No tag detected") durchführen. FeliCa Write ist der einzige Schreibpfad
      ganz ohne diese Vorprüfung — hält der Nutzer innerhalb der 8 s keine Karte an die Antenne,
      läuft der Poller-Callback (einzige Quelle für `app->info_str`) nie, und die App zeigt
      "Failed" mit vollständig leerem Meldungstext statt einer Ursache. Mit Host-Repro
      (Zustandsmodell aller fünf Callback-Ausgänge inkl. Timeout) verifiziert: ALT liefert in 1
      von 5 Fällen eine leere Meldung, NEU (Wait-Rückgabewert geprüft, bei Timeout
      `NTS_ERR_NO_TAG` gesetzt) in 0 von 5, alle vier Callback-Fälle (Erfolg/Denied/Bad
      Response/Aktivierungsfehler) unverändert — Port-Vorschlag inkl. Fix in
      `firmware-mining/nfctools-felica-write-no-tag-timeout-blank-failure.md`. Als Nebenbefund
      notiert (nicht Teil dieses Fundes): dieselbe unbeobachtete zweite Wait-Rückgabe kommt auch
      in den fünf genannten Geschwisterfunktionen nach deren eigener Scanner-Phase vor, dort
      aber mit deutlich kleinerem Zeitfenster (Tag-Präsenz bereits vorab bestätigt) — für eine
      künftige Sitzung als eigener, kleinerer Rückstellpunkt vorgemerkt. Restlicher Cluster
      weiterhin offen: `chameleon`, `nfc_relay` (Fremd-/Zweitgerät), NFC-Tools-Passwort-
      Entfernung/Sektor-Analyse-Restumfang.
- [x] NFC-Tools-Cluster weiterverfolgt: den bei der FeliCa-Write-Sitzung vorgemerkten
      Rückstellpunkt aufgegriffen — dieselbe unbeobachtete zweite Wait-Rückgabe von
      `furi_event_flag_wait()` in den vier Geschwisterfunktionen geprüft, die (anders als
      `write_scan`/`format_tag`) eine zweite, poller-getriebene RF-Sitzung nach ihrer eigenen
      Scanner-Phase durchführen: `set_password_write`, `remove_password_write`,
      `nfc_commands_run`, `lock_tag` (zusammen ~1180 Zeilen, vollständig gelesen). Bestätigt: alle
      vier verwerfen den Rückgabewert identisch (`scenes/other/nfc_tools_scene_set_password_write.c:202`,
      `_remove_password_write.c:205`, `_nfc_commands_run.c:256`, `_lock_tag.c:179`) — läuft der
      jeweilige Poller-Callback innerhalb der 8 s kein einziges Mal (Karte nach erfolgreicher
      Scanner-Erkennung, aber vor dem ersten Ereignis der zweiten RF-Sitzung wieder verloren),
      bleiben `success`/`err[80]` des jeweils lokalen Kontexts bei ihren nullinitialisierten Werten
      stehen — dieselbe leere "Failed"-Meldung wie beim bereits dokumentierten FeliCa-Fund, hier mit
      kleinerem, aber realem Zeitfenster (Tag bereits einmal bestätigt erkannt). Mit generischem
      Host-Repro (fünf Szenarien, gemeinsamer `success`+`err[80]`-Kontext, identisch für alle vier
      Fundstellen) verifiziert: ALT 1 von 5 Fällen mit leerer Meldung, NEU 0 von 5, 0
      Regressionsabweichungen — Port-Vorschlag inkl. Fix (nutzt bewusst nicht `NTS_ERR_NO_TAG`,
      da diese Konstante hier bereits für den separaten Phase-1-Scanner-Timeout reserviert ist,
      sondern die ebenfalls bereits vorhandene, semantisch passendere `NTS_ERR_TAG_CONTACT`) in
      `firmware-mining/nfctools-second-wait-timeout-blank-failure.md`. Restlicher Cluster weiterhin
      offen: `chameleon`, `nfc_relay` (Fremd-/Zweitgerät), NFC-Tools-Passwort-Entfernung/
      Sektor-Analyse-Restumfang.
- [x] NFC-Tools-Cluster weiterverfolgt: den zuletzt offenen Rückstellpunkt "Sektor-Analyse-Restumfang"
      aufgegriffen — `scenes/other/nfc_tools_scene_sector_analysis.c` (389 Zeilen) sowie
      `core/nfc_tools_ntag.c` (123 Zeilen) vollständig gelesen, bisher nur zur Kontextprüfung eines
      anderen Fundes überflogen. Fund: `nfc_tools_sector_worker()` prüft für drei der vier
      Protokollzweige (ICODE, DESFire, FeliCa) das Ergebnis des jeweiligen Dump-/Lese-Helfers und
      springt bei Fehlschlag in den Fehlerpfad — der vierte, MF-Ultralight/NTAG-Zweig ruft
      `nfc_tools_ntag_dump()` auf, verwirft aber dessen `bool`-Rückgabewert. Schlägt der zugrunde
      liegende `mf_ultralight_poller_sync_read_card()`-Aufruf fehl (Tag während des Lesens vom Feld
      genommen), hängt die Funktion "Read error" an `info_str` an und kehrt vorzeitig zurück — der
      Worker spielt danach trotzdem unbedingt die Erfolgs-Notification (Ton+LED) und dasselbe
      `NfcToolsEventAnalysisDone` wie bei einem echten Erfolg, während der sichtbare Text bereits den
      Fehler zeigt: hörbare/haptische Rückmeldung widerspricht dem tatsächlichen Ergebnis, dieselbe
      Fundklasse wie der bereits dokumentierte FeliCa-Write-/Remove-Password-Reihenfolgen-Fund. Mit
      Host-Repro (Zustandsmodell beider Funktionen, Erfolgs- und Fehlschlagfall) verifiziert: ALT
      meldet in 1 von 2 Fällen die falsche Benachrichtigung, NEU (Rückgabewert geprüft, exakt nach
      Vorbild des bestehenden ICODE-Zweigs) in 0 von 2, Regressionsfall (Lesevorgang erfolgreich)
      unverändert — Port-Vorschlag inkl. Fix in
      `firmware-mining/nfctools-sector-analysis-ntag-dump-ignored-result.md`. Dabei
      `nfc_tools_felica_sector_cb()`/`nfc_tools_desfire_sector_cb()` sowie `nfc_tools_mfc_dump()`
      (`void`-Rückgabe hier korrekt, Funktion kann konstruktionsbedingt nie vollständig fehlschlagen)
      mitgeprüft, kein weiterer Fund. Damit ist der Rückstellpunkt "Sektor-Analyse-Restumfang"
      abgeschlossen; restlicher NFC-Werkzeug-Cluster nur noch `chameleon`/`nfc_relay`
      (Fremd-/Zweitgerät, außerhalb reiner Quelltextprüfung).
- [x] Mitgeschleppten Rückstellpunkt aus der Mifare-Fuzzer-Sitzung aufgegriffen: Fund 2
      (Endlosschleife bei "Load UIDs from file" ohne gültige Zeile) geliefert. Echte,
      unveränderte `mifare_fuzzer_scene_emulator.c` per `raw.githubusercontent.com` von
      `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`) erneut bezogen und gegen den
      bereits dokumentierten Fund gegengeprüft: unverändert vorhanden. Fix trägt exakt den
      bereits als `neu_select_line()` vorbereiteten Referenz-Kontrollfluss
      (`found_valid_line`-Merker: höchstens ein vollständiger Dateidurchlauf, danach sauberer
      Abbruch statt endlosem `stream_rewind()`) in die reale Funktion nach, ergänzt um Freigabe
      des bereits allozierten `nfc_data` sowie dieselbe `sequence_blink_stop`-Benachrichtigung
      wie der bestehende Stop-Zweig. Als Unified-Diff gegen eine frische Kopie der echten Quelle
      (`patch --dry-run -p1` vorab verifiziert, real angewendet und Byte-für-Byte gegen die
      editierte Fassung abgeglichen) in
      `firmware-mining/mifare-fuzzer-app-port/mifare_fuzzer_scene_emulator_c.patch`. Der bereits
      vorhandene Host-Repro (`firmware-mining/repro-mifare-fuzzer-load-uids-no-valid-line-hang/repro.c`)
      erneut unter ASan/UBSan gebaut und ausgeführt: ALT hängt bei leerer, reiner
      Kommentar- und ausschließlich falsch-langer Testdatei jeweils an der Iterationsobergrenze,
      NEU bricht sauber ab, Regressionsfall (gültige Datei) unverändert. Bewusst nicht
      mitgeliefert: das `is_attacking`-Flag der Emulator-View aktiv zurückzusetzen (lebt in einer
      zweiten Datei, `views/mifare_fuzzer_emulator.c`) — ohne diesen Fix bleibt der Angriff bei
      dauerhaft ungültiger Datei "aktiv" und der Abbruchzweig wird bei jedem Tick erneut sauber
      getroffen (kein Hang mehr, nur wiederholte Stop-Benachrichtigung) — für eine künftige
      Sitzung vorgemerkt. Details in `firmware-mining/mifare-fuzzer-app-port.md` (Risiko-/
      Port-Skizze-Abschnitt aktualisiert). Damit ist der Mifare-Fuzzer-Port-Vorschlag
      vollständig abgearbeitet (beide eigenen Funde dokumentiert bzw. gepatcht); restlicher
      NFC-Werkzeug-Cluster weiterhin nur `chameleon`/`nfc_relay` (Fremd-/Zweitgerät).
- [x] Aus der vorigen Mifare-Fuzzer-Sitzung vorgemerkten Fund-2-Folgepunkt aufgegriffen: das
      `is_attacking`-Flag der Emulator-View nach dem Fund-2-Abbruchzweig ("Load UIDs from file"
      ohne gültige Zeile) aktiv zurückgesetzt. Echte, unveränderte `views/mifare_fuzzer_emulator.c`/
      `.h` frisch von `raw.githubusercontent.com` (`RogueMaster/flipperzero-firmware-wPlugins`,
      Branch `420`) bezogen: das view-interne `is_attacking` (Emulator-Struct-Feld, vom Tick-Handler
      im Scene-Code direkt gelesen, und das separate, private `MifareFuzzerEmulatorModel::
      is_attacking`, das nur die Anzeige steuert) ließ sich vom Scene-Handler aus bislang nicht
      zurücksetzen — ohne Fix feuert der Tick-Handler bei laufendem Angriff gegen eine durchgehend
      ungültige Datei weiterhin regelmäßig `MifareFuzzerEventStartAttack`, trifft zwar den
      bestehenden, sauberen Abbruchzweig statt zu hängen, aber die UI bleibt dauerhaft auf
      "Stop"+Fortschrittsleiste stehen und meldet wiederholt "Stop" statt einmalig abzubrechen und
      "Start" zu zeigen. Fix: neue, öffentliche Setter-Funktion `mifare_fuzzer_emulator_stop_attacking()`
      in `views/mifare_fuzzer_emulator.c`/`.h` (zieht exakt das bereits vorhandene, korrekte
      Reset-Muster aus `mifare_fuzzer_emulator_enter_callback()` nach, nur als eigenständig
      aufrufbarer Setter statt nur intern beim View-Eintritt), vom Scene-Handler direkt im
      Abbruchzweig aufgerufen. Mit Host-Repro (Zustandsmodell aus Emulator-Flag, View-Model-Flag und
      Tick-Nachtrigger-Logik, 47 simulierte Ticks) verifiziert: ALT trifft den Abbruchzweig 10× statt
      1× und endet mit `model.is_attacking=true` ("Stop" hängen geblieben), NEU trifft ihn genau 1×
      und endet korrekt bei `model.is_attacking=false` ("Start") — als drei Unified-Diffs gegen die
      echte Quelle (zwei für die View-Dateien, einer für den bereits Fund-2-gepatchten Stand von
      `mifare_fuzzer_scene_emulator.c`) in `firmware-mining/mifare-fuzzer-app-port/
      mifare_fuzzer_emulator_c.patch` / `mifare_fuzzer_emulator_h.patch` /
      `mifare_fuzzer_scene_emulator_c_stop_attacking_followup.patch`, jeweils `patch --dry-run -p1`
      vorab verifiziert, real angewendet und Byte-für-Byte abgeglichen. Details in
      `firmware-mining/mifare-fuzzer-app-port.md` aktualisiert. Damit ist auch dieser zuletzt
      vorgemerkte Rückstellpunkt abgeschlossen; restlicher NFC-Werkzeug-Cluster weiterhin nur
      `chameleon`/`nfc_relay` (Fremd-/Zweitgerät).
- [x] Den in der MIFARE-Plus-Phase-2-Sitzung zurückgestellten Punkt "SL0-vs-SL3-Erkennung"
      aufgegriffen: Unleasheds Erweiterung von `mf_plus_get_type_from_version()`/
      `mf_plus_get_type_from_iso4()` (`lib/nfc/protocols/mf_plus/mf_plus_i.c`) geprüft — dabei
      stellt sich heraus, dass die eigentliche SL0-vs-SL3-Unterscheidung selbst keine reine
      Klassifizierung auf bereits vorhandenen Daten ist, wie ursprünglich vermutet, sondern einen
      neuen aktiven Poller-Befehl braucht (`mf_plus_poller_probe_security_level()`, echtes
      Karten-I/O) — fachlich Phase-3-Poller-Gebiet, hardware-blockiert wie der Rest von Phase 3-6.
      Bewusst auf den tatsächlich host-testbaren Teil derselben Unleashed-Änderung begrenzt:
      ATS-Produkt-Abgleich von einer festen 4-Werte-Tabelle auf Nibble-Matching umgestellt (erkennt
      zwei zusätzliche, bei mir bislang abgelehnte MIFARE-Plus-SE-Konfigurationsvarianten aus der
      NXP-TagInfo-Fingerabdrucktabelle), ein ATQA-basierter Größen-Fallback für ein unbekanntes
      GetVersion-Storage-Byte ergänzt, sowie zwei eigenständige Korrektheitsfehler in meiner
      bisherigen SAK-Auswertung behoben: `mf_plus_get_type_from_version()` bildete SAK `0x10`/
      `0x11` (laut AN10833 Security Level 2) über ein reines Zweiweg-`if/else` fälschlich auf SL1
      ab (jetzt korrekt SL2, ein nicht erkannter SAK-Wert liefert jetzt `Unknown` statt geratenem
      SL1), und `mf_plus_get_type_from_iso4()` meldete für SAK `0x10`/`0x11` hartkodiert
      `MfPlusTypeX` statt des durch die bloße SAK allein tatsächlich belegten generischen
      `MfPlusTypePlus`. SAK `0x20` bleibt bei mir bewusst weiterhin unbedingt SL3 (unverändert) —
      keine SL0-Behauptung ohne den nicht mitportierten Probe-Befehl. Als Unified-Diff gegen meine
      `mntm-012`-Basis (`patch --dry-run -p0` gegen eine frische Basiskopie vorab verifiziert, real
      angewendet und Byte-für-Byte gegen die host-getestete Fassung abgeglichen — identisch) in
      `firmware-mining/mfplus-sl0-sl3-detection-port/mf_plus_i.c.patch`, mit sieben Host-
      Testgruppen (neue TagInfo-SE-Varianten, DESFire-/Nicht-Plus-Ablehnung, ATQA-Größen-Fallback,
      SAK-Security-Level-Korrektur, generischer-Plus-Typ, SAK-0x20-SL0-Nichtregression, volle
      Regressionsmatrix über die ursprüngliche Referenztabelle) unter ASan/UBSan verifiziert
      (`ALL CHECKS PASSED`, 101 von 101 Checks) in
      `firmware-mining/repro-mfplus-sl0-sl3-detection-port/repro.c` — Details inkl. ehrlicher
      Scope-Begründung und Testhinweis in `firmware-mining/mfplus-sl0-sl3-detection-port.md`. Damit
      ist der Backlog-Punkt "SL0-vs-SL3-Erkennung" zum host-testbaren Teil abgeschlossen; die
      eigentliche SL0-Erkennung selbst bleibt zusammen mit Phase 3-6 des MIFARE-Plus-/
      Ultralight-AES-Ports offen (braucht echte Hardware).
- [x] Offenen Punkt "Unleashed dev seit unlshd-093" aufgegriffen: die zuletzt notierten sieben
      Kandidaten-Hashes (`c873eb5`, `7518dc7`, `a534a72`, `97e088e`, `6c83a67`, `101c20d`,
      `9148d50`) waren fehlerhaft vorgemerkt — alle sieben sind bereits Vorfahren von `unlshd-093`
      (per `git merge-base --is-ancestor <hash> unlshd-093` einzeln bestätigt, Commit-Datumsbereich
      November 2025 bis August 2026), haben mit dem tatsächlichen `dev`-Fenster seit `unlshd-093`
      nichts zu tun. Stichprobe `c873eb5` ("Fix Ultralight EV1 regression") inhaltlich
      gegengeprüft: der Fix ist bei mir bereits identisch vorhanden, kein Portaufwand. Das
      tatsächliche `dev`-Fenster seit `unlshd-093` frisch selbst ermittelt (`git log --oneline
      unlshd-093..origin/dev`, nur 12 Commits statt der zuvor behaupteten 156): `370ca57bc`
      (Toolbox SimpleArray-Gleichheitslänge), `7db0c26d0` (MIFARE-Classic-Dictionary-Angriff-
      Abbruch bei voll gelesener Karte — als erster Fund dieser Sitzung bearbeitet, siehe unten),
      drei reine "bump apps tag"-Commits sowie `30a1bead8`/`0f09492cd`/`9819755ac`
      (Formatierung/Changelog/Issue-Template, kein Portinhalt), `37e7db36f` ("sub hertz", noch
      inhaltlich zu prüfen), `49238e1bd`/`22ce1d9ae`/`946a52a64` (RFID-Schreibfunktionen für
      EM4100-auf-ID8268/Hitag-S-Klonchips bzw. Chiptyp-Auswahl beim Schreiben, thematisch
      LF-RFID statt NFC, für eine künftige Sitzung vorgemerkt). Ersetzt den bisherigen,
      fehlerhaften Kandidaten-Hinweis vollständig.
- [x] Aus dem soeben korrigierten `dev`-Fenster (seit `unlshd-093`) ersten inhaltlichen Fund
      bearbeitet: `7db0c26d0` (PR #1151, "NFC: stop the MIFARE Classic dictionary attack once the
      card is fully read") — bei mir (mntm-012) läuft der Wörterbuch-Angriff bei
      sektorübergreifend wiederverwendeten Schlüsseln (Werks-/Transit-/Zutrittskarten mit einem
      Schlüssel für mehrere Sektoren) über eine bereits vollständig gelöste Karte hinaus weiter:
      der Sektor-Cursor fragt weiterhin jeden restlichen Sektor gegen das komplette Wörterbuch ab,
      und die Szene startet anschließend unnötig die nächste Wörterbuch-Phase (CUID→User→System)
      statt direkt zu „Read Success" zu springen. Trotz deutlich anderer NFC-App-Architektur bei
      mir (eigenständige `nfc_scene_mf_classic_dict_attack.c` statt Unleasheds neuerem
      `mf_classic_extra_scenes.c`) ist die zugrunde liegende Poller-Zustandsmaschine sowie die
      Drei-Phasen-Wörterbuchlogik der Szene strukturell identisch — direkte Portierung derselben
      Guard-Bedingung (`mf_classic_poller_is_card_read()` vor jeder neuen Schlüsselanfrage bzw. vor
      dem Nested-Einstieg, `card_read`-Prüfung vor jedem Phasenwechsel in der Szene) auf meine
      eigene Dateilage. Mit Host-Repro (drei Ebenen: Referenzsemantik, Poller-Abfragezähler,
      Szenen-Phasenwechsel-Zähler, jeweils ALT vs. NEU inkl. Regressionsfälle) verifiziert: ALT
      320 statt NEU 40 Wörterbuch-Abfragen im Wiederverwendungsfall, ALT startet in allen drei
      geprüften Fällen eine unnötige Folgephase, NEU nicht — 14/14 Checks bestanden, keine ASan-/
      UBSan-Meldungen. Zwei Unified-Diffs gegen meine `mntm-012`-Basis (`patch -p1 --dry-run`
      vorab verifiziert, real angewendet und Byte-für-Byte abgeglichen) in
      `firmware-mining/mf-classic-dict-attack-stop-on-full-read-port/` — Details inkl. der oben
      genannten Backlog-Korrektur in
      `firmware-mining/mf-classic-dict-attack-stop-on-full-read-port.md`.

- [x] Unleashed `dev` seit `unlshd-093`, letzten offenen SubGHz-Kandidaten aus der Restliste
      aufgegriffen: `37e7db36f` ("sub hertz") geprüft — behandelt zwei KeeLoq-Hersteller,
      **Monarch** (bereits vorhanden) und **KEY** (neu). Monarch-Teil bearbeitet: mein
      hartkodierter Diskriminator `0x100` erkennt einen zweiten, real vorkommenden Diskriminator
      (`0x280`, u. a. Dorcas 22366CLN) beim Empfang nicht — fällt aktuell nur zufällig über einen
      eigentlich unabhängigen generischen Rückfallzweig auf „Unknown" statt korrekt erkannt zu
      werden — und vergisst den tatsächlich empfangenen Wert beim Neusenden/Speichern ohnehin
      (immer `0x100`). Port-Vorschlag (neue Prüffunktion, seed-basiertes Merken/Neuerzeugen,
      erweiterte Serialize-Bedingung) in `firmware-mining/keeloq-monarch-discriminator-fix.md`,
      Unified-Diff in `firmware-mining/keeloq-monarch-discriminator-fix/keeloq_c.patch`
      (`patch -p1 --dry-run` vorab gegen frische Basiskopie verifiziert, real angewendet und
      Byte-für-Byte abgeglichen), Host-Repro (Erkennungs-Sweep, Neukodierungs- und
      Speichern/Laden-Rundlauf, BFT-Regression) unter ASan/UBSan in
      `firmware-mining/repro-keeloq-monarch-discriminator-fix/repro.c`: 5391/5391 Prüfungen
      bestanden. KEY-Teil bewusst zurückgestellt: neuer Hersteller, dessen Schlüsselmaterial in
      der separat ausgelieferten, verschlüsselten `keeloq_mfcodes`-Ressource liegen müsste, die
      mir aus dem Quellcode-Repo heraus nicht vorliegt (derselbe Grund wie beim
      Superrollo/GW60-Fund) — für eine künftige Sitzung vorgemerkt, erst lokal prüfen ob `"KEY"`
      in der eigenen Ressource existiert. Damit ist die unlshd-093-Restliste
      (`37e7db36f`/"sub hertz") abgearbeitet; aus demselben `dev`-Fenster weiterhin offen:
      `49238e1bd`/`22ce1d9ae`/`946a52a64` (LF-RFID-Schreibfunktionen), `370ca57bc` (Toolbox
      SimpleArray).
- [x] Letzten offenen Kandidaten aus dem unlshd-093-`dev`-Fenster aufgegriffen: `370ca57bc`
      (PR #1107, "Toolbox: fix SimpleArray equality length") geprüft — `simple_array_is_equal()`
      vergleicht per `memcmp()` nur `other->count` Byte (Elementanzahl) statt
      `other->count * type_size` (tatsächliche Puffergröße); bei mir für alle Byte-Array-Nutzer
      (`type_size == 1`: `iso15693_3`s `block_data`/`block_security`, `type_4_tag`s `ndef_data`)
      zufällig korrekt, für jeden Nutzer mit größerem Elementtyp (`FelicaSystem`,
      `MfDesfireApplication`, `MfDesfireFileData`) vergleicht der Aufruf nur einen Bruchteil des
      tatsächlichen Speichers. Die naheliegende Bytelängen-Korrektur allein wäre eine neue
      Regression: `FelicaSystem`/`MfDesfireApplication`/`MfDesfireFileData` besitzen selbst
      Zeiger auf eigene heap-allozierte Unterlisten, ein jetzt vollständiger `memcmp()` verglicht
      dann diese Zeiger statt der Daten — zwei inhaltlich identische, unabhängig geladene Karten
      würden fälschlich als „verschieden" gelten (laut Unleashed-Changelog genau der Auslöser für
      unnötige FeliCa-Shadow-Dateien bei jeder Emulation). Port-Vorschlag übernimmt deshalb beide
      Teile: die Bytelängen-Korrektur plus je eine neue elementweise Vergleichsfunktion für
      `felica_is_equal()`/`mf_desfire_is_equal()`, die für ihre besitzenden Elementtypen
      `simple_array_is_equal()` durch eine strukturbewusste Alternative ersetzen (rekursiv wieder
      `simple_array_is_equal()` auf die jeweils reinen Unterlisten anwendend). Bei mir gegen
      `mntm-012` als strukturell identisch bestätigt (`FelicaSystem` bei mir ohne das neuere
      `key_version`-Feld, entsprechend in der Portierung berücksichtigt). Unified-Diff (acht
      Stellen in fünf Dateien, `patch -p1 --dry-run` vorab gegen frische Basiskopie verifiziert,
      real angewendet und Byte-für-Byte abgeglichen) in
      `firmware-mining/simple-array-equal-element-length-fix/simple_array_equal.patch`,
      Host-Repro (Byte-Element-Regression, breite Skalar-Elemente mit Unterlängen-Nachweis,
      besitzende Elemente mit Positiv-/drei Negativproben, Datei-Elemente) unter ASan/UBSan in
      `firmware-mining/repro-simple-array-equal-element-length-fix/repro.c`: 4744/4744 Prüfungen
      bestanden. Details in `firmware-mining/simple-array-equal-element-length-fix.md`. Damit ist
      das gesamte unlshd-093-`dev`-Fenster bis auf die LF-RFID-Schreibfunktionen
      (`49238e1bd`/`22ce1d9ae`/`946a52a64`) abgearbeitet.

- [x] Offenen Punkt "Unleashed dev seit unlshd-093" abschließend aufgegriffen: letzten verbleibenden
      Kandidaten `946a52a64` ("merge some rfid related PRs") inhaltlich geprüft statt nur pauschal als
      "LF-RFID-Schreibfunktionen" vorgemerkt zu lassen — der Commit bündelt tatsächlich drei getrennte
      PRs, davon zwei nicht host-testbar (Timer-Capture-Ursprung in `furi_hal_rfid.c`, reiner
      Hardware-Fix; Dolphin-XP-Kosmetik in `nfc_app.c`) und einen dritten, klar host-testbaren:
      OFW PR 4449, "Keri tags now read correctly" (`lib/lfrfid/protocols/protocol_keri.c`). Bei mir
      (mntm-012) liegt die Datei byte-identisch zum Unleashed-Vorher-Stand vor. Fund: mein bestehender
      Decoder versucht den dokumentierten PSK-Skew-Hardwarefehler (Keri-Rahmen nie DC-balanciert, ein
      Pegel liest systematisch ~135 µs zu lang, der andere zu kurz) mit einer einzigen, richtungsfesten
      ±120-µs-Korrektur auszugleichen — trifft die Korrekturrichtung zufällig die tatsächliche
      Skew-Richtung, gleicht sie sie aus, trifft sie die Gegenrichtung, verstärkt sie den Fehler auf
      eine volle Bitperiode und der betroffene Lauf wird durchgehend falsch decodiert (deckt sich mit
      dem Unleashed-Hinweis "no genuine Keri card read at all"). Zusätzlich fehlt mir jede
      Zwei-Rahmen-Konsistenzprüfung (Puffer mit 104 Bit zu klein, um die Nutzlast des zweiten Rahmens
      überhaupt zu erreichen) — bei einem Protokoll ohne Parität/Prüfsumme die einzige verfügbare
      Integritätsprüfung. Port-Vorschlag (Puffer auf 128 Bit vergrößert, zwei statt einer
      Korrekturrichtung parallel versucht, neue Zwei-Rahmen-Übereinstimmungsprüfung) in
      `firmware-mining/keri-lfrfid-skew-and-frame-check-port.md`, Unified-Diff (`patch -p1 --dry-run`
      vorab gegen frische Basiskopie verifiziert, real angewendet und Byte-für-Byte gegen den echten
      Unleashed-Fix abgeglichen — identisch) in
      `firmware-mining/keri-lfrfid-skew-and-frame-check-port/protocol_keri_c.patch`. Host-Repro
      (beide Decoder-Fassungen wortgleich transkribiert, synthetischer Zwei-Rahmen-Keri-Strom mit
      pegelabhängigem Versatz in beiden Richtungen plus direkter Unit-Test der neuen
      Zwei-Rahmen-Prüfung) unter ASan/UBSan in
      `firmware-mining/repro-keri-lfrfid-skew-and-frame-check-port/repro.c`: 9/9 Prüfungen bestanden.
      Bewusst nicht mitportiert: `furi_hal_rfid.c`-Timing-Fix und `nfc_app.c`-Dolphin-XP-Kosmetik aus
      demselben Commit sowie die beiden Geschwister-Commits `22ce1d9ae` (Chiptyp-Auswahl beim
      LF-RFID-Schreiben) und `49238e1bd` (EM4100-auf-ID8268-/Hitag-S-Klonchip-Schreibpfad) — für eine
      künftige Sitzung vorgemerkt. Damit ist das gesamte Unleashed-`dev`-Fenster seit `unlshd-093` bis
      auf diese beiden verbleibenden Schreibfunktions-Kandidaten abgearbeitet. Beide inhaltlich
      geprüft (frischer Klon von `DarkFlippers/unleashed-firmware`, `dev`): keine reinen
      "LF-RFID-Schreibfunktionen", sondern ein größeres, mehrfach review-getriebenes Feature
      (`22ce1d9ae` bringt eine eigene Plugin-/`.fal`-Infrastruktur samt neuem Settings-Menü für die
      Chip-Auswahl beim Schreiben mit; `49238e1bd` baut auf genau dieser Infrastruktur auf) — für den
      hier üblichen Umfang eines einzelnen Sitzungsbeitrags zu groß, für eine künftige, eigens dafür
      vorgesehene Sitzung vorgemerkt statt überstürzt in Teilen zu portieren.
- [x] Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer Ergänzung für die
      bislang nur einen Eintrag (UART Sniff) enthaltende Katalog-Kategorie "GPIO / Serial Bus"
      gesucht. Fund: **GPIO Logic Analyzer** (g3gg0, aktiv weitergepflegt u. a. über einen
      `ecopsychologer`-Fork) — 8-Kanal-Logikanalysator über einen zweiten USB-CDC-Kanal, gibt
      sich per SUMP-Protokoll gegenüber PulseView/sigrok als "Openbench Logic Sniffer" aus.
      API-87-Status gegen den eigenen `mntm-012`-Header-Stand verifiziert (frischer,
      blobloser Sparse-Checkout): alle acht referenzierten Erweiterungsport-Pins
      (`gpio_ext_pc0/pc1/pc3/pb2/pb3/pa4/pa6/pa7`) sowie das USB-CDC-Zweitkanal-Muster
      (`furi_hal_usb_set_config()`/`usb_cdc_single`/`_dual` aus `cli_vcp.c`,
      `furi_hal_cdc_set_callbacks()`, `furi_hal_usb_unlock()`, `FuriApiLock`) identisch zu UART
      Sniffs bereits verifizierter Basis vorhanden; die eigentliche Signalerfassung liest nur
      STM32-Rohregister (`GPIOx->IDR`), ganz ohne Furi-HAL-Abhängigkeit. Dabei eine für den
      PulseView-Einsatz relevante Diskrepanz gefunden: `sump_handle_get_metadata()` meldet
      PulseView unbedingt eine feste 10-MHz-Abtastrate, die tatsächliche Erfassungsschleife
      tastet aber rein ereignisbasiert (bei jeder Pegeländerung ein Sample) statt im
      Zeitraster ab — von der eigenen README bereits ehrlich als offene Einschränkung benannt,
      macht die in PulseView angezeigte Zeitachse für Frequenz-/Baudraten-Messung unzuverlässig,
      die reine Flankenreihenfolge bleibt aber verwertbar. Kein Speicherzugriffsfehler gefunden.
      Kuratiert in `ideas/gpio-logic-analyzer-app-status.md`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` ergänzt. Verwandter, noch nicht geprüfter Kandidat aus
      derselben Suche: `NaejEL/flipperzero-i2ctools` (dedizierter I2C-Scanner/Sniffer/Sender)
      für eine künftige Sitzung vorgemerkt.

## App-Verbesserungen
- [x] lora-ukfe: den in `catalog-trigger-arg-format-mismatch-fix.md` als "Offen" vermerkten
      Rückstellpunkt aufgegriffen — der Katalog-Menüpunkt "Trigger [V3]" ließ sich im Menü nur mit
      fest verdrahtetem `arg=0` auslösen (weder Payload-ID noch Verzögerungswert abfragbar). Dabei
      festgestellt: `scenes/scenes.h` reserviert bereits eine eigene Scene-ID dafür
      (`UkfeSceneTriggerConfirm`), die `scene_handlers.c` bislang nur als Alias auf
      `scene_status_on_enter/_on_event/_on_exit` verdrahtet — nie eine eigene Handler-Datei bekommen
      hat. Neue Scene `scene_trigger_confirm.c` gebaut: Zwei-Schritt-Zahleneingabe (Payload-ID 0-255,
      danach Delay in ms 0-300000) über das Flipper-Standardmodul `gui/modules/number_input.h`
      (bislang in dieser App nicht eingebunden), sendet danach direkt über `ukfe_rf_make_trigger()` +
      `ukfe_uart_send_rf()` einen korrekten 5-Byte-Frame — denselben Builder, den der dort
      dokumentierte Wire-Format-Fix für `send_fn()`s generischen Pfad vorschlägt. Da der
      Katalog-Eintrag jetzt vor `send_fn()` abgefangen wird, läuft er nie mehr über dessen generischen
      1-Byte-Pfad — der dort vorgeschlagene `send_fn()`-Sonderfall wird für diesen Eintrag durch
      dieses Feature überflüssig. `scene_menu.c`/`menu_callback()` bekommt dafür einen gezielten
      Sonderfall (`cmd == UkfeRfCmdTrigger && has_arg` → neue Scene statt `send_fn()`), alle anderen
      Katalog-Einträge bleiben unverändert. Back während der Delay-Eingabe geht einen Schritt zurück
      zur ID-Eingabe (Wert bleibt vorbelegt und überschreibbar) statt die ganze Scene zu verwerfen.
      Mit Host-Repro (kompiliert die echte, unveränderte `scene_trigger_confirm.c` dieser Sitzung
      direkt mit, keine Transkription, zusammen mit dem ebenfalls unveränderten `rf/ukfe_rf.c/.h`;
      nur `lora_ukfe.h`/`scenes.h` sind schlanke Mock-Header für die sonst furi-/gui-abhängigen Typen)
      unter ASan/UBSan verifiziert: Normalablauf inkl. vollem Wire-Rundlauf über
      `ukfe_rf_build_frame()`/`_parse_frame()`, Back-Korrektur (Wert bleibt vorbelegt UND ist
      überschreibbar, gesendeter Frame trägt den korrigierten statt des verworfenen Werts), Back im
      ID-Schritt bleibt unkonsumiert (Standardnavigation greift), sechs ID/Delay-Randfälle (0/255,
      0/300000-Obergrenze) — 4/4 Szenarien bestanden, keine ASan-/UBSan-Meldungen. Unified-Diff gegen
      die unveränderte `G4MEOVER18/lora-ukfe`-Basis (`git apply --check` vorab gegen eine frische
      Kopie verifiziert, real angewendet und byte-für-byte abgeglichen — identisch) in
      `app-improvements/lora-ukfe/trigger-catalog-number-input-port/trigger_catalog_number_input.patch`,
      Host-Repro in `app-improvements/lora-ukfe/repro-trigger-catalog-number-input/repro.c`, Details
      in `app-improvements/lora-ukfe/trigger-catalog-number-input-port.md`. Bewusst nicht mitgeliefert:
      `scene_payload_list.c` bekommt weiterhin keine eigene Delay-ms-Eingabe (separater, bereits
      funktionierender JSON-Pfad über `ukfe_uart_send_trigger()`, sendet unverändert `delay_ms=0`) —
      unabhängige Erweiterung eines anderen Menüpfads, nicht Teil dieses auf den Katalog-Eintrag
      begrenzten Fixes. Der Geschwister-Rückstellpunkt "Payload-Run [V3]" (`arg` ebenfalls fest auf 0,
      aber kein Protokollfehler wie beim Trigger-Fund) bleibt wie in
      `catalog-trigger-arg-format-mismatch-fix.md` vermerkt offen.
- [x] lora-ukfe: den zuvor zurückgestellten Geschwister-Rückstellpunkt "Payload-Run [V3]"
      aufgegriffen — auch dieser Katalog-Eintrag (`FN_PAYLOAD[1]`) hatte `arg` fest auf `0`,
      erreichbar war also nur Payload-Index 0. Anders als beim Trigger-Fund kein
      Protokollfehler (`UkfeRfCmdPayloadRun` braucht laut `rf/ukfe_rf.h:27` bereits nur 1
      Arg-Byte, das passt zum generischen Pfad) — reine Bedienungslücke. Dieselbe Lösung wie
      beim Trigger-Fund portiert, nur einstufig: neue Scene `scene_payload_run_confirm.c`
      fragt einen Payload-Index (0–255) über die bereits vom Trigger-Fix eingebundene
      `NumberInput`-View ab und sendet über denselben generischen 1-Byte-Pfad, den `send_fn()`
      für diesen Eintrag ohnehin nutzt (`cmd=UkfeRfCmdPayloadRun`, `arg_len=1`,
      `args[0]=idx`) — keine neue Kodierlogik. `scene_menu.c`/`menu_callback()` bekommt dafür
      einen zweiten gezielten `else if`-Zweig neben dem bestehenden Trigger-Zweig; alle
      anderen Katalog-Einträge bleiben unverändert. Neue Scene-ID
      `UkfeScenePayloadRunConfirm` in `scenes.h`/`scene_handlers.c` ergänzt. Back wird nicht
      konsumiert (kein Zwischenzustand wie beim zweistufigen Trigger-Ablauf, nur ein
      Eingabeschritt) — Standardnavigation zurück zum Katalog-Menü greift direkt. Mit
      Host-Repro (kompiliert die echte, unveränderte `scene_payload_run_confirm.c` dieser
      Sitzung direkt mit, zusammen mit dem unveränderten `rf/ukfe_rf.c/.h`) unter ASan/UBSan
      verifiziert: Normalablauf inkl. vollem Wire-Rundlauf über
      `ukfe_rf_build_frame()`/`_parse_frame()`, Back bleibt unkonsumiert, sechs
      Index-Randfälle (0, 1, 42, 128, 254, 255), wiederholter Scene-Aufruf setzt Callback/
      Vorbelegung unabhängig vom vorherigen Durchlauf komplett neu — 4/4 Szenarien bestanden,
      keine ASan-/UBSan-Meldungen. Patch setzt auf dem bereits vorgeschlagenen
      `trigger_catalog_number_input.patch` auf (wiederverwendet dessen `NumberInput`-
      Infrastruktur, lokal in dieser Reihenfolge anzuwenden) — `git apply --check` beider
      Patches nacheinander gegen eine frische Kopie der unveränderten
      `G4MEOVER18/lora-ukfe`-Basis (`0dba410a`) verifiziert, real angewendet und
      byte-für-byte abgeglichen — identisch. Unified-Diff in
      `app-improvements/lora-ukfe/payload-run-catalog-number-input-port/payload_run_catalog_number_input.patch`,
      Host-Repro in
      `app-improvements/lora-ukfe/repro-payload-run-catalog-number-input/repro.c`, Details in
      `app-improvements/lora-ukfe/payload-run-catalog-number-input-port.md`. Bewusst nicht
      mitgeliefert: keine Namensauswahl aus der vom Hub gemeldeten Payload-Liste
      (`app->payloads[]`) — bliebe reiner Zahlenwert, da unklar ist, ob das Protokoll-`idx`
      dieselbe Zählung wie das JSON-`id`-Feld verwendet (nur mit Einblick in die
      Hub-Firmware sicher klärbar, nicht rein aus diesem Flipper-Quelltext).

## Firmware-Feature-Mining (Fortsetzung)
- [x] Den bei `8fdb70dd3` (PR #1112) zurückgestellten "restlichen Umfang" (siehe oben, Zeile
      ~1048) aufgegriffen und den ersten der fünf offenen Teilfunde bearbeitet: **Pressing Stop
      stoppt den Mouse-Jiggler-Timer nicht wirklich**, `hid_mouse_jiggler_input_callback()` in
      `applications/system/hid_app/views/hid_mouse_jiggler.c` (nicht-stealth Variante) —
      `furi_timer_stop()`/`_start()` werden ausschließlich im "jetzt läuft"-Zweig aufgerufen, der
      umgekehrte Übergang (Stop-Druck) fasst den periodischen `FuriTimer` gar nicht an; er bleibt
      im zuletzt gewählten Intervall bewaffnet und feuert No-op-Ticks (Lock-Aufnahme +
      `model->running`-Prüfung, sonst nichts), solange die View geöffnet bleibt, obwohl die UI
      "Start" (gestoppt) anzeigt. Bei mir (`mntm-012`) byteidentisch zum Unleashed-Vorher-Stand.
      Mit Host-Repro (OK-Druck-Zweige 1:1 nachgebaut, Mock-Timer mit `armed`-Flag statt der
      opaken `furi_timer_*`-HAL-Aufrufe, vier Gruppen: ALT bleibt nach Stop dauerhaft bewaffnet
      und bedient 1000/1000 weitere Ticks, NEU bedient 0/1000, 5000-Presses-Regression bestätigt
      identisches `running`-Umschaltverhalten UND durchgehend `armed == running` bei NEU, ALT
      bleibt nicht nur für einen Tick hängen sondern dauerhaft) unter ASan/UBSan verifiziert:
      ALLE CHECKS BESTANDEN. Minimale Änderung (unbedingter `furi_timer_stop()`-Aufruf vor der
      bestehenden `if(model->running)`-Verzweigung, kein neues Nebenläufigkeitsmuster, bewusst
      nicht Unleasheds volle Umstrukturierung mit Auslagerung der Timer-Aufrufe aus dem Lock
      übernommen, da das dortige Wettlauf-Risiko einen selbst-neubewaffnenden One-Shot-Timer
      voraussetzt, den dieser periodische, nicht-stealth Jiggler nicht hat) gegen meine
      unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab verifiziert, real angewendet und
      Byte-für-Byte abgeglichen — identisch) in
      `firmware-mining/hid-jiggler-stop-does-not-stop-timer-fix/hid_mouse_jiggler_c.patch`,
      Host-Repro in
      `firmware-mining/repro-hid-jiggler-stop-does-not-stop-timer-fix/repro.c`, Details in
      `firmware-mining/hid-jiggler-stop-does-not-stop-timer-fix.md`. Restlicher Umfang von
      `8fdb70dd3` weiterhin offen: Verlassen der Ansicht setzt `running` nicht zurück (führt beim
      erneuten Öffnen zu fälschlich hervorgehobenem "Stop"), Stealth-Neubewaffnung-Race,
      Stealth-Bewegung kann 0 sein, Jiggler wartet über BLE nicht auf die Verbindung vor dem
      Senden — für künftige Sitzungen vorgemerkt.
- [x] Den nächsten der vier verbliebenen Teilfunde aus `8fdb70dd3` (PR #1112) bearbeitet:
      **Verlassen der Mouse-Jiggler-View setzt `model->running` nicht zurück**,
      `hid_mouse_jiggler_exit_callback()` in
      `applications/system/hid_app/views/hid_mouse_jiggler.c` (nicht-stealth Variante) —
      stoppt korrekt den `FuriTimer`, fasst `model->running` aber nicht an; da das View-Model
      erst bei `hid_mouse_jiggler_free()` (App-Ende) freigegeben wird, nicht beim Verlassen der
      View, bleibt `running == true` stehen, wenn beim Verlassen gerade gejiggelt wurde. Beim
      erneuten Betreten zeichnet `hid_mouse_jiggler_draw_callback()` sofort wieder die
      hervorgehobene "Stop"-Box, obwohl der Timer bereits sauber gestoppt ist — ein OK-Druck
      kippt den Toggle dann nur auf "Start" statt tatsächlich zu starten, erst ein zweiter Druck
      startet. Bei mir (`mntm-012`) byteidentisch zum Unleashed-Vorher-Stand. Unabhängig vom
      bereits gelieferten Stop-Timer-Fix (andere Funktion, beide Patches gegen dieselbe
      unveränderte Basis unabhängig anwendbar). Mit Host-Repro (OK-Toggle inkl. des bereits
      gelieferten unbedingten `furi_timer_stop()`-Stop-Fixes sowie ALT-/NEU-Fassung von
      `hid_mouse_jiggler_exit_callback()` nachgebaut, Mock-Timer mit `armed`-Flag: ALT lässt
      `running=true` nach Quit-während-Laufens stehen und braucht einen zweiten OK-Druck zum
      tatsächlichen Neustart, NEU räumt `running` synchron zum Timer, Exit-während-Stopp bleibt
      in beiden Fassungen folgenlos, 5000-Zyklen-Regression bestätigt `running == armed` bei NEU
      nach jedem Exit) unter ASan/UBSan verifiziert: ALLE CHECKS BESTANDEN. Minimale Änderung
      (ein zusätzlicher `with_view_model(...)`-Block mit `model->running = false`, dasselbe
      Zugriffsmuster wie bereits in `hid_mouse_jiggler_set_connected_status()`, kein neues
      Nebenläufigkeitsmuster) gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1`
      vorab verifiziert, real angewendet und Byte-für-Byte abgeglichen — identisch) in
      `firmware-mining/hid-jiggler-exit-does-not-reset-running-fix/hid_mouse_jiggler_c.patch`,
      Host-Repro in
      `firmware-mining/repro-hid-jiggler-exit-does-not-reset-running-fix/repro.c`, Details in
      `firmware-mining/hid-jiggler-exit-does-not-reset-running-fix.md`. Restlicher Umfang von
      `8fdb70dd3` weiterhin offen: Stealth-Neubewaffnung-Race, Stealth-Bewegung kann 0 sein,
      Jiggler wartet über BLE nicht auf die Verbindung vor dem Senden — für künftige Sitzungen
      vorgemerkt.

## App-Verbesserungen (Fortsetzung)
- [x] Die standalone **RollLab**-App (eigenes Repo, nicht das gleichnamige Modul innerhalb von
      RollForge) auf denselben Fund geprüft, den ich zuvor in `RollForge/rollforge_rf.c` behoben
      habe: `rolllab_rf.c` enthält 1:1 dieselbe OTG-Ownership-Buchhaltung für das externe
      CC1101-Modul (`rl_power_on`/`rl_power_off`, nur umbenannt gegenüber
      `rf_power_on`/`rf_power_off`) — bei mir vorher extern eingeschaltete 5V-Versorgung
      (z. B. "5V on GPIO" im Power-Menü) wird beim bloßen Starten von RollLab ohne angeschlossenes
      externes CC1101-Modul fälschlich als eigenes Eigentum beansprucht und sofort wieder
      abgeschaltet, ohne Fehlermeldung. Fix identisch zur RollForge-Fassung: frühzeitige Rückkehr
      in `rl_power_on()`, wenn die Schiene beim Aufruf bereits an ist. Mit Host-Repro (1:1 aus dem
      bereits verifizierten RollForge-Repro übernommen, nur Funktionsnamen angepasst, drei
      Szenarien: fälschliche Beanspruchung, echtes EXT-Board-Regression, Fehlerfall-Regression)
      unter ASan/UBSan verifiziert, alle Fälle wie erwartet. Unified-Diff gegen die unveränderte
      `G4MEOVER18/RollLab`-Basis (`4ea80481`, `git apply --check` vorab gegen frische Kopie
      verifiziert, real angewendet und byte-für-byte abgeglichen — identisch) in
      `app-improvements/RollLab/otg-power-false-ownership-claim-fix/rolllab_rf_c.patch`, Details
      in `app-improvements/RollLab/otg-power-false-ownership-claim-fix.md`. Dabei zusätzlich
      geprüft, kein Fund: `subghz_devices_begin(int_dev)`-Aufruf vor der Ext-Erkennung ist laut
      `mntm-012`-Firmware-Quelle (`.begin = NULL` für das interne Device) ein dokumentierter No-Op,
      kein Ressourcenleck; die `RLAB_ADVANCE`-Zustands-Restart-Race ist bereits sauber per
      Zustandsprüfung abgesichert (kein Fund wie bei den bereits behobenen
      RollForge-/RollJam-Race-Funden); die eigene, frühere Silence-Terminator-Fix ist bereits im
      aktuellen RollLab-Repo-Stand übernommen.

## Firmware-Feature-Mining (Fortsetzung 2)
- [x] Den nächsten der drei verbliebenen Teilfunde aus `8fdb70dd3` (PR #1112) bearbeitet:
      **Stealth-Neubewaffnung-Race** — `hid_mouse_jiggler_stealth_exit_callback()` in
      `applications/system/hid_app/views/hid_mouse_jiggler_stealth.c` ruft `furi_timer_stop()`
      ungeschützt auf, während sich der Timer-Callback selbst innerhalb desselben Model-Locks,
      den Exit nie nimmt, per Stop+Start neu bewaffnet — ein echtes Nebenläufigkeitsproblem
      (nicht nur eine Anzeigeunstimmigkeit wie bei der bereits gefixten nicht-stealth Variante):
      feuert der Timer gerade beim Verlassen der View, kann Exit's ungeschützter Stop zwischen
      den Callback-internen Stop- und Start-Aufruf geraten und verliert gegen die anschließende
      Neubewaffnung — der Stealth-Jiggler bewegt die Maus dann unbemerkt im Hintergrund weiter,
      bis die App beendet oder die View erneut betreten und der Toggle zweimal gedrückt wird. Mit
      Host-Repro (explizite Durchzählung aller durch die reale Synchronisation erlaubten
      Verschachtelungsreihenfolgen statt einer willkürlich herausgegriffenen: ALT 2 von 3
      möglichen Reihenfolgen lassen den Timer bewaffnet, NEU 0 von 2 möglichen Gesamtreihenfolgen)
      unter ASan/UBSan verifiziert. Fix (Exit setzt `model->running = false` unter dem Model-Lock,
      *vor* ihrem eigenen `furi_timer_stop()` — Reihenfolge hier sicherheitsrelevant, anders als
      bei der bereits gelieferten nicht-stealth-Fassung) minimal auf `exit_callback()` begrenzt,
      Timer-Callback und Timer-Typ unangetastet; bewusst nicht Unleasheds volle Umstrukturierung
      (rand()/Mausbewegung außerhalb des Locks, One-Shot-Timer-Typ) übernommen, da für den
      Race-Fix nicht notwendig. Port-Vorschlag inkl. Fix in
      `firmware-mining/hid-jiggler-stealth-exit-rearm-race-fix.md`, Unified-Diff (patch
      --dry-run -p1 vorab gegen frische Kopie verifiziert, real angewendet und byte-für-byte
      abgeglichen) in
      `firmware-mining/hid-jiggler-stealth-exit-rearm-race-fix/hid_mouse_jiggler_stealth_c.patch`,
      Host-Repro in
      `firmware-mining/repro-hid-jiggler-stealth-exit-rearm-race-fix/repro.c`. Aus demselben
      `8fdb70dd3`-Restumfang weiterhin offen: Stealth-Bewegung kann 0 sein, Jiggler wartet über
      BLE nicht auf die Verbindung vor dem Senden.
- [x] Den nächsten der zwei verbliebenen Teilfunde aus `8fdb70dd3` (PR #1112) bearbeitet:
      **Stealth-Bewegung kann 0 sein** — `move_x`/`move_y` in
      `hid_mouse_jiggler_stealth_timer_callback()` (`applications/system/hid_app/views/
      hid_mouse_jiggler_stealth.c`) werden (nach Anwendung des bereits vorgeschlagenen
      `hid-jiggler-stealth-move-bounds-fix`) gleichverteilt aus `-127..127` gezogen — jede Achse
      kann unabhängig exakt `0` treffen. Mit Host-Repro über den vollständigen
      Zwei-Achsen-Eingaberaum (255×255 = 65025 Ziehungen) exakt quantifiziert: 1/65025 Ziehungen
      liefert `(0, 0)` (dieser Timer-Tick bewegt die Maus überhaupt nicht), 508/65025 liefern
      genau eine Achse als `0`. Ein `(0, 0)`-HID-Report ist für den Host gültig, aber wirkungslos
      — je nach Idle-Erkennung wird der Inaktivitäts-/Sperr-Timer dadurch nicht zurückgesetzt,
      was dem alleinigen Zweck des Stealth Mouse Jigglers zuwiderläuft. Fix: Unleasheds eigene
      `hid_mouse_jiggler_stealth_random_move()`-Hilfsfunktion (`rand() % (2*INT8_MAX) -
      INT8_MAX`, dann bei nicht-negativem Ergebnis `+1`) 1:1 übernommen — bewusst nur diese eine
      unabhängig portierbare Verbesserung aus `8fdb70dd3`, nicht dessen volle Restrukturierung
      (RNG/HID-Aufruf außerhalb des Model-Locks, `FuriTimerTypeOnce`). Mit Host-Repro (voller
      Ein- und Zwei-Achsen-Eingaberaum für ALT und NEU, zusätzlich Bijektionsnachweis für NEUs
      Hilfsfunktion) unter ASan/UBSan bei `-O0`/`-O1` verifiziert: NEU 0/64516 Ziehungen mit
      stiller Achse (ALT: 1 `(0,0)` + 508 Einzelachsen-Treffer), ALLE CHECKS BESTANDEN, keine
      Compiler-Warnungen. Unified-Diff **gegen den Zustand nach Anwendung von
      `hid-jiggler-stealth-move-bounds-fix`** (Anwendungsreihenfolge dokumentiert; volle Kette
      Pristine→Bounds-Fix→dieser Fix Byte-für-Byte gegen die beabsichtigte Endfassung geprüft) in
      `firmware-mining/hid-jiggler-stealth-zero-movement-fix/hid_mouse_jiggler_stealth_c.patch`,
      Host-Repro in `firmware-mining/repro-hid-jiggler-stealth-zero-movement-fix/repro.c`,
      Details in `firmware-mining/hid-jiggler-stealth-zero-movement-fix.md`. Aus demselben
      `8fdb70dd3`-Restumfang weiterhin offen: Jiggler wartet über BLE nicht auf die Verbindung vor
      dem Senden — letzter verbliebener Teilfund dieser Serie.

## Ideen-Recherche (Fortsetzung)
- [x] Aus der GPIO-Logic-Analyzer-Sitzung vorgemerkten Kandidaten aufgegriffen:
      **`NaejEL/flipperzero-i2ctools`** (I2C-Scanner/-Sender über den Erweiterungsport C0/C1 plus
      passiver Sniffer via GPIO-Interrupts auf denselben Pins). Aktiv gepflegt, im offiziellen
      Flipper Application Catalog gelistet (letzter Commit 2025-10-25, `fap_version="1.1"`).
      API-87-Status gegen den eigenen `mntm-012`-Header-Stand verifiziert (frischer, blobloser
      Sparse-Checkout): alle genutzten I2C-HAL-Funktionen (`furi_hal_i2c_acquire/release/
      is_device_ready/trx`, `furi_hal_i2c_handle_external`) sowie die genutzten GPIO-Pins/-
      Funktionen (`gpio_ext_pc0/pc1`, `furi_hal_gpio_init/add_int_callback/remove_int_callback/
      read`) unverändert vorhanden. Beim Quelltext-Review dabei einen potenziellen
      Pufferüberlauf im Sniffer gefunden: `SCLcallback()` erhöht `data_index` nach jedem
      vollständigen Byte unbedingt, ohne Prüfung gegen `MAX_MESSAGE_SIZE` (128) — anders als der
      bereits geprüfte `frame_index` (der bei Überlauf zurückgesetzt wird) — eine einzelne
      I2C-Transaktion mit mehr als 128 Byte ohne zwischenzeitliche Stop-/Start-Bedingung schreibt
      dadurch außerhalb der `data[]`/`ack[]`-Arrays des aktuellen Frames, im EXTI-Interrupt-
      Kontext. Kein Fix-Vorschlag angelegt (externe Drittanwendung, analog zur bereits
      dokumentierten Abtastraten-Diskrepanz beim GPIO Logic Analyzer), nur als Einschränkung
      vermerkt. Kuratiert in `ideas/i2ctools-app-status.md`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` ergänzt. Damit ist die aus der GPIO-Logic-Analyzer-Suche
      vorgemerkte Kandidatenliste abgearbeitet.

## App-Verbesserungen (Fortsetzung 2)
- [x] **g4meover-magspoof**: bislang ungeprüften UART-Kartenleser-Pfad durchgesehen
      (`scenes/mag_scene_read.c`, bislang nur `mag_device_load_data()`/
      `mag_device_parse_card_string()`/`mag_encode()`/`mag_state_load()` geprüft). Fund in
      `uart_worker()`: die eigene Rolling-Kappung von `mag->uart_text_box_store` (der
      `FuriString`, in der sich der vom angeschlossenen UART-MSR-Reader empfangene Rohtext
      sammelt) erhöht `mag->uart_text_box_store_strlen` um die Länge des gerade erst
      eingetroffenen Häppchens (`len`, bis zu `UART_RX_BUF_SIZE` = 320 Byte), *bevor* dieser
      Häppchen tatsächlich angehängt wird — der an `furi_string_right()` übergebene
      Kappungs-Index (`.../2`) rechnet dadurch systematisch mit der projizierten
      Nach-Anhängen-Größe statt mit der zum Aufrufzeitpunkt tatsächlichen. Laut
      `Next-Flip/Momentum-Firmware`s `furi_string_right()` (1:1-Wrapper um `P-p-H-d/mlib`s
      `m_string_right()`) wird dadurch bei jeder ausgelösten Kappung mehr vom bereits
      gesammelten Kartentext verworfen als beabsichtigt (bis zu `len/2` Byte zusätzlich pro
      Auslösung) — mit den echten Puffergrößen dieser App (`UART_TERMINAL_TEXT_BOX_STORE_SIZE`
      = 4096) zwar nicht bis zum Totalverlust des ganzen Puffers, aber lautlos mehr Verlust als
      der eigene Kommentar verspricht; das zugrunde liegende Muster selbst würde bei
      `len >= bisherige Größe` (mit den heutigen Konstanten nicht erreichbar) den kompletten
      Puffer auf einen Schlag leeren. Mit Host-Repro (`m_string_right()`-Semantik wortgetreu aus
      der mlib-Quelle übernommen, ALT/NEU auf demselben Mock-`FuriString`) über den
      vollständigen Auslösebereich der echten Konstanten (51360 Kappungsereignisse, ALT verwirft
      in Summe 5.486.880 Byte mehr als NEU) sowie einen verkleinerten Modellspeicher für den
      sonst nicht erreichbaren Totalverlust-Fall (1054 Fälle, ALT leert dort jedes Mal
      vollständig) unter ASan/UBSan bei `-O0`/`-O1` verifiziert: je 260165 Prüfungen, ALLE
      CHECKS BESTANDEN, keine Compiler-Warnungen. Fix (Kappungsentscheidung/-index nutzen jetzt
      die tatsächliche Größe vor dem Anhängen, keine vorzeitige Erhöhung mehr) in
      `app-improvements/g4meover-magspoof/uart-read-textbox-truncation-order-fix.md`,
      Unified-Diff (`git apply --check` vorab gegen frische Kopie verifiziert, real angewendet
      und byte-für-byte abgeglichen) in
      `app-improvements/g4meover-magspoof/uart-read-textbox-truncation-order-fix/mag_scene_read_c.patch`,
      Host-Repro in
      `app-improvements/g4meover-magspoof/repro-uart-truncation-order-fix/repro.c`. Dabei
      zusätzlich geprüft, kein Fund: `mag_device_parse_card_string()` selbst (Track-1/2/3-
      Grenzsuche über `%`/`;`/`?`, konsistent); die eigenständige, aber nirgends im Baum
      aufgerufene `helpers/mag_text_input.c`/`.h` (582 Zeilen eigenes Tastatur-Widget) — die App
      nutzt laut `mag_i.h`/`mag.c` durchgehend den Standard-`TextInput` aus
      `gui/modules/text_input.h`, totes Gewicht im Build, aber kein Laufzeitfehler, da nirgends
      aufgerufen; `mag_scene_emulate_config.c`/`mag_scene_settings.c` (Variable-Item-Callbacks
      inkl. der beiden sich gegenseitig einschränkenden Track/Reverse-Regler sowie der
      UART-MSR-Bestätigungsdialog-Logik) — auf den ersten Blick riskant wirkende
      Reset-ohne-Text-Update-Zweige beim Nachrechnen als korrekt bestätigt, da der jeweils
      angezeigte Text zum Zeitpunkt des Zurücksetzens bereits zum wiederhergestellten Index
      passt. Restliche, bislang ungeprüfte Dateien dieser App (`mag_scene_about.c`,
      `mag_scene_file_select.c`, `mag_scene_input_name.c`, `mag_scene_saved_menu.c` u. a.)
      überwiegend reine Navigations-Boilerplate, noch nicht im Detail durchgesehen.

## Firmware-Feature-Mining (Fortsetzung 3)
- [x] Den letzten verbliebenen Teilfund aus `8fdb70dd3` (PR #1112, "HID: improve Mouse Jiggler
      timers and Bluetooth controls") bearbeitet: **Jiggler wartet über BLE nicht auf die
      Verbindung vor dem Senden** — betrifft beide Varianten,
      `hid_mouse_jiggler_timer_callback()` und `hid_mouse_jiggler_stealth_timer_callback()` in
      `applications/system/hid_app/views/hid_mouse_jiggler.c` bzw.
      `hid_mouse_jiggler_stealth.c`. Beide Modelle tragen bei mir (`mntm-012`) bereits ein
      `connected`-Feld, gepflegt von `hid_ble_app()`s Verbindungsstatus-Callback über
      `hid_mouse_jiggler_set_connected_status()`/`hid_mouse_jiggler_stealth_set_connected_status()`
      — vollständig vorhanden und verdrahtet, kein eigener Fund. Beide Draw-Callbacks lesen
      `model->connected` auch schon, aber ausschließlich für die BLE-Symbol-Auswahl; beide
      Timer-Callbacks lesen es an keiner Stelle und rufen `hid_hal_mouse_move()` allein aufgrund
      von `model->running` auf — über BLE, während die Verbindung noch nicht steht (Gerät gerade
      beworben, Host außer Reichweite, Pairing offen), verläuft das lautlos ins Leere, ohne
      Fehler und ohne eigenen Hinweis über die bereits vorhandene BLE-Symbol-Anzeige hinaus.
      Fix: neues Makro `hid_model_connected(model)` in `hid.h` (unter `HID_TRANSPORT_BLE`
      `model->connected`, sonst `true`, damit USB-Builds exakt das alte Verhalten behalten),
      Sende-Bedingung beider Timer-Callbacks auf `model->running && hid_model_connected(model)`
      erweitert, beide Draw-Callbacks zeigen bei laufend-aber-getrennt "Waiting for
      Connection..." statt "Press Start to jiggle". Mit Host-Repro (Sende-Entscheidung beider
      Varianten als reine Funktion von Build/`running`/`connected` nachgebaut, erschöpfend über
      alle 2×2×2 Kombinationen je Variante geprüft) unter ASan/UBSan bei `-O0`/`-O1` verifiziert:
      16/16 Prüfungen, ALT sendet in beiden Varianten in jeder laufend-BLE-getrennt-Zeile, NEU in
      keiner mehr, NEU auf USB-Builds in keiner der acht Zeilen von ALT verschieden, ALLE CHECKS
      BESTANDEN, keine Compiler-Warnungen. Unified-Diffs (`git apply --check` je vorab gegen
      frische Kopie verifiziert, real angewendet und byte-für-byte abgeglichen; die
      stealth-Datei gegen den Zustand nach `hid-jiggler-stealth-move-bounds-fix` und
      `hid-jiggler-stealth-zero-movement-fix`, da alle drei denselben Codeblock berühren) in
      `firmware-mining/hid-jiggler-ble-connection-wait-fix/hid_h.patch`,
      `hid_mouse_jiggler_c.patch` und `hid_mouse_jiggler_stealth_c.patch`, Host-Repro in
      `firmware-mining/repro-hid-jiggler-ble-connection-wait-fix/repro.c`, Details in
      `firmware-mining/hid-jiggler-ble-connection-wait-fix.md`. Damit ist die gesamte aus
      `8fdb70dd3` zurückgestellte Fundliste abgearbeitet — kein weiterer Teilfund aus diesem
      Commit offen. Bewusst nicht mitübernommen: Unleasheds größere Restrukturierungen aus
      demselben Commit (stealth-Timer auf `FuriTimerTypeOnce` mit Selbst-Rearm unter Lock,
      `hid_hal_mouse_move()`/`rand()` aus dem Model-Lock heraus bei beiden Varianten,
      Bool-Flag-Umbau der nicht-stealth Timer-Logik) — eigenständige, größere Änderungen ohne
      Bezug zu diesem konkreten Fund.

## App-Verbesserungen (Fortsetzung 3)
- [x] **g4meover-magspoof**: aus der vorgemerkten Restliste weiterverfolgt
      (`mag_scene_about.c`, `mag_scene_file_select.c`, `mag_scene_input_name.c`,
      `mag_scene_saved_menu.c` u. a. — überwiegend reine Navigations-Boilerplate ohne Fund,
      dabei zusätzlich `mag_scene_emulate.c`, `mag_scene_saved_info.c`,
      `mag_scene_delete_confirm.c`, `mag_scene_start.c`, `mag_scene_input_value.c` mitgeprüft).
      Eigentlicher Fund dabei in `helpers/mag_helpers.c::mag_spoof()` (der zentralen
      Sendefunktion, bislang nur über `mag_encode()` selbst geprüft, nicht über ihre Aufrufer):
      die Kopie jedes Tracks aus seinem `FuriString` in einen einfachen C-String
      (`data1 = malloc(furi_string_size(ft1) + 1); strncpy(data1, furi_string_get_cstr(ft1),
      furi_string_size(ft1));`) terminiert den Zielpuffer nie. `strncpy(dst, src, n)`
      null-terminiert nur, wenn `strlen(src) < n` — hier ist `n` aber exakt
      `furi_string_size(ft)`, also exakt `strlen(src)`, die Bedingung ist nie erfüllt, das
      extra für den Terminator reservierte Byte (`data[size]`) bleibt uninitialisierter
      Heap-Inhalt. `mag_encode()` liest den Puffer anschließend mit einer reinen
      „bis zum ersten Nullbyte"-Schleife (`for(int i = 0; *(data + i) != 0; i++)`) — ist das
      uninitialisierte Byte zufällig ungleich null, liest diese Schleife unkontrolliert über
      das Ende der Allokation hinaus und speist fremden Heap-Inhalt als vermeintliche
      Kartendaten in das Bit-Packing ein, das dann tatsächlich über die aktive Sendemethode
      abgestrahlt wird. Anders als der bereits dokumentierte `mag_encode()`-Stack-Overflow bei
      überlangen Tracks (`mag-encode-fixed-buffer-overflow-fix.md`) ist dieser Fund unabhängig
      von der Tracklänge — betrifft auch den leeren Track — und tritt bei jedem einzelnen
      „Send" neu auf; da die drei Puffer bei jedem Emulationsversuch neu alloziert/freigegeben
      werden, liefert ein wiederverwendender Allokator ab dem zweiten „Send" in derselben
      Sitzung zuverlässig denselben, jetzt nicht-null vorbelegten Speicherbereich zurück. Mit
      Host-Repro (Kopier-/Scan-Logik 1:1 nachgebaut, je ein Testfall für die allererste
      Allokation dieser Größe und für eine über `free()`+`malloc()` gezielt wiederverwendete,
      per `fork()`/`waitpid()` isoliert) unter ASan/UBSan bei `-O0`/`-O1` verifiziert: ALT löst
      in beiden Testfällen zuverlässig `heap-buffer-overflow` aus (AddressSanitizer füllt
      frischen Heap zusätzlich mit einem Nicht-Null-Muster, wodurch bereits die allererste
      Allokation betroffen ist), NEU (`strncpy(..., furi_string_size(ftN) + 1)`, exakt die
      bereits für `malloc()` verwendete Größe) besteht beide Testfälle sauber — ALLE CHECKS
      BESTANDEN, keine Compiler-Warnungen. Fix in
      `app-improvements/g4meover-magspoof/mag-spoof-track-copy-missing-null-terminator-fix.md`,
      Unified-Diff (`git apply --check` vorab gegen frische Kopie verifiziert, real angewendet
      und byte-für-byte abgeglichen) in
      `app-improvements/g4meover-magspoof/mag-spoof-track-copy-missing-null-terminator-fix/mag_helpers_c.patch`,
      Host-Repro in
      `app-improvements/g4meover-magspoof/repro-mag-spoof-track-copy-missing-null-terminator-fix/`.
      Restliche, in dieser Sitzung durchgesehene Dateien ohne eigenen Fund:
      `mag_scene_about.c`/`mag_scene_file_select.c`/`mag_scene_saved_menu.c`/`mag_scene_start.c`
      (reine Navigations-Boilerplate), `mag_scene_saved_info.c`/`mag_scene_delete_confirm.c`
      (Anzeige-/Bestätigungslogik, Grenzfälle nachgerechnet, korrekt), `mag_scene_input_name.c`/
      `mag_scene_input_value.c` (laut eigenem `About`-Text sowie mehreren `TODO`s bewusst
      unfertiger „Add Manually"-Pfad — im Hauptmenü bereits auskommentiert, kein aktiver
      Fehler). `g4meover-magspoof` damit bis auf `helpers/mag_text_input.c`/`.h` (bereits als
      totes Aufräum-Kandidat vorgemerkt) vollständig durchgesehen.

## Kompatibilität (Kontext, lokal umgesetzt)
- CLI-Kompat-Shim (`cli_session_open/close`, `cli_vcp`) für Alt-Apps + cli_bridge — Firmware-seitig, lokal.
- 49 Alt-Apps laufen bereits unter `api_mismatch_autoload` (bewiesen per Symbol-Analyse).
