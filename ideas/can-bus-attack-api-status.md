# CAN Bus Attack — API-87-Status

Fortsetzung des seit der CAN-Tools-Sitzung offenen RogueMaster-CAN-Bus-Clusters: nächsten
Kandidaten aus der damals vorgemerkten Restliste (`can_bus_attack`, `can_commander`, `canbus` —
`can_transceiver` inzwischen aufgegriffen) gewählt.

## Quelle

Repo: `jorgepnaranjo25/CAN-Bus-Attack` bzw. `applications/external/can_bus_attack/` im
RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`). Vollständig
gelesen: `app_user.c`/`.h`, alle neun `scenes/`-Dateien, `draw_functions/drawWidgets.c`,
`libraries/{Spi_lib,mcp_can_2515,pid_library,uds_library}.c`/`.h` (zusammen 4349 Zeilen).

## Was

Universitäres Security-Projekt für CAN-Bus-Pentests: **Sniffing** (laufender Empfang, bis zu 100
unterschiedliche CAN-IDs mit je bis zu 20 historischen Rahmen im RAM), **Attack DoS**
(Flooding einer Ziel-ID), **Replay Attack** (zuvor gesnifften Rahmen erneut senden), **Modify
Attack** (gesnifften Rahmen vor dem erneuten Senden bearbeiten) — alle vier über einen
selbstgebauten SPI-Bus-Handle auf einem externen MCP2515-Controller am Erweiterungsport. Zusätzlich
eine UDS- und eine OBD-II-PID-Bibliothek, aktuell beide ohne eigene Szene angebunden. Ausdrücklich
als Bildungs-/Forschungsprojekt deklariert (`application.fam`-Beschreibung).

## Nutzen für mich

Deckt zusammen mit `can_tools` (Signalinterpretation) und `can_transceiver` (Rohframe-Senden/
Empfangen) die aktive Angriffsseite dieser Kategorie ab: DoS, Replay, gezielte Modifikation — die
drei klassischen CAN-Pentest-Primitive für ein eigenes Testfahrzeug/-netz. Kleinste der drei noch
offenen Hardware-Apps, am schnellsten lokal baubar.

## API-87-Status

✅ Kompatibel, geprüft gegen eine frische, unabhängige Zweitkopie von
`Next-Flip/Momentum-Firmware` (Tag `mntm-012`): eigener SPI-Bus-Handle auf `furi_hal_spi_bus_r`
(statt des höherwertigen `furi_hal_spi_bus_handle_external`-Wrappers wie bei `can_tools`/
`can_transceiver`) samt `GpioAltFn5SPI1`, `gpio_ext_pa4/_pa6/_pa7/_pb3`, `LL_SPI_*`,
`FuriHalSpiBusHandleEvent{Init,Deinit,Activate,Deactivate}` — byteidentisch vorhanden, dasselbe
in mehreren anderen Flipper-Apps verbreitete SPI1-Erweiterungsport-Eigenbau-Muster.
`furi_hal_light_set/_blink_start/_blink_stop`, `furi_hal_gpio_read()`, Standard-GUI-/Storage-APIs
— ebenfalls byteidentisch. Kein Port-Hindernis, reines FAP.

Dabei ein Fund gemacht: `read_frame()` (`libraries/mcp_can_2515.c`) maskiert das empfangene
DLC-Steuerbyte nur mit `MCP_DLC_MASK` (`0x0F`, liefert 0–15), ohne das Ergebnis danach auf die
deklarierte Puffergröße `MAX_LEN` (`8`) zu begrenzen — anders als die etablierte Referenz-
bibliothek, von der dieser Treiber abstammt (`coryjfowler`/Seeed `MCP_CAN`, dort `if (dlc > 8)
dlc = 8;`). Für ein manipuliertes oder fehlerhaftes DLC-Feld von 9–15 (das CAN-Protokoll selbst
begrenzt das 4-Bit-Feld hardwareseitig nicht auf 8) schreibt die anschließende Leseschleife
entsprechend über das nur 8 Byte große `CANFRAME.buffer` hinaus — und zwar in ein lokales
Stack-Objekt, da `read_frame()` an allen drei Aufrufstellen (`SnifferOption.c`, `pid_library.c`,
`uds_library.c`) mit einem lokalen `CANFRAME` aufgerufen wird. Mit Host-Repro (vollständiger
Sweep über alle 16 DLC-Nibble-Werte: ab DLC=10 zuverlässig als ASan `stack-buffer-overflow`
erkannt, DLC=9 bereits außerhalb des Array-Vertrags, auf diesem Host-ABI aber noch in der
Tail-Padding des Stack-Objekts) bestätigt. Dieselbe Datei liegt byteidentisch auch im noch
offenen `canbus`-Kandidaten vor. Port-Vorschlag inkl. Fix siehe
`firmware-mining/can-bus-attack-app-port.md`.

## Aufwand

Gering (ein App-Kern, neun Szenen, vier kleine Bibliotheken, ein einzeiliger additiver Patch).
Benötigt wie die beiden übrigen offenen CAN-Apps dieses Clusters ein MCP2515-Transceiver-Modul am
Erweiterungsport für den lokalen Test.

## Sicherheitsrahmen

Fund betrifft die Robustheit des eigenen Pentest-Werkzeugs gegenüber Rahmen vom direkt
verkabelten Bus (eigenes Fahrzeug/Testaufbau oder autorisierter Pentest, CONSTRAINTS Punkt 5) —
kein Bezug zu den von der App selbst bereits deklariert angebotenen Angriffstechniken
(DoS/Replay/Modify). Keine Kartendaten oder echte Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe mit MCP2515-Hardware, danach Funktionstest gemäß Testhinweis in
  `firmware-mining/can-bus-attack-app-port.md`.
- Aus demselben Cluster bleiben `can_commander`, `canbus` noch nicht einzeln geprüft — `canbus`
  dürfte beim künftigen Prüfen denselben `mcp_can_2515.c`-Fund reproduzieren.
