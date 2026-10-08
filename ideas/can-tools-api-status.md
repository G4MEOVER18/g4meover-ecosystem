# CAN Tools — API-87-Status

Neue Ideen-Recherche (kein Rückstellpunkt aus einer früheren Sitzung): gezielt nach einer
bislang komplett fehlenden Kategorie gesucht — Fahrzeug-CAN-Bus als Angriffs-/Auditfläche, über
die bestehende Bus-Analyse (UART Sniff, GPIO Logic Analyzer, i2c Tools, RS485 Modbus), den
Chip-Dump (SPI Flash Dump) und das Hardware-Debug-Interface (SWD Probe) hinaus.

## Quelle

Repo: `MatthewKuKanich/FlipperCANTools` bzw. `applications/external/can_tools/` im
RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`). Eine
Quelldatei (`cantools_app.c`, 2419 Zeilen), vollständig gelesen. `application.fam`:
`appid="can_tools"`, `fap_category="Tools"`, `fap_version=(1, 1)`.

Im selben RogueMaster-Bündel vier weitere, eigenständige CAN-Apps gefunden, alle mit
Zusatzhardware-Abhängigkeit (externes CAN-Transceiver-Modul, meist MCP2515, am
Erweiterungsport) und daher noch nicht einzeln geprüft: `can_bus_attack`
(`jorgepnaranjo25/CAN-Bus-Attack`, universitäres Security-Projekt, Kategorie GPIO),
`can_commander` (`MatthewKuKanich/CAN_Commander`, selber Autor wie CAN Tools, größte der fünf
Apps mit 15165 Zeilen über 29 Quelldateien, eigenes "CAN Commander GPIO Board" nötig),
`can_transceiver` (`Chainmanner/Flipper-CAN-Transceiver`, direkte MCP2515-Low-Level-Ansteuerung),
`canbus` (`ElectronicCats/flipper-MCP2515-CANBUS`, Sniffing + Injection, 9042 Zeilen über
mehrere eigene private Libraries).

## Was

Reine Offline-Hilfsanwendung für das CAN-Signalformat DBC (Vector CAN DataBase), komplett ohne
Hardware-Abhängigkeit — einzige der fünf gefundenen CAN-Apps, die ohne externes
Transceiver-Modul läuft. Vier Funktionen: **DBC Maker** (Signal-Felder wie CAN-ID/Startbit/
Bitlänge/Endianness/Signedness/Skalierung per Texteingabe erfassen, als `BO_`/`SG_`-Zeilen im
DBC-Textformat auf die SD-Karte schreiben), **View DBC** (gespeicherte Signale einer
`.dbc`-Datei auflisten/anzeigen/bearbeiten/löschen), **Decode Data** (eine CAN-ID + bis zu 8
Datenbyte manuell eingeben, gegen alle Signale derselben CAN-ID aus der aktiven `.dbc`-Datei
dekodieren) und **DBC Files** (mehrere `.dbc`-Dateien verwalten). Ein fünfter Menüpunkt
"Decode log" ist laut Quellcode noch ein reiner Platzhalter ("Coming soon").

## Nutzen für mich

Schließt die Lücke zwischen reiner Bus-*Analyse* und tatsächlicher *Interpretation* von
CAN-Frames: die vier hardwareabhängigen CAN-Apps desselben Clusters liefern rohe Frames vom
Bus, aber ohne eine DBC-Definition bleiben die Byte-Inhalte bedeutungslose Hex-Werte. CAN Tools
lässt mich eine DBC-Datei für ein Zielfahrzeug am Schreibtisch aufbauen/pflegen und einzelne
Frames offline gegenprüfen, bevor ich mit echter Hardware am Fahrzeug arbeite. Neue,
eigenständige Katalog-Kategorie "CAN-Bus" (erfordert für die übrigen vier Apps eine
MCP2515-Erweiterung, CAN Tools selbst nicht).

## API-87-Status

✅ Kompatibel, geprüft gegen eine frische, unabhängige Zweitkopie von
`Next-Flip/Momentum-Firmware` (Tag `mntm-012`): die App nutzt ausschließlich Standard-GUI-/
Storage-/Stream-APIs (`gui/modules/{submenu,widget,text_input,byte_input}.h`,
`storage/storage.h`, `stream/stream.h` + `stream/buffered_file_stream.h`,
`dialogs/dialogs.h`), keine Erweiterungsport-Pins oder sonstige Hardware-HAL-Aufrufe — passend
zum reinen Offline-Charakter. Alle verwendeten Funktionssignaturen (u. a.
`byte_input_set_result_callback()`s sechs Parameter) byteidentisch vorhanden. Kein
Port-Hindernis: unverändert aus `applications/external/can_tools/` übernehmbar, kein
Firmware-Core-Patch nötig.

Dabei ein Fund gemacht: `dbc_parse_sg_line()` liest beim Laden einer `.dbc`-Datei zwei Zeichen
(Endianness/Signedness) ohne vorherige Längenprüfung über das Zeilenende hinweg — für eine
hinreichend lange, exakt mit `@` endende Zeile ein echter Lesezugriff außerhalb des
256-Byte-Stack-Puffers (mit Host-Repro unter ASan als `stack-buffer-overflow READ of size 1`
bestätigt). Port-Vorschlag inkl. Fix siehe `firmware-mining/can-tools-app-port.md`.

## Aufwand

Gering (eine Quelldatei, reine externe `.fap`, keine Firmware-Änderung, keine Zusatzhardware).
Für die vier übrigen CAN-Apps desselben Clusters zusätzlich ein MCP2515-Transceiver-Modul am
Erweiterungsport nötig, bei `can_commander` laut eigenem Kommentar explizit ein passendes
"CAN Commander GPIO Board".

## Sicherheitsrahmen

Reine Offline-Hilfsanwendung ohne Netzwerk-/Funkkanal und ohne Interaktion mit einem Fahrzeug —
der gefundene Lesezugriff betrifft nur das Parsen einer lokalen Textdatei. Keine Kartendaten
oder echte Zugangsdaten beteiligt. Die vier übrigen, hardwareabhängigen CAN-Apps greifen aktiv
in ein reales Fahrzeugnetz ein (laut `can_bus_attack`-Beschreibung explizit für autorisierte
Sicherheitstests an eigenen/freigegebenen Fahrzeugen gedacht) — nur an eigenen Fahrzeugen oder
im Rahmen autorisierter Pentests einsetzen (CONSTRAINTS Punkt 5).

## Offen

- Lokale Bauprobe gegen die eigene `mntm-012`-Basis, danach Funktionstest am Gerät (DBC
  anlegen, Signale eintragen, Decode Data gegenprüfen) sowie den Fundnachvollzug aus
  `firmware-mining/can-tools-app-port.md` (präparierte `.dbc`-Datei mit einer exakt mit `@`
  endenden `SG_`-Zeile).
- Die vier übrigen CAN-Apps desselben Clusters (`can_bus_attack`, `can_commander`,
  `can_transceiver`, `canbus`) noch nicht einzeln geprüft — jeweils MCP2515-Hardware
  erforderlich, für künftige Sitzungen einzeln aufgreifbar.
