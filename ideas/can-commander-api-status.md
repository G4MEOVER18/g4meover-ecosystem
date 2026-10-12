# CAN Commander — API-87-Status (letzter offener Kandidat aus dem CAN-Bus-Cluster)

Letzten noch offenen Kandidaten aus dem seit der CAN-Tools-Sitzung geführten RogueMaster-
CAN-Bus-Cluster (`can_bus_attack`, `can_commander`, `can_tools`, `can_transceiver`, `canbus` —
die anderen vier bereits einzeln geprüft, siehe `ideas/can-bus-attack-api-status.md`,
`ideas/can-tools-api-status.md`, `ideas/can-transceiver-api-status.md`,
`ideas/canbus-api-status.md`) aufgegriffen. Bislang im Katalog als "eigenes GPIO-Board laut
Quellcode-Kommentar vorausgesetzt" vorgemerkt — bei aktuellem Quellstand (`v2.3.1`) präzisiert:
kein MCP2515-SPI-Board, sondern ein **eigenständiges ESP32-Companion-Board mit eigener
CAN-Commander-Firmware** (`cancommander.com`), über UART angebunden.

## Quelle
- Repo: `MatthewKuKanich/CAN_Commander` (direktes GitHub-Repo, Commit `6079fe0`, Tag/Version
  `v2.3.1`) bzw. `applications/external/can_commander/` im RogueMaster-Bündel.
- 41 Quelldateien, 15615 Zeilen gesamt; `CAN_Commander_FlipperZero/can_commander/` (Flipper-FAP,
  28 Dateien, ~13000 Zeilen) vollständig gelesen. `Legacy/ArduinoEnvironment/libraries/mcp2515/`
  ist historischer, nicht mehr gebauter Alt-Code (frühere Projektversion) — kein Teil der
  aktuellen App.

## Was
UART-gesteuerte UI/Controller-App für ein externes ESP32-Board, das die eigentliche
CAN-Bus-Hardware (Transceiver, zwei Busse `can0`/`can1`) kapselt. Flipper sendet
Tool-/Konfigurationsbefehle über ein eigenes binäres Protokoll (COBS-Framing, CRC16-CCITT,
921600 Baud), das ESP32-Board führt die CAN-Operationen aus und streamt Events zurück
(Frame-Empfang, Tool-Fortschritt, DBC-Decode-Ergebnisse, Status). Funktionsumfang laut eigenem
README: Bus-Konfiguration/-Filter, manuelles Senden, zwölf Analyse-/Angriffswerkzeuge
(Read All/Filtered/Write/Speed/Valtrack/Unique-IDs/Bittrack/Byte-Watcher/OBD-PID/DBC-Decode/
Custom-Inject/Replay), bis zu fünf gespeicherte Custom-Inject-Profile (Byte-/Bit-/Feld-Ebene,
Multiplexer-Unterstützung), DBC-Signal-Mapping-Profile, WiFi-CLI-Bridge über das ESP32-Board.

## Nutzen für mich
Letzter fehlende Baustein im CAN-Bus-Cluster — ergänzt die bereits geprüften MCP2515-Apps
(`can_bus_attack`/`can_transceiver`/`canbus`, Rohframe-Zugriff) und `can_tools` (reine
Offline-DBC-Dekodierung) um ein einziges, integriertes Werkzeug mit Dashboard-UI,
Signalanalyse (Bittrack/Byte-Watcher als geführte Reverse-Engineering-Hilfen, ohne externes
DBC-File) und Replay/Custom-Inject direkt am Gerät. Erfordert allerdings Anschaffung des
proprietären ESP32-Companion-Boards (im Gegensatz zu den MCP2515-Apps, bei denen ein
Standard-Modul reicht) — bleibt deshalb bis zur Anschaffung ein reiner Software-Fund ohne
eigene Hardware-Verifikation.

## API-87-Status
✅ Kompatibel, geprüft. Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW`
(Tag `mntm-012`) verifiziert: `furi_hal_serial_control_acquire/_release/_is_baud_rate_supported`
sowie `furi_hal_serial_init/_tx/_tx_wait_complete/_async_rx_start` (inkl. exakt der von
`can_commander_uart.c` genutzten Vier-Parameter-Signatur)/`_async_rx_available/_async_rx/
_async_rx_stop/_deinit`, `FuriHalSerialRxEventData` — byteidentisch, moderne API (kein alter
`cli_session_open/close`-Kompat-Pfad nötig). Standard-GUI-/Storage-/`flipper_format`-APIs sowie
`furi_hal_power_enable_otg/_disable_otg()` (5-V-Versorgung des ESP32-Boards) ebenfalls
byteidentisch vorhanden. Kein Port-Hindernis, reines externes FAP.

## Quelltext-Review (Speichersicherheit)
Durchgehend sehr defensiv geschrieben — vergleichbar mit den bereits ohne Fund geprüften
`at0m-b0mb`-Werkzeugen (Nyx/Specter/GhostTag):

- **UART-Framing** (`libraries/can_commander_uart.c`): eigener COBS-Encoder/-Decoder plus
  CRC16-CCITT-Rahmenprüfung. `cc_parse_packet()` validiert `raw_len`/`payload_len` gegen
  `CC_MAX_PAYLOAD` (220 Byte) und gegen die aus dem Längenfeld berechnete erwartete Gesamtlänge,
  bevor irgendein `memcpy()` stattfindet; alle Zielpuffer (`CcPacket.payload`,
  `CcEvent`-Union-Varianten) sind groß genug für den jeweils geprüften Maximalwert. Jeder aus
  dem Netz kommende Enum-Rohwert (`CcToolId`, `CcBus`, `CcEventId`) wird ausschließlich per
  `==`-Vergleich verwendet (`switch`/`if`), nirgends direkt als Array-Index — das bei anderen
  Protokollen übliche Bug-Muster "Fremdwert indiziert Lookup-Tabelle ohne Bounds-Check" tritt
  hier nicht auf. Die COBS-Decode-Schleife selbst (`cc_cobs_decode()`) prüft `write_index`/
  `read_index` bei jedem Schreib-/Lesezugriff gegen die jeweilige Puffergrenze.
- **Custom-Inject-Slots/Argument-Parser** (`can_commander.c`, `scenes/custom_inject_*.c`):
  `app_args_set_key_value()` (Key-Value-Neuaufbau innerhalb eines `APP_ARGS_KV_WORK_MAX`-
  Scratch-Puffers) prüft bei jedem `snprintf()`-Schritt den Rückgabewert gegen den
  verbleibenden Platz und bricht kontrolliert ab, bevor der finale `memcpy()` in den
  Zielpuffer schreibt; `app_custom_inject_get_slot_args()`/`_slot_ptr()` klemmen den Slot-Index
  konsequent auf `< 5`. Datei-I/O (`app_custom_inject_load/_save()`) nutzt ausschließlich
  `FlipperFormat`/`FuriString` statt manuellem Zeilenparsing — keine der beim Schwester-Cluster
  (`can_tools`: `dbc_parse_sg_line()`-Off-by-one) gefundenen Nach-Komma-Fehler reproduziert sich
  hier.
- **Dashboard/Event-Verarbeitung** (`views/dashboard/dashboard_bittrack.c`,
  `dashboard_metric.c`, 2888 Zeilen größte Einzeldatei): Byte-/Bit-Indizes aus Text-Events
  (`dashboard_parse_reverse_change()`, Bittrack-Textparser) werden vor Verwendung als
  Array-Index explizit gegen die jeweilige Array-Dimension geprüft (`byte_idx >= 8U` →
  Ablehnung); String-Scans über das NUL-terminierte `CcToolEvent.text`-Feld (max. 120 Zeichen)
  brechen beim ersten Nicht-Ziffer-Zeichen ab, bevor sie über das Zeilenende hinaus lesen
  könnten. Kein Speicherzugriffsfehler gefunden.

## Gefundener Verbindungsfehler (kein Speicherfehler, aber reproduzierbarer Dauerausfall)
Die App ruft an keiner Stelle `expansion_disable()`/`expansion_enable()`
(`applications/services/expansion/expansion.h`) auf — anders als die fünf bereits dokumentierten
Geschwister-Apps am Erweiterungsport (UART Sniff, RS485 Modbus, GPIO Logic Analyzer, SWD Probe,
Argus). Der Flipper-eigene "Expansion"-Systemdienst ist in seiner Standardeinstellung aktiv und
überwacht exakt das von `can_commander` genutzte UART (`FuriHalSerialIdUsart`) per
Flankenerkennung auf der RX-Leitung; reagiert er auf den Boot-Vorgang des ESP32-Companion-Boards
(App schaltet 5-V-OTG ein, wartet nur 400 ms, bevor sie selbst `furi_hal_serial_control_acquire()`
aufruft), hält er das UART-Handle exklusiv, während `can_commander`s eigener Verbindungsversuch
`NULL` zurückbekommt und nach drei erfolglosen Ping-Versuchen mit der generischen Meldung
`"Not connected to \nCAN Commander"` aufgibt — ohne dass die eigene Fehler-Glossar-Dokumentation
(README, Abschnitt 6) einen Hinweis auf die tatsächliche Ursache (Flipper-Einstellung
`Settings → System → Expansion`) gibt. Vollständige Herleitung (mit Datei-/Funktionsreferenzen
gegen `G4MEOVER-FW`), additiver Fix (zwei neue Aufrufe, ein neues Feld) und Testhinweis in
`firmware-mining/can-commander-expansion-conflict-fix.md`; Patch gegen eine frische,
unabhängige Zweitkopie verifiziert.

## Aufwand
Software-Port gering (reines FAP, ein kleiner additiver Fix, keine Firmware-Core-Änderung) —
aber erst mit dem proprietären ESP32-Companion-Board sinnvoll nutzbar bzw. überhaupt testbar.
Bis zur Anschaffung bleibt der Beitrag auf den Quellcode-Fund und dessen Fix beschränkt.

## Sicherheitsrahmen
Reiner Verbindungs-/Verfügbarkeitsfund, keine Remote-Angriffsfläche — betrifft ausschließlich
die eigene Flipper↔Board-Verbindung über den physisch verkabelten Erweiterungsport. Passt in
den autorisierten Rahmen aus CONSTRAINTS Punkt 5 (eigenes Fahrzeug/Testaufbau oder autorisierter
Pentest). Keine Kartendaten oder echte Zugangsdaten beteiligt.

## Offen
- Anschaffung/Verkabelung des CAN-Commander-ESP32-Boards, danach lokale Bauprobe mit Patch.
- Damit ist das gesamte RogueMaster-CAN-Bus-Cluster (`can_bus_attack`, `can_commander`,
  `can_tools`, `can_transceiver`, `canbus`) jetzt vollständig quelltextlich durchgesehen.
