# Nearby Files — Submodul-Pin-Bump v1.5 → v1.7 (applications/external)

## Was

Die „Nearby Files"-App (`Stichoza/flipper-nearby-files`, GPS-gestützter Distanz-Browser für
`.sub`/`.nfc`/`.rfid`/`.ibtn`) liegt in meinem `mntm-012`-Baum nicht als eigener Port, sondern über
das `applications/external`-Submodul (`Next-Flip/Momentum-Apps`) — aktuell auf Commit
`85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4` gepinnt (`fap_version="1.5"`, siehe
`ideas/nearby-files-gps-app-status.md`). Ich habe den `Next-Flip/Momentum-Apps`-`dev`-Branch frisch
geklont (aktueller Commit `55a446b1b01bf2a2f98161d704e62cc47075ad30`) und per
`git log --oneline -- nearby_files` geprüft, welcher Commit seit meinem Pin zuletzt an diesem
App-Pfad gearbeitet hat: genau ein Merge, `f3102847c3368048b8bf969f6e880865706278a4` (2026-06-02,
„Merge nearby_files from Stichoza/flipper-nearby-files"), der die App direkt von `v1.5` auf
`fap_version="1.7"` bringt (laut `.catalog/CHANGELOG.md` deckt `v1.6` keine Code-Änderung an diesem
Pfad ab, nur die Versionsnummer wurde mitgezogen). Der Pin-Bump ist damit ein **einzelner**
Commit-Sprung, kein mehrstufiges Nachziehen mehrerer Zwischenversionen.

`git diff 85b0cdb4e..f3102847c -- nearby_files` vollständig gelesen (15 Dateien, +933/−236 Zeilen)
und gegen `CONSTRAINTS.md`/API-87.1 geprüft:

- **GPS-Baudraten-Auswahl** (neue Scene `GpsBaudrate` + Menüeintrag „GPS Baudrate"): `gps_reader.c`
  bekommt `gps_reader_get_baudrate()`/`gps_reader_set_baudrate()`, die gewählte Baudrate wird über
  `saved_struct_load/save` (`APP_DATA_PATH("config.bin")`) persistent gespeichert und beim nächsten
  Start automatisch geladen (`gps_reader_alloc(uint32_t initial_baudrate)` — Signaturänderung).
  `gps_reader_set_baudrate()` validiert gegen die unterstützte Baudraten-Liste, setzt bei
  tatsächlicher Änderung `coordinates.valid`/`module_detected`/`satellite_count` zurück und führt
  einen vollständigen Worker-Neustart durch (`gps_reader_stop_worker()` joint den Thread und gibt
  `rx_stream` frei, **bevor** `gps_reader_start_worker()` neu alloziert) — sauber serialisiert,
  keine Race zwischen altem und neuem Worker geprüft (siehe „Geprüft" unten).
- **„Add GPS to file"** (neue Scene `AddGps` + Menüeintrag): `nearby_files_add_gps_to_file()` lässt
  per `DialogsFileBrowserOptions`-Dateibrowser eine `.sub`/`.nfc`/`.rfid`/`.ibtn`-Datei wählen und
  schreibt die aktuelle GPS-Position hinein (`nearby_files_update_coordinates_in_file()`): bestehende
  `Lat:`/`Lon:`/`Latitude:`/`Longitude:`-Zeilen werden entfernt, bei `.sub`-Dateien die neue
  Koordinate direkt vor der `Protocol: `-Zeile eingefügt (passend zum „SubGHz Wardriving"-Format),
  bei anderen Dateitypen ans Ende angehängt. Bricht sauber mit eigener Fehlermeldung ab, wenn kein
  gültiger GPS-Fix vorliegt (`current_coords.valid` geprüft) oder der Schreibvorgang fehlschlägt.
- **iButton-Unterstützung** (bereits mit `v1.6` eingeführt, in meinem `v1.5`-Pin noch nicht
  vorhanden): vierter Scan-Pfad `/ext/ibutton`/`.ibtn` neben SubGHz/NFC/LFRFID, inkl. eigenem
  Icon (`ibutt_10px.png`) und App-Namen-Eintrag für den Start über „Click to launch".
- Vier neue 10px-Icons (`icons/{125,Nfc,ibutt,sub1}_10px.png`) für die Dateityp-Auswahl in der
  „Add GPS"-Scene, dazu `fap_icon_assets="icons"` in `application.fam` ergänzt (Standard-FBT-Feature
  für Icon-Asset-Kompilierung, in meinem eigenen Baum bereits von anderen Apps genutzt — kein neues
  Risiko).
- `nearby_files_scene_config.h` bekommt zwei neue `ADD_SCENE`-Einträge (`add_gps`, `gps_baudrate`),
  `nearby_files_scene_menu.c` verdrahtet beide korrekt mit eigenen Menüeinträgen UND eigenen
  `NearbyFilesCustomEvent*`-Fällen — anders als beim eigenen, noch offenen `lora-ukfe`-
  Settings-Scene-Fund (siehe `app-improvements/lora-ukfe/scene-settings-back-navigation-fix.md`)
  ist hier die neue Scene tatsächlich über einen Menüeintrag erreichbar, kein Nebenbefund zu
  vermerken.

## Geprüft (eigener Review, über die reine Changelog-Übernahme hinaus)

- `gps_reader_set_baudrate()`/`_stop_worker()`/`_start_worker()` auf Race-Bedingungen durchgesehen:
  der Stop-Pfad setzt `WorkerEvtStop`, joint den Thread, und **erst danach** im Worker-Thread selbst
  (`gps_reader_worker()`, nach der `while(1)`-Schleife) wird `gps_reader_serial_deinit()`
  (stoppt `furi_hal_serial_async_rx_stop()` **vor** dem Deinit) und `furi_stream_buffer_free()`
  aufgerufen — die IRQ-Callback (`gps_reader_on_irq_cb`) kann nach dem Stop-Flag also keine Daten
  mehr in einen bereits freigegebenen Stream schreiben. `gps_reader_start_worker()` alloziert einen
  neuen `rx_stream` erst nach dem vollständigen `furi_thread_join()` des alten Threads. Kein neuer
  Fund — sauber serialisiert.
- `nearby_files_update_coordinates_in_file()` (neue Schreibfunktion) zeilenweise durchgesehen:
  CRLF-Behandlung (manueller `i++`-Sprung bei `\r\n`), Einfügepunkt vor `Protocol: ` nur einmal pro
  Datei (`inserted_coordinates`-Flag), Fallback-Anhängen am Dateiende, wenn keine `Protocol: `-Zeile
  gefunden wird oder die Datei kein `.sub` ist — kein Logikfehler gefunden, der Schreibpfad entfernt
  alte Koordinatenzeilen zuverlässig und ersetzt sie durch genau eine neue.
- Der bereits dokumentierte `&&`/`||`-Fund in `nearby_files_process_gps_coordinates()`
  (`ideas/nearby-files-gps-app-status.md`) ist in `v1.7` unverändert an derselben Stelle vorhanden
  (gleiche Funktion, gleiche Zeile, nur um eine Zeile verschoben durch Reformatierung) — per
  `grep -n "0.0f" nearby_files.c` gegen den frischen `v1.7`-Checkout verifiziert. Kein neuer Fund,
  aber der bestehende Fix-Patch muss für den neuen Dateistand neu erzeugt werden (siehe unten).
- `gps_reader_alloc(config_baudrate)`: `nearby_files_load_config_baudrate()`s Rückgabewert wird an
  der Aufrufstelle nicht geprüft (`config_baudrate` bleibt beim ersten Start/ohne Config-Datei auf
  `0`) — das ist unkritisch, weil `gps_reader_alloc()` die übergebene Baudrate selbst gegen die
  unterstützte Liste validiert (`gps_reader_is_supported_baudrate()`) und bei `0` (nicht in der
  Liste) automatisch auf den Default (9600) zurückfällt. Kein Fund, bewusst so geprüft, um keinen
  Phantom-Bug zu melden.

## Betroffene Dateien (lokal)

Kein eigener App-Ordner-Kopiervorgang nötig (anders als beim `wiegand`-Port) — die App läuft über
den Submodul-Pin. Betroffen ist ausschließlich die Pin-Referenz auf
`Next-Flip/Momentum-Apps` in meinem Firmware-Baum (`applications/external`-Submodul-Commit), danach
automatisch im Build enthalten:

- `nearby_files/application.fam` (Version 1.5→1.7, `fap_icon_assets="icons"` neu)
- `nearby_files/nearby_files.c` (+555/−~130 Zeilen: Add-GPS-Schreibpfad, Baudraten-Config, Diff
  vollständig gelesen)
- `nearby_files/nearby_files.h`
- `nearby_files/gps_reader.c` / `gps_reader.h` (Baudraten-Get/Set, Worker-Neustart)
- `nearby_files/scenes/nearby_files_scene_add_gps.c` (neu)
- `nearby_files/scenes/nearby_files_scene_gps_baudrate.c` (neu)
- `nearby_files/scenes/nearby_files_scene_config.h` (zwei neue `ADD_SCENE`-Zeilen)
- `nearby_files/scenes/nearby_files_scene_menu.c` (zwei neue Menüeinträge + Events)
- `nearby_files/icons/{125,Nfc,ibutt,sub1}_10px.png` (neu)
- `nearby_files/README.md` (Doku-Update, kein Codeeinfluss)

Zusätzlich in diesem Repo mitgeliefert — regenerierter Fix für den bereits bekannten, in `v1.7`
unverändert vorhandenen Zero-Coordinate-Filter-Fund (der bisherige Patch aus
`ideas/nearby-files-zero-coordinate-filter-fix/` passt wegen der Reformatierung/Zeilenverschiebung
durch den Pin-Bump nicht mehr — per `git apply --check` gegen einen frischen `v1.7`-Checkout
gegengeprüft und bestätigt fehlgeschlagen):

- `firmware-mining/nearby-files-v1.5-to-v1.7-pin-bump/nearby_files_c_zero_coordinate_filter_v17.patch`
  — identischer semantischer Fix (`&&` → `||` in `nearby_files_process_gps_coordinates()`), nur
  gegen die neue Zeilennummer (728 statt 490) neu erzeugt. Gegen zwei frische, unabhängige
  Zweitkopien des `v1.7`-Standes (`f3102847c3368048b8bf969f6e880865706278a4`) je per
  `git apply --check`/`git apply` UND `patch -p1` verifiziert (letzteres mit Zeilen-Offset −1,
  laut `patch` selbst toleriert), Ergebnis laut `md5sum` in beiden Fällen und beiden Prüfmethoden
  byte-identisch. Der bereits vorhandene Host-Repro
  (`ideas/repro-nearby-files-zero-coordinate-filter/repro.c`) deckt dieselbe Filterbedingung
  dateiversionsunabhängig ab — erneut unter `-O0`/`-O1` mit ASan/UBSan laufen lassen, weiterhin
  `3352 Prüfungen bestanden`, kein neuer Repro nötig.

## Risiko

- Niedrig: additiver Versionssprung derselben, bereits in meinem Baum laufenden App über einen
  einzigen, klar identifizierten Upstream-Commit — kein mehrstufiges Nachziehen, kein eigener
  Quellcode-Fork nötig. Fehlschlag zeigt sich beim Build (`fbt`/`ufbt`) sofort als Compile-Fehler.
- `gps_reader_alloc()`/`gps_reader_set_baudrate()` ändern die Worker-Thread-Lebensdauer (Neustart
  statt dauerhaft laufendem Thread) — in meinem Review sauber serialisiert (siehe „Geprüft"), aber
  nur per Codelesen geprüft, nicht auf echter Hardware mit tatsächlichem Baudratenwechsel am GPS-
  Modul getestet (siehe Offen).
- Die neue „Add GPS to file"-Schreibfunktion öffnet Dateien mit `FSOM_CREATE_ALWAYS` (vollständiges
  Neuschreiben) — bei einem Stromausfall/Herausziehen der SD-Karte während des Schreibvorgangs
  könnte die Zieldatei im ungünstigsten Fall unvollständig sein (für `.sub`/`.nfc`/`.rfid`/`.ibtn`-
  Dateien aus eigenen, bereits erfassten Captures ein vertretbares Risiko, kein Datenverlust Dritter
  — betrifft ausschließlich selbst gewählte, bereits auf der eigenen SD-Karte liegende Dateien).
- Sicherheitsrahmen unverändert: reine Lese-/Anreicherungsfunktion für eigene Captures (GPS-Tagging,
  Browserei nach Distanz), kein Sende-/Angriffsverhalten — passt weiterhin in den defensiven Rahmen
  aus `CONSTRAINTS.md` Punkt 5.

## Port-Skizze

1. Im eigenen `G4MEOVER-FW`-Baum den `applications/external`-Submodul-Pin (`Next-Flip/Momentum-Apps`)
   von `85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4` auf `f3102847c3368048b8bf969f6e880865706278a4`
   vorziehen (`git submodule update` bzw. `git -C applications/external fetch && git -C
   applications/external checkout f3102847c...`, je nach eigenem Submodul-Setup).
2. `fbt`/`ufbt` einmal komplett neu bauen (Pin-Bump zieht den gesamten Momentum-Apps-Stand nach,
   nicht nur `nearby_files` — andere ggf. mitgezogene Änderungen im selben Commit sind außerhalb
   dieser Sitzung nicht geprüft und vor dem Flashen über das normale Build-Log zu kontrollieren).
3. Den regenerierten Patch
   `firmware-mining/nearby-files-v1.5-to-v1.7-pin-bump/nearby_files_c_zero_coordinate_filter_v17.patch`
   gegen `applications/external/nearby_files/nearby_files.c` anwenden (der bisherige `v1.5`-Patch
   aus `ideas/nearby-files-zero-coordinate-filter-fix/` entfällt damit für den neuen Stand).
4. Funktionstest lokal: App starten, „GPS Baudrate" öffnen, eine andere als die aktuelle Baudrate
   wählen, bestätigen, dass die Positions-/Satelliten-Anzeige nach dem Wechsel wieder einen validen
   Fix bekommt (Worker-Neustart); App neu starten und prüfen, dass die gewählte Baudrate automatisch
   geladen wird (`saved_struct`-Persistenz). „Add GPS to file" an einer eigenen `.sub`-Testdatei ohne
   Koordinaten ausprobieren, prüfen, dass `Lat:`/`Lon:` korrekt vor der `Protocol: `-Zeile eingefügt
   werden und die Datei danach weiterhin mit der Sub-GHz-App ladbar ist. iButton-Scan mit einer
   eigenen `.ibtn`-Testdatei unter `/ext/ibutton` gegenprüfen.
5. Nach erfolgreichem Test: `ideas/nearby-files-gps-app-status.md` um einen Verweis auf diesen
   Pin-Bump ergänzen (nächste Sitzung, nicht Teil dieses additiven Fundes).

## Offen

- Lokaler Build/Flash sowie alle unter „Port-Skizze" Punkt 4 genannten Funktionstests stehen aus —
  diese Sitzung liefert ausschließlich den geprüften Pin-Bump-Vorschlag und den regenerierten Patch.
- Baudraten-Wechsel am echten GPS-Modul (tatsächliche Neukonfiguration der Modul-Baudrate selbst,
  z. B. per `$PMTK251`-Kommando) ist **nicht** Teil dieser App — `gps_reader_set_baudrate()`
  konfiguriert nur die UART-Empfangsseite des Flipper neu. Setzt voraus, dass das eigene GPS-Modul
  bereits auf die gewählte Baudrate eingestellt ist (separates, hier nicht geprüftes Thema).
- Die drei neuen `v1.6`/`v1.7`-Funktionen (iButton-Scan, Baudraten-Persistenz, „Add GPS"-Schreibpfad)
  wurden in dieser Sitzung auf eigene Logikfehler durchgesehen (siehe „Geprüft") — kein neuer Fund
  über den bereits bekannten Zero-Coordinate-Filter hinaus.

## Quelle

- `Next-Flip/Momentum-Apps`, Branch `dev`, Pfad `nearby_files/` — Bump-Ziel-Commit
  `f3102847c3368048b8bf969f6e880865706278a4` (2026-06-02), aktueller eigener Pin
  `85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4` (`v1.5`).
- Ursprungsquelle laut `application.fam`: `Stichoza/flipper-nearby-files` (MIT-Lizenz).
- Bisheriger eigener Fund/Fix zum selben Code: `ideas/nearby-files-gps-app-status.md`,
  `ideas/nearby-files-zero-coordinate-filter-fix/`, `ideas/repro-nearby-files-zero-coordinate-filter/`.
