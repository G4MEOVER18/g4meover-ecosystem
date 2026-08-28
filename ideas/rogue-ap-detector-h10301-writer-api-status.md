# Rogue AP Detector & H10301 Writer — API-87-Status (RogueMaster-Rückstellpunkte aufgegriffen)

Zwei getrennte, aus früheren Sitzungen vorgemerkte Rückstellpunkte in einem Durchgang geprüft:
`rogue_ap_detector` (Geschwisterfund aus der SubGHz-Jammer-Detector-Sitzung) und `h10301_writer`
(offener Punkt aus dem NFC-/RFID-Werkzeug-Cluster-Rückstand).

## Rogue AP Detector

### Quelle
- Repo: `barkandbite/flipper_suite` (MIT-Lizenz), Pfad `rogue_ap_detector/` — dieselbe Quelle wie
  der bereits geprüfte SubGHz Jammer Detector. RogueMaster-Bündel (`flipperzero-firmware-wPlugins`,
  Branch `420`) enthält denselben Stand unter `applications/external/rogue_ap_detector/`.
- `appid` `rogue_ap_detector`, `fap_author` „PentestSuite / barkandbite", `fap_category`
  „GPIO/ESP32", `fap_version` 1.0.

### Was
Evil-Twin-/Rogue-AP-Erkennung: ein externes ESP32-Devboard mit ESP32-Marauder-Firmware liefert
per UART (115200 Baud, Flipper-Erweiterungsport) laufend `scanap`-Zeilen (SSID/BSSID/RSSI/Kanal).
Der Flipper parst diese in eine Tabelle (bis 128 Einträge, 30 s Stale-Pruning) und markiert
SSIDs mit 2+ verschiedenen BSSIDs als SUSPECT, bei zusätzlich >20 dBm RSSI-Delta zwischen den
BSSIDs als EVIL TWIN (LED/Vibrationsalarm). Reine Empfangs-/Auswerte-App, sendet selbst nichts.

### Nutzen für mich
Erste **defensive WiFi-Gegenkategorie** — bisher habe ich im WiFi-Abschnitt nur offensive/
Recon-Werkzeuge (Marauder/GhostESP für Deauth/EvilPortal/Scan, Wendigo für Recon). Direkter
Eigennutzen: Verifikation der eigenen Evil-Twin-Testaufbauten bei autorisierten Assessments
(sichtbar/erwartungsgemäß?), sowie als eigenständiges WiFi-Lagebild vor Einsätzen (unautorisierte
APs im Zielumfeld). Ergänzt den bereits verifizierten SubGHz Jammer Detector zur zweiten
defensiven Erkennungskategorie neben SubGHz. Nutzt dieselbe ESP32/Marauder-Hardware, die für
Marauder/GhostESP ohnehin bereits vorgesehen ist — kein zusätzlicher Hardwarekauf.

### API-87-Status
✅ Kompatibel, geprüft. Das Repo selbst gibt in seinem README explizit „Flipper Zero running
official firmware 1.4.x (API 87.1)" als Zielplattform an. Zusätzlich am Quelltext gegen meinen
eigenen `mntm-012`-Header-Stand verifiziert (Momentum-Firmware-Tag lokal sparse ausgecheckt):
`rogue_uart.c` verwendet ausschließlich Standard-APIs, die in `mntm-012` unverändert vorhanden
sind — `expansion_enable()`/`expansion_disable()` (`applications/services/expansion/expansion.h`,
RECORD_EXPANSION), `furi_hal_serial_control_acquire()`/`_release()` mit `FuriHalSerialIdUsart`
(`targets/f7/furi_hal/furi_hal_serial_control.h`, `furi_hal_serial_types.h`), sowie
`furi_hal_serial_init/deinit`, `_async_rx_start/_stop`, `_tx`, `_tx_wait_complete`
(`furi_hal_serial.h`) — exakt dieselben Signaturen, keine Abweichung. GUI-Seite (`rogue_ap.c`)
nutzt ausschließlich `ViewDispatcher`/`Submenu`/`TextBox`/`VariableItemList`, keine
Sonderheader. Kein `requires`-Eintrag im `application.fam` über das Standard-GUI hinaus.
Lokale Bauprobe (`ufbt launch_app`) bleibt trotzdem sinnvoll, um den exakten Header-Stand
byte-genau zu bestätigen (Prüfung hier erfolgte über Quelltext-Diff, nicht per Compiler).

### Aufwand
Gering für die Software (reine externe `.fap`, keine Firmware-Änderung). Hardwareseitig wird
ein ESP32-Devboard mit geflashter Marauder-Firmware benötigt — bereits Teil meiner
WiFi-Kategorie (Marauder/GhostESP), kein separater Hardwarekauf, aber die Verkabelung/das
Board muss beim Testlauf angeschlossen sein.

### Sicherheitsrahmen
Rein defensiv (Erkennung, kein Sendebetrieb, „passive detector" laut eigenem README mit
explizitem Rechtshinweis auf CFAA/Computer Misuse Act). Passt uneingeschränkt in den
autorisierten Rahmen aus CONSTRAINTS Punkt 5. Bekannte Grenzen laut README, die ich für die
Bewertung übernehme: 2,4-GHz-only (Marauder-Limitierung, keine 5-GHz-Erkennung), harte
128-AP-Tabellengrenze ohne Verdrängung ältester Einträge in dichten Umgebungen, legitime
Multi-AP-Deployments (Enterprise/Mesh) lösen SUSPECT aus (Kanal/RSSI-Delta zur Unterscheidung
nötig), Marauder-Parser kann SSIDs mit endenden Ziffern durch eine Beacon-Byte-Strip-Heuristik
verstümmeln.

## H10301 Writer

### Quelle
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `Zacvr/h10301_writer`, MIT-Lizenz,
  `fap_version` 1.3, Autor ZacVR. RogueMaster-Bündel (Branch `420`) enthält denselben Stand
  unter `applications/external/h10301_writer/`.

### Was
Eigenständige, einzeldatei-App (`h10301_writer.c`) für sequenzielles Beschreiben von T5577-Tags
mit HID-Prox-H10301-26-Bit-Credentials (Facility Code + Card Number). Nutzt ausschließlich den
eingebauten LFRFID-Antenna-Pfad, keine Zusatzhardware. Kernfunktion: nach jedem erfolgreichen
Schreibvorgang wird die Card Number automatisch inkrementiert und zusammen mit dem Facility Code
auf die SD-Karte persistiert (`apps_data/h10301_writer/settings.dat`) — erspart das manuelle
Anlegen/Editieren einer eigenen `.rfid`-Datei pro Karte bei Serienschreibvorgängen. UI mit
blinkender Ziffern-Hervorhebung und einstellbarer Schrittweite (1/10/100/1.000/10.000) für
schnelle Werteingabe, 10-Sekunden-Schreib-/Verifizierungs-Timeout mit Erfolgs-/Fehler-Vibration.

### Nutzen für mich
Ergänzt meine bestehende Wiegand-Reader-Kategorie (Sniff/Decode/Replay, Port-Vorschlag bereits
vorliegend) um die fehlende **Schreib-/Provisionierungsseite**: bei autorisierten Zutritts-
kontroll-Assessments (z. B. Verifikation, ob ein Leser tatsächlich nur autorisierte
Facility-Codes akzeptiert, oder Aufbau eines Testkarten-Satzes für einen Kunden) muss ich sonst
für jede Testkarte manuell eine `.rfid`-Datei anlegen; diese App macht daraus einen
Zähl-/Serienworkflow direkt am Gerät. Reine Ergänzung, keine Überschneidung mit vorhandenen
Apps — die native Flipper-LFRFID-App kann zwar auch H10301 schreiben, aber nicht seriell/mit
Auto-Inkrement.

### API-87-Status
✅ Kompatibel, geprüft. Verwendet ausschließlich die generische LFRFID-Worker-/Protocol-Dict-API
(`lib/lfrfid/lfrfid_worker.h`, `toolbox/protocols/protocol_dict.h`), die ich bereits über meinen
eigenen `mntm-012`-Header-Stand (Momentum-Firmware-Tag lokal sparse ausgecheckt) verifiziert
habe: `lfrfid_worker_alloc()`, `_start_thread()`, `_write_start()` (Signatur inkl.
`LFRFIDWorkerWriteCallback`), `_stop()`, `_stop_thread()`, `_free()` sowie das
`LFRFIDWorkerWriteResult`-Enum (`LFRFIDWorkerWriteOK` u. a.) — alle Namen/Signaturen identisch
vorhanden. `LFRFIDProtocolH10301` ist im `mntm-012`-Protokoll-Enum enthalten
(`lib/lfrfid/protocols/lfrfid_protocols.h`/`.c`). Standard-`furi_hal`/GUI/Storage/Notification-
APIs im übrigen Code, kein Sonderheader. Kein `requires`-Eintrag im `application.fam` über das
Standard-GUI hinaus. Lokale Bauprobe (`ufbt launch_app`) bleibt sinnvoll zur Bestätigung am
eigenen Header-Stand, wird aber wegen der 1:1-Übereinstimmung als risikoarm eingeschätzt.

Randnotiz (kein Fund, nur Beobachtung): `save_settings()`/`load_settings()` schreiben/lesen
`sizeof(uint32_t) * 2` Byte direkt ab der `H10301WriterApp`-Struktur-Basisadresse — funktioniert
nur korrekt, weil `fc`/`cn` zufällig die ersten beiden Felder der Struktur sind. Unüblicher,
aber für diese eine App folgenloser Stil (kein Speicherzugriffsfehler, da `sizeof(uint32_t)*2`
klar innerhalb der Struktur liegt); bei eigener Portierung würde ich stattdessen explizit
`fc`/`cn` einzeln serialisieren, aber das ist keine Voraussetzung für den Port selbst.

### Aufwand
Gering — reine externe `.fap`, keine Firmware-Änderung, keine Zusatzhardware (nur handelsübliche
beschreibbare T5577-Tags, die ich für Wiegand-/LFRFID-Tests ohnehin brauche).

### Sicherheitsrahmen
Passt in den autorisierten Rahmen aus CONSTRAINTS Punkt 5 — Schreiben von HID-Prox-Credentials
auf eigene/für den Test bereitgestellte T5577-Tags im Rahmen autorisierter
Zutrittskontroll-Assessments oder Provisionierung eigener Testkarten. Wie bei jeder
LFRFID-Schreibfunktion gilt: nur auf eigene oder ausdrücklich autorisierte Tags anwenden, keine
Duplizierung fremder Zutrittsmedien ohne Freigabe.

## Offen
- Lokale Bauproben (`ufbt launch_app APPSRC=<app-ordner>`) beider Apps gegen die eigene
  `mntm-012`-Basis.
- Rogue AP Detector: Funktionstest mit angeschlossenem ESP32/Marauder-Board (Live-Scan gegen
  ein bekanntes Netz, dann testweise Evil-Twin-Aufbau mit einem zweiten AP gleicher SSID zur
  Verifikation der SUSPECT/EVIL-TWIN-Klassifikation).
- H10301 Writer: Funktionstest mit mehreren T5577-Tags — Serienschreibvorgang (3+ Tags),
  danach Rücklesen über die native Flipper-LFRFID-App zur Verifikation von Facility Code und
  fortlaufender Card Number.
- Restlicher NFC-Werkzeug-Cluster aus dem RogueMaster-Bündel-Rückstellpunkt (über `h10301_writer`
  hinaus) bleibt für künftige Sitzungen offen.
