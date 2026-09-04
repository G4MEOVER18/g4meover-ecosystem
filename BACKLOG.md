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
      (Classic/Ultralight-C) unten separat abgehakt; Parser/Magic-Support weiterhin offen.
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

## Kompatibilität (Kontext, lokal umgesetzt)
- CLI-Kompat-Shim (`cli_session_open/close`, `cli_vcp`) für Alt-Apps + cli_bridge — Firmware-seitig, lokal.
- 49 Alt-Apps laufen bereits unter `api_mismatch_autoload` (bewiesen per Symbol-Analyse).
