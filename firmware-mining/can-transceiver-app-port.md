# CAN Transceiver — externe App von Chainmanner (MCP2515-Low-Level-Ansteuerung)

## Was

Aus der seit der CAN-Tools-Sitzung offenen Restliste des RogueMaster-CAN-Bus-Clusters
(`can_bus_attack`, `can_commander`, `can_transceiver`, `canbus` — alle vier mit
MCP2515-Transceiver-Abhängigkeit, `can_tools` selbst bereits geprüft) **CAN Transceiver**
(`Chainmanner/Flipper-CAN-Transceiver`, App-ID `can_transceiver`) aufgegriffen: mit zwölf
Quelldateien über `can_transceiver.c`/`.h`, `can_transceiver_globalconf.c`/`.h`,
`can_transceiver_rxhistory.c`/`.h`, vier `scenes/`- und vier `views/`-Dateien sowie dem eigenen
`lib/mcp2515/`-Treiber (zusammen ca. 2500 Zeilen über die Haupt-App, dazu der MCP2515-SPI-Treiber
separat) die kleinste der vier noch offenen Hardware-Apps. Laut eigenem README selbst als "erste
App des Autors" und "nicht vollständig" bezeichnet.

Direkte Low-Level-Ansteuerung eines MCP2515-CAN-Controllers über den Erweiterungsport (SPI,
`furi_hal_spi_bus_handle_external`): **CAN Rx** (laufender Empfang, bis zu 1000 Nachrichten im
RAM, Scroll-Liste mit Frame-ID/Daten/Flags, CSV-Export auf die SD-Karte, REC-Fehlerzähler-Anzeige)
und **CAN Tx** (manuelle Frame-ID per Byte-Eingabe, 0–8 Datenbyte, Standard-/Extended-ID,
One-Shot-Modus, TEC/Bus-Off-Anzeige). Kein DBC-Bezug (das deckt das bereits geprüfte `can_tools`
ab) — reines Rohframe-Werkzeug.

## Nutzen für mich

Liefert tatsächliche, rohe CAN-Frames vom Bus (Senden wie Empfangen), die `can_tools` erst
sinnvoll interpretierbar macht — ergänzt damit die bereits dokumentierte Lücke zwischen
Bus-*Zugriff* und Signal-*Interpretation*. Von den vier noch offenen Hardware-Apps die
unmittelbarste (kein eigenes Zusatz-GPIO-Board wie `can_commander` laut eigenem Kommentar, nur
ein Standard-MCP2515-Modul am Erweiterungsport).

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware` (Tag `mntm-012`)
geprüft:

- `furi_hal_spi.h`/`furi_hal_spi_config.h`: `furi_hal_spi_bus_handle_external` sowie
  `furi_hal_spi_acquire/_release/_bus_tx/_bus_rx/_bus_handle_init/_bus_handle_deinit` —
  byteidentisch vorhanden, dasselbe Muster wie bereits bei SPI Flash Dump/SWD Probe/i2c Tools
  verifiziert.
- `furi_hal_power_enable_otg/_disable_otg()` (für die externe 5-V-Versorgung des
  MCP2515-Moduls) — vorhanden, bereits an anderer Stelle (eigene `g4meover-wifi-relay`-Hardware)
  genutzt.
- `gui/modules/{submenu,widget,text_input,byte_input,variable_item_list}.h`,
  `dialogs/dialogs.h`, `storage/storage.h` — Standard-GUI-/Storage-APIs, byteidentisch
  vorhanden, inklusive `byte_input_set_result_callback()`s sechs Parametern exakt wie von
  `can_transceiver_scene_cantx.c` aufgerufen.
- `lib/mlib` (`m-array.h`, `ARRAY_DEF`/`M_EACH`): fester Submodul-Stand (`P-p-H-d/mlib`), vom
  RX-Verlauf (`can_transceiver_rxhistory.c`) genutzt — dieselbe, firmwareweit stabile Bibliothek.

Kein Port-Hindernis: reines externes FAP, keine Firmware-Core-Änderung nötig. Einziger
MCP2515-spezifischer Teil (`lib/mcp2515/mcp2515_spi.c`/`.h`, `mcp2515_spi_worker.c`/`.h`) ist
reine SPI-Registeransteuerung ohne Firmware-HAL-Abhängigkeit über die bereits genannten
`furi_hal_spi_*`-Funktionen hinaus.

## Fund: RX-Verlaufseinträge übernehmen nicht initialisierte Flag-Bits für Standard-/Datenrahmen

`CANTransceiver_SceneCANRx_MessageRxCallback()` (`scenes/can_transceiver_scene_canrx.c`) baut für
jede empfangene Nachricht einen neuen Verlaufseintrag:

```c
CANTransceiver_RxHistory_Entry pNewEntry;   // lokal, KEIN Initialisierer, KEIN memset

pNewEntry.u32FrameID = pInstance->pMCP2515Worker->u32FrameID_Rx & 0x1FFFFFFF;
if (pInstance->pMCP2515Worker->u32FrameID_Rx & 0x80000000)
	pNewEntry.bExtendedIDFrame = true;        // nur jemals auf true gesetzt
if (pInstance->pMCP2515Worker->u32FrameID_Rx & 0x40000000)
	pNewEntry.bRemoteTransferFrame = true;    // nur jemals auf true gesetzt
...
pNewEntry.u4FrameDataLen = pInstance->pMCP2515Worker->u8FrameDataLen_Rx & 0x0F;
...
CANTransceiver_RxHistory_AddEntry(pInstance->pRxHistory, &pNewEntry);  // roher memcpy von pNewEntry
```

`CANTransceiver_RxHistory_Entry` (`can_transceiver_rxhistory.h`) packt drei Bitfelder in ein
gemeinsames Byte: `u4FrameDataLen:4`, `bExtendedIDFrame:1`, `bRemoteTransferFrame:1`,
`reserved:2`. Für `bExtendedIDFrame`/`bRemoteTransferFrame` gibt es **keinen** `else`-Zweig, der
das Bit explizit auf `false` setzt — anders als bei jedem anderen Feld der Funktion. `pNewEntry`
ist eine ganz normale automatische (Stack-)Variable ohne Initialisierer: für einen echten
Standard-, Nicht-Remote-Datenrahmen (beide Markierungsbits in `u32FrameID_Rx` gleich 0) behalten
beide Flag-Bits den unbestimmten Wert, den dieser Speicherplatz zuvor zufällig trug. Da
`u4FrameDataLen` ein Nachbar-Bitfeld im selben Byte ist, übersetzt der Compiler die Zuweisung
`pNewEntry.u4FrameDataLen = ...` in ein Read-Modify-Write des gesamten Bytes — die beiden
Flag-Bits (und `reserved`) werden dabei **nicht** angerührt, der unbestimmte Vorwert überlebt
unverändert bis zum `memcpy()` in `CANTransceiver_RxHistory_AddEntry()` und landet damit sowohl
in der Bildschirmanzeige (`views/can_transceiver_view_canrx.c`, Spalte "F:X"/"F:R") als auch im
CSV-Export (`CANTransceiver_SceneCANRx_SaveFrameCap()`, Spalten `is_extended`/`is_remote`).

Praktische Auswirkung: ein tatsächlich empfangener Standard-ID-/Datenrahmen kann in der
Verlaufsliste und im Export fälschlich als Extended-ID- und/oder Remote-Frame erscheinen (oder
umgekehrt teils korrekt, abhängig vom Zufallsinhalt des Stacks) — für ein Pentest-/
Diagnosewerkzeug, dessen Zweck gerade die verlässliche Protokollierung echter Busnachrichten ist,
ein relevanter Korrektheitsfund, auch ohne Speicherfehler oder Absturzrisiko (reine
Bitfeld-Fehlanzeige, kein Out-of-Bounds-Zugriff).

Betrifft nur den Empfangspfad (`CANTransceiver_SceneCANRx_MessageRxCallback()`); der
Sendepfad (`can_transceiver_scene_cantx.c`) setzt seine Felder vollständig und ohne
bedingte Bitfelder.

## Host-Repro

Vier Szenarien, exakte Struktur- und Feldzuweisungs-Reihenfolge aus
`can_transceiver_rxhistory.h`/`can_transceiver_scene_canrx.c` transkribiert: (1) Regressionsfall
— echter Extended-+Remote-Rahmen, ALT und NEU liefern unabhängig vom Vorinhalt des Speicherplatzes
identisch `true`/`true`; (2) Fundfall — echter Standard-, Nicht-Remote-Rahmen mit sechs
verschiedenen Vorinhalt-Mustern, ALT meldet je nach Muster fälschlich `Ext=1` und/oder `Rem=1`,
NEU immer korrekt `0`/`0`; (3) Eingrenzung — `u32FrameID`/`u4FrameDataLen`/`u8pFrameData` bleiben
auch unter ALT korrekt, der Fund betrifft ausschließlich die beiden bedingt gesetzten Bits; (4)
vollständiger Sweep über alle 256 möglichen Vorinhalt-Bytes — ALT liefert nur für 64/256 (die
Muster, bei denen beide relevanten Bits bereits 0 sind) das korrekte Ergebnis, NEU für 256/256.
Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` verifiziert: alle vier Szenarien bestätigt, identischer Output
beider Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen — in
`firmware-mining/repro-can-transceiver-rxhistory-uninit-flags/repro.c`.

## Betroffene Datei

- `scenes/can_transceiver_scene_canrx.c` — Funktion `CANTransceiver_SceneCANRx_MessageRxCallback()`.

## Port-Skizze

1. App unverändert aus `Chainmanner/Flipper-CAN-Transceiver` (bzw. `applications/external/
   can_transceiver/` im RogueMaster-Bündel, Branch `420`) in meine eigene
   `applications/external/can_transceiver/`-Struktur übernehmen (reines FAP, keine
   Firmware-Core-Änderung).
2. Patch `firmware-mining/can-transceiver-app-port/can_transceiver_scene_canrx_c.patch` danach
   anwenden — fügt ein additives `memset(&pNewEntry, 0, sizeof(pNewEntry));` direkt vor der
   ersten Feldzuweisung ein, nach demselben Muster, das die Funktion selbst bereits für die
   Testdaten im `#if 0`-Block (Zeile ~287, "FIXME: TESTING RX HISTORY") sowie
   `CANTransceiver_SceneCANRx_SaveFrameCap()`/`_ResetGlobalsAndView()` an anderer Stelle nutzt.
   Patch per `git apply --check` gegen eine frische, unabhängige Zweitkopie verifiziert, real
   angewendet, byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

## Testhinweis (lokal, mit MCP2515-Hardware)

1. MCP2515-Transceiver-Modul am Erweiterungsport verkabeln (siehe `scenes/
   can_transceiver_scene_wiring.c`/eigenes README), `ufbt`/`fbt`-Build gegen die eigene
   `mntm-012`-Basis mit `can_transceiver` in `applications/external/`.
2. "CAN Rx" gegen eine zweite CAN-Quelle (zweiter MCP2515-Knoten, Mikrocontroller mit
   CAN-Transceiver oder tatsächliches Fahrzeugnetz im Rahmen eines autorisierten Tests) öffnen,
   mehrere **Standard-ID-Datenrahmen ohne RTR-Bit** senden lassen — vor dem Patch: Spalte "F:"
   zeigt für einzelne Nachrichten wiederholt zufällig "X" und/oder "R" an, obwohl keine der
   gesendeten Nachrichten extended oder remote war; nach dem Patch: durchgehend "F:--" für diese
   Nachrichten.
3. CSV-Export ("Right"-Taste) vor/nach dem Patch gegenprüfen: Spalten `is_extended`/`is_remote`
   müssen nach dem Patch für dieselben Standard-/Datenrahmen durchgehend `0,0` zeigen.
4. Regressionstest: einen echten Extended-ID- und einen echten Remote-Rahmen senden/empfangen,
   beide müssen vor und nach dem Patch unverändert korrekt als "F:X-"/"F:-R" erscheinen.

## Risiko

Niedrig. Rein additive Initialisierung, kein Verhaltensunterschied für die bereits korrekt
funktionierenden Felder (`u32FrameID`, `u4FrameDataLen`, `u8pFrameData`, per Host-Repro-Szenario 3
belegt) und für den bereits korrekten `true`-Fall (Szenario 1). Kein Speicherfehler, keine
Absturzgefahr — reine Anzeige-/Export-Korrektheit.

## Sicherheitsrahmen

Reiner Anzeige-/Protokollierungsfund in einem Werkzeug, das ausschließlich mit dem per
MCP2515-Modul angeschlossenen Bus interagiert (eigenes Fahrzeug/Testaufbau oder autorisierter
Pentest, CONSTRAINTS Punkt 5). Keine Kartendaten oder echte Zugangsdaten beteiligt, keine
Netzwerk-/Funkkomponente über den direkt verkabelten CAN-Bus hinaus.

## Offen

- Lokale Bauprobe mit MCP2515-Hardware weiterhin offen (wie bei allen vier hardwareabhängigen
  CAN-Apps dieses Clusters).
- Aus demselben Cluster bleiben `can_bus_attack`, `can_commander`, `canbus` noch nicht einzeln
  geprüft.

## Quelle

- `Chainmanner/Flipper-CAN-Transceiver` (direktes Repo) bzw. `applications/external/
  can_transceiver/` im RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`,
  Branch `420`).
