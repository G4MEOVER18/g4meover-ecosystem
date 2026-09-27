# RS485 Modbus (Electronic Cats) — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer Ergänzung für die
bislang nur drei Einträge (UART Sniff, GPIO Logic Analyzer, i2c Tools) enthaltende Kategorie
„GPIO / Serial Bus" gesucht — bei mir bereits zwei CAN-Bus-Funde
(`firmware-mining/canbus-attack-app-port.md`, `firmware-mining/canbus-electroniccats-app-port.md`)
vom selben Hersteller (Electronic Cats) dokumentiert, dessen RS485-Modbus-Addon als nächster
naheliegender Kandidat aus derselben Produktreihe geprüft.

## Quelle
- Repo: `ElectronicCats/flipper-rs485modbus` (MIT-Lizenz, 29 Stars, 63 Commits auf `main`).
  Quelltext direkt per `curl` gegen `raw.githubusercontent.com` gelesen (`ModbusApp/Modbus.c/.h`,
  `modbus_uart/modbus_uart.c/.h`, `application.fam`) — nicht nur Zusammenfassung.
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
Kein eigenständiger Speicherfehler in den geprüften Teilen (`modbus_uart.c` vollständig,
`Modbus.c` App-Lebenszyklus/Callbacks) gefunden. `on_rx_cb()` begrenzt jede
`furi_hal_serial_dma_rx()`-Kopie korrekt auf `FURI_HAL_SERIAL_DMA_BUFFER_SIZE` und schleift bei
größerem `size` sauber in mehreren Teilkopien. Parser/Sender/Storage/Ring-Buffer (~restliche
Verzeichnisse) nicht mehr im Detail gelesen — für einen Port vor dem Bauen vollständig
durchzusehen.

## Aufwand
Mittel für die Software: eine Datei (`modbus_uart.c`) müsste für den Serial-Init-Fund umgebaut
werden (STM32-LL-Header/manuelles GPIO-Setup raus, zwei offizielle Aufrufe rein), der
Reihenfolge-Fund in `Modbus.c` ist eine Ein-Zeilen-Verschiebung. Zusätzlich Beschaffung des
Electronic-Cats-RS485-Modbus-Hardwaremoduls nötig (Fremdhardware, wie beim eigenen CAN-Bus-Fund).
Restlicher Quellbestand (`modbus_parser`, `modbus_sender`, `modbus_storage`, `modbus_ring_buffer`,
Szenen) für einen tatsächlichen Port noch vollständig zu lesen.

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
- Vollständige Prüfung von `modbus_parser`/`modbus_sender`/`modbus_storage`/
  `modbus_ring_buffer`/Szenen (bislang nur `modbus_uart` und der App-Lebenszyklus in `Modbus.c`
  geprüft).
- Bei tatsächlichem Port: `Serial_Begin()`-Ersatz durch `furi_hal_serial_init()` +
  `furi_hal_serial_configure_framing()` sowie die `expansion_enable()`/`furi_record_close()`-
  Reihenfolge in `modbus_app_free()` als erste zwei Verbesserungen einbringen.
