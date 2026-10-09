# CAN Transceiver — API-87-Status

Fortsetzung des seit der CAN-Tools-Sitzung offenen RogueMaster-CAN-Bus-Clusters: nächsten
Kandidaten aus der damals vorgemerkten Restliste (`can_bus_attack`, `can_commander`,
`can_transceiver`, `canbus`) aufgegriffen.

## Quelle

Repo: `Chainmanner/Flipper-CAN-Transceiver` bzw. `applications/external/can_transceiver/` im
RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`). Vollständig
gelesen: `can_transceiver.c`/`.h`, `can_transceiver_globalconf.c`/`.h`,
`can_transceiver_rxhistory.c`/`.h`, alle vier `scenes/`- und vier `views/`-Dateien,
`lib/mcp2515/mcp2515_spi.c`/`.h` (Low-Level-Treiber; `mcp2515_spi_worker.c`/`.h` nur anhand des
Headers/der Aufrufstellen geprüft, nicht Byte für Byte).

## Was

Direkte Low-Level-Ansteuerung eines MCP2515-CAN-Controllers über SPI am Erweiterungsport, ohne
jede DBC-Interpretation (das deckt das bereits geprüfte `can_tools` ab). **CAN Rx**: laufender
Empfang mit bis zu 1000 Nachrichten im RAM (wächst dynamisch per `mlib`-Array, kein
Vorab-Fixbuffer), Scroll-Liste mit Frame-ID/Rohdaten/Flags, CSV-Export auf SD, REC-Fehlerzähler.
**CAN Tx**: manuelle Frame-ID (11/29 Bit) und 0–8 Datenbyte per Byte-Eingabe, One-Shot-Modus,
TEC/Bus-Off-Anzeige, MCP2515-Reset-Knopf. Laut eigenem README "erste App des Autors", "läuft
aber", mit dokumentierten offenen TODOs (USB-zu-CAN, OBD-II nicht implementiert).

## Nutzen für mich

Liefert die rohen CAN-Frames, die `can_tools` erst über eine DBC-Datei interpretierbar macht —
schließt zusammen mit `can_tools` die Bus-Zugriff-/Signal-Interpretation-Lücke dieser Kategorie.
Von den vier noch offenen Hardware-Apps die kleinste und am wenigsten von eigener
Zusatz-Hardware abhängige (Standard-MCP2515-Modul, kein eigenes GPIO-Board wie laut Kommentar
bei `can_commander`).

## API-87-Status

✅ Kompatibel, geprüft gegen eine frische, unabhängige Zweitkopie von
`Next-Flip/Momentum-Firmware` (Tag `mntm-012`): `furi_hal_spi_bus_handle_external` samt
`furi_hal_spi_acquire/_release/_bus_tx/_bus_rx/_bus_handle_init/_bus_handle_deinit`
byteidentisch vorhanden (dasselbe SPI-Muster wie bereits bei SPI Flash Dump/SWD Probe/i2c Tools
verifiziert), ebenso `furi_hal_power_enable_otg/_disable_otg()` für die 5-V-Versorgung des
Moduls. GUI-/Storage-APIs (`submenu`/`widget`/`text_input`/`byte_input`/`variable_item_list`,
`dialogs`, `storage`) Standard, byteidentisch vorhanden. `lib/mlib` (fester
`P-p-H-d/mlib`-Submodul-Stand, `ARRAY_DEF`/`M_EACH`) für den RX-Verlauf — dieselbe firmwareweit
stabile Bibliothek. Kein Port-Hindernis, reines FAP.

Dabei ein Fund gemacht: `CANTransceiver_SceneCANRx_MessageRxCallback()` baut jeden
RX-Verlaufseintrag in einer lokalen, nicht initialisierten Variable auf; die beiden Flag-Bitfelder
`bExtendedIDFrame`/`bRemoteTransferFrame` werden nur jemals auf `true` gesetzt, nie explizit auf
`false` — für einen echten Standard-/Datenrahmen bleibt der unbestimmte Vorinhalt des gemeinsamen
Bitfeld-Bytes erhalten und wird mit angezeigt/exportiert. Mit Host-Repro (vollständiger Sweep
über alle 256 möglichen Vorinhalt-Bytes: 64/256 zufällig korrekt vs. 256/256 nach Fix) bestätigt.
Port-Vorschlag inkl. Fix siehe `firmware-mining/can-transceiver-app-port.md`.

## Aufwand

Gering (zwölf Quelldateien plus separater MCP2515-Treiber, ein kleiner additiver Patch). Benötigt
wie alle übrigen drei offenen CAN-Apps dieses Clusters ein MCP2515-Transceiver-Modul am
Erweiterungsport für den lokalen Test.

## Sicherheitsrahmen

Reiner Anzeige-/Protokollierungsfund, kein Speicherfehler, keine Absturzgefahr. Interagiert nur
mit dem per MCP2515-Modul direkt verkabelten Bus (eigenes Fahrzeug/Testaufbau oder autorisierter
Pentest, CONSTRAINTS Punkt 5). Keine Kartendaten oder echte Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe mit MCP2515-Hardware, danach Funktionstest gemäß Testhinweis in
  `firmware-mining/can-transceiver-app-port.md`.
- Aus demselben Cluster bleiben `can_bus_attack`, `can_commander`, `canbus` noch nicht einzeln
  geprüft — jeweils für künftige Sitzungen aufgreifbar.
