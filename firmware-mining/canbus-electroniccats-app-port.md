# CANBUS (ElectronicCats) — externe App aus RogueMaster-Bündel (Automotive-Pentest)

## Was
Ich habe die verbleibenden vorgemerkten RogueMaster-CAN-Kandidaten (`can_commander`,
`can_transceiver`, `canbus`) geprüft, um den offenen Backlog-Punkt aus meiner
`can_bus_attack`-Sitzung abzuschließen.

**`can_transceiver`** (Autor-Eigenbeschreibung im README: "This is my first Flipper Zero
application... not quite complete... a lot of TODOs and FIXMEs") habe ich gegengeprüft:
Sniffer/History/Sender-Menüpunkte existieren im Scene-Manager, die eigentliche Lese-/
Sende-/Speicherlogik ist laut `notes.txt` (Menüpunkte durchweg mit `TODO` markiert) nicht
implementiert; OBD-II ist als Menüpunkt vorhanden, aber laut README nie fertiggestellt. Reine
Teilmenge von `can_bus_attack` ohne zusätzlichen Nutzen — kein Portkandidat.

**`can_commander`** hängt an einer komplett anderen Hardware: kein generisches MCP2515-Modul,
sondern ein eigenständiges kommerzielles ESP32-Board ("CAN Commander", cancommander.com) mit
eigener, geschlossener ESP32-Firmware (Mindestversion laut App-README `v2.2`); die Flipper-App
selbst ist nur ein UART-Fernbedienungs-Client dafür (Versions-Handshake, Menüs, Profile,
Dashboard). Das ist keine reine Software-Portfrage wie bei den MCP2515-Apps, sondern setzt den
Kauf/Bau eines Zusatzgeräts mit eigener Firmware voraus, die außerhalb dieses Repos liegt — für
eine künftige Sitzung als eigener, größerer Prüfpunkt vorgemerkt, hier nicht weiterverfolgt.

**`canbus`** (Quelle: `ElectronicCats/flipper-MCP2515-CANBUS`, offizielle App zum kommerziellen
Electronic-Cats-CANBUS-Add-on, MIT-Lizenz, aktiv gepflegt — GitHub-Tags bis `v1.1.6.2`, RogueMaster
bündelt `fap_version="1.1.6"`) ist wie `can_bus_attack` eine reine GPIO-App für ein
MCP2515+TJA1050-Modul (identisches Pinout: CS→`PA4`, SCK→`PB3`, MOSI→`PA7`, MISO→`PA6`, gepollt,
kein IRQ-Pin verdrahtet). Funktionsumfang überschneidet sich teilweise mit `can_bus_attack`
(Sniffer, Injection/„Sender", UDS-Diagnose, OBD-II-PIDs), bringt aber vier bei mir bislang
fehlende Fähigkeiten mit:
- **SLCAN2CAN-Bridge** (`lib/SLCAN_serial/`, `scenes/SLCAN2CAN_scene.c`): setzt den Flipper über
  den zweiten USB-CDC-Kanal als SLCAN-Adapter (Lawicel-Protokoll) ein — dadurch kann jedes
  Standard-PC-CAN-Tool, das SLCAN spricht (`slcand`+`can-utils`/`candump`, SavvyCAN, Wireshark
  über `slcand`), den Flipper wie einen regulären USB-CAN-Adapter ansprechen. Keine der übrigen
  CAN-Kandidaten (`can_bus_attack`, `can_tools`, `can_transceiver`) bietet PC-Tool-Interop.
- **Auto-Baudrate-Erkennung** (`scenes/DetectorSpeedOption.c`): probiert 125/250/500/1000 kbps
  automatisch durch (Thread mit Empfangs-Timeout je Rate) — nützlich, wenn die Busgeschwindigkeit
  eines unbekannten/fremden Testbusses noch nicht feststeht, bevor Sniffer/UDS/OBD-II sinnvoll
  laufen.
- **CSV-Log-Export + Log-Replay** (`lib/log_exporter/`, `scenes/playLogs.c`, `scenes/readlogs.c`):
  Sniffing-Sessions als CSV auf die SD-Karte exportieren und später am Gerät selbst wieder
  abspielen (nicht nur im internen Puffer wie `can_bus_attack`s Replay/History).
- **Gestufte DTC-Ansicht** (`scenes/UDSOptions/DTCstored.c`, `scenes/Obd2Options/dtcPIDOption.c`):
  eigene Blätterseiten für Summary/Stored/Pending/Permanent-Fehlercodes statt einer rohen
  UDS-Antwort — direkter für Diagnose-Workflows nutzbar.

## Nutzen
- Ergänzt `can_bus_attack` um PC-Tool-Interop (SLCAN) — damit lassen sich am Flipper gesniffte
  CAN-Daten ohne Umweg direkt in etablierter Desktop-Tooling (SavvyCAN, Wireshark, DBC-Decoder
  wie mein bereits geminter `can_tools`) weiterverarbeiten, statt nur im begrenzten
  Flipper-internen History-Puffer.
- Auto-Baudrate-Erkennung spart mir am eigenen/autorisierten Testbus das manuelle Durchprobieren
  der Bitrate, bevor ich mit Sniffer/UDS/OBD-II starte.
- CSV-Export macht Sniffing-Sessions dauerhaft archivierbar und außerhalb des Geräts auswertbar
  (z. B. mit Tabellenkalkulation oder eigenem Skript), unabhängig vom kleinen internen
  History-Puffer.
- Aktiv gepflegte, offizielle Hersteller-App zu einem dokumentierten, käuflichen Add-on (im
  Unterschied zu `can_bus_attack`s Universitätsprojekt-Status) — MIT-Lizenz statt GPLv3, damit
  unkomplizierter in eigene Ableitungen integrierbar, falls ich Teile später in ein eigenes
  Companion-Repo auslagere.
- Nutzt dasselbe MCP2515-Modul/Pinout wie `can_bus_attack` — kein zusätzliches Zubehör, keine
  zusätzliche Verdrahtung nötig, wenn das Modul bereits für `can_bus_attack` beschafft/verkabelt
  ist.

## Betroffene Dateien (lokal)
Neu, unverändert aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/canbus/application.fam`
- `applications_user/canbus/app_user.c` / `.h`
- `applications_user/canbus/LICENSE`
- `applications_user/canbus/icon.png` (fap_icon)
- `applications_user/canbus/assets/*.png` (14 Icons für Widget-Anzeigen, `fap_icon_assets`)
- `applications_user/canbus/draw_functions/drawWidgets.c`
- `applications_user/canbus/lib/SLCAN_serial/SLCAN_serial.c` / `.h`, `SLCAN_writer.c`
- `applications_user/canbus/lib/files_scaner/dir_walker.c`, `files_scaner.c` / `.h`,
  `frame_extractor.c`
- `applications_user/canbus/lib/frame_can/frame_can.c` / `.h`
- `applications_user/canbus/lib/hex_converter/hex_converter.c` / `.h`
- `applications_user/canbus/lib/log_exporter/exporter_csv.c`, `log_exporter.h`
- `applications_user/canbus/lib/lawicel_serial/lawicel_serial.c`, `lawicel_writer.c` (im
  Quellbaum vorhanden, aber laut Referenzsuche von keiner Scene/keinem `app_user.c` aufgerufen —
  vermutlich totes Altcode-Relikt neben der neueren `SLCAN_serial`-Bridge; beim Build-Test
  mitverfolgen, ob `fbt`/`ufbt` diese Dateien überhaupt aufgreift, sonst ohne Funktionsverlust
  weglassbar)
- `applications_user/canbus/libraries/frame_queue.c` / `.h`, `log_user.h`, `mcp_can_2515.c` /
  `.h`, `pid_library.c` / `.h`, `spi_lib.c` / `.h`, `uds_library.c` / `.h`
- `applications_user/canbus/scenes/*.c` (alle 14 Scene-Dateien: `DetectorSpeedOption.c`,
  `Obd2MenuOption.c`, `SLCAN2CAN_scene.c`, `aboutus.c`, `frames_dialog.c`, `logs_options.c`,
  `mainmenu.c`, `playLogs.c`, `readlogs.c`, `send_timestamp_scene.c`, `senderoption.c`,
  `settingsoptionscene.c`, `sniffer_relay_config.c`, `snifferoption.c`,
  `transmit_options.c`, `warningScene.c`)
- `applications_user/canbus/scenes/Obd2Options/*.c` (5 Dateien: `dtcPIDOption.c`,
  `getCarDataOption.c`, `manualPIDSenderOption.c`, `supportedPIDOption.c`, `typicalPIDOption.c`)
- `applications_user/canbus/scenes/UDSOptions/*.c` (6 Dateien: `DTCstored.c`, `ResetECU.c`,
  `SetDiagnosticSession.c`, `UDSGetVIN.c`, `UDSManualRequest.c`, `UDSSettings.c`)
- `applications_user/canbus/scenes_config/app_scene_config.h`,
  `app_scene_functions.c` / `.h`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert.

## Risiko
- Niedrig-mittel: additiver externer App-Ordner, keine Änderung an Firmware-Kernbestand.
  Fehlschlag zeigt sich beim Build (`ufbt`/APPCHK) als Compile-Fehler.
- **Pin-Überschneidung**: identisches MCP2515-Pinout (`PA4`/`PB3`/`PA7`/`PA6`) wie `can_bus_attack`
  und meine bereits geminte Wiegand-Reader-App — dieselbe Konsequenz wie schon in
  `firmware-mining/canbus-attack-app-port.md` festgehalten: kein Laufzeitkonflikt, da immer nur
  eine externe App läuft, aber Wiegand-Modul und MCP2515-Modul beim lokalen Test nicht
  gleichzeitig am GPIO-Header anschließen.
- **SLCAN-Parsing-Auffälligkeit bei erweiterten (29-Bit-)Frames**: in
  `thread_SLCAN_callback()` (`SLCAN2CAN_scene.c`) wird für `ext`-Frames der erste Zeichen-Byte
  des extrahierten ID-Substrings direkt über den von `furi_string_get_cstr()` zurückgegebenen
  Zeiger um `8` verringert (`*(char*)furi_string_get_cstr(can_id_received) -= 8;`), bevor die
  Hex-Ziffern eingelesen werden — Zweck/Korrektheit dieser Anpassung ist aus dem Code allein
  nicht zweifelsfrei nachvollziehbar (kein Kommentar, kein Test im Repo). Vor produktivem Einsatz
  der SLCAN-Bridge mit erweiterten IDs (29 Bit) gegen ein bekanntes Referenztool (z. B.
  `slcand`+`candump` mit einem gezielt gesendeten Extended-Frame) verifizieren; der 11-Bit-Pfad
  (Standard-IDs, in der Praxis der weit überwiegende Fall bei Fahrzeugbussen) durchläuft diesen
  Zweig nicht.
- **USB-CDC-Kanal**: SLCAN-Bridge nutzt den zweiten CDC-Kanal (`SLCAN_CDC_NUM = 1`, neben dem
  primären VCP/CLI-Kanal 0) — aktuell keine eigene App/kein anderer Port in meinem Ökosystem, der
  ebenfalls Kanal 1 beansprucht, daher kein bekannter Konflikt; bei künftigen Ports mit
  USB-CDC-Bedarf gegenprüfen.
- MIT-Lizenz der Quelle (weniger restriktiv als `can_bus_attack`s GPLv3) — reine lokale
  Nutzung/Anpassung unkritisch, bei eigener Weitergabe Lizenzhinweis/Copyright beibehalten.
- Sicherheitsrahmen wie bei `can_bus_attack`: Sniffer/SLCAN-Bridge/UDS/OBD-II/Sender
  ausschließlich am eigenen oder autorisierten Fahrzeug-CAN-Bus bzw. einer isolierten
  Testbank — kein Einsatz an fremden Fahrzeugen ohne Zustimmung (CONSTRAINTS Punkt 5). Die App
  hat, anders als `can_bus_attack`, keine dedizierte Flood-/DoS-Scene — der „Sender"-Modus sendet
  gezielt konfigurierte Frames, kein Dauerfeuer-Modus.
- Kein Datenschutz-/Fremddaten-Risiko für dieses Repo — CSV-Logs (`apps_data/canbus/logs`
  o. ä.) fallen unter dasselbe Capture-Verbot wie `.sub`/`.nfc`/`.rfid` (CONSTRAINTS Punkt 2/3
  sinngemäß) und werden nicht in dieses Repo übernommen.

## Port-Skizze
1. App-Ordner `canbus` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, `applications/external/canbus/`)
   nach `applications_user/canbus/` in meiner Firmware kopieren. Alternativ direkt von
   `ElectronicCats/flipper-MCP2515-CANBUS` (aktuellster Tag `v1.1.6.2`) übernehmen, falls seit dem
   RogueMaster-Bündelstand (`fap_version 1.1.6`) relevante Fixes hinzugekommen sind — vor dem
   Kopieren kurz `git log`/Changelog des Quell-Repos zwischen `1.1.6` und `1.1.6.2` gegenprüfen.
2. `ufbt launch_app APPSRC=applications_user/canbus` bzw. `fbt launch_app
   APPSRC=applications_user/canbus` — deckt API-Abweichungen als Compile-Fehler auf; dabei
   beobachten, ob `lib/lawicel_serial/` tatsächlich mitkompiliert wird (siehe Risiko/Betroffene
   Dateien) und bei Bedarf aus dem Ordner entfernen, falls ungenutzt.
3. MCP2515+TJA1050-Modul mit identischem Pinout wie bei `can_bus_attack` anschließen
   (CS→A4, SCK→B3, MOSI→A7, MISO→A6) — wiederverwendbar, falls bereits für `can_bus_attack`
   verkabelt; Wiegand-Modul währenddessen nicht gleichzeitig angeschlossen lassen.
4. Funktionstest an einer eigenen CAN-Bus-Testbank oder dem eigenen/autorisierten Fahrzeug:
   - Auto-Baudrate-Erkennung gegen einen Testbus mit bekannter, aber der App nicht mitgeteilter
     Bitrate laufen lassen, Ergebnis gegenprüfen.
   - Sniffer + CSV-Export → Export-Datei auf der SD-Karte öffnen, Inhalt gegen mitgeschnittene
     Frames verifizieren; danach Log-Replay am Gerät selbst testen.
   - SLCAN-Bridge: Flipper per USB an einen PC anschließen, `slcand`+`candump` (oder SavvyCAN)
     gegen den zweiten CDC-Kanal verbinden, Frames mit Standard-IDs sniffen/senden — danach
     gezielt ein Extended-Frame (29 Bit) senden/empfangen und die ID gegen die Quelle
     verifizieren (siehe Risiko).
   - UDS/OBD-II an der eigenen ECU: Diagnose-Session, VIN, DTC-Seiten (Stored/Pending/Permanent),
     typische/manuelle PID-Anfragen gegenprüfen.
5. Bei erfolgreichem Test: gemeinsam mit `can_bus_attack` in `applications_user/` belassen (beide
   nutzen dasselbe Modul, ergänzen sich funktional) oder bei Bedarf in ein eigenes Companion-Repo
   auslagern.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/canbus/` (`fap_version 1.1.6`).
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `ElectronicCats/flipper-MCP2515-CANBUS`
  (MIT-Lizenz, offizielle App zum Electronic-Cats-CANBUS-Add-on, aktiv gepflegt, aktuellster Tag
  zum Prüfzeitpunkt `v1.1.6.2`).
- Gegengeprüft, aber nicht als Portkandidat weiterverfolgt: `applications/external/can_transceiver`
  (unvollständige Teilmenge laut eigenem README) und `applications/external/can_commander`
  (eigenständiges kommerzielles ESP32-Board mit eigener Firmware, kein reiner
  MCP2515-Softwareport — für künftige Sitzung vorgemerkt).
