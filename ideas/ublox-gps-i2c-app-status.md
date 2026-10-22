# ublox (u-blox GPS über I2C) — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer Ergänzung für die
Kategorie "Recon / GPS / Wardriving" gesucht — ein zweiter, I2C-basierter GPS-Anbindungsweg als
Alternative zur bereits katalogisierten seriellen NMEA-Anbindung (`GPS (NMEA) + WigleCSV`).

## Quelle
- `liamhays/ublox`, `main`-Branch, Commit `bdb4e19` (2025-08-02), GPL-3.0, `fap_version 0.5`.
  2890 Zeilen über 20 Quelldateien (Kern: `ublox.c`/`ublox_device.c`/`ublox_worker.c`,
  Views/Scenes/Helfer für KML-/GPX-Logging). 27 Sterne, aktiv gepflegt (letzter Commit zum
  Recherchezeitpunkt keine zwei Monate alt).
- Getestete Hardware laut README: SparkFun SAM-M8Q, u-blox NEO-M9N, SparkFun MAX-M10S — alle
  über UBX-Protokollversion 15 angesprochen (deckt laut Autor 8er-, vermutlich auch 9er-/
  10er-Serie ab).

## Was
Liest ein per I2C (Erweiterungsport-Pins 3V3/GND/C0=SCL/C1=SDA, z. B. über ein Qwiic-Kabel)
angeschlossenes u-blox-GPS-Modul aus und zeigt die Daten in zwei Modi an: ein "Handheld"-Modus
(Fix-Typ, Satellitenzahl, Kilometerzähler, Breite/Länge/Höhe, Kurs, Flipper-Uhrzeit, Akkustand)
und ein "Car"-Modus (große Geschwindigkeits-/Kurs-/Kilometerzähler-Anzeige). Zusätzlich: KML-
oder GPX-Track-Logging auf die SD-Karte (nur bei echtem Fix, inkl. Koppelnavigation), sowie ein
Uhrzeit-Abgleich (Flipper-Minute/-Sekunde aus GPS-Zeit, Stunde bleibt für die lokale Zeitzone
erhalten).

## Nutzen für mich
Bislang deckt "Recon / GPS / Wardriving" nur die serielle NMEA-Anbindung (`GPS (NMEA) +
WigleCSV`, UART) sowie die reine Auswertungsseite `Nearby Files` ab. Diese App ist ein
eigenständiger, I2C-basierter Zweitpfad — nützlich, wenn der UART-Erweiterungsport bereits durch
ein anderes Modul (z. B. das eigene 868-FSK-`ukfe_rf`-Board) belegt ist, da I2C und UART
getrennte Erweiterungsport-Pins nutzen und beide Anbindungen parallel am Gerät verkabelt sein
könnten. Zusätzlich liefert sie zwei Funktionen, die die bisherige NMEA-Anbindung nicht bietet:
GPX-Export (neben KML) und einen Uhrzeit-Abgleich direkt vom GPS-Modul.

## API-87-Status
✅ läuft unverändert — reine `furi_hal_i2c_*`/`furi_hal_rtc_*`/`FuriString`/`FuriThread`-API,
alle genutzten Signaturen gegen einen frischen, blobfreien Sparse-Checkout von
`G4MEOVER18/G4MEOVER-FW` (`main`) wörtlich abgeglichen:
- `furi_hal_i2c_acquire()`/`_release()`/`_tx()`/`_rx()`/`_trx()`/`_is_device_ready()` sowie
  `furi_hal_i2c_handle_external` exakt wie in
  `targets/furi_hal_include/furi_hal_i2c.h`/`targets/f7/furi_hal/furi_hal_i2c_config.h`
  deklariert — dasselbe bereits mehrfach verifizierte Muster wie bei i2c Tools/RS485 Modbus/
  SPI Flash Dump.
- `furi_hal_rtc_get_datetime()`/`_set_datetime()` (Uhrzeit-Abgleich), `furi_hal_power_get_pct()`
  (Akkuanzeige), `FuriString`-API, `furi_thread_alloc_ex()`/`_free()`/`_start()`/`_join()`/
  `_get_state()` — durchweg aktuelle, unveränderte Symbole.
- Keine Erweiterungsport-Konflikt-Frage wie bei einigen UART-/SPI-Apps: I2C-Pins (C0/C1) werden
  von meinem eigenen Ökosystem derzeit durch kein anderes Standardmodul belegt.

## Quelltext-Review und Befunde

### Fund 1 (Hauptfund): CFG-PMS/CFG-ODO/CFG-NAV5-Konfigurationsschreibzugriffe wirkungslos
`ublox_worker_init_gps()` (in `ublox_worker.c`) konfiguriert das GPS-Modul beim Start in drei
Lese-Modifizieren-Schreiben-Zyklen (Energiesparmodus "Balanced", Kilometerzähler aktivieren,
dynamisches Plattformmodell setzen). Für alle drei gilt dasselbe Muster: nach dem Lesen der
aktuellen Konfiguration wird
```c
pms_frame_tx.payload = pms_message_rx->message;   // ebenso: odo_frame_tx/nav5_frame_tx
```
gesetzt — also der **rohe, vollständige UBX-Rahmen** (6-Byte-Header `sync1/sync2/class/id/
len_lo/len_hi` gefolgt vom eigentlichen Payload und der Prüfsumme), nicht der eigentliche
Payload-Bereich. Die app-eigene, korrekte Parse-Funktion `ublox_bytes_to_frame()` (in
`ublox_device.c`) zeigt selbst, dass der echte Payload erst bei Byte-Offset 6 beginnt
(`for(i = 6; i < 6 + frame->len; i++) frame->payload[i - 6] = message->message[i];`) — diese
korrekte Funktion wird in `ublox_worker_init_gps()` für den Schreibpfad aber nicht verwendet,
stattdessen wird der rohe Lese-Antwortpuffer direkt wiederverwendet.

Die anschließenden Feldänderungen (`odo_frame_tx.payload[4] |= 1;` für "useODO aktivieren",
`nav5_frame_tx.payload[0] = 1; nav5_frame_tx.payload[2] = platform_model;` für den
Plattformmodus) landen dadurch im 6-Byte-Header statt im echten Payload, und die
anschließende `ublox_frame_to_bytes()`-Kodierung des Schreibbefehls kopiert dieselben
falsch-versetzten Bytes in die an das GPS gesendete Nachricht. Für CFG-PMS wird die
Zielposition zufällig über eine direkte Absolut-Indizierung (`pms_message_rx->message[6 + 1]
= 0x01;`, **nicht** über `payload[]`) getroffen statt über den (falschen) Payload-Zeiger — das
Schreiben selbst trifft zwar `message[7]`, aber der anschließende Re-Encode-Schritt
(`pms_frame_tx.len = 8; pms_frame_tx.payload = pms_message_rx->message;`) kopiert trotzdem
`message[0..7]` statt `message[6..13]` als ausgehenden Payload — das gesetzte Byte landet so an
Payload-Position 7 (laut u-blox-Interface-Beschreibung "reserved") statt an Position 1
("powerSetupValue"), und die übrigen sieben echten Konfigurationsbytes werden komplett durch
Header-Bytes der Lese-Antwort ersetzt.

**Protokollunabhängiger Beweis (kein Wissen über die genaue u-blox-Feldbedeutung nötig):** Mit
acht eindeutigen Markerbytes als simulierte "aktuelle CFG-PMS-Konfiguration" durchlaufe ich im
Host-Repro wortgetreu den PMS-Codepfad gegen die echte, unverändert mitkompilierte
`ublox_frame_to_bytes()`/`ublox_i2c_transfer()`. Tatsächlich gesendeter Payload (ALT):
`b5 62 06 86 08 00 90 01` — das sind Sync-Byte/Sync-Byte/Klasse/ID/Länge-niedrig/erstes echtes
Markerbyte/das modifizierte Byte, **nicht** die beabsichtigten Originalwerte mit
`powerSetupValue = 0x01` an Position 1 (`90 01 92 93 94 95 96 97`). Sechs der acht
ursprünglichen Konfigurationsbytes (Periode, On-Zeit, reservierte Felder) erreichen das GPS-
Modul dabei überhaupt nicht — ersetzt durch Rahmen-Header-Bytes. Dieselbe Herleitung (Payload-
Zeiger ohne `+ 6`, danach relative `payload[N]`-Indizierung) gilt strukturell identisch für
CFG-ODO (Kilometerzähler-Aktivierung/-Modus) und CFG-NAV5 (Plattformmodell) — dort ist der
Effekt sogar eindeutiger, da beide ausschließlich relativ über `payload[]` schreiben (keine
zufällig richtige Absolut-Indizierung wie bei PMS).

**Auswirkung:** Alle drei "Apply my setting"-Schreibzugriffe in `ublox_worker_init_gps()`
setzen nicht die beabsichtigten Felder auf dem GPS-Modul — Energiesparmodus, Kilometerzähler-
Aktivierung/-Betriebsart und dynamisches Plattformmodell bleiben vermutlich auf
Werkseinstellung, ohne dass die App das erkennt (jeder Schreibzugriff bekommt vom Modul trotzdem
ein syntaktisch gültiges ACK, da die Prüfsumme über die — falschen — gesendeten Bytes korrekt
berechnet wird). Reiner Funktionsfehler, kein Speichersicherheitsrisiko (keine Pufferüber-/
-unterläufe, da alle drei Nachrichten exakt so groß bleiben wie vom Protokoll vorgesehen).

### Fund 2 (Nebenfund, dieselbe Datei): zwei TX-Nachrichten nie freigegeben
In derselben Datei werden sechs Sende-Nachrichten per `ublox_frame_to_bytes()` erzeugt; vier
davon werden korrekt mit `ublox_message_free()` freigegeben (PVT-Abfrage, ODO-Abfrage, sowie
PMS-/ODO-Lese-Anfragen). Zwei nicht:
- `ublox_worker_sync_to_gps_time()`: `message_tx` (NAV-TIMEUTC-Abfrage, 8 Byte) wird nach dem
  `ublox_i2c_transfer()`-Aufruf nie freigegeben — leckt bei jedem einzelnen manuellen
  "Sync Time"-Aufruf.
- `ublox_worker_init_gps()`, CFG-NAV5-Schreibbefehl: die zweite `nav5_message_tx`-Allokation
  (44 Byte, das eigentliche Schreibkommando) wird weder im Erfolgs- noch im ACK-Fehlerpfad
  freigegeben — leckt bei jedem Init-Versuch, einschließlich der Wiederholungsschleife in
  `ublox_worker_read_nav_messages()`, die bei fehlendem/getrenntem GPS-Modul alle 500 ms einen
  neuen Init-Versuch startet.
- Zusätzlich fällt auf, dass die CFG-PMS-/CFG-ODO-Schreibbefehle (`pms_message_tx`/
  `odo_message_tx`) zwar im Erfolgspfad korrekt freigegeben werden, aber **nicht**, falls der
  nachfolgende ACK-Transfer fehlschlägt (`if(ack == NULL) { ...; return false; }`) — ein
  selteneres, aber reales Leck im selben Muster.

**Host-Repro (LeakSanitizer):** 50 simulierte Zyklen je Fundstelle, ALT vs. NEU in getrennten
Prozessen, gegen die echte, unverändert mitkompilierte `ublox_device.c`. Unter
`gcc -std=c17 -Wall -Wextra -fsanitize=address,leak,undefined -fno-sanitize-recover=undefined`
bei `-O0`: ALT 4176 Byte über 198 Allokationen geleckt (bei `-O1`: 4200 Byte/200 Allokationen,
identisches Bild, nur andere Sanitizer-interne Buchführung), NEU 0 Byte bei beiden
Optimierungsstufen.

## Host-Repro
`ideas/repro-ublox-cfg-payload-offset-and-tx-leak/repro.c` — kompiliert zusammen mit der echten,
unverändert aus dem Quell-Repository kopierten `ublox_device.c` (nicht neu implementiert),
gegen minimale Mocks für `furi_hal_i2c_*`/`FURI_LOG_*` (`mocks/furi.h`, `mocks/furi_hal.h`).
Enthält beide Funde: den protokollunabhängigen CFG-PMS-Payload-Beweis (per `assert()` hart
verifiziert, kein Sanitizer nötig) sowie die 50-Zyklen-Leck-Gegenprobe für Fund 2 (Lean-
Sanitizer entscheidet beim Prozessende). Bau laut Kommentar im Dateikopf:
```
gcc -std=c17 -Wall -Wextra -fsanitize=address,leak,undefined -fno-sanitize-recover=undefined \
    -Imocks -O0 repro.c ublox_device.c -o repro_o0 && ./repro_o0 alt   # bzw. fix
```
Bei `-O0` und `-O1` jeweils mit `alt` und `fix` ausgeführt — Ergebnis wie oben beschrieben,
identisch bei beiden Optimierungsstufen.

## Fix-Skizze
Additiv, neun Zeilenänderungen in `ublox_worker.c`, Patch in
`ideas/ublox-cfg-payload-offset-and-tx-leak-fix/ublox_worker_c.patch`:
- Fund 1: `pms_frame_tx.payload = pms_message_rx->message;` → `... = pms_message_rx->message +
  6;` (ebenso für `odo_frame_tx`/`nav5_frame_tx`) — die bereits vorhandenen relativen
  `payload[N]`-Zugriffe (ODO/NAV5) zeigen dadurch automatisch auf die richtigen Bytes, ohne dass
  diese Zeilen selbst geändert werden müssen. Die bereits korrekte Absolut-Indizierung bei PMS
  (`message[6 + 1]`) bleibt unverändert gültig.
- Fund 2: `ublox_message_free(message_tx);` direkt nach dem `ublox_i2c_transfer()`-Aufruf in
  `ublox_worker_sync_to_gps_time()` ergänzt; für PMS/ODO/NAV5 den jeweils vorhandenen
  `ublox_message_free(<x>_message_tx);`-Aufruf vom Ende des Erfolgspfads direkt hinter den
  ACK-Transfer-Aufruf vorgezogen (deckt damit zusätzlich den bislang unbehandelten
  ACK-Fehlerpfad ab, ohne eine neue Codezeile für diesen Fall zu benötigen).
- Patch gegen zwei frische, unabhängige Zweitkopien von `liamhays/ublox` (`main`, Commit
  `bdb4e19`) mit `git apply --check` verifiziert, real angewendet, beide Endstände per `md5sum`
  byte-für-byte identisch (`77e4169df93c68061daa632295e255d2`).

## Aufwand
Gering. Reine Software-Fixes ohne Firmware-Änderung (kein `mntm-012`-Patch nötig, läuft bereits
unverändert). Für den lokalen Testlauf wird zusätzlich ein u-blox-GPS-Modul (getestet laut
README: SAM-M8Q/NEO-M9N/MAX-M10S) über ein Qwiic- oder gleichwertiges I2C-Kabel benötigt.

## Sicherheitsrahmen
Reines Mess-/Recon-Werkzeug (GPS-Positionsanzeige/-Logging für eigene Fahrten/Standorte), kein
Angriffswerkzeug — deckungsgleich mit der bereits katalogisierten `GPS (NMEA) + WigleCSV`-
Kategorie. Beide Funde sind reine Code-Qualitäts-/Funktionsfehler (falsche Konfiguration des
eigenen GPS-Moduls bzw. Speicherverbrauch auf dem eigenen Gerät), keine Risiken gegenüber
Dritten.

## Offen
- Lokale Bauprobe (`ufbt`) nach Anwendung des Patches, danach Testlauf gegen ein eigenes
  u-blox-Modul: insbesondere prüfen, ob sich nach dem Fix ein messbar geändertes
  Energiesparverhalten bzw. eine tatsächlich aktive Kilometerzähler-Funktion zeigt (vorher laut
  Fund 1 vermutlich wirkungslos).
- Fix für CFG-ODO/CFG-NAV5 ist strukturell (nicht per eigenem Host-Repro mit echten
  u-blox-Feldwerten) hergeleitet — am tatsächlichen Modul gegen die u-blox-Interface-
  Beschreibung (M8/M9/M10) verifizieren, dass die nach Fix gesendeten Bytes exakt den
  dokumentierten Feldoffsets entsprechen.
- Neue Katalog-Zeile unter "Recon / GPS / Wardriving" ergänzt.
