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
- [~] MIFARE Plus / Ultralight AES: schlüsselbasierte Authentifizierung (`MfPlusKey`,
      `MfUltralightAesKey`, Auth-Poller-Zustände) — **korrigiert, siehe Fortsetzung 22**: dieser
      Eintrag behauptete zuletzt (Fortsetzung 21) fälschlich "noch nicht selbst als
      Firmware-Mining-Fund bearbeitet" bzw. "Phase 1 von 4 noch nicht umgesetzt". Tatsächlich
      bereits vor Fortsetzung 21 geliefert (dort übersehen, kein Querverweis geprüft): Phase 1
      (Krypto-Kern, host-verifiziert gegen FIPS-197/RFC-4493/OpenSSL-Differenzfuzz,
      `firmware-mining/mfplus-mfultralight-aes-auth-port.md`), Phase 2 für beide Kartenfamilien
      (Struct-/Serialisierungs-Erweiterung, `firmware-mining/mfplus-phase2-sl3-struct-
      serialization.md` + `firmware-mining/mfultralight-aes-phase2-struct-serialization.md`)
      sowie der host-testbare Teil von Phase 3 (SAK-/ATQA-Korrekturen,
      `firmware-mining/mfplus-sl0-sl3-detection-port.md`). Wirklich offen (unverändert durch die
      Korrektur): die eigentlichen Poller-Auth-Zustandsautomaten (Rest von Phase 3), Schreibfluss/
      Admin-Keys (Phase 4, Write-IV bei der Quelle selbst unvalidiert), Listener/Emulation
      (Phase 5), UI/Dictionary-Anbindung (Phase 6) — alle vier brauchen echte Hardware zum
      sinnvollen Testen bzw. bauen auf den vorherigen, noch nicht hardwaregetesteten Phasen auf.
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
- [x] Restlichen NFC-Werkzeug-Cluster begonnen (**abgeschlossen, siehe Fortsetzung 23**): **NFC
      Tools** (`applications/external/nfctools`,
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

## Firmware-Feature-Mining (Fortsetzung 4)
- [x] Eigenen bereits dokumentierten Port-Fund **Evil BLE** (`firmware-mining/evil-ble-app-port.md`)
      erneut aufgegriffen und den zugrundeliegenden Scanner-Code (`evil_ble_scanner.c`,
      `barkandbite/flipper_suite`, frisch gegengeprüft) noch einmal Zeile für Zeile gegen den
      in derselben Datei bereits vorhandenen, korrekten Platzhalter-Check gelesen — dabei einen
      zweiten, eigenständigen Fund gemacht, der den ersten Fix in der Praxis wirkungslos macht:
      `evil_ble_scanner_rx_cb()`s Update-Zweig prüft beim erneuten Sichten eines bereits
      bekannten Geräts `strncmp(dev.name, "(unknown)", 9) != 0`, um den Namen/die
      Sende-Payload nur bei einem „echten" Namen aufzufrischen — der Platzhalter für ein nur
      per MAC gemeldetes Gerät lautet laut `parse_scanbt_line()` aber nie `"(unknown)"`,
      sondern immer `"(AA:BB:CC:DD:EE:FF)"`, weshalb diese Bedingung für jede erdenkliche
      Eingabe wahr ist — toter, nie zutreffender Code. Der Update-Zweig ruft dadurch bei
      **jedem** erneuten Sichten eines bereits bekannten Geräts `build_adv_from_name()`
      ungeprüft mit dem aktuellen `dev.name` auf, auch wenn dieser nur der Anzeige-Platzhalter
      ist. Für ein nur per MAC gemeldetes Gerät kippt `adv_data_len` dadurch schon beim
      zweiten Sichten — bei einer laufenden `scanbt`-Sitzung gegen ein Gerät in Reichweite der
      Normalfall, keine Randbedingung — von 0 (richtig: „kein Name bekannt") auf einen
      positiven Wert, dessen Payload den MAC-Platzhaltertext als vermeintlichen
      „Complete Local Name" (AD-Typ `0x09`) enthält — exakt der Fehlerzustand, den mein
      bereits gelieferter `evil_ble.c`-Fix (Fallback-Zweig in `evil_ble_start_clone()`)
      verhindern sollte. Da `evil_ble_start_clone()` nur `if(dev.adv_data_len > 0)` prüft und
      die Payload dann unbesehen übernimmt, wird mein Fallback-Fix dabei komplett umgangen,
      ohne dass das an der Oberfläche sichtbar wäre. Mit Host-Repro (exakter Dedup-/
      Update-Codeblock aus `evil_ble_scanner_rx_cb()` nachgebaut, drei Fälle: MAC-only-Gerät
      beim zweiten Sichten, Regression bei einem dreimal in Folge gemeldeten echten Namen,
      Regression bei einem defensiv angenommenen leeren Namen) unter ASan/UBSan bei `-O0`/`-O1`
      verifiziert: 17/17 Prüfungen bestanden, ALT kippt `adv_data_len` beim MAC-only-Gerät
      fälschlich auf 21 (Länge des kodierten Platzhaltertextes), NEU (derselbe
      Platzhalter-Check `dev.name[0] != '\0' && dev.name[0] != '('`, den die App an zwei
      anderen Stellen bereits selbst verwendet) belässt es korrekt bei 0, für den echten Namen
      liefern ALT und NEU byte-identische Payload/Länge — keine ASan-/UBSan-Meldungen, keine
      Compiler-Warnungen. Fix in
      `firmware-mining/evil-ble-rescan-adv-data-placeholder-leak-fix.md`, Unified-Diff
      (`git apply --check` vorab gegen frische Kopie verifiziert, real angewendet und
      byte-für-byte abgeglichen) in
      `firmware-mining/evil-ble-rescan-adv-data-placeholder-leak-fix/evil_ble_scanner_c.patch`,
      Host-Repro in
      `firmware-mining/repro-evil-ble-rescan-adv-data-placeholder-leak-fix/repro.c`. Beide
      Fixes (dieser sowie der bereits dokumentierte in `evil_ble.c`) gehören zusammen
      appliziert — keiner der beiden macht den anderen entbehrlich.

## App-Verbesserungen (Fortsetzung 4)
- [x] **g4meover-magspoof**: den seit mehreren Sitzungen vorgemerkten letzten offenen Punkt
      abgeschlossen — `helpers/mag_text_input.c`/`.h` (582 bzw. 83 Zeilen, ein eigenständiges
      Tastatur-Widget `Mag_TextInput`) vollständig auf tatsächliche Nutzung geprüft, nicht nur
      als „vermutlich tot" markiert belassen. Jede von `mag_text_input.h` deklarierte Funktion
      (`mag_text_input_alloc()`, `_free()`, `_reset()`, `_get_view()`,
      `_set_result_callback()`, `_set_header_text()`) sowie der Typ `Mag_TextInput`/
      `Mag_TextInputModel` selbst kommt außerhalb dieser beiden Dateien im gesamten Baum kein
      einziges Mal vor (`mag_i.h`, `mag.c`, alle `scenes/*.c`, `helpers/mag_helpers.c`
      durchsucht) — die einzige oberflächliche Namensähnlichkeit, `mag_text_input_callback()`
      (in `mag.c`, verwendet von `mag_scene_input_name.c`/`mag_scene_input_value.c`), ist eine
      andere Funktion für den Standard-`TextInput` aus `gui/modules/text_input.h`, den diese
      beiden Szenen tatsächlich nutzen, und hat mit dem eigenen Widget nichts zu tun.
      `application.fam` listet keine explizite Quelldateiliste, `fbt`/`ufbt` bindet Quelldateien
      per Verzeichnis-Glob ein — beide Dateien werden also bei jedem Build mitkompiliert und
      mitgelinkt, obwohl zur Laufzeit nichts sie je instanziiert: totes Gewicht (Flash,
      Baumzeit), kein Laufzeitfehler. Vorschlag: beide Dateien vollständig entfernen, rein
      subtraktiv, keine andere Datei muss geändert werden, da keine von ihnen
      `mag_text_input.h` inkludiert. Details, Begründung, betroffene Dateien, Risiko und
      Testhinweis (Baum-/Referenz-Prüfung statt eigenständigem Repro, da reine Löschung
      ungenutzten Codes ohne Logikänderung) in
      `app-improvements/g4meover-magspoof/mag-text-input-dead-code-removal.md`, Unified-Diff
      (löscht beide Dateien; `git apply --check` vorab gegen frische Kopie verifiziert, real
      angewendet, Ergebnis byte-für-byte mit einer unabhängig per `git rm` erzeugten
      Referenzkopie abgeglichen — identisch) in
      `app-improvements/g4meover-magspoof/mag-text-input-dead-code-removal/mag_text_input_removal.patch`.
      `g4meover-magspoof` damit jetzt vollständig durchgesehen — keine weiteren offenen Dateien
      aus früheren Sitzungen vorgemerkt.

## App-Verbesserungen (Fortsetzung 5)
- [x] **usb-army-penetrator**: den zuletzt offenen Rückstellpunkt "TFT-Hex-Farbparser
      unerreicht" aufgegriffen — `HardwareTFT::convertStringToColor()`
      (`src/Devices/TFT/HardwareTFT.cpp`) vollständig geprüft statt nur als unerreicht
      vermerkt zu lassen. Aufrufstellen-Vollsuche (`src/main.cpp`,
      `src/Devices/Platform/ESP32BoardSupport.cpp`) bestätigt: der Hex-Zweig ist aktuell
      tatsächlich unerreicht, alle Aufrufer übergeben ausschließlich feste Named-Color-Literale
      über `colorLookup`. Trotzdem Fund gemacht — der Funktionskommentar ("RGB888 hex color
      code") signalisiert eindeutig eine künftige externe Aufrufstelle (Ducky-Kommando,
      Web-UI): die bestehende Fehlerprüfung nach `std::strtoul()` (`errno != 0 || end ==
      hexStr.c_str()`) deckt nur Überlauf und "gar keine Ziffer konsumiert" ab, drei andere
      ungültige-Eingabe-Klassen schlüpfen durch und werden als scheinbar gültige Farbe
      weitergereicht statt wie vom Rückgabewert `TFT_BLACK` in den beiden abgedeckten Fällen
      beabsichtigt abgewiesen zu werden: Trailing-Müll nach den Hex-Ziffern wird von
      `strtoul()` nur stillschweigend abgeschnitten statt geprüft (`end` wird nie gegen das
      Stringende verglichen); ein führendes `+`/`-` wird von `strtoul()` laut C-Standard
      akzeptiert und bei `-` als Zweierkomplement-Wrap zurückgewickelt, für eine
      reine-Hex-Ziffern-Funktion ein unbeabsichtigter Erfolgsfall; Werte über dem eigenen
      24-Bit-RGB888-Format (z. B. eine achtstellige Hex-Eingabe) werden nicht begrenzt, obwohl
      weder Name noch Kommentar der Funktion einen größeren Bereich vorsehen. Mit Host-Repro
      (ALT/NEU 1:1 aus der echten Funktion, `lcd.color24to16()` aus LovyanGFX per
      Passthrough-Stub ersetzt, da der Fund vollständig davor liegt) unter ASan/UBSan
      verifiziert: fünf Regressionsfälle (leer, reines "0x", ungültiges Zeichen, mit/ohne
      "0x"-Präfix) unverändert, fünf Fundfälle (Trailing-Müll, führendes `+`/`-`, führendes
      Leerzeichen, > 24 Bit) vorher fälschlich als "gültig" akzeptiert, nachher korrekt
      `TFT_BLACK` — ALLE CHECKS BESTANDEN. Fix (Zeichenklassen-Prüfung vor `strtoul()`,
      Stringende- und 24-Bit-Bereichsprüfung danach) zusätzlich am realen Funktionskörper
      (Named-Color-Tabelle + `TFT_BLACK`/`lcd`-Stubs) kompiliert und smoke-getestet. Details in
      `app-improvements/usb-army-penetrator/tft-hex-color-parser-validation-gap-fix.md`,
      Unified-Diff (`git apply --check` vorab gegen frische Kopie verifiziert, real angewendet
      und byte-für-byte abgeglichen) in
      `app-improvements/usb-army-penetrator/tft-hex-color-parser-validation-gap-fix/HardwareTFT_c.patch`,
      Host-Repro in
      `app-improvements/usb-army-penetrator/repro-tft-hex-color-parser-validation-gap/repro.cpp`.
      Damit ist auch dieser letzte offene `usb-army-penetrator`-Rückstellpunkt abgeschlossen —
      restlicher Code (Ethernet CH9120/W5500, unverdrahteter Stub ohne angeschlossene Hardware)
      bleibt außerhalb reiner Quelltextprüfung offen.

## Ideen-Recherche (Fortsetzung 2)
- [x] Neue Recherche außerhalb bestehender Rückstellpunkte: dritte Ergänzung für die Kategorie
      „GPIO / Serial Bus" gesucht, ausgehend von den eigenen CAN-Bus-Funden desselben Herstellers
      (`firmware-mining/canbus-attack-app-port.md`, `firmware-mining/canbus-electroniccats-app-port.md`)
      — **RS485 Modbus (`ElectronicCats/flipper-rs485modbus`)** geprüft: Modbus-RTU-Sniffer/-Master
      über ein RS485-Hardwaremodul, alle acht Standard-Funktionscodes inkl. Exception-Decodierung,
      SD-Logging, Paket-Ringpuffer zur Re-Injektion. Quelltext direkt per `curl` gegen
      `raw.githubusercontent.com` gelesen (`ModbusApp/Modbus.c/.h`, `modbus_uart/modbus_uart.c/.h`,
      `application.fam`). API-87-Status gegen den eigenen `mntm-012`-Header-Stand verifiziert
      (`targets/f7/furi_hal/furi_hal_serial.h`/`_types.h`/`_control.h` direkt per `curl` vom Tag
      `mntm-012` gelesen): die genutzte DMA-basierte Serial-API
      (`furi_hal_serial_control_acquire/release`, `_dma_rx_start/_stop`, `furi_hal_serial_dma_rx`,
      `furi_hal_serial_set_br`, `FuriHalSerialRxEvent{Data,Idle}`) exakt vorhanden, `expansion_
      disable()`/`_enable()` beim Start korrekt vor der ersten Serial-Nutzung aufgerufen. Zwei
      Funde gemacht: (1) `modbus_uart.c::Serial_Begin()` konfiguriert USART1 nicht über die
      dafür vorgesehene öffentliche `furi_hal_serial_init()`/`furi_hal_serial_configure_framing()`,
      sondern per direkten STM32-LL-Register-/GPIO-Aufrufen (`LL_USART_Init()`,
      `furi_hal_gpio_init_ex(..., GpioAltFn7USART1)`, `LL_RCC_SetUSARTClockSource()`) von Hand —
      geprüft, ob das technisch nötig ist (Framing-Optionen, die die offizielle API nicht
      abdeckt): Gegenteil bestätigt, `furi_hal_serial_configure_framing()` deckt mit
      `FuriHalSerialDataBits{6,7,8,9}`/`Parity{None,Even,Odd}`/`StopBits{0_5,1,1_5,2}` exakt die
      Obermenge der App-UI-Optionen (7/8/9 Bit, None/Even/Odd, 0,5/1/1,5/2 Stopbits) ab — die
      App dupliziert damit vollständig unnötig eine bereits öffentlich vorhandene
      Firmware-Funktion, mit Kompatibilitätsrisiko bei künftigen Register-/Pin-Änderungen der
      Firmware, die App-seitig lautlos (kein Kompilierfehler) divergieren würden. (2)
      `Modbus.c::modbus_app_free()` ruft `expansion_enable(app->expansion)` erst NACH
      `furi_record_close(RECORD_EXPANSION)` auf — umgekehrte Reihenfolge zum eigenen
      Start-Code und zur bei UART Sniff/GPIO Logic Analyzer bereits bestätigten Konvention;
      nach aktuellem Furi-Record-Modell vermutlich ohne sichtbare Laufzeitauswirkung (Record-
      Close gibt nur die eigene Referenz frei, das Singleton bleibt bestehen), aber ein falsch
      herum aufgerufenes API-Paar. Kein Fix-Vorschlag als eigenständiger Patch angelegt (externe
      Drittanwendung, noch kein eigener Port), beide Funde inkl. Verbesserungsskizze für einen
      künftigen Port dokumentiert. Kuratiert in `ideas/rs485-modbus-app-status.md`, Katalog-Zeile
      und Recherche-Auftrag in `ideas/pentest-app-katalog.md` ergänzt.

## Firmware-Feature-Mining (Fortsetzung 5)
- [x] Letzten der drei aus der Prastel-Sitzung zurückgestellten SubGHz-Kandidaten
      aufgegriffen: **Nice O-Code** (Unleashed `dev`, Commit `945234a`, API 88.9 — dieselbe
      Fundstelle wie Prastel/Security+ 2.0 Keypad). Bei mir (`lib/subghz/protocols/nice_flor_s.c`,
      mntm-012) fehlt die Erkennung komplett: jedes der 16 pro Tastendruck gesendeten Pakete
      wird bei O-Code individuell mit einer paket-abhängigen Maske neu verschlüsselt statt wie
      bei Plain-Nice-FloR-S unverändert wiederholt — meine `feed()` prüft das nicht und meldet
      ein O-Code-Signal aktuell fälschlich als Nice-FloR-S mit Zufallsinhalt statt es
      abzulehnen oder korrekt zu erkennen (Fehlklassifikations-Fund, kein Absturz-/
      Sicherheitsrisiko). Quelltext direkt per `raw.githubusercontent.com` bezogen
      (`nice_flor_s.c`/`.h`, `public_api.h`, `CHANGELOG.md` bei Commit `945234a`) und gegen
      meinen eigenen `mntm-012`-Stand diffgeprüft. Vier neue, rein additive und aktuell
      unverwendete Hilfsfunktionen (IC-parametrisierte Ver-/Entschlüsselung, paket-abhängige
      Maske, Paketindex-Rückgewinnung) 1:1 aus Unleashed übernommen, an meine eigene
      Tabellen-Lesefunktion angepasst (keine Tabellen-Zwischenspeicherung übernommen — bleibt
      symmetrisch zu meinen bestehenden `_encrypt()`/`_decrypt()`) und mit Host-Repro
      (`firmware-mining/repro-nice-o-code-subghz/repro.c`) unter ASan/UBSan bei `-O0`/`-O1`
      verifiziert: Plain-Äquivalenz bei `IC_PLAIN` gegen meine bestehende Chiffre (0
      Abweichungen), Ver-/Entschlüsselung über den vollständigen 16-Bit-IC-Bereich sowie
      fuzzed round-trip, Paketindex-Rückgewinnung unabhängig vom Geheimtext, Masken-Determinismus,
      und eine vollständige synthetische O-Code-Pipeline (Maske → Verschlüsseln →
      Paketindex/Maske/IC zurückgewinnen → Seriennummer/Zähler/Taste exakt rekonstruiert) —
      **2 551 103 Prüfungen, ALLE CHECKS BESTANDEN**, keine Compiler-Warnungen. Patch (additiv,
      ändert keine bestehende Funktion/Struct/Aufrufstelle) in
      `firmware-mining/nice-o-code-subghz-port/{nice_flor_s_c,public_api_h}.patch`
      (`git apply --check` vorab gegen frische Kopie verifiziert, real angewendet und
      byte-für-byte abgeglichen). Die restliche Verdrahtung (Struct-Erweiterung,
      Zwei-Pakete-Live-Erkennungsheuristik in `feed()` — verändert Verhalten am bereits
      produktiven Plain-FloR-S-Pfad —, `.sub`-IC-Feld, sowie die laut Unleashed-CHANGELOG
      separate Installateurcode-Recovery-App) bewusst **nicht** als Patch, sondern als
      Port-Skizze mit exakten Zeilenankern dokumentiert — kein „klein & sicher"-Fund, siehe
      `firmware-mining/nice-o-code-subghz-port.md`. Damit sind alle drei aus der
      Prastel-Sitzung zurückgestellten Kandidaten inzwischen bearbeitet (Security+ 2.0 Keypad
      und Nice O-Code jeweils mit verifiziertem Grundlagen-Beitrag plus Skizze, neue
      KeeLoq-Hersteller bleibt wegen Lernschlüssel-Problematik grundsätzlich zurückgestellt).

## Ideen-Recherche (Fortsetzung 3)
- [x] Vorgemerkten Rückstellpunkt aus der RS485-Modbus-Sitzung aufgegriffen (`ElectronicCats/
      flipper-rs485modbus`, Commit `ff50246`, unverändert): restlichen Quellbestand vollständig
      gelesen (`modbus_parser/`, `modbus_sender/`, `modbus_storage/`, `modbus_ring_buffer/`,
      alle sieben Szenen, `scenes_config/`), bislang nur `modbus_uart.c` und der
      App-Lebenszyklus in `Modbus.c` geprüft. Zwei weitere, konkrete Funde gemacht.
      **Hauptfund** (Speicherkorruption): `modbus_ring_buffer.c::writeRingBuffer()` erhöht
      `delimiterIdx` bei jeder vollständig geparsten Modbus-Nachricht, setzt es aber nur beim
      `writeIdx`-Wrap zurück (`Modbus.h`: `delimiters[32]`, `ringBuffer[255]`), nicht wenn
      `delimiterIdx` selbst die Array-Größe 32 erreicht — bei kurzen RTU-Nachrichten (4–8 Byte,
      Standard bei Read/Write-Antworten) sind nach 32 Nachrichten in einer Sitzung oft erst
      128–256 von 255 Byte belegt, `writeIdx` hat also noch nicht gewrapt, `delimiterIdx` läuft
      trotzdem unkontrolliert über 31 hinaus. `delimiters[32]` liegt im Struct-Layout direkt vor
      `ringBuffer[0]` — jede weitere Nachricht ab der 33. schreibt damit ein
      `writeIdx`-Bookkeeping-Byte in den Speicherbereich der Nutzdaten bereits erfasster
      früherer Nachrichten und überschreibt sie still. Derselbe unbegrenzte Index wird auch
      konsumiert: `scenes/msgs_buffer_scene.c::BuildCMDList()` (Aufbau der "Buffer
      Sender"-Liste) und `OnItemEnterCB()` (Auswahl eines Eintrags, einziger UI-Pfad zum
      erneuten Anzeigen/Senden eines gesniffter Frames) lesen `rb->delimiters[delimiterIdx]`
      bzw. `[index]`/`[index - 1]` ebenfalls ohne Prüfung gegen 32. Mit Host-Repro (Struct und
      `writeRingBuffer()` 1:1 aus `modbus_ring_buffer.c`/`Modbus.h` transkribiert, die
      Rekonstruktionslogik aus `OnItemEnterCB()` als reine Index-/Kopierfunktion nachgebaut)
      unter UBSan verifiziert: expliziter Compiler-Report "index 32 out of bounds for type
      'uint8_t [32]'" beim 33. `writeRingBuffer()`-Aufruf; nach 40 simulierten 4-Byte-Nachrichten
      ist `writeIdx=160` (kein Wrap) und `delimiterIdx=40` (> 31), und die erneute
      Rekonstruktion von Nachricht #0 über dieselbe Logik wie `OnItemEnterCB()` liefert
      nachweislich `83 87 8B 8F` statt der tatsächlich erfassten `40 40 40 40` — die ersten vier
      Byte des Ringpuffers wurden durch die Überläufe der Nachrichten 33–36 mit
      `writeIdx`-Werten überschrieben. Für ein Sniff-/Pentest-Werkzeug, dessen Kernzweck die
      unverfälschte Erfassung von Busverkehr ist, ein Kernfunktions-Fund, kein Randfall.
      **Zweiter, kleinerer Fund**: `modbus_storage.c::open_log_file_stream()` ruft
      `strcpy(app->logFilePath, sequential_file_resolve_path(...))` auf und prüft erst danach
      `if(app->logFilePath != NULL)` — toter Check, da `sequential_file_resolve_path()` bei
      einem fehlschlagenden `strdup()` (z. B. Speicherknappheit) `NULL` zurückgibt und
      `strcpy()` mit NULL-Quellzeiger bereits der Absturz ist; `app->logFilePath` selbst ist
      als Ziel korrekt mit `malloc(100)` vorallokiert (`Modbus.c::modbus_app_alloc()`), nur die
      Quelle kann NULL sein. Kein eigenständiger Fix-Patch angelegt (externe Drittanwendung,
      noch kein eigener Port), beide Funde inkl. Portierungsreihenfolge dokumentiert in
      `ideas/rs485-modbus-app-status.md` (jetzt vier Funde insgesamt für dieses Repo), Host-Repro
      in `ideas/repro-rs485-modbus-ring-buffer-delimiters-overflow/repro.c`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` aktualisiert. Damit ist `ElectronicCats/flipper-
      rs485modbus` jetzt vollständig quelltextlich geprüft — keine weiteren offenen Dateien
      aus früheren Sitzungen vorgemerkt.

## Firmware-Feature-Mining (Fortsetzung 6)
- [x] Unleashed-`dev`-Changelog-Eintrag zu PR #1104 ("Sending a receive-only signal from the
      mobile app or qFlipper no longer crashes the Flipper") aufgegriffen, bislang nicht
      geprüft. Zwei Teile: (1) `subghz_transmitter_free(NULL)`-Absturz für Protokolle ohne
      Encoder — bei mir (mntm-012, `applications/main/subghz/helpers/subghz_txrx.c`) bereits
      durch ein bestehendes `if(instance->transmitter)` vor jedem
      `subghz_transmitter_free()`-Aufruf abgesichert, dieser Absturz tritt bei mir nicht auf.
      (2) "the transmitter pointer is cleared after every teardown instead of being left
      dangling" — geprüft und tatsächlich fehlend gefunden: im Fehlerpfad von
      `subghz_txrx_tx_start()` (u. a. erreichbar über `SubGhzTxRxStartTxStateErrorOnlyRx`,
      ausgelöst z. B. über den RPC-Sendepfad in `subghz_scene_rpc.c`) wird
      `instance->transmitter` freigegeben, aber nicht auf `NULL` gesetzt — anders als an der
      Schwesterstelle `subghz_txrx_tx_stop()` in derselben Datei, die genau das bereits tut.
      Vollständigen Aufrufgraphen von `instance->transmitter` in dieser Datei geprüft (alle
      drei Leser sowie den einzigen Aufrufer des dritten,
      `subghz_scene_read_raw.c`): über keinen mir bekannten, heute existierenden Aufrufpfad
      wird der dangling Zeiger tatsächlich gelesen — ehrlich als Konsistenz-/Haltungsfund
      eingeordnet, nicht als heute auslösbarer Absturz. Host-Repro
      (`firmware-mining/repro-subghz-tx-transmitter-dangling-pointer-cleanup/repro.c`) bildet
      trotzdem die Klasse Fehler nach, vor der der Fix schützt: `struct SubGhzTransmitter`,
      `subghz_transmitter_free()`, `subghz_transmitter_get_protocol_instance()` sowie den
      Fehlerpfad 1:1 transkribiert, dazu ein nachgebauter Aufrufer mit demselben
      Non-NULL-only-Zugriffsmuster wie der bestehende
      `subghz_txrx_set_raw_file_encoder_worker_callback_end()`-Aufrufer. Unter ASan/UBSan bei
      `-O0`/`-O1` verifiziert (je isolierter Kindprozess per `fork()`/`waitpid()`): ALT stürzt
      zuverlässig mit `heap-use-after-free` ab, NEU beendet sich sauber — ALLE CHECKS
      BESTANDEN, keine Compiler-Warnungen. Patch (eine Zeile, rein additiv, identisch zum
      bereits in derselben Datei etablierten Muster) in
      `firmware-mining/subghz-tx-transmitter-dangling-pointer-cleanup/subghz_txrx_c.patch`
      (`git apply --check` vorab gegen frischen `mntm-012`-Klon verifiziert, real angewendet
      und byte-für-byte abgeglichen), Details in
      `firmware-mining/subghz-tx-transmitter-dangling-pointer-cleanup.md`.

## Ideen-Recherche (Fortsetzung 4)
- [x] Neue Recherche außerhalb bestehender Rückstellpunkte: Kategorie "BadUSB / HID" gezielt
      um einen dritten Eintrag ergänzt (bislang nur Bad USB und Mouse Click Recorder) —
      `n-92/kb_fuzzer` ("USB Fuzzer") gefunden: eigenständiges Kleinprojekt (3 Commits, kein
      RogueMaster-Bündel-Fund), sendet bei jedem Tastendruck am Flipper einen neuen
      Zufalls-String (1–20 Zeichen) plus Enter über USB-HID an den Host — Robustheitstest-
      Rauschen statt gezielter DuckyScript-Payload, ergänzt das eigene Bad USB um eine bisher
      fehlende Fuzzing-Variante. Vollständig gelesen (vier Quelldateien, ~250 Zeilen). Frischer,
      unauthentifizierter Klon von `Next-Flip/Momentum-Firmware` (Tag `mntm-012`, Commit
      `e1784e74`) gegen den genutzten Symbolbestand abgeglichen: bricht den Build unverändert —
      `HID_KEYBOARD_L_CTRL`/`_L_ALT`/`_L_GUI` existieren bei mir nicht (nur die
      `KEY_MOD_LEFT_*`-Bitmasken-Varianten), alle drei Vergleiche sind zur Laufzeit ohnehin
      bereits im Original toter Code (der einzige aktive Tastensatz-Eintrag hat `value = 0`,
      identisch zu `HID_KEYBOARD_NONE`) — Ein-Zeilen-Streichung je Symbol behebt den Build ohne
      Funktionsverlust. Zusätzlich beim Lesen gefunden: `usb_hid_keyboard_process()` verwendet
      das `malloc()`-Ergebnis (`create_rand_string()`, `send_string()`) bereits, bevor die
      vorhandene NULL-Prüfung dafür greift — die Prüfung schützt nur noch den nachfolgenden
      `free()`-Aufruf. Host-Repro (Funktion 1:1 transkribiert, `malloc()`-Fehlschlag simuliert)
      unter ASan/UBSan verifiziert: ALT stürzt mit `SEGV`/"store to null pointer" ab, NEU
      (Prüfung vor Nutzung) beendet sich sauber. Details, Fix-Skizzen und Repro in
      `ideas/usb-hid-keyboard-fuzzer-app-status.md` /
      `ideas/repro-usb-hid-keyboard-fuzzer-null-check-order/repro.c`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` ergänzt.

## Firmware-Feature-Mining (Fortsetzung 7)
- [x] Generisches `Loading`-GUI-Widget (`applications/services/gui/modules/loading.c`/`.h`)
      gegen Unleashed-`dev` geprüft (letzter Kandidat: `subghz_txrx.c`-Dangling-Pointer aus
      PR #1104). Zwei zusammengehörige, additive Erweiterungen seit meinem unveränderten
      mntm-012-Basisstand gefunden: `loading_set_progress()`/`loading_reset_progress()` (Commit
      `69c03681`, optionale Fortschrittsleiste unter dem Spinner) und `loading_set_text()`
      (Commit `9e4951074`, PR #1169, `Closes #1168`, API 88.13, optionales Label rechts neben dem
      dann nach links gerückten Spinner, ersetzt laut Commit-Nachricht eine private NFC-eigene
      Kopie derselben Anzeige). Beide Commits ändern ausschließlich diese zwei Dateien, keine
      bestehende Aufrufstelle im Baum (`archive.c`, `bad_usb_app.c`, `ibutton.c`,
      `infrared_app.c`, `nfc_app.c`, `loader.c`, `loader_menu.c`, `js_app.c` per Grep geprüft)
      ruft die drei neuen Funktionen auf — reine Opt-in-API, `loading_draw_callback()` nimmt für
      den Default-Zustand (kein Text, keine Bar) denselben Berechnungspfad wie vorher.
      API-Abhängigkeiten (`elements_progress_bar()`, `furi_string_equal_str/_empty/_set()`,
      `CLAMP()`) bei mir (API 87.1) bereits vorhanden, kein Symbol-Fehlbestand. Mit Host-Repro
      (Default-Icon-Position ALT/NEU über den vollen 256×256-Canvas-Größenraum, Zweig-Trennung
      leer/nicht-leer, Clamp- und Redraw-Unterdrückungslogik aller drei Setter 1:1 aus den echten
      Funktionskörpern transkribiert) unter ASan/UBSan bei -O0/-O1 verifiziert: 65560
      Einzelprüfungen, ALLE CHECKS BESTANDEN, keine Compiler-Warnungen — deckt nur die reine
      Arithmetik-/Zustandslogik ab, nicht die tatsächliche Pixelausgabe (ehrlich als Grenze im
      Risiko-Abschnitt vermerkt). Patch (additiv, betrifft ausschließlich diese zwei Dateien) in
      `firmware-mining/loading-widget-label-and-progress-bar/{loading_c,loading_h}.patch`
      (`git apply --check` vorab gegen frische Kopie meiner eigenen `G4MEOVER18/G4MEOVER-FW`-Basis
      verifiziert, real angewendet und byte-für-byte gegen den tatsächlichen Unleashed-`dev`-Stand
      abgeglichen — identisch), Host-Repro in
      `firmware-mining/repro-loading-widget-label-and-progress-bar/repro.c`, Details in
      `firmware-mining/loading-widget-label-and-progress-bar.md`. Bewusst nicht mitübernommen:
      die eigentliche Verdrahtung in `loader.c` (App-Entpack-Fortschritt) und `nfc_app.c`
      (CUID-Dictionary-Label, laut Unleashed über eine zweite `Loading`-Instanz) — jeweils
      eigenständige, größere Änderungen an aktiv produktivem Code, als Port-Skizze mit exakten
      Zeilenankern dokumentiert statt als Patch.

## Ideen-Recherche (Fortsetzung 5)
- [x] Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer bislang komplett
      fehlenden Kategorie gesucht — bisherige defensive Erkennungs-Funde (SubGHz Jammer Detector,
      Rogue AP Detector, NFC Canary, UART Sniff) decken RF/WiFi/NFC ab, Infrarot fehlte komplett.
      `at0m-b0mb/Nyx-FlipperZero` ("Nyx") gefunden: eigenständiges, aktiv gepflegtes
      Counter-Surveillance-Werkzeug (MIT, `fap_version=1.2`, 2402 Zeilen über 15 Quelldateien) —
      Infrarot-Sweep gegen verdeckte Nachtsicht-Kameras über deren 850/940-nm-Beleuchtung.
      Onboard-Modus nutzt den eingebauten TSOP-75338-Empfänger (Flankenzählung/Tastgrad statt
      Dekodierung, erkennt gepulste/modulierte Quellen, laut eigener Dokumentation blind für
      Gleichstrom-Illuminatoren wegen des 38-kHz-Bandpassfilters); optionaler Probe-Modus
      (1-€-IR-Fototransistor + 10-kΩ-Widerstand an einem ADC-Erweiterungsport-Pin, keine
      Lötarbeit) liest den echten Gleichspannungspegel und unterscheidet über die
      Rippel-Frequenz STEADY (Nachtsicht-Signatur) von FLICKER (Netzbrumm) von PULSED. Sendet
      nachweislich nie (kein `furi_hal_infrared_async_tx_*`-Aufruf im gesamten Quellbestand).
      Gegen frischen `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`) geprüft: alle
      verwendeten HAL-Symbole (`furi_hal_infrared_async_rx_*`, `furi_hal_adc_*`,
      `gpio_pins[]`/`gpio_pins_count`) sowie das `GpioPinRecord`-Struct-Layout in
      `targets/f7/api_symbols.csv`/`furi_hal_resources.h` identisch vorhanden; eigene
      Pintabelle ausgezählt (sechs nicht-Debug-ADC-Pins: PA7/PA6/PA4/PC3/PC1/PC0) — liegt sicher
      unter dem von der App reservierten `MAX_PROBE_PINS=8`-Puffer, kein Pin geht verloren.
      ~1400 von 2402 Zeilen gezielt gelesen (komplette Kern-Messengine `helpers/ir_sense.c`
      inkl. ISR-Flankenzählung/Präsenz-Hysterese/Probe-ADC-Burst/Ambient-Null/
      Thread-Lebenszyklus, Einstellungs-Persistenz `helpers/nyx_store.c` inkl. v1→v2-Migration
      und SD-Karten-Eingabe-Clamping, Haupt-Eingabeschleife `scenes/nyx_scene_sweep.c` inkl.
      Alarm-Timing-Arithmetik, `scenes/nyx_scene_settings.c`/`_probe.c`) — kein eigener Fund; die
      Kern-Engine ist bereits durch eine ausführliche eigene "Honesty pass" (v1.2, dokumentierte
      Root-Cause-Fixes für Chatter-Verhalten, Presence/Meter-Inkonsistenz, verwaiste
      Worker-Threads) des Autors durchgesehen. Restlicher Umfang (~1000 Zeilen, überwiegend
      Zeichen-/Layout-Code in den Views sowie Splash-/About-/Start-Szenen) für eine künftige
      Sitzung vorgemerkt. Details in `ideas/nyx-ir-hidden-camera-detector-app-status.md`,
      Katalog-Zeile (neue Kategorie "Counter-Surveillance / TSCM") in
      `ideas/pentest-app-katalog.md` ergänzt. Drei Schwester-Werkzeuge desselben Autors/
      Werkzeugsatzes (Specter/NFC, Argus/WiFi, GhostTag/BLE) als weitere Kandidaten derselben
      Kategorie vorgemerkt, noch nicht geprüft.

## Firmware-Feature-Mining (Fortsetzung 8)
- [x] `Next-Flip/Momentum-Firmware`-`dev`-Branch nach der `Loading`-Widget-Prüfung (Fortsetzung 7)
      weiter auf kleine, seither dazugekommene Fixes durchgesehen (`git fetch --depth=200 origin
      dev`). Commit `757cca02` gefunden: „Archive: Fix wrong cursor when opening dirs with more
      than 220 files" (@WillyJL). `applications/main/archive/helpers/archive_browser.c::
      archive_list_item_cb()` platziert den Reset `if(model->item_idx < 0) model->item_idx = 0;`
      innerhalb von `if(model->item_cnt <= BROWSER_SORT_THRESHOLD)` (`BROWSER_SORT_THRESHOLD ==
      220`, `file_browser_worker.h`) — bei Ordnern mit mehr als 220 Einträgen läuft der gesamte
      Block inklusive Reset nie, obwohl `model->item_idx` beim einfachen Hineinnavigieren ohne
      Ziel-Datei laut `browser_folder_init()` standardmäßig `-1` ist. Bei mir (Tag `mntm-012`,
      gegen frischen Klon verifiziert) unverändert vorhanden — Bug bestätigt reproduzierbar. Fund
      bis in die Auswirkung nachvollzogen: `draw_list()`/`archive_browser_view.c` vergleicht
      `model->item_idx == idx` mit stets nicht-negativem `idx` (nie Treffer bei `-1`, keine
      sichtbare Cursor-Hervorhebung beim Öffnen), erste Navigationstaste rechnet mit
      `((model->item_idx - scroll_speed) + model->item_cnt) % model->item_cnt` und springt wegen
      des unerwarteten `-1` statt `0` an eine unerwartete Position statt an den Anfang. Mit
      Host-Repro (reine Kontrollfluss-/Schwellenwert-Arithmetik der Reset-Platzierung, ALT/NEU
      über `item_cnt` 0–500 kreuz `item_idx` -5–5) unter ASan/UBSan bei -O0/-O1 verifiziert: 5511
      Einzelprüfungen (davon 1400 mit reproduziertem Fundfall `item_cnt > 220 && item_idx < 0`),
      ALLE CHECKS BESTANDEN, keine Compiler-Warnungen. Patch (eine Umklammerung, kein neuer Code)
      in `firmware-mining/archive-oversized-folder-cursor-reset-skip-fix/archive_browser_c.patch`
      (`git apply --check` vorab gegen frische Kopie meiner eigenen `G4MEOVER18/G4MEOVER-FW`-Basis
      verifiziert, real angewendet und byte-für-byte gegen den tatsächlichen Momentum-`dev`-Stand
      abgeglichen — identisch bis auf einen unabhängigen, bereits vorher vorhandenen Include aus
      einem anderen Commit), Host-Repro in
      `firmware-mining/repro-archive-oversized-folder-cursor-reset-skip-fix/repro.c`, Details in
      `firmware-mining/archive-oversized-folder-cursor-reset-skip-fix.md`.

## Ideen-Recherche (Fortsetzung 6)
- [x] Von den drei in der Nyx-Sitzung (Fortsetzung 5) vorgemerkten Schwester-Werkzeugen
      desselben Autors (`at0m-b0mb`) **Specter** aufgegriffen: passiver 13,56-MHz-NFC-Feld-Sweep
      gegen aktive Skimmer/Rogue-Leser über das ST25R3916-Feld-Erkennungsbit des eingebauten
      NFC-Chips, fünf Modi (Sweep/Fingerprint/Survey/Watch/Logbook), listen-only, keine
      Zusatzhardware. Frisch geklont (Commit `a2b2e35c`, `main`, `fap_version="3.1.1"`, 5440
      Zeilen Firmware-Quelltext über 36 Dateien plus separater `test/`-Ordner mit sieben
      host-seitigen Testzielen, 527 Einzelprüfungen laut README). Gegen frischen
      `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`, API 87.1) verifiziert: alle sieben
      verwendeten `furi_hal_nfc_*`-Symbole (`_acquire`, `_release`, `_low_power_mode_start/_stop`,
      `_reset_mode`, `_field_detect_start/_stop`, `_field_is_present`) in `api_symbols.csv`
      vorhanden, Signaturen gegen `furi_hal_nfc.h` exakt abgeglichen — kein Symbol-Fehlbestand,
      keine Zielplattform-Sonderfälle wie bei Nyx (kein ADC-Pin/Erweiterungsport nötig). Per Grep
      über den gesamten Quellbestand bestätigt: kein einziger Feld-**Erzeugungs**- oder
      Poller-/Listener-Aufruf — die App sendet nachweislich nie, reine Feld-**Erkennung**.
      ~1820 von 5440 Zeilen gezielt gelesen (komplette messungskritische/sicherheitsrelevante
      Schicht: Sampling-Worker `helpers/field_detector.c` inkl. Thread-Lebenszyklus und
      Kalibrierungs-Zustandsmaschine, Kadenz-Klassifikation `emitter_classify.c`/`cadence.h`,
      `survey_verdict.c`, `field_scale.c`, `ema.h`, `present_hold.h`, `log_wrap.h`,
      `log_filter.h`, SD-Logbuch-Schreibpfad `specter_log.c` inkl. Puffergrößen-/
      Truncation-Prüfung, Einstellungs-Migration `specter_settings.c`, Alarm-/App-Lebenszyklus-
      Verdrahtung `specter.c`, unbeaufsichtigter Dauerbetrieb `specter_scene_watch.c`) — kein
      eigener Fund. Wie bei Nyx bereits durch eine eigene, ungewöhnlich gründliche
      Selbstaudit-Kultur des Autors geprägt: jeder im Code erwähnte historische Bug (EMA-
      Rundungsfehler, unbegrenzt wachsender Kadenz-Ring, Wortumbruch-Fehler im Logbuch-Viewer,
      Off-by-one im Logbuch-Filter nach abgeschnittenem Tail-Read) ist mit Root-Cause und Fix im
      Kommentar dokumentiert. Details, vollständige Codeumfangs-Aufschlüsselung und die beiden
      verbliebenen Kandidaten (Argus/WiFi, GhostTag/BLE) in
      `ideas/specter-nfc-reader-detector-app-status.md`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` (Kategorie Counter-Surveillance/TSCM) ergänzt.

## Firmware-Feature-Mining (Fortsetzung 9)
- [x] Offiziellen Momentum-`dev`-Branch seit dem letzten geprüften Stand (`757cca02`,
      Archive-Cursor-Fix) gegengeprüft: nur ein neuer Commit (`d3f89dfe`), reiner
      `applications/external`-Submodul-Bump (ESP-Flasher-Marauder-Version) plus
      Changelog-Zeile — kein Mining-Kandidat in diesem Repo selbst. Unleashed-`dev` seit dem
      letzten vollständig abgearbeiteten Fenster (`946a52a64`) frisch gegengeprüft
      (`git log --oneline 946a52a64..origin/dev`, 33 neue Commits): die meisten bereits
      bekannt/portiert (`7f372c90`/#1104 SubGHz-TX-Dangling-Pointer,
      `8fdb70dd`/#1112 HID-Jiggler-BLE-Warten, `9e495107`/#1169 Loading-Widget-Label+Bar —
      alle drei bereits in früheren Sitzungen als eigene Funde geliefert), reine
      Chore-/Revert-Commits (Loader-Relaunch-Serie komplett wieder zurückgenommen,
      mehrere "bump apps tag") sowie zwei größere, bereits als Rückstellpunkt dokumentierte
      LF-RFID-/iButton-Schreibfunktions-Kandidaten (`22ce1d9a`/#1146, `49238e1b`/#1148,
      seither um `c74f6b1d`/#1153, `4eaa7aae`/#1164, `65d271d7`/#1165 zu einem noch größeren
      zusammenhängenden Feature-Cluster angewachsen — weiterhin bewusst für eine eigens dafür
      vorgesehene, größere Sitzung zurückgestellt statt überstürzt in Teilen zu portieren).
      Neu und noch nicht geprüft vorgemerkt: `178a99e5` (SubGHz Genius-Fernbedienungen),
      `5643ae0c` (SubGHz Nice-Ice-Protokoll-Neufassung), `16def217`/#1155 (zwei neue
      Menü-Stile, rein kosmetisch), `9effa4b4`/#1149 (RFID-Eingabe nach Facility-Code/
      Kartennummer, HID-/Casi-Rusco-Formate — mit dem bereits dokumentierten Rückstellgrund
      "fehlende Grundinfrastruktur in meiner mntm-012-Basis" gegenzuprüfen).
- [x] Aus `418d802b` (Unleashed PR #1167, "Show progress while a large CUID dictionary loads
      on Read") die zugrunde liegende Fortschritts-Formel als Vorlage genommen, aber nicht
      wörtlich portiert (Unleasheds eigentliche Aufrufstelle,
      `mf_classic_extra_scenes.c`/`views/loading_label.c`, existiert bei mir strukturell
      nicht) — stattdessen den in `loading-widget-label-and-progress-bar.md` selbst benannten
      Rückstellpunkt "Verdrahtung in `nfc_app.c` für das CUID-Dictionary-Label" aufgegriffen
      und dabei einen eigenen, schlimmeren Zustand als Unleasheds Vorher-Stand gefunden: mein
      `nfc_scene_mf_classic_dict_attack.c` zeigt während des synchronen CUID-Dictionary-Scans
      (Bitmap-Aufbau vor dem eigentlichen Umschalten auf `NfcViewDictAttack`) überhaupt keine
      Rückmeldung — nicht einmal einen bloßen Spinner, den Unleashed schon vor PR #1167 hatte.
      Zweite, eigenständige `Loading`-Instanz (`nfc_app_i.h`/`nfc_app.c`:
      `loading_dict`/`NfcViewLoadingDict`/`nfc_show_loading_dict_popup()`/
      `nfc_set_loading_dict_progress()`) plus Verdrahtung in der Scan-Schleife
      (`nfc_scene_mf_classic_dict_attack.c`, Anzeigen vor der Schleife mit Label, Fortschritt
      pro Schlüssel über dieselbe stream-größenbasierte Formel wie Unleashed, Ausblenden nach
      `keys_dict_rewind()` — ein einziger Punkt deckt sowohl den Erfolgs- als auch den
      "Dictionary leer"-Ausgang ab). Setzt den bereits vorgeschlagenen, noch nicht
      angewendeten `loading-widget-label-and-progress-bar`-Patch voraus. Mit Host-Repro
      (Fortschritts-Formel gegen 5000 simulierte Dictionary-Größen inkl. Klemmung bei
      Nicht-Vielfachen der Zeilenlänge und Divide-by-Zero-Schutz; Kontrollfluss-Skelett
      wörtlich aus dem gepatchten `do{}while(false)`-Block übernommen, Anzeigen/Ausblenden/
      Fortschritts-Update-Aufrufe für alle vier Ausgänge — keine Datei, Öffnen fehlgeschlagen,
      leeres Dictionary, Erfolg — geprüft) unter ASan/UBSan bei -O0/-O1 verifiziert:
      12.505.560 Einzelprüfungen, ALLE CHECKS BESTANDEN, keine Compiler-Warnungen. Patches
      (`git apply --check` vorab gegen frische Kopien sowohl von `Next-Flip/Momentum-Firmware`
      (Tag `mntm-012`) als auch meiner eigenen `G4MEOVER18/G4MEOVER-FW`-Basis verifiziert —
      beide an allen drei betroffenen Dateien byte-identisch —, real angewendet und
      byte-für-byte abgeglichen) in
      `firmware-mining/nfc-mfc-cuid-dict-scan-progress-port/{nfc_app_i_h,nfc_app_c,
      nfc_scene_mf_classic_dict_attack_c}.patch`, Host-Repro in
      `firmware-mining/repro-nfc-mfc-cuid-dict-scan-progress/repro.c`, Details in
      `firmware-mining/nfc-mfc-cuid-dict-scan-progress-port.md`.

## Ideen-Recherche (Fortsetzung 7)
- [x] Nächsten Kandidaten aus dem Vierer-Werkzeugsatz von `at0m-b0mb` aufgegriffen (nach Nyx/IR
      und Specter/NFC): **GhostTag** (Anti-Stalking-BLE-Tracker-Erkennung) geprüft, Commit
      `1a3f58b9`, Release 2.0.0. Zwei Betriebsarten: Hunt (echte Erkennung über externen
      BLE-fähigen ESP32-Companion am GPIO-UART — ausdrücklich **nicht** das offizielle
      S2-Devboard, das laut Autor keinen Bluetooth-Funk hat) und Demo (deterministisches
      Sechs-Geräte-Szenario ganz ohne Zusatzhardware). Kernlogik stuft ein Gerät erst nach
      mehreren Sichtungen über ein ununterbrochenes Verweilfenster als Verfolger ein
      (Sichtungslücke > 30 s zählt als neue Begegnung), eigene Apple-Geräte im
      "Owner nearby"-Zustand werden bewusst nie als Bedrohung gewertet. Optionaler
      Rein-Onboard-RF-Belegungsmesser ("Air Check") standardmäßig per Compile-Flag
      deaktiviert — vom Autor selbst im `CHANGELOG.md` dokumentiert und hier unabhängig
      gegen den eigenen `mntm-012`-Header-Stand nachvollzogen: `furi_hal_bt_get_rssi()`
      liefert auf offizieller API-87-Firmware für jeden Messwert exakt `0.0`, unabhängig vom
      gewählten RF-Testmodus.
- [x] API-87-Status verifiziert: alle acht `furi_hal_serial_*`/`expansion_*`-Symbole des
      UART-Links sowie alle neun `furi_hal_bt_*`/`bt_*`-Symbole des (standardmäßig
      deaktivierten) Air-Check-Modus in `targets/f7/api_symbols.csv` vorhanden und gegen die
      jeweiligen Header signaturgleich. ~1716 von 4685 Zeilen Flipper-seitigem Quelltext
      gezielt auf Kernlogik/sicherheitsrelevante Schicht gelesen (`tracker_db.c`,
      `air_check.c`, `uart_link.c`, `gt_parse.c` — der einzige Code, der
      angreiferreichbare Bytes aus dem UART parst —, `session_log.c`, `settings.c`,
      `ble_signatures.c`, `demo_source.c`, `ghosttag.c`): kein eigener Fund — wie bei
      Nyx/Specter bereits durch eine eigene, im `CHANGELOG.md` dokumentierte
      Selbstaudit-Kultur geprägt (u. a. Zwei-Thread-Deadlock beim Teardown und verlorene
      Alerts bei einer zweiten Promotion als vom Autor selbst dokumentierte, bereits
      behobene frühere Funde). Details in
      `ideas/ghosttag-anti-stalking-ble-tracker-app-status.md`, Katalog-Zeile in
      `ideas/pentest-app-katalog.md` (Kategorie Counter-Surveillance/TSCM) ergänzt.

## Firmware-Feature-Mining (Fortsetzung 10)
- [x] Unleasheds `dev`-Branch beim letzten offenen Kandidaten weiterverfolgt: `178a99e5`
      ("subghz: make genius remotes work") gegen meine eigene `faac_slh.c` (Tag `mntm-012`)
      geprüft. Dabei festgestellt: meine Fassung ist beim Ver-/Entschlüsseln bereits nach
      Schlüsseltyp (`KEELOQ_LEARNING_FAAC`) statt wie Unleasheds alte Fassung hart nach dem Namen
      `"FAAC_SLH"` gebaut — grundsätzlich offener für mehrere Hersteller desselben Lerntyps. Genau
      dabei eigener, von Unleasheds Fund unabhängiger Kontrollfluss-Bug gefunden: die Sendefunktion
      (`subghz_protocol_faac_slh_encrypt()`) bricht ihre `M_EACH`-Keystore-Schleife beim ersten
      Namenstreffer korrekt ab, die Empfangs-/Anzeige-/Wiedergabefunktion
      (`subghz_protocol_faac_slh_check_remote_controller()`, aufgerufen aus Serialisieren,
      Bildschirmtext und erneutem Senden einer gespeicherten Datei) hat dagegen kein äußeres
      `break` — `break;` im `switch`-Fall beendet nur den `switch`, die Schleife läuft über den
      gesamten Keystore weiter, sodass bei mehr als einem `KEELOQ_LEARNING_FAAC`-Eintrag
      ausschließlich der **letzte** in Keystore-Reihenfolge gewinnt, unabhängig vom tatsächlich
      sendenden Hersteller — genau die Situation, die Unleasheds Genius-Fix erzeugt (Genius sitzt
      als zweiter Eintrag desselben Lerntyps neben FAAC_SLH). Mit Host-Repro (ALT/NEU-Auswahllogik
      1:1 aus der echten Schleife, fünf Szenarien: ein Eintrag [Regression], Anhängen eines zweiten
      Eintrags [zeigt die stillschweigende Umschaltung bei ALT, Stabilität bei NEU], drei
      gleichtypige Einträge in Folge [Erster-Treffer-Regel], kein Treffer [kein Crash]) unter
      ASan/UBSan bei `-O0`/`-O1` verifiziert: ALT wechselt bei jedem Anhängen eines zweiten
      gleichtypigen Eintrags stillschweigend den für **alle** bereits funktionierenden
      FAAC_SLH-Signale verwendeten Schlüssel, NEU bleibt unverändert beim ersten Eintrag — ALLE
      CHECKS BESTANDEN. Minimaler, für den heutigen (vermutlich einzeiligen) Keystore-Stand
      verhaltensidentischer Fix (lokale `matched_faac_learning`-Variable + `if(...) break;` nach
      dem `switch`, spiegelbildlich zur bereits korrekten Sendefunktion) in
      `firmware-mining/faac-slh-decode-manufacturer-last-match-fix.md`, Unified-Diff (`git apply
      --check` vorab gegen frische Kopie meiner eigenen `G4MEOVER18/G4MEOVER-FW`-Basis verifiziert,
      real angewendet und byte-für-byte abgeglichen) in
      `firmware-mining/faac-slh-decode-manufacturer-last-match-fix/faac_slh_c.patch`, Host-Repro in
      `firmware-mining/repro-faac-slh-decode-manufacturer-last-match/repro.c`. Den eigentlichen
      Genius-Port (optionales `Manufacture`-Dateifeld, neue „Add Manually"-Menüeinträge) bewusst
      als Folgeschritt zurückgestellt: Unleasheds Szenen-/Menü-Dateien
      (`applications/main/subghz/plugins/add_manually/*`) existieren bei mir strukturell nicht,
      das Äquivalent liegt über drei andere Dateien verteilt (`subghz_gen_info.c`,
      `subghz_txrx_create_protocol_key.c`, `subghz_scene_set_type.c`) — Wiring muss lokal neu
      aufgebaut werden; zusätzlich ungeklärt, ob mein eigener (verschlüsselter, SHA-256-different
      zu Unleasheds) Keystore bereits einen "Genius"-Eintrag enthält, aus dem Quelltext nicht
      feststellbar.

## App-Verbesserungen (Fortsetzung 6)
- [x] **heltec-v4-hub**: bislang nicht im eigenen App-Verbesserungs-Katalog geführte eigene
      App aufgenommen (die drei eigenständigen Repos `RollJam`/`RollLab`/`lora-ukfe` waren in
      der Vorsitzung unverändert, daher gezielt nach ungeführtem eigenen Code gesucht). Den
      Heltec-WiFi-LoRa-32-V4-"Masterpiece"-Hub (`src/main.cpp`, `scan_and_relay()`/
      `relay_frame()`/`act()`) frisch geklont und geprüft. `src/ukfe_rf.c`/`.h` (gemeinsames
      Protokoll, laut Dateikommentar "plattformunabhaengig, ohne HW-Abhaengigkeit") Byte-für-
      Byte gegen die bereits in `usb-army-penetrator` geprüfte Fassung verglichen — identisch
      bis auf zusätzliche Kommando-Enum-Werte für V4-eigene Befehle; die Protokoll-Logik selbst
      (CRC16, MAC, Build/Parse-Grenzprüfungen) nochmals vollständig nachgerechnet, kein neuer
      Fund darin. Stattdessen dieselbe bereits zweimal dokumentierte Anbindungslücke ein
      drittes Mal gefunden: `s_flipperCounter` (Anti-Replay-Fenster für Flipper→Hub-Frames) ist
      ein reines RAM-Feld, `setup()` lädt nirgends einen persistierten Stand (gesamten Baum
      nach `Preferences`/`nvs_` durchsucht — kein Treffer) — jeder Hub-Neustart öffnet damit das
      Replay-Fenster für jedes zuvor per SDR mitgeschnittene, gültig signierte Flipper-Frame
      wieder, hier mit größerem Blast-Radius als bei den beiden Vorfunden: der Hub führt den
      wiedergespielten Befehl sowohl selbst aus (`act()`) als auch per ESP-NOW-Broadcast an alle
      Satelliten weiter. Fix (dieselbe Reserve-mit-Sicherheitsspanne-Logik wie in
      `usb-army-penetrator`, hier über die ESP32-Arduino-Standardbibliothek `Preferences`/NVS,
      kein zusätzlicher `lib_deps`-Eintrag nötig) in
      `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md`,
      Unified-Diff (`git apply --check` vorab gegen frische Kopie verifiziert, real angewendet
      und byte-für-byte abgeglichen) in
      `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`.
      Host-Repro (identischer Aufbau zum bereits verifizierten `usb-army-penetrator`-Repro, auf
      `heltec-v4-hub`s eigene `ukfe_rf.c`/`.h` angewendet) unter `gcc -std=c11 -Wall -Wextra
      -fsanitize=address,undefined` bei `-O0`/`-O1` verifiziert: Replay vor Fix angenommen, nach
      Fix abgelehnt, legitime neue Befehle funktionieren weiterhin, NVS-Schreibzugriffe bleiben
      selten (10 statt 50 bei 50 Befehlen) — ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen,
      keine Compiler-Warnungen — in
      `app-improvements/heltec-v4-hub/repro-rf-counter-replay-window-reset/repro.c`. Der
      TX-seitige `s_txCounter` (Hub→Satelliten) bewusst unangetastet gelassen (dort nur
      Verfügbarkeits-, kein Sicherheitsrisiko, wie beim ursprünglichen `lora-ukfe`-Fund) und als
      nicht Gegenstand dieser Sitzung vermerkt.

## App-Verbesserungen (Fortsetzung 7)
- [x] **heltec-v4-hub**: den in der Vorsitzung ausdrücklich zurückgestellten TX-seitigen
      Counter aufgegriffen. `s_txCounter` (eigener Zählerraum für `hub_broadcast()`,
      ausgelöst über die Menü-Taste, Kommando `UkfeRfCmdStatus`) ist wie `s_flipperCounter`
      ein reines RAM-Feld, Startwert `100` — jeder Hub-Neustart setzt ihn zurück. Gegen
      `heltec-ukfe-rx` (frisch geklont, `src/main.cpp:433`, `enowCounter`) verifiziert: die
      Satelliten prüfen Hub-Broadcasts mit derselben strikten `counter <= last_counter`-Regel
      wie relayte Flipper-Frames — nach einem Neustart verwirft ein Satellit damit jeden
      `hub_broadcast()`-Frame, bis `s_txCounter` den zuvor akzeptierten Stand wieder überholt
      hat. Anders als beim RX-Fund keine Sicherheitslücke (reine Selbstaussperrung, identisches
      Muster zum ursprünglichen `lora-ukfe`-Fund `rf-counter-restart-lockout-fix.md`), daher mit
      derselben Reserve-mit-Sicherheitsspanne-Logik behoben, hier unter einem eigenen NVS-Key
      (`txCtr`) im bereits etablierten `Preferences`-Namespace `g4meoverv4`. Fund, Begründung und
      Testhinweis in
      `app-improvements/heltec-v4-hub/tx-broadcast-counter-restart-lockout-fix.md`, Patch
      (`git apply --check` zweimal gegen frische Klone verifiziert, byte-für-byte abgeglichen)
      in `tx-broadcast-counter-restart-lockout-fix/main_cpp.patch`, Host-Repro unter
      `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined` bei `-O0`/`-O1` verifiziert
      (drei Szenarien: Ist-Zustand, Fix, Crash-Sicherheit) in
      `repro-tx-broadcast-counter-restart-lockout/repro.c` — ALLE CHECKS BESTANDEN, keine
      ASan-/UBSan-Meldungen, keine Compiler-Warnungen.
      Zusätzlich festgestellt, nicht in diesem Patch behoben: Satelliten mischen Hub-Relay-
      Frames (`relay_frame()`, Flipper-Zählerraum) und Hub-Broadcasts (`hub_broadcast()`,
      eigener `s_txCounter`-Zählerraum) in ein einziges gemeinsames `last_counter`-Fenster
      (`enowCounter`) — beide Zählerräume können sich damit auch unabhängig von einem Neustart
      gegenseitig blockieren. Eigenständiger, größerer Architektur-Fund (betrifft
      `heltec-ukfe-rx`/`lilygo-ukfe-rx`, nicht nur `heltec-v4-hub`), als Kandidat für eine
      künftige Sitzung vorgemerkt statt in dieser kleinen, abgeschlossenen Änderung mit
      angegangen.

## App-Verbesserungen (Fortsetzung 8)
- [x] **ukfe-esp-now-mesh** (neu im Katalog, übergreifend `heltec-v4-hub` +
      `heltec-ukfe-rx` + `lilygo-ukfe-rx`): den in der Vorsitzung vorgemerkten
      Architektur-Fund aufgegriffen. Die Satelliten prüfen Hub-Broadcasts
      (`hub_broadcast()`, eigener `s_txCounter`-Zählerraum ab `100`) und relayte
      Flipper-Frames (`relay_frame()`, unverändert weitergefunkt, Flipper-eigener
      `s_flipperCounter`-Zählerraum) mit genau **einem** gemeinsamen `enowCounter`-
      Anti-Replay-Fenster (verifiziert gegen frische Klone von `heltec-ukfe-rx:433`
      und `lilygo-ukfe-rx:269` — identische Stelle in beiden). Ein hoher Counter aus
      der einen Quelle sperrt damit künftige, gültige Frames der anderen — **ohne
      dass dafür ein Geräte-Neustart nötig wäre** (eigenständig von den beiden
      bereits vorgeschlagenen Neustart-Fixes für `s_flipperCounter`/`s_txCounter`).
      Fix: zusätzliches, 1 Byte großes Ursprungs-Tag VOR dem `ukfe_rf`-Frame
      (außerhalb des MAC/CRC-geschützten Bereichs, reines Routing-Metadatum) —
      `hub_broadcast()`/`relay_frame()` setzen beim Senden ihr jeweiliges Tag
      (`ESPNOW_ORIGIN_HUBCMD`/`ESPNOW_ORIGIN_RELAY`), die Satelliten lesen es in
      `onEspNowRecv()`, verwerfen unbekannte Werte sofort (vor jedem Parse-Versuch)
      und wählen anhand des Tags eines von zwei getrennten Fenstern
      (`enowCounter`/neu `enowBroadcastCounter`), bevor `ukfe_rf_parse_frame()`
      aufgerufen wird. `ukfe_rf.c`/`.h` (gemeinsames Protokoll, MAC+CRC) bleiben in
      allen drei Repos unverändert. Fund, Begründung, Rollout-Hinweis (alle drei
      Geräte müssen synchron geflasht werden) und Testhinweis in
      `app-improvements/ukfe-esp-now-mesh/espnow-origin-tag-counter-window-split-fix.md`,
      drei Patches (`git apply --check` je einmal gegen frische Klone verifiziert,
      byte-für-byte abgeglichen) in
      `espnow-origin-tag-counter-window-split-fix/{heltec-v4-hub,heltec-ukfe-rx,
      lilygo-ukfe-rx}_main_cpp.patch`, Host-Repro (fünf Szenarien: Ist-Zustand/
      Cross-Source-Lockout, Fix, Replay-Schutz pro Fenster bleibt erhalten,
      unbekanntes/zu kurzes Envelope-Byte wird vor jedem Parse-Versuch verworfen,
      MAC/CRC bleibt unabhängig vom Envelope-Byte wirksam) in
      `repro-espnow-origin-counter-window-split/repro.c`, nutzt die unveränderten
      `ukfe_rf.c`/`.h` aus `heltec-ukfe-rx`. Unter
      `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined` bei `-O0` und `-O1`:
      ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen.

## App-Verbesserungen (Fortsetzung 9)
- [x] **heltec-ukfe-rx**/**lilygo-ukfe-rx**: den in der Vorsitzung vorgemerkten
      restlichen Code beider Satelliten-Repos aufgegriffen (`wifi_attack.cpp/.h`,
      `ble_spam.cpp/.h`, `ducky.h`, `evil_portal.cpp/.h` bei `heltec-ukfe-rx`; beide
      Repos frisch geklont statt den Vorsitzungs-Stand nur anzunehmen). `wifi_attack.cpp`
      (802.11-Deauth/Beacon-Spam, millis()-Wraparound-sicher per Unsigned-Subtraktion),
      `ble_spam.cpp` (Apple/Windows/Android-Pairing-Spam) und `evil_portal.cpp`
      (SoftAP/Captive-DNS/Login-Seite) vollständig gelesen — kein Fund, Pufferlängen
      passen (`send_beacon()`: max. 83 von 128 Byte genutzt), Kanal-/State-Logik
      korrekt. In `ducky.h` (laut eigenem Dateikommentar bewusst "Wiederverwendbar
      (V3/LilyGo/V4)", byte-identisch gegen frische Klone von `heltec-ukfe-rx` UND
      `lilygo-ukfe-rx` bestätigt — Fund und Fix betreffen damit beide Repos)
      `ducky_run()`s Zeilen-Einlese-Schleife geprüft: der feste 220-Byte-Stack-Puffer
      pro physischer Skriptzeile behandelt "Zeile zu Ende" und "Puffer voll" identisch
      — bei einer physischen Zeile über 219 Zeichen (z. B. eine lange `STRING`-Payload,
      die das Format selbst nicht begrenzt) wird das Kommando nicht nur hart
      abgeschnitten, der abgeschnittene Rest wird von der äußeren Schleife beim
      nächsten Durchlauf als **eigenständige neue Kommandozeile** erneut geparst.
      Beginnt dieser Rest zufällig mit einem von `ducky_keyname()`/`ducky_modkey()`
      erkannten Token (z. B. weil die Payload ein normales Wort wie "home"/"tab"/"end"
      enthält), löst das einen unbeabsichtigten zusätzlichen Tastendruck mitten in der
      eigentlich reinen Texteingabe aus — wiederholt sich über beliebig viele
      Pufferbreiten hinweg bis zur echten Zeilengrenze. Minimaler, additiver Fix (Rest
      der physischen Zeile bei Pufferüberlauf verwerfen statt neu zu parsen) in
      `app-improvements/ukfe-ducky-overlong-line-reinterpret-fix.md`, ein Patch (Datei
      in beiden Repos identisch; `git apply --check` je einmal gegen frische,
      unabhängige Klone beider Repos verifiziert, real angewendet und byte-für-byte
      abgeglichen) in `ukfe-ducky-overlong-line-reinterpret-fix/{heltec-ukfe-rx,
      lilygo-ukfe-rx}_ducky_h.patch`. Host-Repro (Zeilen-Splitting-/Dispatch-Logik 1:1
      aus dem echten Funktionskörper transkribiert, Arduino `String`/`USBHIDKeyboard`
      durch `std::string`/einen Aktions-Rekorder ersetzt; fünf Szenarien: Regression
      bei normalen Zeilen inkl. Modifier-Kombos, Überlauf-Grenzfall mit exakt einer
      Pufferbreite [zeigt injiziertes `ENTER`], Überlauf über zwei volle Pufferbreiten
      hinweg [zeigt injiziertes `TAB`, belegt dass sich der Fehler beliebig fortsetzt],
      überlaufende Zeile am Skriptende ohne abschließendes `\n` [zeigt injiziertes
      `HOME`], Zeilen exakt unterhalb der Schwelle unverändert) in
      `repro-ukfe-ducky-overlong-line-reinterpret/repro.cpp` unter
      `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` bei `-O0`/`-O1`
      verifiziert: 26 Einzelprüfungen, ALLE CHECKS BESTANDEN, keine ASan-/UBSan-
      Meldungen, keine Compiler-Warnungen. `heltec-v4-hub` besitzt kein `ducky.h`
      (reiner ESP-NOW-Hub ohne USB-HID) — Fund betrifft nachvollziehbar nur die beiden
      Satelliten-Repos.

## App-Verbesserungen (Fortsetzung 10)
- [x] **heltec-ukfe-rx**: den in der Vorsitzung vorgemerkten restlichen Code
      aufgegriffen (`wifi_recon.cpp/.h`, `lorawan_link.cpp/.h`; Repo frisch
      geklont statt den Vorsitzungs-Stand nur anzunehmen). `lorawan_link.cpp`
      (On-Demand-OTAA-Join + Status-Uplink auf demselben SX1262 wie `ukfe_rf`,
      stellt danach immer den FSK-Empfang wieder her) vollständig gelesen — kein
      Fund. `wifi_recon.cpp::promisc_cb()` vollständig gelesen: alle Zweige außer
      `RC_PROBE` (`RC_HANDSHAKE`, `RC_PACKETMON`, `RC_PWNA`, `RC_RID`) prüfen die
      Längengrenze direkt in der jeweiligen Schleifenbedingung, bevor sie einen
      Index lesen — kein Fund dort. Im `RC_PROBE`-Zweig (SSID aus Probe-Request-
      Frames) wird das SSID-Tag-Byte an Offset 24 unabhängig von `len` gelesen,
      während der Funktions-Guard (`len < 24`) ein Frame mit exakt 24 Byte (reiner
      MAC-Header, keine einzige IE — über die Luft von jedem Gerät in Reichweite
      frei erzeugbar, da `promisc_cb()` jedes empfangene Frame ungefiltert
      durchreicht) passieren lässt: ein Lesezugriff ein Byte hinter der von
      `rx_ctrl.sig_len` gemeldeten Framegrenze. Minimaler, additiver Fix (eine
      zusätzliche `len > 24`-Bedingung im bestehenden `if`) in
      `app-improvements/heltec-ukfe-rx/wifi-recon-probe-ssid-tag-oob-read-fix.md`,
      Patch (`git apply --check` gegen einen frischen, unabhängigen Klon
      verifiziert, real angewendet und byte-für-byte abgeglichen) in
      `wifi-recon-probe-ssid-tag-oob-read-fix/wifi_recon_cpp.patch`. Host-Repro
      (SSID-Tag-Parse-Logik 1:1 aus dem echten Funktionskörper transkribiert,
      `malloc()`-Puffer exakt auf Framelänge statt des ESP-IDF-Promiscuous-
      Puffers, damit AddressSanitizer jeden Zugriff über die gemeldete Framelänge
      hinaus zuverlässig erkennt; fünf Szenarien: der Fund-Grenzfall `len == 24`,
      `len == 25` mit nur dem Tag-Byte, eine normale benannte SSID, eine
      Broadcast-Probe mit SSID-Länge 0, eine maximale 32-Byte-SSID — für die vier
      Nicht-Grenzfall-Szenarien zusätzlich Byte-für-Byte-Diff zwischen
      unveränderter und gefixter Logik bestätigt: identische Ausgabe) in
      `repro-wifi-recon-probe-ssid-tag-oob-read/repro.cpp` unter
      `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` bei `-O0`/`-O1`
      verifiziert: gefixte Logik — ALLE CHECKS BESTANDEN, keine ASan-/UBSan-
      Meldungen, keine Compiler-Warnungen; unveränderte Logik — AddressSanitizer
      meldet beim Grenzfall-Szenario zuverlässig `heap-buffer-overflow READ of
      size 1`. `lilygo-ukfe-rx`/`heltec-v4-hub` besitzen kein `wifi_recon.cpp`
      (reine ESP-NOW-/USB-HID-Satelliten bzw. -Hub ohne eigenes WiFi-
      Promiscuous-Sniffing) — Fund betrifft nachvollziehbar nur `heltec-ukfe-rx`.

## Firmware-Feature-Mining (Fortsetzung 11)
- [x] Den in `faac-slh-decode-manufacturer-last-match-fix.md` zurückgestellten Genius-Port
      aufgegriffen: Unleasheds `dev`-Commit `178a99e5` („subghz: make genius remotes work",
      @xMasterX) vollständig portiert. Neuer statischer Helfer
      `subghz_protocol_faac_slh_get_key()` in `lib/subghz/protocols/faac_slh.c` ersetzt die
      bisherigen zwei Keystore-Schleifen (Senden/Empfangen) durch eine einzige, namensbasierte
      Suche mit Fallback auf `FAAC_SLH` bei leerem/unbekanntem Dateiwunsch — behebt dabei den
      dort dokumentierten Auswahl-Bug strukturell mit (beide Patches schließen sich gegenseitig
      aus, siehe Risiko-Abschnitt der neuen `.md`). Neues optionales `Manufacture`-Textfeld in
      der `.sub`-Datei (Lesen/Schreiben an allen vier Aufrufstellen von
      `check_remote_controller()`, Rewind-Muster identisch zu den bereits vorhandenen
      `Seed`/`AllowZeroSeed`-Feldern), `decoder_get_string()` zeigt bei einem Nicht-FAAC_SLH-
      Treffer den Herstellernamen statt „Faac SLH". Unleasheds `applications/main/subghz/plugins/
      add_manually/*`-Pfade existieren bei mir strukturell nicht; stattdessen die drei
      funktional entsprechenden eigenen Dateien identifiziert und darin je einen neuen
      Zwei-Fälle-Block ergänzt: `applications/main/subghz/helpers/subghz_custom_event.h`
      (`SetTypeGeniusSLH_433`/`_868`), `applications/main/subghz/helpers/subghz_gen_info.c`
      (zwei neue `GenFaacSLH`-Fälle mit `.manuf = "Genius"`),
      `applications/main/subghz/scenes/subghz_scene_set_type.c` (zwei neue Menüeinträge „Genius
      SLH 433/868MHz") sowie eine fehlende `Manufacture`-Schreibzeile in
      `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c`
      (`subghz_txrx_gen_faac_slh_protocol()`) — ohne diese hätte eine über „Add Manually"
      erzeugte Genius-Datei beim nächsten Laden wieder als FAAC_SLH gegolten. Fünf Patches (`git
      apply --check` je vorab gegen eine frische, unabhängige Kopie meiner eigenen
      `G4MEOVER18/G4MEOVER-FW`-Basis verifiziert, real angewendet und byte-für-byte mit der
      beabsichtigten Endfassung abgeglichen) in
      `firmware-mining/faac-slh-genius-manufacture-field-port/*.patch`. Host-Repro
      (`subghz_protocol_faac_slh_get_key()` sowie die `display_name`-Auswahl aus
      `decoder_get_string()` 1:1 transkribiert, Mock-Keystore statt `M_EACH`/
      `SubGhzKeyArray_t`; acht Szenarien: Regression mit genau einem Eintrag, der eigentliche
      Fund — ALT wählt bei FAAC_SLH+Genius ohne Dateiwunsch den letzten Eintrag [Genius], NEU
      weiterhin FAAC_SLH —, Dateiwunsch „Genius" bei zwei Keystore-Reihenfolgen, Fallback auf
      FAAC_SLH bei fehlendem Genius-Eintrag im Keystore [deckt die unten offene Unsicherheit ab],
      leerer Keystore ohne Absturz, drei Einträge mit Treffer in der Mitte, Nicht-FAAC-Lerntypen
      werden übersprungen, drei `display_name`-Fälle) unter
      `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` bei `-O0`/`-O1` verifiziert: 21
      Einzelprüfungen, ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine
      Compiler-Warnungen — in
      `firmware-mining/repro-faac-slh-genius-manufacture-field/repro.c`. Weiterhin offen (bereits
      in der Vorsitzung benannt, aus dem Quelltext nicht feststellbar, da der Keystore
      verschlüsselt ist): ob mein eigener Keystore bereits einen „Genius"-Eintrag vom Typ
      `KEELOQ_LEARNING_FAAC` enthält — fehlt er, bleiben die neuen Menüpunkte nutzbar, aber
      wirkungslos (sauberer Fallback auf `FAAC_SLH`, kein Absturz, kein falscher Treffer).

## Ideen-Recherche (Fortsetzung 12)
- [x] Letzten offenen Kandidaten aus dem vierteiligen `at0m-b0mb`-Gegenüberwachungs-
      Werkzeugsatz aufgegriffen: **Argus** (`at0m-b0mb/Argus-FlipperZero`, Commit
      `bfedeac9`, Tag `v1.0`) — WiFi-Deauth-/Disassoc-Flut- und Evil-Twin-AP-Erkennung
      über einen externen ESP32 am GPIO-UART. Vollständig gelesen: Flipper-`.fap`
      (2303 Zeilen über 20 C-Dateien) sowie die separate ESP32-Begleitfirmware
      `esp32/argus_esp32/argus_esp32.ino` (256 Zeilen) — zusammen 100 % des
      Quellbestands. API-87-Status des Flipper-Teils gegen den eigenen
      `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`) verifiziert (dieselbe
      Expansion-Service-Übernahme/`furi_hal_serial_*`-USART-Anbindung wie bei UART
      Sniff/RS485 Modbus/GPIO Logic Analyzer/Rogue AP Detector bereits etabliert),
      kein eigener Fund im Flipper-Teil. In der ESP32-Begleitfirmware
      (`argus_esp32.ino::sniffer_cb()`) einen Fund gemacht: die SSID-Tag-Extraktion
      aus Beacon-/Probe-Response-Frames klemmt die vom Sender deklarierte Tag-Länge
      nur gegen die feste Zielpuffergröße (32 Byte), nicht gegen die tatsächlich im
      empfangenen Frame nach dem Tag-Header verbleibenden Bytes (`body_len - 2`) —
      ein Sender kann ein SSID-Information-Element mit deklarierter Länge 32 in einem
      kürzeren Frame unterbringen und damit einen Overread im ESP32-Empfangspuffer
      auslösen, dieselbe Fehlerklasse wie der bereits in
      `app-improvements/heltec-ukfe-rx/wifi-recon-probe-ssid-tag-oob-read-fix.md`
      gefundene und gefixte SSID-Tag-OOB-Read in einem anderen eigenen ESP32-Projekt.
      Fix-Skizze (zusätzliche `avail = body_len - 2`-Klemmung vor der bestehenden
      32-Byte-Klemmung), Fund, Nutzen, Aufwand und Testhinweis in
      `ideas/argus-wifi-deauth-evil-twin-watchdog-app-status.md`. Host-Repro
      (`extract_ssid_old()`/`extract_ssid_new()` 1:1 aus dem echten
      `sniffer_cb()`-SSID-Tag-Block transkribiert, `body`-Puffer exakt auf `body_len`
      Byte alloziert, je Szenario in einem eigenen `fork()`-Kindprozess; zwei
      Regressionsfälle, zwei Fund-Szenarien — verkürztes Frame mit `avail=8` bei
      deklarierter Länge 32, Header-only-Frame mit `avail=0` — sowie ein
      Äquivalenztest über `body_len` 2–40 × `declared_len` 0–32 für alle
      Normalfälle) unter `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined`
      bei `-O0`/`-O1` verifiziert: ALT crasht beide Fund-Szenarien reproduzierbar mit
      `AddressSanitizer: heap-buffer-overflow` exakt am jeweiligen Allokationsende,
      NEU übersteht beide ohne Overread, Regressionsfälle und Äquivalenztest liefern
      byte-identische Ausgabe zwischen ALT und NEU — ALLE CHECKS BESTANDEN, keine
      weiteren ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `ideas/repro-argus-ssid-tag-oob-read/repro.c`. `ideas/pentest-app-katalog.md`
      um eine neue Zeile in der Tabelle „Counter-Surveillance/TSCM" sowie einen neuen
      Log-Eintrag ergänzt. Damit ist der vierteilige `at0m-b0mb`-Gegenüberwachungs-
      Werkzeugsatz (Nyx/Specter/GhostTag/Argus) vollständig geprüft.

## Ideen-Recherche (Fortsetzung 13)
- [x] Den in `ghosttag-anti-stalking-ble-tracker-app-status.md` zurückgestellten Rest
      aufgegriffen: GhostTag (`at0m-b0mb/GhostTag-FlipperZero`, Commit `1a3f58b9`, Release
      `2.0.0`) vollständig fertig gelesen. Zuvor geprüft waren ~1716 der 4685 Zeilen
      (`helpers/*.c` sowie `ghosttag.c`); in dieser Sitzung den gesamten Rest nachgezogen: alle
      sechs `views/*_view.c`-Dateien (Zeichenroutinen, Eingabe-Callbacks, die `memcmp()`-basierte
      Wiederauffindung des ausgewählten Geräts nach jedem Re-Sort in `device_list_view.c`), alle
      zehn `scenes/ghosttag_scene_*.c`-Dateien (Navigationslogik, Alert-Priorität vor
      Neuzeichnen, bedingte Zeilen-Index-Verwaltung des per Compile-Flag ein-/ausblendbaren Air-
      Check-Menüpunkts, Einstellungen-Persistenz) sowie `ghosttag_i.h`. Die verbleibenden
      Header-Dateien gezielt per Grep auf Funktionslogik (`static inline`, Funktions-Makros)
      durchsucht — keine Treffer außerhalb reiner Konstanten und des Standard-Flipper-
      ADD_SCENE-Boilerplate-Makros. Kein eigener Fund: jede Index-/Array-Schleife ist durch ein
      vorab geklemmtes `n = min(count, Kapazität)` gedeckt (`device_list_view_set_records()`,
      `radar_view_set_data()`), jede `snprintf()`-Formatbreite durch eine vorab geklemmte Zahl
      bewiesen (`clamp_count()`, `format_dwell()` mit expliziter 99m+-Sonderbehandlung statt
      einer unbewiesenen Laufzeitbreite), jede Eingabebehandlung akzeptiert `InputTypeShort` und
      `InputTypeLong` gleich (dokumentiert: die Firmware liefert nach der Long-Press-Schwelle nur
      noch `Long`). Das CHANGELOG dokumentiert mehrere frühere eigene Funde des Autors in genau
      diesen jetzt geprüften Dateien (Zwei-Thread-Deadlock beim Teardown, verlorene
      Long-Press-Eingaben, abgeschnittene vierte Listenzeile, verschmelzende Zeichen im
      FOLLOWING-YOU-Banner) — alle bereits selbst gefunden und gefixt vorgefunden. Damit ist
      GhostTag nach Nyx, Specter und Argus das vierte und letzte Werkzeug des `at0m-b0mb`-
      Vierer-Satzes, dessen Flipper-seitiger Quelltext vollständig durchgesehen ist.
      `ideas/ghosttag-anti-stalking-ble-tracker-app-status.md` und `ideas/pentest-app-katalog.md`
      entsprechend aktualisiert.

## Firmware-Feature-Mining (Fortsetzung 14)
- [x] NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`) erneut aufgegriffen, obwohl
      laut Sektor-Analyse-Sitzung bereits als vollständig durchgesehen vermerkt (einzig
      `chameleon`/`nfc_relay` als Fremd-/Zweitgerät-Fälle offen) — gezielt `core/nfc_tools_icode.c`
      (635 Zeilen) ein zweites Mal gegen dieselbe Fehlerklasse geprüft, die bereits in
      `firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md` einen Fund in `iso15693_write_ndef_blocks()`
      ergab. Fund: `cc_expected` ist ein lokales 4-Byte-Array (die kurze NFC-Forum-Type-5-Tag-CC), wird
      aber zusammen mit der tag-gemeldeten `block_size` an `nfcv_build_write_blk()` übergeben, die davon
      `block_size` Bytes per `bit_buffer_append_bytes()` kopiert — bei jedem ISO15693-Tag mit einer laut
      ISO/IEC 15693-3 gültigen, aber von 4 Byte abweichenden Blockgröße (5-Bit-Systeminfo-Feld, 1-32 Byte
      möglich, wie bereits in `firmware-mining/nfc-iso15693-read-multi-blocks-and-parser-overrun-fix.md`
      als variabler Wert bestätigt) ein Stack-Overread von bis zu 28 Byte, dessen Inhalt anschließend als
      Teil des WRITE_BLOCK-Frames über das HF-Feld gesendet wird. Die vorherige CC.MLEN-Sitzung hatte
      dieselbe Funktion und sogar dieselbe Zeile bereits geprüft, aber nur die Blockgrößen-Abhängigkeit
      der *Format*-Callbacks als spekulativen Kandidat zurückgestellt — die hier betroffene
      NDEF-Schreibfunktion dabei nicht erfasst. Fix: `cc_expected` auf 32 Byte (volle laut Standard
      mögliche Blockgrößenspanne) vergrößert, führende Nullen, plus defensive Klammerung der gesendeten
      Länge auf die Puffergröße an der Aufrufstelle — kompatibel mit dem noch nicht als Patch vorliegenden
      CC.MLEN-Fix (unterschiedliche Zeilen derselben Funktion). Patch (`git apply --check` vorab gegen
      eine frische, unabhängige Kopie von `RogueMaster/flipperzero-firmware-wPlugins` Branch `420`
      verifiziert, real angewendet und byte-für-byte abgeglichen) in
      `firmware-mining/nfctools-icode-cc-write-oob-read-fix/nfc_tools_icode_c.patch`. Host-Repro
      (`nfcv_build_write_blk()` sowie der CC-Schreibzweig 1:1 transkribiert, `cc_expected` wie im echten
      Aufrufkontext auf dem Stack alloziert, drei Szenarien: Regression bei Blockgröße 4, Fund bei
      Blockgröße 8, Fund bei der laut Standard maximalen Blockgröße 32, dazu ein Äquivalenztest der
      CC-Bytes über `block_count` 2–256 bei Blockgröße 4) unter
      `gcc -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALT crasht beide
      Fund-Szenarien reproduzierbar mit `stack-buffer-overflow` exakt am Ende von `cc_expected`, NEU
      übersteht beide, Äquivalenztest ALT/NEU identisch — ALLE CHECKS BESTANDEN, keine weiteren
      ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `firmware-mining/repro-nfctools-icode-cc-write-oob-read/repro.c`. Dabei `cc_current`/den
      `padded`-NDEF-Datenblockpuffer gegengeprüft (beide korrekt an `block_size` gekoppelt bzw. von ihr
      unabhängig) — kein weiterer Fund. `ideas/pentest-app-katalog.md` um eine Korrektur ergänzt: die dort
      zu `nfctools` vermerkten "~10 100/~11 200 Zeilen offen" sind veraltet, der Cluster ist laut dieser
      Sitzung tatsächlich bereits vollständig durchgesehen (nur `chameleon`/`nfc_relay` als
      Fremd-/Zweitgerät-Fälle bleiben offen) — dieser Fund selbst zeigt, dass ein gezielter zweiter Blick
      auf eine bereits "vollständig" geprüfte Datei trotzdem noch etwas finden kann, wenn er auf eine
      andere Fehlerklasse zielt als der erste Durchgang.

## Firmware-Feature-Mining (Fortsetzung 15)
- [x] NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`) fortgesetzt: den in
      `firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md` bereits vollständig analysierten und per
      Host-Repro bestätigten CC.MLEN-Fund (`core/nfc_tools_icode.c::iso15693_write_ndef_blocks()`, bei
      großer Tag-Kapazität läuft das einzelne CC.MLEN-Byte unbemerkt um statt den Schreibvorgang
      abzulehnen) hatte bislang keinen eigenen `.patch` — nur Analyse und Repro. Für diese Sitzung den
      fehlenden Patch nachgereicht statt eine neue Codefläche zu öffnen.
  - Patch exakt nach der bereits dokumentierten Port-Skizze gegen eine frische, unabhängige Kopie von
    `RogueMaster/flipperzero-firmware-wPlugins` Branch `420` erzeugt (`git apply --check` davor
    verifiziert, real angewendet, byte-für-byte mit der Skizze abgeglichen) in
    `firmware-mining/nfctools-icode-cc-mlen-overflow-fix/nfc_tools_icode_c.patch`: vor dem Cast von
    `mlen` auf `uint8_t` eine Bereichsprüfung auf die 32-Bit-Zwischengröße, bei Überschreitung von
    `0xFF` bricht die Funktion mit "Tag too large!" ab statt eine defekte Capability Container zu
    schreiben. Bereits vorliegendes Host-Repro
    (`firmware-mining/repro-nfctools-icode-cc-mlen-overflow/repro.c`) erneut unter
    `gcc -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` ausgeführt: identische Ausgabe wie in
    der ursprünglichen Fund-Sitzung, ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine
    Compiler-Warnungen.
  - Geprüft, ob sich dieser neue Patch mit dem bereits vorliegenden
    `firmware-mining/nfctools-icode-cc-write-oob-read-fix/nfc_tools_icode_c.patch` (ändert denselben
    kurzen Codeblock, aber einen disjunkten Teilausdruck: dort `cc_expected`-Größe/Schreiblänge, hier
    die `mlen`-Berechnung davor) per `git apply` hintereinander anwenden lässt: lehnt beide wegen
    überschneidendem Diff-Kontext ab. Probeweise von Hand zu einer gemeinsamen Fassung zusammengeführt
    und geprüft: kein semantischer Konflikt, beide Änderungen betreffen getrennte Teilausdrücke
    derselben Zeilen — beide `.md`-Dateien um einen entsprechenden Hinweis ergänzt (von Hand
    zusammenführen statt beide `.patch`-Dateien nacheinander mit `git apply`/`patch` anwenden).
  - Damit liegen jetzt für beide bislang dokumentierten `core/nfc_tools_icode.c`-Funde eigenständige,
    unabhängig voneinander anwendbare Patches vor.

## Firmware-Feature-Mining (Fortsetzung 16)
- [x] Den in `ideas/i2ctools-app-status.md` ("Quelltext-Review (Speichersicherheit) und
      gefundene Einschränkung") bereits dokumentierten, aber damals bewusst ohne eigenen
      Fix-Vorschlag zurückgestellten Fund im I2C-Sniffer von `NaejEL/flipperzero-i2ctools`
      aufgegriffen — der Verzicht war eine Fehleinordnung als reine Dritt-App-Einschränkung,
      tatsächlich dieselbe Fehlerklasse (fehlende Kapazitätsprüfung vor einem Pufferzugriff),
      die bei allen anderen `applications/external`-Funden in diesem Repo einen Port-Vorschlag
      inkl. Fix erhalten hat. In `i2csniffer.c::SCLcallback()` zählt `data_index` (`uint8_t`,
      Feld von `i2cFrame`) im ACK-Zweig jedes vollständigen Bytes unbedingt hoch, ohne Prüfung
      gegen `MAX_MESSAGE_SIZE` (128) — anders als `frame_index`, der vor jedem Gebrauch gegen
      `MAX_RECORDS` geprüft wird. Eine einzelne I2C-Transaktion mit mehr als 128 Byte ohne
      zwischenzeitliche Stop-/Start-Bedingung lässt `data_index` auf 128 wachsen und indiziert
      `data[]`/`ack[]` (je `uint8_t[128]`/`bool[128]`) außerhalb ihrer deklarierten Grenzen.
      Dabei die bisherige Einordnung präzisiert: `data`/`ack` sind zwei direkt
      aufeinanderfolgende Arrays desselben `i2cFrame`-Structs (zusammen 256 Byte) — der
      `uint8_t`-Index bleibt auf dieses Feldpaar begrenzt und erreicht nie den nächsten
      `frames[]`-Eintrag, anders als zuvor vermerkt.
  - Fix: additive Grenzprüfung (`if(data_idx >= MAX_MESSAGE_SIZE) return;`) direkt vor dem
    bestehenden Zugriff in `SCLcallback()` — der Frame friert bei 128 Byte ein (keine weiteren
    Bits/Bytes dieser einen Transaktion werden aufgezeichnet) statt `data[]`/`ack[]`
    außerhalb ihrer Grenzen zu indizieren. Patch (`git apply --check` vorab gegen eine frische,
    unabhängige Kopie von `NaejEL/flipperzero-i2ctools`, Commit `1840c30` — unverändert seit
    der letzten Prüfung — verifiziert, real angewendet und byte-für-byte abgeglichen) in
    `firmware-mining/i2ctools-sniffer-data-index-oob-fix/i2csniffer_c.patch`.
  - Host-Repro (`SCLcallback()` 1:1 transkribiert, GPIO-Lesewert durch Funktionsparameter
    ersetzt; fünf Szenarien: Regression bei 20 Byte, Grenzfall bei exakt 128 Byte, Fund beim
    129. Byte auf dem letzten Frame-Slot in einem eigenen `fork()`-Kindprozess, Fix-Nachweis
    mit 129. Byte plus 300 weiteren Taktpulsen ohne Crash, Äquivalenztest über alle
    Transaktionslängen 1–128 Byte) unter
    `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
    -fno-sanitize-recover=undefined` verifiziert: ALT bricht im Fund-Szenario reproduzierbar
    mit `UndefinedBehaviorSanitizer: runtime error: index 128 out of bounds for type
    'uint8_t [128]'` ab, NEU übersteht dasselbe Szenario ohne Fehler, Regressions- und
    Grenzfall-Szenario sowie Äquivalenztest liefern byte-identische Ausgabe zwischen ALT und
    NEU — 10/10 Einzelprüfungen bestanden, ALLE CHECKS BESTANDEN, keine weiteren ASan-/
    UBSan-Meldungen, keine Compiler-Warnungen — in
    `firmware-mining/repro-i2ctools-sniffer-data-index-oob/repro.c`.
  - `ideas/i2ctools-app-status.md` (Fund-Beschreibung präzisiert, Verweis auf den jetzt
    vorliegenden Fix ergänzt, "Offen"/"Sicherheitsrahmen"-Abschnitte aktualisiert) und
    `ideas/pentest-app-katalog.md` (Tabellenzeile aktualisiert) entsprechend angepasst.
  - Repo-Zustand zu Sitzungsbeginn korrigiert: lokaler `main`-Branch-Zeiger stand auf
    `6d0d064` (13 Commits hinter `origin/main`, HEAD war stattdessen detached auf dem
    aktuellen Stand `5401c8d`) — siebter Fund dieser Art, per Fast-Forward behoben.

## Firmware-Feature-Mining (Fortsetzung 17)
- [x] Einen der beiden verbliebenen, bisher pauschal als "Fremd-/Zweitgerät-Fall" zurückgestellten
      Kandidaten aus dem restlichen NFC-Werkzeug-Cluster aufgegriffen: **Chameleon Ultra Client**
      (`muylder/Chameleon_Flipper`, über das RogueMaster-Bündel, `applications/external/chameleon/`).
      Reiner Fernsteuerungsclient für ein externes Chameleon-Ultra-Gerät ohne eigenen
      NFC-Hardwarezugriff, aber mit einem eigenen Protokoll-Parser für dessen Antworten — dort
      geprüft statt den Fremdhardware-Status unbesehen zu übernehmen.
  - Fund: `lib/chameleon_protocol/chameleon_protocol.c::chameleon_protocol_parse_frame()`
    extrahiert das LEN-Antwortfeld (big-endian `uint16_t`, 0–65535, von der Gegenseite der
    USB-CDC-Verbindung frei wählbar — korrekt gegengecheckte LRC2/LRC3 sind reine Prüfsummen,
    kein Geheimnis) und verwendet es ungeprüft als `memcpy()`-Länge in den vom einzigen
    Aufrufer `chameleon_app_rx_callback()` übergebenen `uint8_t data_buffer[512]`
    (`== CHAMELEON_MAX_DATA_LEN`, Teil des `malloc()`-allozierten `ChameleonApp`-Structs) —
    anders als die Sendefunktion `chameleon_protocol_build_frame()` im selben File, die genau
    diese Prüfung für den umgekehrten Weg bereits hat. Jede Antwort mit `LEN > 512` schreibt bis
    zu 65023 Byte über das Ende des Zielpuffers hinaus in die anschließenden Struct-Felder —
    ein echter Heap-Buffer-Overflow, kein reiner `furi_check()`-Abbruch. Den tatsächlichen
    BLE-Pfad gegengeprüft: laut eigenem Code-Kommentar noch ein Stub ohne echte
    GATT-Verbindung, der praktisch nutzbare Auslösekanal ist ausschließlich USB-CDC
    (`lib/uart_handler/uart_handler.c`).
  - Fix: additive Grenzprüfung (`if(*data_len > CHAMELEON_MAX_DATA_LEN) return false;`) direkt
    nach dem Extrahieren von LEN, vor LRC2-Prüfung und `memcpy()` — identisches Muster zur
    bereits vorhandenen Prüfung in `chameleon_protocol_build_frame()`. Patch (`git apply --check`
    vorab gegen eine frische, unabhängige Kopie von `RogueMaster/flipperzero-firmware-wPlugins`,
    Branch `420`, verifiziert, real angewendet und byte-für-byte mit der beabsichtigten
    Endfassung abgeglichen) in
    `firmware-mining/chameleon-protocol-parse-frame-oob-write-fix/chameleon_protocol_c.patch`.
  - Host-Repro (SOF/LRC1/CMD/STATUS/LEN/LRC2-Extraktion und DATA-`memcpy()` 1:1 transkribiert;
    acht Szenarien: Regression bei 20 Byte und beim Grenzfall exakt 512 Byte, Fund bei
    `LEN = 513`/`2000`/`65535` je in einem eigenen `fork()`-Kindprozess, Fix-Nachweis bei
    `LEN = 513`/`65535` mit Wachposten-Muster im Zielpuffer, Äquivalenztest über `data_len`
    0–512 Byte) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined`
    verifiziert: ALT bricht alle drei Fund-Szenarien reproduzierbar mit
    `AddressSanitizer: heap-buffer-overflow ... 0 bytes after 512-byte region` ab, NEU übersteht
    dieselben Antworten ohne jeden Schreibzugriff auf den Zielpuffer, Regressions- und
    Äquivalenztest liefern byte-identische Ausgabe zwischen ALT und NEU — 14/14 Einzelprüfungen
    bestanden, ALLE CHECKS BESTANDEN, keine weiteren ASan-/UBSan-Meldungen, keine
    Compiler-Warnungen — in
    `firmware-mining/repro-chameleon-protocol-parse-frame-oob-write/repro.c`.
  - `ideas/pentest-app-katalog.md`: neuen Log-Eintrag ergänzt, letzter verbliebener
    Cluster-Kandidat jetzt nur noch `nfc_relay` (zwei eigene Flipper-Geräte, UART-Direktverbindung,
    weiterhin zurückgestellt).

## Firmware-Feature-Mining (Fortsetzung 18)
- [x] Phase 2 des Nice-O-Code-Ports aus der Fortsetzung-5-Sitzung aufgegriffen: die dort
      verifizierten, additiven und bis dahin unverwendeten Hilfsfunktionen
      (`subghz_protocol_nice_flor_s_encrypt_ic()`/`_decrypt_ic()`, `subghz_protocol_nice_o_mask()`,
      `subghz_protocol_nice_o_get_parcel()`) jetzt tatsächlich verdrahtet — aber bewusst nur den
      risikoärmsten Ausschnitt aus der dortigen Port-Skizze: Struct-Erweiterung
      (`SubGhzProtocolDecoderNiceFlorS` um `ic`/`o_code`), `.sub`-I/O für ein neues optionales
      `IC`-Hex-Feld, und die Decoder-Anzeige (`get_string()`). `feed()` (Live-Zwei-Pakete-Heuristik
      am bereits produktiven Plain-FloR-S-Empfangspfad) und der Encoder (`get_upload()`) bleiben
      wie in der Vorsitzung dokumentiert zurückgestellt — beide unverändert.
  - `reset()` setzt `ic`/`o_code` ebenfalls zurück (Port-Skizze Schritt 1), damit eine zuvor
    geladene O-Code-Datei nicht den Zustand einer späteren, unabhängigen Live-Aufnahme in
    derselben Decoder-Instanz verfälscht. `deserialize()`/`serialize()` lesen/schreiben das neue
    `IC`-Feld nach demselben Rewind-Muster wie das bestehende `CounterMode`-Feld — fehlt es (jede
    bisherige Datei), bleibt der Zustand exakt wie nach `alloc()`. `get_string()` bekommt eine
    neue Verzweigung an der Spitze: bei `o_code` wird Paketindex → Maske → `decrypt_ic()` auf den
    unteren 44 Bit von `instance->generic.data` angewandt (identisches Schema zur bereits in der
    Vorsitzung verifizierten Pipeline), `btn` weiterhin direkt aus dem unverschlüsselten
    Rahmenbyte; ohne `o_code` bleibt der bestehende `remote_controller()`-Aufruf und die
    bestehenden `furi_string_cat_printf()`-Formatstrings byte-identisch unverändert, nur um eine
    zusätzliche `IC:%04X`-Zeile ergänzt.
  - Patch (baut auf dem bereits vorliegenden Phase-1-Patch der Vorsitzung auf, Anwendungsreihenfolge
    dokumentiert; `git apply --check` beider Patches hintereinander sowie real angewendet gegen
    eine frische, unabhängige Kopie von `Next-Flip/Momentum-Firmware` Tag `mntm-012` verifiziert,
    Ergebnis byte-für-byte mit der Sitzungs-Arbeitskopie abgeglichen) in
    `firmware-mining/nice-o-code-decode-display-port/nice_flor_s_c.patch`.
  - Host-Repro (`alloc()`/`reset()`/`deserialize()`/`serialize()`/`get_string()` neue Zeilen 1:1
    als eigene Funktionen transkribiert, nicht nur die Chiffre wie in der Vorsitzung; fünf
    Prüfschritte: Rückwärtskompatibilitäts-Regression bei fehlendem `IC`-Feld, O-Code
    Ende-zu-Ende über die echten Integrationspunkte inkl. Feld-Rückschreiben und Falsch-IC-Probe,
    `reset()`-Lösch-Nachweis im genauen Cross-Contamination-Szenario, Symmetrie des
    Feld-Fehlens beim Schreiben, ein durchgerechnetes Beispiel) unter
    `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert:
    **1 621 999 Prüfungen, ALLE CHECKS BESTANDEN**, keine Compiler-Warnungen, keine
    ASan-/UBSan-Meldung — in `firmware-mining/repro-nice-o-code-decode-display/repro.c`.
  - Damit liegt für Nice O-Code jetzt eine tatsächlich nutzbare (wenn auch auf das Laden bereits
    IC-versehener Dateien beschränkte) Erweiterung vor statt nur toter, unverdrahteter Krypto-
    Grundlagen. Verbleibend offen: `feed()`-Live-Erkennung (Phase 3, berührt den produktiven
    Empfangspfad), Encoder-Gegenstück (Senden), Installateurcode-Recovery-App — alle drei in
    `firmware-mining/nice-o-code-decode-display-port.md` unter "Offen" benannt.

## Firmware-Feature-Mining (Fortsetzung 19)
- [x] Phase 3 des Nice-O-Code-Ports aufgegriffen: das in Phase 1 unter „Port-Skizze Schritt 5"
      benannte Encoder-Gegenstück zu Phase 2 (Decoder-Anzeige) tatsächlich verdrahtet — eine
      bereits mit `IC`-Feld versehene `.sub`-Datei lässt sich jetzt senden, nicht nur anzeigen.
      `feed()` (Live-Erkennung am produktiven Empfangspfad) und die
      Installateurcode-Recovery-App bleiben wie in allen Vorsitzungen dokumentiert
      zurückgestellt — dieser Patch berührt ausschließlich den Encoder.
  - `SubGhzProtocolEncoderNiceFlorS` um `ic`/`o_code` erweitert (Pendant zum Decoder-Struct aus
    Phase 2). `subghz_protocol_encoder_nice_flor_s_deserialize()` liest optional ein `IC`-Feld
    und rekonstruiert bei dessen Vorhandensein `serial`/`cnt`/`btn` aus dem geladenen
    `Key`-Rahmen über Paketindex → Maske → `decrypt_ic()` (dieselbe in Phase 2 verifizierte
    Pipeline) statt über die bestehende `remote_controller()`, die nur Plain-Flor-S kennt.
    `subghz_protocol_encoder_nice_flor_s_get_upload()` berechnet bei `o_code` den
    Chiffretext jedes der 16 pro Tastendruck gesendeten Pakete einzeln neu (Maske + IC +
    `encrypt_ic()`) statt den einmal berechneten Chiffretext 16-mal zu wiederholen. Ohne
    `IC`-Feld (jede bisherige Datei) bleibt `o_code = false`, beide Funktionen nehmen exakt
    den unveränderten, bisherigen Plain-Pfad.
  - Patch (baut auf Phase 1 auf, unabhängig von Phase 2 — disjunkte Funktionen/Structs,
    `git apply --check` in beiden Reihenfolgen P1→P3 und P1→P2→P3 gegen eine frische,
    unabhängige Kopie von `Next-Flip/Momentum-Firmware` Tag `mntm-012` verifiziert, real
    angewendet und byte-für-byte abgeglichen) in
    `firmware-mining/nice-o-code-encoder-port/nice_flor_s_c.patch`.
  - Host-Repro (`alloc()`/`deserialize()`/`get_upload()` neue Zeilen 1:1 als eigene
    Funktionen transkribiert; vier Prüfschritte: Rückwärtskompatibilitäts-Regression gegen
    eine unabhängige Transkription des unveränderten Encoders, O-Code Send→Receive über
    alle 16 erzeugten Pakete einzeln gegen den bereits in Phase 2 verifizierten Decoder-Pfad
    dekodiert plus Falsch-IC-Probe, `IC`-Feld-Abwesenheitssymmetrie, ein durchgerechnetes
    Beispiel) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined`
    verifiziert: **13 467 921 Prüfungen, ALLE CHECKS BESTANDEN**, keine Compiler-Warnungen,
    keine ASan-/UBSan-Meldung — in `firmware-mining/repro-nice-o-code-encoder/repro.c`.
  - Damit sind von der ursprünglichen Nice-O-Code-Port-Skizze nur noch `feed()`
    (Live-Erkennung, weiterhin der riskanteste Teil) und die Installateurcode-Recovery-App
    offen — Laden, Anzeigen und Senden einer bereits mit bekanntem Installateurcode
    versehenen Datei sind jetzt vollständig verdrahtet.
  - Dokumentation: `firmware-mining/nice-o-code-encoder-port.md`.

## Firmware-Feature-Mining (Fortsetzung 20)
- [x] **Nice O-Code Phase 3 (Encoder-Port)**: den in der Vorsitzung unter „Offen" vermerkten
      Rückstellpunkt „Nice One (72 Bit) + O-Code kombiniert" aufgegriffen — bislang nur
      strukturell begründet („sollte funktionieren"), nicht durch ein eigenes Host-Repro-
      Szenario abgedeckt. Dafür den tatsächlichen, unveränderten Nice-One-72-Bit-Anhang aus
      `lib/subghz/protocols/nice_flor_s.c` (`Next-Flip/Momentum-Firmware`, Tag `mntm-012`,
      frisch per `raw.githubusercontent.com` bezogen) im Detail nachvollzogen:
      `subghz_protocol_nice_one_get_data()`/`_crc()` sowie `subghz_protocol_blocks_
      get_bit_array()` (`lib/subghz/blocks/encoder.c`) — alle drei von diesem Patch nicht
      berührt. Bestätigt: `get_upload()`s `NICE_ONE_COUNT_BIT`-Verzweigung berechnet den
      20-Bit-CRC-Anhang aus einer *Kopie* des bereits gebildeten 52-Bit-Rahmens (Header-Byte +
      Chiffretext), nie aus dem Rahmen selbst — die Kombination mit der in Phase 3 pro Paket
      neu verschlüsselten O-Code-Variante dieses Rahmens ist daher strukturell unproblematisch,
      jetzt mit echtem Host-Repro statt nur Einschätzung bestätigt.
  - `firmware-mining/repro-nice-o-code-encoder/repro.c` um ein fünftes Prüfszenario erweitert
      (50 000 O-Code- plus 2 000 Plain-Iterationen): je Paket (a) der 52-Bit-Rahmen bleibt über
      den bereits verifizierten O-Code-Decoder-Pfad unverändert korrekt dekodierbar, unabhängig
      von einem zusätzlich berechneten Nice-One-Anhang; (b) der Anhang ist deterministisch;
      (c) ein unabhängig aus (Seriennummer, Zähler, Taste, IC, Paketindex) über die Masken-/
      `encrypt_ic()`-Formel neu zusammengesetzter Rahmen liefert denselben Anhang wie der von
      `get_upload()` erzeugte; (d) der Anhang variiert über die 16 Pakete statt auf einen Wert
      einzufrieren. Dieselben vier Prüfungen (ohne (a), da hier kein IC existiert) zusätzlich
      für den unveränderten Plain-Pfad (Nice One ohne O-Code) wiederholt, um zu bestätigen,
      dass die bereits bestehende Kombination durch diesen Patch unangetastet bleibt.
  - Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert:
      **16 783 921 Prüfungen (zuvor 13 467 921), ALLE CHECKS BESTANDEN**, keine
      Compiler-Warnungen, keine ASan-/UBSan-Meldung.
  - `firmware-mining/nice-o-code-encoder-port.md` aktualisiert: „Nice One (72 Bit) + O-Code
      kombiniert" aus „Offen" in die „Verifikation" verschoben (Schritt 5), „Warum nicht mehr"
      und „Quellen" entsprechend ergänzt. Damit bleiben für die ursprüngliche Nice-O-Code-
      Port-Skizze nur noch `feed()` (Live-Erkennung) und die Installateurcode-Recovery-App
      offen — unverändert gegenüber der Vorsitzung.

## App-Verbesserungen (Fortsetzung 11)
- [x] **lora-ukfe**: den in `trigger-catalog-number-input-port.md` und
      `payload-run-catalog-number-input-port.md` jeweils identisch unter „Bewusst nicht
      mitgeliefert"/„Offen" vermerkten Rückstellpunkt aufgegriffen —
      `scenes/scene_payload_list.c`s `payload_callback()` rief beim Anklicken eines Eintrags
      aus der vom Heltec gemeldeten Payload-Liste (JSON-Kommando `payload_list`, eigener
      „Payloads"-Menüpunkt, unabhängig vom Katalog-Pfad) bislang `ukfe_uart_send_trigger(app,
      id, 0)` mit fest verdrahtetem `delay_ms=0` auf — eine Payload auszuwählen löste sie immer
      sofort aus, ohne Möglichkeit zur Delay-Eingabe. Gefixt: `payload_callback()` merkt sich
      jetzt nur noch die angeklickte Payload-ID, schaltet auf die bereits vom
      `trigger_catalog_number_input.patch` eingebundene `NumberInput`-View um und fragt „Delay
      ms (0-300000)" ab (dieselbe `TRIGGER_DELAY_MS_MAX`-Konvention/derselbe Wert wie in
      `scene_trigger_confirm.c`, als eigenes `#define` wiederholt, da C-Makros nicht über
      Übersetzungseinheiten geteilt werden); erst der Ergebnis-Callback sendet
      `ukfe_uart_send_trigger(app, id, delay_ms)` und geht danach wie bisher über
      `scene_manager_previous_scene()` zurück ins Menü. Anders als bei den beiden
      Katalog-Funden keine neue Scene (kein reservierter, ungenutzter Scene-Slot vorhanden) —
      stattdessen ein View-Wechsel innerhalb der bestehenden Scene; `build_list()`, die
      `0x12`-Refresh- und `0x11`-Ack-Behandlung sowie `on_enter`/`on_exit` bleiben
      unverändert. Back während der Delay-Eingabe verlässt bewusst die gesamte Scene (bereits
      bestehendes, unkonditionales `on_event()`-Verhalten, unverändert übernommen statt eines
      zusätzlichen Zurück-zur-Liste-Zwischenschritts) — dieselbe „einfachste Lösung für den
      Einschritt-Fall" wie bei `scene_payload_run_confirm.c`.
  - Patch (setzt auf `trigger_catalog_number_input.patch` auf; beide zusammen `git apply
      --check` gegen eine frische, unabhängige Kopie der unveränderten
      `G4MEOVER18/lora-ukfe`-Basis, Commit `0dba410a`, verifiziert, real angewendet und
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch) in
      `payload-list-delay-input-port/payload_list_delay_input.patch`, einzige geänderte Datei
      `scenes/scene_payload_list.c`.
  - Host-Repro (kompiliert die echte, unveränderte `scenes/scene_payload_list.c` dieser Sitzung
      direkt mit, nur `lora_ukfe.h`/`scenes.h` als schlanke Mock-Header nach demselben Muster
      wie in den beiden vorherigen lora-ukfe-Repros; `ukfe_uart_send_trigger()` als
      aufzeichnender Stub wie zuvor `ukfe_uart_send_rf()`) in
      `repro-payload-list-delay-input/repro.c`: acht Szenarien — Normalablauf (Auswahl mit
      `id`≠Index, Delay-Eingabe, korrekter `ukfe_uart_send_trigger(id,delay_ms)`-Aufruf erst
      nach Bestätigung, danach `previous_scene()`), `delay_ms=0`-Randfall (wird weiterhin
      gesendet), Obergrenze `delay_ms=300000`, `index >= payload_count` (Callback kehrt
      unverändert sofort zurück), `0x12`-Refresh/`0x11`-Ack bleiben auch während einer
      laufenden Delay-Eingabe unverändert funktionsfähig, Back während der Delay-Eingabe
      verlässt die Scene ohne zu senden, `on_exit()` räumt auch mit aktiver
      NumberInput-View auf, Randfall-Rundlauf über sechs ID/Delay-Kombinationen. Unter `gcc
      -std=c11 -Wall -Wextra -fsanitize=address,undefined` bei `-O0`/`-O1` verifiziert: 8/8
      Szenarien bestanden, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen.
  - Dokumentiert in `app-improvements/lora-ukfe/payload-list-delay-input-port.md`. Offen
      bleibt bewusst: Back während der Delay-Eingabe verlässt die ganze Liste statt nur zur
      Liste zurückzugehen (reiner Bedienungskomfort-Punkt); ein `0x12`-Refresh, der die gerade
      ausgewählte ID beim Hub neu vergibt, wird von der laufenden Delay-Eingabe nicht erkannt
      (dieselbe Grenze, die auch das bisherige Sofort-Senden schon hatte, keine
      Verschlechterung).

## Firmware-Feature-Mining (Fortsetzung 21)
- [x] Den seit Beginn offenen Backlog-Punkt "MIFARE Plus / Ultralight AES ... Quellsuche noch
      offen" aufgegriffen und die Quellsuche abgeschlossen. Gegen meinen eigenen `mntm-012`-Baum
      verifiziert (Grep über `lib/nfc/protocols/mf_plus/` und `.../mf_ultralight/`): `MfPlusKey`
      und `MfUltralightAesKey` existieren bei mir tatsächlich nirgends — `MfPlusData` kennt nur
      Version/Typ/Größe/Security-Level (reine Erkennung über GET_VERSION + ISO14443-4A), der
      Poller hat nur sechs Zustände ohne jede Auth-/Sektor-Logik; `MfUltralightC3DesAuthKey`
      (Ultralight-C) und die Passwort-Auth (EV1/NTAG) sind zwar bereits vorhanden, aber kein
      AES-Key-Typ. Unleashed (`dev`, aktueller Stand) hat beide Protokolle um eine vollständige
      AES-128-Drei-Pass-Authentifizierung samt Secure-Messaging erweitert: `mf_plus` wächst von
      1125 auf 4084 Zeilen (+2959, u. a. neue `mf_plus_crypto.c/.h` und
      `mf_plus_listener{,_i}.c/.h`), `mf_ultralight` von 4652 auf 10019 Zeilen (+5367, u. a. neue
      `mf_ultralight_aes_crypto.c/.h`). Eigener Fund beim Lesen von Unleasheds
      `mf_plus_crypto.h`-Datei-Header: der Schreibpfad (Write-IV) ist dort selbst als noch nicht
      hardwarevalidiert markiert ("not yet hardware-validated ... confirm with a real-card write
      trace before the write / emulation paths (PR6/7)") — selbst die Quelle behandelt dies als
      mehrteiliges, stufenweise validiertes Vorhaben, kein abgeschlossener Einzelcommit.
  - Scoping-Dokument mit vollständiger Dateiliste (betroffene Dateien beider Protokolle einzeln
      benannt, mit Zeilenzahlen aus der Unleashed-Quelle), Risikobewertung (Umfang, Krypto-
      Korrektheit in beide Richtungen, unvalidierter Schreibpfad bei der Quelle selbst, bewusste
      Trennung der beiden Krypto-Module) und vierphasiger Port-Skizze (Phase 1: host-verifizierter
      Krypto-Kern gegen öffentliche RFC-4493-/FIPS-197-Testvektoren, ohne Poller-Integration;
      Phase 2: MIFARE-Plus-SL3-Lese-Authentifizierung ohne Schreiben/Emulation; Phase 3: analog
      für Ultralight-AES; Phase 4: Anbindung an den bereits portierten gemeinsamen
      Schlüssel-Dictionary-Manager) in `firmware-mining/mifare-plus-ultralight-aes-auth-port.md`.
  - Damit ist der Backlog-Punkt von "Quellsuche offen" zu "Portierungsvorhaben konkret geplant,
      Phase 1 von 4 als nächster kleiner Schritt einer künftigen Sitzung" fortgeschritten — kein
      Patch in dieser Sitzung, da der Gesamtumfang (>8000 Zeilen Diff kombiniert) deutlich über
      jedem bisher hier portierten Einzelfund liegt und eine seriöse Krypto-Verifikation
      (Testvektoren statt nur Selbstvergleich) eigenen Host-Repro-Aufwand in einer eigenen
      Sitzung verdient.

## Firmware-Feature-Mining (Fortsetzung 22)
- [x] Vor der eigentlichen Arbeit wie in `WORKFLOW.md` vorgeschrieben `reports/` durchgesehen, um
      den in Fortsetzung 21 als nächsten Schritt vermerkten Punkt (Phase 1 der MIFARE-Plus-/
      Ultralight-AES-Port-Skizze: `mf_plus_crypto.c/.h`/`mf_ultralight_aes_crypto.c/.h`
      übernehmen, gegen RFC-4493/FIPS-197 verifizieren) aufzugreifen. Dabei `firmware-mining/`
      nach bereits existierenden MIFARE-Plus-/Ultralight-AES-Dateien durchsucht (`grep -rl`
      über typische Namensmuster) und festgestellt: `firmware-mining/repro-mfplus-
      mfultralight-aes-crypto/`, `firmware-mining/mfplus-phase2-sl3-struct-serialization/`,
      `firmware-mining/mfultralight-aes-phase2-struct-serialization/` und `firmware-mining/
      mfplus-sl0-sl3-detection-port/` existierten bereits, mit zugehörigen `.md`-Dokumenten und
      Backlog-Einträgen aus `reports/2026-09-12-1230.md`, `reports/2026-09-15-0032.md`,
      `reports/2026-09-15-0637.md`, `reports/2026-09-15-1229.md`, `reports/2026-09-18-1233.md` —
      allesamt älter als der in Fortsetzung 21 behauptete Stand "seit Beginn offen".
- [x] Nachvollzogen, was dort bereits geliefert wurde (gegen die jeweiligen `.md`-Dokumente und
      Host-Repros gelesen, nicht nur die Dateinamen): Phase 1 (`mf_plus_crypto.c/.h`/
      `mf_ultralight_aes_crypto.c/.h`, 1:1 aus Unleashed übernommen) war bereits mit acht
      Host-Prüfungen verifiziert — FIPS-197-AES-Referenzvektor, alle vier RFC-4493-CMAC-
      Testvektoren, 4000-Fälle-Differenzfuzz gegen OpenSSL `EVP_MAC` "CMAC", Truncated-MAC
      gegen dieselbe Referenz, AES-CBC-Rundlauf plus Block-für-Block-Handrechnung, Rotations-
      Helfer, Read-/Write-IV-Byte-Layout, Session-Schlüssel-Determinismus — strenger als der in
      Fortsetzung 21 für Phase 1 vorgesehene Umfang (dort nur RFC-4493/FIPS-197 ohne
      Differenzfuzz geplant). Phase 2 war bereits für **beide** Kartenfamilien geliefert (nicht
      nur MIFARE Plus): `MfPlusData`-Struct-Erweiterung samt Serialisierung und
      Rückwärtskompatibilität zu Alt-Dumps, sowie analog `MfUltralightData` um die optionale
      AES-Originalitätssignatur und einen neuen Kartentyp-Eintrag, je mit eigenem Host-Repro
      (`ALL CHECKS PASSED`, kein LeakSanitizer-Fund). Zusätzlich war der host-testbare Teilschritt
      von Phase 3 (SL0-vs-SL3-SAK-/ATQA-Auswertung, zwei eigenständige Korrektheitsfehler in der
      bisherigen SAK-Zuordnung behoben) bereits als eigener Patch geliefert.
- [x] Damit war der in Fortsetzung 21 geschriebene Satz "seit Beginn offen" sachlich falsch, und
      die dort neu angelegte Scoping-Datei `firmware-mining/mifare-plus-ultralight-aes-auth-
      port.md` eine unbemerkte Dopplung der bereits existierenden `firmware-mining/mfplus-
      mfultralight-aes-auth-port.md` — beide dokumentieren denselben Fund anhand praktisch
      identischer Unleashed-Commits (`945234a` vom 2026-09-10 bzw. `197ed204` vom 2026-10-05).
      Gegengeprüft, ob der neuere Commit tatsächlich zusätzlichen Umfang brachte (sonst wäre die
      Fortsetzung-21-Analyse trotz der Dopplung sachlich neuwertig gewesen): `mf_plus_crypto.c`
      byte-für-byte zwischen beiden Commits identisch (`diff` ohne Ausgabe), `mf_ultralight_
      listener.c/_i.c/.h/_i.h` in Zeilenzahl zwischen beiden Commits ebenfalls unverändert (1074/
      657/28/155 Zeilen an beiden Commits) — kein inhaltlicher Fortschritt bei Unleashed zwischen
      den beiden Pin-Daten für dieses Feature, die Fortsetzung-21-Sitzung hat also tatsächlich nur
      bereits vorhandene eigene Arbeit unwissentlich doppelt erfasst, keine neue Quellenlage
      entdeckt.
- [x] Korrektur statt Weiterarbeit an einer falschen Prämisse geliefert (kein Firmware-Patch in
      dieser Sitzung, da sonst eine dritte, überflüssige Kopie der bereits zweimal vorhandenen
      Phase-1-Arbeit entstanden wäre): `firmware-mining/mifare-plus-ultralight-aes-auth-port.md`
      um einen Korrekturhinweis am Dateianfang ergänzt (nicht gelöscht, siehe `CONSTRAINTS.md`
      Punkt 6 — nur ergänzt), der auf die bereits existierenden September-Dokumente verweist und
      den tatsächlichen Stand richtigstellt, sowie den oben stehenden Backlog-Eintrag korrigiert.
      Damit verweisen jetzt beide Scoping-Dokumente konsistent auf denselben Fortschritt, und eine
      künftige Sitzung liest beim nächsten Aufgreifen dieses Punkts den korrekten Stand (Phase 3
      Rest/4/5/6, hardwaregebunden) statt erneut bei Phase 1 anzusetzen.
- [x] Keine weiteren offenen Backlog-Punkte mit demselben Dopplungsrisiko gefunden: Grep über
      `BACKLOG.md` nach allen `- [ ]`/`- [~]`-Einträgen und Gegenprüfung ihrer referenzierten
      Dateinamen gegen `firmware-mining/`/`app-improvements/`/`ideas/` ergab keinen zweiten Fall,
      in dem ein als offen markierter Punkt bereits anderswo unter abweichendem Dateinamen
      vollständig bearbeitet vorliegt.

## Firmware-Feature-Mining (Fortsetzung 23)
- [x] Den letzten im NFC-Werkzeug-Cluster verbliebenen Kandidaten aufgegriffen: **NFC Relay**
      (`leommxj/nfc_relay`, über das RogueMaster-Bündel) war bisher nur als "Fremd-/Zweitgerät-
      Fall (zwei eigene Flipper-Geräte, UART-Direktverbindung), weiterhin zurückgestellt"
      eingeordnet (siehe `ideas/pentest-app-katalog.md`, Chameleon-Fortsetzung), nie
      quelltextlich geprüft. Analog zum bereits dokumentierten `chameleon`-Fund gilt: auch ein
      Werkzeug, das zwingend zwei eigene Geräte braucht, hat einen eigenen Paket-Parser für die
      Relaisverbindung, der unabhängig vom Zweitgerät-Status geprüft werden kann — App
      vollständig gelesen (15 Dateien, 1708 Zeilen: `nfc_relay.c/.h`, `comm.c/.h`,
      `packet.c/.h`, `uart_worker.c/.h`, `as_reader_worker.c/.h`, `as_card_worker.c/.h`, fünf
      Szenen-Dateien).
  - Fund in `as_reader_worker.c::as_reader_worker_task()` (Zustand
    `AsReaderWorkerStateInteractive`): `comm_wait_packet()` prüft beim Entgegennehmen eines
    `NfcRelayPacketApduReq`-Pakets nur den Pakettyp, nicht dessen Länge; `uart_worker_task()`
    erzeugt für `len == 0` einen vollkommen gültigen Null-Byte-Payload-Rahmen. Die bestehende
    Behandlung wertet aber ungeprüft `recv_packet->len - 1` aus ("drop PCB") — bei `len == 0`
    promotet die `uint8_t`-Subtraktion nach `int` (`0 - 1 = -1`), und dieser Wert erreicht als
    riesiger `size_t`-Wert (`0xFFFFFFFF` auf der 32-Bit-Zielarchitektur) den
    `size_bytes`-Parameter von `bit_buffer_copy_bytes()`. Dessen bereits vorhandene
    `furi_check(buf->capacity_bytes >= size_bytes)`-Prüfung schlägt fehl (Zielpuffer nur 256
    Byte) und bricht den Prozess kontrolliert ab (`furi_crash()` am echten Gerät) — ein
    sauberer, aber über die Relaisverbindung aus der Ferne auslösbarer Denial-of-Service
    (Geräteabsturz des "AsReader"-Geräts durch ein einzelnes, drei Byte kurzes Paket), keine
    Speicherkorruption, da der Abbruch vor dem eigentlichen `memcpy()` greift. Laut eigenem
    `README.md` brückt der Autor die Relaisverbindung selbst wahlweise über zwei
    ESP-AT-WLAN-Module durch einen Cloud-Relay-Server aus dem Internet statt nur per direktem
    UART-Kabel — die Angriffsfläche ist damit potenziell größer als "zwei Geräte nebeneinander
    auf dem Tisch".
  - Die naheliegende Geschwisterstelle in `as_card_worker.c`
    (`as_card_worker->apdu_buf_len = recv_packet->len + 1`, reine Addition, passt auch bei
    `len = 255` unverändert in das 260 Byte große `apdu_buf`) gegengeprüft: nicht betroffen.
    Einen zunächst vermuteten zweiten Fund — ein Ganzzahlüberlauf-Bypass der
    Längen-Konsistenzprüfung im `NfcRelayPacketNfcDevData`-Pfad derselben Datei
    (`recv_packet->len == ((SerializedIso14443_4a*)recv_packet->buf)->len_t1_tk +
    sizeof(SerializedIso14443_4a)`) — beim rigorosen Durchrechnen der zugrundeliegenden
    Kongruenzgleichung wieder verworfen: die vorab verlangte Bedingung
    `recv_packet->len > sizeof(SerializedIso14443_4a)` erzwingt bereits, dass die einzige
    Lösung für `len_t1_tk` in der vollen 32-Bit-Wertemenge klein und legitim ist — kein
    zweiter, böswillig großer Lösungswert möglich. Diese Stelle ist entgegen der ersten
    Vermutung tatsächlich bereits korrekt; nicht mitgefixt, da kein echter Fund.
  - Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
    `RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet und
    byte-für-byte abgeglichen) in
    `firmware-mining/nfc-relay-apdu-req-zero-length-underflow-fix/as_reader_worker_c.patch`.
  - Host-Repro (Paketempfang, `bit_buffer_copy_bytes()`s Kapazitätsprüfung und der
    ApduReq-Behandlungsausschnitt 1:1 transkribiert, `size_t` explizit als `uint32_t`
    nachgebildet, um die 32-Bit-Zielarchitektur statt eines ggf. 64-Bit-Hosts abzubilden; sechs
    Prüfschritte: Fund bei `len=0` in einem `fork()`-Kindprozess, Fix-Nachweis bei `len=0`,
    Randfall `len=1`, Sanity-Fälle `len=5`/`len=255`, Äquivalenztest über `len=1..255`) unter
    `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALLE
    CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
    `firmware-mining/repro-nfc-relay-apdu-req-zero-length-underflow/repro.c`. Details inkl.
    Testhinweis in `firmware-mining/nfc-relay-apdu-req-zero-length-underflow-fix.md`,
    `ideas/pentest-app-katalog.md` aktualisiert. Damit ist der komplette NFC-Werkzeug-Cluster
    (`nfctools`, `uhf_rfid`, `chameleon`, `nfc_relay`) jetzt vollständig quelltextlich
    durchgesehen — kein weiterer Kandidat in diesem Cluster offen.

## Firmware-Feature-Mining (Fortsetzung 24)
- [x] Den in `ideas/pentest-app-katalog.md` als "Restlicher Umfang (~35 Schreib-Szenen,
      Passwort-Entfernungs-Input, Sektor-Analyse bereits geprüft) offen" vermerkten Rest des
      `nfctools`-Clusters (`wakdev`, über RogueMaster) aufgegriffen: Passwort-Entfernung,
      Sektor-Analyse, "NFC Commands", Format/Lock Tag waren bereits aus früheren Sitzungen
      dokumentiert — wirklich offen waren noch alle ~29 einfachen Schreib-Eingabe-Szenen
      (`scenes/write/nfc_tools_scene_write_*.c`) und der komplette gemeinsame Schreibpfad
      `nfc_tools_ndef_build()` (`core/nfc_tools_ndef.c`, alle 17 `NdefType`-Fälle). Beides
      vollständig gelesen.
  - Die Eingabe-Szenen selbst sind durchgängig dasselbe kurze Keyboard-Wrapper-Muster
    (Eingabe in einen `app->ndef_bufN`-Puffer mit `sizeof(...)`-Begrenzung, teils zusätzlich per
    `keyboard_set_max_length()` enger begrenzt — SSID 32, WiFi-Passwort 64, passend zu den
    WSC-Klammerungen in `nfc_tools_ndef_build()`) und bieten keine weitere Angriffsfläche.
    `write_bufs_reset()` (`scenes/nfc_tools_scene_write_menu.c`) setzt beim Wählen eines neuen
    NDEF-Typs alle sechs Puffer zurück — keine Altdaten-Vermischung zwischen verschiedenen
    Schreibflüssen, die sich einen Puffer teilen (z. B. WiFi-Passwort/Mail-Betreff/Firma alle in
    `ndef_buf2`). `nfc_tools_ndef_build()` dimensioniert jede `payload`-Allokation exakt auf die
    vorher gemessene Länge; der einzige bereits dokumentierte Fund in dieser Funktion
    (`url_encode()`-Off-by-eins) bleibt der einzige Puffer-/Längenfeld-Fund.
  - Ein neuer Fund: `case NdefTypeContact:` baut die vCard-3.0-Datei aus sechs frei eingebbaren
    Textfeldern (Name/Firma/Adresse/Telefon/E-Mail/URL) mit rohem `%s` zusammen, ohne die nach
    RFC 6350 §3.4 vorgeschriebene Escape-Behandlung von `\`, `;`, `,` und eingebetteten
    Zeilenumbrüchen. `N:` und `ADR:` sind selbst semikolongetrennte Teilfelder — ein Name wie
    "Doe;Jane" oder eine Firma wie "Acme; Inc." verschiebt auf jedem vCard-lesenden Smartphone
    die nachfolgenden Teilfelder (Nachname/Zusatzname bzw. Ort/Region/Postleitzahl/Land) und
    verstümmelt die Kontaktkarte sichtbar beim eigentlichen Verwendungszweck (Antippen durch ein
    drittes Gerät) — kein Speicherfehler auf dem Flipper selbst, ein Korrektheitsfehler in der
    erzeugten Datei.
  - Fix: neue additive Hilfsfunktion `vcard_escape()` (nach demselben Muster wie das bestehende
    `url_encode()` direkt darüber in derselben Datei), aufgerufen an allen sechs Stellen innerhalb
    des einen `case NdefTypeContact:`-Blocks; keine andere `NdefType`-Verzweigung, keine
    Szenen-Datei, keine Puffergröße ändert sich. Ohne Sonderzeichen bleibt die erzeugte Datei
    byte-identisch zum bisherigen Verhalten.
  - Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
    `RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet und
    byte-für-byte abgeglichen) in
    `firmware-mining/nfctools-vcard-unescaped-delimiter-fix/nfc_tools_ndef_c.patch`.
  - Host-Repro (`vcard_escape()` 1:1 aus dem Patch transkribiert, ALT-/NEU-Fassung des
    `case NdefTypeContact:`-Blocks mit einem einfachen wachsenden Host-Puffer statt
    `FuriString`, plus ein RFC-6350-konformer, zeilenanker-sicherer Teilfeld-Splitter zur
    Round-Trip-Prüfung; neun Szenarien: Regression ohne Sonderzeichen, Name mit `;`
    [ALT zerlegt `N:` in 6 statt 5 Teilfelder, Name verstümmelt — NEU korrekt], Name mit `,`/`\`,
    Adresse mit `;` [ALT 8 statt 7 `ADR:`-Teilfelder — NEU korrekt], Firma mit `;`, leere
    optionale Felder weiterhin ausgelassen, "+"/"https://"-Sentinelwerte weiterhin unterdrückt,
    Telefon/E-Mail/URL ebenfalls escaped, Randfall leerer Name) unter
    `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALLE CHECKS
    BESTANDEN, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
    `firmware-mining/repro-nfctools-vcard-unescaped-delimiter/repro.c`. Details inkl. Testhinweis
    in `firmware-mining/nfctools-vcard-unescaped-delimiter-fix.md`,
    `ideas/pentest-app-katalog.md` aktualisiert (Schreibpfad jetzt vollständig durchgesehen,
    offen bleiben die fünf Lese-Szenen, Haupt-/Sonstiges-Menü, Tastatur-View sowie die
    MD5-/QR-Code-Hilfsmodule — noch nicht einzeln geprüft).

## Firmware-Feature-Mining (Fortsetzung 25)
- [x] Den laut `ideas/pentest-app-katalog.md` zuletzt offenen Rest des NFC-Tools-Clusters
      (`wakdev`, über RogueMaster) aufgegriffen: die fünf Lese-Szenen (`scenes/read/*.c`),
      Haupt-/Sonstiges-Menü (`nfc_tools_scene_main_menu.c`/`_other_menu.c`/`_about.c`), die
      Tastatur-View (`views/keyboard/nfc_tools_keyboard.c`) sowie die MD5-/QR-Code-
      Hilfsmodule (`helpers/md5/`, `helpers/qrcode/`) vollständig gelesen. Haupt-/Sonstiges-
      Menü reines Boilerplate ohne Fund. Tastatur-View (589 Zeilen: Layout-Tabellen, Case-
      Handling, Scroll-/Cursor-Zeichnung, `kb_insert()`/`kb_backspace()`-Grenzen) und das
      MD5-Modul (Standard-RFC-1321, Padding-/Bit-Längen-Berechnung in Little-Endian 64-Bit)
      gegen ihre Aufrufer geprüft — beide korrekt, kein Fund. Die fünf Lese-Szenen (reine
      Anzeige-/Navigations-Wrapper um bereits vorhandene Felder) ebenfalls ohne Fund.
- [x] Fund im QR-Code-Hilfsmodul (`helpers/qrcode/nfc_tools_qrcodegen.c`, ein eigenständiger
      minimaler QR-Encoder für Byte-Modus/ECC-Level-L): `nfc_tools_qr_encode()` behandelt
      jede Version 1–7 als einen einzigen Reed-Solomon-Block. Das ist für Version 1–5
      korrekt, aber der echte QR-Standard (ISO/IEC 18004:2015 Tabelle 9) teilt die Daten ab
      Version 6 tatsächlich in zwei verschachtelte RS-Blöcke (v6 = 2×(86,68), v7 = 2×(98,78)
      Codewörter — unabhängig gegen die Python-Referenzbibliothek `segno`
      (`segno.consts.ECC[6][1]`/`[7][1]`) gegengeprüft, nicht nur gegen eigene Berechnung).
      Für Version 6/7 berechnet die Funktion sowohl die ECC-Codewörter falsch als auch die
      Platzierungsreihenfolge — das Ergebnis sieht wie eine fertige QR-Grafik aus
      (`encode()` gibt weiterhin `true` zurück), ist aber strukturell ungültig.
  - Empirisch bestätigt statt nur aus dem Standardtext hergeleitet: die unveränderte Datei
    in ein Host-Programm kompiliert, das für jede Textlänge 1–174 Zeichen eine QR-Grafik
    rendert und mit `zbarimg` (ZBar, unabhängiger Open-Source-Scanner) zurückliest — jede
    Länge bis 106 Zeichen (Version 1–5) liest korrekt zurück, jede Länge ab 107 Zeichen
    (Version 6/7) scheitert reproduzierbar ("scanned 0 barcode symbols"). Der
    NDEF-Record-Wert, aus dem der QR-Inhalt stammt, kann bis zu 127 Zeichen lang sein — die
    106-Zeichen-Grenze wird damit bei alltäglichen Werten (längere URLs, Freitext)
    regelmäßig überschritten.
  - Fix: Versionsschleife in `nfc_tools_qr_encode()` von `<= 7` auf `<= 5` begrenzt (plus
    korrigierter Kopfkommentar). Für Version 1–5 byte-identisches Verhalten; für Werte, die
    vorher Version 6/7 ergeben hätten, zeigt die App jetzt ehrlich "QR unavailable" (bereits
    vorhandener, genutzter Code-Pfad in `nfc_tools_scene_ndef_qr_code.c`) statt einer
    kaputten Grafik. Eine vollständige Mehrblock-RS-Implementierung (würde Version 6/7
    wieder nutzbar machen, bis ~166 Zeichen) ist ein eigenständiger, größerer Port-Schritt
    und bewusst nicht Teil dieses Beitrags.
  - Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
    `RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet
    und byte-für-byte abgeglichen) in
    `firmware-mining/nfctools-qrcode-multiblock-overflow-fix/nfc_tools_qrcodegen_c.patch`.
  - Host-Repro in `firmware-mining/repro-nfctools-qrcode-multiblock-overflow/repro.c`: bindet
    die unveränderte und die gefixte Datei als zwei vollständige, umbenannte
    Übersetzungseinheiten in einen Prozess ein (keine Transkription) und ruft für den
    eigentlichen Scan-Nachweis zusätzlich `zbarimg` per `popen()` auf echten PGM-Renderings
    auf. Vier Szenarien (Regression Version 1–5 byte-identisch, Fund Version 6/7 bei fünf
    Stichproben per `zbarimg` bestätigt unscannbar, Grenzfall 106/107, Randfall leerer Text)
    unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert:
    7/7 Szenarien bestanden, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen. Details
    inkl. Testhinweis in `firmware-mining/nfctools-qrcode-multiblock-overflow-fix.md`,
    `ideas/pentest-app-katalog.md` aktualisiert. Damit ist der NFC-Tools-Cluster (`wakdev`,
    RogueMaster-Bündel) jetzt vollständig quelltextlich durchgesehen — kein weiterer
    Teilumfang in diesem Cluster offen.

## App-Verbesserung (RollJam — Fortsetzung)
- [x] Nach Abschluss des NFC-Werkzeug-Clusters (siehe Firmware-Feature-Mining
      Fortsetzung 23-25) diese Sitzung auf eine App-Verbesserung statt weiteres
      Firmware-Mining gelegt. `reports/` durchgesehen (letzter Eintrag
      `2026-10-06-0035.md`), keine offene App-Verbesserung als nächsten Schritt
      vermerkt — eigenständig `app-improvements/RollJam/` gegen den aktuellen
      Stand von `G4MEOVER18/RollJam` (Commit `1f90c00e1`, unverändert zur
      letzten Prüfung) erneut durchgesehen.
- [x] Zwei zusammenhängende Funde in der `app->abort`-Buchhaltung gemacht: (1)
      die Right-Taste ("Abort") während `PHASE1_JAMMING`/`PHASE2_JAMMING`/
      `PHASE3_REPLAY` setzt dasselbe Flag wie die Zurück-Taste — die
      Hauptschleife `while(!app->abort)` kennt aber nur "App beenden", kein
      "zurück ins Menü"; abweichend vom eigenen Bildschirm-Versprechen
      ("Abort" statt "Exit") und abweichend vom korrekt implementierten
      Abort-zu-Menü-Verhalten der Schwester-App `RollLab`. (2) derselbe
      gezeichnete "Abort"-Hinweis im Zustand `CAPTURED` ist wirkungslos, da
      `CAPTURED` in der Right-Zustandsliste fehlt — analog zum bereits
      dokumentierten `exit-button-left-key-unhandled-fix.md`-Fund, nur beim
      "Abort"- statt beim "Exit"-Hinweis.
- [x] Fix: neues `app->quit`-Feld trennt "App wirklich beenden" (Back) von
      "laufende Phase abbrechen" (Right); Hauptschleife löst `app->abort` am
      Schleifenanfang zu `running=false` (quit oder bereits IDLE/DONE) oder
      `ROLLJAM_IDLE` auf; `CAPTURED` in die Right-Zustandsliste ergänzt;
      fehlender "Abort"-Hinweis in `PHASE3_REPLAY` nachgezogen. Patch (`git
      apply --check` gegen frische, unabhängige Kopie verifiziert, real
      angewendet, byte-für-byte abgeglichen) in `app-improvements/RollJam/
      abort-right-key-quits-app-fix/rolljam_app_c.patch`. Host-Repro (reine
      Zustandsmaschine, sechs Szenarien inkl. Back-Regression über alle
      sechs Zustände) unter `gcc -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined` verifiziert: ALLE CHECKS BESTANDEN, keine
      ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `app-improvements/RollJam/repro-rolljam-abort-right-key-quits-app/
      repro.c`. Details, Nutzen, Testhinweis, Risiko in
      `app-improvements/RollJam/abort-right-key-quits-app-fix.md`.
- [x] Dabei bewusst nicht angefasst: `rolljam_rf.c` (RF-Konfiguration/Timing
      unverändert), `rj_run_jam_capture_phase()` selbst (nur ihr
      Rückgabewert-Konsument in der Hauptschleife ändert sich), sowie der
      separat dokumentierte, weiterhin offene `InputKeyLeft`-Fund
      (`exit-button-left-key-unhandled-fix.md`) — dessen Patch sollte bei
      künftiger Umsetzung konsequenterweise ebenfalls `app->quit = true`
      setzen, dort aber nicht mitgeliefert, um diesen Beitrag klein und
      abgeschlossen zu halten.

## App-Verbesserung (RollJam — Fortsetzung 2)
- [x] Den in der letzten Sitzung als nächster Schritt vermerkten Punkt aufgegriffen:
      `exit-button-left-key-unhandled-fix.md` (Left-Taste auf `IDLE`/`DONE` ohne
      Wirkung) an den inzwischen gelieferten `abort-right-key-quits-app-fix`
      angepasst. Der alte Patch war gegen die unveränderte Basis erzeugt und hätte
      sich mit dem neuen `app->quit`-Feld/der neuen Abort-Auflösung nicht mehr
      konfliktfrei zusammenführen lassen (beide Patches ändern dieselbe Stelle in
      `rj_input_cb()`).
- [x] Frische, unabhängige Kopie von `G4MEOVER18/RollJam` (Commit `1f90c00e1`)
      geklont, `abort-right-key-quits-app-fix`-Patch dagegen angewendet (`git apply
      --check` verifiziert) um den tatsächlichen Zwischenstand nachzubilden, und den
      Left-Tasten-Fix dagegen neu erzeugt: `InputKeyLeft` setzt auf `IDLE`/`DONE` jetzt
      sowohl `app->abort` als auch `app->quit` (analog zu `InputKeyBack`, nicht zu
      `InputKeyRight`) — das zusätzliche `quit`-Flag ist beim aktuellen Stand der
      Abort-Auflösung redundant zum `state==IDLE||state==DONE`-Teil der
      ODER-Bedingung, macht die Taste aber unabhängig von dieser Nebenwirkung und
      konsistent mit der tatsächlichen Absicht (wirklich beenden).
- [x] Patch (`git apply --check` gegen die genannte Zwischenbasis verifiziert, real
      angewendet, byte-für-byte mit der Arbeitskopie abgeglichen) in
      `app-improvements/RollJam/exit-button-left-key-unhandled-fix/rolljam_app_c.patch`.
      Host-Repro erweitert (23 Prüfungen: End-Zustand UND `quit`-Flag für Left auf
      `IDLE`/`DONE`, Regression Left in den vier aktiven Phasen, Regression Back/Right
      je Zustand) unter `gcc -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined`
      verifiziert: ALLE CHECKS BESTANDEN (identischer Output beider
      Optimierungsstufen), keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `app-improvements/RollJam/repro-exit-button-left-key-unhandled/repro.c`.
      `exit-button-left-key-unhandled-fix.md` (Reihenfolge-Hinweis, Port-Skizze,
      Testhinweis, Quelle) entsprechend überarbeitet.
- [x] Damit sind beide App-Verbesserungs-Funde dieser `RollJam`-Durchsicht
      (`abort-right-key-quits-app-fix`, `exit-button-left-key-unhandled-fix`) jetzt
      als zusammenhängendes, konfliktfrei anwendbares Patch-Paar dokumentiert
      (Reihenfolge: zuerst Abort-Fix, dann Exit-Fix). Kein weiterer offener
      Teilumfang bei `RollJam` bekannt (siehe bereits durchgesehener Restumfang in
      `abort-right-key-quits-app-fix.md`/`exit-button-left-key-unhandled-fix.md`).

## Ideen-Recherche (Fortsetzung 14)
- [x] `reports/` durchgesehen: letzte beiden Sitzungen schlossen den NFC-Werkzeug-Cluster
      (Firmware-Feature-Mining) sowie beide `RollJam`-Funde (App-Verbesserung) ab — gezielt auf
      Ideen-Recherche gelegt, da diese Kategorie zuletzt am 2026-10-02 bedient wurde.
      `ideas/pentest-app-katalog.md` durchgesehen: RogueMaster-Bündel (~700 Apps) bislang nur
      gezielt nach bereits bekannten Cluster-/Autoren-Rückstellpunkten durchsucht, nie nach noch
      komplett fehlenden Kategorien — Sparse-Checkout von `applications/external/` (Branch
      `420`, 801 Ordner) gegen den eigenen Katalogtext abgeglichen: GPIO/Serial-Bus-Kategorie
      deckt bislang ausschließlich Bus-*Analyse* ab (UART Sniff, GPIO Logic Analyzer, i2c Tools,
      RS485 Modbus), kein Dump-Pfad von einem fremden Chip auf die SD-Karte.
- [x] **SPI Flash Dump** (`PentestSuite & barkandbite/flipper_suite`, über das
      RogueMaster-Bündel) gefunden und vollständig gelesen (1933 Zeilen, 4 C-Dateien) — liest
      SPI-NOR-Flash-Chips (29 JEDEC-IDs: Winbond/Macronix/GigaDevice/Adesto/ISSI/SST/Spansion)
      über bitgebangtes SPI auf dem Erweiterungsport aus, reiner Lesezugriff. API-87-Status
      verifiziert (alle vier verwendeten `gpio_ext_pa4/pa6/pa7/pb3`-Pins sowie
      `crc32_calc_file()` identisch gegen den eigenen `mntm-012`-Tag-Stand).
- [x] Fund: das eigene README verspricht im Hex-Viewer "Navigate with Up/Down (one line) or
      Left/Right (one page)" sowie "Hold OK to jump to a specific offset" — `hex_viewer.c`s
      `hex_viewer_input_cb()` verdrahtet aber ausschließlich `InputKeyUp`/`InputKeyDown`; für
      `InputKeyLeft`/`InputKeyRight`/`InputKeyOk` liefert die Funktion unbedingt `false`. Bei
      einem vollen 4096-Byte-Preview (1024 Zeilen, 6 sichtbar) bräuchte es ohne Seitensprung
      1018 einzelne Tastendrücke bis zum Dateiende.
- [x] Fix: zwei neue `if`-Zweige nach dem bereits vorhandenen Up/Down-Klemm-Muster — Right
      springt eine Seite (`ROWS_ON_SCREEN` = 6 Zeilen) vor, geklemmt auf `max_offset`; Left
      springt eine Seite zurück, geklemmt auf 0. Patch (`git apply --check` gegen eine frische,
      unabhängige Kopie von `RogueMaster/flipperzero-firmware-wPlugins` Branch `420` Commit
      `16cb41b9` verifiziert, real angewendet, byte-für-byte abgeglichen) in
      `firmware-mining/spi-flash-dump-app-port/hex_viewer_c.patch`. Host-Repro (Scroll-
      Arithmetik 1:1 transkribiert; sieben Szenarien: Regression Up/Down, Fund-Nachweis ALT
      reagiert nicht auf Left/Right, Fix-Nachweis Right/Left je mit Klemm-Grenze, Randfall
      Dump passt auf einen Bildschirm, Randfall 1000 Rechts-Drücke in Folge ohne Überlauf,
      Äquivalenz Ok/Back) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: 3150
      Prüfungen, ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine
      Compiler-Warnungen — in
      `firmware-mining/repro-spi-flash-dump-hex-viewer-page-scroll/repro.c`. Dabei einen
      eigenen Fehler im Repro selbst gemacht und per ASan/UBSan sofort gefunden (redundantes
      `assert(cond); if(!(cond))`-Muster wertet eine seiteneffektbehaftete Bedingung doppelt
      aus) — behoben, bevor das Repro als Nachweis gezählt wurde.
- [x] Details, Nutzen, betroffene Datei, Testhinweis, Risiko in
      `firmware-mining/spi-flash-dump-app-port.md`. `ideas/pentest-app-katalog.md` um eine
      Tabellenzeile (Kategorie "GPIO / Serial Bus") sowie einen Log-Eintrag unter "Nächste
      Recherche-Aufträge" ergänzt. Bewusst zurückgestellt: der ebenfalls im README versprochene
      Hold-OK-Adresssprung (bräuchte eine eigene Hex-Eingabe-Ansicht, kein kleiner Fix mehr).

## Kompatibilität (Kontext, lokal umgesetzt)
- CLI-Kompat-Shim (`cli_session_open/close`, `cli_vcp`) für Alt-Apps + cli_bridge — Firmware-seitig, lokal.
- 49 Alt-Apps laufen bereits unter `api_mismatch_autoload` (bewiesen per Symbol-Analyse).

## Firmware-Feature-Mining (Fortsetzung 26)
- [x] Offenen `unlshd-090`-Punkt aus dem Backlog aufgegriffen ("neue SubGHz-Protokolle listen
      → Port-Vorschläge"). Gegen den tatsächlich neuesten Unleashed-Tag geprüft (`unlshd-093`,
      aktueller als `unlshd-090`): `lib/subghz/protocols/` dort vollständig gegen meine
      `mntm-012`-Basis verglichen, sieben fehlende Protokolle gefunden
      (`allstar_firefly`, `beninca_arc`, `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`,
      `treadmill37` — `prastel`/`telcoma_edge` ebenfalls fehlend, aber bereits eigene
      Port-Vorschläge aus früheren Sitzungen, kein neuer Fund).
- [x] **Beninca ARC** ausgewählt (einziges der sieben mit AES-128 statt KeeLoq-Krypto — neue
      Primitive für meinen SubGHz-Stack). Vollständig gelesen (`beninca_arc.c`/`.h` + die zwei
      bei Unleashed neuen Hilfsmodule `aes_common.c`/`.h` und `common.c`/`.h`). Jedes verwendete
      HAL-/Lib-Symbol einzeln gegen `mntm-012` geprüft: `blocks/{decoder,encoder,const,math,
      custom_btn_i}.h` sowie `subghz_keystore.h`/`_i.h` byteidentisch; `blocks/generic.h` im
      gemeinsamen Teil identisch, Unleashed aber mit zusätzlicher
      `subghz_block_generic_global`-Override-Infrastruktur (Counter-/Button-Editor-UI), die bei
      mir komplett fehlt; `common.h` (Unleasheds PR-#1116-Flash-Spar-Refactor, von 60 Dateien
      dort genutzt) existiert bei mir ebenfalls nicht.
- [x] AES-128-Implementierung aus `aes_common.c` Zeile für Zeile gegen FIPS-197 nachgerechnet
      (SBox/ShiftRows/MixColumns/KeySchedule) und auf dem Host gegen den offiziellen
      FIPS-197-Testvektor verifiziert (unabhängige Quelle) — keine Abweichung. Protokollspezifische
      Schlüsselableitung + Encrypt/Decrypt über 50 000 zufällige Rundläufe verifiziert.
- [x] Fund (geringe praktische Relevanz, kein Speicherfehler): Die Erfassung der ersten
      64-Bit-Rahmenhälfte in der Decoder-Zustandsmaschine hängt zusätzlich zum Bitzähler an
      einem `decode_data != 0`-Gate — bei einer (bei echtem AES-Chiffretext praktisch nie
      auftretenden, ca. 1-zu-2⁶⁴) exakt-Null ersten Hälfte bliebe `generic.data` auf dem
      Altwert eines früheren Pakets stehen, während `generic.data_2` trotzdem überschrieben und
      der volle Callback ausgelöst wird. Über 200 000 echte Verschlüsselungen empirisch
      bestätigt: `data == 0` trat kein einziges Mal auf. Fix: explizites `half_captured`-Flag
      statt Dateninhalt-Abhängigkeit, per Host-Repro als verhaltensgleich zum Original
      nachgewiesen (Regressionstest über 20 000 zufällige Rahmen).
- [x] Port: `aes_common.c`/`.h` unverändert übernommen; `beninca_arc.c`/`.h` an meinen
      Protokoll-Stil angepasst (keine `common.h`-Abhängigkeit, eigene `_alloc/_free/_reset/
      _stop/_yield`, direktes `furi_hal_subghz_get_rolling_counter_mult()`-Muster statt
      `_global`-Override, kein Editor-UI-Push in `get_string()`, `half_captured`-Fix); je ein
      zusätzlicher Include/Registry-Eintrag in `protocol_items.c`/`.h`. Sechs Patches
      (`git apply --check` gegen eine frische, unabhängige Zweitkopie von `mntm-012` verifiziert,
      real angewendet, byte-für-byte mit der separat editierten Fassung abgeglichen — identisch)
      in `firmware-mining/beninca-arc-aes-subghz-port/*.patch`. Host-Repro (AES-FIPS-197-Vektor,
      AES-Rundlauf, Protokoll-Encrypt/Decrypt-Rundlauf, Zero-Chiffretext-Häufigkeit, Fund- und
      Regressionsnachweis der Decoder-Zustandsmaschine) unter `gcc -std=c11 -O0`/`-O1 -Wall
      -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert:
      20013/20013 Checks bestanden, identisches Ergebnis bei beiden Optimierungsstufen, keine
      ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `firmware-mining/repro-beninca-arc-aes-subghz-port/repro.c`.
- [x] Details, Kompatibilitätsprüfung, Risiko, Port-Skizze, Testhinweis in
      `firmware-mining/beninca-arc-aes-subghz-port.md`. Bewusst nicht portiert: Unleasheds
      `common.c`/`.h`-Refactor selbst, `subghz_protocol_beninca_arc_create_data()` (optionale
      "Add Manually"-Hilfsfunktion, bei Unleashed selbst nicht an die Vtable angebunden), die
      tatsächliche Keystore-Ressourcendatei (Herstellerschlüssel ist Dateninhalt, nicht Code —
      wie schon beim Superrollo-Normal-Learning-Schlüssel). Als weitere, noch nicht geprüfte
      Kandidaten aus demselben `unlshd-093`-Vergleich offen: `allstar_firefly`, `ditec_gol4`,
      `jarolift`, `keyfinder`, `nord_ice`, `treadmill37` (alle ebenfalls mit der
      `common.h`-Abhängigkeit, `jarolift` zusätzlich mit KeeLoq- statt AES-Krypto).

## App-Verbesserung (usb-army-penetrator — Fortsetzung)
- [x] `reports/` durchgesehen: letzte Sitzung schloss Firmware-Feature-Mining ab (Beninca ARC
      AES-SubGHz-Port), davor Ideen-Recherche (SPI Flash Dump) — App-Verbesserung seit den beiden
      `RollJam`-Sitzungen nicht mehr an der Reihe, für diese Sitzung gewählt. Vorherige
      `usb-army-penetrator`-Sitzung (Fortsetzung 5) hatte den TFT-Hex-Farbparser als "letzten
      offenen Rückstellpunkt" markiert und nur noch Ethernet-CH9120/W5500 (unverdrahteter Stub)
      als bewusst außerhalb reiner Quelltextprüfung liegend offengelassen — gezielt über diese
      bereits bekannte Liste hinaus nach einem bislang nie geprüften Modul gesucht statt denselben
      Rest erneut zu betrachten.
- [x] Frische, unabhängige Kopie von `G4MEOVER18/usb-army-penetrator` (main, Commit `411b153`)
      geklont, vollständige Dateiliste unter `src/` gegen alle bisherigen
      `app-improvements/usb-army-penetrator/*.md`-Funde abgeglichen: `src/Utilities/Settings.cpp`
      (zentrale Setting-Schreib-/Lesefunktion, von Web-UI und DuckyScript aus erreichbar) war in
      keinem bisherigen Fund erwähnt — ausgewählt.
- [x] **Fund**: `setSettingValue()` übergibt für die Typen `Int16`/`UInt16` den client-/
      skript-kontrollierten `value`-String ungeprüft an `std::stoi()` — wirft laut C++-Standard
      `std::invalid_argument`/`std::out_of_range` bei leerem, nicht-numerischem oder zu großem
      Wert. Gesamten `src/`-Baum nach `try`/`catch` durchsucht (`grep -rn "catch\s*(\|try\s*{"
      src/`) — null Treffer irgendwo im Projekt; `platformio.ini` setzt für keine der
      ESP32-/RP2040-Build-Umgebungen `-fno-exceptions`. Eine nicht gefangene Ausnahme läuft damit
      bis `std::terminate()` durch und reißt das gesamte Firmware-Image ab. Aufrufketten
      verifiziert: `src/Comms/Web/WebServer.cpp` (`/set*`-Endpunkt, kein Login/Token) und
      `src/Attacks/Ducky/Extensions.h` (`SETTING`-Befehl) reichen den Wert ungeprüft durch;
      tatsächlich betroffene registrierte Settings (`grep -rn "SettingType::Int16\|
      SettingType::UInt16" src/`): `USB_AGENT_POLLING_TIME_IN_SEC` (`Agent.cpp`) sowie
      `usbDeviceType`/`usbClassType`/`usbDeviceVID`/`usbDevicePID`/`usbVersion`/
      `usbDeviceVersion` (alle sechs in `USBCore.cpp`). Ein einzelner HTTP-Request wie
      `GET /set?name=usbDeviceVID&value=zzz` reicht aus, um das Gerät mitten in einem laufenden
      Pentest abstürzen zu lassen — unauthentifiziert, ohne jede Interaktion am Zielsystem.
- [x] Beim Analysieren selbst eine erste Vermutung widerlegt, bevor sie als Fund behauptet wurde:
      `"0xZZZZ"`/`"0x"` allein werfen entgegen der ersten Annahme NICHT — `std::stoi(value,
      nullptr, 16)` folgt der `strtol`-Präfixregel (`"0x"` nur als Präfix erkannt, wenn direkt
      eine gültige Hex-Ziffer folgt, sonst wird nur die führende `"0"` geparst) und liefert `0`
      zurück statt zu werfen. Per Host-Probe verifiziert, bevor der eigentliche Repro-Satz
      geschrieben wurde — tatsächlicher Wurfpfad im UInt16-Zweig ist ein zu langer/großer
      Hex-Wert (`"0x100000000"`, `"0x80000000"`, > `INT_MAX`), nicht ein ungültiges Zeichen nach
      `"0x"`. Der Misparse selbst (ungültiger Hex-Rest wird stillschweigend als `0` statt als
      Fehler behandelt) ist ein eigener, deutlich harmloserer, bewusst nicht mitbehobener
      Nebenfund (kein Absturz).
- [x] Fix: `try`/`catch` um beide `std::stoi()`-Aufrufstellen (`Int16`- und `UInt16`-Zweig),
      loggt denselben "Invalid setting value"-Hinweis wie die bereits vorhandenen
      Validierungszweige, `ret` bleibt `false` — kein Verhaltensunterschied für jeden bereits
      gültigen Wert. Patch (`git apply --check` gegen eine frische, unabhängige Zweitkopie von
      `main` Commit `411b153` verifiziert, real angewendet, byte-für-byte mit der separat
      editierten Fassung abgeglichen — identisch) in
      `app-improvements/usb-army-penetrator/settings-int-uint16-stoi-uncaught-exception-crash-fix/settings_cpp.patch`.
- [x] Host-Repro (ALT/NEU 1:1 aus `setSettingValue()`s Int16-/UInt16-Logik, kein
      Arduino/ESP32-Header nötig) unter `g++ -std=c++17 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined` verifiziert: 30 Prüfungen, ALLE CHECKS BESTANDEN, identischer
      Output beider Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen —
      in
      `app-improvements/usb-army-penetrator/repro-settings-int-uint16-stoi-uncaught-exception/repro.cpp`.
- [x] Details, Nutzen, betroffene Datei, Testhinweis, Risiko in
      `app-improvements/usb-army-penetrator/settings-int-uint16-stoi-uncaught-exception-crash-fix.md`.
      Bewusst nicht mitbehoben: der oben beschriebene Hex-Misparse-Nebenfund (harmlos, kein
      Absturz) — für eine künftige Sitzung vorgemerkt, falls eine strengere Hex-Validierung
      gewünscht wird. Restlicher, bereits aus Fortsetzung 5 bekannter offener Punkt
      (Ethernet CH9120/W5500, unverdrahteter Stub ohne angeschlossene Hardware) bleibt
      unverändert außerhalb reiner Quelltextprüfung offen.

## Firmware-Feature-Mining (Fortsetzung 27)
- [x] Aus der seit der Beninca-ARC-Sitzung offenen `unlshd-093`-Restliste (`allstar_firefly`,
      `ditec_gol4`, `jarolift`, `keyfinder`, `nord_ice`, `treadmill37`) **`jarolift.c`**
      aufgegriffen — letzte Sitzung war App-Verbesserung (usb-army-penetrator), Firmware-Mining
      seit Beninca ARC nicht mehr an der Reihe.
- [x] Kompatibilitätsprüfung gegen `mntm-012`: KeeLoq-Kern
      (`subghz_protocol_keeloq_common_encrypt/_decrypt/_normal_learning`) sowie
      `custom_btn_i.h`/`custom_btn.c` (Tasten-Remap) sind bei mir byteidentisch vorhanden —
      keine neue Krypto-Primitive nötig. Es fehlen: die `KEELOQ_LEARNING_NORMAL_JAROLIFT`-
      Konstante selbst (bei mir nur Lerntypen 0–8 definiert), die komplette
      `subghz_block_generic_global`-Override-Infrastruktur (wie bereits bei Beninca ARC
      dokumentiert) und `common.c`/`.h` (Unleasheds Flash-Spar-Refactor, komplett nicht Teil
      meiner Basis). Port im etablierten eigenen Stil (eigene `_alloc/_free/_reset/_stop/
      _yield`, eigene `serialize()` statt `common.c`-Helfer, direktes
      `furi_hal_subghz_get_rolling_counter_mult()`-Muster statt `_global`-Override, kein
      Editor-UI-Push in `get_string()`) — exakt dasselbe Anpassungsmuster wie beim
      Beninca-ARC-Port.
- [x] **Fund (in Unleashed selbst, nicht durch die Portierung eingeführt)**: beim
      Host-Repro-Rundlauf (Encode→Decode über 50 000 zufällige Serial/Btn/Cnt/Seed-
      Kombinationen) fiel auf, dass Unleasheds eigene
      `subghz_protocol_jarolift_remote_controller()` das untere Byte des Grouping-/Seed-Felds
      systematisch verliert: `instance->seed = ((decrypt >> 24) << 8) | (group >> 8);` — `group`
      ist nach der doppelten 8-Bit-Spiegelung bereits der reine untere Byte-Wert (Bit 0–7), die
      zusätzliche `>> 8` liefert für einen solchen Wert immer 0. Richtig: `| group` ohne
      Rechtsschiebung. Praktische Relevanz: kein reiner Anzeigefehler — `seed` ist auch die
      einzige Quelle für `data_2` beim erneuten Senden (mit und ohne Tasten-Remap), ein mit dem
      Original geladenes und erneut gesendetes Signal würde also mit einem auf das untere Byte
      Null gesetzten Kanal-/Grouping-Byte gesendet, sobald der echte Wert nicht zufällig schon
      auf `0x00` endet. Mit zufälligen Seeds bestätigt: Unleashed-Formel trifft nur 84/20000
      (≈1-zu-238, nahe am erwarteten 1-zu-256 für ein zufälliges Byte), meine korrigierte
      Fassung 20000/20000. In meinen Port übernommen und mit Kommentar an der Stelle
      dokumentiert.
- [x] Host-Repro (KeeLoq-Kern-Rundlauf, `normal_learning`-Rundlauf, Encode→Decode-Rundlauf mit
      synthetischem Testschlüssel, Seed-Bug-Demonstration Original-vs-Fix, "Data"-Hex-Feld-
      Byte-Packing-Rundlauf, Falschschlüssel-Falsch-Akzeptanz-Rate) unter
      `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` verifiziert: 130004 Prüfungen, ALLE CHECKS BESTANDEN,
      identischer Output beider Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine
      Compiler-Warnungen — in
      `firmware-mining/repro-jarolift-keeloq-subghz-port/repro.c`.
- [x] Fünf Patches (`jarolift_c`/`_h`, `keeloq_common_h`, `protocol_items_c`/`_h`) — selbst
      erneut unabhängig gegen zwei frische Zweitkopien von `Next-Flip/Momentum-Firmware` Tag
      `mntm-012` mit `git apply --check` geprüft, real angewendet, byte-für-byte mit der
      Arbeitskopie abgeglichen — identisch. `KEELOQ_LEARNING_NORMAL_JAROLIFT` bewusst als `10u`
      statt `9u` definiert (mit Kollisionshinweis-Kommentar), da `9u` bereits vom noch nicht
      angewendeten `BENINCA_ARC_KEY_TYPE`-Vorschlag belegt ist — beide Patches vorab in beiden
      Reihenfolgen gegeneinander geprüft, kein Konflikt.
- [x] Details, Kompatibilitätsprüfung, kryptografische Prüfung, Fund, Risiko, Port-Skizze,
      Testhinweis in `firmware-mining/jarolift-keeloq-subghz-port.md`.
- [ ] Aus demselben `unlshd-093`-Vergleich bleiben fünf weitere, noch nicht geprüfte Kandidaten
      offen: `allstar_firefly`, `ditec_gol4`, `keyfinder`, `nord_ice`, `treadmill37`.
- [ ] Lokal zu tun: die fünf Patches gegen die eigene `mntm-012`-Basis anwenden, eigenen
      Jarolift-Herstellerschlüssel in der eigenen `subghz_keystore`-Ressource ergänzen, mit
      `ufbt`/`fbt firmware_all` bauen, am Gerät SubGHz → 433,92 MHz AM650 gegen einen echten
      Jarolift-Sender empfangen und Felder Sn/Btn/Cnt gegenprüfen; gezielt den Seed-Fix
      verifizieren, indem ein aufgezeichnetes Signal unverändert erneut gesendet wird und der
      Antrieb wie beim Original-Sender reagiert.

## Ideen-Recherche (Fortsetzung 15)
- [x] `reports/` durchgesehen: letzte Sitzung (Firmware-Feature-Mining, Jarolift) sowie die
      beiden Sitzungen davor (App-Verbesserung usb-army-penetrator, Masterplan-Sachstand-Check)
      bedienten nicht die Kategorie Ideen-Recherche — zuletzt am 2026-10-02 (SPI Flash Dump,
      siehe "Ideen-Recherche (Fortsetzung 14)") bedient. `ideas/pentest-app-katalog.md`
      durchgesehen: gezielt nach einer weiteren, bislang komplett fehlenden Kategorie gesucht,
      über die bestehende Bus-Analyse (UART Sniff/GPIO Logic Analyzer/i2c Tools/RS485 Modbus)
      und den reinen Chip-Dump (SPI Flash Dump) hinaus — Hardware-Debug-Schnittstellen
      (JTAG/SWD) als bislang unbesetzte Angriffs-/Auditfläche identifiziert.
- [x] **SWD Probe** (`g3gg0/flipper-swd_probe`, derselbe Autor wie der bereits katalogisierte
      GPIO Logic Analyzer) gefunden und vollständig gelesen (vier Quelldateien, ca. 4400
      Zeilen) — probiert ARM-SWD-Signale über acht Erweiterungsport-Pin-Paare per Bisektion
      durch, zeigt erkannten JEP106-Hersteller/Pinout, danach volle ARM-Debug-Interface-
      Implementierung (Register-/Speicherzugriff über Memory-AP, CoreSight-ROM-Table-Walk,
      Cortex-M-Haltepunkte) plus eigene Skriptsprache für wiederkehrende Abläufe. API-87-Status
      gegen den eigenen `mntm-012`-Tag-Stand verifiziert (frischer, blobloser Checkout): alle
      acht referenzierten Erweiterungsport-Pins sowie die USB-CDC-Zweitkanal-Technik
      (`furi_hal_usb_set_config()`/`usb_cdc_single`/`_dual`, `furi_hal_cdc_set_callbacks()`,
      `furi_hal_usb_unlock()`) identisch zu GPIO Logic Analyzer/UART Sniff bereits verifizierter
      Basis vorhanden.
- [x] Dabei eine bislang bei GPIO Logic Analyzer/UART Sniff nicht explizit dokumentierte
      Besonderheit präzisiert: `usb_uart.c` öffnet die CLI-Sitzung über die *alte*
      `cli_session_open()`/`cli_session_close()`-API (globaler `cli_vcp`-Zeiger) statt der im
      eigenen `mntm-012`-Stand tatsächlich vorhandenen, record-basierten
      `cli_vcp_enable()`/`cli_vcp_disable()`-API (`applications/services/cli/cli_vcp.h`) — per
      Gegenprobe an einer frischen Zweitkopie von `g3gg0/flipper-logic_analyzer` bestätigt, dass
      dieselbe ältere API dort Zeile für Zeile identisch verwendet wird, SWD Probe also kein
      Einzelfall ist. Läuft bei mir firmwareseitig bereits über den in `BACKLOG.md` unter
      "Kompatibilität" dokumentierten CLI-Kompat-Shim für Alt-Apps (`cli_session_open/close`,
      `cli_vcp`) — kein eigener Patch nötig, beim eigenen Bau aber nicht versehentlich auf die
      aktuelle API "reparieren" (würde den bereits funktionierenden Shim-Pfad umgehen).
- [x] Fund: zwei vom Dateibrowser gelieferte Pfade (`FuriString`, keine Längenbegrenzung — ein
      Flipper-SD-Pfad kann durch verschachtelte Ordner deutlich länger als 127 Zeichen werden)
      landen ungeprüft per `strcpy()` in feste `char[128]`-Felder (`MAX_FILE_LENGTH`) zweier
      `malloc()`-allozierter Structs (`ScriptContext.filename` in `swd_execute_script()`,
      `AppFSM.script_detected` im Dateibrowser-"Down"-Handler in `ModePageScan`/`ModePageFound`)
      — ein echter Heap-Buffer-Overflow ab 128 Zeichen Gesamtpfadlänge, kein reiner
      `furi_check()`-Abbruch. Die beiden anderen `strcpy()`/`strcat()`-Stellen im selben File
      sind bereits sauber längengeprüft (`swd_scriptfunc_call()`/`swd_scriptfunc_mem_dump()`).
- [x] Fix: additive Längenprüfung vor beiden bisher ungeschützten Stellen, analog zum bereits
      vorhandenen Muster im selben File (Fehlermeldung über `state_string` statt stillem
      Überlauf). Patch (`git apply --check` vorab gegen eine frische, unabhängige Zweitkopie von
      `g3gg0/flipper-swd_probe` verifiziert, real angewendet, byte-für-byte mit der Arbeitskopie
      abgeglichen) in `firmware-mining/swd-probe-app-port/swd_probe_app_c.patch`. Host-Repro
      (beide `strcpy()`-Stellen 1:1 nachgebildet, vier Szenarien: Regression kurzer Pfad,
      Grenzfall exakt 127 Zeichen, Fund bei 496-Byte-Pfad, Fix-Nachweis für beide Stellen) unter
      `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` verifiziert: ALT bricht reproduzierbar mit
      AddressSanitizer `heap-buffer-overflow` ab, NEU übersteht denselben Fall sauber mit
      `"Path too long"`-Meldung, Regressions-/Grenzfall identisch — keine Compiler-Warnungen,
      keine ASan-/UBSan-Meldung bei NEU — in
      `firmware-mining/repro-swd-probe-script-path-oob-write/repro.c`.
- [x] Details, Kompatibilitätsprüfung, Fund, Risiko, Port-Skizze, Testhinweis in
      `firmware-mining/swd-probe-app-port.md`. Kuratiert in `ideas/swd-probe-api-status.md`.
      `ideas/pentest-app-katalog.md` um neue Kategorie "Hardware Debug Interface (JTAG/SWD)"
      sowie einen Log-Eintrag unter "Nächste Recherche-Aufträge" ergänzt.
- [ ] Lokale Bauprobe gegen die eigene `mntm-012`-Basis, danach Funktionstest an einem eigenen
      Cortex-M-Board mit bekanntem, offenem SWD-Port (Pin-Erkennung, `IDCODE`/JEP106-Anzeige,
      Speicherlesen über die ADI-Implementierung); gezielt den Pfadlängen-Fix nachvollziehen
      (`.swd`-Skript in einem ≥128 Zeichen tiefen SD-Ordner über den Dateibrowser auswählen).

## App-Verbesserung (lilygo-ukfe-rx — neu erfasst)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (SWD Probe), davor
      Firmware-Feature-Mining (Jarolift) und App-Verbesserung (usb-army-penetrator) —
      App-Verbesserung für diese Sitzung gewählt. `lilygo-ukfe-rx` (LilyGo-T-Dongle-S3-
      ESP-NOW-Satellit, natives USB-HID) war bisher in keinem `app-improvements/`-Durchgang
      erfasst (`RollJam`, `RollLab`, `RollForge`, `ProtoPirate`, `FlipperJammer`,
      `g4meover-magspoof`, `usb-army-penetrator`, `lora-ukfe`, `heltec-v4-hub`,
      `heltec-ukfe-rx`, `ukfe-esp-now-mesh` bereits mindestens einmal geprüft) — gezielt
      aufgegriffen statt eines bereits mehrfach durchgesehenen Repos.
- [x] Frische, unabhängige Kopie von `G4MEOVER18/lilygo-ukfe-rx` (main, Commit `aa8a285`)
      geklont, vollständige Dateiliste gelesen (`main.cpp`, `ukfe_rf.c`/`.h`, `wifi_attack.cpp`/
      `.h`, `ble_spam.cpp`/`.h`, `ducky.h` — 974 Zeilen gesamt). `send_beacon()`-Puffergrenzen,
      `ducky_run()`-Zeilenpuffer, `HidStream`-Chunk-Zusammenbau sowie die ESP-NOW-Mailbox
      (`enowFlag`/`enowBuf`/`enowLen`) geprüft — jeweils bereits korrekt längengeprüft, keine
      Speicherfehler gefunden.
- [x] **Fund**: `enowCounter` (Anti-Replay-Rolling-Counter der WROOM→LilyGo-ESP-NOW-Richtung)
      ist ein reines RAM-Feld, startet bei jedem Neustart wieder bei `0` — exakt derselbe Fund
      wie bereits in `usb-army-penetrator` und `heltec-v4-hub` dokumentiert, hier im vierten
      geprüften Gerät. Besonders relevant, weil der LilyGo Befehle (`HidDucky`/`HidStream`/
      `PayloadRun`/`WifiDeauth`/`BeaconSpam`/`BleSpam`) direkt am angeschlossenen Zielrechner
      bzw. per Funk ausführt — ein mitgeschnittenes, gültig signiertes Frame kann nach jedem
      Neustart erneut eingespielt werden und löst die Aktion (z. B. eine BadUSB-HID-Payload)
      ein zweites Mal aus, ohne dass der Operator erneut gesendet hat.
- [x] Fix: dieselbe bereits zweimal verifizierte Reserve-mit-Sicherheitsspanne-Logik
      (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`, NVS über
      `Preferences`, Namespace `g4meoverLily`) ein viertes Mal angewendet. Patch
      (`git apply --check` gegen eine frische, unabhängige Zweitkopie verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch) in
      `app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`.
- [x] Host-Repro (echte `ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()`, 50 signierte
      `HidDucky`-Frames, Mitschnitt- und Neustart-Simulation, Ist-Zustand vs. Fix, Crash-
      Sicherheit des persistierten Floors) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: alle
      Szenarien bestätigt, identischer Output beider Optimierungsstufen, keine ASan-/UBSan-
      Meldungen, keine Compiler-Warnungen — in
      `app-improvements/lilygo-ukfe-rx/repro-rf-counter-replay-window-reset/repro.c`.
- [x] Details, Nutzen, betroffene Datei, Risiko, Testhinweis in
      `app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md`.
      Bewusst nicht angefasst: `send_espnow_ack()`/`respCounter` (Rückweg LilyGo→WROOM, reiner
      Verfügbarkeits-Bug bei RAM-Reset, keine Sicherheitslücke).
- [ ] Lokal zu tun: Patch einbauen, PlatformIO-Build, auf den LilyGo flashen. Funktionstest:
      `HidDucky`-Befehl vom WROOM-Hub senden, LilyGo per Reset-Taste neu starten, zuvor
      mitgeschnittenes Frame erneut per ESP-NOW einspielen — erwartet: keine erneute HID-
      Eingabe. Erst-Inbetriebnahme (NVS-Namespace `g4meoverLily` einmal löschen) sowie harter
      Reset mitten in einer laufenden Sitzung gegenprüfen.
- [ ] Aus demselben Ökosystem-Katalog bleiben weitere, noch nicht auf diesen Fund geprüfte
      Geräte offen: `lilygo-ukfe-rx`s LilyGo→WROOM-Rückweg (`respCounter`, s. o., nur
      Verfügbarkeit), sowie `g4meover-wifi-relay` (noch nie auf den `ukfe_rf`-Anti-Replay-
      Fund geprüft).

## Firmware-Feature-Mining (Fortsetzung 28)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (lilygo-ukfe-rx), davor
      Ideen-Recherche (SWD Probe) und Firmware-Feature-Mining (Jarolift) — Firmware-Feature-
      Mining für diese Sitzung entsprechend wieder an der Reihe. Repo-Zustand geprüft: HEAD war
      detached auf demselben Commit wie `origin/main`, lokaler `main`-Zeiger stand 10 Commits
      zurück — per `git fetch`/`git checkout main`/`git merge --ff-only origin/main` behoben
      (reines Zeiger-Problem, kein Datenverlust). Git-Identität vor dem ersten Commit gesetzt.
- [x] Aus der seit der Beninca-ARC-Sitzung offenen `unlshd-093`-Restliste (`allstar_firefly`,
      `ditec_gol4`, `keyfinder`, `nord_ice`, `treadmill37` — `jarolift` bereits in Fortsetzung 27
      aufgegriffen) **`nord_ice.c`** gewählt: einziges der fünf verbleibenden Kandidaten ohne
      jede Kryptografie (statisches 33-Bit-Protokoll) — bewusst der kleinste, risikoärmste der
      fünf offenen Kandidaten, nachdem die letzten beiden Firmware-Mining-Sitzungen bereits
      KeeLoq/AES-Protokolle mit Keystore-Abhängigkeit brachten.
- [x] Kompatibilitätsprüfung gegen `mntm-012`: jedes von `nord_ice.c` verwendete Kern-Symbol
      (`SubGhzBlockConst`/`Decoder`/`Generic`, `subghz_protocol_blocks_add_bit/
      _get_hash_data/_reverse_key`, `subghz_block_generic_serialize/
      _deserialize_check_count_bit`, `DURATION_DIFF`, `SubGhzProtocolTypeStatic`) ist
      byteidentisch vorhanden. Einzig fehlender Teil: `common.c`/`.h` (Unleasheds
      Flash-Spar-Refactor, wie bei Beninca ARC/Jarolift bereits dokumentiert nicht Teil meiner
      Basis) — hier aber folgenlos, da Nord ICE weder Keystore noch Zähler-/Tasten-Override
      benutzt, also reines Boilerplate ohne funktionale Anpassung.
- [x] Beim Durchgehen von `check_remote_controller()`/`get_upload()`/`feed()` **kein** Fehler
      gefunden (anders als bei Beninca ARC und Jarolift) — einzig eine Kommentarungenauigkeit
      im Original (`// 26 bits for serial`, tatsächlich 27 belegte Bit, davon 26 echte Entropie)
      sowie eine dokumentierte, aber folgenlose Eigenschaft des unveränderten GAP-Timings bei
      mehreren Wiederholungen (nur jede zweite Wiederholung liefert einen eigenständigen
      Dekodier-Treffer, ein einzelner Treffer reicht für den praktischen Empfang).
- [x] Host-Repro (vier reale Tastendruck-Beispiele aus dem Original-Kommentar gegen
      `check_remote_controller()`, Encode→Decode-Rundlauf über 20 000 zufällige 33-Bit-Werte mit
      1–5 Wiederholungen inkl. der GAP-Wiederholungs-Eigenschaft, zwei Negativfälle für zu kurzes
      GAP bzw. verstümmeltes Bit-Timing) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: alle vier
      Szenarien bestanden, identischer Output beider Optimierungsstufen, keine ASan-/UBSan-
      Meldungen, keine Compiler-Warnungen — in
      `firmware-mining/repro-nord-ice-subghz-port/repro.c`.
- [x] Vier Patches (`nord_ice_c`/`_h`, `protocol_items_c`/`_h`) — gegen eine frische,
      unabhängige `mntm-012`-Zweitkopie mit `git apply --check` geprüft, real angewendet,
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch. Zusätzlich gegen eine dritte
      Zweitkopie mit bereits angewendetem Beninca-ARC- und Jarolift-`protocol_items`-Patch
      geprüft: Beninca-ARC-Reihenfolge konfliktfrei, Jarolift-Reihenfolge erzeugt einen
      Kontext-Versatz am Dateiende (beide hängen ihren Eintrag an dieselbe Anker-Zeile an) —
      dokumentiert, mit einzeiliger manueller Alternative.
- [x] Details, Kompatibilitätsprüfung, Fund (keiner), Risiko, Port-Skizze, Testhinweis in
      `firmware-mining/nord-ice-subghz-port.md`.
- [ ] Aus demselben `unlshd-093`-Vergleich bleiben vier weitere, noch nicht geprüfte Kandidaten
      offen: `allstar_firefly`, `ditec_gol4`, `keyfinder`, `treadmill37`.
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden (Reihenfolge
      gegenüber Jarolift beachten, siehe oben), mit `ufbt`/`fbt firmware_all` bauen, am Gerät
      SubGHz → 433,92 MHz AM650 gegen einen echten Nord-ICE-Sender empfangen und Serial/Btn
      gegenprüfen; gezielt das Repeat/GAP-Verhalten nachvollziehen (Signal mit `Repeat` > 1
      senden, gegenprüfen dass nur jede zweite Wiederholung einen Empfänger-Treffer auslöst).

## Ideen-Recherche (Fortsetzung 16)
- [x] `reports/` durchgesehen: letzte Sitzung (`2026-10-08-0635.md`) war Firmware-Feature-Mining
      (Nord ICE), davor App-Verbesserung (lilygo-ukfe-rx) und Ideen-Recherche (SWD Probe) —
      Ideen-Recherche für diese Sitzung entsprechend wieder an der Reihe (längste Zeit seit
      letztem Durchgang dieser Kategorie unter den drei rotierenden Bereichen). Repo-Zustand
      geprüft (HEAD stand sauber auf `origin/main`, kein Korrekturbedarf). Git-Identität vor dem
      ersten Commit gesetzt.
- [x] Gezielt nach einer bislang komplett fehlenden Kategorie gesucht: Fahrzeug-CAN-Bus, über
      die bestehende Bus-Analyse (UART Sniff/GPIO Logic Analyzer/i2c Tools/RS485 Modbus) und den
      Hardware-Debug-Port (SWD Probe) hinaus. Fünf CAN-Apps im RogueMaster-Bündel
      (`applications/external/`, Branch `420`) gefunden: `can_bus_attack`, `can_commander`,
      `can_tools`, `can_transceiver`, `canbus` — vier davon mit MCP2515-Transceiver-Abhängigkeit.
      Als ersten, kleinsten und einzigen hardwarefreien Kandidaten **CAN Tools**
      (`MatthewKuKanich/FlipperCANTools`) gewählt: reine DBC-Dateierstellung und
      Signaldekodierung, komplett offline nutzbar.
- [x] Vollständig gelesen (eine Quelldatei, `cantools_app.c`, 2419 Zeilen). API-87-Kompatibilität
      gegen eine frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware` (Tag
      `mntm-012`) geprüft: alle verwendeten GUI-/Storage-/Stream-/Dialogs-Symbole (u. a.
      `byte_input_set_result_callback()`s sechs Parameter) byteidentisch vorhanden, keine
      Erweiterungsport-Pins oder sonstige Hardware-HAL-Aufrufe (passend zum reinen
      Offline-Charakter) — kein Port-Hindernis.
- [x] Fund: `dbc_parse_sg_line()` liest beim Laden einer `.dbc`-Datei nach dem Startbit-/
      Bitlängen-Feld zwei weitere Zeichen (Endianness/Signedness) bedingungslos, ohne vorherige
      Prüfung, ob nach dem abschließenden `@` überhaupt noch Zeichen folgen — anders als jedes
      andere Feld im selben Parser, das jeweils über `if(end == p || *end != '…') return false;`
      abgesichert ist. Für eine kurze, unvollständige Zeile liest das mindestens 1 Byte
      uninitialisierten Stack-Speicher (noch innerhalb des 256-Byte-Zeilenpuffers aus
      `dbc_load_file()`); für eine exakt 255 Byte lange, mit `@` endende Zeile (z. B. durch
      einen entsprechend langen Signalnamen, den der Parser beim Scannen nicht begrenzt) liegt
      der zweite Zeichenzugriff exakt 1 Byte hinter dem deklarierten Array — ein echter
      Lesezugriff außerhalb des Puffers.
- [x] Host-Repro (Regressionsfall mit vollständiger, gültiger Zeile; zwei Fundszenarien; plus
      direkter Nicht-Fork-Nachweis des vollen AddressSanitizer-Berichts) unter
      `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` verifiziert: alle Szenarien bestätigt (`stack-buffer-
      overflow READ of size 1`, exakt 1 Byte hinter dem Array, direkt per ASan-Bericht
      nachgewiesen), identischer Output beider Optimierungsstufen, keine Compiler-Warnungen —
      in `firmware-mining/repro-can-tools-sg-line-parser-oob-read/repro.c`.
- [x] Patch (additive Prüfung `if(!p[0] || !p[1]) return false;` vor den beiden Zeichenzugriffen,
      analog zum bereits vorhandenen Validierungsmuster im selben Parser) — per
      `git apply --check` gegen eine frische, unabhängige Zweitkopie des RogueMaster-Bündels
      (Branch `420`, nur `applications/external/can_tools/`) verifiziert, real angewendet,
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch. In
      `firmware-mining/can-tools-app-port/cantools_app_c.patch`.
- [x] Details, Nutzen, API-87-Kompatibilität, Fund, Risiko, Port-Skizze, Testhinweis in
      `firmware-mining/can-tools-app-port.md`. Kuratiert in `ideas/can-tools-api-status.md`.
      `ideas/pentest-app-katalog.md` um neue Kategorie "CAN-Bus" (mit den vier übrigen,
      hardwareabhängigen Apps als offene Kandidaten) sowie einen Log-Eintrag unter "Nächste
      Recherche-Aufträge" ergänzt.
- [x] Nebenfund notiert, bewusst nicht in diesem Patch behoben: `sanitize_filename()` (selbe
      Datei) filtert Zeichen mit `(c >= 'A' && c <= 'z')` statt `(c >= 'A' && c <= 'Z')` — lässt
      dadurch fünf zusätzliche ASCII-Zeichen (`[`, `\`, `]`, `^`, `` ` ``) unersetzt durch den
      Dateinamen-Filter; ohne erkennbare Sicherheitswirkung, da `.` (für `..`-Traversal) bereits
      nicht im erlaubten Zeichensatz enthalten ist und Flipper-Pfade `/` statt `\` als Trenner
      verwenden — eigenständiger, kleinerer Folgefund für eine künftige Sitzung.
- [ ] Lokal zu tun: `can_tools` (mit Patch) gegen die eigene `mntm-012`-Basis bauen, am Gerät
      DBC Maker/View DBC/Decode Data gegenprüfen (Regressionstest), danach den Fundnachvollzug
      aus `firmware-mining/can-tools-app-port.md` (präparierte `.dbc`-Datei mit einer exakt mit
      `@` endenden `SG_`-Zeile) durchführen.
- [ ] Aus demselben CAN-Bus-Fund bleiben vier weitere, noch nicht geprüfte Kandidaten offen:
      `can_bus_attack`, `can_commander`, `can_transceiver`, `canbus` — alle vier benötigen ein
      externes MCP2515-Transceiver-Modul am Erweiterungsport.
- [ ] Der `sanitize_filename()`-Zeichenbereichs-Nebenfund (siehe oben) bleibt als eigenständiger,
      kleinerer Folgepunkt offen.

## App-Verbesserung (g4meover-wifi-relay — neu erfasst)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (CAN Tools), davor
      Firmware-Feature-Mining (Nord ICE) und App-Verbesserung (lilygo-ukfe-rx) —
      App-Verbesserung für diese Sitzung entsprechend wieder an der Reihe. Repo-Zustand zu
      Sitzungsbeginn geprüft: HEAD stand detached auf demselben Commit wie `origin/main` (nach
      `git fetch`), lokaler `main`-Zeiger stand 12 Commits zurück — per `git checkout main` +
      `git merge --ff-only origin/main` behoben (reines Zeiger-Problem, kein Datenverlust).
      Git-Identität vor dem ersten Commit gesetzt.
- [x] Aus dem seit der `lilygo-ukfe-rx`-Sitzung offenen Punkt "`g4meover-wifi-relay` noch nie
      auf den `ukfe_rf`-Anti-Replay-Fund geprüft" aufgegriffen — einziges noch offenes Gerät des
      Ökosystems für diesen bereits dreimal (`usb-army-penetrator`, `heltec-v4-hub`,
      `lilygo-ukfe-rx`) bestätigten Fundmuster. Frische, unabhängige Kopie von
      `G4MEOVER18/g4meover-wifi-relay` (main, Commit `9b4a9e1`) geklont, vollständige
      Dateiliste gelesen (`main.cpp`, `ukfe_rf.c`/`.h` — 456 Zeilen gesamt). UART-Framing-
      Akkumulator (`scan_and_relay()`), ESP-NOW-Empfang (`on_espnow_recv()`), CLI-Hex-Injektion
      (`cli_inject_hex()`) geprüft — jeweils bereits korrekt längengeprüft, keine
      Speicherfehler gefunden.
- [x] **Fund**: `s_rxCounter` (Anti-Replay-Rolling-Counter der Flipper→Relay-Richtung, an alle
      Satelliten per ESP-NOW-Broadcast weitergereicht) ist ein reines RAM-Feld, startet bei
      jedem Neustart wieder bei `0` — exakt derselbe Fund wie bereits in
      `usb-army-penetrator`/`heltec-v4-hub`/`lilygo-ukfe-rx` dokumentiert, hier im fünften
      geprüften Gerät. Besonders relevant, weil der Relay die zentrale Weiterleitungsstelle für
      alle vom Flipper ausgelösten Befehle an *alle* Satelliten gleichzeitig ist
      (`WifiDeauth`/`BeaconSpam`/`EvilPortal`/`Trigger`/`PayloadRun`) — ein Neustart allein
      reicht aus, damit ein zuvor mitgeschnittener Befehl erneut an alle Satelliten
      rebroadcastet wird. Nebenbeobachtung: derselbe Code persistiert `s_txCounter` (CLI-Ping)
      bereits korrekt per NVS-Margin — nur der sicherheitsrelevante `s_rxCounter` war
      übersehen.
- [x] Fix: dieselbe bereits dreimal verifizierte Reserve-mit-Sicherheitsspanne-Logik
      (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`) ein fünftes Mal
      angewendet, diesmal im bereits vorhandenen `Preferences`-Objekt (`g_prefs`, Namespace
      `"g4meover"`) mit neuem Key `"rxctr"` statt eines neuen Namespaces. Patch
      (`git apply --check` gegen eine frische, unabhängige Zweitkopie verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch) in
      `app-improvements/g4meover-wifi-relay/rx-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`.
- [x] Host-Repro (echte `ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()`, 50 signierte
      `UkfeRfCmdWifiDeauth`-Frames, Mitschnitt- und Neustart-Simulation, Ist-Zustand vs. Fix,
      Crash-Sicherheit des persistierten Floors) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: alle
      Szenarien bestätigt, identischer Output beider Optimierungsstufen, keine ASan-/UBSan-
      Meldungen, keine Compiler-Warnungen — in
      `app-improvements/g4meover-wifi-relay/repro-rx-counter-replay-window-reset/repro.c`.
- [x] Details, Nutzen, betroffene Datei, Risiko, Testhinweis in
      `app-improvements/g4meover-wifi-relay/rx-counter-replay-window-reset-on-reboot-fix.md`.
- [ ] Lokal zu tun: Patch einbauen, PlatformIO-Build, auf den WROOM flashen. Funktionstest:
      `WifiDeauth`-Befehl vom Flipper senden, den Relay per Reset-Taste oder Stromtrennen neu
      starten, zuvor mitgeschnittenes Frame erneut einspielen — erwartet: `s_rejected` steigt,
      kein erneuter Rebroadcast an die Satelliten. Erst-Inbetriebnahme (NVS-Namespace
      `g4meover` einmal löschen) sowie harter Reset mitten in einer laufenden Sitzung
      gegenprüfen.
- [ ] Damit ist der `ukfe_rf`-Anti-Replay-Fund jetzt in allen fünf bekannten Ökosystem-Geräten
      (`usb-army-penetrator`, `heltec-v4-hub`, `lilygo-ukfe-rx`, `g4meover-wifi-relay`) als
      Port-Vorschlag dokumentiert. Weiterhin offen: der in `lilygo-ukfe-rx` als reiner
      Verfügbarkeits-Nebenfund zurückgestellte Rückweg (`respCounter`, LilyGo→WROOM), sowie das
      in dieser Sitzung notierte Restrisiko, dass die jeweiligen Satelliten-Fixes lokal noch
      nicht zwingend eingebaut/geflasht sind.

## Firmware-Feature-Mining (Fortsetzung 29)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (g4meover-wifi-relay), davor
      Ideen-Recherche (CAN Tools) und Firmware-Feature-Mining (Nord ICE) — Firmware-Feature-
      Mining für diese Sitzung entsprechend wieder an der Reihe. Repo-Zustand geprüft (`git
      status` sauber, lokaler `main`-Zeiger deckungsgleich mit `origin/main`). Git-Identität vor
      dem ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der seit der Beninca-ARC-Sitzung offenen `unlshd-093`-Restliste (`allstar_firefly`,
      `ditec_gol4`, `keyfinder`, `treadmill37` — `jarolift`/`nord_ice` inzwischen aufgegriffen)
      **`keyfinder.c`** gewählt: mit 287 Zeilen die kürzeste der vier verbleibenden, unkrypto-
      grafischen Kandidaten (statisches 24-Bit-Protokoll, 20 Bit Serial + 4 Bit ID).
- [x] Kompatibilitätsprüfung gegen `mntm-012`: jedes von `keyfinder.c` verwendete Kern-Symbol
      (`SubGhzBlockConst`/`Decoder`/`Generic`, `subghz_protocol_blocks_add_bit/_get_hash_data/
      _reverse_key`, `subghz_block_generic_serialize/_deserialize_check_count_bit`,
      `DURATION_DIFF`) byteidentisch vorhanden. Einzig fehlender Teil wie bei Beninca ARC/
      Jarolift/Nord ICE: `common.c`/`.h` (Unleasheds Flash-Spar-Refactor) — folgenlos, da
      KeyFinder weder Keystore noch Zähler-/Tasten-Override benutzt.
- [x] **Fund**: `feed()`s separater `KeyFinderDecoderStepFinish`-Zustand liest seine eigenen
      `level`/`duration`-Parameter nicht — der Rahmenende-LOW-Impuls (`te_short*10`), der genau
      in diesem Aufruf ankommt, wird dadurch verworfen, statt von `Reset` als Scharfschalt-
      Signal für den nächsten Rahmen erkannt zu werden. Bei unmittelbar aufeinanderfolgenden
      Wiederholungen (exakt wie `encoder.yield()` sie tatsächlich sendet — kein zusätzlicher
      Leerlauf zwischen den Wiederholungen) wird dadurch nur jede zweite Wiederholung
      (`gefunden == ceil(repeat/2)`) tatsächlich dekodiert; die vier echten Tastendruck-Beispiele
      aus dem Original-Kommentar selbst dekodieren unverändert korrekt (Bit-Extraktion ist nicht
      betroffen). Praktisch folgenlos (ein Treffer pro Tastendruck reicht), aber eine reine
      Zustandsmaschinen-Falle statt eines dem Format inhärenten Timing-Merkmals wie bei Nord
      ICE — daher per kleiner, risikoarmer Korrektur (Fund-Callback/Reset eine Stufe früher,
      direkt beim Erkennen von `end_count == 4`, statt im separaten Folgezustand) behoben;
      `KeyFinderDecoderStepFinish` dadurch im Port überflüssig und entfernt.
- [x] Host-Repro (vier reale Tastendruck-Beispiele aus dem Original-Kommentar gegen
      `check_remote_controller()`; Einzelrahmen-Encode→Decode-Rundlauf über 20 000 zufällige
      24-Bit-Werte; Fund-Nachvollzug mit unverändertem Original über 5000 zufällige Werte mit
      1–5 Wiederholungen, `gefunden == ceil(repeat/2)` bestätigt; Korrektur-Verifikation mit
      derselben Eingabe, `gefunden == repeat` bestätigt; zwei Negativfälle für verstümmelten
      Rahmenende- bzw. inneren Bit-Timing-Impuls) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: alle sechs
      Szenarien bestanden, identischer Output beider Optimierungsstufen, keine ASan-/UBSan-
      Meldungen, keine Compiler-Warnungen — in
      `firmware-mining/repro-keyfinder-subghz-port/repro.c`.
- [x] Vier Patches (`keyfinder_c`/`_h`, `protocol_items_c`/`_h`) — gegen eine frische,
      unabhängige `mntm-012`-Zweitkopie mit `git apply --check` geprüft, real angewendet,
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch. Zusätzlich gegen separate
      Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift- bzw. Nord-ICE-
      `protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge konfliktfrei, Jarolift- und
      Nord-ICE-Reihenfolge erzeugen je einen Kontext-Versatz am Dateiende (alle drei hängen
      ihren Eintrag an dieselbe Anker-Zeile an) — dokumentiert, mit einzeiliger manueller
      Alternative.
- [x] Details, Kompatibilitätsprüfung, Fund, Host-Repro, Risiko, Port-Skizze, Testhinweis in
      `firmware-mining/keyfinder-subghz-port.md`.
- [ ] Aus demselben `unlshd-093`-Vergleich bleiben zwei weitere, noch nicht geprüfte Kandidaten
      offen: `allstar_firefly`, `ditec_gol4` (beide ohne Kryptografie; `ditec_gol4.c` mit
      675 Zeilen deutlich umfangreicher), sowie `treadmill37` (280 Zeilen, noch nicht geprüft).
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden (Reihenfolge
      gegenüber Jarolift/Nord ICE beachten, siehe oben), mit `ufbt`/`fbt firmware_all` bauen,
      am Gerät SubGHz → 433,92 MHz AM650 gegen einen echten KeyFinder-Sender empfangen und
      Serial/ID gegenprüfen; gezielt das Repeat-Verhalten nachvollziehen (Signal mit `Repeat` > 1
      senden, gegenprüfen dass mit der Korrektur jede Wiederholung einen eigenen Treffer
      auslöst).

## App-Verbesserung (g4meover-wifi-relay — Antwort-Pfad, respCounter-Punkt richtiggestellt)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (KeyFinder), davor
      App-Verbesserung (g4meover-wifi-relay) und Ideen-Recherche (CAN Tools) — App-Verbesserung
      für diese Sitzung entsprechend wieder an der Reihe. Repo-Zustand zu Sitzungsbeginn
      geprüft: `HEAD` stand detached (Cloud-Heartbeat-Commit vom letzten Sachstand-Check war
      nicht auf `main` gemerged), per `git checkout main` + `git merge --ff-only` behoben, kein
      Datenverlust. Git-Identität vor dem ersten Commit gesetzt
      (`Yanis / G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Den zuletzt in `app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md`
      als "bewusst nicht angefasst, reiner Verfügbarkeits-Nebenfund" zurückgestellten
      `respCounter`-Punkt (LilyGo→WROOM-Rückweg) aufgegriffen. Vor dem vorgemerkten Fix geprüft,
      *wer* `respCounter` auf Empfängerseite auswertet (frische, unabhängige Kopie von
      `G4MEOVER18/g4meover-wifi-relay`, Commit `9b4a9e1`, `src/main.cpp` vollständig gelesen).
      Dabei festgestellt: die ursprüngliche Einordnung trifft nicht zu — `on_espnow_recv()`
      ruft `ukfe_rf_parse_frame(..., NULL)`, die Anti-Replay-Prüfung ist für **alle**
      Satelliten-Antworten vollständig deaktiviert (laut `ukfe_rf.h`-Kommentar bewusste
      Design-Entscheidung, nicht versehentlich). `respCounter` wird dadurch nie ausgewertet —
      weder vor noch nach einem Neustart; der vermutete Selbstaussperrungs-Effekt existiert
      nicht. Der eigentliche, größere Fund: ein mitgeschnittenes, gültig signiertes
      Antwort-Frame eines Satelliten kann **jederzeit, beliebig oft, ohne jeden Neustart**
      erneut eingespielt werden und wird unverändert an den Flipper-UART durchgereicht
      (veraltete/gefälscht "frische" Status-/Scan-/Ack-Anzeige am Flipper).
- [x] Fix: eigenes Anti-Replay-Fenster je Absender-MAC (`sat_counter_slot()`, feste Tabelle
      `SAT_COUNTER_TABLE_SIZE = 8`) statt `NULL` — löst damit auch korrekt das
      Kollisionsproblem, das `NULL` ursprünglich motiviert hat (ein gemeinsames Fenster über
      alle Satelliten hätte wie beim bereits behobenen Hub-Broadcast/Relay-Fund unabhängige
      Zählerräume gegenseitig blockiert). Kein NVS/`Preferences`-Bezug nötig, da ein
      Relay-Neustart die Tabelle leert und der erste Kontakt jedes Satelliten dann wie ein
      neuer Absender behandelt wird (Startwert `0`, sofort vom tatsächlichen Counter
      überholt) — anders als bei den bisherigen TX-/RX-Neustart-Funden entsteht hier kein
      Lockout-Risiko. Patch (`git apply --check` gegen eine frische, unabhängige Zweitkopie
      verifiziert, real angewendet, byte-für-byte abgeglichen — identisch) in
      `app-improvements/g4meover-wifi-relay/sat-resp-ack-replay-missing-counter-check-fix/main_cpp.patch`.
- [x] Host-Repro (echte `ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()`, Satellit mit fünf
      regulären ACKs, mitgeschnittenes Frame zehnmal erneut eingespielt; Ist-Zustand vs. Fix;
      zweiter, unabhängig zählender Satellit zur Kollisionsprobe; Tabelle-voll-Fallback) unter
      `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` verifiziert: alle Szenarien bestätigt, identischer
      Output beider Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen —
      in `app-improvements/g4meover-wifi-relay/repro-sat-resp-ack-replay-missing-counter-check/repro.c`.
- [x] Details, Nutzen, betroffene Datei, Risiko, Testhinweis in
      `app-improvements/g4meover-wifi-relay/sat-resp-ack-replay-missing-counter-check-fix.md`.
- [x] `respCounter`-Backlog-Punkt (`lilygo-ukfe-rx`) damit als gegenstandslos geschlossen: der
      vermutete Neustart-Lockout hat nie bestanden, der tatsächliche Fund (fehlende
      Replay-Prüfung jederzeit, nicht nur nach Neustart) ist jetzt am einzigen Empfangspunkt
      (Relay) für alle Satelliten einheitlich behoben, ohne Satelliten-Firmware-Änderung.
- [ ] Lokal zu tun: Patch einbauen, PlatformIO-Build, auf den WROOM flashen. Funktionstest:
      Befehl vom Flipper auslösen, ACK-Anzeige prüfen; mit einem zweiten ESP32 im
      Promiscuous-Modus ein zuvor mitgeschnittenes Satelliten-Antwort-Frame per ESP-NOW erneut
      an den Relay einspielen — erwartet: `resp rejected` (CLI `status`) steigt, keine erneute
      Anzeige am Flipper. Zusätzlich: zwei Satelliten gleichzeitig betreiben und gegenprüfen,
      dass sich beide weiterhin unabhängig voneinander durchreichen lassen; Relay-Neustart
      mitten in einer laufenden Sitzung (erster Kontakt jedes Satelliten danach sofort
      angenommen, kein Lockout).
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt, MIFARE-
      Plus-/Ultralight-AES-Rest, PSA-Bruteforce-Krypto-Konstanten, vier offene CAN-Bus-
      Kandidaten, `sanitize_filename()`-Zeichenbereichs-Nebenfund) bleiben unverändert offen.
      Aus demselben `unlshd-093`-Vergleich (siehe oben) bleiben `allstar_firefly`,
      `ditec_gol4`, `treadmill37` offen.

## Firmware-Feature-Mining (Fortsetzung 30)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (CAN Transceiver), davor
      App-Verbesserung (g4meover-wifi-relay, Antwort-Pfad) und Firmware-Feature-Mining
      (KeyFinder) — Firmware-Feature-Mining für diese Sitzung entsprechend wieder an der Reihe.
      Repo-Zustand zu Sitzungsbeginn geprüft: `HEAD` stand detached (ein vorheriger
      Cloud-Lauf hatte 17 Commits auf `HEAD`, der lokale `main`-Zeiger war nicht
      mitgezogen — `origin/main` war über den Fast-Forward hinweg bereits aktuell, reines
      Checkout-Artefakt ohne Datenverlust), per `git checkout main` + `git merge --ff-only`
      behoben. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der seit der Beninca-ARC-Sitzung offenen `unlshd-093`-Restliste (`allstar_firefly`,
      `ditec_gol4`, `treadmill37` — `jarolift`/`keyfinder`/`nord_ice` inzwischen aufgegriffen)
      **`treadmill37.c`** gewählt: mit 281 Zeilen die kürzere der beiden verbleibenden
      Kandidaten, architektonisch nahezu baugleich zur bereits portierten Nord ICE (gleiche
      drei Decoder-Zustände, gleiches Kurz/Lang-GAP-Timing-Schema, andere Konstanten/
      Bit-Layout).
- [x] Kompatibilitätsprüfung gegen `mntm-012`: jedes von `treadmill37.c` verwendete Kern-Symbol
      byteidentisch vorhanden (inkl. `SubGhzBlockGeneric.cnt`, von Nord ICE/KeyFinder nicht
      benötigt, hier aber verwendet). Einzig fehlender Teil wie bei den vier vorherigen Ports:
      `common.c`/`.h` — folgenlos, da Treadmill37 weder Keystore noch Zähler-/Tasten-Override
      benutzt.
- [x] **Fund 1** (Kommentarungenauigkeit, keine Funktionsänderung): Der Original-Kommentar
      stellt die sechs Beispielwerte als 40-Bit-Strings mit 23-Bit-"serial"-Block dar, obwohl
      `min_count_bit_for_found = 37` und `check_remote_controller()`s `data >> 17` bei einem
      37-Bit-Wert nur 20 Bit Serial liefert — die drei führenden Nullen im Kommentar sind reines
      40-Bit-Padding, kein echtes Datenfeld. Im Port durch eine auf 37 Bit korrigierte Tabelle
      ersetzt.
- [x] **Fund 2** (keine eigenständige Fehlfunktion, wie bei Nord ICE bereits dokumentiert und
      hier gegengeprüft statt übersehen): dasselbe geteilte GAP-Timing dient als Rahmenende und
      Scharfschalt-Signal für den nächsten Rahmen — bei direkt aneinandergereihten
      Wiederholungen wird dadurch genau jede zweite tatsächlich dekodiert
      (`found_count == ceil(repeat/2)`, per Host-Repro bestätigt). Unverändertes
      Original-Verhalten, konsistent mit der bereits bei Nord ICE getroffenen Einordnung.
- [x] Host-Repro (sechs reale Beispielwerte gegen `check_remote_controller()`, inkl. Stop-Bit-
      Gegenprobe; Einzelrahmen- und Wiederholungs-Encode→Decode-Rundlauf über 20 000 zufällige
      37-Bit-Werte mit 1–5 Wiederholungen, `found_count == ceil(repeat/2)` bestätigt; zwei
      Negativfälle für zu kurzen End-GAP bzw. verstümmeltes Bit-Timing) unter `gcc -std=c11
      -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      verifiziert: alle vier Szenarien bestanden, identischer Output beider
      Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `firmware-mining/repro-treadmill37-subghz-port/repro.c`.
- [x] Vier Patches (`treadmill37_c`/`_h`, `protocol_items_c`/`_h`) — gegen eine frische,
      unabhängige `mntm-012`-Zweitkopie mit `git apply --check` geprüft, real angewendet,
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch. Zusätzlich gegen vier weitere
      Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift-, KeyFinder- bzw.
      Nord-ICE-`protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge konfliktfrei, die
      anderen drei erzeugen denselben bereits bei Jarolift/Nord-ICE/KeyFinder dokumentierten
      Kontext-Versatz am Dateiende (alle vier hängen ihren Eintrag an dieselbe Anker-Zeile an).
- [x] Details, Kompatibilitätsprüfung, beide Funde, Host-Repro, Risiko, Port-Skizze,
      Testhinweis in `firmware-mining/treadmill37-subghz-port.md`.
- [ ] Aus demselben `unlshd-093`-Vergleich bleiben zwei weitere, noch nicht geprüfte Kandidaten
      offen: `allstar_firefly`, `ditec_gol4` (beide ohne Kryptografie; `ditec_gol4.c` mit
      675 Zeilen deutlich umfangreicher).
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden (Reihenfolge
      gegenüber Jarolift/KeyFinder/Nord-ICE beachten, siehe oben), mit `ufbt`/`fbt
      firmware_all` bauen, am Gerät SubGHz → 433,92 MHz AM650 gegen einen echten
      Treadmill37-/QH-433-Sender empfangen und Serial/Btn gegenprüfen; gezielt das
      Repeat-Verhalten nachvollziehen (Signal mit `Repeat` > 1 senden, gegenprüfen dass nur
      jede zweite Wiederholung einen eigenen Treffer auslöst).

## App-Verbesserung (heltec-ukfe-rx — 868-FSK- UND ESP-NOW-Anti-Replay-Fenster, Neustart-Fix)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Treadmill37),
      davor Ideen-Recherche (CAN Transceiver) und App-Verbesserung (g4meover-wifi-relay,
      Antwort-Pfad) — App-Verbesserung für diese Sitzung entsprechend wieder an der Reihe.
      Git-Identität vor dem ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Unter den eigenen, per GitHub öffentlich erreichbaren App-Repos `heltec-ukfe-rx`
      gezielt gewählt: einziges Gerät im Katalog mit bislang nur einem einzigen erfassten
      Fund (`wifi-recon-probe-ssid-tag-oob-read-fix`), zusätzlich aus der Sitzung vom
      2026-10-01 ausdrücklich als „Kandidat für eine künftige App-Verbesserungs-Sitzung"
      vermerkt (868-Radio-Empfangspfad `lastCounter`, von jenem Fund damals nicht
      betroffen). `g4meover-magspoof` und `ukfe-esp-now-mesh` sind private Repos, mit der
      aktuellen GitHub-Zugriffsbreite dieser Sitzung nicht klonbar — für diese Sitzung
      übersprungen statt angenommen.
- [x] Frischen, unabhängigen Klon von `G4MEOVER18/heltec-ukfe-rx` (`main`, Commit `23e1f40`)
      angelegt, alle Quelldateien vollständig gelesen (`main.cpp`, `ukfe_rf.c`/`.h`,
      `wifi_attack.cpp`/`.h`, `wifi_recon.cpp`/`.h`, `evil_portal.cpp`/`.h`,
      `ble_spam.cpp`/`.h`, `ducky.h`, `lorawan_link.cpp`/`.h`).
- [x] Fund bestätigt: `heltec-ukfe-rx` führt laut eigenem Code-Kommentar bewusst zwei
      getrennte Anti-Replay-Fenster (`lastCounter` für 868-FSK vom Flipper, `enowCounter`
      für ESP-NOW vom WROOM-Relay) — beide reine RAM-Felder, Startwert `0`, kein
      `Preferences`/NVS-Zugriff im gesamten Baum. Derselbe Fund wie bereits in
      `usb-army-penetrator`/`heltec-v4-hub`/`lilygo-ukfe-rx`/`g4meover-wifi-relay`
      dokumentiert, hier aber zweifach im selben Gerät: der 868-Pfad betrifft den Hub-
      Broadcast an die gesamte Satelliten-Flotte, der ESP-NOW-Pfad die direkte
      Befehlsausführung (`act()`) am Gerät selbst.
- [x] Dieselbe bereits viermal verifizierte Reserve-mit-Sicherheitsspanne-Logik
      (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`) ein fünftes Mal
      angewendet, hier zweifach instanziiert (ein `Preferences`-Namensraum `g4meoverHtk`,
      zwei unabhängige Keys `rf868Ctr`/`rfEnowCtr`).
- [x] Host-Repro (je 50 gültig signierte `WifiDeauth`-Frames pro Fenster, Mitschnitt +
      Neustart-Replay vor/nach Fix; zusätzliches Szenario C: identischer Counter-Wert in
      beiden Fenstern gleichzeitig akzeptiert, kein Cross-Window-Lockout) unter `gcc
      -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` verifiziert: alle Szenarien bestanden, identischer
      Output beider Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine
      Compiler-Warnungen — in
      `app-improvements/heltec-ukfe-rx/repro-rf-counter-replay-window-reset/repro.c`.
- [x] Patch (`main_cpp.patch`) gegen eine frische, unabhängige `heltec-ukfe-rx`-Zweitkopie
      mit `git apply --check` geprüft, real angewendet, byte-für-byte mit der
      Arbeitskopie abgeglichen — identisch. In
      `app-improvements/heltec-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix/`.
- [x] Details, Fund, Nutzen, Risiko, Testhinweis in
      `app-improvements/heltec-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md`.
- [ ] Lokal zu tun: Patch anwenden, PlatformIO-Build für `heltec-ukfe-rx`, auf die
      Hardware flashen. Funktionstest wie im Testhinweis der `.md`-Datei beschrieben: je
      einen Befehl über 868 (Flipper) und über ESP-NOW (WROOM-Relay) senden, Gerät per
      Reset-Taste neu starten, beide zuvor mitgeschnittenen Frames erneut aussenden — vor
      dem Fix löst das Gerät beide erneut aus, nach dem Fix bleibt die Reaktion in beiden
      Fällen aus.
- [ ] Der restliche, in dieser Sitzung mitgelesene Code (`wifi_attack.cpp/.h`,
      `evil_portal.cpp/.h`, `ble_spam.cpp/.h`, `ducky.h`, `lorawan_link.cpp/.h`) ohne
      eigenständigen neuen Fund — Kandidat für eine künftige Sitzung.
- [ ] `g4meover-magspoof` und `ukfe-esp-now-mesh` bleiben mit der aktuellen
      GitHub-Zugriffsbreite dieser Sitzung nicht erreichbar — für eine künftige Sitzung mit
      breiterem Zugriff zurückgestellt, kein Datenverlust (beide bereits mehrfach in
      früheren Sitzungen mit damaligem Zugriff geprüft/gepatcht).
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES-Rest, PSA-Bruteforce-Krypto-Konstanten, vier offene
      CAN-Bus-Kandidaten, `sanitize_filename()`-Zeichenbereichs-Nebenfund, zwei offene
      `unlshd-093`-SubGHz-Kandidaten `allstar_firefly`/`ditec_gol4`) bleiben unverändert
      offen.

## Firmware-Feature-Mining (Fortsetzung 31)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (heltec-ukfe-rx), davor
      Firmware-Feature-Mining (Treadmill37) und Ideen-Recherche (CAN Transceiver) —
      Firmware-Feature-Mining für diese Sitzung entsprechend wieder an der Reihe. Repo-Zustand
      zu Sitzungsbeginn geprüft: `HEAD` stand erneut detached (derselbe wiederkehrende
      Checkout-Artefakt-Befund wie in mehreren Vorsitzungen, kein Datenverlust), lokaler
      `main`-Zeiger lag 20 Commits hinter `origin/main` zurück — über `git checkout main` +
      `git merge --ff-only origin/main` behoben (reiner Fast-Forward, Arbeitsverzeichnis war
      sauber, kein Stash nötig). Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der seit der Beninca-ARC-Sitzung offenen `unlshd-093`-Restliste (`allstar_firefly`,
      `ditec_gol4` — `jarolift`/`keyfinder`/`nord_ice`/`treadmill37` inzwischen aufgegriffen)
      **`allstar_firefly.c`** gewählt: letzter verbleibender Kandidat ohne Kryptografie
      (`ditec_gol4` bleibt mit 675 Zeilen als einziger offen). Anders als die vier vorherigen
      Ports kein Serial+Zähler/Taste-Layout, sondern ein 9-stelliger trinärer DIP-Schaltercode
      (`+`/`0`/`-`, je zwei Bit) für eine Supertex-ED-9-Torantrieb-Fernbedienung.
- [x] Kompatibilitätsprüfung gegen `mntm-012`: jedes von `allstar_firefly.c` verwendete
      Kern-Symbol byteidentisch vorhanden. Einzig fehlender Teil wie bei den vier vorherigen
      Ports: `common.c`/`.h` (inkl. der darin definierten `SUBGHZ_ASSERT_*`-Sanity-Makros, beide
      ersatzlos entfallen) — folgenlos, da Allstar Firefly weder Keystore noch Zähler-/
      Tasten-Override benutzt. Zusätzlich gegengeprüft: die bei mir entfernten
      `SubGhzProtocolFlag_ReversRB2/_Alarms/_Sensors/_Princeton/_NiceFlorS`-Werte (jetzt eigenes
      `SubGhzProtocolFilter`-Enum) werden von `allstar_firefly.c` keiner benutzt.
- [x] **Fund** (keine eigenständige Fehlfunktion, wie bei Nord ICE/Treadmill37 bereits
      dokumentiert und hier für diese Implementierung — eigene Konstanten `te_short=600`,
      `te_long=4000`, GAP `te_short*50=30000 µs` — gegengeprüft statt übersehen): dasselbe
      geteilte GAP-Timing dient als Rahmenende und Scharfschalt-Signal für den nächsten Rahmen —
      bei direkt aneinandergereihten Wiederholungen (Encoder-Default `repeat=5`) wird dadurch
      genau jede zweite tatsächlich dekodiert (`found_count == ceil(repeat/2)`, per Host-Repro
      über 20 000 Zufallswerte bestätigt). Unverändertes Original-Verhalten, konsistent mit der
      bereits bei Nord ICE/Treadmill37 getroffenen Einordnung.
- [x] Host-Repro (Hand-Dekodierung des Unleashed-Header-eigenen DIP-Beispiels `+--000++-` auf
      0x30ABC zurückgeführt und gegen dieselbe 2-Bit-Gruppen-Zuordnung bestätigt, die
      `get_string()` benutzt; Einzelrahmen- und Wiederholungs-Encode→Decode-Rundlauf über
      20 000 zufällige 18-Bit-Werte mit 1–5 Wiederholungen, `found_count == ceil(repeat/2)`
      bestätigt; zwei Negativfälle für zu kurzen End-GAP bzw. verstümmeltes Bit-Timing) unter
      `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` verifiziert: alle vier Szenarien bestanden, identischer
      Output beider Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen —
      in `firmware-mining/repro-allstar-firefly-subghz-port/repro.c`.
- [x] Vier Patches (`allstar_firefly_c`/`_h`, `protocol_items_c`/`_h`) — gegen eine frische,
      unabhängige `mntm-012`-Zweitkopie mit `git apply --check` geprüft, real angewendet,
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch. Zusätzlich gegen fünf weitere
      Zweitkopien mit je einzeln bereits angewendetem Beninca-ARC-, Jarolift-, KeyFinder-,
      Nord-ICE- bzw. Treadmill37-`protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge
      konfliktfrei, die anderen vier erzeugen denselben bereits dokumentierten Kontext-Versatz
      am Dateiende (alle fünf hängen ihren Eintrag an dieselbe Anker-Zeile an).
- [x] Details, Kompatibilitätsprüfung, Fund, Host-Repro, Risiko, Port-Skizze, Testhinweis in
      `firmware-mining/allstar-firefly-subghz-port.md`.
- [ ] Aus demselben `unlshd-093`-Vergleich bleibt ein letzter, noch nicht geprüfter Kandidat
      offen: `ditec_gol4` (ohne Kryptografie, 675 Zeilen).
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden (Reihenfolge
      gegenüber Jarolift/KeyFinder/Nord-ICE/Treadmill37 beachten, siehe oben), mit `ufbt`/`fbt
      firmware_all` bauen, am Gerät SubGHz → 318 MHz AM650 gegen einen echten
      Allstar-Firefly-318ALD31K-Sender empfangen und die drei DIP-Zeilen gegen die physische
      Schalterstellung prüfen; gezielt das Repeat-Verhalten nachvollziehen (Signal mit
      `Repeat` > 1 senden, gegenprüfen dass nur jede zweite Wiederholung einen eigenen Treffer
      auslöst).

## Ideen-Recherche (Fortsetzung 17)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Allstar Firefly),
      davor App-Verbesserung (heltec-ukfe-rx) und Firmware-Feature-Mining (Treadmill37) —
      Ideen-Recherche war zuletzt vor drei Sitzungen (CAN Transceiver) an der Reihe,
      entsprechend für diese Sitzung gewählt. Repo-Zustand zu Sitzungsbeginn geprüft: `HEAD`
      stand detached auf einem älteren Commit, lokaler `main`-Zeiger lag 21 Commits hinter
      `origin/main` zurück — nach `git fetch` stellte sich heraus, dass `origin/main` den
      detached `HEAD`-Commit bereits enthielt (keine Divergenz, reiner veralteter lokaler Ref).
      Über `git checkout main` + `git merge --ff-only origin/main` behoben (reiner
      Fast-Forward, Arbeitsverzeichnis war sauber, kein Stash nötig). Git-Identität vor dem
      ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `ideas/can-transceiver-api-status.md` und den CAN-Bus-Abschnitt in
      `ideas/pentest-app-katalog.md` durchgesehen: aus der dort seit der CAN-Tools-Sitzung
      offenen Restliste (`can_bus_attack`, `can_commander`, `canbus` — `can_transceiver`
      inzwischen aufgegriffen) **CAN Bus Attack** (`jorgepnaranjo25/CAN-Bus-Attack`) als
      kleinste der drei noch offenen Apps gewählt (4349 Zeilen gegenüber 9042/`canbus` und
      15165/`can_commander`).
- [x] Frischen, unabhängigen Sparse-Checkout von `RogueMaster/flipperzero-firmware-wPlugins`
      (Branch `420`) angelegt (nur `applications/external/{can_bus_attack,can_commander,
      canbus}/`, zum Größenvergleich aller drei verbleibenden Kandidaten), `can_bus_attack`
      vollständig gelesen: `app_user.c`/`.h`, alle neun `scenes/`-Dateien,
      `draw_functions/drawWidgets.c`, `libraries/{Spi_lib,mcp_can_2515,pid_library,
      uds_library}.c`/`.h`.
- [x] API-87-Kompatibilität gegen eine frische, unabhängige Zweitkopie von
      `Next-Flip/Momentum-Firmware` (Tag `mntm-012`) geprüft: eigener SPI-Bus-Handle auf
      `furi_hal_spi_bus_r` (statt des höherwertigen `furi_hal_spi_bus_handle_external`-Wrappers
      wie bei `can_tools`/`can_transceiver`) samt `GpioAltFn5SPI1`, `gpio_ext_pa4/_pa6/_pa7/
      _pb3`, `LL_SPI_Init/_Enable/_Disable/_SetRxFIFOThreshold`,
      `FuriHalSpiBusHandleEvent{Init,Deinit,Activate,Deactivate}` — byteidentisch vorhanden.
      `furi_hal_light_set/_blink_start/_blink_stop`, `furi_hal_gpio_read()`, Standard-GUI-/
      Storage-APIs ebenfalls byteidentisch. Kein Port-Hindernis.
- [x] **Fund**: `read_frame()` (`libraries/mcp_can_2515.c`) maskiert das empfangene
      DLC-Steuerbyte nur mit `MCP_DLC_MASK` (`0x0F`, liefert 0–15), ohne das Ergebnis danach auf
      die deklarierte Puffergröße `MAX_LEN` (`8`) zu begrenzen — anders als die etablierte
      Referenzbibliothek, von der dieser Treiber abstammt (`coryjfowler`/Seeed `MCP_CAN`, dort
      `if (dlc > 8) dlc = 8;`). Für ein manipuliertes/fehlerhaftes DLC-Feld 9–15 (das
      CAN-Protokoll begrenzt das 4-Bit-Feld hardwareseitig nicht auf 8) schreibt die
      anschließende Leseschleife über das nur 8 Byte große `CANFRAME.buffer` hinaus — und zwar
      in ein lokales Stack-Objekt, da `read_frame()` an allen drei Aufrufstellen
      (`SnifferOption.c:194`, `pid_library.c:86/109`, `uds_library.c:55`) mit einem lokalen
      `CANFRAME` aufgerufen wird. Dieselbe Datei liegt laut `diff` byteidentisch auch im noch
      offenen `canbus`-Kandidaten desselben Clusters vor.
- [x] Host-Repro (Regressionsfall DLC=8; Fundfall 1 DLC=9 — bereits außerhalb des Array-Vertrags,
      auf dem Host-ABI aber noch in der eigenen Tail-Padding des 16-Byte-`CANFRAME`-Objekts,
      daher explizit als Grenzfall dokumentiert statt übersehen; Fundfall 2 DLC=10 — kleinster
      tatsächlich von ASan erkannter Fall; Fundfall 3 DLC=15 — maximaler Fall; vollständiger
      Sweep über alle 16 DLC-Nibble-Werte) unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: für DLC 0–8
      verhalten sich ALT/NEU in allen 9 Fällen identisch und sicher, für DLC 9–15 liefert NEU in
      allen 7 Fällen das erwartete, begrenzte Ergebnis, ALT löst in 6 von 7 Fällen (DLC 10–15)
      einen von ASan erkannten `stack-buffer-overflow` aus (DLC=9 fällt wie beschrieben in die
      Tail-Padding), identischer Output beider Optimierungsstufen, keine Compiler-Warnungen —
      in `firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c`.
- [x] Patch (additive Begrenzungszeile `if(frame->data_lenght > MAX_LEN) frame->data_lenght =
      MAX_LEN;` direkt nach der DLC-Maskierung, nach dem Muster der Referenzbibliothek) — per
      `git apply --check` gegen eine frische, unabhängige Zweitkopie des RogueMaster-Bündels
      (Branch `420`, nur `applications/external/can_bus_attack/`) verifiziert, real angewendet,
      byte-für-byte mit der Arbeitskopie abgeglichen — identisch. In
      `firmware-mining/can-bus-attack-app-port/mcp_can_2515_c.patch`.
- [x] Details, Nutzen, API-87-Kompatibilität, Fund, Host-Repro, Risiko, Port-Skizze, Testhinweis
      in `firmware-mining/can-bus-attack-app-port.md`. Kuratiert in
      `ideas/can-bus-attack-api-status.md`. `ideas/pentest-app-katalog.md` (CAN-Bus-Tabelle und
      Log-Eintrag unter "Nächste Recherche-Aufträge") aktualisiert.
- [ ] Lokal zu tun: `can_bus_attack` (mit Patch) gegen die eigene `mntm-012`-Basis bauen, mit
      MCP2515-Hardware am Erweiterungsport verkabeln (`CS=PA4`, `SCK=PB3`, `MOSI=PA7`,
      `MISO=PA6`), Regressionstest mit gültigen Rahmen (DLC 0–8), danach den Fundnachvollzug aus
      `firmware-mining/can-bus-attack-app-port.md` (Rahmen mit manipuliertem DLC-Feld 9–15 über
      einen zweiten CAN-Knoten senden) durchführen.
- [ ] Aus demselben CAN-Bus-Cluster bleiben `can_commander` und `canbus` noch nicht einzeln
      geprüft — `canbus` dürfte beim künftigen Prüfen denselben `mcp_can_2515.c`-Fund
      reproduzieren.
- [ ] `uds_library.c`/`pid_library.c` (in `can_bus_attack`) sind aktuell ohne eigene Szene
      angebunden — eigenständiger, kleinerer Rückstellpunkt für eine künftige Sitzung.

## App-Verbesserung (heltec-ukfe-rx — Ducky-Interpreter, Rest einer ueberlangen Zeile)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (CAN Bus Attack), davor
      Firmware-Feature-Mining (Allstar Firefly) und App-Verbesserung (heltec-ukfe-rx,
      868-/ESP-NOW-Anti-Replay) — App-Verbesserung für diese Sitzung entsprechend wieder an
      der Reihe. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der letzten `heltec-ukfe-rx`-Sitzung ausdrücklich zurückgestellten Restliste
      (`wifi_attack.cpp/.h`, `evil_portal.cpp/.h`, `ble_spam.cpp/.h`, `ducky.h`,
      `lorawan_link.cpp/.h` — ohne eigenständigen Fund gelesen, aber noch nicht gezielt
      durchsucht) aufgegriffen. Frischen, unabhängigen Klon von `G4MEOVER18/heltec-ukfe-rx`
      (`main`, Commit `23e1f40`, unverändert gegenüber der letzten Sitzung) angelegt, alle
      fünf Dateien erneut vollständig gelesen.
- [x] **Fund** in `src/ducky.h::ducky_run()`: die Zeilenlese-Schleife begrenzt den
      `line[220]`-Puffer korrekt (kein Speicherfehler), bricht bei einer über 219 Zeichen
      langen physischen Skriptzeile aber nur am Pufferlimit ab, ohne den Rest dieser Zeile
      zu überspringen — die nächste Außenschleifen-Iteration liest diesen Rest als
      eigenständige NEUE Kommandozeile. Trifft der Rest zufällig exakt einen von
      `ducky_keyname()`/`ducky_modkey()` erkannten Namen (`ENTER`, ein Modifier-Kombo wie
      `GUI-r`, ...), wird er tatsächlich als Tastendruck/-kombination ausgeführt statt als
      reiner Text getippt zu werden — realistisch über den gestreamten Pfad `0x52 HidStream`
      (`duckyStream[2048]`), der beliebig lange `STRING`/`STRINGLN`-Zeilen zulässt.
      `ducky.h` byteidentisch auch in `lilygo-ukfe-rx` (`diff` gegen frischen, unabhängigen
      Klon bestätigt), dort also derselbe Fund. `heltec-v4-hub` hat keinen HID-/Ducky-Pfad;
      `usb-army-penetrator` hat eine andere, eigenständige Ducky-Implementierung
      (`Attacks/Ducky/DuckyPayload.cpp/.h`) — ungeprüft, eigener Rückstellpunkt.
- [x] Fix (additiv, drei Zeilen): wird der Puffer exakt voll, OHNE dass `'\n'` erreicht
      wurde, den Rest der echten Zeile bis zum nächsten `'\n'` verwerfen statt ihn in der
      nächsten Iteration als neue Zeile zu lesen. Patch gegen zwei frische, unabhängige
      Zweitkopien (`heltec-ukfe-rx` UND `lilygo-ukfe-rx`) mit `git apply --check` verifiziert,
      real angewendet, jeweils byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (`Arduino.h`/`USBHIDKeyboard.h` als schlanke Host-Mocks — `String` auf
      `std::string`, `press()`/`write()`/`print()`/`releaseAll()` als Log-Aufzeichnung statt
      echtem USB-HID-Traffic; `ducky_orig.h`/`ducky_fixed.h` sind unverändert übernommene
      Kopien aus `src/ducky.h`, je in eigenem Namensraum eingebunden): fünf Szenarien —
      normales kurzes Skript (Regression), Zeile exakt an der Puffergrenze ohne Überlauf
      (bestätigt 0 Iterationen der neuen Skip-Schleife im Grenzfall), Zeilenrest `ENTER`
      (ALT injiziert einen zusätzlichen `WRITE`, NEU nicht), Zeilenrest `GUI-r` (ALT presst
      zusätzlich GUI+`r`, NEU nicht), zwei direkt aufeinanderfolgende überlange Zeilen (ALT
      zwei zusätzliche Tastendrücke, NEU synchronisiert sich pro Zeile korrekt neu) — unter
      `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: alle acht Prüfungen bestanden, identische Ausgabe beider
      Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
      `app-improvements/heltec-ukfe-rx/repro-ducky-overlong-line-tail-misparsed-as-command/repro.cpp`.
- [x] Details, Fund, Nutzen, Risiko, Testhinweis in
      `app-improvements/heltec-ukfe-rx/ducky-overlong-line-tail-misparsed-as-command-fix.md`.
      Patch in
      `app-improvements/heltec-ukfe-rx/ducky-overlong-line-tail-misparsed-as-command-fix/ducky_h.patch`.
- [ ] Lokal zu tun: Patch in `heltec-ukfe-rx` UND `lilygo-ukfe-rx` anwenden, PlatformIO-Build,
      auf Hardware flashen. Funktionstest wie im Testhinweis der `.md`-Datei beschrieben: über
      `0x52 HidStream` ein Skript mit einer `STRING`-Zeile von mehr als 219 Zeichen senden,
      deren abgeschnittener Rest absichtlich `ENTER` oder `GUI-r` lautet — vor dem Fix löst das
      Gerät den zusätzlichen Tastendruck/die Kombination tatsächlich am Zielrechner aus, danach
      nicht mehr.
- [ ] `wifi_attack.cpp/.h`, `evil_portal.cpp/.h`, `ble_spam.cpp/.h`, `lorawan_link.cpp/.h`
      (in dieser Sitzung erneut mitgelesen) weiterhin ohne eigenständigen neuen Fund.
- [ ] `usb-army-penetrator`s eigenständige Ducky-Implementierung (`Attacks/Ducky/
      DuckyPayload.cpp/.h`) auf denselben Fehlerklasse-Fall ungeprüft — eigener
      Rückstellpunkt für eine künftige Sitzung.
- [ ] `g4meover-magspoof` und `ukfe-esp-now-mesh` bleiben mit der aktuellen
      GitHub-Zugriffsbreite dieser Sitzung nicht erreichbar.
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES-Rest, PSA-Bruteforce-Krypto-Konstanten, vier offene
      CAN-Bus-Kandidaten, `sanitize_filename()`-Zeichenbereichs-Nebenfund, zwei offene
      `unlshd-093`-SubGHz-Kandidaten `allstar_firefly`/`ditec_gol4`) bleiben unverändert
      offen.

## Firmware-Feature-Mining (Fortsetzung 32)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (heltec-ukfe-rx), davor
      Ideen-Recherche (CAN Bus Attack) und Firmware-Feature-Mining (Allstar Firefly) —
      Firmware-Feature-Mining für diese Sitzung entsprechend wieder an der Reihe. Repo-Zustand
      zu Sitzungsbeginn geprüft: `HEAD` stand detached exakt auf `origin/main`s Commit (reiner
      veralteter lokaler `main`-Ref, 24 Commits zurück, kein Datenverlust) — über
      `git checkout main` + `git merge --ff-only origin/main` behoben. Git-Identität vor dem
      ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der seit der Beninca-ARC-Sitzung offenen `unlshd-093`-Restliste (`allstar_firefly`,
      `ditec_gol4` — `jarolift`/`keyfinder`/`nord_ice`/`treadmill37`/`allstar_firefly` inzwischen
      aufgegriffen) **`ditec_gol4.c`** gewählt: letzter verbleibender Kandidat (675 Zeilen).
      Anders als die fünf vorherigen Ports ein eigenes, proprietäres Rolling-Code-Schema
      (Bit-Rotation/-Spiegelung, feste XOR-Maskierung, invertierbarer 8-Bit-LCG) statt KeeLoq
      oder einer statischen Fernbedienung.
- [x] Kompatibilitätsprüfung gegen `mntm-012`: jedes verwendete Kern-Symbol byteidentisch
      vorhanden. `common.c`/`.h` wie bei den fünf vorherigen Ports nicht vorhanden, ersetzt durch
      eigene `_alloc/_free/_reset/_stop/_yield`. Zusätzlich fehlt `subghz_block_generic_global`
      (Zähler-/Tasten-Override-Infrastruktur für die Signal-Editor-UI) — anders als bei den
      fünf vorherigen Ports hier tatsächlich aktiv aufgerufen (nicht nur auskommentiert), nach
      demselben beim Jarolift-Port etablierten Muster ersetzt (immer der Standard-Zähler-Pfad,
      Editor-UI-Push in `get_string()` ersatzlos entfernt).
- [x] **Kryptografische Prüfung** (eigenständiges Schema, keine externe Primitive): LCG-Inverse
      einzeln gegengerechnet; voller Rundlauf `gol4_rolling_encode()`→`_decode()` über 50 000
      zufällige Rahmen bit-exakt (Host-Repro Szenario A); `raw[2]==0→0xF0`-Sonderfall
      (Prog-Modus) einzeln verifiziert (Szenario A2); vier reale Beispiele aus dem
      Decoder-eigenen Kommentar dekodieren zur selben Seriennummer mit korrektem Tastennibble
      (Szenario B).
- [x] **Fund 1**: Decoder-Zustandsmaschine braucht mindestens zwei Wiederholungen — eine
      einzelne Übertragung (`repeat=1`) wird **nie** dekodiert (`found_count==floor(repeat/2)`,
      schlechter als die bei Nord ICE/Treadmill37/Allstar Firefly dokumentierte
      `ceil(repeat/2)`-Klasse, weil das Rahmenende erst verzögert über das erste Datenbit des
      *nächsten* Rahmens erkannt wird, nicht direkt über dessen eigenes GAP). Empirisch für
      `repeat`=1..6 nachgemessen (Host-Repro Szenario C). Unverändertes Original-Verhalten, nur
      dokumentiert, nicht verändert.
- [x] **Fund 2**: `serial_to_display()`s `(s[0]<<24)|...` überläuft für jedes `s[0]>=0x80` einen
      signed `int` (Undefined Behavior) — nicht theoretisch, alle vier realen Beispiele im
      Decoder-Kommentar haben `s[0]==0xCC`. Unter UBSan reproduziert (Host-Repro Szenario F,
      isoliert per `fork()`). Fix: jedes Byte vor dem Schieben nach `uint32_t` gecastet,
      mathematisch ergebnisgleich — in meinen Port übernommen.
- [x] Host-Repro (sechs Szenarien A/A2/B/C/D/E plus F, siehe oben) unter
      `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined`: alle bestanden, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen — in
      `firmware-mining/repro-ditec-gol4-subghz-port/repro.c`.
- [x] Vier Patches (`ditec_gol4_c`/`_h`, `protocol_items_c`/`_h`) gegen eine frische,
      unabhängige `mntm-012`-Zweitkopie mit `git apply --check` geprüft, real angewendet,
      byte-für-byte abgeglichen — identisch. Zusätzlich gegen fünf weitere Zweitkopien mit je
      einzeln bereits angewendetem Beninca-ARC-, Jarolift-, KeyFinder-, Nord-ICE- bzw.
      Treadmill37-`protocol_items`-Patch geprüft: Beninca-ARC-Reihenfolge konfliktfrei, die
      anderen vier erzeugen den bereits dokumentierten Kontext-Versatz am Dateiende.
- [x] Details, Kompatibilitätsprüfung, kryptografische Prüfung, beide Funde, Host-Repro, Risiko,
      Port-Skizze, Testhinweis in `firmware-mining/ditec-gol4-subghz-port.md`.
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden (Reihenfolge
      gegenüber Jarolift/KeyFinder/Nord-ICE/Treadmill37/Allstar-Firefly beachten, siehe `.md`),
      mit `ufbt`/`fbt firmware_all` bauen, am Gerät SubGHz → 433 MHz AM650 gegen einen echten
      Ditec-GOL4-Handsender empfangen; gezielt Fund 1 nachvollziehen (eigenes `.sub` mit
      `Repeat=1` erneut senden, gegenprüfen dass kein Treffer erscheint) und Fund 2
      (Seriennummer einer `0xCC...`-Fernbedienung korrekt angezeigt).
- [ ] Die `unlshd-093`-Restliste (`allstar_firefly`/`ditec_gol4`/`jarolift`/`keyfinder`/
      `nord_ice`/`treadmill37`) ist damit vollständig aufgegriffen — kein weiterer offener
      Kandidat aus diesem Cluster.
- [ ] "Add Manually"-UI-Anbindung für `ditec_gol4_create_data()` (Serial/Taste/Zähler ohne
      eigenen Mitschnitt anlegen) bewusst nicht Teil dieses Ports — eigenständiger,
      optionaler Rückstellpunkt für eine künftige Sitzung, falls gewünscht.
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES-Rest, PSA-Bruteforce-Krypto-Konstanten, vier offene
      CAN-Bus-Kandidaten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s eigenständige Ducky-Implementierung auf dieselbe Fehlerklasse
      ungeprüft, `g4meover-magspoof`/`ukfe-esp-now-mesh` mit der aktuellen
      GitHub-Zugriffsbreite nicht erreichbar) bleiben unverändert offen.

## Ideen-Recherche (Fortsetzung 18)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Ditec GOL4), davor
      App-Verbesserung (heltec-ukfe-rx) und Ideen-Recherche (CAN Bus Attack) — Ideen-Recherche
      für diese Sitzung entsprechend wieder an der Reihe (letzte zwei Sitzungen waren Firmware/
      App, die dritte Kategorie fehlte). Repo-Zustand zu Sitzungsbeginn geprüft: `HEAD` stand
      detached, lokaler `main`-Zeiger lag 25 Commits hinter `origin/main` zurück — nach
      `git fetch origin main` stellte sich heraus, dass `origin/main` exakt dem detached
      `HEAD`-Commit entspricht (keine Divergenz, reiner veralteter lokaler `main`-Ref). Über
      `git checkout main` + `git reset --hard origin/main` behoben (reiner Fast-Forward,
      Arbeitsverzeichnis war sauber, kein Stash nötig). Git-Identität vor dem ersten Commit
      gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `ideas/pentest-app-katalog.md` (CAN-Bus-Abschnitt) durchgesehen: aus der seit der
      CAN-Tools-Sitzung offenen Restliste (`can_bus_attack`, `can_commander`, `can_transceiver`,
      `canbus` — die ersten drei inzwischen aufgegriffen) **CANBUS**
      (`ElectronicCats/flipper-MCP2515-CANBUS`) als letzten Kandidaten ohne eigenes
      GPIO-Zusatzboard gewählt (`can_commander` benötigt laut Quellcode-Kommentar ein eigenes
      "CAN Commander GPIO Board", bleibt daher offen).
- [x] Frischen, unabhängigen Klon von `ElectronicCats/flipper-MCP2515-CANBUS` angelegt,
      vollständig gelesen: App-Kern (`app_user.c`/`.h`), alle 21 `scenes/`-Dateien (inkl.
      `Obd2Options/`/`UDSOptions/`-Unterordner), fünf eigene `fap_private_libs`-Bibliotheken
      (`lib/frame_can`, `lib/files_scaner`, `lib/SLCAN_serial`, `lib/log_exporter`,
      `lib/hex_converter`), `draw_functions/drawWidgets.c`, `libraries/`
      (`Spi_lib`/`mcp_can_2515`/`frame_queue`/`pid_library`/`uds_library`) — 8968 Zeilen,
      größte der vier geprüften MCP2515-Apps dieses Clusters. Zusätzlich frischen,
      unabhängigen Zweitklon von `jorgepnaranjo25/CAN-Bus-Attack` angelegt und per `diff`
      gegen `canbus` verglichen.
- [x] API-87-Kompatibilität gegen eine frische, unabhängige Zweitkopie von
      `Next-Flip/Momentum-Firmware` (Tag `mntm-012`) geprüft: eigener SPI-Bus-Handle auf
      `furi_hal_spi_bus_r` (dasselbe Eigenbau-Muster wie bei `can_bus_attack`, `libraries/
      Spi_lib.c` zwischen beiden Apps byteidentisch) samt `GpioAltFn5SPI1`/`gpio_ext_*`/
      `LL_SPI_*` byteidentisch gegen den eigenen `mntm-012`-Stand. Standard-GUI-/Storage-/
      Stream-APIs sowie `furi_hal_serial_*` (SLCAN-UART-Pfad) ebenfalls byteidentisch. Kein
      Port-Hindernis.
- [x] `diff` zwischen den beiden frischen Klonen bestätigt: `libraries/mcp_can_2515.c`/`.h` und
      `libraries/uds_library.c` sind byteidentisch zwischen `canbus` und dem bereits geprüften
      `can_bus_attack`, `libraries/pid_library.c` unterscheidet sich nur durch eine triviale,
      funktionsgleiche Variableninitialisierung (`char* first = NULL;` vs. `char* first;`).
      Damit reproduziert sich der dort dokumentierte DLC-Überlauf-Fund (`read_frame()` maskiert
      das empfangene DLC-Steuerbyte nur mit `MCP_DLC_MASK` (0x0F, liefert 0–15), ohne auf
      `MAX_LEN` (8) zu begrenzen) unverändert.
- [x] **Neuer Befund dieser Sitzung**: anders als bei `can_bus_attack`, wo `pid_library.c`/
      `uds_library.c` laut eigenem Menü nie über eine Szene eingebunden waren (toter Code), sind
      beide in `canbus` tatsächlich über eigene Hauptmenüpunkte erreichbar: "UDS Services" →
      `scenes/UDSOptions/*.c` → `uds_library.c`-Hilfsfunktionen → `read_frames_uds()` →
      `read_can_message()` → `read_frame()` (`CANFRAME` bleibt hier wie bei `can_bus_attack`
      lokal auf dem Stack — gleiche Fehlerklasse, neuer erreichbarer Pfad); "Scanner OBD2" →
      `scenes/Obd2Options/manualPIDSenderOption.c` → `pid_manual_request()` →
      `read_can_message()`, geschrieben in `obdii->frame_to_received` — ein Inline-Feld der
      `OBDII`-Struktur (`libraries/pid_library.h:86-87`), die selbst Teil der einmalig per
      `malloc()` angelegten Haupt-`App`-Struktur ist. Ein manipuliertes/fehlerhaftes DLC-Feld
      9–15 schreibt hier also über das Ende eines heap-residenten statt eines kurzlebigen
      Stack-Puffers hinaus — ein Heap- statt Stack-Overflow bei diesem einen zusätzlichen Pfad.
- [x] Host-Repro bewusst nicht dupliziert: da `mcp_can_2515.c` byteidentisch ist, deckt der
      bereits bestehende `firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c` (dessen
      eigener Kommentarkopf schon auf `canbus` verweist) die betroffene Funktion verbatim ab.
- [x] Patch 1:1 aus `can_bus_attack` übernommen (`if(frame->data_lenght > MAX_LEN)
      frame->data_lenght = MAX_LEN;`), gegen eine frische, unabhängige `canbus`-Zweitkopie
      (eigene `applications/external/canbus/`-Struktur nachgebaut) per `git apply --check`
      verifiziert, real angewendet; Ergebnis per `diff` byte-für-byte mit der bereits
      gepatchten `can_bus_attack`-Datei abgeglichen — identisch. In
      `firmware-mining/canbus-app-port/mcp_can_2515_c.patch`.
- [x] Details, Nutzen, API-87-Kompatibilität, Fund, Host-Repro-Verweis, Risiko, Port-Skizze,
      Testhinweis in `firmware-mining/canbus-app-port.md`. Kuratiert in
      `ideas/canbus-api-status.md`. `ideas/pentest-app-katalog.md` (CAN-Bus-Tabellenzeile und
      Log-Eintrag unter "Nächste Recherche-Aufträge") aktualisiert.
- [ ] Lokal zu tun: `canbus` (mit Patch) gegen die eigene `mntm-012`-Basis bauen, mit
      MCP2515-Hardware am Erweiterungsport verkabeln (`CS=PA4`, `SCK=PB3`, `MOSI=PA7`,
      `MISO=PA6`), Regressionstest mit gültigen Rahmen (DLC 0–8), danach Fundnachvollzug für
      Sniffing/UDS (Stack-Fall) UND Scanner OBD2 (Heap-Fall) gemäß Testhinweis in
      `firmware-mining/canbus-app-port.md`.
- [ ] Aus demselben CAN-Bus-Cluster bleibt nur noch `can_commander` (15165 Zeilen, eigenes
      GPIO-Board laut Quellcode-Kommentar vorausgesetzt) offen — das Cluster ist damit bis auf
      diesen einen Fremdhardware-Sonderfall vollständig aufgegriffen.
- [ ] `lib/SLCAN_serial`, `lib/files_scaner` (Player/Read-LOG-CSV-Parser), `lib/log_exporter`,
      `lib/hex_converter`, `libraries/frame_queue.c`, `draw_functions/drawWidgets.c` und die
      restlichen Szenen vollständig gelesen, ohne eigenen Fund über die dokumentierte Klasse
      hinaus — insbesondere `frame_extractor.c`s `frame_splitter()` prüft bei einer
      `.csv`-Log-Zeile nur auf "mindestens ein `:`-Trennzeichen vorhanden" statt auf die
      erwartete Feldanzahl, ist aber dank `mlib`s eigenem Index-Clamping in `m_string_mid()`/
      `m_string_right()` speichersicher (schlimmstenfalls stillschweigend falsch/leer geparste
      Zeile, kein Absturz) — eigenständiger, kleinerer Rückstellpunkt für eine künftige, tiefere
      Prüfung.

## App-Verbesserung (usb-army-penetrator — Fortsetzung 2)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (CANBUS), davor
      Firmware-Feature-Mining (Ditec GOL4), davor App-Verbesserung (heltec-ukfe-rx, Ducky) — die
      letzten zwei Sitzungen deckten Ideen/Firmware ab, App-Verbesserung fehlte entsprechend und
      war für diese Sitzung an der Reihe. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der seit der `heltec-ukfe-rx`-Ducky-Sitzung offenen Restliste den dort als eigenständig
      vermerkten Rückstellpunkt aufgegriffen: `usb-army-penetrator`s **eigene**
      Ducky-Implementierung (`src/Attacks/Ducky/DuckyPayload.cpp`/`.h`,
      `src/Attacks/Ducky/Extensions.h`) war dort ausdrücklich als "ungeprüft" vermerkt, weil sie
      nicht dasselbe wiederverwendbare `src/ducky.h` wie `heltec-ukfe-rx`/`lilygo-ukfe-rx`
      nutzt, sondern einen vollständig anderen, auf der externen Bibliothek
      `i-am-shodan/DuckScriptInterpreter` (Pin `23aa3ba87e95f402ccab53beb9ead54602643dd7`,
      `platformio.ini` `lib_deps_core`) basierenden Interpreter.
- [x] Frischen, unabhängigen Klon von `G4MEOVER18/usb-army-penetrator` (`main`, Commit
      `411b153`) UND von `i-am-shodan/DuckScriptInterpreter` (gepinnter Commit) angelegt;
      `DuckyPayload.cpp`/`.h` und `Extensions.h` vollständig gelesen (alle ~35 registrierten
      Extension-Befehle/-Funktionen). Bereits dokumentierter Bug derselben Fehlerklasse
      (ungefangene C++-Ausnahme, kein `try`/`catch` irgendwo im Projekt) in
      `settings-int-uint16-stoi-uncaught-exception-crash-fix.md` als Vorlage für die
      Erreichbarkeits-/Exception-Prüfung genutzt.
- [x] **Fund**: `handleDisplayText()` (Implementierung des `DISPLAY_TEXT <x> <y> <text...>`-
      Befehls) bestimmt die Grenze von `yPosStr` und den Start von `text` über
      Längenarithmetik statt über geprüfte `find()`-Treffer — zwei `std::string::substr()`-
      Aufrufe können dadurch mit einer `pos > size()` aufgerufen werden und werfen
      `std::out_of_range`: einmal wenn `y` komplett fehlt (`DISPLAY_TEXT 10`), einmal wenn nur
      `text` fehlt (`DISPLAY_TEXT 10 20`, kein abschließendes Leerzeichen). `grep -rn "try\|catch"
      src/DuckyParse.cpp` in der externen Bibliothek bestätigt: `DuckyInterpreter::Execute()`
      ruft die Extension-Funktion direkt auf, ohne jede Absicherung — die Ausnahme läuft bis
      `std::terminate()` durch, keine der `platformio.ini`-Umgebungen setzt
      `-fno-exceptions`. Alle übrigen Handler in `Extensions.h`, die ebenfalls
      `str.substr(str.find(' ') + 1)` nutzen, geprüft und als sicher bestätigt (`npos + 1`
      überläuft für `size_t` definiert zu `0`, kein Wurf) — nur `handleDisplayText()` kombiniert
      zwei gefundene Positionen zu einer dritten, ungeprüften.
- [x] Fix: beide `find()`-Treffer vor Verwendung auf `npos` geprüft, bei fehlendem Argument
      derselbe "Bad argument"-Logstil wie `handleUSBMode`/`handleLED` plus `return false`
      (vom Interpreter als `SCRIPT_ERROR` behandelt, bricht die Skriptausführung kontrolliert
      ab statt das Gerät abzureißen). Für jeden wohlgeformten Aufruf rechnerisch identisches
      Ergebnis zu vorher. Patch gegen eine frische, unabhängige Zweitkopie per
      `git apply --check` verifiziert, real angewendet, Diff geprüft — in
      `display-text-oob-substr-crash-fix/extensions_h.patch`.
- [x] Host-Repro (`display-text-oob-substr-crash-fix/repro.cpp`, reine Argumentparsing-Logik,
      kein Arduino/ESP32-Header nötig) unter ASan/UBSan bei `-O0`/`-O1`: beide unvollständigen
      Befehle werfen im ALT-Zustand bestätigt, im NEU-Zustand kontrolliert abgelehnt;
      Regressionsfälle (wohlgeformter Aufruf, expliziter Leer-Text, Variablen statt Literalen
      für `x`/`y`) identisch vorher/nachher. 11 Prüfungen, alle bestanden, identischer Output
      bei beiden Optimierungsstufen, keine Compiler-Warnungen.
- [x] Details, Nutzen, Fund, Patch, Host-Repro, Risiko, Testhinweis in
      `app-improvements/usb-army-penetrator/display-text-oob-substr-crash-fix.md`.
- [ ] Restliche, in dieser Sitzung mitgelesene, aber nicht vertiefte Teile von
      `DuckyPayload.cpp`/`Extensions.h` ohne eigenen Fund über diese eine Fehlerklasse hinaus:
      `setPayloadCmdLine()`s Verwerfen leerer Zeilen beim Zerlegen einer Kommandozeile
      (für DuckyScript folgenlos, da Leerzeilen dort ohnehin No-Ops sind), `handleRawHid()`,
      `handleSetSetting()`, die `AGENT_RUN`/`ESP32M`/`RUN_PAYLOAD`-Dispatch-Handler — jeweils
      eigenständiger, kleinerer Rückstellpunkt für eine künftige, tiefere Prüfung.
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES-Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, "Add Manually"-UI-Anbindung für
      `ditec_gol4_create_data()`, `can_commander` mit eigenem GPIO-Board, `g4meover-magspoof`/
      `ukfe-esp-now-mesh` mit der aktuellen GitHub-Zugriffsbreite nicht erreichbar) bleiben
      unverändert offen.

## Firmware-Feature-Mining (Fortsetzung 33)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (usb-army-penetrator,
      DISPLAY_TEXT), davor Ideen-Recherche (CANBUS) und Firmware-Feature-Mining (Ditec GOL4) —
      Firmware-Feature-Mining für diese Sitzung entsprechend wieder an der Reihe.
      Git-Identität vor dem ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Die `unlshd-093`-Restliste ist bereits vollständig abgearbeitet (siehe
      Superrollo-GW60-Eintrag) und kein neuerer `unlshd-09x`/`unlshd-1xx`-Tag vorhanden
      (per `git ls-remote --tags` bestätigt) — stattdessen gegen den aktuellen
      Unleashed-`dev`-Branch (Commit `af2e764`, 2026-10-11) per
      `git diff unlshd-093 af2e764` geprüft, was seit dem letzten Release tatsächlich neu
      ist, statt bereits bekannten Umfang zu wiederholen: zwei neue, eigenständige
      SubGHz-Protokolle (`doorbell32.c/.h`, `keyfinder2.c/.h`), eine neue NFC-Protokollfamilie
      (`texkom`), ein neues LFRFID-Werkzeugmodul (`hitags.c/.h`), dazu Erweiterungen an
      bereits portierten Protokollen (`nord_ice.c`, `faac_slh.c`, `prastel.c`, `keeloq.c`).
- [x] **`doorbell32.c`/`.h`** gewählt: kleinster, vollständig eigenständiger neuer Kandidat
      (247+71 Zeilen bei Unleashed), reines statisches 1:3-PWM-Protokoll ohne Rolling-Code,
      Princeton-Familie mit 32 statt 24 Bit und 14×Te- statt 31×Te-Guard.
- [x] Kompatibilitätsprüfung gegen eine frische `G4MEOVER-FW`-Zweitkopie: alle verwendeten
      Hilfsstrukturen (`SubGhzBlockDecoder`/`_Generic`, `SubGhzProtocolDecoder`/`_Encoder`-
      Vtables) vorhanden. Wie bei allen bisherigen SubGHz-Ports fehlt die gemeinsame
      `common.c`-Hilfsschicht samt `SUBGHZ_ASSERT_*`-Makros — eigene `_alloc`/`_free`/
      `_reset`/`_get_hash_data`/`_stop`/`_yield`-Funktionen nach dem Vorbild meines eigenen,
      bereits produktiven `princeton.c` (gleiche Protokollfamilie) geschrieben. Zusätzlich
      fehlt mir `SubGhzProtocolFlag_Sensors` komplett im Flag-Enum — bewusst weggelassen
      (reine UI-Kategorisierung, kein Einfluss auf Encode/Decode).
- [x] **Fund** (unverändertes Upstream-Verhalten, nur dokumentiert): der Decoder braucht
      mindestens drei Wiederholungen statt zwei bis zur ersten Meldung
      (`found_count == max(repeat-2, 0)`) — der allererste Rahmen wird mangels
      Sync-Guard komplett auf Bit-Ebene verworfen, der zweite vervollständigt zwar 32 Bit,
      scheitert aber am `last_data`-Vergleich (noch 0) — eine Stufe strenger als die bei
      Nord ICE/Treadmill37/Allstar Firefly/Ditec GOL4 dokumentierte
      `ceil(repeat/2)`/`floor(repeat/2)`-Klasse.
- [x] Host-Repro (sechs Testgruppen: Rundlauf+Bit-Exaktheit, Negativtest unterschiedliche
      Rahmen, Negativtest Bit-Korruption mit Resync inkl. differentieller Gegenprobe,
      Negativtest Guard-Fensterprüfung, `te`-Rekonstruktion synchron zum Callback-Zeitpunkt)
      unter `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` bei `-O0`/`-O1`: 73378 Prüfungen, alle bestanden,
      identischer Output beider Optimierungsstufen, keine Compiler-Warnungen — in
      `firmware-mining/repro-doorbell32-subghz-port/repro.c`.
- [x] Vier Patches (`doorbell32_c`/`_h`, `protocol_items_c`/`_h`) gegen eine frische,
      unabhängige `G4MEOVER-FW`-Zweitkopie mit `git apply --check` verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch. Reihenfolge-Hinweis gegenüber den
      anderen, noch nicht angewendeten SubGHz-Port-Vorschlägen (derselbe Dateiende-Kontext
      wie bei Jarolift/KeyFinder/Nord-ICE/Treadmill37/Allstar-Firefly/Ditec-GOL4/Superrollo)
      dokumentiert.
- [x] Details, Kompatibilitätsprüfung, Fund, Host-Repro, Risiko, Testhinweis in
      `firmware-mining/doorbell32-subghz-port.md`.
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden, mit
      `ufbt`/`fbt firmware_all` bauen, am Gerät gegen eine eigene, autorisierte
      Funktürklingel empfangen; gezielt den Fund nachvollziehen (`Repeat=1` erneut senden →
      kein Treffer, `Repeat=3` → ein Treffer).
- [ ] Weitere neue `dev`-Branch-Kandidaten aus demselben Diff bewusst zurückgestellt für
      künftige Sitzungen: `keyfinder2.c`/`.h` (zweites neues SubGHz-Protokoll, zu prüfen ob
      Namenskollision mit meinem bereits portierten `keyfinder.c` ein Problem ist),
      `texkom` (neue NFC-Protokollfamilie, mehrteilig wie MIFARE-Plus-AES, eigener
      Rückstellpunkt), `lib/lfrfid/tools/hitags.c`/`.h` (959+77 Zeilen, größter einzelner
      Kandidat, eigener Rückstellpunkt), die kleineren Erweiterungen an bereits portierten
      `nord_ice.c`/`faac_slh.c`/`prastel.c`/`keeloq.c` (zu prüfen, ob sie neue Hersteller/
      Fehlerkorrekturen oder nur Refactoring enthalten, bevor ich sie gegen meine bereits
      vorgeschlagenen Patches für dieselben Dateien abgleiche).
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest (Poller-Zustandsautomat, laut eigener
      Risikoeinschätzung hardwaregebunden), PSA-Bruteforce-Krypto-Konstanten, vier offene
      CAN-Bus-Kandidaten (nur `can_commander` mit eigenem GPIO-Board), `sanitize_filename()`-
      Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler,
      `g4meover-magspoof`/`ukfe-esp-now-mesh` mit der aktuellen GitHub-Zugriffsbreite nicht
      erreichbar) bleiben unverändert offen.

## App-Verbesserung (usb-army-penetrator — Fortsetzung 3)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Doorbell32
      SubGHz-Port), davor App-Verbesserung (DISPLAY_TEXT) und Ideen-Recherche (CANBUS) —
      App-Verbesserung für diese Sitzung entsprechend wieder an der Reihe. Git-Identität vor
      dem ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `src/Attacks/Ducky/Extensions.h` erneut auf den noch offenen Rückstellpunkt
      ("restliche Ducky-Handler") durchgesehen — alle Handler mit dem
      `str.substr(str.find(' ') + 1)`-Muster waren bereits beim DISPLAY_TEXT-Fund als sicher
      bestätigt. Stattdessen den Aufruferpfad von `handleUSBMode()`
      (`USB_MOUNT_DISK_READ_ONLY`/`_CDROM_READ_ONLY`) weiterverfolgt bis
      `src/Devices/USB/USBMSC.cpp`, noch nicht geprüft.
- [x] **Fund**: `USBMSC::end()` prüft `if (!mscFile)` — exakt umgekehrt zu der in
      derselben Datei dreifach (`msc_read_cb`/`msc_write_cb`/`msc_flush_cb`) konsistent
      verwendeten Bedingung `mscFile && strlen(mscFile.name()) != 0` für "ein Disk-Image ist
      offen". Dadurch: im Hauptfall (Image gemountet) läuft der gesamte Funktionskörper
      nicht — kein `setUnitReady(false)`, kein Flush; im Rohzugriffs-Fall (`mountSD()`) läuft
      er, flusht aber das ungültige `File`-Objekt statt `Devices::Storage` (dem tatsächlich
      aktiven Backing-Store, wie `msc_flush_cb()` es im identischen Fall richtig macht).
      `USBMSC::end()` wird von `USBCore::end()` aufgerufen, das wiederum der zentrale
      Aufräum-Schritt direkt vor jedem Soft-Reset in `DuckyPayload::loop()`
      (`requiresReset`-Zweig, `ESP.restart()`/`watchdog_reboot()`) ist — also genau der Pfad,
      über den ein laufendes Ducky-Skript den Controller neu startet, während ein
      Mass-Storage-Gerät aktiv am Host angemeldet ist.
- [x] Fix (additiv, übernimmt 1:1 die im selben File bereits etablierte
      Fallunterscheidung aus `msc_flush_cb()`): `setUnitReady(false)` unconditional,
      danach `mscFile.flush()` bei offenem Image, sonst `Devices::Storage.flush()`.
- [x] Patch gegen zwei frische, unabhängige Zweitkopien von
      `G4MEOVER18/usb-army-penetrator` (`411b153`) mit `git apply --check` verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (zwei Szenarien: Image gemountet, kein Image gemountet/Rohzugriff) unter
      `gcc -std=c++17 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` bei `-O0`/`-O1`: alle Prüfungen bestanden, identischer
      Output beider Optimierungsstufen, keine Compiler-Warnungen — in
      `repro-usbmsc-end-inverted-flush/repro.cpp`.
- [x] Details, Fund, Patch, Host-Repro, Risiko, Testhinweis in
      `usbmsc-end-inverted-flush-fix.md`. Patch und Repro in
      `usbmsc-end-inverted-flush-fix/` bzw. `repro-usbmsc-end-inverted-flush/`.
- [ ] Lokal zu tun: Patch anwenden, mit der jeweiligen Plattform-`platformio.ini`-Umgebung
      bauen; Hauptfall (`USB_MOUNT_DISK_READ_ONLY <image>.img`, dann Reset mitten im Mount)
      und Rohzugriffs-Fall (`mountSD()`, Schreibzugriff vom Host, dann Reset) je einmal am
      Gerät nachvollziehen.
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `can_commander` mit eigenem GPIO-Board, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `keyfinder2`/`texkom`/`hitags` aus dem Unleashed-dev-Diff,
      `g4meover-magspoof`/`ukfe-esp-now-mesh` mit der aktuellen GitHub-Zugriffsbreite nicht
      erreichbar) bleiben unverändert offen.

## Ideen-Recherche (CAN-Bus-Cluster — Abschluss)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (usb-army-penetrator,
      USBMSC::end()), davor Firmware-Feature-Mining (Doorbell32) und App-Verbesserung
      (DISPLAY_TEXT) — Ideen-Recherche war entsprechend seit drei Sitzungen nicht mehr an der
      Reihe und für diese Sitzung fällig. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Letzten noch offenen Kandidaten aus dem RogueMaster-CAN-Bus-Cluster aufgegriffen:
      **CAN Commander** (`MatthewKuKanich/CAN_Commander`), bislang als "eigenes GPIO-Board
      vorausgesetzt" ohne eigene Quellcode-Prüfung vorgemerkt. Frische, unabhängige Zweitkopie
      angelegt (Commit `6079fe0`, `v2.3.1`, 15615 Zeilen über 41 Dateien) — Präzisierung
      gegenüber dem bisherigen Katalog-Eintrag: kein MCP2515-SPI-Board, sondern ein
      eigenständiges ESP32-Companion-Board mit eigener CAN-Commander-Firmware, über ein
      COBS/CRC16-UART-Protokoll (921600 Baud) angebunden.
- [x] Nach dem bei **Chameleon Ultra Client**/**NFC Relay** etablierten Muster (Fremdhardware
      ohne eigenen Sensorzugriff, aber eigener Protokoll-Parser lohnt trotzdem die Prüfung) die
      komplette UART-Framing-Schicht (`libraries/can_commander_uart.c`: COBS-Encoder/-Decoder,
      CRC16-CCITT, Paket-/Event-Parser) sowie die Haupt-App-Logik (Custom-Inject-Slots,
      DBC-Profile, Argument-Parser, Datei-I/O) und alle Dashboard-Views (~13000 Zeilen gesamt)
      gelesen. Kein Speicherfehler gefunden — jede `memcpy()`/jeder Array-Zugriff ist gegen eine
      geprüfte Länge geklemmt, jeder extern kommende Enum-Rohwert (`CcToolId`/`CcBus`) wird nur
      per `==`-Vergleich verwendet, nie als Array-Index.
- [x] **Fund** (kein Speicherfehler, reproduzierbarer Dauerausfall): die App ruft nirgends
      `expansion_disable()`/`expansion_enable()` auf (`applications/services/expansion/
      expansion.h`) — anders als die fünf bereits dokumentierten Erweiterungsport-Geschwister
      (UART Sniff, RS485 Modbus, GPIO Logic Analyzer, SWD Probe, Argus). Gegen eine frische,
      unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (`mntm-012`) nachvollzogen: der
      Flipper-eigene Expansion-Systemdienst ist standardmäßig aktiv (`expansion_on_system_start()`
      ruft unbedingt `expansion_enable()` auf) und überwacht per Default-Einstellung
      (`expansion_settings_load()` fällt beim Fehlen einer eigenen Einstellungsdatei auf
      `uart_index = 0` = `FuriHalSerialIdUsart` zurück) exakt dasselbe UART, das
      `can_commander_uart.c` selbst nutzt — per Flankenerkennung auf der RX-Leitung
      (`furi_hal_serial_control_enable_expansion_irq()`, `GpioModeInterruptFall`), reagiert
      also schon auf das erste Startbit. Die App selbst schaltet beim Start zuerst 5-V-OTG für
      das ESP32-Board ein, wartet dann nur 400 ms, bevor sie `furi_hal_serial_control_acquire()`
      aufruft — bootet das Board in dieser Zeit und wechselt Pegel auf der RX-Leitung, kann der
      Expansion-Dienst das UART-Handle zuerst bekommen (`expansion_worker.c` ruft selbst
      `furi_hal_serial_control_acquire()` auf und hält das Handle während seines eigenen
      Handshake-Versuchs); `cc_client_open()` bekommt dann `NULL` zurück, drei Ping-Versuche
      scheitern, die App zeigt nur die generische Meldung `"Not connected to \nCAN Commander"`.
      Eigene Fehler-Glossar-Dokumentation (README) verweist für diese Meldung ausschließlich auf
      "verify power/wiring/module state" — kein Hinweis auf die tatsächliche Ursache
      (`Settings → System → Expansion`), da dem Autor das Zusammenspiel offenbar nicht bekannt
      ist.
- [x] Additiver Fix (zwei neue Aufrufe, ein neues `Expansion*`-Feld, nach demselben bei UART
      Sniff/GPIO Logic Analyzer/SWD Probe etablierten Muster: `expansion_disable()` vor dem
      ersten `furi_hal_serial_control_acquire()` in `app_alloc()`, `expansion_enable()` nach dem
      letzten `furi_hal_serial_control_release()` in `app_free()` — Reihenfolge bewusst nach dem
      Client-Teardown, um die bei RS485 Modbus dokumentierte vertauschte Reihenfolge nicht zu
      wiederholen) gegen eine frische, unabhängige Zweitkopie von `MatthewKuKanich/CAN_Commander`
      mit `git apply --check` verifiziert, real angewendet, Ergebnis per `diff -r` byte-für-byte
      mit der Arbeitskopie abgeglichen — identisch.
- [x] Details, API-87-Kompatibilitätsprüfung, Fund, Fix, Testhinweis, Risiko in
      `firmware-mining/can-commander-expansion-conflict-fix.md` (Patch in
      `firmware-mining/can-commander-expansion-conflict-fix/can_commander_expansion_fix.patch`).
      Kuratiert in `ideas/can-commander-api-status.md`. `ideas/pentest-app-katalog.md`
      (CAN-Commander-Tabellenzeile korrigiert/aktualisiert, Log-Eintrag unter "Nächste
      Recherche-Aufträge") ergänzt. Damit ist das gesamte RogueMaster-CAN-Bus-Cluster
      (`can_bus_attack`, `can_commander`, `can_tools`, `can_transceiver`, `canbus`) vollständig
      quelltextlich durchgesehen.
- [ ] Lokal zu tun: Anschaffung/Verkabelung des proprietären CAN-Commander-ESP32-Boards, danach
      Patch gegen die eigene `can_commander`-Basis anwenden, mit `ufbt`/`fbt` bauen,
      Fundnachvollzug gemäß Testhinweis in `firmware-mining/can-commander-expansion-conflict-fix.md`
      (Verbindungsversuch bei aktivierter Expansion-Erkennung vor/nach Patch vergleichen).
- [ ] Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `keyfinder2`/`texkom`/`hitags` aus dem Unleashed-dev-Diff,
      `g4meover-magspoof`/`ukfe-esp-now-mesh` mit der aktuellen GitHub-Zugriffsbreite nicht
      erreichbar) bleiben unverändert offen.

## Firmware-Feature-Mining (Fortsetzung 34)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (CAN Commander,
      CAN-Bus-Cluster-Abschluss), davor App-Verbesserung (usb-army-penetrator, `USBMSC::end()`)
      und ein Masterplan-Heartbeat — Firmware-Feature-Mining für diese Sitzung entsprechend
      wieder an der Reihe. Git-Identität vor dem ersten Commit gesetzt (`G4MEOVER18
      <g4me.over.18@gmail.com>`).
- [x] Aus der seit der Doorbell32-Sitzung offenen `unlshd-093`→`dev`-Restliste
      (`keyfinder2`/`texkom`/`hitags`) den dort als zweiten neuen SubGHz-Kandidaten
      zurückgestellten Punkt aufgegriffen: **`keyfinder2.c`/`.h`** (283+60 Zeilen). Gegen den
      `dev`-Branch erneut geprüft — weiterhin Commit `af2e764` (2026-10-11), kein neuerer
      Stand, keine Wiederholung.
- [x] Den von der Doorbell32-Sitzung offen zurückgestellten Verdacht geklärt: keine
      Namenskollision mit meinem bereits portierten `keyfinder.c` (eigenständiges, älteres
      Protokoll) — alle Symbole sind durchgehend getrennt (`subghz_protocol_keyfinder` vs.
      `subghz_protocol_keyfinder2`, `KeyFinderDecoderStep*` vs. `KeyFinder2DecoderStep*`,
      Anzeigename `"KeyFinder"` vs. `"KeyFinder2"`), per Diff beider Patch-Sätze verifiziert.
- [x] Kompatibilitätsprüfung gegen eine frische `G4MEOVER-FW`-Zweitkopie (Tag-Commit
      `b359f6f`): alle verwendeten Hilfsstrukturen vorhanden. Wie bei jedem bisher
      portierten SubGHz-Protokoll fehlt die gemeinsame `common.c`-Hilfsschicht — hier der
      einfachere Fall als bei Doorbell32/Princeton, da das Original selbst weder ein `te`-
      noch ein `last_data`-Feld führt (siehe Fund); eigene `_alloc`/`_free`/`_reset`/
      `_get_hash_data`/`_serialize`-Funktionen ohne Zusatzzustand, 1:1 nach dem in `common.c`
      selbst nachgelesenen Verhalten. `SubGhzProtocolFlag_Sensors` erneut nicht in meinem
      Flag-Enum vorhanden, bewusst weggelassen.
- [x] **Fund** (kein Speicherfehler, zwei dokumentierte Verhaltensunterschiede zu den
      Geschwister-Ports, beides unverändertes Upstream-Verhalten): (1) KeyFinder2 prüft vor
      dem Melden keinen zweiten, identischen Rahmen (anders als `princeton.c`/`doorbell32.c`
      mit ihrem `last_data`-Vergleich) — ein einzelner, nicht wiederholter 11-Bit-Rahmen wird
      bereits beim ersten Durchlauf gemeldet; (2) der `Reset`-Zustand braucht keine
      vorherige Gap/Guard-Erkennung, sondern erkennt die erste Flanke eines Rahmens direkt —
      bei lückenlos aneinandergereihten Wiederholungen wird dadurch **jede** gemeldet
      (`found_count == repeat`), nicht `repeat-2` wie bei Doorbell32 oder
      `ceil(repeat/2)` wie bei der GAP-Doppelnutzungs-Klasse. Beide Punkte sind zwei Seiten
      derselben Designentscheidung (keine Gap-Synchronisation ⇒ kein natürlicher Anker für
      eine Zwei-Rahmen-Bestätigung) und für den künftigen Hardware-Testhinweis relevant
      (empfindlicher gegenüber Einzelstörimpulsen als die Geschwister-Protokolle).
- [x] Host-Repro (fünf Testgruppen: Rundlauf+Bit-Exaktheit inkl. Einzelrahmen-Fall über 20000
      Zufallsnutzlasten, Negativtest Bit-Korruption im breiten Header mit Sofort-Resync,
      Negativtest Bit-Korruption im schmalen Trailer mit Sofort-Resync, Zustandswechsel-
      Grenzfallprüfung breit→schmal exakt nach Bit 3, Encoder-Puffergrößenprüfung) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: 80016 Prüfungen, alle bestanden, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen — in
      `firmware-mining/repro-keyfinder2-subghz-port/repro.c`.
- [x] Vier Patches (`keyfinder2_c`/`_h`, `protocol_items_c`/`_h`) gegen eine frische,
      unabhängige `G4MEOVER-FW`-Zweitkopie mit `git apply --check` verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch.
- [x] Details, Kompatibilitätsprüfung, Fund, Host-Repro, Risiko, Testhinweis in
      `firmware-mining/keyfinder2-subghz-port.md`.
- [ ] Lokal zu tun: die vier Patches gegen die eigene `mntm-012`-Basis anwenden, mit
      `ufbt`/`fbt firmware_all` bauen, am Gerät gegen einen eigenen, autorisierten
      KeyFinder2-Fob (oder ein synthetisches `.sub`-Testsignal) empfangen; gezielt den Fund
      nachvollziehen (ein einzelner, nicht wiederholter Rahmen wird bereits gemeldet).
- [ ] Aus derselben `dev`-Branch-Restliste weiterhin zurückgestellt: `texkom` (neue
      NFC-Protokollfamilie, mehrteilig wie MIFARE-Plus-AES, eigener Rückstellpunkt),
      `lib/lfrfid/tools/hitags.c`/`.h` (959+77 Zeilen, größter einzelner Kandidat, eigener
      Rückstellpunkt), die kleineren Erweiterungen an bereits portierten `nord_ice.c`/
      `faac_slh.c`/`prastel.c`/`keeloq.c`.
- [ ] Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `g4meover-magspoof`/`ukfe-esp-now-mesh` mit der aktuellen
      GitHub-Zugriffsbreite nicht erreichbar) bleiben unverändert offen.

## App-Verbesserung (g4meover-deck — neu erfasst)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (KeyFinder2),
      davor Ideen-Recherche (CAN Commander) und App-Verbesserung (usb-army-penetrator,
      `USBMSC::end()`) — App-Verbesserung für diese Sitzung entsprechend wieder an der
      Reihe. Repo-Zustand zu Sitzungsbeginn geprüft: HEAD stand detached auf demselben
      Commit wie `origin/main`, lokaler `main`-Zeiger stand 32 Commits zurück — per
      `git checkout main` + `git merge --ff-only origin/main` behoben (reines
      Zeiger-Problem, kein Datenverlust, der zuvor als offen vermerkte Push bereits
      durchgegangen). Git-Identität vor dem ersten Commit gesetzt.
- [x] `g4meover-deck` (Raspberry-Pi-Ökosystem-OS) zum ersten Mal für `app-improvements/`
      aufgegriffen — bisher kein eigener Eintrag. Frische, unabhängige Kopie von
      `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`) geklont, komplette
      Python-Quelle gelesen (`deck/*.py`, `deck/tools/*.py`, ~2000 Zeilen).
- [x] **Fund**: `SatelliteLink.send()` (`deck/satellite_link.py`) persistiert den
      Rolling-Counter für das `ukfe_rf`-Anti-Replay-Schema erst **nach** dem UART-Write
      an die Satelliten, nicht davor. Stirbt der Pi-seitige Prozess (Kill, SSH-Abbruch,
      vor allem ein Stromausfall auf der SD-Karten-basierten Zielhardware ohne USV)
      zwischen dem erfolgreichen Write und `_save_counter()`, hat der Satellit den Frame
      bereits akzeptiert (sein `last_counter` steht auf dem neuen Wert), aber der
      dafür verwendete Counter steht nicht auf der Platte — der nächste Prozessstart
      sendet denselben oder einen niedrigeren Wert erneut und wird vom Satelliten als
      bereits verbraucht verworfen (stiller Verlust des jeweils ersten Befehls nach
      einem unsauberen Absturz). Alle drei Aufrufer (`deck_ctl.py`, `deck_daemon.py`,
      `tools/hid_stream_send.py`) teilen sich `send()` und sind gleich betroffen.
      Zusätzlich festgestellt und mitbehoben: `_save_counter()` ruft `os.replace()`
      ohne vorheriges `fsync()` auf — auf dem beschriebenen SD-Karten-Zielsystem ohne
      USV kann dadurch sogar ein bereits atomar umbenannter Stand nach einem echten
      Stromausfall wieder auf eine ältere Version zurückfallen.
- [x] Fix (additiv, zwei Stellen in derselben Methode): `_save_counter()` ruft jetzt
      `f.flush()`/`os.fsync(f.fileno())` vor `os.replace()`; `send()` persistiert den
      neuen Counter jetzt vor dem UART-Write statt danach. Stirbt der Prozess dazwischen,
      bleibt höchstens ein Counter-Wert ungenutzt liegen — bereits in `_load_counter()`s
      eigenem Kommentar als unkritischer Fall vorgesehen. Patch (`git apply --check`
      gegen eine frische, unabhängige Zweitkopie verifiziert, real angewendet,
      byte-für-byte abgeglichen — identisch) in
      `app-improvements/g4meover-deck/satellite-link-counter-saved-after-send-fix/satellite_link_py.patch`.
- [x] Host-Repro (echte `ukfe_rf.py` sowie `satellite_link_orig.py`/`_fixed.py`, 1:1 Stand
      vor/nach dem Patch, Mock-`serial`-Modul statt pyserial, simulierter Satellit mit der
      realen `counter <= last_counter`-Regel, injizierter Absturz direkt nach einem
      erfolgreichen UART-Write): Ist-Zustand verwirft den ersten Befehl nach dem
      simulierten Neustart (1/10), Fix verwirft keinen (0/10) — in zwei
      aufeinanderfolgenden Läufen deterministisch reproduziert, in
      `app-improvements/g4meover-deck/repro-satellite-link-counter-saved-after-send/repro.py`.
- [x] Details, Nutzen, betroffene Datei, Risiko, Testhinweis in
      `app-improvements/g4meover-deck/satellite-link-counter-saved-after-send-fix.md`.
- [ ] Lokal zu tun: Patch einbauen, `deck-ctl sat ping` zweimal ausführen, den
      Deck-Prozess (bzw. den `systemd`-Dienst) mitten in einem dritten Aufruf hart
      abbrechen (`kill -9` oder Pi stromlos schalten direkt nach "gesendet: ping" auf der
      Konsole), danach erneut `deck-ctl sat ping` ausführen — vor dem Fix bleibt die
      Reaktion am Satelliten beim ersten Aufruf nach dem Neustart aus, nach dem Fix
      reagiert er sofort.
- [ ] Separat festgestellt, bewusst nicht mitgelöst (eigenständiger, größerer Fund):
      `deck_daemon` (Dauerbetrieb) und einzelne `deck-ctl sat ...`-Aufrufe können
      dieselbe `counter_file` ohne Datei-Locking über Prozessgrenzen hinweg
      gleichzeitig beschreiben. `scenario_runner.py` nutzt für Labor-Szenarien bewusst
      einen eigenen, nicht persistierten In-Memory-Zähler — kein eigenständiger Fund.
      `g4meover-deck` darüber hinaus noch nicht vollständig durchgesehen
      (`flipper_link.py`, `device_discovery.py`, `handshake_capture.py`,
      `hackrf_link.py`, `lorix_link.py`, `dogytag_link.py`, `zigbee_link.py`,
      `wpa_crack.py`/`deck/tools/wpa_crack.py`) — eigener Rückstellpunkt für künftige
      App-Verbesserungs-Sitzungen. Die übrigen seit längerem zurückgestellten
      Backlog-Punkte (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3
      Rest, PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler, `texkom`/`hitags` aus
      dem Unleashed-dev-Diff) bleiben unverändert offen.

## Ideen-Recherche (LoRa Relay — neue Kategorie)
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (`g4meover-deck`), davor
      Firmware-Feature-Mining (KeyFinder2) und Ideen-Recherche (CAN Commander) — Ideen-Recherche
      war seit zwei Sitzungen nicht mehr an der Reihe und für diese Sitzung fällig.
      `ideas/pentest-app-katalog.md` vollständig durchgesehen: alle bisherigen Cluster laut
      eigenem Log bereits vollständig quelltextlich durchgesehen. Git-Identität vor dem ersten
      Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer bislang komplett fehlenden Kategorie gesucht statt einen bestehenden
      Cluster erneut anzufassen: "LoRa/LoRaWAN" — fehlt bislang trotz `g4meover-deck`s bereits
      vorhandenem `lorix_link.py`-Pfad. RogueMaster-Bündel (`flipperzero-firmware-wPlugins`,
      Branch `420`) per `git clone --filter=blob:none --no-checkout` durchsucht
      (`applications/external/`, 820 Einträge) — **LoRa Relay** (App-ID `lora_relay`) als
      einzige LoRa-App gefunden, Original-Quelle `ElectronicCats/flipper-SX1262-LoRa`
      (Repo-HEAD-Commit `430db6c`), SX1262-Sniffer/-Sender für ein eigenes Erweiterungsmodul.
- [x] Vollständig gelesen (zwei Kerndateien: `lora.c` 1064 Zeilen SX1262-SPI-Treiber,
      `lora_relay.c` 2732 Zeilen App-Logik/UI; vendortes `text_input`-Eingabemodul und
      `validators.c` überflogen, kein eigener Fund). API-87-Status gegen eine frische,
      unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag-Commit `b359f6f`, `mntm-012`)
      verifiziert: alle vier Erweiterungsport-Pins (`gpio_ext_pa4/pc0/pc1/pc3`), drei
      zweckentfremdete interne Pins (`gpio_swclk`/`gpio_usart_tx`/`gpio_usart_rx`),
      `furi_hal_spi_bus_handle_external`, zweiter USB-CDC-Kanal (`usb_cdc_dual`/
      `furi_hal_cdc_send`, identisches Muster zu GPIO Logic Analyzer) sowie Standard-GUI-/
      Storage-/RTC-APIs byteidentisch vorhanden. Kein Port-Hindernis, reines externes FAP.
- [x] **Fund**: `lora_view_sniffer_draw_callback()` (`lora_relay.c`) baut beim SD-Logging einer
      Sniff-Sitzung eine JSON-Zeile per `snprintf(final_string, 666, ...)` — `final_string` ist
      aber nur `char final_string[400]`, die `666` ein Literal statt `sizeof(final_string)`.
      Alle anderen rund zwanzig `snprintf()`-Aufrufe derselben Datei übergeben korrekt
      `sizeof(<puffer>)` — dieser eine Aufruf ist die einzige Ausnahme. Der dominante variable
      Anteil des Formatstrings ist die Hex-Darstellung des zuletzt per LoRa empfangenen,
      vollständig fremdbestimmten Pakets (bis zu 255 Byte Nutzlast → bis zu 510 Hex-Zeichen) —
      ein einzelnes LoRa-Paket mit ausreichend langer Nutzlast (ab rund 132 Byte) während
      aktivem "Recording" überschreibt dadurch den Stack, ganz ohne Pairing/Authentifizierung
      (LoRa kennt keinen Verbindungsaufbau); beim maximal möglichen 255-Byte-Paket schreibt
      `snprintf()` bis zu 638 Nutzzeichen plus Nullterminator — rund 239 Byte über das
      tatsächliche Array hinaus. GCCs eigene `_FORTIFY_SOURCE`-Prüfung erkennt den Widerspruch
      bereits statisch beim Kompilieren (`specified bound 666 exceeds destination size 400`).
- [x] Fix (additiv, eine Zeile): Größenangabe durch `sizeof(final_string)` ersetzt — identisches
      Muster zu jedem anderen `snprintf()`-Aufruf in derselben Datei, kein
      Verhaltensunterschied im Normalfall (Äquivalenztest siehe Host-Repro). Patch gegen eine
      frische, unabhängige Zweitkopie von `ElectronicCats/flipper-SX1262-LoRa` (Commit
      `430db6c`) mit `git apply --check` verifiziert, real angewendet, Ergebnis gegen eine
      zweite, unabhängig geklonte Kopie per `diff -r` byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (`final_string`-Aufbau 1:1 transkribiert, nur die drei Firmware-Aufrufe durch
      feste Platzhalterwerte ersetzt): fünf fork-isolierte Szenarien (Regression 20-Byte-Paket
      ORIGINAL, Fund an der Schwelle 140-Byte-Paket ORIGINAL, Fund worst case 255-Byte-Paket
      ORIGINAL, Fix 255-Byte-Paket FIX, Äquivalenztest 20-Byte-Paket ORIGINAL vs. FIX) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: alle fünf Szenarien bestanden, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen — in
      `firmware-mining/repro-lora-relay-app-port/repro.c`.
- [x] Details, API-87-Kompatibilitätsprüfung, Fund, Fix, Host-Repro, Risiko, Testhinweis in
      `firmware-mining/lora-relay-app-port.md` (Patch in
      `firmware-mining/lora-relay-app-port/lora_relay_sniffer_log_overflow.patch`). Kuratiert
      in `ideas/lora-relay-api-status.md`. `ideas/pentest-app-katalog.md` um die neue Kategorie
      "LoRa" sowie einen Log-Eintrag unter "Nächste Recherche-Aufträge" ergänzt.
- [ ] Lokal zu tun: die eine Zeile gegen die eigene `lora_relay`-App-Kopie patchen, mit
      `ufbt`/`fbt` bauen; Fundnachvollzug gemäß Testhinweis in
      `firmware-mining/lora-relay-app-port.md` (SX1262-Modul am Erweiterungsport, Sniffer-
      Ansicht öffnen, "Recording" aktivieren, von einem zweiten eigenen LoRa-Sender ein
      Testpaket mit mindestens ~140 Byte Nutzlast senden).
- [ ] Der Transmitter-/Replay-Pfad (`tx_payload()`/`send_data()`, Log-Datei-Parsing) bewusst
      nicht als eigenständiger Fund dokumentiert (VLA-Länge stammt aus derselben, zuvor
      gelesenen Zeile, setzt bereits physischen SD-Karten-Zugriff voraus) — Rückstellpunkt für
      eine künftige Sitzung. Die übrigen seit längerem zurückgestellten Backlog-Punkte
      (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, `texkom`/`hitags` aus dem
      Unleashed-dev-Diff, `g4meover-deck`s restliche Module aus der letzten
      App-Verbesserungs-Sitzung) bleiben unverändert offen.

## Firmware-Feature-Mining (Hitag S / ID82xx-Magic-Tag-Werkzeug)
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (LoRa Relay, neue Kategorie),
      davor App-Verbesserung (`g4meover-deck`) und Firmware-Feature-Mining (KeyFinder2) —
      Firmware-Feature-Mining für diese Sitzung entsprechend wieder an der Reihe. Vor dem
      ersten Commit festgestellt, dass der lokale `main`-Branch-Zeiger noch nicht auf den
      letzten Merge-Commit vorgerückt war (detached HEAD von der letzten Sitzung) — per `git
      merge --ff-only` nachgezogen; auf `origin/main` war der Stand bereits korrekt vorhanden
      (nur der lokale Branch-Zeiger war veraltet), keine Datenabweichung. Git-Identität gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus der seit der Doorbell32-Sitzung offenen `unlshd-093`→`dev`-Restliste
      (`texkom`/`hitags`, `keyfinder2` bereits portiert) diesmal statt erneut zurückzustellen
      den dort als "größter einzelner Kandidat" mehrfach vertagten Punkt aufgegriffen:
      `lib/lfrfid/tools/hitags.c`/`.h` (959+77 Zeilen). Gegen den `dev`-Branch erneut
      geprüft — weiterhin Commit `af2e764`, kein neuerer Stand.
- [x] Kompatibilitätsprüfung gegen eine frische `G4MEOVER-FW`-Zweitkopie (Tag-Commit
      `b359f6f`, `mntm-012`): `targets/f7/furi_hal/furi_hal_rfid.h` und `lib/bit_lib/bit_lib.h`
      stellen jede von `hitags.c` benutzte Funktion/Signatur byteidentisch bereit (dasselbe
      LF-125-kHz-Interface wie bei den bereits vorhandenen `tools/t5577.c`/`em4305.c`);
      `lib/lfrfid/SConscript`s `GlobRecursive("*.c*")` erfasst neue Tool-Dateien automatisch.
      Eigenständige Lücke identifiziert statt übersehen: `lfrfid_worker.h` kennt nur
      Einzelprotokoll-Schreiben, das `LFRFIDWriteType`/`LFRFIDWriteRequest`-Dispatch-System,
      für das `hitags.c` im `dev`-Branch eigentlich gedacht ist (sichtbar am
      `test_lfrfid_protocol_em_write_hitags`-Unit-Test), existiert in `mntm-012`s Basis nicht
      — das Werkzeugmodul selbst ist voll kompatibel, die Integrationsschicht fehlt komplett.
- [x] Zwei additive Patches (`hitags_h.patch`/`hitags_c.patch`, Dateien unverändert aus dem
      Unleashed-Quelltext übernommen) gegen drei unabhängige, frische `G4MEOVER-FW`-Zweitkopien
      mit `git apply --check` verifiziert, real angewendet, paarweise per `diff -r`
      byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (abweichend vom bisherigen Transkriptions-Muster): die tatsächliche,
      unveränderte `hitags.c` zusammen mit der real aus `G4MEOVER-FW` kopierten
      `lib/bit_lib/bit_lib.c` gegen schlanke Platzhalter für `furi.h`/`furi_hal_rfid.h`/
      `core/check.h` kompiliert und die echte, modul-eigene `hitags_selftest()` ausgeführt —
      prüft CRC-8, alle vier Frame-Builder und den Anticollision-Decoder (inkl.
      TTF-Verwechslungsschutz, Alignment-Grenzfall, Präfix-Abgleich-Trust-Anchor) gegen
      Datenblatt-/Proxmark3-Vektoren, ohne jede Hardware. Unter `gcc -std=c17 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei `-O0`/`-O1`: `NULL`
      (alle Vektoren bestanden) in beiden Fällen, identischer Output, keine ASan-/
      UBSan-Meldungen.
- [x] Details, Kompatibilitätsprüfung, Lücke, Patches, Host-Repro, Risiko, zweiphasige
      Port-Skizze (Phase 1 hier erledigt; Phase 2 Integrationsschicht als eigener
      Rückstellpunkt, zwei Optionen gegenübergestellt) und Testhinweis in
      `firmware-mining/hitags-lfrfid-magic-tag-port.md` (Patches in
      `firmware-mining/hitags-lfrfid-magic-tag-port/`, Host-Repro in
      `firmware-mining/repro-hitags-lfrfid-magic-tag-port/`).
- [ ] Lokal zu tun: beide Patches gegen die eigene `mntm-012`-Basis anwenden, mit
      `ufbt`/`fbt firmware_all` bauen (reiner Compile-Test ohne Phase 2 — noch kein
      Aufrufer); Host-Repro lokal nachvollziehen. Hardware-Test erst nach Phase 2 sinnvoll.
- [ ] Phase 2 (Integrationsschicht: generisches `LFRFIDWriteType`-Dispatch-System nachbauen
      ODER ein schlanker eigenständiger Scene-Pfad, siehe Abwägung im Dokument) bleibt
      eigener Rückstellpunkt für eine künftige Sitzung, ebenso `hitagmicro.c`/`.h`
      (327+57 Zeilen, dieselbe BPLM-Timing-Familie, nicht Teil dieser Sitzung) und `texkom`
      aus derselben `dev`-Branch-Restliste. Die übrigen seit längerem zurückgestellten
      Backlog-Punkte (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, `g4meover-deck`s restliche Module)
      bleiben unverändert offen.

## App-Verbesserung (g4meover-deck — Cross-Prozess-Counter-Race, aus vorigem Fund aufgegriffen)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Hitag S), davor
      Ideen-Recherche (LoRa Relay) und App-Verbesserung (g4meover-deck, Save-vor-Send) —
      App-Verbesserung für diese Sitzung entsprechend wieder an der Reihe. Git-Identität
      vor dem ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Frische, unabhängige Zweitkopie von `G4MEOVER18/g4meover-deck` (main, weiterhin
      Commit `b030e45`) geklont. Statt eines der noch komplett ungeprüften Module
      (`device_discovery.py`, `handshake_capture.py`, `hackrf_link.py`, `lorix_link.py`,
      `dogytag_link.py`, `zigbee_link.py`, `wpa_crack.py`) aufzugreifen, bewusst den bereits
      zweimal zurückgestellten, konkret benannten Punkt aus
      `satellite-link-counter-saved-after-send-fix.md` übernommen: `deck_daemon` und
      `deck-ctl` teilen sich `counter_file` ohne Datei-Locking über Prozessgrenzen hinweg.
      `deck_daemon.py`/`deck_ctl.py` erneut gelesen, um die genaue Instanziierung zu
      bestätigen: `deck_daemon.py` erzeugt genau ein `_link`-Objekt beim Prozessstart
      (Modulebene, `ThreadingHTTPServer`), `deck_ctl.py`s `make_link()` erzeugt pro
      CLI-Aufruf ein frisches `SatelliteLink`.
- [x] **Fund**: `SatelliteLink.send()` liest `self._counter` nur einmal bei `__init__` von
      der Platte und hält ihn danach nur im RAM. `self._lock` (`threading.Lock`) schützt
      ausschließlich mehrere Threads innerhalb eines einzelnen Prozesses, nichts zwischen
      zwei verschiedenen Prozessen. Läuft `deck_daemon` dauerhaft und wird währenddessen ein
      `deck-ctl sat ...`-Aufruf ausgeführt, kennt dessen frisch geladener, niedrigerer
      In-Memory-Zähler den zwischenzeitlichen Daemon-Fortschritt nicht — der vom `deck-ctl`-
      Aufruf gesendete Counter wird vom Satelliten als bereits verbraucht verworfen, UND
      dessen `_save_counter()` überschreibt die Platte anschließend mit diesem zu niedrigen
      Wert. Jeder weitere, unabhängig gestartete `deck-ctl`-Aufruf liest ab dann denselben zu
      niedrigen Stand und wird seinerseits verworfen — eine ganze Serie nachfolgender Befehle
      (nicht nur einer), bis der wiederholt erhöhte Zähler den wahren, dem Satelliten bereits
      bekannten Stand wieder eingeholt hat. Alle drei Aufrufer (`deck_ctl.py`, `deck_daemon.py`,
      `tools/hid_stream_send.py`) teilen sich `send()` und sind gleich betroffen.
- [x] Fix (additiv, neue Klasse `_CounterFileLock` plus angepasster Kritikalabschnitt in
      `send()`): exklusiver Datei-Lock auf einer separaten `counter_file + ".lock"`-Datei
      (`fcntl.flock` unter POSIX, `msvcrt.locking` unter Windows — beides Standardbibliothek,
      keine neue Abhängigkeit), unter dem `send()` die `counter_file` bei jedem Aufruf per
      `max(self._counter, self._load_counter()) + 1` neu einliest, bevor es erhöht und
      persistiert. Patch baut auf dem bereits vorgeschlagenen Zwischenstand aus
      `satellite-link-counter-saved-after-send-fix/satellite_link_py.patch` auf (beide ändern
      dieselbe Methode) — Anwendungsreihenfolge im Dokument vermerkt. Gegen zwei frische,
      unabhängige Zweitkopien verifiziert (Kopie 1 nur vorigen Patch, Kopie 2 vorigen Patch
      plus diesen Patch mit `git apply --check` geprüft und real angewendet) — beide Kopien
      byte-für-byte identisch zum erwarteten Endstand. Patch in
      `app-improvements/g4meover-deck/satellite-link-cross-process-counter-race-fix/satellite_link_py.patch`.
- [x] Host-Repro (echte `ukfe_rf.py`, `satellite_link_before.py`/`_after.py` 1:1 vor/nach
      diesem Patch, Mock-`serial`-Modul, simulierter Satellit mit der realen
      `counter <= last_counter`-Regel): ein `deck_daemon`-Stellvertreter sendet während ein
      zuvor konstruierter `deck-ctl`-Stellvertreter noch nicht gesendet hat fünf Befehle,
      danach zehn weitere, je frisch konstruierte Nachlauf-Aufrufe — Ist-Zustand verwirft
      4/10 Nachlauf-Aufrufe, Fix verwirft 0/10, in mehreren Läufen deterministisch
      reproduziert, in
      `app-improvements/g4meover-deck/repro-satellite-link-cross-process-counter-race/repro.py`.
      Zusätzlich mit vier echten, gleichzeitig laufenden Threads (kein gemeinsames Lock-Objekt,
      bildet vier echte Prozesse nach) bestätigt: von 200 gesendeten Befehlen werden vor dem
      Fix nur 50 angenommen (150 verworfen), nach dem Fix alle 200 — in
      `repro-satellite-link-cross-process-counter-race/repro_threads.py`.
- [x] Details, Nutzen, betroffene Datei, Anwendungsreihenfolge, Risiko, Testhinweis in
      `app-improvements/g4meover-deck/satellite-link-cross-process-counter-race-fix.md`.
- [ ] Lokal zu tun: zuerst `satellite-link-counter-saved-after-send-fix/satellite_link_py.patch`,
      dann `satellite-link-cross-process-counter-race-fix/satellite_link_py.patch` gegen die
      eigene `satellite_link.py` anwenden. Funktionstest: `deck_daemon`-Dienst (oder
      `python3 deck_daemon.py` im Vordergrund) starten, parallel mehrfach `deck-ctl sat ping`
      ausführen, während über die Ökosystem-UI/`curl -X POST .../sat/ping` ebenfalls Befehle
      an denselben Satelliten gehen — vor dem Fix bleibt die Reaktion bei einem Teil der
      `deck-ctl`-Aufrufe aus, nach dem Fix reagiert der Satellit auf jeden einzelnen Aufruf.
- [ ] `g4meover-deck` weiterhin nicht vollständig durchgesehen (`device_discovery.py`,
      `handshake_capture.py`, `hackrf_link.py`, `lorix_link.py`, `dogytag_link.py`,
      `zigbee_link.py`, `wpa_crack.py`/`deck/tools/wpa_crack.py`, `flipper_link.py`) —
      eigener Rückstellpunkt für künftige App-Verbesserungs-Sitzungen. Die übrigen seit
      längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/
      Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `texkom`/`hitagmicro.c` aus dem Unleashed-dev-Diff, Hitag-S-Phase-2-
      Integrationsschicht) bleiben unverändert offen.

## Firmware-Feature-Mining (NFC FeliCa-Listener — neuer Durchlauf)
- [x] Unleashed `dev` erneut gegen den zuletzt geprüften Stand (`af2e764`) geprüft: genau ein
      neuer Inhaltscommit, `3fb1d517` (PR #4465, "NFC: Fix FeliCa command handling",
      13.10.2026) — bündelt zehn Teilfixe für die FeliCa-Emulation (`felica_listener.c`,
      `felica_poller*.c`, `felica.h`). Neun der zehn Teilfixe bauen auf einer
      `FelicaSystem`/`FelicaService`-Multisystem-Infrastruktur auf, die in meiner
      `mntm-012`-Basis (`G4MEOVER-FW`, `b359f6f`) nicht existiert (mein `felica_listener.c`
      entspricht einem älteren, einfacheren Entwicklungsstand) — nicht portierbar ohne vorher
      die Multisystem-Emulation selbst nachzuziehen, eigener größerer Rückstellpunkt
      (vergleichbar mit `texkom`).
- [x] Den einen eigenständig portierbaren Teilfix aufgegriffen: "Fix FeliCa emulation hanging
      after rejected commands" — betrifft ausschließlich `felica_listener_run()`
      (`NfcEventTypeRxEnd`-Zweig), bei mir byte-identisch zum Unleashed-Vorher-Stand
      (`3fb1d517^`, gegen frische Zweitkopie verglichen). **Fund:** `command` wird nur in
      einem einzigen Ablehnungspfad (System-Code nicht unterstützt) auf `NfcCommandReset`
      gesetzt; die vier übrigen Ablehnungspfade (falsche CRC, falsche Länge,
      Hardware-Polling-Leak, falsche IDm) sowie ein Frame-Exchange-Fehler bei sonst gültiger
      Anfrage bleiben auf `NfcCommandContinue` stehen, obwohl kein Response gesendet wurde —
      die Emulation haengt danach bis zum naechsten externen Field-Off/-On-Zyklus. Fix
      (additiv): lokale `FelicaError error`-Variable auf jedem Pfad gesetzt, finale
      `command`-Entscheidung von `error == FelicaErrorFeatureUnsupported` auf
      `error != FelicaErrorNone` verallgemeinert; ein neuer Enum-Wert `FelicaErrorIgnore`
      (Namensgebung aus Unleasheds PR übernommen) markiert die zwei bewusst stillen Pfade.
      Zwei additive Patches (`felica_h.patch`, `felica_listener_c.patch`) gegen zwei frische,
      unabhängige `G4MEOVER-FW`-Zweitkopien mit `git apply --check` verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch. Host-Repro (Kontrollfluss von
      `felica_listener_run()`s `NfcEventTypeRxEnd`-Zweig, ALT/NEU nebeneinander, acht
      Szenarien) unter ASan/UBSan bei `-O0`/`-O1`: ALT lässt 5/8 abgelehnte Anfragen ohne
      Reset, NEU setzt in allen fünf Fällen korrekt zurück, beide Erfolgsfälle unverändert —
      identischer Output beider Optimierungsstufen, keine Compiler-Warnungen. Details, Fund,
      Risiko, Port-Skizze, Testhinweis in `firmware-mining/felica-listener-reject-hang-fix.md`
      (Patches in `firmware-mining/felica-listener-reject-hang-fix/`, Host-Repro in
      `firmware-mining/repro-felica-listener-reject-hang-fix/`).
- [ ] Lokal zu tun: `felica_h.patch`, dann `felica_listener_c.patch` gegen die eigene
      `mntm-012`-Basis anwenden, mit `ufbt`/`fbt firmware_all` bauen. Funktionstest: eigene
      FeliCa-Karte emulieren, mit einem Reader/zweiten Flipper mehrfach hintereinander
      pollen/lesen, dabei mindestens eine Anfrage bewusst zur Ablehnung bringen (z. B.
      Polling mit fremdem System-Code, oder zwei Leseversuche ohne Feld-Unterbrechung) — vor
      dem Fix bleibt die Karte danach fuer den Rest der Sitzung stumm, nach dem Fix antwortet
      sie auf die naechste gueltige Anfrage wieder normal.
- [ ] Die neun verbleibenden Teilfixe aus PR #4465 (Standard-FeliCa-Multisystem-Emulation)
      bleiben zurückgestellt, bis die zugrundeliegende Multisystem-Infrastruktur selbst
      portiert ist — eigener, groesserer Rueckstellpunkt. Die uebrigen seit laengerem
      zurueckgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/
      Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `texkom`/`hitagmicro.c`, Hitag-S-Phase-2-Integrationsschicht) bleiben
      unveraendert offen.

## Ideen-Recherche (Sobe — IR-Ghost-Detector, Gegenstück zu Nyx)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (NFC FeliCa-Listener),
      davor App-Verbesserung (`g4meover-deck`) und Firmware-Feature-Mining (Hitag S) —
      Ideen-Recherche war von den drei WORKFLOW-Kategorien am längsten nicht an der Reihe
      (letzter Ideen-Recherche-Eintrag: LoRa Relay). Git-Identität vor dem ersten Commit
      gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`). Zusätzlich den zwischenzeitlichen
      Cloud-Heartbeat-Commit (Masterplan, Cross-Repo-Vertiefung) auf `main` übernommen (reiner
      Fast-Forward, kein eigener Beitrag dieser Sitzung).
- [x] Gezielt nach einem Gegenstück/Alternativkandidaten zur bereits katalogisierten
      Counter-Surveillance-App **Nyx** (IR-Erkennung versteckter Kameras) gesucht, statt einen
      der bereits mehrfach durchsuchten Cluster (NFC/CAN/GPIO-Serial) erneut aufzugreifen.
      **Sobe** (`sacriphanius/sobe`, GitLab, MIT) gefunden: 521 Zeilen über acht Quelldateien,
      vollständig gelesen (100 %). Sehr junges Projekt (zwei Commits, ein Tag, letzte Aktivität
      26.07.2026).
- [x] Gegenüber Nyx als eigenständig statt redundant eingeordnet (anders als der
      Badge-Audit-vs.-Access-Audit-Fall): Sobes zusätzlicher aktiver 38-kHz-Sweep-Modus
      (Optic-Spy-Finder-Prinzip gegen Kamera-Linsen-Reflexionen) deckt einen Erkennungsfall ab,
      den das listen-only Nyx laut eigener Doku bewusst nicht abdeckt — beide Apps ergänzen
      sich, kein Verwerfungs-Fall.
- [x] API-87-Status gegen eine frische, bloblose `Next-Flip/Momentum-Firmware`-Zweitkopie
      (Tag `mntm-012`, Commit `b359f6f`) verifiziert: alle sieben verwendeten
      `furi_hal_infrared_*`-Funktionen, der `FuriHalInfraredTxPin`-Enum,
      `furi_hal_power_enable_otg()`/`_disable_otg()`, `gpio_ext_pa7`, die drei verwendeten
      `NotificationSequence`-Symbole und `elements_progress_bar()` byteidentisch vorhanden.
      Dabei die `furi_hal_infrared`-Zustandsmaschine (`InfraredStateIdle`/`AsyncRx`/`AsyncTx`/...)
      im Firmware-Quelltext nachvollzogen und bestätigt, dass Sobes im ~65-ms-Takt wechselnder
      RX/TX-Sweep-Zyklus (`ir_worker.c::sobe_worker_thread()`) keine der internen
      `furi_check()`-Zustandsvorbedingungen verletzt (beide Stop-Funktionen sind synchron und
      garantieren `InfraredStateIdle` vor der Rückkehr) — kein Crash-Risiko, trotz anfänglicher
      Vermutung eines möglichen `furi_check()`-Abbruchs im Sweep-Modus.
- [x] **Fund**: der GPIO-Autoerkennungsblock in `sobe.c::sobe_app()` reimplementiert von Hand
      exakt die bereits vorhandene Firmware-Funktion `furi_hal_infrared_detect_tx_output()`
      (identischer Algorithmus) — reine Duplikation, kein eigener Fund dort. Der tatsächliche
      Fund liegt in `ir_worker.c::sobe_worker_set_tx_output()`: die einzige Stelle im gesamten
      Quelltext, die `furi_hal_power_enable_otg()`/`_disable_otg()` aufruft, wird genau einmal
      beim App-Start aufgerufen (wenn ein externes IR-Modul erkannt wurde) und nie wieder — auch
      nicht in der Aufräumsequenz von `sobe_app()` beim Beenden. Zum Vergleich der offizielle
      Referenzpfad (`applications/main/infrared/infrared_app.c`): dort steht direkt vor
      `infrared_free()` explizit `infrared_set_tx_pin(internal)` plus
      `infrared_enable_otg(false)` — die Firmware macht die OTG-Abschaltung beim Beenden also
      zur Pflicht für jede extern aktivierende App. Konsequenz: mit angeschlossenem externem
      IR-Modul gestartet, bleibt die 5-V-OTG-Boost-Versorgung nach regulärem App-Ende dauerhaft
      an (bekannter Akku-Verschleiß-Faktor), kein Ziel-Sicherheitsrisiko.
- [x] Fix (additiv, vier neue Zeilen in `sobe.c::sobe_app()`): direkt nach
      `sobe_worker_stop(worker);` und vor `sobe_worker_free(worker);` die bereits vorhandene
      Funktion ein zweites Mal mit `false` aufrufen, wenn extern aktiviert war — spiegelt exakt
      das offizielle `infrared_set_tx_pin(internal) + infrared_enable_otg(false)`-Muster. Patch
      gegen zwei frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (Kontrollfluss-Nachbildung von `sobe_worker_set_tx_output()` sowie dem
      Start-/Exit-Ablauf von `sobe_app()`, Mock für `furi_hal_power_enable_otg()`/`_disable_otg()`
      über ein gemeinsames globales Bool) unter ASan/UBSan bei `-O0`/`-O1`: zwei
      Regressionsfälle (kein externes Modul) lassen OTG in beiden Versionen korrekt aus, ALT
      lässt OTG nach simuliertem App-Ende bei erkanntem externem Modul an (der Fund), NEU schaltet
      es korrekt wieder ab — identischer Output beider Optimierungsstufen, keine
      Compiler-/Sanitizer-Meldungen.
- [x] Details, Fund, Fix, Host-Repro, Risiko, Testhinweis in
      `ideas/sobe-ir-ghost-detector-app-status.md` (Patch in
      `ideas/sobe-otg-left-enabled-on-exit-fix/sobe_c.patch`, Host-Repro in
      `ideas/repro-sobe-otg-left-enabled-on-exit/repro.c`). `ideas/pentest-app-katalog.md` um
      die neue Katalog-Zeile (Counter-Surveillance/TSCM) sowie einen Log-Eintrag unter "Nächste
      Recherche-Aufträge" ergänzt.
- [ ] Lokal zu tun: Patch gegen die eigene `sobe.c` anwenden, mit `ufbt launch_app`/`fbt` bauen.
      Funktionstest: eigenes externes IR-Modul am Erweiterungsport anschließen, Sobe starten
      (Anzeige muss "EXT" zeigen), über "Zurück" beenden, danach über `Settings → Power →
      USB-Power-Output` (oder per `power` CLI) prüfen, dass die 5-V-Ausgabe aus ist — vor dem
      Fix bleibt sie an, nach dem Fix ist sie nach dem Beenden aus. Ohne externes Modul (reiner
      interner IR-Betrieb) unverändertes Verhalten vor/nach dem Fix.
- [ ] Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `texkom`/`hitagmicro.c`, Hitag-S-Phase-2-Integrationsschicht,
      `g4meover-deck`s restliche Module) bleiben unverändert offen.

## App-Verbesserung: g4meover-deck — dogytag_link.py Klartext-Default-Zugangsdaten

- [x] Frische, unabhängige Zweitkopie von `G4MEOVER18/g4meover-deck` (main, weiterhin
      Commit `b030e45`) geklont. Statt eines der noch komplett ungeprüften Module
      (`device_discovery.py`, `handshake_capture.py`, `hackrf_link.py`, `zigbee_link.py`,
      `wpa_crack.py`) den beim letzten Cross-Repo-Heartbeat bereits identifizierten, bisher
      nur dokumentierten Punkt aufgegriffen: `dogytag_link.py` las `BROKER_USER`/`BROKER_PASS`
      per `os.getenv(..., "yanis")`/`os.getenv(..., "12345678")` — ein echt wirkendes
      Zugangsdaten-Paar als Default direkt im Quelltext eines öffentlichen Repos, im
      Gegensatz zum an zwei anderen Stellen im selben Repo (`lorix_link.py._key()`,
      `ukfe_rf.py`s `DEFAULT_SECRET`-Override) konsequent genutzten
      Env-zuerst/gitignored-`secret_local.py`-danach-Muster.
- [x] Fix (additiv): `BROKER_USER`/`BROKER_PASS`-Konstanten entfernt, neue Funktion
      `_creds()` liest `DOGYTAG_MQTT_USER`/`DOGYTAG_MQTT_PASS` aus der Umgebung oder
      `MQTT_USER`/`MQTT_PASS` aus gitignored `secret_local.py` (analog `lorix_link._key()`).
      `_client()` ruft `username_pw_set()` nur noch auf, wenn tatsächlich ein Benutzername
      konfiguriert ist — ohne Konfiguration verbindet sich der Client anonym statt mit dem
      festen Rateversuch. Patch gegen zwei frische, unabhängige Zweitkopien mit
      `git apply --check` verifiziert, real angewendet, byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (`repro.py`, `dogytag_link.py` ALT/NEU direkt von der Platte geladen, `paho.
      mqtt.client` durch einen Aufzeichnungs-Stand-in ersetzt, kein Broker/Netzwerk nötig):
      ALT sendet ohne jede Konfiguration unbedingt `yanis`/`12345678`, NEU verbindet sich im
      selben Fall ohne Zugangsdaten; beide Stände verwenden echte, per Env gesetzte
      Zugangsdaten korrekt, NEU zusätzlich mit dem `secret_local.py`-Fallback geprüft.
- [x] Details, Fund, Fix, Host-Repro, Risiko, Testhinweis, Quelle in
      `app-improvements/g4meover-deck/dogytag-link-hardcoded-credentials-fix.md` (Patch in
      `app-improvements/g4meover-deck/dogytag-link-hardcoded-credentials-fix/dogytag_link_py.patch`,
      Host-Repro in
      `app-improvements/g4meover-deck/repro-dogytag-link-hardcoded-credentials-fix/repro.py`).
- [ ] Lokal zu tun: Patch gegen die eigene `dogytag_link.py` anwenden. Entweder
      `deck/secret_local.py` mit `MQTT_USER`/`MQTT_PASS` anlegen oder
      `DOGYTAG_MQTT_USER`/`DOGYTAG_MQTT_PASS` exportieren (eine davon nötig, falls der reale
      Broker Auth verlangt), danach `python3 deck/dogytag_link.py` ausführen und prüfen, dass
      `detect()` weiterhin `{"ready": True, ...}` liefert. Mit `tcpdump`/`mosquitto`-Logging
      auf Port 1883 bestätigen, dass `yanis`/`12345678` nicht mehr verschickt wird.
- [ ] `g4meover-deck` weiterhin nicht vollständig durchgesehen (`device_discovery.py`,
      `handshake_capture.py`, `hackrf_link.py`, `zigbee_link.py`,
      `wpa_crack.py`/`deck/tools/wpa_crack.py`, `flipper_link.py`) — eigener Rückstellpunkt
      für künftige App-Verbesserungs-Sitzungen. Die übrigen seit längerem zurückgestellten
      Backlog-Punkte (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, `texkom`/`hitagmicro.c`,
      Hitag-S-Phase-2-Integrationsschicht) bleiben unverändert offen.

## Firmware-Feature-Mining: SubGHz "Add Manually" -- Security+-2.0-Transmitter-Leak

- [x] Unleashed-`dev`-Branch erneut gegen den zuletzt geprüften Stand (`af2e764`)
      gegengeprüft (frische, unauthentifizierte Zweitkopie, Tiefe 50): sechs neue Commits,
      davon fünf reine Pflege (Changelog, `fbt format`, Nice-O-Code-Verbesserungen,
      Multi-Frame-Dedupe) oder bereits portiert (FeliCa-Listener-Fix, letzte Sitzung) und ein
      neuer, bisher nicht geprüfter Inhaltscommit: `e1ebaa3` ("SubGHz: safe exit on unknown
      preset, fix Security+ 2.0 transmitter leak", schließt Issues #1159/#1161), acht Dateien,
      zwei eigenständige Fixes gebündelt.
- [x] Beide Teile einzeln gelesen. Den größeren (`subghz_setting_get_inx_preset_by_name()` von
      `furi_crash()` auf `-1`-Rückgabewert, fünf Dateien/Aufrufer anzupassen) als eigenständigen
      Rückstellpunkt für eine künftige Sitzung eingeordnet — mehrteilig genug für einen eigenen
      Beitrag. Den zweiten, vollständig eigenständigen Teil aufgegriffen: in
      `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c` folgen alle acht
      `subghz_txrx_gen_*_protocol()`-Funktionen (für den "Add Manually"-Dialog) demselben
      Muster -- Wegwerf-`SubGhzTransmitter` allozieren, `instance->fff_data` befüllen, danach
      sofort wieder freigeben -- außer `subghz_txrx_gen_secplus_v2_protocol()`: die einzige der
      acht, die den Transmitter nie wieder freigibt.
- [x] Gegen eine frische, unabhängige `G4MEOVER-FW`-Zweitkopie (`mntm-012`, `b359f6f`)
      verifiziert: die Funktion ist byte-identisch zum Unleashed-Vorher-Stand (`e1ebaa3^`).
      Aufrufer (`subghz_scene_set_counter.c`/`subghz_scene_set_type.c`) sowie den Teardown-Pfad
      (`subghz_txrx_free()`) geprüft: jeder erneute Aufruf während einer laufenden
      SubGHz-App-Sitzung (z. B. "Add Manually" → Security+ 2.0 mehrfach hintereinander ohne
      die App zu verlassen) überschreibt `instance->transmitter` mit einer frischen Allokation,
      bevor die vorherige je freigegeben wurde -- ein echter, bei normaler Bedienung wiederholt
      auslösbarer Heap-Leak auf dem speicherknappen STM32WB55.
- [x] Fix (additiv, eine Zeile): `subghz_transmitter_free(instance->transmitter);` unmittelbar
      vor dem bestehenden `return ret;` ergänzt -- gleicht die Funktion exakt an das in
      derselben Datei bereits etablierte Muster der sieben Schwesterfunktionen an. Patch
      (`subghz_txrx_create_protocol_key_c.patch`) gegen zwei frische, unabhängige Zweitkopien
      mit `git apply --check` verifiziert, real angewendet, byte-für-byte abgeglichen --
      identisch.
- [x] Host-Repro (`repro.c`, echte `malloc()`/`free()`-Wrapper um einen Platzhalter-Transmitter,
      ALT/NEU nebeneinander, *n* simulierte "Add Manually"-Durchläufe gegen dieselbe langlebige
      `SubGhzTxRx`-Instanz): unter
      `gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: ALT meldet bei 5 Durchläufen zuverlässig `LeakSanitizer: detected memory
      leaks` ("Direct leak of 20 byte(s) in 5 object(s)", Exit 1), NEU beendet sich in beiden
      Stufen sauber ohne jede Meldung (Exit 0).
- [x] Details, Fund, Fix, Risiko, Port-Skizze, Testhinweis, Host-Repro, Quelle in
      `firmware-mining/secplus-v2-manual-gen-transmitter-leak-fix.md` (Patch in
      `firmware-mining/secplus-v2-manual-gen-transmitter-leak-fix/`, Host-Repro in
      `firmware-mining/repro-secplus-v2-manual-gen-transmitter-leak-fix/`).
- [ ] Lokal zu tun: Patch gegen die eigene `subghz_txrx_create_protocol_key.c` anwenden, mit
      `ufbt`/`fbt firmware_all` bauen. Funktionstest: SubGHz-App → "Add Manually" → "Security+
      2.0" mehrfach hintereinander mit unterschiedlichen Werten bestätigen, ohne die App
      zwischendurch zu verlassen -- funktional (Schlüssel wird weiterhin korrekt erzeugt,
      gespeichert, gesendet) vor/nach dem Fix unverändert; der behobene Leak selbst ist am
      Gerät ohne Heap-Statistik-Tooling nicht direkt beobachtbar (siehe Host-Repro für den
      quantitativen Nachweis).
- [ ] Der größere, zurückgestellte Teil desselben Upstream-Commits ("safe exit on unknown
      preset": `subghz_setting_get_inx_preset_by_name()` + fünf abhängige Dateien/Aufrufer,
      alle gegen meine `mntm-012`-Basis als byte-identisch zum Unleashed-Vorher-Stand
      verifiziert, grundsätzlich portierbar) bleibt eigener Rückstellpunkt für eine künftige
      Sitzung. Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-
      Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-
      Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-
      penetrator`s restliche Ducky-Handler, `texkom`/`hitagmicro.c`, Hitag-S-
      Phase-2-Integrationsschicht, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
      `g4meover-deck`s restliche Module) bleiben unverändert offen.

## Ideen-Recherche: PINGEQUA RF Lab — 2,4-GHz-Spektrumanalysator (neue Kategorie)
- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (SubGHz "Add
      Manually" Security+-2.0-Transmitter-Leak), davor App-Verbesserung (`g4meover-deck`
      dogytag_link.py) und Ideen-Recherche (Sobe) — nach dem etablierten Rotationsmuster war
      Ideen-Recherche wieder an der Reihe. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer bislang komplett fehlenden Kategorie gesucht: ein eigener
      2,4-GHz-Breitband-Spektrumüberblick, Gegenstück zum bereits katalogisierten
      Sub-GHz-`Frequency Analyzer+` — die bestehenden nRF24-Einträge
      (`ideas/nrf24-sniffer-mousejack-api-status.md`) decken nur Sniffing/Mousejack/
      Kanal-Diagnose auf Protokollebene ab, keinen visuellen Spektrum-Scan. **PINGEQUA RF Lab**
      (`pingequalab/rf-lab`, MIT, Hersteller-FAP zum eigenen PINGEQUA-2-in-1-Board) gefunden und
      vollständig gelesen (4770 Zeilen über 29 Dateien: Chip-Arbiter, nRF24-Register-Layer,
      CC1101-Minimaltreiber, Board-Erkennung, Scanner-/Signalgenerator-Szenen, Settings-/
      Log-Persistenz, About-Screen). Aktiv gepflegt, aktueller Release-Tag `v0.5.4`.
- [x] API-87-Status gegen einen frischen, blobfreien Sparse-Checkout von `G4MEOVER18/G4MEOVER-FW`
      (Tag `v2.0.0`, Commit `b359f6f`) verifiziert: alle drei vom Chip-Arbiter angesteuerten
      GPIO-Pins (`gpio_ext_pa4`/`_pb2`/`_pc3`) sowie `furi_hal_spi_bus_handle_external` samt
      aller sechs genutzten SPI-HAL-Funktionen (`_handle_init`/`_deinit`, `_acquire`/`_release`,
      `furi_hal_spi_bus_trx`) byteidentisch vorhanden — dasselbe bereits mehrfach verifizierte
      Muster wie bei SPI Flash Dump/SWD Probe/i2c Tools/CAN Transceiver. Der Chip-Arbiter
      dokumentiert im eigenen Kommentarkopf zudem einen bereits selbst behobenen
      API-Kompatibilitätsfund (`v0.2.1`: nicht exportiertes
      `furi_hal_spi_bus_handle_external_extra`-Handle, seit `v0.2.2` durch das öffentliche
      `furi_hal_spi_bus_handle_external` plus manuell verwaltete CS-Pins ersetzt).
- [x] **Fund**: der dreiseitige About-Screen (`views/about_view.c::draw_page_brand()`) zeigt die
      App-Version als festen Literal-String `"v0.5.3"` an. Per `git log -- views/about_view.c`
      auf den `v0.5.3`-Release-Commit (`16e6752`) zurückverfolgt, seitdem nicht mehr
      aktualisiert — der nachfolgende `v0.5.4`-Release-Commit (`312e6a0`, laut `CHANGELOG.md`
      ein echter Funktionsfix der Signalgenerator-Bus-Arbitrierung) hat `application.fam`s
      `fap_version` auf `(0, 5, 4)` angehoben, die einzige Versions-String-Stelle im gesamten
      Quellbestand aber übersehen (per `grep` über alle Dateien bestätigt: keine andere Datei
      trägt eine Versionsnummer). Gegen das tatsächlich ausgelieferte
      `dist/v0.5.4/pingequa_rf_toolkit.fap`-Binary verifiziert: `strings … | grep -i "v0.5"`
      liefert ausschließlich `v0.5.3` — die aktuell veröffentlichte `.fap`-Datei zeigt auf dem
      Gerät also eine Versionsnummer, die bereits eine Veröffentlichung hinter der tatsächlich
      installierten liegt. Reiner Informationsfund (irreführende Selbstauskunft beim
      Support/Debugging), kein Funktions-/Sicherheitsrisiko — Scanner und Signalgenerator selbst
      sind von diesem String vollständig unberührt.
- [x] Fix (additiv, eine Zeile): `"v0.5.3"` → `"v0.5.4"` in `draw_page_brand()`. Patch
      (`about_view_c.patch`) gegen zwei frische, unabhängige Zweitkopien von `pingequalab/rf-lab`
      mit `git apply --check` verifiziert, real angewendet, byte-für-byte abgeglichen —
      identisch.
- [x] Details, Fund, Fix, Testhinweis, Quelle, Sicherheitsrahmen in
      `ideas/pingequa-rf-lab-2in1-app-status.md` (Patch in
      `ideas/pingequa-rf-lab-stale-about-version-fix/about_view_c.patch`).
      `ideas/pentest-app-katalog.md` um die neue Katalog-Zeile (Kategorie "2.4G / BLE") sowie
      einen Log-Eintrag unter "Nächste Recherche-Aufträge" ergänzt.
- [ ] Lokal zu tun: Patch gegen die eigene `views/about_view.c` anwenden, mit `ufbt launch` neu
      bauen. Funktionstest: `Apps → GPIO → PINGEQUA RF Lab → About`, erste Seite zeigt jetzt
      `v0.5.4` statt `v0.5.3` — Scanner/Signalgenerator/Export/Settings-Persistenz unverändert,
      da der Patch ausschließlich diese eine Zeichenkette betrifft. Zusätzliche Hardware
      (PINGEQUA-2-in-1-Board) für den eigentlichen Funktionstest von Scanner/Signalgenerator
      selbst weiterhin erforderlich, unabhängig von diesem kosmetischen Fix.
- [ ] Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `texkom`/`hitagmicro.c`, Hitag-S-Phase-2-Integrationsschicht, neun
      verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, `g4meover-deck`s restliche Module,
      SubGHz "safe exit on unknown preset" (zurückgestellter größerer Teil desselben
      Upstream-Commits)) bleiben unverändert offen.

## App-Verbesserung: g4meover-deck — zigbee_link.py verwechselt AREQ mit SRSP-Antwort

- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (PINGEQUA RF Lab), davor
      Firmware-Feature-Mining (SubGHz "Add Manually" Security+-2.0-Transmitter-Leak) und
      App-Verbesserung (`g4meover-deck` dogytag_link.py) — nach dem etablierten
      Rotationsmuster (Ideen → App → Firmware → Ideen → App → ...) war App-Verbesserung für
      diese Sitzung an der Reihe. Git-Identität vor dem ersten Commit gesetzt (`G4MEOVER18
      <g4me.over.18@gmail.com>`).
- [x] Frische, unabhängige Zweitkopie von `G4MEOVER18/g4meover-deck` (main, weiterhin Commit
      `b030e45`) geklont. Statt eines der bereits zweimal gefundenen Zähler-Race-Fälle
      (`satellite_link.py`) eines der in `dogytag-link-hardcoded-credentials-fix.md` als
      Rückstellpunkt benannten, noch komplett ungeprüften Module aufgegriffen:
      `device_discovery.py`, `handshake_capture.py` und `hackrf_link.py` vollständig
      gelesen (reine Wrapper um CLI-Tools/pyserial, Eingabe konsequent geklemmt bzw. per
      `subprocess.run()` mit Argument-Liste statt Shell — kein Fund), `wpa_crack.py`
      vollständig gelesen (Hashcat-/hcxpcapngtool-Pipeline, Feldindizes stimmen mit der
      eigenen Formatdokumentation überein, keine Shell-Injektion — kein Fund), danach
      `zigbee_link.py` (CC2531-Koordinator über TI-Z-Stack-ZNP/MT) vollständig gelesen.
- [x] **Fund** in `zigbee_link.py::ZigbeeLink._sreq()`: das ZNP-Rahmenformat kodiert den
      Frame-Typ in den oberen drei Bit von CMD0 (SREQ `0x20`, SRSP `0x60`, AREQ `0x40`) —
      die eigene Kopfzeilen-Dokumentation belegt das bereits selbst
      ("SYS_PING -> fe 02 61 01 79 01 1a", CMD0=`0x61` = SRSP, während das gesendete SREQ
      laut `SYS_PING = (0x21, 0x01)` CMD0=`0x21` trägt). `_sreq()` liest nach dem Senden
      aber einfach den ersten auf `SOF` folgenden Frame vom Draht und hält ihn für die
      Antwort, ohne CMD0/CMD1 gegen die erwartete SRSP-Signatur zu prüfen. Ein
      Z-Stack-Koordinator sendet jederzeit unaufgeforderte AREQs (`SYS_RESET_IND`,
      `ZDO_STATE_CHANGE_IND`, `AF_INCOMING_MSG`, Device-Announce) — läuft eine solche der
      echten SRSP voraus, liest `ping()`/`version()` deren Nutzdaten als falsches, aber
      plausibel aussehendes Ergebnis, ohne jede Fehlermeldung; die echte SRSP bleibt
      unverarbeitet im Puffer und wird erst vom nächsten Aufruf verworfen.
- [x] Fix (additiv, zwei neue Zeilen): erwartetes SRSP-CMD0 aus dem gesendeten Kommando
      berechnet (`(cmd[0] & 0x1F) | 0x60`), vor der bestehenden Rückgabe gegen CMD0/CMD1 des
      eingetroffenen Frames geprüft — bei Nichtübereinstimmung wird der Frame verworfen und
      innerhalb des bestehenden Zeitbudgets weitergelesen statt ihn ungeprüft
      zurückzugeben. Patch (`zigbee_link_py.patch`) gegen zwei frische, unabhängige
      Zweitkopien mit `git apply --check` verifiziert, real angewendet, byte-für-byte
      abgeglichen — identisch.
- [x] Host-Repro (`repro.py`, `_frame()`/`_sreq()` ALT/NEU 1:1 transkribiert, `MockSerial`
      bildet nur das von `_sreq()` genutzte pyserial-Teilinterface nach, kein echtes
      CC2531/Serial nötig): acht Szenarien — Regression ohne Störung (ALT=NEU), der
      eigentliche Fund (AREQ vor echter SRSP: ALT liefert die AREQ-Nutzdaten als falsche
      Antwort, NEU verwirft sie und liefert die echte SRSP), nur AREQ ohne nachfolgende
      SRSP (ALT meldet fälschlich Erfolg, NEU korrekt `None`), mehrere AREQs hintereinander,
      ein anderes Kommando (`SYS_VERSION`) sowie eine fremde SRSP für ein anderes Kommando
      (beide von NEU korrekt verworfen bzw. gefunden), ein Äquivalenztest über 20
      Antwortlängen ohne Störung — 29/29 Einzelprüfungen bestanden, ALLE CHECKS BESTANDEN.
- [x] Details, Fund, Fix, Host-Repro, Risiko, Testhinweis, Quelle in
      `app-improvements/g4meover-deck/zigbee-link-sreq-areq-confusion-fix.md` (Patch in
      `app-improvements/g4meover-deck/zigbee-link-sreq-areq-confusion-fix/zigbee_link_py.patch`,
      Host-Repro in
      `app-improvements/g4meover-deck/repro-zigbee-link-sreq-areq-confusion/repro.py`).
- [ ] Lokal zu tun: Patch gegen die eigene `zigbee_link.py` anwenden. Funktionstest: CC2531
      am Pi anschließen, `ZigbeeLink(port).info()` wiederholt aufrufen während aktivem
      Netzwerkbetrieb (z. B. während ein weiteres Zigbee-Gerät joint) — vor dem Fix können
      einzelne Aufrufe sporadisch ein plausibel aussehendes, aber falsches
      `capabilities`-Bitmuster liefern, nach dem Fix bleibt `info()` entweder korrekt oder
      liefert `reachable: False`/`None` statt einer falschen Antwort. Ohne eingehende AREQs
      (ruhiger Bus) unverändertes Verhalten vor/nach dem Fix.
- [ ] Bewusst nicht mitgefixt (eigener, kleinerer Rückstellpunkt): `_sreq()` prüft weiterhin
      nicht die FCS der eingehenden Antwort selbst — dieser Patch behebt ausschließlich die
      Typverwechslung zwischen SRSP und AREQ. `g4meover-deck` weiterhin nicht vollständig
      durchgesehen (`flipper_link.py` dünner Wrapper, bereits überflogen, aber nicht Zeile für
      Zeile als eigenständiger Fund-Kandidat geprüft; `deck_ctl.py`/`deck_daemon.py`/
      `scenario_runner.py`/`lorix_link.py`/`ukfe_rf.py` als Aufrufer/Kernmodule bereits an
      anderer Stelle mitgeprüft). Die übrigen seit längerem zurückgestellten Backlog-Punkte
      (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, `texkom`/`hitagmicro.c`,
      Hitag-S-Phase-2-Integrationsschicht, neun verbleibende FeliCa-Multisystem-Teilfixe aus
      PR #4465, SubGHz "safe exit on unknown preset") bleiben unverändert offen.

## Firmware-Feature-Mining: SubGHz "safe exit on unknown preset" (zurückgestellter Teil von e1ebaa3)

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      App-Verbesserung (`g4meover-deck` `zigbee_link.py`), davor Ideen-Recherche (PINGEQUA RF
      Lab), davor Firmware-Feature-Mining (SubGHz Security+-2.0-Transmitter-Leak) — nach dem
      etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war
      Firmware-Feature-Mining für diese Sitzung an der Reihe. Git-Identität vor dem ersten
      Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Den in `secplus-v2-manual-gen-transmitter-leak-fix.md` benannten Rückstellpunkt
      aufgegriffen: den größeren, zurückgestellten Teil von Unleashed-Commit `e1ebaa3`
      ("SubGHz: safe exit on unknown preset, fix Security+ 2.0 transmitter leak"). Frische,
      unabhängige Zweitkopie von `DarkFlippers/unleashed-firmware` (`dev`-Branch, Tiefe 30)
      geklont — weiterhin derselbe Commit, kein neuerer Stand seitdem. Vollständigen Diff
      erneut gelesen: `subghz_setting_get_inx_preset_by_name()` wechselt von `furi_crash()`
      auf einen regulären `-1`-Rückgabewert, vier Aufrufer (direkt oder über
      `subghz_setting_get_preset_data_by_name()`) werden entsprechend auf den neuen
      Fehlerfall angepasst.
- [x] Gegen eine frische, unabhängige `G4MEOVER-FW`-Zweitkopie (`mntm-012`, Tag `v2.0.0`,
      Commit `b359f6f`) verifiziert: die fünf betroffenen Dateien existieren alle, haben sich
      im Rahmen (Frequenzlisten/Regionen in `subghz_setting.c`, `tx_from_internal_fff` in
      `subghz_txrx.c`) zwar an anderen Stellen bereits vom geprüften Unleashed-Vorher-Stand
      (`e1ebaa3^`) entfernt — die jeweils betroffenen Funktionen/Codeblöcke selbst
      (`subghz_setting_get_inx_preset_by_name()`, `subghz_setting_get_preset_data_by_name()`,
      der Preset-Datenblock in `subghz_txrx_tx_start()`, `subghz_txrx_rx_start()` komplett,
      der `preset_index`-Block in `subghz_key_load()` und in
      `subghz_scene_receiver_config_var_list_enter_callback()`) sind byte-identisch zum
      Unleashed-Vorher-Stand. Per `grep` über den gesamten Quellbestand zusätzlich bestätigt:
      keine weiteren, hier nicht behandelten Aufrufer der beiden geänderten Funktionen.
- [x] Fünf additive Patches erstellt (keine zwei überlappen sich): `furi_crash()` →
      `FURI_LOG_E()` plus `-1`-Rückgabe in `subghz_setting_get_inx_preset_by_name()`;
      `NULL`-Prüfung vor dem Array-Zugriff in `subghz_setting_get_preset_data_by_name()`;
      Doc-Kommentare in `subghz_setting.h`; `preset_data`-Zwischenvariable plus
      `NULL`-Prüfung in `subghz_txrx_tx_start()`/`subghz_txrx_rx_start()`
      (`subghz_txrx.c`); `size_t` → `int` plus `-1`-Abbruch in `subghz_key_load()`
      (`subghz_i.c`, identisches `break`-Muster wie die zehn bereits vorhandenen
      Fehlerpfade derselben Funktion); Default-Index-Fallback in
      `subghz_scene_receiver_config_var_list_enter_callback()`. Alle fünf Patches gegen
      zwei frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real
      angewendet, byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (`repro.c`, `subghz_setting_get_inx_preset_by_name()`/
      `subghz_setting_get_preset_data_by_name()` sowie stellvertretend für die vier echten
      Aufrufer das `subghz_txrx_rx_start()`-Muster ALT/NEU nebeneinander, `furi_crash()`
      exakt wie im bestehenden `stub_furi_crash.c`-Muster dieses Repos durch `_exit(42)`
      nachgebildet): unter
      `gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: bekannter Preset-Name liefert in ALT und NEU identisch Exit 0 (RX
      gestartet); unbekannter Preset-Name löst in ALT `furi_crash()` aus (Exit 42, Prozess
      stirbt), in NEU wird der Fehler geloggt und der Vorgang kontrolliert abgebrochen
      (Exit 0, Prozess bleibt am Leben) — vier aufeinanderfolgende Läufe je Kombination
      deterministisch reproduziert, keine Compiler-/Sanitizer-Warnungen in beiden Stufen.
- [x] Details, Fund, Fix, Risiko, Port-Skizze, Testhinweis, Host-Repro, Quelle in
      `firmware-mining/subghz-safe-exit-unknown-preset.md` (fünf Patches in
      `firmware-mining/subghz-safe-exit-unknown-preset/`, Host-Repro in
      `firmware-mining/repro-subghz-safe-exit-unknown-preset/`). Damit ist der gesamte
      Inhalt von Unleashed-Commit `e1ebaa3` jetzt vollständig in diesem Repo erfasst (beide
      Teile portiert).
- [ ] Lokal zu tun: alle fünf Patches gegen die eigene Firmware anwenden (Reihenfolge
      unabhängig, keine Überlappung), mit `ufbt`/`fbt firmware_all` bauen. Funktionstest:
      eine gespeicherte `.sub`-Datei nehmen, das `Preset:`-Feld auf einen erfundenen Namen
      setzen, zurück nach `SubGHz/` kopieren und im SubGHz-App-Hauptmenü öffnen — vor dem
      Fix stürzt die Firmware beim Öffnen/Senden ab (Reset), nach dem Fix erscheint ein
      Fehler-Log-Eintrag, die App bleibt bedienbar. Für alle regulären, mit real
      existierenden Presets gespeicherten Dateien funktional unverändert vor/nach dem Fix.
- [ ] Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `texkom`/`hitagmicro.c`, Hitag-S-Phase-2-Integrationsschicht, neun
      verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, `g4meover-deck`s restliche
      Module (`flipper_link.py`-Bestätigung)) bleiben unverändert offen.

## Ideen-Recherche: TV Remote (jmanion0139) — neue Kategorie "Infrarot (Fernbedienung)"
- [x] `reports/` durchgesehen: letzte Sitzung (`2026-10-15-1228.md`) war Firmware-Feature-Mining
      (SubGHz "safe exit on unknown preset", zurückgestellter Teil von `e1ebaa3`), davor
      App-Verbesserung (`g4meover-deck` zigbee_link.py) und Ideen-Recherche (PINGEQUA RF Lab) —
      nach dem etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → Ideen → ...) war
      Ideen-Recherche für diese Sitzung an der Reihe. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer bislang komplett fehlenden Kategorie gesucht: eine eigene
      Infrarot-Fernbedienungs-App, Gegenstück zur bisher nur über Gegenüberwachung (Nyx/Sobe)
      abgedeckten IR-Erkennungsseite. **TV Remote** (`jmanion0139/flipper_tv_remote`, GPL-3.0,
      Tag `v1.1`, Commit `9ce9d3e`) gefunden und vollständig gelesen (2390 Zeilen über sechs
      Quelldateien: App-Lebenszyklus/Dateiformat, Lern-Ansicht, Fernbedienungs-Ansicht).
- [x] API-87-Status gegen einen frischen, blobfreien Sparse-Checkout von `G4MEOVER18/G4MEOVER-FW`
      (Tag `v2.0.0`, Commit `b359f6f`, `lib/infrared`/`lib/flipper_format`/`lib/toolbox`)
      verifiziert: alle genutzten `infrared_worker_*`/`infrared_get_protocol_*`/
      `flipper_format_*`/`saved_struct_*`-Symbole byteidentisch vorhanden — reine GUI-/
      Infrarot-/Storage-API, kein Erweiterungsport, kein Zusatzmodul nötig (nur eingebauter
      IR-Blaster/-Empfänger).
- [x] **Fund**: `tv_remote_ir_burst()` (`views/tv_remote_remote.c`) ruft `tv_remote_tx_start()`
      und prüft danach pauschal `app->tx_active`, ohne zu unterscheiden, ob der Start tatsächlich
      für die angeforderte Taste gegriffen hat oder wegen des `worker_active`-Guards sofort
      abgebrochen wurde. Ist bei aktiver Einstellung "Hold TX: Continuous" bereits eine
      Dauer-Halten-Sendung für eine andere Taste aktiv (z. B. Up gehalten → Vol_up-Dauersendung),
      bricht ein Back-Tipp oder ein per Doppeltipp ausgelöster Power-Befehl diese zwar vorzeitig
      ab, sendet dabei aber selbst nie das angeforderte Signal — obwohl die UI es bereits als
      ausgelöst (Ring eingefärbt) anzeigt. Kein konstruierter Randfall: mit aktiviertem
      "Continuous"-Modus ist "während des Lautstärke-/Senderkanal-Haltens zu Back/Power
      wechseln, ohne vorher loszulassen" eine naheliegende Bediensituation.
- [x] Fix (additiv, in `tv_remote_ir_burst()`): vor dem eigentlichen Start eine bereits für eine
      ANDERE Taste laufende Übertragung explizit über das vorhandene `tv_remote_tx_stop()`
      beenden, bevor `tv_remote_tx_start()` für die neue Taste aufgerufen wird. Patch gegen zwei
      frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real angewendet,
      byte-für-byte abgeglichen — identisch.
- [x] Host-Repro (`repro.c`): `tv_remote_tx_start()`/`_tx_stop()`/`_ir_burst()` 1:1 aus dem
      Quelltext übernommen (IR-Hardware-Aufrufe durch einen Sende-Log-Zähler ersetzt). Vier
      Szenarien (Regression, Back während laufendem Continuous-Hold, Power-Doppeltipp während
      laufendem Continuous-Hold, Äquivalenztest für den praktisch unerreichbaren Gleichtasten-Fall)
      unter `gcc -fsanitize=address,undefined` bei `-O0`/`-O1`: ALT sendet in beiden
      Fund-Szenarien nachweislich 0 Back-/Power-Signale, NEU sendet in beiden Fällen korrekt —
      ALLE CHECKS BESTANDEN in beiden Stufen, keine Compiler-/Sanitizer-Warnungen.
- [x] Details, Fund, Fix, Host-Repro, Testhinweis, Quelle, Sicherheitsrahmen in
      `ideas/flipper-tv-remote-app-status.md` (Patch in
      `ideas/flipper-tv-remote-back-power-burst-during-continuous-tx-fix/
      views_tv_remote_remote_c.patch`, Host-Repro in
      `ideas/repro-flipper-tv-remote-back-power-burst-during-continuous-tx/repro.c`).
      `ideas/pentest-app-katalog.md` um die neue Kategorie "Infrarot (Fernbedienung)" sowie einen
      Log-Eintrag unter "Nächste Recherche-Aufträge" ergänzt.
- [ ] Lokal zu tun: Patch gegen die eigene Kopie von `flipper_tv_remote` anwenden, mit `ufbt`
      bauen. Funktionstest: Settings → "Hold TX: Continuous" aktivieren, eine gelernte
      Fernbedienung öffnen, eine D-Pad-Richtung gedrückt HALTEN (löst die Dauer-Sendung der
      Hold-Aktion aus), währenddessen kurz auf Back tippen — vor dem Fix bricht die
      Dauer-Sendung ab, ohne dass am Zielgerät ein Back-Befehl ankommt; nach dem Fix wird Back
      tatsächlich gesendet. Mit "Hold TX: Single" (Standard) nicht reproduzierbar.
- [ ] Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, `texkom`/`hitagmicro.c`, Hitag-S-Phase-2-Integrationsschicht, neun
      verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, `g4meover-deck`s restliche
      Module (`flipper_link.py`-Bestätigung), SubGHz "safe exit on unknown preset"
      (zurückgestellter größerer Teil desselben Upstream-Commits, inzwischen vollständig
      portiert)) bleiben unverändert offen.

## App-Verbesserung: g4meover-deck — flipper_link.py::find_flipper() akzeptiert im Fallback jedes ttyACM*-Gerät ungeprüft

- [x] `reports/` durchgesehen: letzte Sitzung (`2026-10-15-1827.md`) war Ideen-Recherche
      (TV Remote, neue Kategorie "Infrarot (Fernbedienung)"), davor Firmware-Feature-Mining
      (SubGHz "safe exit on unknown preset") und App-Verbesserung (`g4meover-deck`
      zigbee_link.py) — nach dem etablierten Rotationsmuster (Firmware → Ideen → App →
      Firmware → ...) war App-Verbesserung für diese Sitzung an der Reihe. Git-Identität vor
      dem ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Frische, unabhängige Zweitkopie von `G4MEOVER18/g4meover-deck` (main, weiterhin Commit
      `b030e45`) geklont. Statt eines erneuten Durchlaufs über bereits mehrfach geprüfte
      Module den in `zigbee-link-sreq-areq-confusion-fix.md` als letzten offenen Rückstellpunkt
      benannten `flipper_link.py` aufgegriffen (bislang nur als "dünner Wrapper" überflogen,
      nicht Zeile für Zeile geprüft) und vollständig gelesen (71 Zeilen).
- [x] **Fund** in `find_flipper()`: erste Stufe (`device_discovery.find_port`, korrekter
      VID:PID-Abgleich) ist von einem breiten `except Exception` umgeben; schlägt sie fehl
      (z. B. `device_discovery` nicht im `sys.path`, `pyserial` fehlt, sonstige Ausnahme),
      liefert die zweite Stufe ungeprüft den alphabetisch ersten `/dev/ttyACM*`-Pfad zurück —
      ohne jede Identitätsprüfung. `device_discovery.KNOWN` dokumentiert selbst, dass sowohl
      der CC2531-Zigbee-Koordinator (VID `0x0451`) als auch der LilyGo T-Dongle S3 (VID
      `0x303A`, eigener Code-Hinweis `"native-USB-CDC remote still (S3-Quirk)"`) unter Linux
      ebenfalls als `/dev/ttyACM*` erscheinen können — im Vollbetrieb des Ökosystems (mehrere
      Geräte gleichzeitig am Pi) der Normalfall. Liegt eines dieser Geräte alphabetisch vor dem
      Flipper und schlägt `device_discovery` fehl, öffnet `FlipperLink` den falschen Port und
      sendet flipperbridge-RPC-Bytes an ein Gerät, das sie nicht versteht — ohne erkennbaren
      Grund für den Fehlschlag an dieser Stelle. Nebenfund: `sorted(glob.glob(...))` sortiert
      rein lexikografisch, ab zweistelligen Portnummern in falscher Reihenfolge
      (`"/dev/ttyACM10"` < `"/dev/ttyACM2"`).
- [x] Fix (additiv, neue Zwischenstufe zwischen den beiden bestehenden): direkter
      `serial.tools.list_ports.comports()`-VID:PID-Abgleich gegen die bereits in
      `device_discovery.KNOWN` hinterlegte Flipper-Kennung (`0x0483:0x5740`) — greift auch,
      wenn `device_discovery` selbst nicht verfügbar ist, solange nur `pyserial` installiert
      ist. Erst wenn auch das fehlschlägt, der bisherige blinde Glob-Fallback, jetzt
      längenbewusst statt rein lexikografisch sortiert. Patch gegen zwei frische, unabhängige
      Zweitkopien mit `git apply --check` verifiziert, real angewendet, byte-für-byte
      abgeglichen — identisch. Mit `python3 -m py_compile` geprüft.
- [x] Host-Repro (`repro.py`, `find_flipper()` ALT/NEU 1:1 transkribiert, `device_discovery`
      und `serial.tools.list_ports` als Fake-Module in `sys.modules` eingehängt bzw. bewusst
      weggelassen für ein echtes `ModuleNotFoundError` — kein echter Flipper, kein echtes
      `pyserial` nötig): sieben Szenariengruppen — Regression (device_discovery findet
      Flipper direkt), der eigentliche Fund (device_discovery fehlt, CC2531 vor Flipper auf
      ttyACM*: ALT liefert den falschen Port, NEU den Flipper), dieselbe Fund-Variante mit
      LilyGo T-Dongle S3, der dokumentierte Grenzfall (weder device_discovery noch pyserial
      verfügbar: ALT=NEU, keine Verschlechterung), der Sortier-Nebenfund bei zweistelligen
      Portnummern, kein Gerät angeschlossen, ein Äquivalenztest über 20 von device_discovery
      direkt gelieferte Ports — 26/26 Einzelprüfungen bestanden, ALLE CHECKS BESTANDEN.
- [x] Details, Fund, Fix, Host-Repro, Risiko, Testhinweis, Quelle in
      `app-improvements/g4meover-deck/flipper-link-unverified-fallback-fix.md` (Patch in
      `app-improvements/g4meover-deck/flipper-link-unverified-fallback-fix/flipper_link_py.patch`,
      Host-Repro in
      `app-improvements/g4meover-deck/repro-flipper-link-unverified-fallback/repro.py`).
- [ ] Lokal zu tun: Patch gegen die eigene `flipper_link.py` anwenden. Funktionstest: Flipper
      und CC2531 (oder LilyGo T-Dongle S3) gleichzeitig am Pi anschließen, `device_discovery.py`
      kurzzeitig aus dem `sys.path` entfernen (simuliert unvollständige `install.sh`-Installation),
      `FlipperLink().info()` aufrufen — vor dem Fix kann je nach alphabetischer ttyACM-Reihenfolge
      das falsche Gerät gewählt werden, nach dem Fix immer der per VID:PID identifizierte
      Flipper-Port. Mit vorhandenem `device_discovery.py` (Normalfall) unverändertes Verhalten.
- [ ] Bewusst nicht mitgefixt (eigener, kleinerer Rückstellpunkt): schließt den blinden
      Glob-Fallback (dritte Stufe) nicht komplett — fehlt auch `pyserial` komplett, bleibt das
      bisherige ungeprüfte Verhalten (dokumentierte, bewusst in Kauf genommene Grenze für
      diesen seltenen Fall). Die übrigen seit längerem zurückgestellten Backlog-Punkte
      (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, `texkom`/`hitagmicro.c`,
      Hitag-S-Phase-2-Integrationsschicht, neun verbleibende FeliCa-Multisystem-Teilfixe aus
      PR #4465) bleiben unverändert offen. `g4meover-deck` ist damit vollständig durchgesehen
      (alle Module mindestens einmal gelesen).

## Firmware-Feature-Mining: ID82xx/Hitag-micro-Magic-Chip-Schreiber (`hitagmicro.c`/`.h`)

- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (`g4meover-deck`
      `flipper_link.py`), davor Ideen-Recherche (TV Remote) und Firmware-Feature-Mining
      (SubGHz "safe exit on unknown preset") — nach dem etablierten Rotationsmuster
      (Firmware → Ideen → App → Firmware → ...) war Firmware-Feature-Mining für diese
      Sitzung an der Reihe. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Aus derselben seit der Doorbell32-Sitzung offenen `unlshd-093`→`dev`-Restliste
      (`keyfinder2`/`texkom`/`hitags`/`hitagmicro`) den in `hitags-lfrfid-magic-tag-port.md`
      als eigenen Rückstellpunkt benannten zweiten Kandidaten derselben BPLM-Timing-Familie
      aufgegriffen: `lib/lfrfid/tools/hitagmicro.c`/`.h` (327+57 Zeilen). Gegen den
      `dev`-Branch erneut geprüft — jetzt Commit `f62964c` (zuletzt `af2e764`; einziger
      Commit seither, der die Dateien berührt, ist ein reiner `fmt`-Lauf, Dateigröße
      unverändert — keine inhaltliche Änderung, keine Wiederholung).
- [x] Kompatibilitätsprüfung gegen eine frische `G4MEOVER-FW`-Zweitkopie (`b359f6f`):
      `furi_hal_rfid_*`-Timer-Interface und `bit_lib_set_bit`/`_get_bit`/`_reverse_16_fast`
      byteidentisch vorhanden; anders als `hitags.c` ruft `hitagmicro.c` nie
      `tim_read_capture_*` auf (rein open-loop Schreiben, kein UID-Lesen, keine
      Anticollision). Dieselbe Integrationslücke wie bei `hitags.c` (kein
      Mehrfachziel-Dispatch in `lfrfid_worker.h`) bestätigt — kein Port-Hindernis für das
      Werkzeugmodul selbst.
- [x] Zwei additive Patches (`hitagmicro_c.patch`/`hitagmicro_h.patch`, Dateien unverändert
      aus dem Unleashed-Quelltext übernommen) gegen zwei unabhängige, frische
      `G4MEOVER-FW`-Zweitkopien mit `git apply --check` verifiziert (beide Reihenfolgen
      geprüft), real angewendet, per `diff -r` und `md5sum` gegen den Unleashed-Quelltext
      abgeglichen — identisch.
- [x] Host-Repro (abweichend von der reinen Transkriptions-Methode, da `hitagmicro.c` —
      anders als `hitags.c` — keinen eigenen Selbsttest mitbringt): die tatsächliche,
      unveränderte `hitagmicro_write()` gegen einen aufzeichnenden `furi_delay_us()`-Host-Shim
      aufgerufen, der jedes gesendete Bit aus der Dauer-Sequenz rekonstruiert (jedes Bit
      läuft vor jeder anderen Aktion durch `furi_delay_us(96|160|288|64)`), ohne eine
      einzige modul-interne `static`-Funktion direkt anzufassen. Die rekonstruierten Frames
      gegen eine unabhängig geschriebene zweite CRC-16-/Frame-Builder-Implementierung
      geprüft (gleicher Algorithmus, strukturell andere Umsetzung: bitweise statt
      Byte-Realignment). Drei Testläufe (je eine Chip-Variante, unterschiedliche
      Nutzdaten): 9/9 Feld-Zyklen, 18/18 Frames bitgenau, Schreibreihenfolge
      block0→block1→config aus dem tatsächlichen Aufruf bestätigt, Varianten-Tabelle und
      CRC-Randfall (`bitlength==0`) geprüft. Differenzielle Gegenprobe: zwei mutierte
      Kopien (vertauschtes CRC-Polynom, vertauschte Schreibreihenfolge) ließen das Repro
      wie erwartet mit 57 bzw. 21 Prüfungen fehlschlagen — keine falsch-grüne Prüfung.
      Unter `gcc -fsanitize=address,undefined` bei `-O0`/`-O1`: `ALLE CHECKS BESTANDEN` in
      beiden Stufen, identischer Output, keine ASan-/UBSan-Meldungen (abgesehen von zwei
      bereits dokumentierten harmlosen Compiler-Warnungen außerhalb des geprüften Pfads).
- [x] Details, Kompatibilitätsprüfung, Fund, Patches, Host-Repro, Risiko, Port-Skizze,
      Testhinweis in `firmware-mining/hitagmicro-lfrfid-tool-port.md` (Patches in
      `firmware-mining/hitagmicro-lfrfid-tool-port/`, Host-Repro in
      `firmware-mining/repro-hitagmicro-lfrfid-tool-port/`).
- [ ] Lokal zu tun: beide Patches gegen die eigene `mntm-012`-Basis anwenden, mit
      `ufbt`/`fbt firmware_all` bauen (reiner Compile-Test ohne Integrationsschicht — noch
      kein Aufrufer); Host-Repro lokal nachvollziehen. Hardware-Test erst nach der
      Integrationsschicht sinnvoll (siehe Port-Skizze).
- [ ] Integrationsschicht bleibt jetzt gemeinsamer Rückstellpunkt für `hitags.c` UND
      `hitagmicro.c` (dieselbe fehlende `LFRFIDWriteType`-Dispatch-Lücke, zwei Werkzeuge
      warten jetzt darauf — siehe Abwägung in `hitagmicro-lfrfid-tool-port.md`), ebenso
      `texkom` aus derselben `dev`-Branch-Restliste (größte, mehrteilige NFC-Protokollfamilie,
      eigener Rückstellpunkt). Die übrigen seit längerem zurückgestellten Backlog-Punkte
      (NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, neun verbleibende
      FeliCa-Multisystem-Teilfixe aus PR #4465) bleiben unverändert offen.

## Ideen-Recherche: Sentry Safe Opener (H4ckd4ddy) — elektronischer Tresor-Bypass

- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (`hitagmicro.c`/`.h`),
      davor App-Verbesserung (`g4meover-deck` `flipper_link.py`) und Ideen-Recherche
      (TV Remote) — nach dem etablierten Rotationsmuster (Firmware → Ideen → App →
      Firmware → Ideen → ...) war Ideen-Recherche für diese Sitzung an der Reihe.
      `ideas/pentest-app-katalog.md` auf verbleibende ❓-Einträge geprüft: keine offen mehr
      (vollständig durchgesehen) — stattdessen gezielt nach einer bislang komplett
      fehlenden Kategorie gesucht (Schloss-/Aktuator-Seite der physischen Zutrittskontrolle,
      Gegenstück zur bisher nur kartenleserseitig abgedeckten Wiegand-Reader-/H10301-
      Writer-Kategorie).
- [x] `H4ckd4ddy/flipperzero-sentry-safe-plugin` (MIT, `fap_version="2.0"`, bereits Teil der
      Unleashed-/Xtreme-Standard-App-Sammlungen) gefunden und vollständig gelesen (einzige
      Quelldatei `sentry_safe.c`, 333 Zeilen): GPIO-Bypass elektronischer Sentry-Safe-/
      Master-Lock-Tresorschlösser über die intern abgegriffene 4800-Baud-Steuerleitung
      (Weck-Puls + 8-Byte-Rahmen), überschreibt den hinterlegten Code ohne ihn zu kennen.
      API-87-Status gegen einen frischen, unabhängigen Checkout von `G4MEOVER18/G4MEOVER-FW`
      (Tag `v2.0.0`, Commit `b359f6f`, `mntm-012`) verifiziert: `gpio_ext_pc1` byteidentisch
      vorhanden und zugleich firmwareseitig bereits als LPUART-TX-Pin hinterlegt — der
      App-eigene Wechsel zwischen rohem GPIO- und UART-Peripherie-Modus auf demselben Pin ist
      damit beabsichtigtes Pin-Mapping, kein Bug. `furi_hal_gpio_*`/`furi_hal_serial_*`/
      `expansion_disable()`/`_enable()` byteidentisch vorhanden (dieselben bereits bei UART
      Sniff/RS485 Modbus/SWD Probe/CAN Commander verifizierten Symbole bzw. — im Gegensatz zum
      CAN-Commander-Fund — hier korrekt verwendet).
- [x] Eine Code-Beobachtung notiert, aber bewusst **nicht** als Fund mit Host-Repro geführt:
      doppelter `furi_mutex_release()`-Aufruf im Sende-Pfad. Gegen `furi/core/mutex.c` der
      eigenen `mntm-012`-Basis geprüft — ein zweiter Aufruf auf einem bereits freigegebenen,
      nicht-rekursiven Mutex liefert dort nur `errQUEUE_FULL` zurück (`xSemaphoreGive()` auf
      volle Queue), kein `furi_check()`/`furi_crash()` auf diesem Pfad. Kein reproduzierbares
      Fehlverhalten — daher ohne Fix-Vorschlag/Host-Repro dokumentiert, um keinen
      unbestätigten Fund zu behaupten.
- [x] Details, Quelle, Protokollbeschreibung, API-87-Status, Testhinweis, Sicherheitsrahmen in
      `ideas/sentry-safe-electronic-lock-bypass-app-status.md`. Katalog-Zeile in
      `ideas/pentest-app-katalog.md` (Abschnitt "Physical Access Control (Wiegand/GPIO)")
      ergänzt.
- [ ] Lokal zu tun: App unverändert gegen die eigene `mntm-012`-Basis mit `ufbt`/`fbt` bauen
      (reiner Compile-Test). Funktionstest ausschließlich am eigenen Sentry-Safe-/Master-Lock-
      Tresor (autorisierter Eigentest, siehe Testhinweis im Dokument).

## App-Verbesserung: g4meover-deck — wpa_crack.show_cracked() trunkiert Passwörter mit ':'

- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (Sentry Safe Opener),
      davor Firmware-Feature-Mining (`hitagmicro.c`/`.h`) und App-Verbesserung
      (`g4meover-deck` `flipper_link.py`) — nach dem etablierten Rotationsmuster
      (Firmware → Ideen → App → Firmware → Ideen → App → ...) war App-Verbesserung für
      diese Sitzung an der Reihe. Von den in `dogytag-link-hardcoded-credentials-fix.md`
      als Rückstellpunkt benannten, noch ungeprüften `g4meover-deck`-Modulen
      (`device_discovery.py`, `handshake_capture.py`, `hackrf_link.py`, `lorix_link.py`,
      `wpa_crack.py` — `flipper_link.py`/`zigbee_link.py`/`dogytag_link.py`/
      `satellite_link.py` bereits geprüft) `wpa_crack.py` gewählt (schließt den
      Handshake-Capture→Cracking-Kreis, höchste Sicherheitsrelevanz: ein falsch
      gemeldetes "geknacktes" Passwort ist für ein Pentest-Werkzeug ein echter
      Vertrauensbruch).
- [x] Frische, unabhängige Zweitkopie von `G4MEOVER18/g4meover-deck` geklont (main, Commit
      `b030e45` — identischer Stand wie bei der vorigen `zigbee_link.py`-Prüfung).
      `deck/wpa_crack.py` (158 Zeilen) sowie `deck/handshake_capture.py` (97 Zeilen, direkter
      Vorlieferant der `.pcap`-Dateien für `wpa_crack.py`) vollständig gelesen.
      `handshake_capture.py` ohne Fund: `_pcap_global_header()`/`_pcap_record()` erzeugen ein
      standardkonformes klassisches `.pcap` (Magic `0xA1B2C3D4`, Version 2.4,
      `LINKTYPE_IEEE802_11`), `parse_hsraw()` filtert sinnvoll auf gerade, mindestens
      24-stellige Hex-Strings.
- [x] Fund in `wpa_crack.py::show_cracked()`: Extraktion des geknackten Klartext-Passworts
      aus `hashcat -m 22000 ... --show` per `line.rsplit(":", 1)[-1]` — trennt am
      **letzten** `:` der Zeile. Der Hash-Teil selbst (hc22000-Format, `*`-getrennte Felder)
      enthält nie `:`, aber WPA-PSK-ASCII-Passphrasen dürfen laut IEEE 802.11i jedes
      druckbare ASCII-Zeichen 0x20–0x7E (8–63 Zeichen) enthalten, Doppelpunkt eingeschlossen
      — ein völlig gültiges, nicht einmal ungewöhnliches Passwortzeichen. Bei einem
      Passwort mit `:` (z. B. `pass:word`, `a:b:c:super:geheim`) liefert `show_cracked()`
      nur das Fragment hinter dem letzten `:` zurück (`"word"` bzw. `"geheim"`), ohne
      jede Fehlermeldung — der Aufrufer (`crack_dictionary()`/`crack_mask()`, letztlich die
      Dashboard-Anzeige) hält das Fragment für das vollständige Passwort. hashcat selbst
      schneidet nichts ab (im Potfile bleibt der volle Treffer korrekt gespeichert) — der
      Verlust passiert ausschließlich in dieser Python-Extraktion.
- [x] Fix (additiv, eine Zeile): `line.rsplit(":", 1)[-1]` durch `line.split(":", 1)[1]`
      ersetzt (Trennung am **ersten** `:` statt am letzten) — korrekt, weil der Hash-Teil
      vor dem ersten `:` nie selbst `:` enthält, alles danach (inklusive weiterer `:`) ist
      das vollständige Passwort. Für den Normalfall (Passwort ohne `:`) verhalten sich
      `rsplit(":", 1)[-1]` und `split(":", 1)[1]` identisch — reine Erweiterung, kein
      verändertes Verhalten im bisher korrekten Fall.
- [x] Patch (`wpa_crack_py.patch`) gegen zwei frische, unabhängige Zweitkopien mit
      `git apply --check` verifiziert, real angewendet, per `md5sum` abgeglichen —
      identisch.
- [x] Host-Repro (`repro.py`, ohne hashcat/hcxtools): `show_cracked()`s betroffene
      Schleife 1:1 als `_parse_show_ALT()`/`_parse_show_NEU()` transkribiert, `_fmt_mac()`
      unverändert aus der Originaldatei importiert. Eingabe ist exakt der Text, den
      `hashcat --show` auf stdout schreibt. 13 Einzelprüfungen: Regression ohne `:`
      (ALT/NEU identisch), Fund mit einem `:` (ALT trunkiert, NEU vollständig), verschärfter
      Fall mit mehreren `:`, zwei Zeilen in einer `--show`-Ausgabe, `ssid`/`bssid`-
      Extraktion über alle Fälle unverändert, Randfall ganz ohne `:` (kein Crash),
      Mutationstest-Gegenprobe (mutierte NEU-Variante zurück auf `rsplit` reproduziert den
      Fund erneut — Repro ist scharf). 13/13 bestanden, `ALLE CHECKS BESTANDEN`.
- [x] Details, Fund, Patch, Host-Repro, Risiko, Testhinweis in
      `app-improvements/g4meover-deck/wpa-crack-show-cracked-colon-password-truncated-fix.md`
      (Patch in `wpa-crack-show-cracked-colon-password-truncated-fix/`, Host-Repro in
      `repro-wpa-crack-show-cracked-colon-password-truncated/`).
- [ ] Lokal zu tun: Patch gegen die eigene `g4meover-deck`-Kopie anwenden. Funktionstest mit
      echtem hashcat (siehe Testhinweis im Dokument: `.hc22000`-Potfile-Eintrag mit `:` im
      Passwort anlegen, `show_cracked()` vorher/nachher vergleichen) — keine Flipper-
      Hardware nötig.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `device_discovery.py`,
      `hackrf_link.py`, `lorix_link.py` aus derselben `g4meover-deck`-Modulliste noch
      ungeprüft; Integrationsschicht für `hitags.c`/`hitagmicro.c`, `texkom`,
      NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, neun verbleibende FeliCa-Multisystem-
      Teilfixe aus PR #4465.

## Firmware-Feature-Mining: Hitag S (`hitags.c`) in den LFRFID-Schreib-Dispatch eingehängt

- [x] Rückstellpunkt aus `hitags-lfrfid-magic-tag-port.md`/`hitagmicro-lfrfid-tool-port.md`
      ("Integrationsschicht fehlt, Option (a) generisches Dispatch vs. (b) eigener Pfad")
      am tatsächlichen Quelltext von `mntm-012` (frische Zweitkopie, `b359f6f`) geprüft statt
      an der Erinnerung an den `dev`-Branch-Vorschlag: die generische Dispatch-
      Infrastruktur (`LFRFIDWriteType`-Enum, `LFRFIDWriteRequest`-Union in
      `lfrfid_protocols.h`, Mehrfachziel-Schleife in
      `lfrfid_worker_modes.c::lfrfid_worker_mode_write_process()`) existiert bereits in der
      eigenen Basis — Option (a) ist nicht nachzubauen, sondern nur um einen Eintrag zu
      erweitern. Korrigiert die in den beiden vorigen Sitzungen dokumentierte Annahme.
- [x] `LFRFIDWriteTypeHitagS` ergänzt (`lfrfid_protocols.h`: Enum-Eintrag, Union-Feld,
      `#include` für `tools/hitags.h`), Dispatch in `lfrfid_worker_modes.c` ergänzt
      (`hitags_read_uid()` vor `hitags_write()`, Fehlschlag wird wie "kann nicht geschrieben
      werden" behandelt, kein Crash), `protocol_em4100.c::protocol_em4100_write_data()` um
      die MSB-first-Zerlegung von `encoded_data` in `page4`/`page5` erweitert (gilt wegen
      gemeinsamer Funktion auch für `EM410032`/`EM410016`).
- [x] Jede der 20 anderen Protokoll-Dateien, die `LFRFIDWriteRequest`/`write_type` lesen,
      einzeln gegen den neuen Enum-Wert geprüft: alle vergleichen per Namen mit
      `result = false` als Vorgabe — keine verändert sich durch diesen Patch, keine
      implizite Positionsannahme über die alte `LFRFIDWriteTypeMax`-Größe gefunden.
- [x] Feldzustand zwischen den Dispatch-Zweigen direkt an `t5577_write()`/`em4305_write()`s
      Quelltext nachvollzogen (nicht angenommen): beide hinterlassen das 125-kHz-Feld aus —
      genau die von `hitags_read_uid()` dokumentierte Voraussetzung.
- [x] Drei Patches (`lfrfid_protocols_h.patch`, `lfrfid_worker_modes_c.patch`,
      `protocol_em4100_c.patch`) gegen zwei frische, unabhängige Zweitkopien (auf Basis der
      bereits vorliegenden `hitags_h.patch`/`hitags_c.patch`) mit `git apply --check`
      verifiziert, real angewendet, per `diff -r`/`md5sum` abgeglichen — identisch.
- [x] Host-Repro: echte, unveränderte Header (`lfrfid_protocols.h` mit dieser Änderung,
      `t5577.h`, `em4305.h`, `hitags.h`) zusammen übersetzt (`repro_main.c`) — keine
      Namenskollision, `LFRFIDWriteTypeMax` wächst korrekt auf 3. Neue `page4`/`page5`-
      Zerlegung gegen die bereits produktive T5577-Formel für 8 Testvektoren verglichen
      (`bitcheck.c`), zwei Mutationstest-Gegenproben (vertauschte Shift-Reihenfolge,
      vertauschte Seitenzuordnung) korrekt als Fehlschlag erkannt. `gcc -std=c17 -Wall
      -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
      `-O0`/`-O1`: beide Prüfungen fehlerfrei in beiden Stufen.
- [x] Details, Befund, Betroffene Dateien, Risiko (insbesondere die bei jedem Schreibzyklus
      zusätzlich anfallende `hitags_read_uid()`-Latenz, auch wenn der aufgelegte Tag kein
      Hitag S ist — aus dem tatsächlichen Schleifencode abgeleitet, nicht hypothetisch),
      Port-Skizze, Host-Repro, Testhinweis in
      `firmware-mining/lfrfid-hitags-write-dispatch-integration.md` (Patches im
      gleichnamigen Verzeichnis, Host-Repro in
      `firmware-mining/repro-lfrfid-hitags-write-dispatch-integration/`).
- [ ] Lokal zu tun: alle fünf Patches (erst `hitags_h.patch`/`hitags_c.patch`, dann die drei
      neuen) gegen die eigene `mntm-012`-Basis anwenden, mit `ufbt`/`fbt firmware_all`
      bauen — jetzt ein echter Integrationstest (Werkzeug erstmals verlinkt), kein reiner
      Compile-Test mehr. Hardware-Test an einem eigenen, leeren Hitag-S/ID8268-Rohling
      (Proxmark3-Gegenprobe vorher/nachher, siehe Testhinweis im Dokument) sowie
      Negativtest/Latenzmessung an einem T5577-/EM4305-Rohling.
- [ ] Neuer eigener Rückstellpunkt: dieselbe Integration für `hitagmicro.c`/`.h` (drei
      zusätzliche Enum-Werte, je einer pro `HitagMicroVariant`, kein UID-Preread nötig) ist
      durch dasselbe Modell möglich, blockiert aber an einer fehlenden, verifizierten Quelle
      für den `LFRFIDHitagMicro::config`-Bitwert (TTF/EM4100-Konfiguration der Zielseite
      `0xFF`) — kein Patch ohne diese Klärung, um kein unbelegtes Magic-Konstant für eine
      echte Hardware-Beschreibung vorzuschlagen.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `device_discovery.py`,
      `hackrf_link.py`, `lorix_link.py` aus der `g4meover-deck`-Modulliste noch ungeprüft;
      `texkom` (NFC-Protokollfamilie aus der `unlshd-093`→`dev`-Restliste), NFC/RFID-Parser-
      Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465.

## Ideen-Recherche: Glitch Trigger (at0m-b0mb) — neue Kategorie "Fault Injection / Glitching"

- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining ("Hitag S in den
      LFRFID-Schreib-Dispatch eingehängt"), davor App-Verbesserung (`g4meover-deck`
      `wpa_crack.py`) und Ideen-Recherche (Sentry Safe Opener) — nach dem etablierten
      Rotationsmuster (Firmware → Ideen → App → Firmware → Ideen → ...) war Ideen-Recherche
      für diese Sitzung an der Reihe. `ideas/pentest-app-katalog.md` auf verbleibende
      ❓-Einträge geprüft: keine offen — stattdessen gezielt nach einer bislang komplett
      fehlenden Kategorie gesucht. Alle bisherigen Hardware-Debug-/Bus-Funde (SWD Probe, GPIO
      Logic Analyzer, i2c Tools, UART Sniff, RS485 Modbus, CAN-Bus-Cluster, SPI Flash Dump)
      lesen/sprechen einen Ziel-Bus passiv/aktiv an — keiner deckt die aktive
      Spannungsglitching-/Fault-Injection-Seite ab.
- [x] `at0m-b0mb/GlitchTrigger-FlipperZero` (MIT, App-ID `glitch_trigger`, dieselbe Werkstatt
      wie das bereits katalogisierte TSCM-Quartett Nyx/Specter/GhostTag/Argus) gefunden und
      vollständig gelesen (36 Quelldateien, 3656 Zeilen: App-Lebenszyklus, DWT-Puls-Engine,
      UART-Erfolgsstring-Watcher, SD-Profile/CSV-Log, Fault-Map-Grid, alle elf Szenen, alle
      vier Custom-Views). Geprüfter Stand: Tag `v1.3`, HEAD-Commit `c40b64b` (zwei
      Folgecommits betreffen nur README/Docs, kein Quelltext).
- [x] Was/Nutzen: präziser GPIO-Impulsgenerator für die Trigger-Seite einer
      Spannungsglitching-Kampagne (Manuell/Extern/Repeat, DWT-zyklusgenaues Timing
      ~15,6 ns, 1D-/2D-Sweep mit Dwell/Linear-oder-Random, Auto-Hit über GPIO- oder
      UART-Erfolgsstring-Feedback, Live-Fault-Map + CSV-Export, SD-Profile) — kein eigener
      Glitcher, treibt eine externe MOSFET-/Gate-Driver-Crowbar an (README macht das
      wiederholt und explizit klar). API-87-Status gegen eine frische, unabhängige
      Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag `mntm-012`, Commit `b359f6f`) verifiziert:
      alle sechs genutzten Erweiterungsport-Pins, `FURI_CRITICAL_ENTER/_EXIT`/`DWT->CYCCNT`
      (STM32WB55), `furi_hal_serial_control_*`/`furi_hal_serial_*` sowie
      `applications/services/expansion/expansion.h` byteidentisch vorhanden — dessen
      Doc-Kommentar benennt die geforderte `expansion_disable()`-vor-/`expansion_enable()`-
      nach-Acquire-Reihenfolge sogar normativ selbst.
- [x] Quelltext bis auf den folgenden Fund durchgehend defensiv (wrap-sicheres
      `glitch_delay_cycles()`, korrektes gleitendes Fenster in `glitch_serial.c` ohne
      Off-by-one, durchgehend geprüfte Bit-Index-Grenzen in `glitch_map.c`, korrekte
      Zielpuffergrößen in beiden Text-Eingabe-Szenen) — vergleichbar mit den bereits ohne
      Speicherfehler-Fund geprüften `at0m-b0mb`-Werkzeugen (Nyx/Specter/GhostTag).
- [x] Fund: `glitch_app_alloc()`/`glitch_app_free()` rufen nirgends
      `expansion_disable()`/`_enable()` auf (per `grep -rn "expansion"` vor dem Fix bestätigt
      — null Treffer), obwohl `helpers/glitch_serial.c` für die Sweep-Feedback-Quelle "UART"
      `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)` nutzt — exakt das UART, das der
      Flipper-eigene Expansion-Systemdienst standardmäßig überwacht (identischer
      Wirkmechanismus wie beim bereits dokumentierten CAN-Commander-Fund: Flanke auf der
      RX-Leitung lässt den Expansion-Worker das Handle zuerst belegen,
      `furi_hal_serial_control_acquire()` liefert danach `NULL`, `glitch_serial_start()` gibt
      `false` zurück). Compoundierender, bewusst nicht gepatchter Zweitbefund: die Sweep-UI
      zeigt das "A"-Auto-Hit-Abzeichen unverändert aus `app->params.auto_hit` (Nutzerabsicht)
      statt aus dem tatsächlichen `s_uart`-Verbindungsstatus — eine unbeaufsichtigte Kampagne
      kann dadurch unbemerkt mit Hit-Rate 0 % laufen, ohne dass der Nutzer den fehlenden
      Feedback-Kanal erkennen kann.
- [x] Unabhängiger Zweitfund per `git log -p` auf den exakten Release-Commit zurückverfolgt:
      `GLITCH_VERSION` (`glitch_trigger_i.h`) blieb beim Release-Commit `7c03f73` ("Glitch
      Trigger 1.3") auf `"1.2"` stehen, während `application.fam`s `fap_version` korrekt auf
      `"1.3"` angehoben wurde (bei den drei vorherigen Releases wurden beide Stellen stets
      synchron gebumpt) — derselbe Bug-Typ wie beim bereits katalogisierten
      PINGEQUA-RF-Lab-Fund, hier aber auf den exakten Commit statt nur das ausgelieferte
      Binary zurückgeführt.
- [x] Fix (additiv: ein neues Feld, ein neuer Include, zwei neue Aufrufe, ein
      Einzeiler-Versionsbump) gegen zwei frische, unabhängige Zweitkopien von
      `at0m-b0mb/GlitchTrigger-FlipperZero` (Commit `c40b64b`) mit `git apply --check`
      verifiziert, real angewendet, per `diff -r` byte-für-byte abgeglichen — identisch.
      `expansion_disable()` bewusst für die gesamte App-Laufzeit (nicht nur während eines
      laufenden UART-Sweeps), da die App den Erweiterungsport ohnehin durchgehend für ihre
      eigenen Glitch-Out-/Trigger-In-/Feedback-Pins belegt — dieselbe bereits beim
      CAN-Commander-Fix akzeptierte Abwägung.
- [x] Details, Fund, Fix, API-87-Status, Quelltext-Review, Testhinweis, Sicherheitsrahmen in
      `firmware-mining/glitchtrigger-expansion-conflict-fix.md` (Patch im gleichnamigen
      Verzeichnis) sowie `ideas/glitchtrigger-fault-injection-app-status.md`. Neue
      Katalog-Kategorie "Fault Injection / Glitching" in `ideas/pentest-app-katalog.md`
      ergänzt.
- [ ] Lokal zu tun: `glitch_trigger` (mit Patch) gegen die eigene `mntm-012`-Basis mit
      `ufbt`/`fbt` bauen, `applications/external/glitch_trigger/` einbinden. Funktionstest
      wie in `firmware-mining/glitchtrigger-expansion-conflict-fix.md` beschrieben (Expansion
      aktiviert lassen, UART-Feedback-Sweep vor/nach Patch vergleichen; Regressionstest mit
      einem echten Erweiterungsmodul nach App-Ende). Zusätzlich Beschaffung/Aufbau der
      MOSFET-/Gate-Driver-Crowbar-Stufe für den eigentlichen Hardware-Glitch-Test weiterhin
      offen.
- [ ] Neuer eigener Rückstellpunkt: der compoundierende UI-Befund (kein sichtbarer
      Unterschied zwischen "Auto-Hit angefordert" und "Auto-Hit-UART tatsächlich verbunden"
      im Sweep-View) bleibt offen — würde einen neuen, von `s_uart` gespeisten Status im
      Sweep-View erfordern, bewusst nicht in dieser Sitzung spekulativ gelöst.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `device_discovery.py`,
      `hackrf_link.py`, `lorix_link.py` aus der `g4meover-deck`-Modulliste noch ungeprüft;
      `texkom` (NFC-Protokollfamilie aus der `unlshd-093`→`dev`-Restliste), NFC/RFID-Parser-
      Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-
      Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration
      (blockiert an fehlender Config-Bitwert-Quelle).

## App-Verbesserung: g4meover-deck — lorix_link.device_fleet() liest nur die erste TTN-Seite

- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (Glitch Trigger), davor
      Firmware-Feature-Mining (Hitag S in den LFRFID-Schreib-Dispatch) und App-Verbesserung
      (`wpa_crack.py`) — nach dem etablierten Rotationsmuster (Ideen → App → Firmware → Ideen →
      ...) war App-Verbesserung für diese Sitzung an der Reihe. Von den zuletzt noch offenen
      `g4meover-deck`-Modulen (`device_discovery.py`, `hackrf_link.py`, `lorix_link.py`)
      `device_discovery.py` und `hackrf_link.py` mitgeprüft — ohne Fund (reine
      VID:PID-Zuordnung bzw. CLI-Wrapper ohne eigene Parsing-/Zustandslogik mit
      Fehlerpotenzial).
- [x] `lorix_link.py` (TTN-HTTP-API-Anbindung der LORIX-One-Gateway: Erreichbarkeit,
      Gateway-Verbindung, registrierte Geräte-Flotte) vollständig gelesen. Fund:
      `device_fleet()` fragt `/api/v3/applications/{APP}/devices` ohne jedes `limit`/`page`
      ab — die TTN-/The-Things-Stack-HTTP-API ist bei allen Listen-Endpunkten serverseitig
      seitenbasiert paginiert (offizielle Pagination-Doku: `limit`/`page`-Query-Parameter,
      `X-Total-Count`-Antwort-Header als Gesamtzahl), ohne explizites `limit` greift ein
      serverseitiger Standardwert. Hat die eigene DogyTag-Flotte mehr Geräte registriert als
      dieser Standardwert, liefert `device_fleet()` ohne jede Fehlermeldung nur die erste
      Seite zurück — `count` zu klein, fehlende Geräte (inkl. Join/Session-Status) fehlen
      komplett in `devices`. Für ein wachsendes Ökosystem (jeder neue Tracker landet in
      derselben TTN-Application) kein Rand-, sondern ein mit der Zeit zwangsläufig
      eintretender Fall.
- [x] Fix (additiv: neue `_get_page()` liest zusätzlich den `X-Total-Count`-Header,
      `device_fleet()` liest in einer Schleife `limit`/`page`-parametrisierte Seiten bis der
      Total-Count-Stand erreicht ist bzw., ohne diesen Header, bis eine kürzere Seite als
      angefordert zurückkommt). Eigene erste Fassung der Abbruchbedingung noch beim
      Selbst-Review verworfen: `len(batch) < limit` als alleiniges Abbruchkriterium hätte bei
      einem Server, der jede Seite serverseitig unter das angeforderte `limit` kappt, die
      Schleife fälschlich vor dem tatsächlichen Flottenende abgebrochen — jetzt nur noch
      Fallback, wenn kein `X-Total-Count` vorliegt. Patch gegen zwei frische, unabhängige
      Zweitkopien (eine davon ein komplett neuer, dritter Checkout) von
      `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`) mit `git apply --check` verifiziert,
      real angewendet, per `md5sum` abgeglichen — identisch. `python3 -m py_compile` danach
      fehlerfrei.
- [x] Host-Repro (kein TTN-Zugang/Netzwerk nötig): `device_fleet()` ALT/NEU 1:1 transkribiert,
      `MockTTN` bildet nur das genutzte TTN-API-Verhalten nach (Pagination, Total-Count-Header,
      Session-Lookup je Gerät) — 8 Szenarien: Regression ohne Fund, der Fund selbst
      (47 Geräte, Standardseite 20 → ALT meldet 20, NEU meldet 47), exaktes Seitengrenzen-
      Vielfaches ohne Off-by-one, Fallback ohne Total-Count-Header, serverseitig gekappte
      Seitengröße trotz Total-Count (deckt genau den beim Selbst-Review verworfenen
      Zwischenstand ab), leere Flotte ohne Endlos-Loop, Äquivalenz ALT==NEU für
      Flottengrößen 0..20 — 8/8 bestanden.
- [x] Details, Fund, Fix, Testhinweis, Risiko in
      `app-improvements/g4meover-deck/lorix-link-device-fleet-pagination-fix.md`
      (Patch im gleichnamigen Verzeichnis, Repro in
      `repro-lorix-link-device-fleet-pagination/`).
- [ ] Lokal zu tun: echten `DOGYTAG_TTN_KEY`/`DOGYTAG_TTN_APP` setzen, `python3 -m
      deck.lorix_link` vor/nach dem Patch gegen die eigene TTN-Application laufen lassen.
      `DOGYTAG_TTN_DEVICE_PAGE_SIZE` bewusst klein setzen (z. B. `1`), um das
      Mehrseiten-Verhalten auch mit einer kleinen echten Testflotte zu beobachten.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom`
      (NFC-Protokollfamilie aus der `unlshd-093`→`dev`-Restliste), NFC/RFID-Parser-
      Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-
      Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration
      (blockiert an fehlender Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status
      (kein vom tatsächlichen UART-Verbindungsstatus gespeister Status im Sweep-View).

## Firmware-Feature-Mining: Texkom-NFC-Protokollkern (Phase 1 von 5)

- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung (`g4meover-deck`
      `lorix_link.py`-Pagination), davor Ideen-Recherche (Glitch Trigger) und
      Firmware-Feature-Mining (Hitag S in den LFRFID-Schreib-Dispatch) — nach dem
      etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war
      Firmware-Feature-Mining für diese Sitzung an der Reihe. Git-Identität vor dem
      ersten Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `texkom` stand seit der `unlshd-093`→`dev`-Diff-Sichtung als größte, mehrteilige
      offene NFC-Protokollfamilie auf der Rückstellliste und wurde seither in praktisch
      jeder Firmware-Feature-Mining-Sitzung zugunsten kleinerer, vollständig
      abschließbarer Kandidaten zurückgestellt (`keyfinder2`/`hitags`/`hitagmicro` aus
      derselben Diff-Restliste sind inzwischen alle bearbeitet). Statt erneut
      zurückzustellen, diesmal vollständig gesichtet (Unleashed `dev`, Commit
      `f62964cb8216c566cb3940992d0fea6bbe91d840`, 2026-10-16): 18 Dateien, 2040 Zeilen
      über fünf Schichten (Protokollkern 798 Zeilen, Poller 167, Listener 122,
      `DigitalSequence`-Signalpreset 261, Low-Level-RF `furi_hal` 527 — Texkom-Tags
      nehmen an keinem ISO-Anticollision-/Befehlssatz teil, Transparent-Mode auf dem
      STM32WB55 nötig —, App-UI-Anbindung 165).
- [x] Protokollkern (`texkom.c/.h/_i.h`, 798 Zeilen) als einzige Schicht ohne jeden
      `furi_hal`-Bezug identifiziert: reine Datenstruktur, drei CRC-Varianten (TK13
      eigenes Poly-0x31-CRC-8, TK17 Dallas/Maxim-CRC-8, MMBIT genibbelter XOR),
      Lade-/Speicherlogik (reiner `flipper_format_*`-Durchgriff) und drei
      Intervall-Decoder (TK13/TK15/TK17) samt Dispatch — ausschließlich mit
      `uint8_t`-Lückenlisten rechnend, vollständig host-testbar.
- [x] Alle vier Abhängigkeiten geprüft und gegen eine frische, unabhängige
      `G4MEOVER18/G4MEOVER-FW`-Zweitkopie (`main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`,
      Tag `v2.0.0`/`mntm-012`) per `diff` Byte-für-Byte bestätigt: `nfc/protocols/
      nfc_device_base_i.h`, `nfc/nfc_common.h` (inkl. `NFC_UNIFIED_FORMAT_VERSION`),
      `one_wire/maxim_crc.{c,h}`, `flipper_format/flipper_format.h` — alle bereits
      identisch vorhanden, kein Portierungshindernis. `lib/nfc/protocols/
      nfc_protocol.c` als künftigen Phase-5-Integrationspunkt identifiziert (Texkom
      dort bereits als eigenständiger Basisknoten ohne Kindprotokolle kartiert).
- [x] Quelltext (614 Zeilen `texkom.c`) gezielt auf Speicherfehler/Off-by-one geprüft:
      **kein eigener Fund** — alle Array-Zugriffe an zur Übersetzungszeit bekannte
      Konstanten gebunden, die exakt den tatsächlichen Puffergrößen entsprechen, alle
      `furi_check()`-Nullprüfungen korrekt. Reiner, sauberer Port ohne Bugfund in
      dieser Sitzung.
- [x] Host-Repro (acht Szenarien: TK13-Rundlauf über den öffentlichen Dispatch,
      TK13→TK15-Rückstufung bei verrauschten Zeiten inkl. expliziter Gegenprobe, dass
      `decode_tk13()` dabei tatsächlich fehlschlägt, TK17-Rundlauf, MMBIT-Nibble-Maske,
      Negativtest korrupte Preamble mit Erholungsprobe, Negativtest Typ-/
      Codierungs-Inkonsistenz, Negativtest TK17-Decoder außerhalb Band/Summe-0,
      CRC-Konsistenz aller Varianten plus `Unknown`) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined
      -fno-sanitize-recover=undefined` bei `-O0`/`-O1`: 57 Prüfungen, alle bestanden,
      identischer Output beider Optimierungsstufen, keine Compiler-Warnungen.
- [x] Die drei Dateien (`texkom.c/.h/_i.h`) unverändert gegen eine zweite, unabhängige
      Zweitkopie von Unleashed `dev` (Commit `f62964c`) per `diff` Byte-für-Byte
      abgeglichen — identisch.
- [x] Details, Schichtenübersicht, Verifikation, Risiko, fünfstufige Port-Skizze in
      `firmware-mining/texkom-nfc-protocol-core-port.md` (fertige Dateien im
      gleichnamigen Verzeichnis, Repro in
      `firmware-mining/repro-texkom-nfc-protocol-core-port/`).
- [ ] Lokal zu tun: `texkom.c/.h/_i.h` nach `lib/nfc/protocols/texkom/` kopieren, in die
      `lib/nfc`-Build-Liste aufnehmen, mit `ufbt`/`fbt` bauen (baut eigenständig, wird
      von nichts referenziert, keine Verhaltensänderung). Phase 2-5 (Poller, Listener,
      Signalpreset, `furi_hal`-Low-Level-RF, Protokollbaum-Registrierung/UI) gemäß
      Port-Skizze in künftigen, auf je eine Schicht fokussierten Sitzungen.
- [ ] Neuer eigener Rückstellpunkt: Phase 4 (`furi_hal_nfc_texkom.c`, 527 Zeilen,
      Transparent-Mode-Ansteuerung) als höchstes Risiko dieser Portierung markiert —
      eigene, nur auf diese Datei fokussierte künftige Sitzung vorgesehen, Register-/
      Timing-Konstanten gegen die eigenen `furi_hal_nfc_iso14443a.c`/
      `furi_hal_nfc_iso15693.c` gegenzurechnen, bevor ein Patch vorgeschlagen wird.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-
      Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-
      Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s
      restliche Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus
      PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status (kein vom
      tatsächlichen UART-Verbindungsstatus gespeister Status im Sweep-View).

## Ideen-Recherche: PromptZero — sprachgesteuerter Flipper-Orchestrator (PC-Tool)

- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Texkom-NFC-
      Protokollkern, Phase 1 von 5), davor App-Verbesserung (`g4meover-deck`
      `lorix_link.py`-Pagination) und Ideen-Recherche (Glitch Trigger) — nach dem
      etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → Ideen → ...)
      war Ideen-Recherche für diese Sitzung an der Reihe. Git-Identität vor dem ersten
      Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Statt eines weiteren Flipper-FAP-Clusters gezielt nach einem existierenden
      PC-seitigen Baustein für die bislang fehlende Kommandoebene über
      `g4meover-deck` gesucht — bisher nur Einzelmodul-Skripte
      (`flipper_link.py`/`hackrf_link.py`/`lorix_link.py`/`zigbee_link.py`/
      `wpa_crack.py`), jedes für sich bereits in früheren App-Verbesserungs-
      Sitzungen geprüft/gepatcht, aber keine gemeinsame Kommandoebene darüber.
- [x] **PromptZero** (`xunholy/promptzero`, AGPL-3.0-or-later, Go, 246 Sterne,
      2347+ Commits) gefunden und über README/Docs/Tool-Referenz geprüft: ein
      natürlichsprachlicher Flipper-Operator über ein laut eigener Doku 700+
      Einzel-Tools umfassendes Register (SubGHz RX/TX/Bruteforce/Chat, Infrarot
      RX/TX/Bruteforce, NFC Detect/Dump/Emulate/APDU/Mifare-Ultralight-Schreiben,
      125-kHz-RFID Read/Emulate/T5577-Write, iButton, GPIO/I2C, BadUSB, Storage,
      ~15 FAP-Launcher, optional ESP32-Marauder-WiFi-Funktionen bei
      angeschlossenem Devboard), jedes Tool mit fester Risikostufe
      (low/medium/high/critical). Anthropic-Claude als Sprachmodell-Backend
      (eigener API-Key Voraussetzung).
- [x] Firmware-Zielgruppe explizit primär **Momentum** (zusätzlich Unleashed/
      RogueMaster, offizielle Firmware nur mit reduziertem Funktionsumfang) —
      genau meine eigene Firmware-Familie. Zwei genutzte Tools (`subghz_rx_raw`,
      `js_run`) setzen Momentum-exklusive Erweiterungen voraus, beide im eigenen
      `mntm-012`-Stand bereits enthalten. Kein Flipper-C-Quelltext (PC-seitiges
      Go-CLI-Tool, spricht über die serielle Flipper-Kern-CLI), daher keine
      Byte-für-Byte-Verifikation wie bei einem Firmware-/App-Port möglich oder
      nötig — andere Prüftiefe, vor dem ersten lokalen Einsatz die erwartete
      Standard-CLI-Befehlsmenge gegen den eigenen `v2.0.0`/`mntm-012`-Stand kurz
      gegenprüfen.
- [x] Sicherheitsmodell geprüft statt Quelltext-Speicherfehlersuche (passt zur
      anderen Natur dieses Fundes): REPL-Konsens-Gate mit Vorschau
      (Frequenz/Modulation/Hexdump) plus 2-Sekunden-Verzögerung vor einer
      positiven Bestätigung für High-/Critical-Tools, MCP-Modus verweigert
      High/Critical standardmäßig ohne explizite Umgebungsvariable
      (`PROMPTZERO_MCP_ALLOW_HIGH`/`_CRITICAL`), ohne initialisiertes Audit-Log
      verweigert der Agent High+-Aktionen komplett, zusätzlich ein
      `--read-only`-Modus. Deckt sich mit dem eigenen defensiven/autorisierten
      Rahmen. Offene Dokumentationslücke benannt: README belegt nicht, ob sich
      die REPL-Bestätigung per Flag für unbeaufsichtigten Betrieb umgehen lässt —
      vor jedem automatisierten (Cron-/Skript-)Einsatz lokal zu verifizieren.
- [x] Details, Fund, Sicherheitsrahmen, Risiko (AGPL-Lizenz bei etwaigem späteren
      Vendoring, offene Konsens-Flag-Frage, `critical`-eingestufte Bruteforce-/
      Deploy-Tools, laufende API-Kosten), Nächster Schritt in
      `ideas/promptzero-ai-flipper-orchestrator-recherche.md`. Neue Zeile unter
      "Eigene Kreationen" in `ideas/pentest-app-katalog.md` ergänzt (Inspiration
      für eine künftige `g4meover-deck`-Kommandoebene), Recherche-Narrativ im
      Katalog fortgeschrieben.
- [ ] Lokal zu tun: Installations-Skript vor Ausführung ansehen (nicht blind
      gegen das Produktivsystem laufen lassen), danach im `--read-only`-Modus
      gegen den eigenen Flipper verbinden und `device_info`/`storage_list` als
      harmlosen Rauchtest fahren. Erst danach, mit aktivem Konsens-Gate,
      einzelne Low-/Medium-Risiko-Tools gegen eigene Geräte testen. Vor jedem
      High-/Critical-Tool-Einsatz das Konsens-Verhalten selbst am Terminal
      nachvollziehen statt nur der Doku zu vertrauen.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom` Phase 2-5
      (Poller, Listener, Signalpreset, `furi_hal`-Low-Level-RF als höchstes
      Risiko, Protokollbaum-Registrierung/UI), NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s
      restliche Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus
      PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status (kein vom
      tatsächlichen UART-Verbindungsstatus gespeister Status im Sweep-View).

## App-Verbesserung: FlipperJammer — Run-Scene-Ausgang landet auf der Dauer-Auswahl statt im Konfigurationsmenü
- [x] `reports/` durchgesehen: letzte Sitzung war Ideen-Recherche (PromptZero),
      davor Firmware-Feature-Mining (Texkom-NFC-Protokollkern) und
      App-Verbesserung (`g4meover-deck` `lorix_link.py`-Pagination) — nach dem
      etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → Ideen →
      App → ...) war App-Verbesserung für diese Sitzung an der Reihe.
      Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `g4meover-deck` ist bereits vollständig durchgesehen (siehe vorherige
      Sitzung), ebenso `g4meover-magspoof` und `RollJam`. Von den eigenen
      Apps mit noch offenem Durchsichts-Rest `FlipperJammer` gewählt: das
      frühere Tiefenaudit deckte explizit nur `modules/*.c` und
      `jammer_app.c` ab (siehe `em4100-encoder-data-independent-output-fix.md`,
      `hw-detect-otg-leak-after-exit-fix.md`) — `scenes/` (18 Dateien,
      ~1900 Zeilen, Scene-Manager-Navigation und Dauer-/Start-Flow) war nie
      geprüft.
- [x] **Fund**: Jeder Modus mit wählbarer Laufzeit schiebt dafür die
      gemeinsame `Duration`-Scene zwischen Konfigurationsscene und Run-Scene
      (`Config -> Duration -> Run`, per `scene_manager_next_scene()` jeweils
      gepusht, nie ersetzt). Jede der zehn betroffenen Run-Scenes
      (`SubGhzRun`/`Nrf24Run`/`BleRun`/`WifiRun`/`IrRun`/`UsbFuzzerRun`/
      `RfidJamRun`/`NfcJamRun`/`RfidFuzzerRun`/`NfcFuzzerRun`) verlässt sich
      beim automatischen Ausstieg (Timer-Ablauf, `0xDEAD`-Event) auf ein
      einfaches `scene_manager_previous_scene()` — poppt immer nur eine
      Ebene. Über den optionalen Direkt-Start-Weg (`Config -> Run`, ohne
      Dauer-Auswahl) ist das zufällig richtig; über den Dauer-Weg landet man
      auf der `Duration`-Scene selbst statt auf der Konfigurationsscene.
      Für `RfidJamRun`/`NfcJamRun`/`RfidFuzzerRun`/`NfcFuzzerRun` gibt es
      laut `scene_rfid_menu.c` **gar keinen** Direkt-Start-Weg — dort ist das
      kein Rand-, sondern der Regelfall: jeder einzelne Lauf dieser vier
      Modi endet unausweichlich auf der Dauer-Auswahl statt im RFID/NFC-Menü.
      Dieselbe fehlende Behandlung des Back-Events (fällt auf den
      Scene-Manager-Standard-Pop zurück) betrifft auch das Drücken der
      Zurück-Taste während des Laufs.
- [x] Fix (additiv, zehn Dateien): in jeder Run-Scene's `on_event()` ersetzt
      `scene_manager_search_and_switch_to_previous_scene(app->scene_manager,
      Scene<Parent>)` das bisherige `scene_manager_previous_scene(...)` im
      `0xDEAD`-Zweig, plus neuer `SceneManagerEventTypeBack`-Zweig mit
      demselben gezielten Sprung — sucht die genannte Elternscene rückwärts
      im Navigationsstapel und kürzt ihn bis dorthin, unabhängig davon ob
      die `Duration`-Scene dazwischenliegt. Zielscene: die jeweilige
      `*Config`-Scene bei den sechs Modulen mit eigener Konfigurationsscene,
      `RfidMenu` bei den vier Modulen ohne eigene (deren einziger Einstieg
      bereits darüber läuft). `scene_duration.c` selbst unverändert — der
      Fix sitzt bewusst auf der Ausgangsseite, da dort unabhängig vom
      genommenen Eintrittsweg korrekt. `scene_rfid_spoofer.c`/
      `scene_nfc_spoofer.c` (kein Dauer-/Run-Muster) und
      `scene_nrf24_diag.c`/`scene_info.c` geprüft, kein Fund, bewusst nicht
      angefasst.
- [x] Host-Repro: die relevanten Scene-Manager-Funktionen
      (`scene_manager_next_scene`/`scene_manager_previous_scene`/
      `scene_manager_search_and_switch_to_previous_scene`) 1:1 als
      Host-Modell nachgebaut (Push/Pop/Truncate-Semantik, gegen die
      tatsächliche Flipper-Firmware-Quelle abgeglichen), für alle zehn
      Module je Dauer-Weg und (soweit vorhanden) Direkt-Start-Weg
      durchgespielt. Unter `gcc -std=c11 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
      `-O0`/`-O1`: 32 Einzelprüfungen, alle bestanden, identischer Output
      beider Optimierungsstufen, keine Compiler-Warnungen, keine
      Sanitizer-Befunde — in
      `app-improvements/FlipperJammer/repro-scene-run-exit-lands-on-duration-picker/repro.c`.
- [x] Zehn Patches gegen zwei frische, unabhängige Zweitkopien von
      `G4MEOVER18/FlipperJammer` (`6a3cd49`) mit `git apply --check`
      verifiziert, real angewendet, je Datei per `md5sum` abgeglichen —
      identisch, in
      `app-improvements/FlipperJammer/scene-run-exit-lands-on-duration-picker-fix/`.
- [x] Details, Fund, Fix, Host-Repro, Risiko, Testhinweis in
      `app-improvements/FlipperJammer/scene-run-exit-lands-on-duration-picker-fix.md`.
- [ ] Lokal zu tun: alle zehn Patches gegen die eigene `FlipperJammer`-Kopie
      anwenden, mit `ufbt launch`/`fbt` bauen. Für jedes der zehn Module:
      Dauer auf 10 Sekunden stellen, starten, Timer ablaufen lassen — muss
      auf der Konfigurationsscene (bzw. bei RFID/NFC-Jammer/-Fuzzer im
      RFID/NFC-Menü) landen statt auf der Dauer-Auswahl; zusätzlich Back
      während des Laufs testen. Gegenprobe über den Direkt-Start-Weg (wo
      vorhanden): keine Regression.
- [ ] Mit diesem Fund ist `FlipperJammer` jetzt vollständig durchgesehen
      (`modules/*.c`, `jammer_app.c` aus früheren Sitzungen, `scenes/*.c` aus
      dieser Sitzung).
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom`
      Phase 2-5, NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES
      Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s
      restliche Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe
      aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz.
      `lora-ukfe` (eigene App, nur die RF-Console-Teile bereits geprüft)
      weiterhin nicht vollständig durchgesehen.

## Firmware-Feature-Mining: Texkom-NFC-Poller (Phase 2 von 5)

- [x] Lokale `main`-Zweigspitze war beim Sitzungsstart detached und 28 Commits
      hinter `origin/main` (reines Checkout-Problem dieses Containers, keine
      Datenverlustgefahr — `origin/main` stand bereits auf dem aktuellen
      Commit). Vor dem ersten eigenen Commit per Fast-Forward korrigiert,
      lokal/`origin` jetzt wieder identisch. Git-Identität gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `reports/` durchgesehen: letzte Sitzung war App-Verbesserung
      (`FlipperJammer`, Run-Scene-Ausgang-Fix), davor Ideen-Recherche
      (PromptZero) und Firmware-Feature-Mining (Texkom-Protokollkern,
      Phase 1 von 5) — nach dem etablierten Rotationsmuster (Firmware →
      Ideen → App → Firmware → ...) war Firmware-Feature-Mining für diese
      Sitzung an der Reihe. Phase 1 selbst (Protokollkern) stand laut
      eigener Port-Skizze als nächster Schritt bereits fest, statt erneut
      einen neuen Kandidaten zu suchen: Phase 2 (Poller) aufgegriffen.
- [x] Poller-Quelle (`texkom_poller.c/.h/_defs.h/_i.h`, 167 Zeilen) erneut
      gegen einen frischen, unabhängigen Unleashed-`dev`-Stand geholt
      (Commit `b510f625dd5a8962d9129b6bb17dc8a73c90fa07`, 2026-10-18):
      `texkom.c/.h/_i.h` (Phase 1) dabei per `diff` erneut Byte-für-Byte
      identisch zum zuvor geprüften Stand bestätigt — keine Änderung seit
      der letzten Sichtung.
- [x] Poller-Logik gelesen: reine Kontrolllogik ohne eigene Bit-/
      Byte-Mathematik — ein Aufruf von `nfc_texkom_poller_rx()`, Ergebnis
      unverändert an `texkom_decode_intervals()` (Phase 1) weitergereicht,
      Callback nur bei Erfolg ausgelöst. Zwei Symbole fehlen bei mir noch
      (`NfcTechTexkom`-Enum-Wert, `nfc_texkom_poller_rx()`-Funktion) — beide
      erst mit Phase 4 real implementiert, aber trivial additiv (ein
      Enum-Wert, eine Dispatch-Tabellenzeile, eine Wrapper-Funktion). Gegen
      eine frische, unabhängige `G4MEOVER18/G4MEOVER-FW`-Zweitkopie (`main`,
      weiterhin `b359f6fc32f282ce7217dd2d48a4e3478771852e`) per `diff`
      bestätigt: `lib/nfc/protocols/nfc_generic_event.h`/`nfc_poller_base.h`
      (die restliche Abstraktionsschicht, über die jeder Poller ans `Nfc`-
      Objekt angebunden wird) bereits byte-identisch vorhanden; `nfc/nfc.h`
      unterscheidet sich exakt um die zwei genannten Texkom-Symbole plus
      einen unabhängigen `NfcEventTypeListenerTick`-Wert (Listener-Pacing,
      nicht Teil dieser Phase).
- [x] Host-Repro (acht Szenarien: Alloc/Free-Lebenszyklus inkl.
      `nfc_config()`-Aufrufprüfung, Event-Filter auf
      `NfcEventTypePollerReady`, TK13 Ende-zu-Ende über `run()` inkl.
      Prüfung dass der Rückgabewert exakt vom Callback stammt, TK17 über
      `detect()` ohne Callback-Aufruf, erfolgreicher Capture mit von keinem
      Decoder akzeptierten Daten ohne Antasten der vorherigen Daten,
      `NfcErrorTimeout` mit explizite Gegenprobe gegen noch gültig
      aussehende Altdaten im Puffer, alle vier Varianten Ende-zu-Ende,
      Zeiger-Identität von `general_event`/`texkom_event` über mehrere
      Aufrufe) unter `gcc -std=c17 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
      `-O0`/`-O1`: 46 Prüfungen, alle bestanden, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.
- [x] Die vier Dateien (`texkom_poller.c/.h/_defs.h/_i.h`) unverändert in
      `firmware-mining/texkom-nfc-protocol-core-port/` übernommen, Repro in
      `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_poller.c`.
      Port-Vorschlag (`firmware-mining/texkom-nfc-protocol-core-port.md`)
      entsprechend aktualisiert: Betroffene-Dateien-, Verifikations-,
      Risiko- und Port-Skizze-Abschnitte auf Phase 2 erweitert.
- [ ] Lokal zu tun: `texkom_poller.c/.h/_defs.h/_i.h` nach
      `lib/nfc/protocols/texkom/` kopieren (neben die bereits übernommenen
      Phase-1-Dateien), in dieselbe Build-Liste aufnehmen. Compiliert erst,
      sobald `NfcTechTexkom` und `nfc_texkom_poller_rx()` existieren — beide
      landen sinnvollerweise zusammen mit Phase 4.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom`
      Phase 3-5, NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES
      Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s
      restliche Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe
      aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz.
      `lora-ukfe` weiterhin nicht vollständig durchgesehen.

## Ideen-Recherche: Nearby Files (Stichoza) — GPS-Datei-Browser, Zero-Koordinaten-Filter-Fund

- [x] `reports/` durchgesehen: letzte Sitzung war Firmware-Feature-Mining (Texkom-NFC-Poller,
      Phase 2 von 5), davor App-Verbesserung (`FlipperJammer`, Run-Scene-Ausgang-Fix) und
      Ideen-Recherche (PromptZero) — nach dem etablierten Rotationsmuster
      (Firmware → Ideen → App → Firmware → Ideen → ...) war Ideen-Recherche für diese Sitzung
      an der Reihe. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer Ergänzung für die bislang dünnste Katalog-Kategorie gesucht
      ("Recon / GPS / Wardriving" — bisher nur reines Logging über `FlipDeFlock`/
      `GPS (NMEA) + WigleCSV`, keine Auswertungsseite). **Nearby Files**
      (`Stichoza/flipper-nearby-files`, aktueller Stand Commit `669a8c7`, `fap_version` 1.7)
      gefunden und vollständig gelesen (`nearby_files.c`, `gps_reader.c`/`.h`, sechs Szenen):
      GPS-gestützter Datei-Browser, der eigene SubGHz-/NFC-/RFID-/iButton-Captures nach
      Entfernung zur aktuellen Position sortiert anzeigt, Klick startet die passende App direkt
      mit der Datei.
- [x] Kein neuer Port-Kandidat — die App liegt bereits in meinem eigenen `mntm-012`-Baum
      (`applications/external`-Submodul, `Next-Flip/Momentum-Apps`, dort in meinem FW-Stand auf
      Commit `85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4` gepinnt). Per direktem `curl` gegen genau
      diesen gepinnten Commit (`raw.githubusercontent.com`, kein Firmware-Checkout nötig)
      verifiziert: mein Stand führt `fap_version` 1.5 — zwei Versionsstufen hinter dem aktuellen
      Upstream-`v1.7` (fehlend: `iButton`-Scan, persistente Baudraten-Konfiguration,
      "Add GPS"-Schreibpfad, alle erst ab `v1.6`/`v1.7`).
- [x] Fund in der bei `v1.5` und `v1.7` identischen Funktion
      `nearby_files_process_gps_coordinates()` (Zeile 487 in meiner `v1.5`, Zeile 728 in
      Upstream-`v1.7`): der "ist das kein `(0,0)`-Sentinel"-Check verwendet `&&` statt `||` —
      `if((float)latitude != 0.0f && (float)longitude != 0.0f)`. Das verwirft nicht nur den
      echten `(0,0)`-Nullwert, sondern jede Datei, deren Koordinate auf **einer einzigen** Achse
      exakt `0.0` ist. Ein echter, von "SubGHz Wardriving" automatisch eingetragener Capture auf
      dem Äquator oder dem Nullmeridian (Breite oder Länge exakt `0.0`, die andere Achse ganz
      normal) verschwindet dadurch ohne jede Fehlermeldung aus der Liste, obwohl er gültige
      GPS-Daten trägt — kein konstruierter Randfall, da Greenwich/London und weite Teile
      Spaniens/Frankreichs/Algeriens/Ghanas auf dem Nullmeridian liegen.
- [x] Additiver Ein-Operator-Fix (`&&` → `||`) in
      `ideas/nearby-files-zero-coordinate-filter-fix/nearby_files_c.patch`, gegen zwei frische,
      unabhängige Zweitkopien der exakten `v1.5`-Quelle (direkt vom gepinnten
      Momentum-Apps-Commit geladen) mit `git apply --check` verifiziert, real angewendet, beide
      Endstände per `md5sum` byte-für-byte identisch. Gilt unverändert auch für Upstream-`v1.7`.
- [x] Host-Repro (`ideas/repro-nearby-files-zero-coordinate-filter/repro.c`, acht Prüfgruppen:
      Regression, echter `(0,0)`-Sentinel, Fund auf Nullmeridian, Fund auf Äquator, Nahe-Null-
      Sanity-Check, explizite Abweichungs-/Übereinstimmungsprüfung, 1521-Punkte-
      Äquivalenz-Sweep über das gesamte Lat/Lon-Gitter ohne Null-Achse, Gegenprobe mit
      absichtlich zurückgesetztem Fix) unter `gcc -std=c17 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei `-O0`/`-O1`: 3352
      Prüfungen, beide Durchläufe bestanden, identischer Output, keine Compiler-Warnungen, keine
      Sanitizer-Befunde.
- [x] Details, Fund, Fix, Testhinweis, Versions-Rückstellpunkt in
      `ideas/nearby-files-gps-app-status.md`. Neue Katalog-Zeile unter
      "Recon / GPS / Wardriving" in `ideas/pentest-app-katalog.md` ergänzt, Recherche-Narrativ im
      Katalog fortgeschrieben.
- [ ] Lokal zu tun: Patch gegen `applications/external/nearby_files/nearby_files.c` im eigenen
      Firmware-Baum anwenden, mit `fbt`/`ufbt` bauen und flashen. Funktionstest: eine `.sub`-Datei
      von Hand mit `Lat: 51.507400`/`Lon: 0.000000` (oder umgekehrt mit Breite `0.0`) versehen,
      App mit gültigem GPS-Fix starten — vor dem Fix fehlt die Datei in der Liste, nach dem Fix
      erscheint sie mit korrekter Entfernungsangabe.
- [ ] Separater Rückstellpunkt (eigene Sitzung wert, sobald Firmware-Feature-Mining an der Reihe
      ist): Versionslücke `v1.5` → `v1.7` nachziehen (Submodul-Pin-Bump in `G4MEOVER-FW` auf
      einen Momentum-Apps-Stand mit `v1.7`), inklusive Prüfung der drei neuen Funktionen
      (`iButton`-Scan, Baudraten-Persistenz, "Add GPS"-Schreibpfad) auf eigene Fehler.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom` Phase 3-5 (Listener,
      Signalpreset, `furi_hal`-Low-Level-RF als höchstes Risiko, Protokollbaum-Registrierung/UI),
      NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund,
      `usb-army-penetrator`s restliche Ducky-Handler, neun verbleibende
      FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration
      (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz. `lora-ukfe` weiterhin
      nicht vollständig durchgesehen.

## App-Verbesserung: lora-ukfe — json_parse.c vollständig toter Code (Entfernung), Protokolllücke für drei Event-Typen dokumentiert

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Ideen-Recherche (Nearby Files), davor Firmware-Feature-Mining (Texkom-NFC-Poller
      Phase 2), davor App-Verbesserung (FlipperJammer) — nach dem etablierten
      Rotationsmuster (Firmware → Ideen → App → ...) war App-Verbesserung für diese
      Sitzung an der Reihe. Git-Identität gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`);
      lokale `main`-Zweigspitze war zu Sitzungsbeginn erneut detached (reines
      Checkout-Problem dieses Containers, `origin/main` bereits aktuell) — vor dem ersten
      eigenen Commit per `git checkout -B main origin/main` korrigiert.
- [x] Den zuletzt in den "Offen"-Abschnitten von `payload-list-response-dispatch-fix.md`
      und `scanhit-response-decode-fix.md` zurückgestellten Punkt aufgegriffen:
      "`json_parse.c`/`ukfe_json_handle()` bleiben toter Code — Entfernen oder
      Reaktivieren ist eine separate Entscheidung." Frischen Klon von
      `G4MEOVER18/lora-ukfe` (`0dba410a`, unverändert) gezogen, per
      `grep -rn "json_parse\|ukfe_json_handle"` über den gesamten Baum bestätigt: die
      Funktion hat weiterhin keinen Aufrufer außer ihrer eigenen Definition und der
      Deklaration in `lora_ukfe.h` — nicht nur `handle_payload_list()` (bereits bekannt),
      sondern alle fünf Event-Handler der Datei (`handle_status`/`handle_trigger_ack`/
      `handle_payload_list`/`handle_lora_rx`/`handle_wifi_ap`/`handle_error`).
      Zusatzbefund beim Abgleich aller fünf JSON-Event-Typen gegen das aktuelle
      `UkfeRfResp`-Enum (`rf/ukfe_rf.h`): zwei haben inzwischen ein Binär-Äquivalent
      (`status`/`payload_list`), für die restlichen drei (`lora_rx`, `wifi_ap`, `error`)
      existiert **kein** passender Response-Code im Enum — anders als bei
      `UkfeRfRespPayload`/`UkfeRfRespScanHit` (Enum-Wert vorhanden, nur `case` gefehlt) ist
      das hier keine reine Flipper-seitige Verdrahtungslücke, sondern bräuchte eine
      koordinierte Protokollerweiterung mit Gegenstück auf der Hub-Firmware (separates
      Repo) — bewusst nicht unilateral als neuer Wire-Code erfunden, sondern als eigener
      Rückstellpunkt dokumentiert.
- [x] `command_catalog.h` sowie alle Scenes (`scene_handlers.c`, `scene_menu.c`,
      `scene_mode_menu.c`, `scene_status.c`, `scene_settings.c`, `scene_log.c`,
      `scene_payload_list.c`) im Rahmen dieser Sitzung erneut gegen den frischen Klon
      abgeglichen — unverändert gegenüber der bereits abgeschlossenen Vorsitzung (Fund dort:
      `catalog-trigger-arg-format-mismatch-fix.md`/`trigger-catalog-number-input-port.md`),
      kein neuer Fund in diesem Teil. `rf/rf_comm.c`/`rf/ukfe_rf.c` (Kernprotokoll,
      CRC/MAC/Anti-Replay) ebenfalls gegenkontrolliert — identisch zum in mehreren
      Schwester-Apps (`heltec-v4-hub`, `usb-army-penetrator`, `heltec-ukfe-rx`) bereits
      geprüften Referenzstand, keine neue Abweichung.
- [x] Fix (rein subtraktiv): `json_parse.c` vollständig entfernt, zweizeilige Deklaration
      aus `lora_ukfe.h` entfernt. Patch gegen zwei frische, unabhängige Zweitkopien der
      unveränderten `G4MEOVER18/lora-ukfe`-Basis (`0dba410a`) mit `git apply --check`
      verifiziert, real angewendet, Ergebnis per `md5sum` abgeglichen — identisch;
      anschließend erneut `grep -rn "json_parse\|ukfe_json_handle"` über den gepatchten
      Baum (`.c`/`.h`/`.fam`) bestätigt keine verbleibende Referenz mehr. Details, Fund,
      Nutzen, Risiko, Testhinweis, Offen-Punkte (Protokollerweiterung `lora_rx`/`wifi_ap`/
      `error`, veraltete `README.md` auf altem JSON-Protokoll-Stand) in
      `app-improvements/lora-ukfe/json-parse-dead-code-removal.md`, Patch in
      `app-improvements/lora-ukfe/json-parse-dead-code-removal/json_parse_dead_code_removal.patch`.
- [ ] Lokal zu tun: Patch gegen die eigene `lora-ukfe`-Kopie anwenden, mit `ufbt build`
      bauen (muss unverändert erfolgreich sein, da der entfernte Code nie aufgerufen
      wurde), danach App auf dem Gerät wie gewohnt starten/bedienen — identisches
      Verhalten vor/nach dem Patch erwartet.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen, plus neu benannt): `texkom`
      Phase 3-5 (Listener, Signalpreset, `furi_hal`-Low-Level-RF als höchstes Risiko,
      Protokollbaum-Registrierung/UI), NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
      LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-
      Integration (blockiert an fehlender Config-Bitwert-Quelle),
      GlitchTrigger-Sweep-UI-Auto-Hit-Status, PromptZero-Installationsskript-Sichtprüfung
      vor lokalem Einsatz, Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump).
      `lora-ukfe` damit bis auf den neu benannten Protokollerweiterungs-Rückstellpunkt
      (`lora_rx`/`wifi_ap`/`error` ohne Binär-Äquivalent, braucht Hub-Gegenstück) und die
      veraltete `README.md` vollständig durchgesehen.

## Firmware-Feature-Mining: Texkom-NFC-Listener/Signalpreset (Phase 3 von 5)

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      App-Verbesserung (lora-ukfe), davor Ideen-Recherche (Nearby Files), davor
      Firmware-Feature-Mining (Texkom-Poller Phase 2) — nach dem etablierten
      Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war Firmware-Feature-Mining
      für diese Sitzung an der Reihe. Lokale `main`-Zweigspitze war zu Sitzungsbeginn
      erneut 32 Commits hinter `origin/main` (reines Checkout-Problem dieses Containers,
      `origin/main` bereits aktuell) — per Fast-Forward korrigiert, keine
      Datenverlustgefahr. Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Den seit Phase 1/2 in der Port-Skizze vorgesehenen nächsten Schritt aufgegriffen:
      Listener (`texkom_listener.c/.h/_defs.h/_i.h`) und Signalpreset
      (`texkom_signal.c/.h`) von Unleashed (`DarkFlippers/unleashed-firmware`, `dev`,
      aktueller Zweigkopf `3c6d6fa34f0be567d49a61a4c4c04dc2eb98f2d7`, per `git ls-remote`
      bestimmt) geholt, jede Datei zweimal unabhängig geladen und per `diff`/`md5sum`
      byte-für-byte identisch bestätigt. Die bereits übernommenen Phase-1/2-Dateien
      (`texkom.c/.h/_i.h`, `texkom_poller.c/.h/_defs.h/_i.h`) gegen diesen frischen Stand
      erneut per `diff` geprüft — unverändert.
- [x] Fund beim Zusammenlesen von `texkom_signal.c` (neu) mit dem bereits aus Phase 1
      bekannten `texkom_i.h`-Kommentar "TK13 sends 128 impulses, TK17 sends 65": der
      Grund für die Asymmetrie der beiden Intervall-Konstanten (127 vs. 64) liegt im
      TK17-Abschlusssymbol, das — anders als das TK13-Abschlusssymbol — vor der
      Park-Periode einen zusätzlichen Impuls sendet und dadurch die letzte Lücke des
      letzten Symbolpaars überhaupt erst messbar macht. Beide Zahlen waren seit Phase 1
      als Konstanten bekannt, ihre Begründung ergab sich erst jetzt aus dem Signalpreset —
      kein Fehler, sondern ein Erkenntnisgewinn, der die Phase-1/2-Verifikation nachträglich
      besser erklärt.
- [x] Zweiter Fund (für Phase 4 vorgemerkt, siehe "Risiko" im Port-Vorschlag):
      `texkom_listener.c` ruft `texkom_signal_tx()` nirgends selbst auf und übergibt auch
      nirgends eine `TexkomCoding`/`TexkomSignalCoding` — Listener und Signalpreset sind in
      diesem Entwicklungsstand strukturell unabhängig und treffen sich erst innerhalb der
      noch nicht portierten Phase-4-Schicht (`furi_hal_nfc_texkom.c`). Wie diese Schicht aus
      den rohen, über `nfc_listener_tx()` entgegengenommenen Bytes die Codierung ableitet,
      ist noch offen und als eigener Klärungspunkt für die Phase-4-Sitzung dokumentiert,
      nicht unterstellt.
- [x] Beide Dateigruppen unverändert nach
      `firmware-mining/texkom-nfc-protocol-core-port/` übernommen. Host-Repro in
      `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_listener.c`: Signalpreset
      differentiell gegen den bereits host-verifizierten Phase-1-Decoder (TK13-/TK17-/
      MMBIT-Rundlauf über je 8 UIDs, ein Negativ-Szenario, Alloc/Free-Lebenszyklus), Listener
      gegen ein gemocktes `Nfc`/`BitBuffer`/`nfc_listener_tx()` (Alloc/Free inkl.
      `nfc_config()`-Aufrufprüfung, `run()` bei Tick-Event sendet die exakten Rahmen-Byte
      genau einmal, `run()` bei Fremdereignis sendet nicht, `run()` bei gemeldetem
      Sendefehler bleibt unverändert robust). Unter `gcc -std=c17 -Wall -Wextra
      -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei `-O0`/`-O1`: 178
      Prüfungen, alle bestanden, identischer Output beider Optimierungsstufen, keine
      Compiler-Warnungen, keine Sanitizer-Befunde. Gegenprobe zur Absicherung des Repros
      selbst: eine mutierte Kopie mit vertauschter Null-/Eins-Symbolwahl lässt 53 der 178
      Prüfungen scheitern (Repro ist für echte Bitzuordnungsfehler empfindlich); eine
      zweite Mutation (Langlücke um 1 Takt-Einheit verändert) lässt alle 178 weiterhin
      bestehen (erwartungsgemäß, da `texkom_average_gaps()` seine Schwellwerte selbst aus
      den gemessenen Lücken herleitet) — beide Mutationen nur zur Absicherung durchgeführt,
      nicht Teil des gelieferten Repros.
- [x] Port-Vorschlag `firmware-mining/texkom-nfc-protocol-core-port.md` um Phase 3
      erweitert (Betroffene Dateien, Verifikation, Risiko, Port-Skizze, Quelle).
- [ ] Lokal zu tun: beide Dateigruppen gegen den eigenen Firmware-Baum anwenden, in die
      jeweilige Build-Liste (`lib/nfc`, `lib/digital_signal`) aufnehmen. Der Listener
      compiliert erst, sobald `NfcTechTexkom` existiert (kommt mit Phase 4, wie bei
      Phase 2); der Signalpreset ist davon unabhängig und bräuchte nur einen `GpioPin`.
- [ ] Phase 4 (nächster, mit Abstand größter Rückstellpunkt dieser Portierung):
      `furi_hal_nfc_texkom.c` — Low-Level-RF, Transparent-Mode auf dem STM32WB55, höchstes
      Risiko dieser gesamten Portierung (siehe "Risiko"), plus die neu benannte offene
      Frage, wie diese Schicht die Codierung aus den rohen Listener-Bytes ableitet.
- [ ] Phase 5 (danach): `NfcProtocolTexkom`-Registrierung im Protokollbaum, UI-Anbindung.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
      LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-
      Integration (blockiert an fehlender Config-Bitwert-Quelle),
      GlitchTrigger-Sweep-UI-Auto-Hit-Status, PromptZero-Installationsskript-Sichtprüfung
      vor lokalem Einsatz, Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump),
      lora-ukfe-Protokollerweiterung (`lora_rx`/`wifi_ap`/`error`) und veraltete
      `README.md`.

## Ideen-Recherche: CCID Emulator (PentestSuite & barkandbite) — neue Kategorie USB Smartcard (CCID/ISO 7816-4)

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Firmware-Feature-Mining (Texkom-NFC-Listener/Signalpreset, Phase 3 von 5), davor
      App-Verbesserung (lora-ukfe), davor Ideen-Recherche (Nearby Files) — nach dem etablierten
      Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war Ideen-Recherche für diese
      Sitzung an der Reihe. Lokale `main`-Zweigspitze war zu Sitzungsbeginn erneut hinter
      `origin/main` angezeigt (reines Checkout-Problem dieses Containers nach einem Fetch stellte
      sich heraus: `origin/main` war nach einem `git fetch` tatsächlich bereits identisch mit dem
      zuvor detachten `HEAD` — per Fast-Forward des lokalen `main`-Zeigers auf `HEAD` korrigiert,
      keine Datenverlustgefahr). Git-Identität vor dem ersten Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer bislang komplett fehlenden Katalog-Kategorie gesucht: USB-seitige
      Smartcard-*Emulation* (Gegenstück zu den bestehenden NFC/RFID-*Lesegerät*-Funden wie
      PicoPass/Seader/NFC Magic/MFKey). Erfolglos recherchiert: Zigbee/802.15.4-Sniffer (kein
      Flipper-FAP gefunden, nur die rohe BLE-genutzte STM32WB55-802.15.4-Hardwarefähigkeit ohne
      Firmware-Anbindung), GPS-Spoofing-/Jamming-Detektor (keine Flipper-App gefunden), POCSAG als
      eigenständige App (existiert nur als SubGHz-Protokolldecoder, kein Ideen-Recherche-Fall).
      Über die RogueMaster-Bündel-Verzeichnisliste (`applications/external`, sparse abgefragt)
      **CCID Emulator** (`ccid_emulator`, Autor laut `application.fam`: `PentestSuite &
      barkandbite/flipper_suite`) gefunden.
- [x] Sparse geklonter `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
      `applications/external/ccid_emulator/`): sieben Quelldateien (`ccid_emulator.c/.h`,
      `ccid_handler.c/.h`, `ccid_usb.c/.h`, `card_parser.c/.h`), 2822 Zeilen, vollständig gelesen.
      Zusätzlich das Ursprungsrepo `barkandbite/flipper_suite` separat geklont zum Abgleich — dort
      fehlen `ccid_usb.c/.h`/`card_parser.c/.h` (nur Teilbestand), dafür sieben statt zwei
      mitgelieferte Beispielprofile unter `ccid_emulator_sample_cards/`. Die
      RogueMaster-Fassung ist der vollständigere, eigenständig baubare Stand und damit als
      Grundlage der Prüfung gewählt.
- [x] API-87-Status verifiziert, dabei ein bemerkenswerter Herkunftsfund: der von der App
      benötigte `usb_ccid.h`-Header (keine der beiden Quellen liefert ihn selbst mit) kommt aus
      dem `lib/libusb_stm32`-Submodul. RogueMasters Pin dieses Submoduls (Commit `6ca2857`, "CCID:
      code formatting and doxygen docs") ist ein Commit auf dem offiziellen, upstream
      `flipperdevices/libusb_stm32`-Repo selbst (direkt auf "Add CCID support") — **exakt
      derselbe Commit**, den mein eigenes `G4MEOVER18/G4MEOVER-FW` (`main`, `b359f6f`) für
      `lib/libusb_stm32` pinnt (per `git ls-tree` auf beiden Seiten verglichen). CCID-Unterstützung
      ist damit kein RogueMaster-Fork-Sonderfall, sondern bereits Teil des offiziellen
      USB-Stack-Submoduls, den ich selbst führe. Zusätzlich in `targets/f7/api_symbols.csv`
      bestätigt: sowohl `lib/libusb_stm32/inc/usb_ccid.h` als auch `.../usbd_core.h` (die beiden
      Header, über die `ccid_usb.c` den USB-Gerätestack direkt anspricht) sind bei mir als
      freigegebene App-SDK-Header (`Header,+`) gelistet — keine Firmware-Änderung nötig.
- [x] Zweiter Herkunftsfund (folgenlos, nur dokumentiert): mein API-87.1-Stand führt noch eine
      ältere, native Firmware-HAL-Anbindung für CCID (`targets/furi_hal_include/furi_hal_usb_ccid.h`,
      Funktionen `furi_hal_usb_ccid_insert_smartcard()`/`_remove_smartcard()`/`_set_callbacks()`,
      Variable `usb_ccid`), die ein eigener Kommentar in `ccid_usb.h` der App selbst als "used to
      live in the firmware HAL" beschreibt. RogueMasters eigener, neuerer Firmwarestand (API 88.21)
      hat diese native Anbindung bereits entfernt, nur der rohe `usb_ccid.h`-Header bleibt. Für
      mich folgenlos, da die App ausschließlich ihren eigenen SDK-Header-basierten Pfad nutzt und
      keine `furi_hal_usb_ccid_*`-Funktion aufruft.
- [x] Quelltext-Review (Speichersicherheit): `card_parser.c` (Hex-/Wildcard-Parser mit explizitem
      Abbruch statt stiller Kürzung bei Überlauf, bereits aus einer früheren eigenen
      Fehlerkorrekturrunde der Autoren selbst, "issue #59"), `ccid_handler.c` (Antwortlängen vor
      `memcpy()` zusätzlich gegen die USB-Transportpuffergröße geklemmt), `ccid_emulator.c`
      (ATR-/Log-/Pfad-Pufferbau durchweg mit vorab berechneten Maximalgrößen,
      Divisionsschutz im Scroll-Balken) — kein eigener Speicherfehler gefunden. Eine zunächst wie
      Totlogik wirkende USB-VID/PID-Preset-Auswahl (`ccid_usb_presets[]`) bei genauerem Blick als
      bereits selbst ehrlich gemachte, dokumentierte Einschränkung identifiziert (Settings-Seite
      zeigt dafür nur ein festes "Default (SDK fixed)"-Label), kein Fund. Einzige
      Doku-Ungenauigkeit (kein Code-Fehler): README nennt `piv_emulator.ccid` als mitgeliefertes
      Beispielprofil, tatsächlich beim ersten Start erzeugt wird `piv_card.ccid`.
- [x] `ideas/ccid-emulator-usb-smartcard-app-status.md` angelegt (Quelle, Was, Nutzen, API-87-
      Status inkl. beider Herkunftsfunde, Quelltext-Review, Aufwand, Sicherheitsrahmen, Offen).
      Neue Katalog-Kategorie "USB Smartcard (CCID/ISO 7816-4)" in `ideas/pentest-app-katalog.md`
      ergänzt (Tabellenzeile plus Eintrag unter "Nächste Recherche-Aufträge").
- [ ] Lokal zu tun: Bauprobe (`ufbt` gegen die eigene Firmware-Basis) und Test gegen eine echte
      PC/SC-fähige Gegenstelle (`pcsc_scan`/`opensc-tool`), erwartete Kartenerkennung mit dem
      mitgelieferten `test_card.ccid`. Bei Bedarf die sieben zusätzlichen Beispielprofile aus
      `barkandbite/flipper_suite` (`ccid_emulator_sample_cards/`) manuell auf die SD-Karte
      übernehmen.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom` Phase 4-5 (`furi_hal`-
      Low-Level-RF als höchstes Risiko, Protokollbaum-Registrierung/UI), NFC/RFID-Parser-
      Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest,
      `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
      Ducky-Handler, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
      LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-
      Integration (blockiert an fehlender Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-
      Hit-Status, PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz,
      Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung
      (`lora_rx`/`wifi_ap`/`error`) und veraltete `README.md`.

## ProtoPirate — PSA: manuelle Neu-Codierung nutzte nach BF2-Fund den falschen Bruteforce-Kernel

- [x] Rückstellpunkt "PSA-Bruteforce-Kernel" vollständig abgeschlossen: nach dem bereits
      dokumentierten BF2-Counter/CRC-Konflationsfund (`psa-bf2-counter-crc-buffer8-
      conflation-fix.md`, Decoder-/Erfolgspfad-Seite) jetzt die Gegenrichtung geprüft — was
      passiert, wenn ein Bruteforce-Fund später für manuelle Codeerzeugung/erneuten Versand
      genutzt wird (`protocols/psa.c`, Encoder-Seite).
- [x] Fund: `psa_brute_force_run()` hat zwei Kernel mit grundverschiedener Schlüsselableitung
      (BF1: TEA-Zwischenverschlüsselung fester Konstanten; BF2: Schlüsselplan XOR Counter,
      16-Bit- statt 8-Bit-CRC), markiert beide Erfolgsfälle aber mit identischem
      `decrypted_type = 0x36`. `subghz_protocol_encoder_psa_deserialize()`/
      `psa_encoder_build_upload()` kennen für `Type:0x36` nur den BF1-Pfad
      (`psa_build_buffer_mode36()`) — jeder BF2-Fund erzeugt beim manuellen Neu-Erzeugen/
      erneuten Senden damit einen am echten Empfänger ungültigen Key1/Key2. Algebraisch
      hergeleitet, dass BF2 (wie bereits BF1) seinen Such-Counter allein aus dem Serial
      rekonstruieren kann (`PSA_CRYPTO_BF2_START | (serial & 0xFFFFFF)`) — der Fix braucht
      also nur einen neuen Typ-Marker (`0x37`) und eine zweite Encoder-Funktion, keine neue
      Persistenz.
- [x] Host-Repro (`repro-psa-bf-mode36-wrong-kernel-reencode/repro.c`): 120 synthetische
      BF1- und 120 BF2-Fälle, "echter Sender" per Vorwärts-TEA-Verschlüsselung simuliert,
      gegen die wörtlich unveränderte BF1/BF2-Erfolgsprüfung aus `psa_brute_force_run()`
      verifiziert, dann durch ALT- (Status quo) und NEU-Encoder-Pfad zurückgeführt und
      byteweise verglichen. BF1 rundet in 120/120 Fällen korrekt (Regression), BF2 mit ALT in
      0/120, mit NEU in 120/120. Mutationsprobe (BF2-Schlüssel-XOR → AND) lässt gezielt genau
      die 480 BF2/NEU-spezifischen Prüfungen scheitern, keine BF1-Prüfung. Unter
      `gcc -fsanitize=address,undefined` bei `-O0`/`-O1`, exit 0.
- [x] `app-improvements/ProtoPirate/psa-bf-mode36-wrong-kernel-reencode-fix.md` angelegt
      (Was, Nutzen, betroffene Dateien, vollständiger Patch, Risiko, Testhinweis, Zusätzlich
      geprüft, Quelle) plus Patches (`psa_crypto_bf_c.patch`, `psa_c.patch`) und Host-Repro.
      Nebenfund dokumentiert: `ENABLE_EMULATE_FEATURE` ist in der eigenen `defines.h`
      entgegen einer Angabe der vorherigen Sitzung tatsächlich aktiv — der Encoder-Pfad ist
      live kompilierter Code, keine tote Funktion.
- [ ] Lokal zu tun: beide Patches gegen den eigenen `ProtoPirate`-Stand anwenden, bauen
      (`ufbt`), und — sobald lokal ein per BF2 entschlüsselter Fund vorliegt — dessen
      manuelle Neu-Codierung gegen eine Gegenstelle mit bekanntem Schlüssel prüfen (vor dem
      Fix abgelehnt, danach akzeptiert).
- [ ] Die reverse-engineerten TEA-Schlüsselplan-Konstanten selbst (`psa_crypto_bf1_key_
      schedule`/`psa_crypto_bf2_key_schedule`, `PSA_CRYPTO_BF1_CONST_U4/U5`) bleiben bewusst
      zurückgestellt — eigener, als riskant eingestufter Bereich, kein offener Fix-Punkt
      dieser beiden Sitzungen, sondern separat zu bewertendes Kryptografie-Risiko.

## Texkom NFC-Protokollfamilie — Phase 4a: Anschlussstücke statt Blindflug auf Phase 4

- [x] Für diese Sitzung war laut eigener Port-Skizze eine auf `furi_hal_nfc_texkom.c`
      fokussierte Register-/Timing-Prüfung gegen die eigene `furi_hal_nfc_iso14443a.c`/
      `furi_hal_nfc_iso15693.c` vorgesehen (Phase 4, höchstes Risiko dieser Portierung).
      Durchgeführt — mit einem Ergebnis, das die Port-Skizze selbst verändert: die Datei
      hängt an drei Stellen von Code ab, der bei mir noch nicht existiert und nicht Teil
      der ursprünglich als "Phase 4" benannten einen Datei ist. Ohne diese drei Stellen
      würde `furi_hal_nfc_texkom.c` bei mir gar nicht bauen, unabhängig von der
      Register-Prüfung selbst.
- [x] Treiberbasis (`lib/drivers/st25r3916.c/.h/_reg.c/.h`) gegen einen frischen
      Unleashed-`dev`-Stand (Commit `525dcce0e461d4f4b852a32cc6e3b4d075b50f17`,
      2026-10-20) per `diff -q` Byte-für-Byte identisch bestätigt — jedes von
      `furi_hal_nfc_texkom.c` verwendete Register-/Bit-Symbol existiert bei mir bereits,
      kein neues Register nötig.
- [x] Acht Registerwerte Zeile für Zeile gegen die eigene, bereits gebaute
      ISO15693-/ISO14443A-Poller-/Listener-Konfiguration gegengerechnet: sieben von acht
      sind wortwörtliche Wiederverwendung (MODE/RX_CONF1/RX_CONF2/RX_CONF4/CORR_CONF1/
      AUX_MOD identisch zu ISO15693; CORR_CONF2 spiegelbildlich dazu, passend zu
      scf_sc212 statt scf_sc424). Einzige echte, neue Abweichung: `RX_CONF3` (AM-Kanal-
      Dämpfung, 3 von 7 Stufen auf der Poller-RX-Seite) — laut eigenem Quelltextkommentar
      beim Hardware-Bringup empirisch ergänzt, nicht durch Quelltextvergleich
      verifizierbar, bleibt der einzige Punkt, der nur an echter Hardware mit
      Spektrumanalysator/Oszilloskop zu prüfen ist.
- [x] Richtigstellung zur vorherigen Fassung des Risikopunkts: "Transparent Mode bereits
      für ISO14443A/ISO15693 genutzt" war so nicht korrekt — bei mir nutzt ihn bislang nur
      der ISO15693-*Listener*, nicht ISO14443A (per `grep` über den gesamten eigenen Baum
      bestätigt).
- [x] Drei vorgelagerte Abhängigkeitslücken gefunden und als eigene, additive bzw.
      nachweislich verhaltenserhaltende Patches geschlossen:
      (1) `NfcTechTexkom`/`FuriHalNfcTechTexkom` (zwei neue Enum-Werte) und
      `nfc_texkom_poller_rx()` (neue Funktion) — die in Phase 2/3 bereits als "kommt erst
      mit Phase 4" benannten Anschlusspunkte, jetzt gegen den exakten Unleashed-`dev`-
      Quelltext identifiziert und übernommen; (2) `furi_hal_nfc_transparent_mode_enter/
      _exit` generalisiert — bei mir bislang nur ISO15693-Listener-lokal (`static`) unter
      anderem Namen vorhanden, Funktionskörper per `diff` Zeichen-für-Zeichen identisch zur
      neuen generischen Fassung bestätigt, reine Hochzieh-Refaktorierung ohne neue Logik;
      (3) `lib/signal_reader/signal_reader.c` behandelt den bei mir bereits vorhandenen
      Enum-Wert `SignalReaderTriggerNone` in `start()`/`stop()` bislang nicht gesondert
      (schaltet immer den EXTI-/DMA-Resync-Pfad scharf) — `signal_reader_alloc()` hat bei
      mir aktuell keinen einzigen Aufrufer außerhalb der eigenen Bibliothek (per `grep`
      über den gesamten Baum bestätigt), die Lücke hat bislang niemanden gestört und der
      Fix trägt deshalb kein Regressionsrisiko.
- [x] Bewusst nicht mitgenommen: eine im selben Unleashed-`dev`-Diff mitgekommene, zu
      Texkom unabhängige ISO15693-Funktionsgruppe (`detect_mode`/`force_1outof4`/
      `force_1outof256`, Parser-Modusauswahl) sowie eine Konstanten-Umbenennung
      (`FURI_HAL_NFC_ISO15693_BIT_LEN`) — reine parallele Weiterentwicklung desselben
      Dateibestands, nichts mit Texkom zu tun.
- [x] Neun Einzelpatches (`firmware-mining/texkom-nfc-protocol-core-port/
      phase4a-connective-patches/`: `nfc_h.patch`, `nfc_c.patch`, `signal_reader_h.patch`,
      `signal_reader_c.patch`, `furi_hal_nfc_h.patch`, `furi_hal_nfc_i_h.patch`,
      `furi_hal_nfc_tech_i_h.patch`, `furi_hal_nfc_c.patch`, `furi_hal_nfc_iso15693_c.patch`)
      gegen vier unabhängige, frische Zweitkopien von `G4MEOVER18/G4MEOVER-FW` (`main`,
      unverändert `b359f6f`) mit `git apply --check` sowohl einzeln als auch als
      zusammengefasster Diff verifiziert, real angewendet, alle neun Zieldateien per
      `md5sum` über diese vier Kopien verglichen — durchgängig identisch.
      `firmware-mining/texkom-nfc-protocol-core-port.md` entsprechend um Phase 4a erweitert
      (neue "Betroffene Dateien"/"Verifikation"-Abschnitte, Risiko- und Port-Skizze-
      Nummerierung auf 4a/4b aufgeteilt).
- [ ] Lokal zu tun: die neun Phase-4a-Patches anwenden, dann einen ersten Build-Versuch
      (`ufbt`) — Phase 2 (Poller) und Phase 3 (Listener/Signalpreset) sollten jetzt
      compilieren, solange nur noch `furi_hal_nfc_texkom.c` (Phase 4b) fehlt. Jeder
      verbleibende Compile-Fehler an dieser Stelle wäre ein eigener Fund, den diese
      Sitzung übersehen hat.
- [ ] Neuer, enger umrissener Rückstellpunkt: Phase 4b (`furi_hal_nfc_texkom.c` selbst,
      527 Zeilen) — Anschlusspunkte und sieben von acht Registerwerten sind jetzt geklärt,
      offen bleibt nur noch `RX_CONF3`, das sich nur an echter Hardware bestätigen lässt,
      sowie Phase 5 (Protokollbaum-Registrierung/UI).

## Ideen-Recherche: Bambu RFID (DanTheMan827) — neue Zeile unter NFC/RFID

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Firmware-Feature-Mining (Texkom-NFC-Protokollkern, Phase 4a), davor App-Verbesserung
      (ProtoPirate, PSA-Bruteforce-Kernel-Fix), davor Ideen-Recherche (CCID Emulator) — nach
      dem etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war
      Ideen-Recherche für diese Sitzung an der Reihe. Git-Identität vor dem ersten Commit
      gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer bislang fehlenden Unterkategorie innerhalb NFC/RFID gesucht:
      Dekodierung eines proprietären Hersteller-Tag-Formats über eine bereits bekannte,
      UID-abgeleitete Schlüsselherleitung — Gegenstück zu Key-Recovery (MFKey/Nested) und
      Klon-/Reset-Funktion (NFC Magic). Erfolglos recherchiert: ein Flipper-FAP für
      Zigbee/802.15.4-Sniffing über ein CC2531-Dongle (keine Verbindung zwischen
      CC2531-Sniffing-Material und Flipper Zero in den Suchergebnissen, nur allgemeine
      TI-/Wireshark-Dokumentation zum Dongle selbst).
- [x] **Bambu RFID** (`DanTheMan827/bambu-rfid-flipper`, Commit `7709f2a`, 2026-08-15, Version
      `0.2.0`) gefunden — liest Bambu-Lab-Filamentspulen-RFID-Tags (MIFARE Classic 1K) über die
      eingebaute Antenne, leitet alle 16 Sektor-Key-A/-B-Paare aus der 4-Byte-UID her
      (HMAC-SHA256/HKDF, öffentlich dokumentierter fester Master-Schlüssel), dekodiert
      Material/Filamenttyp/Farbe/Temperaturen/Produktionsdatum/Tray-UID, exportiert ein
      Fünf-Datei-Bündel (Rohdump/JSON/`.nfc`/Keys/README), ohne die zusätzliche RSA-Signatur zu
      fälschen. Zwölf Quelldateien vollständig gelesen.
- [x] Auch über RogueMaster verbreitet (`applications/external/bambu_rfid/`, Branch `420`) —
      beide Fassungen unabhängig geklont, alle zwölf Quelldateien per `diff -b -B`
      (Whitespace-unabhängig) gegenübergestellt: durchgängig identisch bis auf
      `clang-format`-Zeilenumbrüche, keine funktionale Abweichung. Ursprungsrepo als primäre
      Quelle gewählt.
- [x] API-87-Status verifiziert gegen `G4MEOVER18/G4MEOVER-FW` (`main`, `b359f6f`): alle
      verwendeten NFC-/MIFARE-Classic-/Storage-Symbole per `grep` bestätigt vorhanden, die
      beiden Kern-Funktionssignaturen (`mf_classic_poller_sync_detect_type(Nfc*,
      MfClassicType*)`, `mf_classic_poller_sync_read(Nfc*, const MfClassicDeviceKeys*,
      MfClassicData*)`) wörtlich gegen `mf_classic_poller_sync.h` abgeglichen — exakte
      Parameterreihenfolge/-typen. Krypto-Abhängigkeit (`bambu_crypto.cpp`) ruft nur
      `mbedtls_md_info_from_type()`/`mbedtls_md_hmac()` auf (eigene manuelle HKDF-Expansion,
      kein `mbedtls_hkdf()`); `lib/mbedtls_cfg.h` hat `MBEDTLS_MD_C`/`MBEDTLS_SHA256_C` bereits
      aktiviert. Kein Firmware-Patch nötig, keine Zusatzhardware.
- [x] Quelltext-Review (Speichersicherheit): alle zwölf Quelldateien gelesen, kein eigener Fund
      — jeder `snprintf()`-Aufruf prüft Rückgabewert und Zielgröße vor Weiterverwendung,
      Pfad-Namen werden vor Dateizugriffen gegen `safe_name()`/`safe_relative_path()` gefiltert,
      die UID wird nach dem Lesen erneut gegen die vor dem Scan erfasste UID verglichen, ein
      `MfClassicErrorPartialRead` wird nur akzeptiert, wenn zusätzlich alle 64 Blöcke als
      gelesen markiert sind. Einzige Beobachtung (keine Fehlinterpretation): Mehrfarben-Spulen
      mit `color_count` > 2 zeigen den Zähler korrekt an, aber ohne zusätzliche Farbwerte
      jenseits der zweiten Farbe — dokumentierte Funktionsgrenze.
- [x] `ideas/bambu-rfid-filament-tag-app-status.md` angelegt (Quelle, Was, Nutzen, API-87-Status,
      Quelltext-Review, Aufwand, Sicherheitsrahmen, Offen). `ideas/pentest-app-katalog.md`
      erweitert: neue Zeile unter "NFC / RFID", neuer Rechercheauftrag-Eintrag am Ende.
- [ ] Lokale Bauprobe (`ufbt`) und Testlauf gegen eine eigene Bambu-Lab-Filamentspule weiterhin
      offen.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler, neun verbleibende
      FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration
      (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz,
      Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung
      (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-Schlüsselplan-Konstanten (bewusst
      zurückgestellt), Phase 4b (`furi_hal_nfc_texkom.c`, `RX_CONF3`) und veraltete
      `README.md`.

## App-Verbesserung: usb-army-penetrator — RAW_HID-Handler übernimmt dieselbe ungeprüfte `std::stoi()`-Absturzklasse wie der bereits behobene Settings-Pfad

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Ideen-Recherche (Bambu RFID), davor Firmware-Feature-Mining (Texkom Phase 4a), davor
      Ideen-Recherche (CCID Emulator) — nach dem etablierten Rotationsmuster (Firmware →
      Ideen → App → ...) war App-Verbesserung für diese Sitzung an der Reihe. Git-Identität
      vor dem ersten eigenen Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Den seit der `Fortsetzung 2`-Sitzung (DISPLAY_TEXT-Fund) explizit als eigenständigen,
      noch nicht vertieften Rückstellpunkt benannten `handleRawHid()` (`RAW_HID`-Ducky-
      Befehl, `src/Attacks/Ducky/Extensions.h`) aufgegriffen — einer der seit mehreren
      Sitzungen unter "`usb-army-penetrator`s restliche Ducky-Handler" gesammelten offenen
      Punkte. Frischen Klon von `G4MEOVER18/usb-army-penetrator` (`main`, unverändert
      Commit `411b153`) gezogen, Funktion vollständig gelesen.
- [x] **Fund**: `handleRawHid()` übergibt das `"0x"`-präfixierte Hex-Argument ungeprüft an
      `std::stoi(hidStr, nullptr, 16)` — dieselbe Schwachstellenklasse wie der bereits
      behobene `settings-int-uint16-stoi-uncaught-exception-crash-fix` (`Settings.cpp`):
      `std::stoi()` prüft den `int`-Zielbereich zusätzlich zur `strtol`-Konvertierung und
      wirft `std::out_of_range`, sobald der Hex-Wert `INT_MAX` (`0x7FFFFFFF`) übersteigt,
      z. B. bei `RAW_HID 0x80000000` oder `RAW_HID 0xFFFFFFFF`. Per `grep -rn "catch\s*(\|
      try\s*{" src/` erneut bestätigt: weiterhin kein `try`/`catch` im gesamten Projekt,
      `platformio.ini` setzt für keine Umgebung `-fno-exceptions` — die Ausnahme läuft bis
      `std::terminate()` durch und reißt die gesamte Firmware ab, erreichbar über jedes
      `.ds`-Payload (eigenes Skript, `RUN_PAYLOAD`-Nachladen, Tippfehler). Dieselbe bereits
      beim Settings.cpp-Fund dokumentierte `strtol`-Präfixregel gegengeprüft und bewusst
      **nicht** erneut als Fund behauptet: `"0x"` allein bzw. `"0xZZZZ"` werfen nicht,
      werden stillschweigend als `0x00` interpretiert — kein Absturzpfad. Gezielt nach
      weiteren `std::stoi()`/`std::stol()`-Aufrufstellen in derselben Datei gesucht: keine
      weitere (`asciiOrVariableToInt()` nutzt `atoi()`, das bei Fehlparse `0` statt eines
      Wurfs liefert — nicht betroffen).
- [x] Fix (additiv, identisches Muster wie beim Settings.cpp-Fix): `try`/`catch` um den
      `std::stoi()`-Aufruf, loggt denselben Stil wie der bereits vorhandene
      "Invalid value"-Zweig, `return false` statt Absturz. Ein neuer `#include <stdexcept>`.
      Patch gegen zwei frische, unabhängige Zweitkopien von `G4MEOVER18/usb-army-penetrator`
      (`main`, unverändert `411b153`) mit `git apply --check` verifiziert, real angewendet,
      per `md5sum` abgeglichen — identisch.
- [x] Host-Repro (reine `std::stoi()`/Ausnahme-Logik 1:1 aus dem `hidStr`-Zweig übernommen,
      kein Arduino/TinyUSB/HID-Objekt nötig) unter
      `gcc -std=c++17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: 27 Prüfungen (drei Fund-Szenarien ≥ `0x80000000`, zwei
      `strtol`-Präfix-Gegenproben, sechs ALT- und sechs NEU-Regressionsfälle, davon vier
      paarweise identisch), alle bestanden, identischer Output beider Optimierungsstufen,
      keine Compiler-Warnungen, keine Sanitizer-Befunde.
- [x] Details, Fund, Nutzen, Patch, Host-Repro, Risiko, Sicherheitsrahmen, Testhinweis in
      `app-improvements/usb-army-penetrator/raw-hid-stoi-uncaught-exception-crash-fix.md`,
      Patch in
      `app-improvements/usb-army-penetrator/raw-hid-stoi-uncaught-exception-crash-fix/extensions_h.patch`,
      Repro in
      `app-improvements/usb-army-penetrator/repro-raw-hid-stoi-uncaught-exception/repro.cpp`.
- [ ] Lokal zu tun: Patch gegen die eigene `usb-army-penetrator`-Basis anwenden, mit der
      jeweiligen Plattform-`platformio.ini`-Umgebung bauen; ein `.ds`-Testskript mit
      `RAW_HID 0x80000000` laufen lassen (vor dem Fix Neustart samt C++-Abort im
      Serial-Monitor, nach dem Fix kontrolliert abgelehnt und Skriptausführung läuft
      weiter), danach `RAW_HID 0x04` (gültiger HID-Code) zur Regressionsprobe.
- [ ] `handleRawHid()` ist damit abgeschlossen; aus derselben, seit der `Fortsetzung
      2`-Sitzung offenen Liste bleiben `handleSetSetting()` (Ducky-Dispatch, der
      eigentliche `std::stoi()`-Aufruf dahinter in `Settings.cpp` bereits behoben,
      die Argument-Aufteilung selbst in `Extensions.h` aber noch nicht gezielt geprüft)
      sowie die `AGENT_RUN`/`ESP32M`/`RUN_PAYLOAD`-Dispatch-Handler als engerer
      Nachfolge-Rückstellpunkt für "`usb-army-penetrator`s restliche Ducky-Handler".
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
      LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-
      Integration (blockiert an fehlender Config-Bitwert-Quelle),
      GlitchTrigger-Sweep-UI-Auto-Hit-Status, PromptZero-Installationsskript-
      Sichtprüfung vor lokalem Einsatz, Nearby-Files-Versionslücke v1.5→v1.7
      (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung (`lora_rx`/`wifi_ap`/`error`),
      PSA-Bruteforce-TEA-Schlüsselplan-Konstanten (bewusst zurückgestellt), Phase 4b
      (`furi_hal_nfc_texkom.c`, `RX_CONF3`), Texkom-Phase 5 (Protokollbaum-Registrierung/
      UI), Bambu-RFID-Lokalbau/-test und veraltete `README.md`.

## Texkom NFC-Protokollfamilie — Phase 4b: `furi_hal_nfc_texkom.c` unverändert übernommen, Abtast-/Zeitmess-Zustandsmaschine erstmals host-verifiziert

- [x] Mit Phase 4a (Anschlussstücke, Registerprüfung) aus der vorherigen Sitzung abgeschlossen,
      war für diese Sitzung laut eigener Port-Skizze `furi_hal_nfc_texkom.c` selbst (Phase 4b,
      höchstes Risiko dieser Portierung) an der Reihe. Datei erneut über zwei unabhängige Abrufe
      geladen: Prüfsumme `38c7f707f2d8c9156cdd726068cc9644`, 527 Zeilen — identisch zum in
      Phase 4a bereits geprüften Stand, keine Änderung seither. Vollständig gelesen und in drei
      Gruppen zerlegt: reine Registerkonfiguration (bereits in Phase 4a Register für Register
      gegengerechnet), reine Hardwareansteuerung ohne eigene Logik (delegiert an bereits
      vorhandene/generalisierte Funktionen), und eine dritte Gruppe, die bislang nicht als solche
      erkannt war — die Abtast-/Zeitmess-Zustandsmaschine, die aus dem vom Analogfrontend bei
      500 ksps abgetasteten Bitstrom die Lückenliste rekonstruiert (`furi_hal_nfc_texkom_feed()`/
      `_handle_impulse()`/`_cut_frame()`), ist reine, hardwareunabhängige Kontrolllogik ohne
      jeden Registerzugriff — genau wie Phase 2s Poller-Dispatch, nur bislang übersehen, weil die
      Phase-4a-Sitzung ausschließlich auf die Register-/Timing-Prüfung fokussiert war.
- [x] Diese Zustandsmaschine zusammen mit zwei kleineren reinen Dispatch-Funktionen
      (`furi_hal_nfc_texkom_coding()`, der Rahmengrößen-/Codierungs-Dispatch in
      `_listener_tx()`, mit gemocktem `texkom_signal_tx()` — dessen eigene Logik bereits in
      Phase 3 verifiziert ist) jetzt erstmals host-verifiziert. Beim Lesen auf Speicherfehler/
      Off-by-one geprüft: `gaps[]` (127 Byte) durchgängig durch `gap_count < MAX_GAPS` vor dem
      Schreiben geschützt, `since_impulse` kann vor dem Speichern nachweislich nie über
      `FRAME_GAP` (100, laut `_Static_assert` klein genug für ein Byte) steigen, die
      Fast-Path-Abkürzung für durchgängig-Low-Bytes gegen den langsamen Bit-für-Bit-Pfad von
      Hand durchgerechnet und für äquivalent befunden. **Kein eigener Fund.**
- [x] Host-Repro in
      `firmware-mining/repro-texkom-nfc-protocol-core-port/repro_furi_hal_texkom.c` (die drei
      Zustandsmaschinen-Funktionen sowie `_coding()` unverändert transkribiert, `_listener_tx()`
      mit gemocktem `texkom_signal_tx()`; echte `FuriHalSpiBusHandle`/`SignalReader`/
      `furi_thread_flags_*`-Objekte bleiben vollständig außen vor, da diese Funktionen keine
      davon aufrufen) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: 445 Prüfungen (TK13-/TK17-förmiger Rahmen, Fast-/Slow-Path-Äquivalenz,
      Overflow-Ablehnung, Müllzug gefolgt von gültigem Zug im selben Aufruf,
      Refraktärzeit-Absorption, früher Rückgabepfad nach gültigem Schnitt, Aufteilung über
      mehrere `feed()`-Aufrufe wie die echten 32-Byte-Blöcke, Codierungswahl,
      Listener-tx-Dispatch inkl. Rahmengrößenprüfung), alle bestanden, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde. Drei
      Mutationsproben zur Absicherung des Repros selbst durchgeführt: zwei (Fast-Path-
      Akkumulationsfehler, TK13-Lückenzahl-Vergleich um eins verschoben) jeweils sofort von
      402 bzw. den gezielt betroffenen Prüfungen aufgefangen; eine dritte (Refraktärzeit-
      Vergleichsoperator statt Konstante geändert) bewusst nicht aufgefangen und als für das
      reale Signal irrelevanter Exakt-Grenzfall erklärt (Refraktärzeit mit 2x Sicherheitsabstand
      zum kürzesten realen Lückenabstand gewählt), statt stillschweigend übergangen.
- [x] Datei unverändert nach
      `firmware-mining/texkom-nfc-protocol-core-port/phase4b-furi-hal/furi_hal_nfc_texkom.c`
      übernommen. `firmware-mining/texkom-nfc-protocol-core-port.md` entsprechend erweitert:
      Phase 4b von "künftige Sitzung" auf "geliefert" verschoben, Risikoabschnitt für diese
      Datei in zwei getrennte Einträge aufgeteilt (niedrig für die jetzt verifizierte Logik,
      mittel für die verbleibende, nur an Hardware prüfbare Registeransteuerung inkl.
      `RX_CONF3`), Port-Skizze aktualisiert (Phase 5 jetzt einziger offener Schritt).
- [ ] Lokal zu tun: Phase 4a-Patches plus Phase 4b anwenden, dann einen ersten vollständigen
      Build-Versuch (`ufbt`) — Phase 1-4b sollten jetzt zusammen durchlaufen, solange nur noch
      Phase 5 (Protokollbaum-Registrierung/UI) fehlt. Jeder verbleibende Compile-Fehler an
      dieser Stelle wäre ein eigener Fund, den diese Sitzung übersehen hat. Erste Lesetests mit
      einem Spektrumanalysator/Oszilloskop am Antennenausgang begleiten, um `RX_CONF3` (die
      einzige noch nicht quelltextseitig bestätigte Registerabweichung) tatsächlich zu
      validieren.
- [ ] Letzter offener Schritt dieser Portierung: Phase 5 (`lib/nfc/protocols/nfc_protocol.c/.h`
      — `NfcProtocolTexkom` als neuen Basisknoten registrieren, plus
      `applications/main/nfc/helpers/protocol_support/texkom/*`, UI-Anbindung).

## Ideen-Recherche: Evil Portal (bigbrodude6119) — neue Zeile unter WiFi

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Firmware-Feature-Mining (Texkom-NFC-Protokollfamilie, Phase 4b), davor App-Verbesserung
      (usb-army-penetrator, RAW_HID-`std::stoi()`-Fix), davor Ideen-Recherche (Bambu RFID) —
      nach dem etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war
      Ideen-Recherche für diese Sitzung an der Reihe. Git-Identität vor dem ersten Commit
      gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer Ergänzung für die bislang nur drei Einträge (Marauder/GhostESP-UI,
      Wendigo, Rogue AP Detector) enthaltende Kategorie "WiFi" gesucht. Mehrere Kandidaten
      erfolglos/als bereits bekannt verworfen (DAP Link — bereits im eigenen
      `applications/external/dap_link` enthalten, kein neuer Fund; `Idorobots/ofw-flipperzero-nrf24`
      — Fork von `mothball187/flipperzero-nrf24`, bereits katalogisiert; Fault-Injection-/
      LoRa-/EMV-Skimmer-Suchen lieferten nur bereits bekannte Treffer).
- [x] **Evil Portal** (`bigbrodude6119/flipper-zero-evil-portal`, `main`, Commit `d196f60`,
      2023-07-17, MIT, 2,4k Sterne/286 Forks) gefunden — WLAN-Captive-Portal mit gefälschter
      Login-Seite über ein per UART angeschlossenes ESP32-S2-/WROOM-Board, SD-Protokollierung
      eingegebener Zugangsdaten, Community-Vorlagenbibliothek. Flipper-`.fap` (729 Zeilen über
      sechs Dateien) vollständig gelesen.
- [x] API-87-Status verifiziert gegen `G4MEOVER18/G4MEOVER-FW` (`main`, `b359f6f`): die
      UART-Schicht (`evil_portal_uart.c`) nutzt die alte direkte `furi_hal_uart_tx()`/
      `_set_br()`/`_set_irq_cb()`/`furi_hal_console_disable()`/`_enable()`-API — per
      Volltextsuche über den gesamten eigenen Quellbestand bestätigt, dass keine dieser vier
      Funktionen mehr existiert (nur noch die Handle-basierte `furi_hal_serial_*`-API, an sechs
      Stellen im eigenen Baum verifiziert, u. a. `expansion_worker.c`/`uart_echo.c`/
      `subghz_gps.c`). Echte Portierungsarbeit nötig, kein firmwareseitiger Kompat-Shim wie bei
      SWD Probe/GPIO Logic Analyzer (alte CLI-API) mehr vorhanden.
- [x] Quelltext-Review (Speichersicherheit): zwei gleichartige Lecks gefunden — sowohl im
      SET_AP_CMD-Zweig (`evil_portal_uart.c:71`) als auch im SET_HTML_CMD-Zweig
      (`scenes/evil_portal_scene_console_output.c:117`, dem eigentlichen "Start Portal"-Pfad)
      wird ein per `furi_string_alloc()` erzeugtes `FuriString*` mit `free()` statt
      `furi_string_free()` freigegeben; der separat allozierte Inhaltspuffer (bei SET_HTML_CMD
      die komplette geladene Phishing-Seite) bleibt dabei hängen. Beide Fundstellen liegen im
      Kern-Workflow, nicht in einem Randfall.
- [x] Host-Repro in `ideas/repro-evil-portal-furi-string-free-leak/repro.c` (Stand-in-Nachbildung
      der realen `FuriString`-Semantik aus `furi/core/string.c`, beide Fundstellen 1:1
      nachgebaut, je 50 simulierte "Start Portal"-Zyklen) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,leak,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`, ALT/NEU in getrennten Prozessen: ALT 8250 Byte über 100 Allokationen
      (1200 Byte SET_AP_CMD, 7050 Byte SET_HTML_CMD), NEU 0 Byte, identisch bei beiden
      Optimierungsstufen. Zusätzlicher Regressionsnachweis für das Stand-in-Modell selbst (kurzer
      unpräfigierter Inhalt bleibt inline, kein Leck).
- [x] Fix-Skizze (zwei additive Einzeiler, `free()` → `furi_string_free()`) inkl. Patches gegen
      eine frische, unabhängige Zweitkopie mit `git apply --check` verifiziert, real angewendet.
      `ideas/evil-portal-wifi-captive-portal-app-status.md` angelegt (Quelle, Was, Nutzen,
      API-87-Status, Quelltext-Review, Host-Repro, Fix-Skizze, Aufwand, Sicherheitsrahmen,
      Offen). `ideas/pentest-app-katalog.md` erweitert: neue Zeile unter "WiFi", neuer
      Rechercheauftrag-Eintrag am Ende.
- [ ] Lokale Portierung der UART-Schicht auf `furi_hal_serial_*` sowie anschließende Bauprobe
      (`ufbt`) und Testlauf gegen ein eigenes ESP32-S2-/WROOM-Board weiterhin offen.
      Funktionsvergleich mit der bereits katalogisierten Marauder-EvilPortal-Funktion
      (SD-Protokollarchitektur/Vorlagenvielfalt) ebenfalls offen.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler, neun verbleibende
      FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration
      (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz,
      Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung
      (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-Schlüsselplan-Konstanten (bewusst
      zurückgestellt), Texkom-Phase 5 (Protokollbaum-Registrierung/UI) und veraltete
      `README.md`.

## App-Verbesserung: usb-army-penetrator — SET_SETTING_STRING verwirft jeden mehrwortigen Wert statt ihn zu übernehmen

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Ideen-Recherche (Evil Portal), davor Firmware-Feature-Mining (Texkom Phase 4b), davor
      App-Verbesserung (usb-army-penetrator, RAW_HID-`std::stoi()`-Fix) — nach dem etablierten
      Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war App-Verbesserung für diese
      Sitzung an der Reihe. Git-Identität vor dem ersten eigenen Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Den seit der `RAW_HID`-Sitzung explizit als engeren Nachfolge-Rückstellpunkt benannten
      `handleSetSetting()` (Ducky-Dispatch für `SET_SETTING_BOOL`/`_INT16`/`_UINT16`/`_STRING`,
      `src/Attacks/Ducky/Extensions.h`) aufgegriffen — der eigentliche `std::stoi()`-Aufruf
      dahinter in `Settings.cpp` ist bereits behoben, die Argument-Aufteilung in der Funktion
      selbst war bislang nicht gezielt geprüft. Frischen Klon von
      `G4MEOVER18/usb-army-penetrator` (`main`, unverändert Commit `411b153`) gezogen, Funktion
      vollständig gelesen.
- [x] **Fund**: `handleSetSetting()` zerlegt die komplette Zeile über `Ducky::SplitString()`
      (aus der eingebundenen `i-am-shodan/DuckScriptInterpreter`-Bibliothek, Pin-Commit
      `23aa3ba8` laut `platformio.ini`, `DuckyParse.h` gelesen) an jedem Whitespace-Zeichen ohne
      Anführungszeichen-Unterstützung und verlangt danach exakt drei Teile. Für
      `SET_SETTING_STRING <Name> <Wert>` heißt das: jeder Wert mit mehr als einem Wort erzeugt
      mehr als drei Teile und die ganze Zeile wird verworfen (`totalErrors++`, Einstellung
      bleibt unverändert) — obwohl der Rest der Zeile klar als ein einziger String-Wert gemeint
      war. Per `grep -rn "registerUserConfigurableSetting.*SettingType::String"` bestätigt, dass
      das reale, bereits registrierte Einstellungen betrifft: `WIFI_AP_NAME`/`WIFI_AP_PWD`
      (`HardwareWiFi.cpp`) und `USB_DeviceManufacturer`/`USB_DeviceProductDescriptor`
      (`USBCore.cpp`) — alle vier plausibel mehrwortig. `setSettingValue()`
      (`Settings.cpp:50-53`) selbst ist für den `String`-Typ uneingeschränkt
      (`prefs.putString()` ohne weitere Prüfung); die Einschränkung entsteht ausschließlich
      durch die Zerlegung in `handleSetSetting()`. Gezielt gegengeprüft und bewusst **nicht**
      als weiteren Fund behauptet: mehrere Leerzeichen zwischen Name und Wert erzeugen durch
      `Ducky::SplitString()` keine leeren Zwischen-Token, `SET_SETTING_BOOL`/`_INT16`/`_UINT16`
      (deren Werte nie Leerzeichen enthalten dürfen) sind unverändert nicht betroffen.
- [x] Fix (additiv): Für `SET_SETTING_STRING` wird die Mindestanzahl statt der exakten Anzahl
      Teile geprüft (`split.size() < 3` statt `!= 3`, weiterhin `!= 3` für die anderen drei
      Varianten — deren Verhalten bleibt dadurch byte-identisch), der Wert wird dann aus der
      Originalzeile neu ausgeschnitten statt aus `split[2]` übernommen (ab dem Ende des
      gefundenen Namens den trennenden Whitespace überspringen, Rest der Zeile unverändert als
      Wert). Patch gegen zwei frische, unabhängige Zweitkopien von
      `G4MEOVER18/usb-army-penetrator` (`main`, unverändert `411b153`) mit `git apply --check`
      verifiziert, real angewendet, per `md5sum` abgeglichen — identisch.
- [x] Host-Repro (`handleSetSetting()` in ALT-/NEU-Form sowie `Ducky::SplitString()` 1:1
      transkribiert, `setSettingValue()` durch einen aufzeichnenden Stand-in ersetzt, kein
      `Preferences`/NVS-Objekt nötig) unter
      `gcc -std=c++17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: 19 Prüfungen (drei Fund-Szenarien — WiFi-Passwort/-AP-Name/USB-Hersteller
      mit mehrwortigem Wert, ALT verwirft/NEU übernimmt korrekt —, eine Gegenprobe mit mehreren
      inneren Leerzeichen, zwei `SET_SETTING_STRING`-Regressionsfälle, zwölf Regressionsfälle
      über `BOOL`/`INT16`/`UINT16` mit paarweise identischem Rückgabewert/Name/Wert/
      Fehlerzähler zwischen ALT und NEU), alle bestanden, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde. Mutationsprobe
      (Entfernen des Whitespace-Überspringens nach dem Namen) lässt die erste Prüfung sofort mit
      einem führenden Leerzeichen im übernommenen Wert fehlschlagen — bestätigt, dass die
      Prüfung diesen Teil der Logik tatsächlich abdeckt.
- [x] Details, Fund, Nutzen, Patch, Host-Repro, Risiko, Sicherheitsrahmen, Testhinweis in
      `app-improvements/usb-army-penetrator/set-setting-string-whitespace-value-fix.md`, Patch
      in
      `app-improvements/usb-army-penetrator/set-setting-string-whitespace-value-fix/extensions_h.patch`,
      Repro in
      `app-improvements/usb-army-penetrator/repro-set-setting-string-whitespace-value/repro.cpp`.
- [ ] Lokal zu tun: Patch gegen die eigene `usb-army-penetrator`-Basis anwenden, mit der
      jeweiligen Plattform-`platformio.ini`-Umgebung bauen; ein `.ds`-Testskript mit
      `SET_SETTING_STRING WIFI_AP_PWD mein geheimes passwort` laufen lassen (vor dem Fix
      "SET_SETTING needs two arguments has: 3" im Log und Einstellung unverändert, nach dem Fix
      vollständig übernommen), danach `SET_SETTING_BOOL`/`_UINT16` mit gültigen und ungültigen
      Werten zur Regressionsprobe.
- [ ] `handleSetSetting()` ist damit abgeschlossen; aus derselben, seit der `Fortsetzung
      2`-Sitzung offenen Liste bleiben die `AGENT_RUN`/`ESP32M`/`RUN_PAYLOAD`-Dispatch-Handler
      als engerer Nachfolge-Rückstellpunkt für "`usb-army-penetrator`s restliche Ducky-Handler".
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
      LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-
      Integration (blockiert an fehlender Config-Bitwert-Quelle),
      GlitchTrigger-Sweep-UI-Auto-Hit-Status, PromptZero-Installationsskript-Sichtprüfung vor
      lokalem Einsatz, Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump),
      lora-ukfe-Protokollerweiterung (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-
      Schlüsselplan-Konstanten (bewusst zurückgestellt), Texkom-Phase 5
      (Protokollbaum-Registrierung/UI), UART-Portierung des Evil-Portal-Ports auf
      `furi_hal_serial_*` und veraltete `README.md`.

## Firmware-Feature-Mining: Indala224 — neues LFRFID-Protokoll (224-Bit-PSK2, Motorola/Indala)

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      App-Verbesserung (usb-army-penetrator, `SET_SETTING_STRING`-Fix), davor Ideen-Recherche
      (Evil Portal), davor Firmware-Feature-Mining (Texkom Phase 4b) — nach dem etablierten
      Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war Firmware-Feature-Mining
      für diese Sitzung an der Reihe. Git-Identität vor dem ersten eigenen Commit gesetzt
      (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Texkom-Phase-5 (letzter offener Schritt der laufenden Portierung) laut eigener
      Port-Skizze bewusst zurückgestellt: "erst sinnvoll, sobald Phase 1-4b lokal bauen" — ohne
      lokale Baurückmeldung kein sinnvoller nächster Schritt dort. Stattdessen `lib/nfc/
      protocols/` und `lib/lfrfid/protocols/` vollständig (Dateiliste, nicht nur Stichproben)
      gegen einen frischen Unleashed-`dev`-Stand gegenübergestellt, um einen echten neuen
      Lückenfund statt einer weiteren Texkom-Wiederholung oder eines bereits mehrfach
      geprüften Fix-Clusters zu liefern. NFC-Seite: keine Lücke außer dem bereits laufenden
      Texkom-Port. LFRFID-Seite: `protocol_indala224.{c,h}` fehlt komplett — ein eigenständiges
      224-Bit-PSK2-Protokoll (Motorola/Indala), strukturell unabhängig von der bereits
      vorhandenen 26-Bit-Variante.
- [x] **Indala224** vollständig gelesen (278 Zeilen) und auf Speicherfehler/Off-by-one
      geprüft — **kein eigener Fund**. Abhängigkeit identifiziert: der T5577-Schreibpfad nutzt
      eine bei mir ebenfalls fehlende, neue Helferdatei `lfrfid_write_fill.{c,h}` (72 Zeilen);
      bewusst nur deren T5577-Hälfte mitgenommen, die viel größere, unabhängige Umbau-Welle,
      die dieselbe Helferdatei bei zwanzig bereits vorhandenen Protokollen nachträglich statt
      ihres jeweils eigenen Inline-Codes einsetzt, bewusst **nicht** übernommen (siehe
      `indala224-lfrfid-protocol-port.md`, "Risiko").
- [x] Beide Dateien über zwei unabhängige Abrufmethoden (`git clone --branch dev` und
      `raw.githubusercontent.com`, exakter Commit-Hash `a70a69d9d47c1e0a3b1f868aa25c4d0a7a5346d5`)
      geladen, per `md5sum` identisch bestätigt. Unverändert nach
      `firmware-mining/indala224-lfrfid-protocol-port/lib/lfrfid/protocols/` übernommen.
- [x] Zwei additive Zweizeiler-Patches für die bereits vorhandenen
      `lfrfid_protocols.h/.c` (neuer Enum-Wert `LFRFIDProtocolIndala224`, ans Ende von
      `LFRFIDProtocol` angehängt; neue `#include`- und Registrierungszeile in
      `lfrfid_protocols[]`) abgeleitet, gegen zwei frische, unabhängige Zweitkopien meiner
      eigenen Firmware-Basis (`main`, unverändert `b359f6f`) mit `git apply --check`/
      `git apply` verifiziert, real angewendet, per `md5sum` abgeglichen — identisch.
- [x] Host-Repro in
      `firmware-mining/repro-indala224-lfrfid-protocol-port/repro.c` (Protokollkern plus
      T5577-Hälfte der Helferdatei unverändert transkribiert, `bit_lib_*`/`level_duration_*`/
      `ProtocolBase` unverändert aus der eigenen Basis, `FuriString` auf einen minimalen,
      verhaltensgleichen Stand-in reduziert) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: **58 Prüfungen, alle bestanden**, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde. Testvektoren
      unverändert aus Unleasheds eigenem, laut Kommentar Proxmark3-verifiziertem Unit-Test
      übernommen, aber in einer eigenen, unabhängigen Repro-Umgebung (direkter Funktionsaufruf
      statt `protocol_dict`/`pulse_glue`, zusätzliche eigene Negativ-/Grenzfall-Szenarien wie
      "ein Bit zu kurz" und "gekipptes Präambel-Leitbit bei rechnerisch exaktem Minimum") erneut
      durchlaufen lassen, nicht blind übernommen. Zwei Mutationsproben (XOR→AND in der
      differentiellen Decodierung; Off-by-one im T5577-Blockindex) lassen die jeweils
      zutreffenden Prüfungen gezielt fehlschlagen — nicht Teil des gelieferten Repros.
- [x] `firmware-mining/indala224-lfrfid-protocol-port.md` angelegt (Was, Nutzen, betroffene
      Dateien, Risiko, Verifikation, Port-Skizze, Quelle).
- [ ] Lokal zu tun: die drei neuen/geänderten Dateien plus zwei Patches anwenden, in die
      `lib/lfrfid`-Build-Liste aufnehmen, mit `ufbt`/`fbt firmware_all` bauen (sollte ohne
      Fehler durchlaufen, da alle Änderungen additiv sind). Danach Funktionstest mit einer
      eigenen Indala224-Karte (falls verfügbar): Lesen gegen ein unabhängiges
      Referenzwerkzeug (Proxmark3, `lf indala reader`) vergleichen, Emulation gegen ein
      zweites eigenes Lesegerät, Schreibversuch auf eine eigene, leere T5577-Karte mit
      Rücklesen zur Bestätigung.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler (`AGENT_RUN`/`ESP32M`/
      `RUN_PAYLOAD`-Dispatch-Handler), neun verbleibende FeliCa-Multisystem-Teilfixe aus
      PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz,
      Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung
      (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-Schlüsselplan-Konstanten (bewusst
      zurückgestellt), Texkom-Phase 5 (Protokollbaum-Registrierung/UI, weiterhin an eine
      lokale Baurückmeldung zu Phase 1-4b gebunden), UART-Portierung des Evil-Portal-Ports auf
      `furi_hal_serial_*`, die bewusst nicht übernommene `lfrfid_write_fill_*()`-Umbau-Welle
      an zwanzig bereits vorhandenen LFRFID-Protokollen und veraltete `README.md`.

## Ideen-Recherche: BC Scanner (polarikus) — neue Kategorie Barcode/POS-Testing

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Firmware-Feature-Mining (Indala224-LFRFID-Protokoll), davor App-Verbesserung
      (usb-army-penetrator, `SET_SETTING_STRING`-Fix), davor Ideen-Recherche (Evil Portal) —
      nach dem etablierten Rotationsmuster (Firmware → Ideen → App → Firmware → ...) war
      Ideen-Recherche für diese Sitzung an der Reihe. Git-Identität vor dem ersten eigenen
      Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] Gezielt nach einer bislang komplett fehlenden Kategorie gesucht (Eingabevalidierung an
      Kassensystemen/POS über eine emulierte Scanner-Identität, Gegenstück zum eigenen BadUSB
      auf der COM-Port- statt HID-Seite). Erfolglos vorrecherchiert: ein eigenständiger
      Flipper-Zigbee/802.15.4-Sniffer (bereits in einer früheren Sitzung erfolglos geprüft,
      bestätigt — keine neue Quelle gefunden).
- [x] **BC Scanner** (`polarikus/flipper-zero_bc_scanner_emulator`, Commit `7f512ec`,
      2026-10-11, `fap_version 0.5`, GPL-3.0, 170 Sterne) gefunden — emuliert einen
      USB-COM-Port-Barcodescanner für Kassensystem-Eingabevalidierungstests, Datei-Modus
      (`.bc`-Dateien auf SD) plus RPC-/BLE-Modus über eine separate Android-Begleit-App. Per
      `git clone` sowie unabhängig per `raw.githubusercontent.com` auf exakt diesen Commit
      geladen, beide Methoden per `md5sum` identisch bestätigt. Alle neun Quelldateien
      (~850 Zeilen) vollständig gelesen.
- [x] API-87-Status verifiziert gegen eine frische, unabhängige Zweitkopie von
      `G4MEOVER18/G4MEOVER-FW` (`main`, `b359f6f`): `furi_hal_usb_unlock()`/`_is_locked()`/
      `_set_config(&usb_cdc_single, NULL)`, `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)`/
      `_init()`/`_deinit()`/`_control_release()`, `furi_hal_cdc_send()` sowie
      `cli_vcp_disable()`/`_enable()` über `RECORD_CLI_VCP` exakt nach demselben Muster wie in
      der eigenen, bereits gebauten `usb_uart_bridge`-App genutzt. Der RPC-/BLE-Modus nutzt
      `rpc/rpc_app.h` wörtlich wie das offizielle `rpc_debug_app` (`RpcAppSystem`,
      `RpcAppSystemEvent`, `rpc_system_app_set_callback()`/`_send_started()`/`_send_exited()`/
      `_confirm()`). `requires=["gui","dialogs","notification","rpc_start"]` — alle vier
      App-IDs vorhanden. Kein Firmware-Patch nötig, reiner `.fap`-Build.
- [x] Quelltext-Review: Fund in `bc_scanner_script.c` — `bc_script_read_file()` setzt
      `line_nb` bei jedem gelesenen 50-Byte-Häppchen auf die Häppchengröße zurück statt auf der
      beim Öffnen einmalig ermittelten Gesamtdateigröße zu bleiben; die Sendeschleife setzt
      `line_cur` auf denselben Häppchenwert. Die View-Formel `(line_cur*100)/line_nb`
      (`views/bc_scanner_view.c:71`) zeigt dadurch bei jeder Datei über 50 Byte durchgehend
      `100 %` statt eines ansteigenden Sendefortschritts — reiner Anzeigefehler, der
      tatsächlich über den COM-Port gesendete Byteinhalt ist unberührt. Der RPC-/BLE-Modus ist
      nicht betroffen (keine Fortschrittsanzeige, ein Sendeaufruf pro Barcode-Wert).
      Gegengeprüft und bewusst nicht als weiteren Fund behauptet: eine Typverwechslung
      zwischen zwei Enums in `bc_scanner_scene_file_select.c:28` trifft bei der aktuellen
      Enum-Reihenfolge zufällig denselben Zahlenwert wie der korrekte Scene-Index — fragil,
      aber ohne beobachtbare Fehlwirkung beim aktuellen Stand.
- [x] Host-Repro in `ideas/repro-bc-scanner-progress-percent-overwrite/repro.c`
      (`bc_script_read_file()`, Sendeschleife und View-Formel 1:1 transkribiert, minimale
      Mock-Storage, kein Hardware-/USB-Zugriff nötig) unter
      `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`, sechs Prüfgruppen (237-Byte- und 150-Byte-Datei: ALT durchgehend
      `100 %`, NEU rampt korrekt; 12-Byte-Datei als Äquivalenzfall; Regressionsprüfung der
      View-Formel; 65500-Byte-Datei nahe der `uint16_t`-Obergrenze ohne Überlauf;
      Mutationsprobe mit abweichendem, ebenfalls falschem Ergebnis zur Bestätigung der
      Testschärfe): **8 Prüfungen bestanden**, identischer Output beider Optimierungsstufen,
      keine Compiler-Warnungen, keine Sanitizer-Befunde.
- [x] Fix-Skizze (additiv: `line_nb`-Reset in `bc_script_read_file()` entfernen, `line_cur`
      in der Sendeschleife statt mit dem zuletzt gelesenen Häppchen mit der kumulierten
      Byteanzahl befüllen) in `ideas/bc-scanner-emulator-app-status.md` angelegt (Quelle, Was,
      Nutzen, Aufwand, API-87-Status, Fund, Fix-Skizze, Testhinweis, Sicherheitsrahmen,
      Nächster Schritt). `ideas/pentest-app-katalog.md` erweitert: neue Kategorie "Barcode /
      POS-Testing" mit erster Zeile, neuer Rechercheauftrag-Eintrag am Ende.
- [ ] Lokal zu tun: `.fap` gegen den eigenen `mntm-012`-Stand bauen, Fix in
      `bc_scanner_script.c` einbringen, Funktionstest mit einer `.bc`-Datei über 50 Byte
      während laufender Übertragung (Prozentanzeige vor/nach Fix vergleichen) sowie
      Byteinhalt-Vergleich am COM-Port-Gegenstück. Separate Android-Begleit-App
      (`flipper-zero_bc_scanner_emulator-Android-app`) für eine künftige Sitzung vorgemerkt,
      bislang nicht geprüft.
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler (`AGENT_RUN`/`ESP32M`/
      `RUN_PAYLOAD`-Dispatch-Handler), neun verbleibende FeliCa-Multisystem-Teilfixe aus
      PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
      Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz,
      Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung
      (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-Schlüsselplan-Konstanten (bewusst
      zurückgestellt), Texkom-Phase 5 (Protokollbaum-Registrierung/UI, weiterhin an eine
      lokale Baurückmeldung zu Phase 1-4b gebunden), UART-Portierung des Evil-Portal-Ports auf
      `furi_hal_serial_*`, die bewusst nicht übernommene `lfrfid_write_fill_*()`-Umbau-Welle
      an zwanzig bereits vorhandenen LFRFID-Protokollen und veraltete `README.md`.

## App-Verbesserung: usb-army-penetrator — RUN_PAYLOAD: Dateiendungsvergleich Groß-/Kleinschreibungs-abhängig

- [x] `WORKFLOW.md`/`CONSTRAINTS.md` gelesen, `reports/` durchgesehen: letzte Sitzung war
      Ideen-Recherche (BC Scanner), davor Firmware-Feature-Mining (Indala224), davor
      App-Verbesserung (`SET_SETTING_STRING`-Fix) — nach dem etablierten Rotationsmuster
      (Firmware → Ideen → App → Firmware → ...) war App-Verbesserung für diese Sitzung an der
      Reihe. Gezielt den seit mehreren Sitzungen offenen Rückstellpunkt
      "`usb-army-penetrator`s restliche Ducky-Handler (`AGENT_RUN`/`ESP32M`/
      `RUN_PAYLOAD`-Dispatch-Handler)" aufgegriffen. Git-Identität vor dem ersten eigenen
      Commit gesetzt (`G4MEOVER18 <g4me.over.18@gmail.com>`).
- [x] `handleAgentRun()`/`handleESP32Marauder()`/`handleRunPayload()` in
      `src/Attacks/Ducky/Extensions.h` gegen eine frische Zweitkopie von
      `G4MEOVER18/usb-army-penetrator` (`main`, Commit `411b153`) gelesen. Für
      `handleAgentRun()`/`handleESP32Marauder()`: bestätigt, dass beide dem bereits in einer
      früheren Sitzung einzeln als sicher geprüften `str.substr(str.find(' ') + 1)`-Muster
      folgen (kein Absturzpfad, kein `std::stoi()`) — kein neuer Fund.
- [x] Fund in `handleRunPayload()`: die Dateityp-Weiche vergleicht die Endung
      case-sensitiv (`arg.ends_with(".ds")`/`(".png")`). Der Dateiname kommt unverändert aus
      dem Web-UI-Upload-Pfad (`handleUpload()` in `src/Comms/Web/WebServer.cpp`, Zeile 530 —
      keine Normalisierung) und `/payloadinfo` (Zeile 278) listet ebenfalls nur exakt
      kleingeschriebene `.ds`-Dateien. Eine groß- oder gemischtgeschrieben benannte `.ds`-/
      `.png`-Datei existiert also real auf dem Gerät, `RUN_PAYLOAD <Name>` läuft aber bei
      beiden `ends_with()`-Zweigen durch, ohne etwas zu tun und ohne Log-Zeile — Rückgabewert
      bleibt `true` (vom Ducky-Interpreter als erfolgreich gewertet).
- [x] Zusätzlich bestätigt (kein eigener Fund, mit demselben Patch aufgeräumt): der zweite,
      unmittelbar auf `Attacks::Ducky.setPayload(arg)` folgende `duckyFileParser.Restart()`
      in der `.ds`-Verzweigung ist totes Doppel — `DuckyPayload::setPayload()`
      (`DuckyPayload.cpp`, Zeile 286) ruft `duckyFileParser.Restart()` bereits selbst auf.
      Gegen die exakt in `platformio.ini` gepinnte vendored Bibliothek
      `i-am-shodan/DuckScriptInterpreter` (Commit `23aa3ba87e95f402ccab53beb9ead54602643dd7`)
      verifiziert: `DuckyInterpreter::Restart()` (`src/DuckyParse.h:185`) setzt nur
      `_lineNumber`/drei Stacks zurück — ein zweiter Aufruf ohne Zustandsänderung dazwischen
      ist beobachtbar wirkungslos.
- [x] Web-UI-Angriffsflächen-Gegenprobe: die `AGENT_RUN`-Abkürzung `/runagentcmd`
      (`WebServer.cpp:249`, baut `"AGENT_RUN " + cmdline` als Ducky-Rohzeile) öffnet keine
      zusätzliche Angriffsfläche gegenüber dem bereits vollständig offenen `/rawinput`
      (Zeile 241) — wer dort hinkommt, kann ohnehin jede beliebige, auch mehrzeilige
      Ducky-Rohzeile einschleusen (`setPayloadCmdLine()` zerlegt an `\n`,
      `DuckyPayload.cpp:301`). Kein Fund.
- [x] Fix (additiv): Vergleich über eine Großbuchstaben-Kopie (`std::transform(...,
      ::toupper)`, nach demselben bereits in `src/Utilities/Settings.cpp` etablierten Muster),
      der an `setPayload()`/`setPayloadCmdLine()` übergebene Dateiname bleibt in
      Originalschreibung. Neuer `else`-Zweig loggt einen bisher unerkannten Dateityp
      (`"RUN_PAYLOAD unsupported file type: ..."`) statt eines unsichtbaren No-ops. Entfernter
      doppelter `Restart()`-Aufruf im selben Patch. Patch gegen zwei frische, unabhängige
      Zweitkopien von `usb-army-penetrator` (`411b153`) mit `git apply --check` verifiziert,
      real angewendet, per `md5sum` byte-für-byte abgeglichen — identisch.
- [x] Host-Repro in `repro-run-payload-case-insensitive-extension/repro.cpp` (Dispatch-Logik
      1:1 übernommen, Seiteneffekte durch einen `Branch`-Rückgabewert ersetzt) unter
      `g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
      bei `-O0`/`-O1`: sechs Groß-/Mischschreibungs-Fundbestätigungen, eine Regression
      (unbekannter Dateityp bleibt bei ALT unverändert `None`), sieben NEU-Checks (alle
      korrekt behoben, inkl. neuem `Unsupported`-Zweig), eine Byte-Identitätsprüfung
      (übergebener Dateiname bleibt in Originalschreibung), ein bereits bekannter
      Grenzfall (`RUN_PAYLOAD` ohne Argument, kein Absturz). **16 Prüfungen bestanden**,
      identischer Output beider Optimierungsstufen, keine Compiler-Warnungen, keine
      Sanitizer-Befunde.
- [x] Vollständiger Eintrag (Was, Nutzen, betroffene Dateien, Fix, Testhinweis, Risiko,
      Sicherheitsrahmen, Quelle) in
      `app-improvements/usb-army-penetrator/run-payload-case-insensitive-extension-fix.md`.
- [ ] Lokal zu tun: Patch gegen die eigene `usb-army-penetrator`-Basis anwenden, mit der
      vorhandenen PlatformIO-Umgebung bauen, Funktionstest: eine Datei mit Großendung
      (`TEST.DS`) über die Web-UI hochladen, per `RUN_PAYLOAD TEST.DS` starten (vor dem Fix
      passiert nichts, nach dem Fix läuft das Skript), zusätzlich ein bereits vorher
      funktionierendes kleingeschriebenes `.ds`/`.png`-Payload wiederholen (unverändertes
      Verhalten).
- [ ] Damit ist der Rückstellpunkt "`usb-army-penetrator`s restliche Ducky-Handler" abgeschlossen.
      Separat vorgemerkt (bewusst nicht in diesen Patch aufgenommen, reiner Web-UI-Code
      außerhalb des Ducky-Dispatch-Rückstellpunkts): dieselbe Case-Schwäche in
      `/payloadinfo` (`WebServer.cpp:278`) und der exakte String-Vergleich
      `filename == "autorun.ds"` im Upload-Handler (Zeile 525, verhindert bei Großschreibung
      nicht das versehentliche Überschreiben eines laufenden Autorun-Payloads).
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`-Web-UI-Case-Schwächen (`/payloadinfo`,
      `autorun.ds`-Vergleich im Upload-Handler — neu benannt, siehe oben), neun
      verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-
      Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an
      fehlender Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status,
      PromptZero-Installationsskript-Sichtprüfung vor lokalem Einsatz,
      Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump), lora-ukfe-Protokollerweiterung
      (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-Schlüsselplan-Konstanten (bewusst
      zurückgestellt), Texkom-Phase 5 (Protokollbaum-Registrierung/UI, weiterhin an eine
      lokale Baurückmeldung zu Phase 1-4b gebunden), UART-Portierung des Evil-Portal-Ports auf
      `furi_hal_serial_*`, die bewusst nicht übernommene `lfrfid_write_fill_*()`-Umbau-Welle
      an zwanzig bereits vorhandenen LFRFID-Protokollen und veraltete `README.md`.

## Firmware-Feature-Mining: SubGhzBlockGenericGlobal — fehlender Rolling-Code-Counter/Button-Override- und Endless-TX-Mechanismus

- [x] `lib/subghz/protocols/*` und `lib/lfrfid/protocols/*` vollständig gegen Unleashed
      (Standard-Branch, Commit `f31555e4610a74af609c1cc7c3e9db999499fe1d`) gegenübergestellt —
      keine neue Protokolldatei, jedes Rolling-Code-Format bereits vorhanden. Stattdessen beim
      Prüfen der externen App `DarkFlippers/SubGHz_Remote` (Submodul von Unleashed, Commit
      `df6334cdde0c88e22ab09ba16b12222cfccf834a`) eine fehlende Infrastrukturschicht gefunden:
      `lib/subghz/blocks/generic.h`/`.c` fehlt bei mir komplett die
      `SubGhzBlockGenericGlobal`-Struktur plus vier Funktionen
      (`_counter_override_set/get`, `_button_override_set/get`, `_reset`) — der Mechanismus,
      über den ~15 Rolling-Code-Protokolle (KeeLoq, FAAC SLH, Secplus v1/v2, Alutech AT-4N,
      Beninca ARC, Jarolift, Prastel, Ditec GOL4, Phoenix V2, Nice FloR-S, CAME Atomo,
      KingGates Stylo4K, Somfy Keytis/Telis) Zähler-/Tastenwert vor dem Senden überschreiben
      lassen, plus ein `endless_tx`-Flag für Dauersenden statt fester Wiederholzahl.
      `subghz_custom_btn_*()` (eigene, unabhängige API) dagegen bereits byte-identisch
      vorhanden.
- [x] Eigenen Fund beim Vorbereiten des Patches: meine `generic.c`/`.h` sind **keine** reine
      Untermenge von Unleasheds Version — eigene, unabhängige Erweiterung
      (`subghz_block_generic_serialize_common()` plus `Lat`/`Lon`-Geotagging, genutzt von vier
      bereits vorhandenen Protokolldateien `pcsg_generic.c`/`tpms_generic.c`/`ws_generic.c`/
      `bin_raw.c`) wäre bei einem blinden Dateiersatz verloren gegangen. Deshalb additive
      Zusammenführung statt Ersatz: Unleasheds vier Funktionen plus Struct eingefügt, eigene
      Erweiterung unverändert belassen.
- [x] Patch-Trio (`generic_h.patch`, `generic_c.patch`, `base_c.patch`) einzeln und zusammen
      per `git apply --check`/`git apply` gegen eine frische, unveränderte
      `G4MEOVER18/G4MEOVER-FW`-Arbeitskopie (`b359f6fc32f282ce7217dd2d48a4e3478771852e`)
      verifiziert: alle drei greifen, zusammen 104 Zeilen Hinzufügung, 0 Löschungen.
      `base.c`-Hook (`#include "../blocks/generic.h"` plus
      `subghz_block_generic_global_reset(0);` in `subghz_protocol_decoder_base_get_string()`)
      byte-für-byte Unleasheds eigener Anbindung entsprechend.
- [x] Host-Repro in `firmware-mining/repro-subghz-block-generic-global-override-port/`
      kompiliert die tatsächliche, nach dem Merge entstandene `generic.c` unverändert mit
      (inklusive der eigenen Lat-Lon-Erweiterung, die mitkompiliert aber nie aufgerufen wird),
      gegen minimale Mocks für `FuriString`/`FlipperFormat`/`Stream`. Geprüft: Override
      "einmal verbraucht" (Counter und Button), Masken-Formel für jeden bei Unleashed
      tatsächlich verwendeten Bitlängen-Wert (Counter 8/16/20/28/32, Button 1/2/3/4/8),
      `_reset()` erhält `endless_tx` in beide Richtungen, frischer Reset meldet korrekt "kein
      Override anhängig". Unter
      `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
      -fno-sanitize-recover=undefined`: **101 Prüfungen bestanden**, identischer Output beider
      Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
      `g_unexpected_stub_calls` durchgehend 0.
- [x] Separates UB-Probe-Programm (`ub_check.c`) bestätigt den in "Risiko" dokumentierten,
      nach heutigem Code nicht erreichbaren, aber für eine künftige Protokoll-Anbindung zu
      beachtenden Fallstrick: `_counter_override_get()` ohne vorheriges Setzen von
      `cnt_length_bit` löst unter UBSan exakt den erwarteten Shift-Exponent-32-Fehler aus.
- [x] Vollständiger Eintrag (Was, Nutzen, betroffene Dateien, Risiko, Verifikation,
      Port-Skizze, Quelle) in
      `firmware-mining/subghz-block-generic-global-override-port.md`.
- [ ] Lokal zu tun: `generic.h`/`generic.c` ersetzen (oder die drei Patches anwenden), mit
      `ufbt`/`fbt firmware_all` bauen — reine Ergänzung, sollte ohne Compile-Fehler
      durchlaufen. Funktionstest: ein beliebiges SubGhz-Signal anzeigen lassen (löst den neuen
      `base.c`-Hook aus), Verhalten muss unverändert bleiben (reiner
      Infrastruktur-Schritt, noch kein sichtbares Feature in dieser Phase).
- [ ] Phase 2 (künftige Sitzung): ein Pilotprotokoll anschließen (KeeLoq bietet sich an),
      danach die übrigen ~14 Protokolle (Phase 3), dann `endless_tx` an die eigene
      SubGhz-Transmitter-App anschließen. Erst danach wird ein eigener Port der 3440-Zeilen-App
      `SubGhz Remote` sinnvoll (eigene Port-Vorschlag-Datei, u. a. Map-Datei-Format,
      Szenen/Views, Abgleich ihrer eigenen `subghz_txrx`-Kopie gegen meine unabhängig
      weiterentwickelte Version — bereits ein funktionaler Unterschied beim OTG-Einschaltpfad
      gesehen).
- [ ] Weiterhin offen (unverändert aus vorherigen Sitzungen): NFC/RFID-Parser-Teilaspekt,
      MIFARE-Plus-/Ultralight-AES Phase 3 Rest, `sanitize_filename()`-Zeichenbereichs-
      Nebenfund, `usb-army-penetrator`-Web-UI-Case-Schwächen (`/payloadinfo`,
      `autorun.ds`-Vergleich im Upload-Handler), neun verbleibende FeliCa-Multisystem-
      Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration (Hardware-Test),
      `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender Config-Bitwert-Quelle),
      GlitchTrigger-Sweep-UI-Auto-Hit-Status, PromptZero-Installationsskript-Sichtprüfung vor
      lokalem Einsatz, Nearby-Files-Versionslücke v1.5→v1.7 (Submodul-Pin-Bump),
      lora-ukfe-Protokollerweiterung (`lora_rx`/`wifi_ap`/`error`), PSA-Bruteforce-TEA-
      Schlüsselplan-Konstanten (bewusst zurückgestellt), Texkom-Phase 5 (Protokollbaum-
      Registrierung/UI, weiterhin an eine lokale Baurückmeldung zu Phase 1-4b gebunden),
      UART-Portierung des Evil-Portal-Ports auf `furi_hal_serial_*`, die bewusst nicht
      übernommene `lfrfid_write_fill_*()`-Umbau-Welle an zwanzig bereits vorhandenen
      LFRFID-Protokollen, SubGhzBlockGenericGlobal Phase 2/3 (neu benannt, siehe oben) und
      veraltete `README.md`.
