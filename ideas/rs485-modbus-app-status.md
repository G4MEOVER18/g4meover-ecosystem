# RS485 Modbus (Electronic Cats) — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer Ergänzung für die
bislang nur drei Einträge (UART Sniff, GPIO Logic Analyzer, i2c Tools) enthaltende Kategorie
„GPIO / Serial Bus" gesucht — bei mir bereits zwei CAN-Bus-Funde
(`firmware-mining/canbus-attack-app-port.md`, `firmware-mining/canbus-electroniccats-app-port.md`)
vom selben Hersteller (Electronic Cats) dokumentiert, dessen RS485-Modbus-Addon als nächster
naheliegender Kandidat aus derselben Produktreihe geprüft.

## Quelle
- Repo: `ElectronicCats/flipper-rs485modbus` (MIT-Lizenz, 29 Stars, 63 Commits auf `main`,
  aktueller Commit `ff50246` — unverändert seit der ersten Prüfung). Quelltext direkt per
  `git clone`/`curl` gegen `raw.githubusercontent.com` gelesen (`ModbusApp/Modbus.c/.h`,
  `modbus_uart/`, `modbus_parser/`, `modbus_sender/`, `modbus_storage/`, `modbus_ring_buffer/`,
  `scenes/`, `scenes_config/`, `application.fam`) — nicht nur Zusammenfassung.
- Erfordert das dazugehörige Electronic-Cats-Hardwaremodul „Flipper Add-On RS485 Modbus"
  (eigene Platine, steckt auf dem Erweiterungsport, RS485-Transceiver dahinter) — dieselbe
  Fremdhardware-Kategorie wie das bereits dokumentierte CAN-Bus-Addon desselben Herstellers.
- `appid` `modbus`, `fap_category` „Misc", `requires=["gui"]`, kein `fap_version`-Eintrag im
  `application.fam`.

## Was
Modbus-RTU-Werkzeug über RS485 (Erweiterungsport-UART, `FuriHalSerialIdUsart` fest verdrahtet,
kein Kanal-Umschalter wie bei UART Sniff): passiver Sniffer mit Live-Textbox (Hex/ASCII-Wahl),
Master-seitiges Senden vorgefertigter oder manuell eingegebener Anfragen (Read Coils/Discrete
Inputs/Holding/Input Registers, Write Single/Multiple Coil/Register — alle acht Standard-
Funktionscodes inkl. Exception-Code-Decodierung), Session-Log auf SD sowie ein Ringpuffer für
zuletzt gesehene Pakete zur erneuten Injektion. Konfigurierbar: 12 Baudraten (1200–921600),
Datenbreite 7/8/9 Bit, Stopbits 0,5/1/1,5/2, Parität None/Even/Odd, Timeout.

## Nutzen für mich
Modbus RTU ist ein in Industrieumgebungen (SPS, HLK, Energiezähler, Gebäudeleittechnik) weit
verbreitetes serielles Protokoll, das UART Sniff nur als undekodierten Rohbytestrom anzeigen
würde — diese App bringt die protokollspezifische Decodierung (Funktionscode, Adresse, Menge,
CRC, Exceptions) und eine Sendefunktion für autorisierte Konfigurations-/Fuzzing-Tests gegen
eigene oder ausdrücklich freigegebene Modbus-Slaves mit. Ergänzt die bestehenden CAN-Bus-Funde
um einen zweiten, ebenfalls industriell verbreiteten Bus-Typ, für den ich bislang kein
protokollbewusstes Werkzeug habe.

## API-87-Status
🔧 Größtenteils kompatibel, mit einem konkreten Vorbehalt. Gegen den eigenen `mntm-012`-Header-
Stand verifiziert (`targets/f7/furi_hal/furi_hal_serial.h`,
`furi_hal_serial_types.h`, `furi_hal_serial_control.h`, Tag `mntm-012` direkt per `curl`
gelesen): `furi_hal_serial_control_acquire()`/`_release()` mit `FuriHalSerialIdUsart`,
`furi_hal_serial_set_br()`, `furi_hal_serial_dma_rx_start()`/`_stop()`, `furi_hal_serial_dma_rx()`
sowie `FuriHalSerialRxEvent`/`FuriHalSerialRxEventData`/`FuriHalSerialRxEventIdle` — exakt
dieselben Signaturen, keine Abweichung. `expansion_disable()`/`_enable()` werden beim Start in
der vom Header vorgeschriebenen Reihenfolge aufgerufen (`expansion_disable()` vor der ersten
Serial-Nutzung).

**Konkreter Fund:** `modbus_uart.c::Serial_Begin()` konfiguriert USART1 nicht über die dafür
vorgesehene öffentliche API, sondern per direkten `LL_USART_Init()`/`LL_USART_Enable()`-Aufrufen
gegen das rohe STM32WB-Register-Interface (`stm32wbxx_ll_usart.h`) und setzt GPIO-Alternate-
Function/Takt (`furi_hal_gpio_init_ex(..., GpioAltFn7USART1)`, `furi_hal_bus_enable()`,
`LL_RCC_SetUSARTClockSource()`) ebenfalls von Hand — nur die Baudrate wird hinterher zusätzlich
über die offizielle `furi_hal_serial_set_br()` gesetzt. Geprüft, ob das technisch nötig ist, weil
die offizielle `furi_hal_serial_init()` die geforderten Framing-Optionen nicht abdecken würde:
Gegenteil ist der Fall — `furi_hal_serial_init()` übernimmt laut eigenem Header-Kommentar
bereits "Configures GPIO, configures and enables transceiver", und die ebenfalls öffentliche
`furi_hal_serial_configure_framing(handle, data_bits, parity, stop_bits)` deckt mit
`FuriHalSerialDataBits{6,7,8,9}`, `FuriHalSerialParity{None,Even,Odd}` und
`FuriHalSerialStopBits{0_5,1,1_5,2}` exakt die Obermenge der von dieser App angebotenen
UI-Optionen (7/8/9 Bit, None/Even/Odd, 0,5/1/1,5/2 Stopbits) ab — keine der App-Einstellungen
läge außerhalb dessen, was die offizielle API ohnehin unterstützt. `Serial_Begin()` dupliziert
damit vollständig unnötig eine bereits vorhandene, öffentlich dokumentierte Firmware-Funktion.
Für einen Port wäre das der naheliegendste Verbesserungspunkt: `Serial_Begin()` durch
`furi_hal_serial_init(handle, baud)` + `furi_hal_serial_configure_framing(...)` ersetzen — kein
STM32-LL-Header, kein manuelles GPIO/Takt-Setup mehr nötig. Risiko der aktuellen Fassung: die
App hängt an einem exakten, nirgends offiziell garantierten Registrier-/Pin-Detail
(Alternate-Function 7 auf denselben zwei Erweiterungsport-Pins, Standard-Taktquelle), das die
Firmware über die öffentliche API kapselt und bei Bedarf ändern könnte, ohne dass ein App, der
korrekt nur die öffentliche API nutzt, davon betroffen wäre — diese App aber schon, lautlos,
ohne Kompilierfehler (reines STM32WB-Registerverhalten zur Laufzeit).

**Zweiter, kleinerer Fund:** `modbus_app_free()` (`Modbus.c`) ruft `expansion_enable(app->
expansion)` erst NACH `furi_record_close(RECORD_EXPANSION)` auf — umgekehrte Reihenfolge zum
eigenen Start-Code (`expansion_disable()` klar vor der ersten Serial-Nutzung) und zur bei UART
Sniff/GPIO Logic Analyzer bereits als korrekt bestätigten Konvention (öffnen+disable vor
Gebrauch, enable+schließen danach, in umgekehrter Reihenfolge). `furi_record_close()` gibt beim
aktuellen Furi-Record-Modell nur die eigene Referenz frei, das Expansion-Service-Singleton bleibt
für die Lebensdauer des Systems bestehen — nach aktuellem `mntm-012`-Stand daher vermutlich ohne
sichtbare Laufzeitauswirkung, aber ein falsch herum aufgerufenes API-Paar, das bei einer
strengeren künftigen Record-Implementierung (z. B. Freigabe beim letzten Schließen) zum
Zugriff auf einen bereits freigegebenen Zeiger würde. Für einen Port: Reihenfolge einfach
vertauschen (`expansion_enable()` vor `furi_record_close(RECORD_EXPANSION)`).

## Quelltext-Review (Speichersicherheit)
Restlicher Quellbestand jetzt vollständig gelesen: `modbus_parser/modbus_parser.c`/`.h`,
`modbus_sender/modbus_sender.c`/`.h`, `modbus_storage/modbus_storage.c`/`.h`,
`modbus_ring_buffer/modbus_ring_buffer.c`/`.h` sowie alle sieben Szenen
(`main_scene.c`, `settings_scene.c`, `sniffer_scene.c`, `sender_scene.c`,
`manual_sender_scene.c`, `byte_input_scene.c`, `msgs_buffer_scene.c`) und
`scenes_config/app_scene_functions.c`/`.h`. Dabei zwei weitere, konkrete Funde gemacht.

**Dritter Fund (Speicherkorruption, der schwerwiegendste der drei):**
`modbus_ring_buffer.c::writeRingBuffer()` schreibt bei jedem Aufruf (= jede vollständig
geparste Modbus-Nachricht, `modbus_parser.c::ModbusParser()` ruft dies für jede
Master-Anfrage auf) das Ende-Offset der Nachricht nach `rb->delimiters[rb->delimiterIdx]`
und erhöht `delimiterIdx` danach um 1 — zurückgesetzt wird `delimiterIdx` aber nur, wenn
`writeIdx` über 255 wrapt (`Modbus.h`: `delimiters[32]`, `ringBuffer[255]`), nicht wenn
`delimiterIdx` selbst die Array-Größe 32 erreicht. Bei kurzen Nachrichten (Standard-RTU-
Antworten sind oft nur 4–8 Byte) sind nach 32 Nachrichten häufig erst 128–256 von 255 Byte
belegt — `writeIdx` hat also noch nicht gewrapt, `delimiterIdx` läuft trotzdem unkontrolliert
über 31 hinaus. `delimiters[32]` liegt im Struct-Layout direkt vor `ringBuffer[0]` (siehe
`Modbus.h`, `delimiters` und `ringBuffer` sind benachbarte Felder desselben Structs) — jede
weitere Nachricht ab der 33. schreibt damit ein Bookkeeping-Byte (den aktuellen `writeIdx`)
in den Speicherbereich, der eigentlich die *Nutzdaten bereits erfasster früherer Nachrichten*
enthält, und überschreibt sie so still. Der Konsum dieses Felds ist ebenso unbegrenzt:
`scenes/msgs_buffer_scene.c::BuildCMDList()` iteriert `delimiterIdx` beim Aufbau der "Buffer
Sender"-Liste ohne Prüfung gegen 32 (nur gegen `i < 255`), und `OnItemEnterCB()` liest beim
Auswählen eines Eintrags erneut ungeprüft `rb->delimiters[index]`/`[index - 1]` — betrifft
also direkt den einzigen UI-Pfad, über den ein gesniffter Frame erneut angezeigt oder per
"Buffer Sender" erneut gesendet werden kann. Mit Host-Repro (Struct und
`writeRingBuffer()` 1:1 aus `modbus_ring_buffer.c`/`Modbus.h` transkribiert, die
Rekonstruktionslogik aus `OnItemEnterCB()` als reine Index-/Kopierfunktion nachgebaut) unter
UBSan verifiziert: UBSan meldet den Zugriff `rb->delimiters[32]` beim 33. Aufruf explizit als
"index 32 out of bounds for type 'uint8_t [32]'"; nach 40 simulierten 4-Byte-Nachrichten ist
`writeIdx=160` (kein Wrap, < 255) und `delimiterIdx=40` (> 31), und das erneute Rekonstruieren
von Nachricht #0 über dieselbe Logik wie `OnItemEnterCB()` liefert `83 87 8B 8F` statt der
tatsächlich erfassten `40 40 40 40` — die ersten vier Bytes des Ringpuffers (Nachricht #0)
wurden durch die `delimiters[32..35]`-Überläufe der Nachrichten 33–36 mit `writeIdx`-Werten
überschrieben. Für ein Sniff-/Pentest-Werkzeug, dessen Kernzweck die unverfälschte Erfassung
von Busverkehr ist, ist stille Korruption bereits erfasster Frames nach mehr als 32 kurzen
Nachrichten in einer Sitzung ein Kernfunktions-Fund, kein Rand­fall — Repro in
`ideas/repro-rs485-modbus-ring-buffer-delimiters-overflow/repro.c`
(`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`, keine ASan-Meldung, da die
Schreibzugriffe innerhalb der `malloc(sizeof(RingBuffer))`-Allokation bleiben — die Korruption
ist eine Struct-interne Feldüberschreibung, kein Heap-Overflow über die Allokation hinaus,
UBSan erkennt trotzdem den index-out-of-bounds-Zugriff auf `delimiters[32]` selbst). Für einen
Port: `delimiters[]`/`delimiterIdx` müsste selbst zirkulär behandelt werden (z. B.
`delimiterIdx % 32` beim Schreiben und beim Lesen in `BuildCMDList()`/`OnItemEnterCB()`, mit
der Konsequenz, dass bei mehr als 32 gepufferten Nachrichten alte Einträge aus der Liste
fallen statt fremde Nutzdaten zu überschreiben) oder das Array auf eine realistischere Anzahl
vergrößert werden.

**Vierter Fund (NULL-Dereferenzierung, kleiner):** `modbus_storage.c::open_log_file_stream()`
ruft `strcpy(app->logFilePath, sequential_file_resolve_path(...))` auf und prüft erst *danach*
`if(app->logFilePath != NULL)` — der bereits vorhandene NULL-Check ist damit toter Code, denn
`sequential_file_resolve_path()` liefert laut eigener Definition `NULL` zurück (`snprintf()`
schlägt fehl, oder — der praktisch wahrscheinlichere Fall — `strdup()` liefert bei
Speicherknappheit `NULL`), bevor der Aufrufer das je sehen könnte: `strcpy()` mit einem
NULL-Quellzeiger ist bereits der Absturz. `app->logFilePath` selbst ist beim App-Start korrekt
mit `malloc(100)` allokiert (`Modbus.c::modbus_app_alloc()`) — das Ziel ist also gültig, nur
die Quelle kann NULL sein. Für einen Port: das Ergebnis von
`sequential_file_resolve_path()` vor dem `strcpy()` prüfen, nicht danach.

`on_rx_cb()` (`modbus_uart.c`) begrenzt jede `furi_hal_serial_dma_rx()`-Kopie weiterhin korrekt
auf `FURI_HAL_SERIAL_DMA_BUFFER_SIZE` und schleift bei größerem `size` sauber in mehreren
Teilkopien (bereits in der vorherigen Sitzung geprüft). `modbus_parser.c::pduParser()` liest
zwar mehrfach ungeprüft aus dem vom UART gelieferten `buf` anhand des empfangenen
Funktionscodes, tut das aber innerhalb desselben `RX_BUF_SIZE + 1`(256)-Byte-Puffers, den
`handle_rx_data_cb()` ohnehin vorher per `buf[len] = '\0'` referenziert — kein eigenständiger,
über diesen bereits vom Aufrufer garantierten Rahmen hinausgehender Fund gemacht (keine
CRC-/Längen-abhängige Fallunterscheidung dort, die den Puffer selbst vergrößern könnte).

## Aufwand
Mittel für die Software: eine Datei (`modbus_uart.c`) müsste für den Serial-Init-Fund umgebaut
werden (STM32-LL-Header/manuelles GPIO-Setup raus, zwei offizielle Aufrufe rein), der
Reihenfolge-Fund in `Modbus.c` ist eine Ein-Zeilen-Verschiebung, der NULL-Check-Fund in
`modbus_storage.c` ebenfalls. Der `delimiters[32]`-Überlauf-Fund (`modbus_ring_buffer.c` +
`msgs_buffer_scene.c`) ist der aufwändigste der vier: er betrifft die zentrale
Datenstruktur des Sniffers/der "Buffer Sender"-Funktion an mehreren Stellen konsistent und
sollte bei einem Port nicht isoliert, sondern zusammen mit allen Lese-/Schreibzugriffen auf
`delimiters[]` gefixt werden. Zusätzlich Beschaffung des Electronic-Cats-RS485-Modbus-
Hardwaremoduls nötig (Fremdhardware, wie beim eigenen CAN-Bus-Fund).

## Sicherheitsrahmen
Aktiv (Master-Senden inkl. Write-Funktionscodes) — passt in den autorisierten Rahmen aus
CONSTRAINTS Punkt 5 nur gegen eigene Modbus-Slaves oder im Rahmen ausdrücklich autorisierter
Assessments (z. B. Segmentierungstest einer eigenen Gebäudeleittechnik/SPS-Testumgebung). Kein
Massenangriffs- oder DoS-Potenzial über das hinaus, was Schreibfunktionscodes gegen ein
Zielgerät ohnehin immer mitbringen — vor Einsatz wie jedes aktive Feldbus-Werkzeug nur an
freigegebenen Systemen verwenden, da Write-Kommandos auf echten Aktoren (Ventile, Relais)
unmittelbar physisch wirksam werden können.

## Offen
- Lokale Bauprobe (`ufbt`) sowie Beschaffung des Electronic-Cats-RS485-Modbus-Addons.
- Quelltext jetzt vollständig gelesen (alle Verzeichnisse inkl. Szenen) — keine weiteren
  ungeprüften Dateien in diesem Repo offen.
- Bei tatsächlichem Port, in absteigender Priorität: (1) `delimiters[32]`-Überlauf in
  `modbus_ring_buffer.c`/`msgs_buffer_scene.c` beheben (echte Datenkorruption bereits erfasster
  Frames, Kernfunktion betroffen), (2) `Serial_Begin()`-Ersatz durch `furi_hal_serial_init()` +
  `furi_hal_serial_configure_framing()`, (3) NULL-Check vor statt nach `strcpy()` in
  `open_log_file_stream()`, (4) `expansion_enable()`/`furi_record_close()`-Reihenfolge in
  `modbus_app_free()`.
