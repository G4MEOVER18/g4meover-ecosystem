# CAN Tools — externe App aus dem RogueMaster-Bündel (DBC-Erstellung/Offline-Decoder)

## Was
Aus den seit der `can_bus_attack`-Sitzung offen vorgemerkten RogueMaster-CAN-Kandidaten
(`can_commander`, `can_tools`, `can_transceiver`, `canbus`, `canonivm`) habe ich `can_tools`
geprüft. `dev` steht weiterhin exakt auf `a55e3939` (kein neuer Commit seit der letzten
Gegenprüfung) — daher diesen Lauf auf den CAN-Kandidaten verwendet statt eine leere
dev-Gegenprüfung zu protokollieren. `canonivm` ist trotz des Namens ein reiner
Canon-Kamera-Intervalometer (kein CAN-Bus-Bezug) und damit kein Pentest-Kandidat.

**Kandidat:** `applications/external/can_tools` (Autor Matthew KuKanich, Quelle
`MatthewKuKanich/FlipperCANTools`, letzter Commit 2026-01-03, drei Commits Historie —
derselbe Autor wie `can_commander`, dort deutlich umfangreicher). Einzeldatei-App
(`cantools_app.c`, 2419 Zeilen), reine Scene-Manager-/GUI-App ohne externe Libraries/
Hardware-Abhängigkeit:

- **DBC Maker:** Signal-Definitionen (Name, CAN-ID, Start-Bit, Bit-Länge, Byte-Reihenfolge
  Intel/Motorola, Vorzeichen, Offset, Skalierung, Einheit, Min/Max) über Formularfelder
  anlegen, als DBC-`SG_`-Zeile generieren und auf SD-Karte speichern/anhängen.
  DBC-Header (`BO_ <can_id> <name>: 8 VehicleBus`) wird automatisch mitgeführt.
- **View DBC:** gespeicherte Signale durchsuchen, Detailansicht inkl. generierter
  DBC-Zeile, Bearbeiten/Löschen.
- **Decode Data:** eine CAN-ID + DLC + Datenbytes manuell eingeben (Byte-Input-Widget) —
  die App wertet alle gespeicherten Signale mit passender CAN-ID gegen den eingegebenen
  Frame aus und zeigt die dekodierten physikalischen Werte.
- Import bereits vorhandener `.dbc`-Dateien (Zeilenparser für `BO_`/`SG_`-Syntax,
  übernimmt CAN-ID, Signalname, Start-Bit, Bit-Länge, Vorzeichen, Skalierung, Offset).

Die Bit-Extraktion (`decode_extract_raw()`, Zeile 1199) habe ich am Code gegen die
DBC-Spezifikation geprüft: Intel/Little-Endian-Pfad baut den Frame LSB-first
(`data[i] << 8*i`) und schneidet mit `(value >> start_bit) & mask` aus — Standardformel
für DBC-Intel-Bitnummerierung. Motorola/Big-Endian-Pfad iteriert von `start_bit` MSB-first
abwärts über Byte-/Bitindex (`bit_index = start_bit - i`, `byte_index = bit_index/8`,
`bit_in_byte = 7 - bit_index%8`) inkl. Bounds-Guard (`start_bit + 1 < bit_length` →
Ablehnung, `byte_index >= data_len` → Ablehnung) — beides deckt sich mit der
Referenzimplementierung des DBC-Formats, kein Speicherzugriffsfehler gefunden
(Vorzeichenerweiterung bei `is_signed` ebenfalls korrekt: Maskierung auf `bit_length` Bit
vor dem Vergleich mit `sign_bit`).

Technisch: nur `furi.h`, `gui/*`, `dialogs/dialogs.h`, `storage/storage.h`,
`stream/stream.h`, `stream/buffered_file_stream.h` sowie Standard-C (`stdlib.h`,
`string.h`, `ctype.h`) — keine `furi_hal_*`-Hardwaretreiber, kein SPI/GPIO, kein externes
Modul nötig. Alle verwendeten GUI-Bausteine (`submenu_alloc`, `widget_alloc`,
`text_input_alloc`, `byte_input_alloc`, `scene_manager_*`, `view_dispatcher_*`) sind seit
vielen API-Ständen unverändert. `requires=["gui","storage","dialogs"]` in der Quell-`.fam`
(RogueMasters Fork lässt `requires` weg, Verhalten identisch, da alle drei Records ohnehin
Standard sind), kein `api_version`-Constraint. Keine Überschneidung mit der bereits
geminten `can_bus_attack`-App (dort: Live-Sniffing/UDS/Replay/DoS über MCP2515-Hardware;
hier: Hardware-unabhängige Offline-DBC-Erstellung und manuelle Frame-Dekodierung).

## Nutzen
- Ergänzt `can_bus_attack` um die Auswertungsseite: mit `can_bus_attack` gesniffte
  Frames (History-Export) lassen sich hier gegen selbst definierte Signal-Layouts
  dekodieren, ohne dass ein PC/Laptop mit `cantools`/Wireshark nötig ist — direkt auf
  dem Gerät am eigenen/autorisierten Testbus.
- **Kein CAN-Modul zum Testen nötig:** Decode Data nimmt die Frame-Bytes manuell entgegen,
  daher lokal ohne MCP2515-Hardware voll prüfbar (reine Logik/Formular-App) — passt gut
  als App-Verbesserungs-nahes Firmware-Mining-Ziel für eine Sitzung ohne Hardware-Zugriff.
- Baut eigene, wiederverwendbare Signal-Bibliotheken für wiederkehrende
  Fahrzeug-/Steuergeräte-Audits auf (einmal definierte Signale bleiben auf der SD-Karte
  gespeichert), statt bei jedem Einsatz Bytes von Hand zu interpretieren.
- Sehr geringer Portaufwand: eine Quelldatei, keine Fremd-Libraries, keine
  Hardware-Pin-Konflikte mit `can_bus_attack` oder der Wiegand-App (siehe
  `firmware-mining/canbus-attack-app-port.md`, `firmware-mining/wiegand-reader-app.md`).

## Betroffene Dateien (lokal)
Neu, unverändert aus der Quelle übernehmbar:
- `applications_user/can_tools/application.fam`
- `applications_user/can_tools/cantools_app.c`
- `applications_user/can_tools/cantools_app.png` (fap_icon)

Bewusst **nicht** übernehmen: `README.md`/`changelog.md`/`screenshots/` (nur
Repo-Dokumentation, für die App-Funktion irrelevant) sowie den in der `.fam` referenzierten
`fap_icon_assets="icons"`-Eintrag — im Quellbaum (weder Upstream noch RogueMaster-Fork)
existiert kein `icons/`-Verzeichnis, und `cantools_app.c` referenziert keine
Icon-Animation. Gegen den Firmware-Buildscript (`scripts/fbt_tools/fbt_assets.py`,
`scripts/assets.py`) geprüft: `os.walk()` über ein fehlendes Verzeichnis liefert in Python
einfach keine Treffer statt eines Fehlers, der Build bricht dadurch **nicht** ab (erzeugt
nur eine leere, ungenutzte `can_tools_icons.c/h`) — trotzdem beim lokalen Einbau die Zeile
`fap_icon_assets="icons"` aus der `.fam` streichen, da sie so oder so wirkungslos ist.

## Risiko
- Niedrig: additiver externer App-Ordner ohne Hardware-/Treiberanbindung, keine Änderung
  an Firmware-Kernbestand. Einzige App-eigene Zustandsänderung ist das Anlegen/Beschreiben
  einer eigenen DBC-Datei unter dem App-Datenverzeichnis (`storage_simply_mkdir`,
  `DBC_USER_PATH`) — kein Zugriff auf andere App-Daten.
- Reine Analyse-/Dokumentationsfunktion, kein aktives Senden auf einen Bus — fällt nicht
  unter den DoS-/Massenangriff-Vorbehalt aus CONSTRAINTS Punkt 5 wie die DoS-Test-Funktion
  von `can_bus_attack`.
- Selbst erstellte DBC-Dateien/Signal-Definitionen sind eigene Konfigurationsdaten, keine
  Fremd-Kartendaten oder echten Captures — unproblematisch für Punkt 2/3, solange keine
  fremden Fahrzeug-/Flotten-DBC-Dateien mit Herstellerinterna hier eingecheckt werden
  (nur die App selbst, keine erzeugten `.dbc`-Inhalte, gehört in dieses Repo).
- Lizenz der Quelle nicht explizit im Repo vermerkt (kein `LICENSE`-File im Quellbaum,
  weder Upstream noch RogueMaster-Fork) — bei lokaler Nutzung unkritisch, bei
  Veröffentlichung eigener Änderungen als eigenständiges Repo vorher beim Autor
  klären/nachfragen.

## Port-Skizze
1. Die drei unter „Betroffene Dateien" gelisteten Dateien aus
   `MatthewKuKanich/FlipperCANTools` (oder identisch aus
   `RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`,
   `applications/external/can_tools/`) nach `applications_user/can_tools/` kopieren.
2. In der kopierten `application.fam` die Zeile `fap_icon_assets="icons"` entfernen
   (wirkungslos, siehe oben) — restliche Felder unverändert übernehmen.
3. `ufbt launch_app APPSRC=applications_user/can_tools` bzw. `fbt launch_app
   APPSRC=applications_user/can_tools` — laut Codeprüfung werden keine API-Abweichungen
   gegenüber mntm-012/API 87.1 erwartet (reine GUI-/Storage-/Dialogs-App, keine
   `furi_hal_*`-Aufrufe).
4. Funktionstest ohne Hardware: DBC Maker → ein Testsignal anlegen (z. B. RPM, CAN-ID
   `0x201`, Start-Bit 0, 16 Bit, Intel, unsigned, Skalierung 0.25, Offset 0) → View DBC
   prüft die generierte `SG_`-Zeile → Decode Data mit passenden Bytes (z. B.
   `E8 03 00 00 00 00 00 00` für Rohwert 1000 → erwarteter dekodierter Wert 250.0) den
   Rundtrip verifizieren; anschließend mit Motorola/Big-Endian und einem signed-Signal
   denselben Rundtrip wiederholen.
5. Optional gegen echte, mit `can_bus_attack`-Sniffer aufgezeichnete Frames vom eigenen/
   autorisierten Testbus testen: passende Signal-Layouts (z. B. aus einem eigenen
   Fahrzeug-Service-Handbuch oder selbst durch Vergleich mit bekannten Sensorwerten
   ermittelt) anlegen und gegen echte Frame-Bytes dekodieren.

## Quelle
- `MatthewKuKanich/FlipperCANTools` (Upstream, `main`, Commit `8fa1fcf`, 2026-01-03).
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/can_tools/` (nahezu identischer Fork-Stand, `requires`-Feld in
  der `.fam` weggelassen, sonst funktional gleich).
