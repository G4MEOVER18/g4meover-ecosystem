# Evil Portal (WiFi Captive Portal) — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer Ergänzung für die
bislang nur drei Einträge (Marauder/GhostESP-UI, Wendigo, Rogue AP Detector) enthaltende
Kategorie "WiFi" gesucht — eine eigenständige, Flipper-seitig geführte Captive-Portal-/
Credential-Harvesting-App statt eines Features innerhalb der ESP32-Marauder-Firmware selbst.

## Quelle
- `bigbrodude6119/flipper-zero-evil-portal`, `main`-Branch, Commit `d196f60`
  (2023-07-17, "removed whitespace"), MIT-Lizenz. 2,4k Sterne/286 Forks — eine der
  bekanntesten Flipper-Community-Apps dieser Art, aber seit über zwei Jahren ohne neuen Commit
  (README nennt Version `0.0.2`, eine angekündigte `0.0.3` mit Multi-Portal-Unterstützung ist
  nie erschienen) — bei der Aufwandseinschätzung unten berücksichtigt.
- Zwei Komponenten: ein Flipper-`.fap` (`flipper/flipper-evil-portal/`, sechs Quelldateien,
  729 Zeilen) für Menü/SD-Verwaltung/UART-Steuerung, sowie eine ESP32-Arduino-Firmware
  (`esp32/EvilPortal/EvilPortal.ino`) für den eigentlichen Access Point/Webserver, verbunden
  über das Erweiterungsport-UART (TX/RX gekreuzt, 3V3/GND).

## Was
Macht ein per UART angeschlossenes ESP32-S2-/ESP32-WROOM-Board zu einem offenen WLAN-Access-
Point mit gefälschter Login-Seite (captive portal). Jedes Gerät, das sich verbindet, bekommt
die unter `apps_data/evil_portal/index.html` liegende Seite ausgeliefert; eingegebene Zugangs-
daten werden über UART an den Flipper zurückgemeldet und dort auf der SD-Karte protokolliert
(`logs/log_N.txt`). Eine `portals/`-Vorlagensammlung liefert mehrere fertige Phishing-Seiten-
Varianten zum Austausch gegen `index.html`. Das Flipper-Menü bietet AP-Name setzen, Portal
starten/zurücksetzen, Logs speichern sowie eine Live-Konsolenausgabe des UART-Verkehrs.

## Nutzen für mich
Die Kategorie "WiFi" deckt bislang nur Scan-/Deauth-/EvilPortal-Funktionen innerhalb der
ESP32-Marauder-Firmware selbst (bereits mein Hauptwerkzeug) sowie die defensive Gegenseite
(Rogue AP Detector) ab. Diese App ist ein eigenständiger, deutlich schlankerer Zweitpfad ohne
volle Marauder-Firmware auf dem Devboard (ein einfacher Arduino-Sketch reicht), mit eigener
lokaler SD-Protokollarchitektur und einer Community-Vorlagenbibliothek für autorisierte
WLAN-Phishing-/Security-Awareness-Tests (z. B. im Rahmen eines mit Auftraggeber abgestimmten
Social-Engineering-Assessments, als Vergleich zur bereits vorhandenen Marauder-EvilPortal-
Funktion). Kein neuer Port-Kandidat im Sinne einer vollständigen Firmware-Integration, da rein
extern über UART betrieben — aber als eigenständige App-Option mit eigenem Review-Ergebnis
dokumentiert.

## API-87-Status
🔧 Migration nötig — die Flipper-seitige `.fap` baut gegen `mntm-012`/API 87.1 **nicht**
unverändert.

- `evil_portal_uart.c` spricht die UART-Peripherie direkt über die veraltete, von meinem
  Firmwarestand nicht mehr geführte API an: `furi_hal_uart_tx()`, `furi_hal_uart_set_br()`,
  `furi_hal_uart_set_irq_cb()`, `furi_hal_console_disable()`/`_enable()`. Ich habe in
  `targets/f7/api_symbols.csv` meines eigenen `G4MEOVER-FW` (`main`, `b359f6f`) sowie im
  gesamten übrigen Quellbestand per Volltextsuche **keine einzige** dieser vier Funktionen
  gefunden — anders als bei den bereits katalogisierten CLI-Kompat-Fällen (SWD Probe/GPIO Logic
  Analyzer, alte `cli_session_open/close`-API) gibt es für das alte direkte `furi_hal_uart_*`
  keinen firmwareseitigen Kompat-Shim mehr, die Symbole sind vollständig entfernt.
- Mein Stand führt stattdessen ausschließlich die neuere Handle-basierte Serial-API
  (`furi_hal_serial_init()`/`_configure_framing()`/`_async_rx_start()`/`_control_acquire()` auf
  einem `FuriHalSerialHandle*`), bereits an sechs Stellen im eigenen Quellbestand verifiziert
  (u. a. `applications/services/expansion/expansion_worker.c`,
  `applications/debug/uart_echo/uart_echo.c`, `applications/main/subghz/helpers/subghz_gps.c`)
  — dasselbe Muster, das bereits bei RS485 Modbus (`Serial_Begin()`-Duplikat) in meinem Katalog
  dokumentiert ist. Ein Port müsste `evil_portal_uart_init()`/`_tx()`/`_on_irq_cb()` komplett auf
  dieses Handle-Modell umstellen (Erwerb per `furi_hal_serial_control_acquire()`, Framing/Baud
  über `furi_hal_serial_configure_framing()`, RX über den Async-Callback statt den direkten
  IRQ-Callback) — kein triviales Austauschen einzelner Funktionsnamen, sondern eine echte
  API-Migration, vergleichbar mit der bereits dokumentierten Umstellung bei anderen über UART
  angebundenen Apps in meinem Katalog.
- Restlicher Flipper-`.fap`-Quellbestand (GUI/`ViewDispatcher`/`SceneManager`/
  `VariableItemList`/`TextBox`, `Storage`-API) nutzt durchgehend aktuelle, bei mir unverändert
  vorhandene Symbole — die Migration betrifft ausschließlich die UART-Schicht.
- Die ESP32-Seite (`EvilPortal.ino`, Arduino/ESP-IDF über `AsyncTCP`/`ESPAsyncWebServer`) ist von
  meiner Flipper-API-87.1-Frage unabhängig (eigener Mikrocontroller, eigene Toolchain) — hier
  kein Kompatibilitätsproblem, lediglich die übliche Arduino-IDE-Flash-Vorbereitung laut README.

## Quelltext-Review (Speichersicherheit) und Befund
Beim Lesen des kompletten Flipper-seitigen Quellbestands (729 Zeilen über sechs Dateien)
zwei gleichartige Lecks gefunden, beide über denselben Fehler: ein per `furi_string_alloc()`
erzeugtes `FuriString*`-Handle wird mit dem rohen `free()` statt mit `furi_string_free()`
freigegeben.

- `furi_string_alloc()`/`furi_string_free()` sind in meiner eigenen Firmware
  (`furi/core/string.c`) wie folgt definiert:
  `furi_string_alloc()` = `malloc(sizeof(FuriString)); string_init(string->string);`,
  `furi_string_free(s)` = `string_clear(s->string); free(s);`. Das darunterliegende
  m-string-Backend hält kurze Inhalte inline im Handle, muss für längere Inhalte aber einen
  separat allozierten Puffer nachziehen — nur `string_clear()` (und damit nur
  `furi_string_free()`) gibt diesen Zusatzpuffer wieder frei. Ein bloßes `free()` auf das
  Handle gibt ausschließlich das Handle selbst frei und lässt den Inhaltspuffer hängen, sobald
  einer alloziert wurde.
- **Fund 1** (`evil_portal_uart.c`, `uart_worker()`, SET_AP_CMD-Antwortzweig): baut
  `FuriString *out_data = furi_string_alloc(); furi_string_cat(out_data, "setap="); furi_string_cat(out_data, (char *)uart->app->ap_name);`
  und gibt danach mit `free(out_data);` statt `furi_string_free(out_data);` frei. Läuft bei
  jedem "Start Portal" einmal (Antwort auf die SET_AP_CMD-Bestätigung vom ESP32). Der separat
  gelesene `uart->app->ap_name`-Klartextpuffer selbst wird daneben korrekt mit dem passenden
  `free(uart->app->ap_name)` freigegeben — dieser zweite Aufruf ist richtig, da `ap_name` kein
  `FuriString` ist.
- **Fund 2** (`scenes/evil_portal_scene_console_output.c`,
  `evil_portal_scene_console_output_on_enter()`, SET_HTML_CMD-Zweig — der eigentliche
  "Start Portal"-Pfad): baut analog
  `FuriString *data = furi_string_alloc(); furi_string_cat(data, "sethtml="); furi_string_cat(data, (char *)app->index_html);`
  und gibt ebenfalls mit `free(data);` statt `furi_string_free(data);` frei. Praktisch der
  größere der beiden Lecks, da `index_html` die komplette geladene Phishing-Seite enthält
  (typischerweise mehrere hundert Byte bis einige KB, nicht nur eine kurze SSID) — und dieser
  Pfad bei jedem einzelnen "Start Portal" durchlaufen wird, nicht nur als Randfall.
- Beide Fundstellen betreffen damit den Kernworkflow der App (Portal starten), nicht einen
  Nebenpfad — auf einem Gerät mit so begrenztem RAM wie dem Flipper Zero (STM32WB55) bei
  wiederholtem Neustart/Umkonfigurieren des Portals während eines Assessments ein über die Zeit
  relevanter Speicherverlust.
- Restlicher Quellbestand ohne eigenen Fund: `evil_portal_storage.c`s
  `sequential_file_resolve_path()` prüft Puffergröße vor `snprintf()` korrekt, alle
  `storage_file_*`-Handles werden durchgehend gepaart geöffnet/geschlossen/freigegeben; der in
  `uart_worker()`/`evil_portal_console_output_handle_rx_data_cb()` genutzte `rx_buf[len] = '\0'`
  bleibt innerhalb der mit `+1` dimensionierten Puffergrenze (`RX_BUF_SIZE=320`,
  `rx_buf[RX_BUF_SIZE + 1]`), kein Off-by-one.

## Host-Repro
`ideas/repro-evil-portal-furi-string-free-leak/repro.c`: Stand-in-Nachbildung der realen
`FuriString`-Semantik (kurzer Inhalt bleibt inline im Handle, längerer Inhalt bekommt einen
separat allozierten Puffer, der nur über `furi_string_free()` wieder freigegeben wird), beide
Fundstellen 1:1 nachgebaut (SET_AP_CMD mit einer realistischen 18-Zeichen-SSID, SET_HTML_CMD
mit einer 132-Byte-Beispiel-Login-Seite), je 50 simulierte "Start Portal"-Zyklen ALT
(`free()`) vs. NEU (`furi_string_free()`-Äquivalent). Unter
`gcc -std=c17 -Wall -Wextra -fsanitize=address,leak,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1` jeweils in getrennten Prozessen ausgeführt (damit LeakSanitizer ALT/NEU nicht
im selben Lauf vermischt):
- ALT: 1200 Byte über 50 Zyklen beim SET_AP_CMD-Zweig, 7050 Byte über 50 Zyklen beim
  SET_HTML_CMD-Zweig — zusammen 8250 Byte in 100 Allokationen, identisch bei `-O0` und `-O1`,
  von LeakSanitizer jeweils exakt auf die `my_furi_string_cat_str()`-Allokation in den
  Fund-Codepfaden zurückgeführt.
- NEU (Fix): 0 Byte geleckt bei `-O0` und `-O1`.
- Zusätzlicher Regressionsnachweis für das Stand-in-Modell selbst: ein kurzer, unpräfigierter
  Inhalt bleibt inline und erzeugt von vornherein keinen separaten Puffer — der Fund greift
  gezielt erst, sobald der Inhalt (wie bei beiden realen Aufrufstellen durch das "setap="/
  "sethtml="-Präfix praktisch immer der Fall) die kleine Inline-Kapazität überschreitet.

## Fix-Skizze
Additiv, je ein Funktionsaufruf pro Fundstelle (Patches in
`ideas/evil-portal-furi-string-free-leak-fix/`):
- `evil_portal_uart_c.patch`: `free(out_data);` → `furi_string_free(out_data);` in
  `evil_portal_uart.c`.
- `evil_portal_scene_console_output_c.patch`: `free(data);` → `furi_string_free(data);` in
  `scenes/evil_portal_scene_console_output.c`.
- Beide Patches gegen eine frische, unabhängige Zweitkopie von
  `bigbrodude6119/flipper-zero-evil-portal` (`main`, Commit `d196f60`) mit `git apply --check`
  verifiziert, real angewendet, Ergebnis per `diff` gegen die erwartete Einzeiler-Änderung
  bestätigt.
- Diese beiden Fixes lösen das API-87-Migrationsproblem nicht — sie beheben nur die beiden
  Speichersicherheits-Funde innerhalb der UART-Schicht, die ein Port ohnehin komplett auf die
  `furi_hal_serial_*`-API umstellen muss.

## Aufwand
Mittel. Kein Firmware-Patch nötig (reine externe `.fap` + separate ESP32-Firmware), aber echte
Portierungsarbeit auf der UART-Schicht (siehe API-87-Status) statt eines reinen Rebuilds, dazu
ein Flipper-WiFi-Devboard oder ein kompatibles ESP32-S2-/WROOM-Zusatzboard als Hardware-
Voraussetzung. Zusätzlich zu berücksichtigen: Projekt seit `d196f60` (2023-07-17) ohne neuen
Commit — keine Upstream-Fixes zu erwarten, ein eigener Port müsste beide Lecks sowie die
API-Migration selbst tragen.

## Sicherheitsrahmen
Aktives Social-Engineering-/Credential-Harvesting-Werkzeug (Rogue-AP mit gefälschter
Login-Seite) — ausschließlich im Rahmen eines mit dem Auftraggeber/Netzbetreiber abgestimmten,
autorisierten WLAN-Phishing-/Security-Awareness-Assessments oder gegen eigene Testgeräte
einsetzen, nie gegen nicht einwilligende Dritte oder im öffentlichen Raum ohne Freigabe —
deckungsgleich mit CONSTRAINTS Punkt 5 und demselben Rahmen, den ich bereits bei Evil BLE/
BLE Spam für aktive Täuschungs-/Identitätsvalidierungswerkzeuge dokumentiert habe. Kein
Baustein zu Massenangriffen oder Detection-Evasion — reine App-Review-/Portierungsfrage, keine
Anleitung zum Einsatz gegen Dritte.

## Offen
- Lokale Bauprobe nach der UART-Schicht-Migration (`ufbt`), danach Testlauf gegen ein eigenes
  ESP32-S2-/WROOM-Board und ein eigenes Testgerät, inkl. gezielter Nachvollzug beider Lecks vor
  und nach dem Fix (z. B. `free`-Heap-Tracking über mehrere "Start Portal"-Zyklen).
- Marauder-eigene EvilPortal-Funktion (bereits in meinem Katalog unter "Marauder/GhostESP"
  gelistet) als direkter Funktionsvergleich noch offen — insbesondere, ob Marauders Variante
  dieselbe lokale SD-Protokollarchitektur/Vorlagenvielfalt bietet oder nur das reine
  Portal-Hosting.
- Neue Katalog-Zeile unter "WiFi" ergänzt.
