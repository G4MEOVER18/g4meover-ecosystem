# i2c Tools — API-87-Status

Rückstellpunkt aus der GPIO-Logic-Analyzer-Sitzung aufgegriffen: dort als verwandter, noch nicht
recherchierter Kandidat aus derselben "GPIO / Serial Bus"-Suche vorgemerkt (`ideas/gpio-logic-
analyzer-app-status.md`, Abschnitt "Offen").

## Quelle
- Repo: `NaejEL/flipperzero-i2ctools` (GPL-3.0, `LICENSE`-Datei im Repo verifiziert). Frisch,
  blobfrei geklont (`GIT_LFS_SKIP_SMUDGE=1 git clone --depth 50`) und alle acht Quelldateien
  (`i2cscanner.c/.h`, `i2csniffer.c/.h`, `i2csender.c/.h`, `i2ctools.c`, `i2ctools_i.h`, fünf
  Dateien unter `views/`) vollständig gelesen.
- Aktiv gepflegt: 42 Commits auf `main`, letzter Commit `1840c30` (Merge PR #12) vom
  2025-10-25. `docs/README.md`/`docs/changelog.md` sowie der Commit "Add needed files for Flipper
  Application Catalog" bestätigen die Listung im offiziellen Flipper Application Catalog
  (aktuelle Version laut `application.fam`: `fap_version="1.1"`, Changelog nennt v1.0 "Initial
  release for Flipper Application Catalog", v1.1 "Add infos screen").

## Was
Drei Werkzeuge für den I2C-Bus über den Erweiterungsport (Verdrahtung laut README: C0→SCL,
C1→SDA, GND→GND, 3,3-V-Logik vorausgesetzt, sonst Pegelwandler/Isolator wie ISO1541 nötig):
- **Scanner**: durchläuft alle 7-Bit-Adressen (`furi_hal_i2c_is_device_ready()`) und listet
  gefundene Peripherie-Adressen.
- **Sender**: schreibt ein Byte an eine zuvor gefundene Adresse und liest bis zu zwei
  Antwort-Bytes zurück (`furi_hal_i2c_trx()`), für einfache Register-Abfragen.
- **Sniffer**: rein passiver Bus-Mitschnitt, unabhängig von der Hardware-I2C-Peripherie über
  zwei GPIO-Interrupt-Callbacks auf denselben Pins (SCL: `GpioModeInterruptRise`, SDA:
  `GpioModeInterruptRiseFall`) realisiert — der SDA-Callback erkennt Start-/Stop-Bedingungen
  (SDA-Wechsel bei High-SCL), der SCL-Callback liest bei jeder steigenden SCL-Flanke ein
  Datenbit bzw. nach acht Bits das ACK/NACK-Bit. Ergebnis: bis zu 128 aufgezeichnete Frames mit
  Byte- und ACK-Anzeige, kein zusätzliches PC-Tool nötig (anders als der SUMP-basierte GPIO
  Logic Analyzer, der PulseView voraussetzt).

## Nutzen für mich
Ergänzt den bereits kuratierten GPIO Logic Analyzer (protokollunabhängig, aber PulseView-
abhängig) um ein enger fokussiertes, eigenständig auf dem Gerät nutzbares I2C-Werkzeug — direkte
Lesbarkeit auf dem Flipper-Display ohne PC-Kopplung. Bei autorisierten Hardware-Assessments
deckt das einen häufigen Fall ab, den weder UART Sniff noch der Logic Analyzer so gezielt
abdecken: viele eingebettete Zielsysteme (Sensor-/EEPROM-/RTC-Peripherie auf Platinen,
Konfigurationsspeicher, manche Zutrittskontroll-Baugruppen) hängen an I2C statt UART/SPI. Scanner
+ Sender erlauben eine schnelle Ersteinschätzung (welche Adressen antworten, einfache
Register-Reads), der Sniffer bestätigt anschließend, was während des normalen Betriebs tatsächlich
über den Bus läuft. Reine Ergänzung, keine funktionale Überschneidung mit bereits katalogisierten
Apps.

## API-87-Status
✅ Kompatibel, geprüft. Frischer, blobloser Sparse-Checkout von `Next-Flip/Momentum-Firmware`
(Tag `mntm-012`) gegen meinen eigenen Header-Stand abgeglichen:
- I2C-HAL (`targets/furi_hal_include/furi_hal_i2c.h`): `furi_hal_i2c_acquire()`,
  `furi_hal_i2c_release()`, `furi_hal_i2c_is_device_ready()`, `furi_hal_i2c_trx()` — alle vier
  von Scanner/Sender genutzten Funktionen unverändert vorhanden, Signaturen identisch.
  `furi_hal_i2c_handle_external` (`targets/f7/furi_hal/furi_hal_i2c_config.h`) — der genutzte
  externe Bus-Handle (Erweiterungsport C0/C1, nicht der interne Power-I2C-Bus) — ebenfalls
  unverändert vorhanden.
- GPIO-HAL/-Pins (`targets/f7/furi_hal/furi_hal_gpio.h`, `furi_hal_resources.h`):
  `gpio_ext_pc0`/`gpio_ext_pc1` (vom Sniffer für dieselben SCL/SDA-Pins wie Scanner/Sender
  verwendet), `furi_hal_gpio_init()`, `furi_hal_gpio_add_int_callback()`/
  `_remove_int_callback()`, `furi_hal_gpio_read()`, sowie die genutzten Enum-Werte
  `GpioModeInterruptRise`/`GpioModeInterruptRiseFall`/`GpioModeAnalog`, `GpioPullNo`,
  `GpioSpeedHigh`/`GpioSpeedLow` — alle unverändert vorhanden.
- `application.fam`: `requires=["gui"]`, keine weiteren HAL-Abhängigkeiten über GUI/I2C/GPIO
  hinaus, `fap_category="GPIO"` (bei mir eine bereits existierende Kategorie).

## Quelltext-Review (Speichersicherheit) und gefundene Einschränkung
Scanner/Sender: keine dynamische Puffergrößen-Berechnung, feste `uint8_t`-Arrays
(`addresses[MAX_I2C_ADDR + 1]`, `recv[2]`), Schleifenindizes bleiben innerhalb ihrer festen
Grenzen — kein Fund.

Sniffer: **potenzieller Pufferüberlauf in `SCLcallback()`** (`i2csniffer.c`). Für jedes Frame
gibt es feste `data[MAX_MESSAGE_SIZE]`/`ack[MAX_MESSAGE_SIZE]`-Arrays mit `MAX_MESSAGE_SIZE =
128`; `frame_index` (welches Frame gerade befüllt wird) wird beim Start eines neuen Frames
gegen `MAX_RECORDS` geprüft (`clear_sniffer_buffers()` bei Überlauf), aber `data_index`
(welches Byte innerhalb des aktuellen Frames gerade befüllt wird) wird in `SCLcallback()` nach
jedem vollständigen Byte unbedingt hochgezählt (`i2c_sniffer->frames[frame].data_index++`),
ohne Prüfung gegen `MAX_MESSAGE_SIZE` — anders als beim Frame-Zähler gibt es hier keinen
Reset-Pfad. Eine einzelne I2C-Transaktion mit mehr als 128 übertragenen Bytes ohne
zwischenzeitliche Stop-/Start-Bedingung (z. B. ein EEPROM- oder Flash-Sequential-Read mit
großem Blockzähler, oder ein absichtlich so konstruiertes Zielgerät) lässt `data_index` über
127 hinauswachsen und schreibt dadurch außerhalb der `data[]`/`ack[]`-Arrays. Nachträglich
präzisiert (vorige Einordnung "in nachfolgende `i2cFrame`-Einträge hinein" war ungenau): `data`
und `ack` sind zwei direkt aufeinanderfolgende `uint8_t`-Arrays desselben `i2cFrame`-Structs,
zusammen exakt 256 Byte groß — da `data_index` ein `uint8_t` ist (Wertebereich 0–255), bleibt
der OOB-Zugriff auf dieses Feldpaar desselben Frame-Slots begrenzt und erreicht nie den
nächsten `frames[]`-Eintrag. Da der Callback in einem GPIO-EXTI-Interrupt-Kontext läuft, ist
der praktische Effekt eine stille Verwechslung von Datenbytes und ACK-Flags statt eines
kontrollierten Fehlers. Port-Vorschlag inkl. Fix (additive Grenzprüfung vor dem bestehenden
Zugriff, Host-Repro mit UBSan-Array-Bounds-Nachweis) jetzt nachgereicht, siehe
`firmware-mining/i2ctools-sniffer-data-index-oob-fix.md` — für den lokalen Einsatz bis zum Port
weiterhin unten unter "Sicherheitsrahmen" vermerkt.

## Aufwand
Gering. Acht kompakte Quelldateien, reine externe `.fap`, keine Firmware-Änderung, keine
Zusatzhardware über den vorhandenen Erweiterungsport hinaus (optional ein Pegelwandler/Isolator
bei Zielsystemen mit abweichendem Logikpegel, siehe README-Warnhinweis).

## Sicherheitsrahmen
Scanner/Sender sind aktiv (senden auf den Bus) — nur an eigenen oder ausdrücklich für den Test
freigegebenen I2C-Bussen einsetzen, passt in den autorisierten Rahmen aus CONSTRAINTS Punkt 5.
Sniffer ist rein passiv (liest nur Pegel). Kein Sicherheitsrisiko für das beobachtete
Zielsystem selbst (der Sniffer sendet nichts); der oben dokumentierte `data_index`-Überlauf bei
Zielsystemen mit sehr langen I2C-Transaktionen betrifft nur die eigene App-Zuverlässigkeit und
ist jetzt mit Port-Vorschlag inkl. Fix dokumentiert (siehe oben).

## Offen
- Lokale Bauprobe (Klon des Repos gegen die eigene `mntm-012`-Basis, `ufbt launch_app` bzw.
  `fbt`), danach Funktionstest an einem bekannten I2C-Gerät (z. B. eigenes EEPROM/RTC-Modul):
  Scanner findet die bekannte Adresse, Sender liest ein bekanntes Register korrekt, Sniffer
  zeigt eine kurze, bekannte Transaktion (Adresse, Byte(s), ACK) korrekt an.
  Pegelwandler/Isolator bereithalten, falls das Zielsystem nicht mit 3,3 V arbeitet.
  Den jetzt vorliegenden Sniffer-Fix (`firmware-mining/i2ctools-sniffer-data-index-oob-fix.md`)
  vor diesem Testlauf einspielen, danach denselben Funktionstest durchführen (muss laut
  Host-Repro-Äquivalenztest unverändert funktionieren) und, falls ein passendes Zielgerät
  vorliegt, zusätzlich eine Transaktion > 128 Byte ohne Stop-Bedingung mitschneiden (darf jetzt
  weder abstürzen noch die ersten 128 Byte verfälschen).
- Damit ist die aus der GPIO-Logic-Analyzer-Suche vorgemerkte "GPIO / Serial Bus"-Kandidatenliste
  abgearbeitet; keine weiteren Rückstellpunkte aus dieser Suche offen.
