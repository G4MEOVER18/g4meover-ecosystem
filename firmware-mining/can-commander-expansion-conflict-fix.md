# CAN Commander — externe App von MatthewKuKanich (ESP32-Companion-Board, UART)

## Was

Letzten noch offenen Kandidaten aus dem seit der CAN-Tools-Sitzung geführten RogueMaster-
CAN-Bus-Cluster (`can_bus_attack`, `can_commander`, `can_tools`, `can_transceiver`, `canbus` —
die anderen vier inzwischen aufgegriffen) aufgegriffen: **CAN Commander**
(`MatthewKuKanich/CAN_Commander`, App-ID `can_commander`). Anders als im Katalog bisher vermerkt
("eigenes GPIO-Board laut Quellcode-Kommentar vorausgesetzt", 15165 Zeilen über 29 Dateien,
MCP2515-artige SPI-Ansteuerung vermutet) ist die aktuelle Version (`v2.3.1`, 15615 Zeilen über
41 Dateien, davon `CAN_Commander_FlipperZero/can_commander/` die eigentliche Flipper-App) **kein**
MCP2515-SPI-Treiber, sondern ein reines UART-Protokoll (921600 Baud, COBS-Framing + CRC16-CCITT)
zu einem eigenständigen **ESP32-Companion-Board mit eigener CAN-Commander-Firmware**
(`cancommander.com`) — dieselbe Fremdhardware-Kategorie wie `can_commander` im `nfc_relay`-/
`chameleon`-Sinn (zwei eigene Geräte bzw. ein Fremdgerät mit eigenem Protokoll), nicht die
MCP2515-Kategorie von `can_bus_attack`/`can_transceiver`/`canbus`.

Wie bei **Chameleon Ultra Client** und **NFC Relay** (beide ebenfalls als "Fremdhardware, kein
eigener Sensorzugriff" zurückgestellt, dann aber wegen des eigenen Protokoll-Parsers doch
geprüft) lohnt sich die Prüfung trotz fehlender eigener Hardware: `libraries/
can_commander_uart.c` (COBS-Encoder/-Decoder, CRC16, Paket-/Event-Parser) ist vollständig gelesen,
ebenso die Haupt-App (`can_commander.c`, ~5000 Zeilen App-Logik: Custom-Inject-Slots,
DBC-Profile, Argument-Parser, Datei-I/O) und die Dashboard-Views (`views/dashboard/*.c`, ~3900
Zeilen: Bittrack, Reverse/Byte-Watcher, DBC-Decode-Anzeige). Kein Speicherfehler gefunden — die
UART-Framing-Schicht ist durchgehend defensiv (jede `memcpy()`/`snprintf()` klemmt gegen eine
geprüfte Länge, jeder externe Enum-Wert wird nur per `==`-Vergleich verwendet, nie als
Array-Index; Details siehe unten). Der eine echte, konkrete Fund liegt eine Ebene darunter: im
**UART-Zugriffs-Lifecycle**, nicht im Protokoll-Parsing selbst.

## Nutzen für mich

Schließt den letzten offenen Punkt aus dem CAN-Bus-Cluster ab (nur noch als reiner
Fremdhardware-Fall ohne eigenen Quellcode-Fund offen, siehe `ideas/can-commander-api-status.md`).
Zusätzlich ein eigenständiger, sofort verwertbarer Fund: dieselbe Fehlerklasse, die ich bereits
bei UART Sniff/RS485 Modbus/GPIO Logic Analyzer/SWD Probe/Argus als "Expansion-Service-
Übernahme" etabliert habe, fehlt hier komplett (nicht nur in falscher Reihenfolge wie bei
RS485 Modbus) — ein direkter, lehrreicher Gegenbeleg dafür, dass dieses Muster bei jeder neuen
UART-Erweiterungsport-App einzeln geprüft werden muss, auch wenn der Autor selbst (laut eigenem
README) bereits mehrfach an der Verbindungsrobustheit gearbeitet hat.

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag `mntm-012`)
geprüft:

- `furi_hal_serial_control.h`: `furi_hal_serial_control_acquire()`, `_release()`,
  `_is_baud_rate_supported()` — byteidentisch vorhanden.
- `furi_hal_serial.h`: `furi_hal_serial_init()`, `_tx()`, `_tx_wait_complete()`,
  `_async_rx_start()` (inkl. exakt der von `can_commander_uart.c` genutzten
  Vier-Parameter-Signatur `(handle, callback, context, report_errors)`), `_async_rx_available()`,
  `_async_rx()`, `_async_rx_stop()`, `_deinit()`, `FuriHalSerialRxEventData` — byteidentisch
  vorhanden, moderne API (kein alter `cli_session_open/close`-Kompat-Pfad wie bei GPIO Logic
  Analyzer/SWD Probe/UART Sniff nötig).
- `applications/services/expansion/expansion.h`: `RECORD_EXPANSION`, `Expansion`-Opaque-Typ,
  `expansion_enable()`/`expansion_disable()` — byteidentisch vorhanden, exakt dieselbe Bibliothek,
  die UART Sniff/RS485 Modbus/GPIO Logic Analyzer/SWD Probe/Argus bereits nutzen (müssten).
- Standard-GUI-/Storage-/`flipper_format`-APIs — byteidentisch, keine Abweichung.
- `furi_hal_power_enable_otg()`/`_disable_otg()` (5-V-Versorgung des ESP32-Boards) — vorhanden,
  bereits an anderer Stelle (`g4meover-wifi-relay`) genutzt.

Kein Port-Hindernis: reines externes FAP (`applications/external/can_commander/`), keine
Firmware-Core-Änderung nötig.

## Fund: Kein `expansion_disable()`/`expansion_enable()` — App kann vom Stock-Expansion-Service dauerhaft ausgesperrt werden

`app_alloc()`/`app_free()`/`cancommander_main()` (`can_commander.c`) öffnen nirgends
`RECORD_EXPANSION` und rufen nirgends `expansion_disable()`/`expansion_enable()` auf (per
`grep -rn "expansion"` im gesamten Quellbestand bestätigt — null Treffer). Fünf andere, bereits
in diesem Katalog dokumentierte Erweiterungsport-Apps (UART Sniff, RS485 Modbus, GPIO Logic
Analyzer, SWD Probe, Argus) tun das; bei UART Sniff/GPIO Logic Analyzer/SWD Probe korrekt, bei
RS485 Modbus mit einer bereits dokumentierten Reihenfolge-Verwechslung — `can_commander` hat
stattdessen **gar keinen** der beiden Aufrufe.

Das ist deshalb kein kosmetisches Versäumnis, sondern führt nachweislich zu einem dauerhaften
Verbindungsausfall, wenn der Stock-"Expansion"-Dienst in seiner Standardeinstellung läuft
(mein eigener `G4MEOVER-FW`-Zweitkopie-Stand geprüft, Datei- und Funktionsnamen unten):

1. `applications/services/expansion/expansion.c::expansion_on_system_start()` ruft beim
   Systemstart unbedingt `expansion_enable()` auf (sofern eine SD-Karte gemountet ist).
   `expansion_settings_load()` fällt beim Fehlen einer eigenen Einstellungsdatei auf
   `memset(settings, 0, ...)` zurück — `uart_index = 0` entspricht `FuriHalSerialIdUsart`,
   also genau dem UART, das `can_commander_uart.c` selbst für die Board-Verbindung nutzt
   (`furi_hal_serial_control_acquire(FuriHalSerialIdUsart)`). Der Expansion-Dienst überwacht
   damit standardmäßig dieselben Erweiterungsport-Pins.
2. `expansion_control_handler_enable()` registriert dafür
   `furi_hal_serial_control_set_expansion_callback()`, das laut
   `furi_hal_serial_control.c::furi_hal_serial_control_enable_expansion_irq()` einen
   GPIO-Interrupt auf **fallende Flanke** des RX-Pins scharf schaltet — reagiert also bereits auf
   das allererste Startbit eines beliebigen Bytes, nicht erst auf ein vollständiges
   Handshake-Paket.
3. Das vom `can_commander`-App selbst dokumentierte Boot-Verhalten (README, Abschnitt 1 "Scope
   and App Behavior"): die App schaltet beim Start zuerst die 5-V-OTG-Versorgung für das
   ESP32-Board ein, wartet dann **fest 400 ms**, bevor sie überhaupt `cc_client_open()` (→
   `furi_hal_serial_control_acquire()`) aufruft. Das ESP32-Board bootet in dieser Zeit und beginnt
   typischerweise früher als 400 ms nach Power-On, auf seiner UART-TX-Leitung (= Flipper-RX)
   Pegel zu wechseln (Boot-Meldungen, erste eigene Ping-Antwortversuche, Bus-Rauschen) — jeder
   dieser Pegelwechsel kann die obige Flanken-Erkennung auslösen.
4. Löst die Flanke aus, wechselt der Expansion-Dienst nach `ExpansionStateRunning` und ruft
   `expansion_worker_alloc()`/`_start()` auf — `expansion_worker.c` ruft darin selbst
   `furi_hal_serial_control_acquire(instance->serial_id)` auf und hält das Handle, während es
   mit der (deutlich niedrigeren) Standard-Baudrate des offiziellen Erweiterungsmodul-Protokolls
   auf ein eigenes Handshake wartet.
5. Ruft `can_commander` danach (nach den vollen 400 ms) selbst
   `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)` auf (`cc_client_open()`), liefert die
   Funktion laut eigener Doku ("return NULL or if interfaces is in use") **`NULL`** zurück, weil
   das Handle bereits dem Expansion-Worker gehört — `cc_client_open()` schlägt fehl, drei
   Ping-Versuche (`app_connect()`) laufen ins Leere, und die App zeigt ausschließlich die
   generische Meldung `"Not connected to \nCAN Commander"`.
6. Die eigene Fehler-Glossar-Dokumentation der App (README, Abschnitt 6) weist für genau diese
   Meldung ausschließlich auf **"verify power/wiring/module state"** hin — es gibt in der
   gesamten App und README keinen Hinweis darauf, dass die Ursache in den Flipper-eigenen
   `Settings → System → Expansion`-Einstellungen liegen kann. Der Nutzer folgt der dokumentierten
   Fehlersuche und findet die tatsächliche Ursache nie.
7. Der Expansion-Worker gibt das Handle irgendwann wieder frei (eigenes Handshake-Timeout bzw.
   `ExpansionMessageTypeModuleDisconnected`), aber zu diesem Zeitpunkt hat `can_commander` seine
   drei Ping-Versuche bereits erschöpft und aufgegeben — ein manueller
   `Settings → Connect/Reconnect` in der App kann danach zufällig funktionieren (wenn der
   Expansion-Dienst inzwischen wieder im Leerlauf ist) oder erneut fehlschlagen (wenn ein neuer
   Pegelwechsel die Erkennung erneut auslöst), was das Verhalten für den Nutzer zusätzlich
   unvorhersehbar macht.

Betroffen ist damit **jede** Erstverbindung (und jeder `force_reconnect`) auf einem Flipper, auf
dem die Expansion-Modul-Erkennung aktiv ist — das ist, da `expansion_enable()` beim Systemstart
unbedingt aufgerufen wird und die Standardeinstellung exakt das von `can_commander` genutzte
UART überwacht, der Auslieferungszustand selbst, nicht ein exotischer Sonderfall.

## Betroffene Dateien

- `CAN_Commander_FlipperZero/can_commander/can_commander.h` — `struct App` (fehlendes
  `Expansion*`-Feld).
- `CAN_Commander_FlipperZero/can_commander/can_commander.c` — `app_alloc()`/`app_free()`
  (fehlende `expansion_disable()`/`expansion_enable()`-Aufrufe um den gesamten
  App-Lebenszyklus).

## Fix

Additiv, nach demselben bei UART Sniff/GPIO Logic Analyzer/SWD Probe bereits etablierten Muster
(„`expansion_disable()` VOR der ersten `furi_hal_serial_control_acquire()`-Nutzung beim Start,
`expansion_enable()` NACH der letzten `furi_hal_serial_control_release()`-Nutzung beim Stop";
siehe `ideas/uart-sniff-api-status.md`):

- `can_commander.h`: `#include <expansion/expansion.h>`, Feld `Expansion* expansion;` in
  `struct App` direkt neben `CcClient* client;`.
- `can_commander.c`, `app_alloc()`: direkt nach dem erfolgreichen `cc_client_alloc()` (und damit
  klar vor jedem späteren `cc_client_open()`-Aufruf aus `cancommander_main()`/`app_connect()`)
  `app->expansion = furi_record_open(RECORD_EXPANSION); expansion_disable(app->expansion);`.
- `can_commander.c`, `app_free()`: direkt nach `cc_client_free(app->client)` (das intern
  `cc_client_close()` und damit `furi_hal_serial_control_release()` aufruft) —
  `if(app->expansion) { expansion_enable(app->expansion); furi_record_close(RECORD_EXPANSION); }`.
  Reihenfolge bewusst *nach* dem Client-Teardown, um die bei RS485 Modbus dokumentierte
  vertauschte Reihenfolge nicht zu wiederholen.

Patch (`firmware-mining/can-commander-expansion-conflict-fix/can_commander_expansion_fix.patch`)
gegen eine frische, unabhängige Zweitkopie von `MatthewKuKanich/CAN_Commander` (`v2.3.1`,
Commit `6079fe0`) mit `git apply --check` verifiziert, real angewendet, Ergebnis per `diff -r`
byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

## Host-Repro

Nicht sinnvoll als eigenständiges Host-Programm nachstellbar — der Fund ist kein
Speicherfehler/keine Logikabweichung in reinem C-Code, sondern ein Ressourcenkonflikt zwischen
zwei Firmware-Diensten (`Expansion`-Systemdienst vs. App) um ein einziges Hardware-UART-Handle,
nur über den tatsächlichen `furi_hal_serial_control`-/`expansion`-Quellcode der Firmware selbst
nachvollziehbar (siehe Fund-Abschnitt, Schritte 1–7, jeder Schritt mit exakter
Datei-/Funktionsreferenz gegen `G4MEOVER-FW`/`mntm-012`). Der Fix selbst ist rein additiv und
ändert an keiner Stelle bestehende Kontrollflüsse, die ein Host-Repro der UART-Protokollschicht
(COBS/CRC16, siehe `ideas/can-commander-api-status.md`) beeinflussen würden.

## Testhinweis (lokal, mit CAN-Commander-ESP32-Board)

1. `can_commander` (mit Patch) gegen die eigene `mntm-012`-Basis mit `ufbt`/`fbt` bauen,
   `applications/external/can_commander/` wie gewohnt einbinden.
2. Vor dem Patch: sicherstellen, dass `Settings → System → Expansion` auf dem Gerät **aktiviert**
   ist (Flipper-Auslieferungszustand) und auf `USART` zeigt (Standard). CAN-Commander-Board per
   Erweiterungsport anschließen, App öffnen — reproduzierbar (nicht bei jedem Versuch, da vom
   exakten Boot-Timing des ESP32-Boards abhängig, aber über mehrere Neustarts der App/des Boards
   wiederholt beobachtbar) `"Not connected to \nCAN Commander"` trotz korrekt verkabeltem,
   funktionierendem Board.
3. Nach dem Patch: identischer Ablauf, App ruft `expansion_disable()` vor dem ersten
   Verbindungsversuch auf — der Expansion-Dienst kann das Handle währenddessen nicht mehr an sich
   reißen, `Ping` muss zuverlässig bei jedem Start gelingen.
4. Regressionstest: App normal beenden (Zurück aus dem Hauptmenü) und erneut ein
   Erweiterungsmodul (z. B. WiFi Dev Board, falls vorhanden) verwenden — muss nach dem
   `expansion_enable()` in `app_free()` unverändert funktionieren (kein dauerhaftes Abschalten
   der Expansion-Erkennung über das Ende der `can_commander`-Sitzung hinaus).

## Risiko

Niedrig. Rein additiv (zwei neue Aufrufe plus ein neues Feld), ändert nichts an der bestehenden
UART-/Protokolllogik. Einziger Nebeneffekt (beabsichtigt, identisch zu den fünf bereits
etablierten Referenz-Apps): während `can_commander` läuft, kann der Flipper währenddessen kein
offizielles Erweiterungsmodul gleichzeitig über dieselben Pins erkennen — exakt das erwartete,
schon mehrfach akzeptierte Verhalten für eine App, die den Erweiterungsport exklusiv für ein
eigenes Gerät nutzt.

## Sicherheitsrahmen

Reiner Verbindungs-/Verfügbarkeitsfund (kein Speicherfehler, keine Remote-Angriffsfläche) in
einem Werkzeug, das ausschließlich mit dem per UART direkt angeschlossenen eigenen
CAN-Commander-Board interagiert (eigenes Fahrzeug/Testaufbau oder autorisierter Pentest,
CONSTRAINTS Punkt 5). Keine Kartendaten oder echte Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe sowie Anschaffung/Verkabelung des CAN-Commander-ESP32-Boards weiterhin offen
  (einzige verbleibende Fremdhardware-Anschaffung aus dem CAN-Bus-Cluster).
- Die UART-Protokollschicht selbst (`can_commander_uart.c`: COBS-Encoder/-Decoder, CRC16,
  Paket-/Event-Parser) sowie die Haupt-App-Logik (Custom-Inject-Slots, DBC-Profile,
  Dashboard-Views) sind durchgesehen und defensiv implementiert (jede Puffergrenze geprüft,
  jeder externe Enum-Wert nur per Vergleich statt als Array-Index verwendet) — kein weiterer
  Fund über den Expansion-Konflikt hinaus. Details in `ideas/can-commander-api-status.md`.

## Quelle

- `MatthewKuKanich/CAN_Commander` (direktes Repo, `v2.3.1`, Commit `6079fe0`) bzw.
  `applications/external/can_commander/` im RogueMaster-Bündel.
- Referenz-Fix-Muster: `ideas/uart-sniff-api-status.md`, `ideas/rs485-modbus-app-status.md`
  (bereits dokumentierte `expansion_disable()`/`_enable()`-Nutzung derselben Firmware-API).
