# CANBUS — API-87-Status

Fortsetzung des seit der CAN-Tools-Sitzung offenen RogueMaster-CAN-Bus-Clusters: letzten
verbleibenden Kandidaten aus der damals vorgemerkten Restliste (`can_bus_attack`,
`can_commander`, `can_transceiver`, `canbus` — `can_tools`, `can_transceiver` und
`can_bus_attack` inzwischen aufgegriffen, `can_commander` bleibt offen) gewählt.

## Quelle

Repo: `ElectronicCats/flipper-MCP2515-CANBUS` bzw. `applications/external/canbus/` im
RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`), App-Ordner
`Canbus_app/`, App-ID `canbus_app`. Vollständig gelesen: `app_user.c`/`.h`, alle 21
`scenes/`-Dateien (inkl. `Obd2Options/`- und `UDSOptions/`-Unterordner), die fünf eigenen
`fap_private_libs`-Bibliotheken (`lib/frame_can`, `lib/files_scaner`, `lib/SLCAN_serial`,
`lib/log_exporter`, `lib/hex_converter`), `draw_functions/drawWidgets.c` und `libraries/`
(`Spi_lib`, `mcp_can_2515`, `frame_queue`, `pid_library`, `uds_library`) — zusammen 8968 Zeilen
über 9042 Zeilen laut Katalog-Schätzung (Differenz: Assets/Binärdateien im Repo).

## Was

Mit Abstand funktionsreichste der vier MCP2515-abhängigen CAN-Apps dieses Clusters, zehn
Hauptmenüpunkte: **Sniffing** (laufender Empfang), **SLCAN to CAN** (SLCAN-Protokoll-Import über
UART, Weiterleitung auf den CAN-Bus — eigene `lib/SLCAN_serial`), **Speed Detector**,
**Sender** (manuelles Senden), **Player** (zuvor aufgezeichnete `.csv`-Logs von der SD-Karte
abspielen/erneut auf den Bus senden — `lib/files_scaner`), **Scanner OBD2** (volle OBD-II-
Mode-01-Abfrage-Suite: Typical/Supported/Manual-PID, DTC-Auslesen, Fahrzeugdaten — eigene
`libraries/pid_library.c`), **UDS Services** (ISO-14229-ähnliche Diagnosedienste: VIN-Abfrage,
DTC-Auslesen, ECU-Reset, Security-Access/Session-Control — eigene `libraries/uds_library.c`),
**Read LOG** (CSV-Log-Betrachter), **Settings**, **About Us**. CSV-Export über `lib/log_exporter`.

## Nutzen für mich

Schließt das letzte große Funktionsloch dieses Clusters: anders als `can_tools` (reine
Offline-DBC-Interpretation), `can_transceiver` (reines Rohframe-Senden/Empfangen) und
`can_bus_attack` (Sniffing/DoS/Replay/Modify als universitäres Pentest-Projekt) bringt `canbus`
als einzige der vier geprüften Apps eine vollständige, über eigene Menüpunkte tatsächlich
erreichbare OBD-II- UND UDS-Diagnoseschicht mit — die praxisnächste Ergänzung für autorisierte
Fahrzeug-Pentests (ECU-Diagnose, nicht nur Rohbus-Zugriff).

## API-87-Status

✅ Kompatibel, geprüft gegen eine frische, unabhängige Zweitkopie von
`Next-Flip/Momentum-Firmware` (Tag `mntm-012`): wie bereits bei `can_bus_attack` verifiziert,
baut sich diese App ihren eigenen `FuriHalSpiBusHandle` direkt auf `furi_hal_spi_bus_r` zusammen
(`libraries/Spi_lib.c:spi_alloc()`, byteidentisch zu `can_bus_attack`s Variante — siehe unten),
nicht den höherwertigen `furi_hal_spi_bus_handle_external`-Wrapper wie `can_tools`/
`can_transceiver`. `GpioAltFn5SPI1`, `gpio_ext_pa4/_pa6/_pa7/_pb3`, `LL_SPI_Init/_Enable/
_Disable/_SetRxFIFOThreshold`, `FuriHalSpiBusHandleEvent{Init,Deinit,Activate,Deactivate}` —
byteidentisch vorhanden. GUI-/Storage-/Stream-APIs (`submenu`/`widget`/`text_box`/`text_input`/
`byte_input`/`variable_item_list`/`file_browser`, `dialogs`, `storage`, `stream`/`file_stream`
für den CSV-Log-Pfad), `furi_hal_serial_*` (für `SLCAN to CAN` über UART) — Standard,
byteidentisch vorhanden. Kein Port-Hindernis, reines FAP.

**Direktvergleich gegen die bereits geprüfte `can_bus_attack`-Quelle** (eigener, unabhängiger
`diff` zwischen frischen Klonen beider Repos): `libraries/mcp_can_2515.c`/`.h`,
`libraries/uds_library.c` und `libraries/Spi_lib.c`/`.h` sind **byteidentisch**;
`libraries/pid_library.c` unterscheidet sich nur durch eine triviale, funktionsgleiche
Variableninitialisierung (`char* first = NULL;` vs. `char* first;`). Dieselbe Herkunft, derselbe
Treiberkern.

### Fund: derselbe DLC-Überlauf wie bei `can_bus_attack` — hier aber über zwei zusätzliche, tatsächlich erreichbare Menüpunkte auslösbar

Der bereits in `can_bus_attack` dokumentierte Fund (`read_frame()` maskiert das empfangene
DLC-Steuerbyte nur mit `MCP_DLC_MASK` (0x0F, liefert 0–15), ohne auf die deklarierte 8-Byte-
Puffergröße `MAX_LEN` zu begrenzen) reproduziert sich hier unverändert — die betroffene Datei ist
byteidentisch. Der eigentliche neue Befund dieser Sitzung: **anders als bei `can_bus_attack`,
wo `pid_library.c`/`uds_library.c` laut eigenem Menü (`app_user.h`) nie über eine Szene
eingebunden und damit toter Code waren**, sind in `canbus` beide tatsächlich über eigene,
erreichbare Hauptmenüpunkte verdrahtet:

- **"UDS Services"** → `scenes/UDSOptions/*.c` → `libraries/uds_library.c`-Hilfsfunktionen
  (`uds_read_dtc`/`uds_reset_ecu`/`uds_get_vin`/…) → `read_frames_uds()` →
  `read_can_message()` → `read_frame()`. Dieselbe `CANFRAME`-Instanz bleibt hier wie bei
  `can_bus_attack` lokal auf dem Stack der jeweiligen `uds_library.c`-Funktion (`CANFRAME
  frame_to_received = {0};`) — gleiche Fehlerklasse (Stack-Überlauf), neuer, tatsächlich
  erreichbarer Aufrufpfad.
- **"Scanner OBD2"** → `scenes/Obd2Options/manualPIDSenderOption.c` →
  `pid_manual_request()` → `read_can_message()` → `read_frame()`, geschrieben in
  `obdii->frame_to_received`. Anders als alle bisher für diesen Fund geprüften Aufrufstellen ist
  `frame_to_received` hier **kein** Stack-Lokal, sondern ein Inline-Feld der `OBDII`-Struktur
  (`libraries/pid_library.h`), die selbst als Wertfeld `OBDII obdii;` Teil der einmalig per
  `malloc()` angelegten Haupt-`App`-Struktur ist (`app_user.h`). Ein manipuliertes/fehlerhaftes
  DLC-Feld 9–15 in einer ECU-Antwort während einer OBD-II-Abfrage schreibt hier also nicht in
  einen kurzlebigen Stack-Frame, sondern direkt über das Ende eines **heap-residenten, über die
  gesamte App-Laufzeit bestehenden** Puffers hinaus — ein Heap-Overflow in das unmittelbar
  danach liegende Feld der `App`-Struktur statt ein Stack-Overflow, mit entsprechend länger
  nachwirkendem Korruptionspotenzial. Der volle Sniffing-Pfad (`SnifferOption.c`) bleibt wie bei
  `can_bus_attack` ebenfalls stack-basiert.

Port-Vorschlag inkl. Fix (derselbe additive Patch, 1:1 aus `can_bus_attack` übernommen und gegen
eine frische, unabhängige `canbus`-Zweitkopie neu verifiziert — Datei ist byteidentisch, Patch
greift unverändert) siehe `firmware-mining/canbus-app-port.md`.

## Aufwand

Gering (ein identischer, additiver Patch, bereits einmal gegen `can_bus_attack` host- und
patch-verifiziert, hier nur gegen eine zweite, unabhängige Zweitkopie neu bestätigt). Benötigt
wie alle übrigen drei bereits geprüften CAN-Apps dieses Clusters ein MCP2515-Transceiver-Modul am
Erweiterungsport; für den OBD-II-/UDS-Fundnachvollzug zusätzlich eine echte oder simulierte ECU
(bzw. ein zweiter Mikrocontroller mit eigenem CAN-Controller, der ein Antwortframe mit
manipuliertem DLC-Nibble sendet).

## Sicherheitsrahmen

Reiner Robustheits-/Speicherfehlerfund im eigenen Pentest-Werkzeug gegenüber Rahmen vom per
MCP2515-Modul direkt verkabelten Bus (eigenes Fahrzeug/Testaufbau oder autorisierter Pentest,
CONSTRAINTS Punkt 5) — kein Bezug zu Angriffstechniken selbst. Keine Kartendaten oder echte
Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe mit MCP2515-Hardware, danach Funktionstest gemäß Testhinweis in
  `firmware-mining/canbus-app-port.md`.
- Aus demselben Cluster bleibt `can_commander` (`MatthewKuKanich/CAN_Commander`, 15165 Zeilen,
  eigenes "CAN Commander GPIO Board" laut Quellcode-Kommentar vorausgesetzt) als letzter, noch
  nicht einzeln geprüfter Kandidat — damit ist das CAN-Bus-Cluster bis auf diesen einen
  Fremdhardware-Sonderfall vollständig aufgegriffen.
- `lib/SLCAN_serial`, `lib/files_scaner` (Player/Read-LOG-CSV-Parser), `lib/log_exporter`,
  `lib/hex_converter`, `libraries/frame_queue.c`, `draw_functions/drawWidgets.c` und die
  restlichen Szenen (`SpeedDetectorOption`, `senderOption`, `settingsOptionScene`,
  `sniffer_relay_config`, `transmit_options`, `send_timestamp_scene`, `warningScene`) sind
  dieser Cluster-eigenen Bibliotheken wegen vollständig gelesen, aber ohne eigenen,
  dokumentierten Fund über die bereits behandelte DLC-Überlauf-Klasse hinaus — eigenständiger
  Rückstellpunkt für eine künftige, tiefere Prüfung (insbesondere `frame_extractor.c`s
  `frame_splitter()`, der bei einer `.csv`-Log-Zeile mit weniger als den erwarteten fünf
  `:`-Trennzeichen nicht auf eine exakte Feldanzahl prüft, sondern nur auf "mindestens ein
  Trennzeichen vorhanden" — über `m_string_mid()`/`m_string_right()`s eigene Index-Clamping laut
  `mlib`-Quelltext aber speichersicher, im schlechtesten Fall nur eine stillschweigend
  falsch/leer geparste Zeile statt eines Absturzes, daher kein eigenständiger Speicherfehlerfund).
