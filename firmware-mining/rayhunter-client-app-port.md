# Rayhunter Client — externe App aus RogueMaster-Bündel (IMSI-Catcher-Detektor)

## Was
Ich habe den seit der SubGHz-Jammer-Detector-Sitzung offenen Commit `b8fe671` ("Add 6 new
security tools: uart_sniff, subghz_jammer, ble_scanner, evil_ble, rogue_ap_detector,
rayhunter_client") aus dem RogueMaster-Bündel weiterverfolgt. Davon habe ich `subghz_jammer`
(= mein bereits geprüfter SubGHz Jammer Detector) und `rogue_ap_detector` bereits abgearbeitet;
als nächsten Kandidaten aus derselben Commit-Reihe habe ich **Rayhunter Client** geprüft
(`applications/external/rayhunter_client/`, `fap_author` „PentestSuite / barkandbite", Quelle
laut `application.fam`/`fap_weburl` `barkandbite/flipper_suite` — dieselbe Quelle wie SubGHz
Jammer Detector und Rogue AP Detector).

Die App zeigt den Bedrohungsstatus des **EFF-Rayhunter**-IMSI-Catcher-Detektors
(`github.com/EFForg/rayhunter`) auf dem Flipper-Bildschirm an. Rayhunter selbst läuft auf einem
Orbic-RC400L-Mobilfunk-Hotspot und überwacht dort das Zellbasisband auf Stingray-/IMSI-Catcher-
Indikatoren (Null-Cipher-Aushandlung, Identitätsabfragen, verdächtige Zellwechsel). Der Flipper
verbindet sich per UART (115200 Baud, Erweiterungsport) mit einem ESP32-Board, das als
WiFi-Bridge fungiert: es sendet `rayhunter_poll`-Kommandos entgegen, ruft dafür per HTTP GET
`/api/analysis-report/live` beim Rayhunter-Daemon ab und reicht die NDJSON-Antwort zeilenweise
zurück. Die App parst daraus vier Bedrohungsstufen (CLEAN/LOW/MEDIUM/HIGH, Keyword-basiert),
zeigt Status/Paket-/Warnzähler an und löst bei HIGH eine Vibration + rotes LED-Blinken aus.

Codebasis ist klein und sauber getrennt (`rayhunter.c` GUI/Szenen/Timer, `rayhunter_worker.c`
NDJSON-Parser/Klassifikation, `rayhunter_uart.c` UART-Abstraktion) — beim Quelltext-Review keine
Speicherzugriffsfehler gefunden: alle String-Kopien laufen über `strncpy`/`snprintf` mit
expliziter Zielgrößenangabe und expliziter Nullterminierung, die Teardown-Reihenfolge in
`rh_app_free()` ist absichtlich so gewählt (UART-Worker-Thread zuerst joinen, danach erst die
Views freigeben), dass kein Use-after-free durch eine noch laufende Callback-Zeile entstehen
kann — im Quellcode selbst per Kommentar begründet.

## Nutzen
- Dritte **defensive Erkennungskategorie** neben SubGHz Jammer Detector (Funk-Dauerträger) und
  Rogue AP Detector (WiFi Evil Twin) — hier für Mobilfunk/Zellularnetz. Deckt damit alle drei
  Funkschichten ab, die bei einem autorisierten Assessment typischerweise relevant sind
  (SubGHz/WiFi/Mobilfunk), mit derselben ESP32-Hardware-Basis wie die beiden anderen Detektoren.
- Direkter Eigennutzen bei autorisierten Assessments: Verifikation, ob im Testumfeld ein
  IMSI-Catcher aktiv ist, sowie allgemeines Mobilfunk-Lagebild vor/während eines Einsatzes.
  Ergänzt eine Lücke, die weder SubGHz Jammer Detector noch Rogue AP Detector abdecken
  (Mobilfunk-Basisband, nicht ISM-Band/WiFi).
- Reine Anzeige-/Auswerte-App ohne eigenen Sendebetrieb — sendet nur die UART-Poll-Kommandos an
  das eigene ESP32-Board, keine Interaktion mit fremden Mobilfunknetzen selbst (das übernimmt der
  separat laufende Rayhunter-Daemon auf dem eigenen Orbic-Hotspot).
- Nutzt exakt dieselbe UART-Erweiterungsport-API wie mein bereits verifizierter Rogue AP
  Detector — kein zusätzliches API-Risiko gegenüber einem bereits geprüften Muster.

## Betroffene Dateien (lokal)
Neu, unverändert aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/rayhunter_client/application.fam`
- `applications_user/rayhunter_client/rayhunter.c` / `.h`
- `applications_user/rayhunter_client/rayhunter_worker.c` / `.h`
- `applications_user/rayhunter_client/rayhunter_uart.c` / `.h`
- `applications_user/rayhunter_client/README.md`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung).

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen meinen eigenen `mntm-012`-Header-Stand verifiziert (Momentum-
  Firmware-Tag lokal sparse ausgecheckt, `applications/services/expansion/expansion.h`,
  `targets/f7/furi_hal/furi_hal_serial.h`, `furi_hal_serial_control.h`). Alle verwendeten
  Signaturen 1:1 vorhanden: `expansion_enable()`/`expansion_disable()`,
  `furi_hal_serial_control_acquire()`/`_release()` mit `FuriHalSerialIdUsart`,
  `furi_hal_serial_init/deinit`, `_async_rx_start/_stop`, `_async_rx`, `_tx`,
  `_tx_wait_complete` — identisches API-Muster wie bei meinem bereits verifizierten Rogue AP
  Detector (`ideas/rogue-ap-detector-h10301-writer-api-status.md`). GUI-Seite nutzt ausschließlich
  Standardheader (`gui/view_dispatcher.h`, `gui/modules/variable_item_list.h`, `widget.h`,
  `notification/notification_messages.h`), kein `requires`-Eintrag im `application.fam` über das
  Standard-GUI hinaus. Lokale Bauprobe bleibt sinnvoll, um den Header-Stand byte-genau am
  Compiler zu bestätigen (diese Prüfung erfolgte per Quelltext-Diff, nicht per Build).
- **Hardwarekette ist die eigentliche Einstiegshürde, nicht die Software**: die App selbst ist
  nur das Anzeigeglied einer vierteiligen Kette (Flipper → ESP32-WiFi-Bridge-Sketch → WLAN →
  Orbic-RC400L mit laufendem Rayhunter-Daemon). Ohne alle vier Glieder zeigt die App dauerhaft
  „Waiting for RH bridge..." — kein Softwarefehler, sondern erwartetes Verhalten laut eigenem
  README. Vor lokalem Test prüfen, ob der ESP32-Bridge-Sketch aus derselben Quelle
  (`barkandbite/flipper_suite`) vorliegt oder selbst geschrieben werden muss (im hier geprüften
  Quellbaum nur die Flipper-Client-Seite enthalten, kein ESP32-Sketch mitgeliefert).
- **Keyword-basierte Klassifikation** (README selbst benennt dies als Grenze): die Bedrohungsstufe
  wird per Teilstring-Suche in der NDJSON-Zeile ermittelt (`"High"`, `"Medium"`, `"Low"`,
  `"Informational"`, `clean`), nicht per strukturiertem JSON-Parsing. Ein Rayhunter-Daemon-Update,
  das die Feldbezeichnungen ändert, würde hier stumm auf „Waiting..."/CLEAN zurückfallen statt
  einen Parse-Fehler zu melden — beim lokalen Test gegen die tatsächlich installierte
  Rayhunter-Version gegenprüfen.
- Einstellungen (Host/Port/Poll-Intervall) werden laut eigenem README nicht persistiert und
  fallen bei jedem Neustart auf die Standardwerte (`192.168.1.1:8080`, 5 s) zurück — reine
  Komforteinschränkung, kein Sicherheits- oder Speicherproblem.
- Sicherheitsrahmen: rein passiv/defensiv (Anzeige eines fremden Erkennungsergebnisses, kein
  eigener Sendebetrieb, keine Mobilfunk-Interaktion durch die Flipper-App selbst). Passt
  uneingeschränkt in den autorisierten Rahmen aus CONSTRAINTS Punkt 5.

## Port-Skizze
1. App-Ordner `rayhunter_client` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/rayhunter_client/`) nach `applications_user/rayhunter_client/` in
   meiner Firmware kopieren.
2. `ufbt launch_app APPSRC=applications_user/rayhunter_client` bzw. `fbt launch_app
   APPSRC=applications_user/rayhunter_client` — deckt etwaige Header-Abweichungen als
   Compile-Fehler auf.
3. Vor dem Funktionstest die fehlende ESP32-Bridge-Seite klären: entweder einen vorhandenen
   `barkandbite/flipper_suite`-ESP32-Sketch für diese Rolle identifizieren/beziehen, oder einen
   minimalen eigenen Bridge-Sketch schreiben (UART-Kommando `rayhunter_poll` entgegennehmen,
   `GET http://<host>:<port>/api/analysis-report/live` gegen den Orbic-Hotspot ausführen,
   NDJSON-Antwort zeilenweise über UART zurückreichen).
4. Hardwarekette aufbauen: ESP32-Board per UART an den Flipper-Erweiterungsport, WLAN-Verbindung
   ESP32 → Orbic-RC400L (mit installiertem/laufendem `EFForg/rayhunter`-Daemon).
5. Funktionstest:
   - Verbindungsindikator (gefüllter Punkt) erscheint, sobald die erste gültige Zeile mit
     `RH:`/`analysis_state`/`rayhunter`-Marker eintrifft.
   - Mit bekannten CLEAN-Bedingungen (kein Angriff) beginnen, danach — falls im eigenen
     Testlabor verfügbar — mit einem kontrollierten Testszenario (z. B. dokumentierte
     Rayhunter-Testvektoren/Demo-Modus, falls vom EFF-Projekt bereitgestellt) die Stufen
     LOW/MEDIUM/HIGH gegenprüfen; bei HIGH Vibration + rotes Blinken verifizieren.
   - Settings-Menü (Host/Port/Poll-Intervall) durchgehen, Footer-Anzeige gegen die aktive
     Auswahl verifizieren.
6. Bei erfolgreichem Test: zusammen mit Rogue AP Detector und SubGHz Jammer Detector als dritte
   defensive Erkennungs-App führen — alle drei teilen sich dieselbe ESP32-Hardwarebasis.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/rayhunter_client/` (Teil von Commit `b8fe671`, „Add 6 new security
  tools").
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `barkandbite/flipper_suite` (dieselbe
  Quelle wie SubGHz Jammer Detector und Rogue AP Detector).
- Externe Referenz für den eigentlichen Detektor: `EFForg/rayhunter` (EFF, Orbic-RC400L-IMSI-
  Catcher-Erkennung), von der Flipper-App nur als Statusanzeige konsumiert.
- Aus derselben Commit-Reihe (`b8fe671`) weiterhin offen: `uart_sniff`, `ble_scanner`,
  `evil_ble` — für eine künftige Sitzung vorgemerkt.
