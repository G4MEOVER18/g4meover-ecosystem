# Unitemp (Umweltsensor-Multireader) — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer bislang komplett
fehlenden Kategorie im GPIO-/Erweiterungsport-Bereich gesucht — Umweltsensoren (Temperatur/
Feuchte/Druck/CO₂) als Gegenstück zu den bisherigen reinen Bus-/Debug-Werkzeugen (GPIO Logic
Analyzer, i2c Tools, UART Sniff, RS485 Modbus, SPI Flash Dump, SWD Probe), die alle Busse/Chips
analysieren, aber keine konkreten Sensordaten dekodieren.

## Quelle
- `quen0n/unitemp-flipperzero`, `master`-Branch, Commit `ae62710` (03.06.2026), GPL-3.0,
  `fap_version 2.1`. 10936 Zeilen über 70 Quelldateien (Kern: `sensors.c`/`unitemp.c`, vier
  Interface-Abstraktionen `onewire_sensor.c`/`singlewire_sensor.c`/`i2c_sensor.c`/
  `spi_sensor.c`, 23 Sensortreiber unter `sensors/`, Szenen/Views fürs Menü). 353 Sterne,
  46 Forks, aktiv gepflegt (letzter Commit zum Recherchezeitpunkt unter fünf Monate alt).
- Fap-Kategorie laut `application.fam`: `GPIO` — passt zur bestehenden Katalog-Kategorie
  "GPIO / Serial Bus".

## Was
Liest bis zu 23 verschiedene Umweltsensor-Modelle über vier Hardware-Schnittstellen aus und
zeigt Temperatur, relative Feuchte, Luftdruck und CO₂-Konzentration (plus abgeleiteten
Hitzeindex/Taupunkt und eine Luftqualitätsbewertung) direkt am Gerät an:
- **Single Wire** (DHT11/12/20/21/22, AM2320 im SW-Modus): VCC Pin 9 (3V3), GND Pin 8/11/18,
  Data auf einem beliebigen freien Digitalpin.
- **1-Wire** (DS18B20/DS18S20/DS1822): gleiche Strom-/Masseverdrahtung, Data bevorzugt auf
  Pin 17 (1W), mehrere Sensoren parallel am selben Bus möglich (eigene ROM-Enumeration).
- **I²C** (AHT10/20, SHT2x/3x/4x, SHTC3, HTU21x(F), Si7021, HDC1080/2080, BMx280, BME680, LM75,
  MAX31725, TMP102, SCD30/40/41): SDA Pin 15 (C1), SCL Pin 16 (C2).
- **SPI** (MAX6675/MAX31855 Thermoelement-Verstärker): MOSI Pin 2 (A7), MISO Pin 3 (A6),
  SCK Pin 5 (B3), CS frei wählbar.
Sensoren werden über ein eigenes Menü angelegt, benannt (bis zu zehn Zeichen) und dauerhaft in
`sensors.list` auf der SD-Karte gespeichert; mehrere Sensoren lassen sich gleichzeitig in einer
Übersicht überwachen (konfigurierbares Poll-Intervall je Sensormodell).

## Nutzen für mich
Füllt eine bislang komplett fehlende Kategorie im GPIO-/Erweiterungsport-Bereich: alle
bisherigen Funde dort (GPIO Logic Analyzer, i2c Tools, UART Sniff, RS485 Modbus, SPI Flash Dump,
SWD Probe) sind Bus-/Protokoll-Werkzeuge, keines davon dekodiert tatsächliche Sensordaten.
Konkreter Nutzen bei einer autorisierten physischen Begehung: schnelle, werkzeuglose
Umgebungsdiagnose vor Ort (z. B. Temperatur/Feuchte in einem Serverraum oder Tresorumfeld
protokollieren, oder einen fremden Umweltsensor, der an eine Alarm-/Zugangskontrolllogik
gekoppelt ist, mit bekannten Referenzwerten gegenprüfen), sowie als eigenständiges,
batteriebetriebenes Mess-/Logging-Werkzeug für eigene Hardwareprojekte (z. B. Gehäuseklima
meiner eigenen ukfe-Boards). Ergänzt damit die bestehende GPIO-/Serial-Bus-Sammlung um eine
reine Sensordaten-Kategorie statt einer weiteren Bus-Analyse.

## API-87-Status
✅ läuft unverändert — alle genutzten HAL-Funktionen gegen einen frischen, blobfreien
Sparse-Checkout von `G4MEOVER18/G4MEOVER-FW` (`main`, Commit `b359f6f`, derselbe bereits
mehrfach verifizierte Stand) wörtlich abgeglichen:
- `furi_hal_gpio_init()`/`_write()`/`_read()` exakt wie in `targets/f7/furi_hal/furi_hal_gpio.h`
  deklariert (Single-Wire-/1-Wire-Bit-Banging).
- `furi_hal_spi_bus_handle_init()`/`_acquire()`/`_bus_rx()`/`_release()` exakt wie in
  `targets/furi_hal_include/furi_hal_spi.h` deklariert (MAX6675/MAX31855).
- I²C-Zugriff läuft über die eigene `i2c_sensor.c`-Abstraktion auf denselben
  `furi_hal_i2c_*`-Funktionen, die bereits bei `ublox`/i2c Tools/RS485 Modbus verifiziert wurden
  — keine weitere Einzelprüfung nötig, kein Firmware-Patch erforderlich.
- Keine Erweiterungsport-Konfliktfrage: GPIO-/1-Wire-Betrieb nutzt einen frei wählbaren
  Digitalpin, I²C/SPI nutzen die Standard-Erweiterungsport-Pins ohne Berührung mit einem
  bereits durch ein eigenes Standardmodul belegten Port.

## Quelltext-Review und Fund

### Fund: `unitemp_sensors_load()` — ungebrenzter `%s`-`sscanf()` in einen 11-Byte-Stack-Puffer
`sensors.c::unitemp_sensors_load()` liest die Sensorliste zeilenweise aus der eigenen
Konfigurationsdatei `sensors.list` (`APP_DATA_PATH`, normalerweise
`/ext/apps_data/unitemp/sensors.list`, eine gewöhnliche Textdatei auf der SD-Karte) und zerlegt
jede Zeile mit
```c
char name[11] = {0};
char model[11] = {0};
...
sscanf(line_cstr, "%s %s %d %n", name, model, &temp_offset, &str_offset);
```
Das `%s`-Format in `sscanf()` hat **keine Feldbreitenbegrenzung** — es schreibt so viele Zeichen
wie bis zum nächsten Leerzeichen/Whitespace/Dateiende vorkommen, unabhängig von der tatsächlichen
Puffergröße. Enthält das erste Leerzeichen-getrennte Feld der Zeile (der "Name") mehr als zehn
Zeichen, überschreibt `sscanf()` den 11-Byte-Stack-Puffer `name` — ein klassischer
Stack-Buffer-Overflow (CWE-121), ausschließlich durch den Inhalt einer Textdatei ausgelöst, ohne
jeden Flipper-/Hardwarezugriff.

Die unmittelbar folgende Prüfung
```c
if(sensor_model != NULL && sizeof(name) > 0 && sizeof(name) <= 11) {
```
verhindert das nicht: `sizeof(name)` ist bei einem als `char name[11]` deklarierten Array eine
**Compile-Zeit-Konstante** (immer `11`), unabhängig davon, wie viele Zeichen tatsächlich
eingelesen wurden — der Ausdruck ist bei jedem Programmlauf wahr. Die Prüfung sieht wie ein
beabsichtigter Längen-Check aus (vermutlich mit `strlen(name) <= 10` verwechselt), ist aber ein
wirkungsloses No-Op, und selbst wenn sie wirksam wäre, käme sie ohnehin erst **nach** dem schon
abgelaufenen `sscanf()`-Aufruf.

Erreichbarkeit: Über die eigene Tastatur-Eingabe (`text_input`, 11-Byte-Puffer inkl.
Nullterminator fest vorgegeben, siehe `unitemp_scene_sensor_edit_name.c`) ist ein zu langer Name
nicht erzeugbar — dieser Pfad ist sicher. Die Datei `sensors.list` ist aber eine gewöhnliche,
von Hand editierbare Textdatei auf der SD-Karte; zusätzlich liest dieselbe Funktion beim ersten
Start nach einem Versionswechsel automatisch eine Alt-Format-Datei
(`/ext/unitemp/sensors.cfg`, Migrationspfad) über denselben ungebrenzten Parser ein. Ein zu
langer erster Eintrag in beiden Dateien — etwa durch manuelles Bearbeiten, einen fehlerhaften
externen Sync-/Batch-Schritt beim Vorbefüllen mehrerer Sensoren, oder eine beschädigte Zeile —
löst den Überlauf ohne jede Nutzerinteraktion beim App-Start aus.

### Fix (additiv, ein Zeichen)
```c
sscanf(line_cstr, "%10s %10s %d %n", name, model, &temp_offset, &str_offset);
```
Feldbreite `10` für beide `%s`-Konvertierungen (Platz für den Nullterminator im 11-Byte-Puffer
bleibt dabei automatisch erhalten) — exakt das von `scanf`/`sscanf` selbst vorgesehene Mittel
gegen genau diese Klasse von Fehlern, keine Strukturänderung. Ein zu langer Name wird dadurch
nicht mehr abgeschnitten *in den Stack*, sondern einfach bei 10 Zeichen gekappt; die
nachfolgenden Felder (`model`/`temp_offset`) verschieben sich dann zwar (Eingabe bereits
fehlerhaft/manipuliert), aber ohne Speicherkorruption — dieselbe "lieber sauber kaputt als
unsauber abstürzen"-Eigenschaft wie bei den bereits dokumentierten Feldbreitenfunden in anderen
Apps dieses Katalogs.
Patch (`unitemp-sensor-name-overflow-fix/sensors_c.patch`) gegen zwei frische, unabhängige
Zweitkopien von `quen0n/unitemp-flipperzero` (`master`, Commit `ae62710`) mit `git apply --check`
verifiziert, real angewendet, per `md5sum` byte-für-byte abgeglichen — identisch.

### Host-Repro
`repro-unitemp-sensor-name-overflow/repro.c`: entnimmt die reale Pufferdeklaration
(`char name[11]`/`char model[11]`) und den realen `sscanf()`-Aufruf 1:1 aus `sensors.c`
(nur `furi_string_get_cstr()` durch einen gewöhnlichen `const char*` ersetzt — für die
Pufferfrage ohne Bedeutung). Zwei Fälle:
- Fall 1 (Name ≤10 Zeichen, wie über die eigene Tastatur-Eingabe erzeugbar): ALT und NEU
  verhalten sich identisch, kein Fund.
- Fall 2 (Name == 11 Zeichen, wie aus einer manuell editierten/beschädigten `sensors.list`):
  ALT löst unter AddressSanitizer bei `-O0` **und** `-O1` einen
  `stack-buffer-overflow`-Abbruch exakt in der `sscanf()`-Zeile aus; NEU (Feldbreite `%10s`)
  bleibt bei beiden Optimierungsstufen innerhalb des Puffers (Name wird sauber auf 10 Zeichen
  gekappt statt den Stack zu überschreiben).
`gcc -g -O0/-O1 -fsanitize=address,undefined`: **ALT bricht in Fall 2 ab (stack-buffer-overflow),
NEU besteht beide Fälle.**

## Testhinweis (lokal)
Patch gegen die eigene `unitemp-flipperzero`-Kopie anwenden, bauen/flashen. Über das Menü einen
Sensor mit einem zehn Zeichen langen Namen anlegen und speichern — Verhalten bleibt unverändert
(Regressionsfall). Danach `sensors.list` auf der SD-Karte von Hand öffnen und die erste Zeile auf
einen über zehn Zeichen langen ersten Eintrag verlängern (z. B. `sensor_lang DS18B20 0 17`), App
neu starten: vor dem Fix ist das Verhalten durch die Stack-Überschreibung undefiniert
(bestenfalls ein fehlerhaft übernommener Name/Absturz, siehe Host-Repro); nach dem Fix wird der
Name auf zehn Zeichen gekappt, kein Absturz.

## Sicherheitsrahmen
Rein defensiver Fund an einem eigenen Werkzeug für eigene/autorisierte Hardware — kein
Fremdangriff, keine Umgehung einer Schutzfunktion, passt zum eigenen defensiven/autorisierten
Rahmen.

## Risiko
Gering: additive Einzeiler-Änderung an einer bereits vorhandenen Format-Zeichenkette, keine
Struktur-/Verhaltensänderung für den regulären (App-UI-erzeugten) Eingabepfad.

## Nächster Schritt
Lokal bauen/flashen und wie oben beschrieben verifizieren; danach ggf. Upstream-PR gegen
`quen0n/unitemp-flipperzero` erwägen (eigenständiger Fund, nicht an eigene Firmware gebunden).
