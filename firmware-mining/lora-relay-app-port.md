# LoRa Relay (ElectronicCats/flipper-SX1262-LoRa) — Port-Vorschlag + Fund

## Was

Neue Katalog-Kategorie "LoRa" gezielt gesucht — bislang fehlt im eigenen Pentest-Werkzeugkatalog
jede LoRa-eigene App, obwohl `g4meover-deck` bereits einen `lorix_link.py`-Pfad für LoRa-Hardware
auf der Pi-Seite führt. **LoRa Relay** (App-ID `lora_relay`, über das RogueMaster-Bündel
referenziert, Original-Quelle `ElectronicCats/flipper-SX1262-LoRa`, Repo-HEAD-Commit `430db6c`)
gefunden: ein LoRa-Sniffer/-Sender für ein eigenes SX1262-Erweiterungsmodul
("Electronic Cats Flipper Add-On: Sub-GHz"), derselbe Hersteller wie die bereits portierten
`RS485 Modbus`/`CANBUS`-Funde. Quellbestand überschaubar (zwei Kerndateien,
`applications_user/lora_app/lora.c` 1064 Zeilen SX1262-Treiber + `lora_relay.c` 2732 Zeilen
App-Logik, dazu ein vendortes `text_input`-Eingabemodul und ein winziges `validators.c`),
vollständig gelesen.

**Funktionsumfang laut README**: LoRa-Parameter (Frequenz, Bandbreite, Spreading Factor, Coding
Rate, Syncword) konfigurieren, LoRaWAN-US915/EU868-Kanalmenüs, empfangene Pakete live anzeigen
und per USB-CDC-Zweitkanal als binäres Frame weiterreichen (Relay-Funktion, daher der Name),
Sniff-Sitzungen als JSON-Zeilen auf die SD-Karte loggen, Pakete aus einer Log-Datei erneut
senden (Replay).

## Nutzen für mich

Füllt eine komplett fehlende Kategorie — LoRa/LoRaWAN-Verkehr passiv mitschneiden und für
spätere Analyse/Replay auf die SD-Karte loggen, direkt mit demselben Erweiterungsport-Muster wie
die bereits dokumentierten GPIO/Serial-Bus-Funde. Ergänzt `g4meover-deck`s `lorix_link.py`
(Pi-seitige LoRa-Anbindung) um ein eigenständiges Flipper-Gegenstück ohne Pi-Abhängigkeit.
Zusätzlich ein konkreter, eigenständig verwertbarer Speicherfehler-Fund (siehe unten) mit
direkter Remote-Auslösbarkeit — jeder autorisierte Testsender in Funkreichweite kann die
"Recording"-Funktion der App zum Absturz bringen, ganz ohne Pairing/Authentifizierung, weil LoRa
selbst keine Verbindungsaufnahme kennt.

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag-Commit `b359f6f`,
`mntm-012`) geprüft:

- Erweiterungsport-Pins: `gpio_ext_pa4`, `gpio_ext_pc0`, `gpio_ext_pc1`, `gpio_ext_pc3` sowie die
  Zweckentfremdung dreier interner Pins (`gpio_swclk` als LED-Beacon, `gpio_usart_tx`/
  `gpio_usart_rx` als `pin_ant_sw`/`pin_busy`) — alle byteidentisch in den eigenen
  `furi_hal_resources.h`/`furi_hal_gpio.h` vorhanden. `gpio_button_back` (für den
  Zurück-Button-Handler) ebenfalls vorhanden.
- `furi_hal_spi_bus_handle_external` (Bus/MISO/MOSI/SCK-Felder, `cs` wird von der App selbst auf
  `pin_nss1` umgebogen) — byteidentisches Muster zu den bereits portierten SPI-Erweiterungsport-
  Apps (SPI Flash Dump, CAN Transceiver, i2c Tools).
- `furi_hal_usb_set_config()`/`furi_hal_usb_get_config()` mit `usb_cdc_dual`/`usb_cdc_single`
  sowie `furi_hal_cdc_send()` (Zweikanal-USB-CDC für den Relay-Ausgabepfad) — identisches Muster
  zu GPIO Logic Analyzer (SUMP-Protokoll über zweiten CDC-Kanal), Symbole byteidentisch vorhanden.
  `CDC_PORT_NUM` ist ein App-eigenes Makro (`#define CDC_PORT_NUM 1`), keine
  Firmware-Abhängigkeit.
- Standard-GUI-/`ViewDispatcher`-/`VariableItemList`-/Storage-/`DialogsFileBrowserOptions`-APIs
  sowie `furi_hal_rtc_get_datetime()` — byteidentisch, keine Abweichung.
- Eigenes, vendortes `text_input`-Modul (`modules/text_input.c`) für die Frequenz-Eingabe —
  unabhängig von Firmware-Core-Änderungen, baut nur auf Standard-`Gui`/`ViewPort`-APIs auf.

Kein Port-Hindernis: reines externes FAP (`applications/external/lora_relay/` im
RogueMaster-Bündel bzw. eigener `applications_user/lora_app/`-Ordner), keine
Firmware-Core-Änderung nötig.

## Fund: `final_string[400]` — `snprintf()`-Größenangabe (666) größer als der tatsächliche Puffer

`lora_view_sniffer_draw_callback()` (`lora_relay.c`) ist der periodische Draw-Callback der
Sniffer-Ansicht. Bei jedem erfolgreich empfangenen LoRa-Paket (`lora_receive_async()`,
`receiveBuff[255]`) wird das Paket zunächst als Hex-String in `asciiBuff[512]` umgewandelt
(`bytesToAsciiHex()`, bis zu 510 Hex-Zeichen für ein volles 255-Byte-Paket). Ist die
"Recording"-Funktion aktiv (`model->flag_file`, per OK-Taste umschaltbar), baut die Funktion
anschließend eine JSON-Logzeile:

```c
char final_string[400];
...
snprintf(
    final_string,
    666,                                   // <-- Literal, nicht sizeof(final_string) (== 400)
    "{\"date\":\"%s\", \"time\":\"%s\", \"frequency\":\"%s\", \"bw\":\"%s\", \"sf\":\"%s\", \"RSSI\":\"%d\", \"payload\":\"%s\"}",
    date_string, time_string, freq_str, config_bw_names[...], config_sf_names[...],
    getRSSI(), asciiBuff);
```

`snprintf()` selbst hält sich exakt an die übergebene Größe (666) — es ist also kein
`snprintf()`-Bug, sondern ein falscher Größenparameter: `final_string` ist tatsächlich nur 400
Byte groß, `snprintf()` darf laut diesem Aufruf aber bis zu 665 Nutzzeichen plus Nullterminator
schreiben. Der dominante variable Anteil des Formatstrings ist `asciiBuff` — die Hex-Darstellung
eines vollständig fremdbestimmten LoRa-Pakets, das einfach nur in Funkreichweite empfangen
wurde, ganz ohne Pairing, Authentifizierung oder Protokoll-Handshake (LoRa kennt keinen
Verbindungsaufbau). Mit den übrigen Feldern auf realistischen Werten (Datum/Zeit/Frequenz/BW/
SF/RSSI) reicht bereits ein Paket von rund 132+ Byte Nutzlast, um die reale 400-Byte-Grenze zu
überschreiten; beim maximal möglichen 255-Byte-LoRa-Paket schreibt `snprintf()` bis zu 638
Nutzzeichen plus Nullterminator — rund 239 Byte über das tatsächliche Stack-Array hinaus, in
direkt danebenliegende Stack-Variablen desselben Frames.

Alle anderen `snprintf()`-Aufrufe in derselben Datei (rund zwanzig Stellen, u. a. jedes
`text_buf[11]`-Frequenz-Label) übergeben durchgehend korrekt `sizeof(<puffer>)` — dieser eine
Aufruf ist die einzige Ausnahme, kein systematisches Muster. GCCs eigene `_FORTIFY_SOURCE`-Prüfung
erkennt den Widerspruch bereits beim Kompilieren statisch (`specified bound 666 exceeds
destination size 400`), bestätigt den Fund also unabhängig vom Host-Repro.

**Auslösebedingung**: "Recording" muss aktiv sein (ein bewusster Nutzeraktionsschritt, OK-Taste),
danach reicht jedes empfangene LoRa-Paket mit ausreichend langer Nutzlast — kein spezielles
Schadpaket nötig, nur eine Länge oberhalb der Schwelle. Für eine App, deren zentraler Zweck das
passive Mitschneiden von beliebigem, nicht vertrauenswürdigem Funkverkehr ist, ist das die
erwartete, nicht die seltene Eingabe.

## Betroffene Dateien

- `applications_user/lora_app/lora_relay.c` — `lora_view_sniffer_draw_callback()`, der
  `snprintf()`-Aufruf für `final_string`.

## Fix

Additiv, eine Zeile: Größenangabe durch `sizeof(final_string)` ersetzen (identisches Muster zu
jedem anderen `snprintf()`-Aufruf in derselben Datei). Kein Verhaltensunterschied im Normalfall
(Äquivalenztest unten) — bei einem zu langen Paket schneidet `snprintf()` die JSON-Zeile jetzt
korrekt am tatsächlichen Pufferende ab (kontrolliertes Abschneiden, keine valide JSON-Zeile mehr
für diesen einen Logeintrag, aber kein Speicherfehler) statt den Stack zu überschreiben.

Patch (`firmware-mining/lora-relay-app-port/lora_relay_sniffer_log_overflow.patch`) gegen eine
frische, unabhängige Zweitkopie von `ElectronicCats/flipper-SX1262-LoRa` (Commit `430db6c`) mit
`git apply --check` verifiziert, real angewendet, Ergebnis gegen eine zweite, unabhängig
geklonte Kopie per `diff -r` byte-für-byte abgeglichen — identisch.

## Host-Repro

`lora_view_sniffer_draw_callback()`s `final_string`-Aufbau 1:1 transkribiert (exakte Konstanten,
Formatstring, Feldreihenfolge; nur die drei Firmware-Aufrufe `furi_hal_rtc_get_datetime()`/
`getRSSI()`/`furi_string_get_cstr()` durch feste Platzhalterwerte ersetzt). Fünf Szenarien, je in
einem eigenen fork-isolierten Kindprozess (ein ASan-Abort in einem Szenario darf die übrigen
nicht verhindern):

1. Regression: 20-Byte-Paket, ORIGINAL-Größenangabe (666) → kein Überlauf.
2. Fund an der Schwelle: 140-Byte-Paket, ORIGINAL → ASan `stack-buffer-overflow` (WRITE).
3. Fund worst case: 255-Byte-Paket (maximale LoRa-Nutzlast), ORIGINAL → ASan
   `stack-buffer-overflow`, WRITE of size 639 exakt am `final_string`-Ende.
4. Fix: identisches 255-Byte-Paket, FIX (`sizeof`) → kein Überlauf.
5. Äquivalenztest: 20-Byte-Paket, ORIGINAL vs. FIX → byteidentischer JSON-String (Fix ändert im
   Normalfall nichts am Ergebnis).

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: alle fünf Szenarien bestanden (Fund-Szenarien lösen wie erwartet aus, Fix- und
Regressionsszenarien bleiben sauber), identischer Output beider Optimierungsstufen, keine
Compiler-Warnungen — in `firmware-mining/repro-lora-relay-app-port/repro.c`.

## Testhinweis (lokal, mit SX1262-LoRa-Erweiterungsmodul)

1. `lora_relay` (mit Patch) gegen die eigene `mntm-012`-Basis mit `ufbt`/`fbt` bauen,
   SX1262-Modul am Erweiterungsport anschließen.
2. Vor dem Patch: Sniffer-Ansicht öffnen, "Recording" per OK-Taste aktivieren, von einem zweiten,
   eigenen LoRa-Sender (z. B. ein zweites SX1262-Modul oder ein beliebiger eigener
   LoRa-Transceiver auf derselben Frequenz/denselben Modulationsparametern) ein Testpaket mit
   mindestens ~140 Byte Nutzlast senden — reproduzierbarer Absturz/undefiniertes Verhalten beim
   Schreiben der Logzeile.
3. Nach dem Patch: identischer Ablauf, dasselbe große Testpaket wird empfangen und geloggt, ohne
   Absturz (Logzeile ggf. am Pufferende abgeschnitten statt vollständig).
4. Regressionstest: ein kurzes Testpaket (< 100 Byte) vor/nach dem Patch senden — resultierende
   Logzeile muss in beiden Fällen identisch sein.

## Risiko

Niedrig. Rein additiv (ein geänderter Funktionsparameter), ändert nichts an der
Empfangs-/Sende-/Konfigurationslogik. Einziger Nebeneffekt: eine Logzeile für ein sehr langes
Paket wird jetzt am Pufferende abgeschnitten statt (unsicher) vollständig geschrieben — für ein
Logging-Werkzeug der korrekte Kompromiss gegenüber einem Speicherfehler.

## Sicherheitsrahmen

Speicherfehler (stack-buffer-overflow WRITE) in einer reinen Empfangs-/Logging-App, ausgelöst
durch beliebigen, nicht authentifizierten LoRa-Funkverkehr in Reichweite — für autorisierte
RF-Pentests/CTF-Übungen relevant (eigenes Zielsystem sendet absichtlich lange Pakete, um die
Robustheit des Sniff-Werkzeugs selbst zu prüfen), kein Angriff auf fremde Geräte oder Daten.
