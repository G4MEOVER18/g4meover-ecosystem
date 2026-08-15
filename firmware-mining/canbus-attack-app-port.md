# CAN Bus Attack — externe App aus RogueMaster-Bündel (Automotive-Pentest)

## Was
Ich habe im RogueMaster-App-Bündel (`flipperzero-firmware-wPlugins`, Branch `420`,
`applications/external/`) die bereits vorgemerkten CAN-Bus-Kandidaten durchgesehen (`can_bus_attack`,
`can_commander`, `can_tools`, `can_transceiver`, `canbus`, `canonivm`) und `picopass`/`seader`/
`seos_compatible` erneut geprüft. Firmware-`dev` und Unleashed haben seit meinem letzten Stand
(`9148d50c` bzw. `unlshd-090`) keine neuen Commits/Releases gebracht (`dev` steht exakt auf
`9148d50c`, `unlshd-090` ist weiterhin der neueste Tag) — daher diesen Lauf auf den offen
vorgemerkten RogueMaster-Kandidaten CAN-Bus verwendet, statt eine leere dev-Gegenprüfung zu
protokollieren.

**Kandidat:** `applications/external/can_bus_attack` (Autor jorgepnaranjo25, Quelle
`jorgepnaranjo25/CAN-Bus-Attack`, GPLv3, Universitäts-Cybersicherheits-Projekt "für Bildungs- und
ethische Zwecke"). Eigenständige GPIO-App für das externe **MCP2515+TJA1050-CAN-Bus-Modul**
(bekanntes, dokumentiertes Flipper-Zubehör, z. B. `ElectronicCats/flipper-MCP2515-CANBUS`) — deckt
eine Geräteklasse ab, die mir bisher komplett fehlt: Automotive-/CAN-Bus-Diagnose und -Pentest
(mein Katalog deckt SubGHz/NFC/RFID/Wiegand/BadUSB/2.4G ab, aber keinen Fahrzeug-Bus).

Funktionsumfang laut Quellcode (27 Quelldateien, ~3.400 Zeilen, reine Scene-Manager-/GUI-App ohne
externe Libraries/Submodule):
- **Sniffer** (`scenes/SnifferOption.c`): passives Mitschneiden aller CAN-Frames über den
  MCP2515-Empfangspfad, optional gefiltert auf eine einzelne CAN-ID (Maske+Filter-Register des
  MCP2515), Live-Anzeige + Verlaufsspeicher (`MAX_HISTORY = 20` Frames je ID).
- **UDS-Bibliothek** (`libraries/uds_library.c`): Unified-Diagnostic-Services-Client (ISO 14229) —
  Diagnose-Session wechseln (Default/Programming/Extended/Safety), ECU-Reset (Hard/KeyOffOn/Soft)
  auslösen, Standard-Diagnoseanfragen an eine konfigurierbare ECU-Request-/Response-ID (Default
  `0x7E0`/`0x7E8`) senden.
- **PID-Bibliothek** (`libraries/pid_library.c`): OBD-II-Mode-01-PID-Anfragen (SAE J1979) an die
  Standard-ECU-Broadcast-ID `0x7DF`, Antworten von `0x7E8`–`0x7EA`.
- **Replay-Attack** (`scenes/replayAttackScene.c`): zuvor gesniffte/gespeicherte Frames erneut auf
  den Bus senden.
- **Modify-Attack** (`scenes/modifyAttackScene.c`, `editFrameScene.c`, `editIDScene.c`): Frame-ID
  und -Payload manuell editieren, dann einmalig senden.
- **DoS-Test** (`scenes/attackDoSScene.c`): Dauerfeuer eines konfigurierten Frames über einen
  eigenen `FuriThread`, start/stop per Zentrum-Taste — Belastungstest für die Bus-Arbitrierung/
  ECU-Fehlertoleranz eines Fahrzeugnetzes (klassische CAN-Bus-Pentest-Übung: prüft, ob ein
  Steuergerät bei hoher Priorität/Flut-Last in einen Fehlerzustand geht).
- **History** (`scenes/HistoryScene.c`): gespeicherte Frame-Reihen je CAN-ID einsehen.
- Logging optional nach `apps_data/canbus/logs` (`app_user.c`, `makePaths()`).

Technisch: SPI-Treiber (`libraries/Spi_lib.c`) über den Standard-`furi_hal_spi_bus_r`/
`FuriHalSpiBusHandle`-Mechanismus (identisch zum Muster, das auch offizielle Firmware-Apps für
externe SPI-Module verwenden), GPIO-Pins fix auf `PA4`(CS)/`PB3`(SCK)/`PA7`(MOSI)/`PA6`(MISO) —
kein Interrupt-Pin verdrahtet, der MCP2515 wird gepollt statt per IRQ bedient. `application.fam`
setzt `fap_category="GPIO"`, keine `requires`-Liste über die Standard-GUI-/Storage-/Dialogs-Records
hinaus, kein `api_version`-Constraint. Keine Hinweise auf veraltete/entfernte Firmware-APIs
gegenüber mntm-012/API 87.1 (nur `furi_hal_gpio`/`furi_hal_spi`/`furi_thread`/Scene-Manager/GUI-
Module, alles seit vielen API-Ständen stabil).

## Nutzen
- Schließt eine komplett neue Gerätekategorie in meinem Katalog: Fahrzeug-/Automotive-CAN-Bus-
  Pentest (Sniffing, UDS-Diagnose, OBD-II-PID-Abfragen, Replay, gezielte Frame-Injection,
  DoS-Belastungstest) — bisher deckt kein eigener oder geminter Fund diesen Bereich ab.
- Direkt für autorisierte Fahrzeugdiagnose/-Sicherheitsaudits nutzbar: eigenes/autorisiertes
  Fahrzeug am OBD-II-Port (Pin 6 = CAN-H, Pin 14 = CAN-L) auf Klartext-CAN-Schwachstellen prüfen
  (die meisten Fahrzeugbusse sind unauthentifiziert/unverschlüsselt — jeder Teilnehmer kann
  beliebige Frames senden), UDS-Diagnosezugriff eines eigenen Steuergeräts verifizieren,
  Fehlertoleranz-Verhalten unter Lastspitzen testen.
- Nutzt ein reales, bereits dokumentiertes Flipper-Zubehör (MCP2515-Board) statt Custom-Hardware —
  Beschaffung/Verdrahtung ist über die verlinkte ElectronicCats-Doku bereits beschrieben.
- Kein API-Versions-Constraint, keine externen Submodule/Libraries (im Gegensatz zu `picopass`/
  `seader`/`seos_compatible`, die weiterhin wegen ASN.1/mbedTLS-Abhängigkeiten und einer neuen
  RFID-Protokollschicht deutlich größeren Portaufwand bedeuten und für eine künftige Sitzung
  vorgemerkt bleiben) — Portaufwand hier ist überschaubar (27 Quelldateien, reiner GPIO-App-Ordner,
  kein Eingriff in Firmware-Kernbestand).

## Betroffene Dateien (lokal)
Neu, unverändert aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/can_bus_attack/application.fam`
- `applications_user/can_bus_attack/app_user.c`
- `applications_user/can_bus_attack/app_user.h`
- `applications_user/can_bus_attack/canbus_app_icons.c`
- `applications_user/can_bus_attack/canbus_app_icons.h`
- `applications_user/can_bus_attack/LICENSE`
- `applications_user/can_bus_attack/draw_functions/drawWidgets.c`
- `applications_user/can_bus_attack/libraries/Spi_lib.c` / `.h`
- `applications_user/can_bus_attack/libraries/mcp_can_2515.c` / `.h`
- `applications_user/can_bus_attack/libraries/pid_library.c` / `.h`
- `applications_user/can_bus_attack/libraries/uds_library.c` / `.h`
- `applications_user/can_bus_attack/libraries/log_user.h`
- `applications_user/can_bus_attack/scenes/AboutUs.c`
- `applications_user/can_bus_attack/scenes/HistoryScene.c`
- `applications_user/can_bus_attack/scenes/SnifferOption.c`
- `applications_user/can_bus_attack/scenes/attackDoSScene.c`
- `applications_user/can_bus_attack/scenes/editFrameScene.c`
- `applications_user/can_bus_attack/scenes/editIDScene.c`
- `applications_user/can_bus_attack/scenes/mainMenu.c`
- `applications_user/can_bus_attack/scenes/modifyAttackScene.c`
- `applications_user/can_bus_attack/scenes/replayAttackScene.c`
- `applications_user/can_bus_attack/scenes/warningScene.c`
- `applications_user/can_bus_attack/scenes_config/app_scene_config.h`
- `applications_user/can_bus_attack/scenes_config/app_scene_functions.c` / `.h`
- `applications_user/can_bus_attack/assets/icon.png`, `Icon_Error.png`, `Icon_No_module.png`,
  `icons_etsi_uma.png`
- `applications_user/can_bus_attack/icon.png` (fap_icon)

Bewusst **nicht** übernehmen: `docs/img/*` (nur README-Bebilderung, für die App-Funktion
irrelevant) — spart Platz, keine funktionale Auswirkung.

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (baut jede App per `ufbt` gegen
das Momentum-SDK mntm-012/API 87.1 inkl. APPCHK).

## Risiko
- Niedrig-mittel: additiver externer App-Ordner, keine Änderung an Firmware-Kernbestand oder
  gemeinsamem State. Fehlschlag zeigt sich beim Build (`ufbt`/APPCHK) als Compile-Fehler.
- **Pin-Überschneidung mit meiner bereits geminten Wiegand-Reader-App**: `can_bus_attack` nutzt
  exakt dieselben vier GPIO-Pins (`PA4`/`PA7`/`PA6`/`PB3`) wie die Wiegand-App aus
  `firmware-mining/wiegand-reader-app.md` (dort D0=A4/Mosfet-Gate B3, D1=A7/Mosfet-Gate A6) —
  für völlig unterschiedliche Zwecke (SPI-Bus vs. Wiegand-D0/D1+Mosfet). Da auf dem Flipper immer
  nur eine externe App gleichzeitig läuft und beide Apps ihre Pins beim Beenden auf
  `GpioModeAnalog` zurücksetzen (Wiegand bestätigt, MCP2515-SPI-Deinit ebenso, siehe
  `Spi_lib.c`/`EventDeinit`), ist kein Laufzeitkonflikt zu erwarten — wichtig ist nur, beim
  lokalen Test **nicht** beide externen Module (Wiegand-Leser + MCP2515-Board) gleichzeitig am
  GPIO-Header angeschlossen zu haben.
- **DoS-Test-Funktion** (`attackDoSScene.c`): Dauerfeuer-Frameinjektion ist per Definition ein
  Belastungstest, kein passives Werkzeug — CONSTRAINTS Punkt 5 verbietet mir Anleitungen für
  Massenangriffe/DoS außerhalb des autorisierten Rahmens. Einsatz ausschließlich am eigenen oder
  vertraglich autorisierten Fahrzeug/Testbus (z. B. isolierte CAN-Bus-Testbank, eigenes Fahrzeug
  im Stand) — niemals an einem in Betrieb befindlichen fremden Fahrzeug oder Straßenverkehr. Die
  App selbst hat keine In-App-Warnung vor der DoS-Scene; beim lokalen Einbau würde ich vor
  `app_scene_attack_dos` einen kurzen Bestätigungsdialog ergänzen (analog zu
  `dialog_message_show_storage_error`-Mustern, die die App bereits nutzt), um Fehlbedienung
  vorzubeugen — reine UI-Ergänzung, kein Kernlogik-Eingriff.
- Sicherheitsrahmen für alle Modi (Sniffer/UDS/Replay/Modify/DoS): ausschließlich am eigenen oder
  autorisierten Fahrzeug-CAN-Bus, nicht am realen Straßenfahrzeug während der Fahrt oder an
  fremden Fahrzeugen ohne Zustimmung (siehe CONSTRAINTS Punkt 5).
- Kein Datenschutz-/Fremddaten-Risiko für dieses Repo — Sniffer-/History-Logs (`apps_data/canbus/
  logs`, `.log`-Dateien) fallen unter dasselbe Capture-Verbot wie `.sub`/`.nfc`/`.rfid`
  (CONSTRAINTS Punkt 2/3 sinngemäß) und werden nicht in dieses Repo übernommen.
- GPLv3-Lizenz der Quelle beachten (Copyleft) — reine lokale Nutzung/Anpassung ist unkritisch,
  bei Veröffentlichung eigener Änderungen als eigenständiges Repo wäre GPLv3-Weitergabe zu
  beachten.

## Port-Skizze
1. App-Ordner `can_bus_attack` (alle unter „Betroffene Dateien" gelisteten Dateien, ohne
   `docs/img/`) aus `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/can_bus_attack/`) nach `applications_user/can_bus_attack/` in meiner
   Firmware kopieren.
2. `ufbt launch_app APPSRC=applications_user/can_bus_attack` bzw. `fbt launch_app
   APPSRC=applications_user/can_bus_attack` — Build deckt etwaige API-Abweichungen sofort als
   Compile-Fehler auf (laut Quellcode-Durchsicht nicht erwartet).
3. Optional: kleinen Bestätigungsdialog vor der DoS-Scene ergänzen (siehe Risiko) — betrifft nur
   `scenes/attackDoSScene.c` bzw. `scenes/mainMenu.c` (Callback vor
   `scene_manager_handle_custom_event(..., AttackDoSOptionEvent)`).
4. MCP2515+TJA1050-Modul gemäß `ElectronicCats/flipper-MCP2515-CANBUS`-Wiring an GPIO-Header
   anschließen (CS→A4, SCK→B3, MOSI→A7, MISO→A6, VCC/GND) — Wiegand-Modul währenddessen nicht
   gleichzeitig angeschlossen lassen (siehe Risiko).
5. Funktionstest lokal am eigenen/autorisierten Fahrzeug oder einer CAN-Bus-Testbank (zwei
   MCP2515-Boards + Termination-Widerstände reichen für einen eigenen Testbus ohne Fahrzeug):
   Sniffer → Frames eines eigenen Steuergeräts/Testbus lesen, UDS-Session/PID-Anfrage → Antwort
   einer eigenen ECU prüfen, Replay/Modify → auf demselben Testbus verifizieren, DoS-Test **nur**
   auf der isolierten Testbank, nie am fahrbereiten Fahrzeug.
6. Bei erfolgreichem Test: App optional in ein eigenes Repo `g4meover-canbus` auslagern und
   `ci-templates/build-fap.yml` als CI ergänzen, oder dauerhaft in `applications_user/` meiner
   Firmware belassen.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/can_bus_attack/`.
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `jorgepnaranjo25/CAN-Bus-Attack`
  (GPLv3, Universitäts-Cybersicherheits-Projekt, Autor Jorge Pimentel Naranjo).
- Referenz-Hardware: `ElectronicCats/flipper-MCP2515-CANBUS` (MCP2515+TJA1050-GPIO-Modul,
  Wiring-Dokumentation).
