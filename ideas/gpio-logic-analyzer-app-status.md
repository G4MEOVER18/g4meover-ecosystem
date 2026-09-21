# GPIO Logic Analyzer — API-87-Status

Neue Ideen-Recherche (kein Rückstellpunkt aus einer früheren Sitzung): gezielt nach einer
Ergänzung für die Kategorie "GPIO / Serial Bus" gesucht, die über den bereits vorhandenen
UART Sniff hinausgeht — ein protokollunabhängiges Werkzeug für unbekannte digitale Signale auf
Platinen.

## Quelle
- Repo: `g3gg0/flipper-logic_analyzer` (GPL-3.0, ursprünglich von g3gg0.de; die README auf dem
  `master`-Branch enthält bereits einen Hinweis auf laufende Kompatibilitätsarbeit für aktuelle
  Flipper-Firmwares, u. a. über einen aktiven Fork `ecopsychologer/flipper-logic-analyzer`
  mit 19 eigenen Commits). Vier Quelldateien: `logic_analyzer_app.c/.h`, `sump.c/.h`,
  `usb_uart.c/.h`, plus `application.fam` (appid `logic_analyzer`, `fap_category` „GPIO").
- Discord-Diskussionsfaden des Originalautors verlinkt in der README (2023er Ursprung), seither
  von Drittem weitergepflegt.

## Was
Macht den Flipper zu einem einfachen 8-Kanal-Logikanalysator, der sich über eine zweite
USB-CDC-Schnittstelle (`vcp_ch = 1`, parallel zur normalen CLI) als "Openbench Logic Sniffer"
(SUMP-Protokoll) ausgibt und direkt in PulseView/sigrok geöffnet werden kann
(`pulseview -d ols:conn=/dev/ttyACM1`). Erfasst werden die acht Erweiterungsport-Pins
C0/C1/C3/B2/B3/A4/A6/A7 über einen simplen Software-Thread, der `GPIOA/B/C->IDR` liest und bei
jeder Pegeländerung ein Sample in einen Ringpuffer (bis 16384 Einträge) schreibt; ein Tastendruck
(OK) löst manuell aus, sonst ein per SUMP-Kommando gesetzter Trigger-Bitmaske-Vergleich.

## Nutzen für mich
Ergänzt UART Sniff (protokollspezifisch, aber nur UART) um ein protokollunabhängiges
Werkzeug: unbekannte digitale Signale auf einer zu prüfenden Platine (z. B. ein nicht
dokumentierter Bus, eine vermutete SPI-/I2C-Leitung, ein Reset-/Boot-Strapping-Pin während des
Einschaltens) lassen sich vor dem gezielten Einsatz eines protokollspezifischen Tools erst grob
in PulseView sichten und dekodieren (sigrok bringt SPI-/I2C-/UART-Protokoll-Decoder mit). Direkter
Eigennutzen bei autorisierten Hardware-Assessments als Vorstufe zu Wiegand Reader/CAN-Bus-Funden
und dem UART Sniff — deckt den Fall ab, in dem das Protokoll auf der Leitung vorab noch gar nicht
bekannt ist. Reine Ergänzung, keine funktionale Überschneidung mit bereits katalogisierten Apps.

## API-87-Status
✅ Kompatibel, geprüft. Quelltext direkt per `curl` gegen meinen eigenen `mntm-012`-Header-Stand
(Momentum-Firmware-Tag, frisch sparse ausgecheckt) verifiziert: alle acht referenzierten
Erweiterungsport-Pins (`gpio_ext_pc0/pc1/pc3/pb2/pb3/pa4/pa6/pa7`) existieren unverändert in
`targets/f7/furi_hal/furi_hal_resources.h`. Die USB-CDC-Zweitkanal-Technik in `usb_uart.c`
entspricht exakt dem bereits für UART Sniff (`ideas/uart-sniff-api-status.md`) und für den
eingebauten USB-UART-Bridge-Systemdienst verwendeten Muster: `furi_hal_usb_set_config()` mit
`usb_cdc_single`/`usb_cdc_dual` (definiert in `applications/services/cli/cli_vcp.c`, bei mir
unverändert vorhanden), `furi_hal_cdc_set_callbacks()` (`furi_hal_usb_cdc.h`, identische
Signatur), `furi_hal_usb_unlock()` (`furi_hal_usb.c`, vorhanden) sowie `FuriApiLock`
(`lib/toolbox/api_lock.h`, unverändert `typedef FuriEventFlag* FuriApiLock`). Die eigentliche
Signalerfassung liest nur die Roh-STM32-Register `GPIOA->IDR`/`GPIOB->IDR`/`GPIOC->IDR` direkt —
kein Furi-HAL-Aufruf, damit unabhängig von der Firmware-API-Version. Kein `requires`-Eintrag im
`application.fam` über CLI/GUI hinaus.

## Quelltext-Review (Speichersicherheit) und gefundene Einschränkung
Kein Speicherzugriffsfehler gefunden (feste Puffergrößen, `snprintf` in der Anzeige, Ringpuffer-
Indizierung bleibt innerhalb von `sump->read_count`, das selbst durch das SUMP-Kommando auf
höchstens `MAX_SAMPLE_MEM` begrenzt wird). Beim Gegenlesen von `sump.c` gegen die tatsächliche
Erfassungsschleife in `logic_analyzer_app.c` aber eine Diskrepanz gefunden, die für den PulseView-
Einsatz relevant ist: `sump_handle_get_metadata()` meldet PulseView unbedingt eine maximale
Abtastrate von `max_sample_rate = 10000000` (10 MHz) — die eigentliche Erfassung
(`capture_thread_worker()`) tastet aber gar nicht mit fester Rate ab, sondern läuft als reine
Software-Schleife, die bei jeder erkannten Pegeländerung genau ein Sample schreibt (asynchrones
Ereignis-Sampling statt synchronem Zeitraster) — genau das bestätigt auch die eigene README
("fixed sampling rate not supported (yet?)", "maximum capture rate unclear […] guess in the
100kHz range"). PulseView interpretiert SUMP-Traces aber grundsätzlich als gleichmäßig im
Zeitraster abgetastet und zeigt sie entsprechend der vom Gerät gemeldeten festen Rate an — bei
diesem Aufbau bedeutet das: die in PulseView angezeigten Zeitabstände zwischen Flanken sind nicht
verlässlich, nur die Flankenreihenfolge selbst. Für Bit-Timing-kritische Auswertung (exakte
Baudrate/Taktfrequenz aus der Aufnahme ablesen) ungeeignet; für reine Vorab-Sichtung
("welches Muster liegt überhaupt an, wie viele Leitungen wechseln wann relativ zueinander") und
die manuelle Trigger-Auslösung per Tastendruck weiterhin brauchbar. Kein Fix-Vorschlag hierfür
angelegt — läge in der Firmware-Mining-Kategorie (dieses Repos eigentliche Firmware, nicht die
hier nur recherchierte Drittanwendung), und die README dokumentiert die Einschränkung bereits
selbst ehrlich.

## Aufwand
Gering für die Software (vier Dateien, reine externe `.fap`, keine Firmware-Änderung, keine
Zusatzhardware — nutzt ausschließlich den vorhandenen Erweiterungsport plus einen zweiten
virtuellen COM-Port über dasselbe USB-Kabel). Für den lokalen Einsatz zusätzlich PulseView/sigrok
auf dem PC nötig (kostenlos, plattformübergreifend).

## Sicherheitsrahmen
Rein passiv im Sinn der eigenen Signalerfassung (liest nur Pegel, sendet selbst nichts auf die
beobachteten Leitungen) — passt in den autorisierten Rahmen aus CONSTRAINTS Punkt 5. Wie bei
jedem passiven Sniffer gilt: nur an eigenen oder ausdrücklich für den Test freigegebenen Platinen/
Bussen einsetzen. Der zweite virtuelle COM-Port ersetzt während des Betriebs die normale CLI
(`vcp_ch = 1` reserviert den Zweitkanal) — beim Portieren prüfen, ob das mit eigenen
CLI-Automatisierungen (z. B. Batch-Emulator-Skripten) kollidiert.

## Offen
- Lokale Bauprobe (`ufbt launch_app APPSRC=logic_analyzer` bzw. Klon des Repos) gegen die eigene
  `mntm-012`-Basis, danach Funktionstest mit einem bekannten Signal (z. B. eigener UART- oder
  I2C-Leitung mit bekanntem Muster) zur Verifikation der Flankenerkennung in PulseView.
- Die oben dokumentierte Abtastraten-Diskrepanz beim eigenen Testlauf im Hinterkopf behalten —
  Zeitachse in PulseView nicht für exakte Frequenzmessung verwenden.
- Verwandter, noch nicht recherchierter Kandidat aus derselben Suche: `NaejEL/flipperzero-i2ctools`
  (dedizierter I2C-Scanner/Sniffer/Sender, GPL-3.0, 115 Stars) — enger fokussiert als dieser
  generische Logikanalysator, für eine künftige Sitzung vorgemerkt.
