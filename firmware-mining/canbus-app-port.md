# CANBUS — externe App von ElectronicCats (MCP2515-CAN-Sniffer/-Sender mit OBD-II-/UDS-Suite)

## Was

Aus der seit der CAN-Tools-Sitzung offenen Restliste des RogueMaster-CAN-Bus-Clusters
(`can_bus_attack`, `can_commander`, `can_transceiver`, `canbus` — `can_tools`,
`can_transceiver` und `can_bus_attack` bereits geprüft) den letzten noch ohne eigene
GPIO-Zusatzhardware auskommenden Kandidaten aufgegriffen: **CANBUS**
(`ElectronicCats/flipper-MCP2515-CANBUS`, App-ID `canbus_app`), mit App-Kern
(`app_user.c`/`.h`), 21 `scenes/`-Dateien, fünf eigenen `fap_private_libs`-Bibliotheken und den
MCP2515-Kernbibliotheken (`Spi_lib`, `mcp_can_2515`, `frame_queue`, `pid_library`,
`uds_library`) zusammen 8968 Zeilen — größte der vier geprüften MCP2515-Apps dieses Clusters
(`can_commander` bleibt mit 15165 Zeilen und eigenem GPIO-Board-Erfordernis der letzte offene
Kandidat).

Direkte Low-Level-Ansteuerung eines MCP2515-CAN-Controllers über den Erweiterungsport (eigener
SPI-Bus-Handle auf `furi_hal_spi_bus_r`, dasselbe Eigenbau-Muster wie bei `can_bus_attack`):
**Sniffing**, **SLCAN to CAN** (SLCAN-Protokoll-Import über UART), **Speed Detector**,
**Sender**, **Player** (CSV-Logs von der SD-Karte erneut auf den Bus senden), **Scanner OBD2**
(volle OBD-II-Mode-01-Suite), **UDS Services** (ISO-14229-ähnliche Diagnosedienste), **Read
LOG**, **Settings**, **About Us**.

## Nutzen für mich

Einzige der vier geprüften CAN-Apps mit vollständiger, tatsächlich über eigene Menüpunkte
erreichbarer OBD-II- UND UDS-Diagnoseschicht — praxisnächste Ergänzung zu `can_tools`
(Signalinterpretation), `can_transceiver` (Rohframes) und `can_bus_attack` (DoS/Replay/Modify)
für autorisierte Fahrzeug-ECU-Diagnose statt reinem Rohbus-Zugriff.

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware` (Tag `mntm-012`)
geprüft: `furi_hal_spi_bus_r` samt `furi_hal_spi_acquire/_release/_bus_tx/_bus_rx/
_bus_handle_init/_bus_handle_deinit`, `GpioAltFn5SPI1`, `gpio_ext_pa4/_pa6/_pa7/_pb3`,
`LL_SPI_Init/_Enable/_Disable/_SetRxFIFOThreshold`, `FuriHalSpiBusHandleEvent{Init,Deinit,
Activate,Deactivate}` — byteidentisch vorhanden, exakt dasselbe Muster wie bereits bei
`can_bus_attack` verifiziert (`libraries/Spi_lib.c` ist zwischen beiden Apps byteidentisch, per
`diff` zwischen frischen, unabhängigen Klonen bestätigt). GUI-/Storage-/Stream-APIs
(`submenu`/`widget`/`text_box`/`text_input`/`byte_input`/`variable_item_list`/`file_browser`,
`dialogs`, `storage`, `stream`/`file_stream`), `furi_hal_serial_*` (SLCAN-UART-Pfad) — Standard,
byteidentisch vorhanden. Kein Port-Hindernis, reines FAP.

## Fund: derselbe DLC-Überlauf wie bei `can_bus_attack`, hier über zwei zusätzliche, tatsächlich erreichbare Menüpunkte

`read_frame()` (`libraries/mcp_can_2515.c`) liest einen vom MCP2515 empfangenen CAN-Rahmen:

```c
furi_hal_spi_bus_rx(spi, &data_ctrl, 1, TIMEOUT);

frame->data_lenght = data_ctrl & MCP_DLC_MASK;   // MCP_DLC_MASK == 0x0F -> 0..15
frame->req = (data_ctrl & MCP_RTR_MASK) ? 1 : 0;

for(uint8_t i = 0; i < frame->data_lenght; i++) {
    furi_hal_spi_bus_rx(spi, &frame->buffer[i], 1, TIMEOUT);   // frame->buffer ist uint8_t[8]
}
```

Per `diff` zwischen frischen, unabhängigen Klonen von `ElectronicCats/flipper-MCP2515-CANBUS`
und `jorgepnaranjo25/CAN-Bus-Attack` bestätigt: `libraries/mcp_can_2515.c`/`.h` und
`libraries/uds_library.c` sind **byteidentisch**, `libraries/pid_library.c` unterscheidet sich
nur durch eine funktionsgleiche Variableninitialisierung. Der bereits in
`firmware-mining/can-bus-attack-app-port.md` dokumentierte Fund (DLC-Feld 9–15 schreibt 1–7
Byte über das deklarierte `CANFRAME.buffer[8]` hinaus, da `MCP_DLC_MASK` das 4-Bit-Feld nur auf
0–15 statt auf 0–8 begrenzt) reproduziert sich hier unverändert.

**Neu gegenüber `can_bus_attack`**: dort waren `pid_library.c`/`uds_library.c` laut eigenem
Menü (`app_user.h`s `SniffingTestOption`/`AttackDoSOption`/`ReplayAttackOption`/
`ModifyAttackOption`/`AboutUsOption`) nie über eine Szene eingebunden — toter Code, nur
`SnifferOption.c` rief `read_frame()` tatsächlich auf. In `canbus` sind beide Bibliotheken über
eigene, im Hauptmenü erreichbare Punkte verdrahtet:

- **"UDS Services"** → `scenes/UDSOptions/*.c` (`UDSGetVIN.c`, `DTCstored.c`, `ResetECU.c`, …)
  → `uds_library.c`-Hilfsfunktionen → `read_frames_uds()` → `read_can_message()` →
  `read_frame()`. `CANFRAME frame_to_received = {0};` bleibt hier wie bei `can_bus_attack` lokal
  auf dem Stack der jeweiligen `uds_library.c`-Funktion — gleiche Fehlerklasse
  (Stack-Buffer-Overflow), neuer, tatsächlich erreichbarer Aufrufpfad.
- **"Scanner OBD2"** → `scenes/Obd2Options/manualPIDSenderOption.c` →
  `pid_manual_request()` → `read_can_message()` → `read_frame()`, geschrieben in
  `obdii->frame_to_received`. `frame_to_received` ist hier kein Stack-Lokal, sondern ein
  Inline-Feld der `OBDII`-Struktur (`libraries/pid_library.h:86-87`), die selbst als Wertfeld
  `OBDII obdii;` Teil der einmalig per `malloc()` angelegten Haupt-`App`-Struktur ist
  (`app_user.h`). Ein manipuliertes/fehlerhaftes DLC-Feld 9–15 in einer ECU-Antwort während
  einer OBD-II-Abfrage schreibt damit über das Ende eines **heap-residenten, über die gesamte
  App-Laufzeit bestehenden** Puffers hinaus statt über einen kurzlebigen Stack-Frame — ein
  Heap-Overflow in das unmittelbar danach liegende Feld der `App`-Struktur, mit entsprechend
  länger nachwirkendem Korruptionspotenzial als der bereits dokumentierte Stack-Fall. Der volle
  Sniffing-Pfad (`SnifferOption.c:310/311`, `CANFRAME frame = app->can_frame;`) bleibt wie bei
  `can_bus_attack` ebenfalls stack-basiert (lokale Kopie der Struktur).

Praktische Auswirkung: ein einzelner manipulierter oder fehlerhafter Rahmen auf dem Bus — durch
eine defekte ECU, einen Bus-Fehler oder einen gezielten Testimpuls im Rahmen eines autorisierten
Pentests — kann nicht mehr nur während des passiven Sniffings, sondern auch während einer
gewöhnlichen OBD-II- oder UDS-Diagnoseabfrage einen Speicherfehler auslösen, im OBD-II-Fall mit
Heap- statt nur Stack-Reichweite.

## Host-Repro

Da `libraries/mcp_can_2515.c` byteidentisch zu `can_bus_attack` ist, deckt der dort bereits
erstellte Host-Repro (`firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c`, dessen
eigener Kommentarkopf explizit auf diese Übereinstimmung verweist) die hier betroffene Funktion
Byte für Byte identisch ab — Regressionsfall (DLC=8), die drei Fundfälle (DLC=9 Tail-Padding-
Grenzfall, DLC=10 kleinster von ASan erkannter Fall, DLC=15 Maximalfall) und der vollständige
16-Werte-Sweep gelten unverändert; eine zweite, inhaltlich identische Kopie dieses Repros wäre
reine Duplizierung ohne zusätzlichen Erkenntniswert. Eigenständig neu geprüft wurde dagegen die
hier dokumentierte Erreichbarkeits- und Heap-vs-Stack-Unterscheidung (per Quelltextlesung beider
Aufrufpfade, siehe oben) sowie die Patch-Verifikation unten.

## Betroffene Datei

- `libraries/mcp_can_2515.c` — Funktion `read_frame()`.

## Port-Skizze

1. App unverändert aus `ElectronicCats/flipper-MCP2515-CANBUS` (`Canbus_app/`, bzw.
   `applications/external/canbus/` im RogueMaster-Bündel, Branch `420`) in meine eigene
   `applications/external/canbus/`-Struktur übernehmen (reines FAP, keine Firmware-Core-
   Änderung).
2. Patch `firmware-mining/canbus-app-port/mcp_can_2515_c.patch` danach anwenden — identische
   additive Begrenzungszeile wie bei `can_bus_attack`:
   `if(frame->data_lenght > MAX_LEN) frame->data_lenght = MAX_LEN;`, direkt nach der
   DLC-Maskierung. Patch per `git apply --check` gegen eine frische, unabhängige
   `canbus`-Zweitkopie verifiziert, real angewendet; das Ergebnis per `diff` byte-für-byte mit
   der bereits gepatchten `can_bus_attack`-Datei abgeglichen — identisch (beide Dateien waren
   vor dem Patch schon byteidentisch, der Patch selbst ändert an beiden exakt dieselbe Zeile).

## Testhinweis (lokal, mit MCP2515-Hardware)

1. MCP2515-Transceiver-Modul am Erweiterungsport verkabeln (`CS=PA4`, `SCK=PB3`, `MOSI=PA7`,
   `MISO=PA6`, siehe `libraries/Spi_lib.h` — identische Pinbelegung wie bei `can_bus_attack`),
   `ufbt`/`fbt`-Build gegen die eigene `mntm-012`-Basis mit `canbus` in `applications/external/`.
2. Regressionstest: "Sniffing" gegen eine zweite CAN-Quelle mit ausschließlich gültigen Rahmen
   (DLC 0–8) öffnen — Verhalten vor/nach Patch muss identisch sein.
3. Fundnachvollzug Sniffing/UDS (Stack-Fall): wie im Testhinweis von
   `firmware-mining/can-bus-attack-app-port.md` beschrieben einen Rahmen mit manipuliertem
   DLC-Feld (9–15) senden, dabei zusätzlich "UDS Services" → eine beliebige Abfrage (z. B.
   "Get VIN") mit einer antwortenden ECU/einem zweiten Mikrocontroller testen, der ein
   Antwortframe mit manipuliertem DLC-Nibble sendet.
4. Fundnachvollzug OBD2 (Heap-Fall, neu): "Scanner OBD2" → "Manual PID Sender" mit einer
   ECU/einem simulierten Antwortsender verwenden, der auf die Anfrage mit einem Frame mit
   DLC-Nibble 9–15 antwortet — vor dem Patch potenziell Korruption des `app->obdii`
   nachfolgenden `App`-Struktur-Felds (je nach konkretem Struct-Layout des Build), nach dem
   Patch: Antwort wird mit auf 8 begrenzter Datenlänge verarbeitet.
5. Falls lokal kein Fault-Injector/keine manipulierbare ECU verfügbar ist: der bereits für
   `can_bus_attack` erstellte Host-Repro deckt die zugrundeliegende Funktion vollständig und
   reproduzierbar ab; der Hardware-Test hier ist in erster Linie eine Regressionsprüfung für den
   gültigen Bereich (DLC 0–8) sowie ein Funktionsnachweis für die beiden neu erreichbaren
   Aufrufpfade.

## Risiko

Niedrig. Identischer additiver Patch wie bei `can_bus_attack`, bereits dort host- und
patch-verifiziert; kein Verhaltensunterschied für den protokollkonformen Bereich (DLC 0–8). Für
DLC 9–15 ändert der Patch das Verhalten bewusst von "undefiniert/Speicherfehler" zu "Rahmen wird
mit auf 8 begrenzter Datenlänge verarbeitet" — dieselbe bewusste Kappung wie in der
Referenzbibliothek (`coryjfowler`/Seeed `MCP_CAN`), kein Informationsverlust für reale,
protokollkonforme CAN-Rahmen.

## Sicherheitsrahmen

Fund und Fix betreffen ausschließlich die Robustheit des eigenen Pentest-/Diagnose-Werkzeugs
gegenüber Rahmen vom per MCP2515-Modul direkt verkabelten Bus (eigenes Fahrzeug/Testaufbau oder
autorisierter Pentest, CONSTRAINTS Punkt 5). Keine Kartendaten oder echte Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe mit MCP2515-Hardware weiterhin offen, wie bei allen vier bereits geprüften
  CAN-Apps dieses Clusters.
- Aus demselben Cluster bleibt `can_commander` (15165 Zeilen, eigenes GPIO-Board laut
  Quellcode-Kommentar vorausgesetzt) als letzter, noch nicht einzeln geprüfter Kandidat.
- Restliche, `canbus`-eigene Bibliotheken (`lib/SLCAN_serial`, `lib/files_scaner`,
  `lib/log_exporter`, `lib/hex_converter`, `libraries/frame_queue.c`) vollständig gelesen, ohne
  eigenen Fund über die hier dokumentierte Klasse hinaus — eigenständiger, kleinerer
  Rückstellpunkt für eine künftige, tiefere Prüfung (insbesondere `frame_extractor.c`s
  `frame_splitter()`, siehe `ideas/canbus-api-status.md`, Abschnitt „Offen").

## Quelle

- `ElectronicCats/flipper-MCP2515-CANBUS` (direktes Repo, Ordner `Canbus_app/`) bzw.
  `applications/external/canbus/` im RogueMaster-Bündel
  (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`).
