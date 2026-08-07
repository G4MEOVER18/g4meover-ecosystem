# Wiegand Reader — externe App aus RogueMaster-Bündel (Physical-Access-Audit)

## Was
Ich habe das RogueMaster-App-Bündel (`flipperzero-firmware-wPlugins`, Branch `420`,
`applications/external/`, ~700 Einträge) durchgesehen, um laut Backlog nützliche, API-87-
kompatible Apps zu finden, die mir fehlen. Die **Wiegand Reader**-App (`applications/external/
wiegand`, Autor jamisonderek, Quelle `jamisonderek/flipper-zero-tutorials`, MIT-Lizenz) sticht
heraus: eine eigenständige GPIO-App zum Sniffen, Decodieren, Speichern und Wiedergeben von
Wiegand-Signalen zwischen Kartenleser und Türcontroller — klassisches Werkzeug für autorisierte
physische Zutrittskontroll-Audits, das mir in meinem Katalog bisher komplett fehlt (mein Katalog
deckt SubGHz/NFC/RFID/BadUSB/2.4G/WiFi ab, aber keine dedizierte Wiegand-Schnittstelle).

Funktionsumfang laut Quelle:
- Unterstützt W4, W8, W24, W26, W32, W34, W35, W36, W37, W40, W48.
- Read-Modus: sniffed D0/D1-Flanken per GPIO-Interrupt, misst Timing über den DWT-Zykluszähler,
  erkennt Frame-Ende per Timeout (>25 ms Pause) und bekannte Bitlängen.
- Decode: für W4/W8 (Tastenfeld inkl. ESC/Enter), W24/W26 (Facility Code + Card-ID, Paritätsprüfung),
  W35/W36/W48 (HID Corporate 1000 / Keyscan) — inkl. Bit-Error-Anzeige bei Paritätsfehlern.
- Save/Load: Rohdaten (Bitfolge + Timings) als `.wgn`-Datei unter `apps_data/wiegand/`.
- Play (Replay): gibt eine geladene/gemessene Bitfolge über D0/D1 aus (Open-Drain + optionale
  MOSFET-Ansteuerung für niederohmige Leser), inkl. Timing-Rekonstruktion aus den gespeicherten
  Flanken.

Technisch: reines `furi_hal_gpio`/`furi_hal_power`(OTG)/`FuriTimer`/Scene-Manager-App, keine
externen Libraries, keine `api_version`-Einschränkung im `application.fam` — nutzt durchgehend
aktuelle Firmware-APIs (`furi_hal_gpio_init_simple`, `GpioModeInterruptRiseFall`,
`furi_hal_gpio_add_int_callback`, `expansion.h` zum Deaktivieren des Expansion-Moduls während der
Nutzung). Kein Hinweis auf veraltete/entfernte APIs gegenüber mntm-012.

## Nutzen
- Schließt eine echte Lücke in meinem Katalog: physische Zutrittskontroll-Systeme (Wiegand ist
  bei Türlesern/Keypads extrem verbreitet) kann ich bisher gar nicht testen — nur SubGHz/NFC/RFID
  sind abgedeckt.
- Direkt für autorisierte physische Pentests nutzbar: Leser/Keypad auf Schwachstellen prüfen
  (Klartext-Wiegand ohne Verschlüsselung ist Stand der Technik bei den meisten Bestandssystemen),
  Timing/Format eines unbekannten Lesers verifizieren, mit eigener/autorisierter Testhardware
  Replay-Verhalten eines Controllers prüfen.
- Sehr kleiner Port-Aufwand: 2 Quelldateien + 7 Scene-Dateien (zusammen ~1170 Zeilen), keine
  externen Libs, keine Submodule, kein API-Versions-Constraint — im Gegensatz zu größeren
  Kandidaten im selben Bündel (z. B. `seader`, `picopass`), die ASN.1/mbedTLS-Abhängigkeiten
  mitbringen und daher deutlich mehr Portaufwand bedeuten würden.
- MIT-lizenziert, aktiv über den `flipper-zero-tutorials`-Autor gepflegt.

## Betroffene Dateien (lokal)
Neu, unverändert aus der Quelle übernehmbar (eigenständiger externer App-Ordner, kein Eingriff in
Firmware-Kernbestand nötig):
- `applications_user/wiegand/application.fam`
- `applications_user/wiegand/wiegand.c`
- `applications_user/wiegand/wiegand.h`
- `applications_user/wiegand/wiegand.png`
- `applications_user/wiegand/scenes/wiegand_data.c`
- `applications_user/wiegand/scenes/wiegand_instructions.c`
- `applications_user/wiegand/scenes/wiegand_load.c`
- `applications_user/wiegand/scenes/wiegand_main_menu.c`
- `applications_user/wiegand/scenes/wiegand_play.c`
- `applications_user/wiegand/scenes/wiegand_read.c`
- `applications_user/wiegand/scenes/wiegand_save.c`
- `applications_user/wiegand/scenes/wiegand_scan.c`
- `applications_user/wiegand/LICENSE`

Bewusst **nicht** übernehmen: `examples/C1.wgn` aus der Quelle — CONSTRAINTS verbieten mir
generell Capture-/Kartendaten-Dateien in diesem Repo, unabhängig davon ob es sich um ein
Beispiel oder eine echte Aufzeichnung handelt. Für den lokalen Funktionstest reicht ein selbst
mit der App erzeugtes `.wgn` von einem eigenen/autorisierten Testleser.

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (baut jede App per `ufbt` gegen
das Momentum-SDK mntm-012/API 87.1 inkl. APPCHK) — bei Bedarf in ein eigenes App-Repo
`g4meover-wiegand` (oder direkt in `applications_user/` meiner Firmware) kopieren.

## Risiko
- Niedrig: additiver externer App-Ordner, keine Änderung an Firmware-Kernbestand oder
  gemeinsamem State. Fehlschlag zeigt sich beim Build (`ufbt`/APPCHK) als Compile-Fehler.
- GPIO-Pin-Nutzung ist fix im Quellcode: D0 = Pin A4 (+ Mosfet-Gate B3), D1 = Pin A7
  (+ Mosfet-Gate A6), aktiviert dabei `furi_hal_power_enable_otg()` (5V am GPIO-Header). Vor dem
  lokalen Test gegen meine anderen GPIO-nutzenden Apps (z. B. lora-ukfe/E220-Modul-Config,
  RF-Console) abgleichen, dass keine Pin-Überschneidung vorliegt — die App selbst greift nur
  während ihrer eigenen Laufzeit auf die Pins zu (Read/Play initialisieren, Stop setzt auf
  `GpioModeAnalog` zurück), ein dauerhafter Konflikt ist bei Einzel-App-Nutzung nicht zu erwarten.
- Play-Modus (TX) legt laut Quelle beide Leitungen aktiv auf Low, während gesendet wird — laut
  Autoren-Warnung besteht Kurzschlussgefahr, wenn währenddessen gleichzeitig ein echter Leser
  Daten sendet. Bei niederohmigen Lesern (<1 kΩ Pull-up) sind laut Doku zusätzliche MOSFETs +
  Pull-down-Widerstände nötig (siehe Wiring-Abschnitt in der Quelle) — vor dem Anschluss an
  fremde/unbekannte Leser-Hardware unbedingt selbst nachmessen.
- Sicherheitsrahmen: ausschließlich an eigener oder vertraglich autorisierter Zutrittskontroll-
  Hardware einsetzen (Wiegand überträgt bei den meisten Bestandssystemen unverschlüsselt —
  Sniffing/Replay ist damit sensibel und nur im autorisierten Pentest-/CTF-/Lehr-Kontext
  zulässig, siehe CONSTRAINTS Punkt 5).
- Kein Datenschutz-/Fremddaten-Risiko für dieses Repo, solange keine `.wgn`-Dateien gepusht
  werden (siehe CONSTRAINTS Punkt 2/3 — Capture-Verbot gilt sinngemäß auch für `.wgn`).

## Port-Skizze
1. App-Ordner `wiegand` (alle unter „Betroffene Dateien" gelisteten Dateien außer `examples/`)
   aus `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, `applications/external/
   wiegand/`) unverändert nach `applications_user/wiegand/` in meiner Firmware kopieren.
2. `ufbt launch_app APPSRC=applications_user/wiegand` bzw. `fbt launch_app
   APPSRC=applications_user/wiegand` — Build deckt etwaige API-Abweichungen sofort als
   Compile-Fehler auf (laut Quellcode-Durchsicht nicht erwartet, da nur aktuelle
   `furi_hal_gpio`/`furi_hal_power`/Scene-Manager-APIs verwendet werden).
3. GPIO-Pinbelegung (A4/A7/A6/B3) gegen meine übrigen GPIO-Apps abgleichen (siehe Risiko) —
   reine Dokumentenprüfung, kein Codeeingriff nötig, solange keine dauerhafte Pin-Reservierung
   durch eine andere App vorliegt.
4. Funktionstest lokal an eigenem/autorisiertem Wiegand-Leser oder -Keypad: Read-Modus →
   Bitfolge + erkanntes Format prüfen, Save → `.wgn` erzeugen, Load + Play → Wiedergabe am
   selben oder einem Testcontroller verifizieren (siehe Wiring-Hinweise der Quelle für
   niederohmige Leser).
5. Bei erfolgreichem Test: App optional in ein eigenes Repo `g4meover-wiegand` auslagern und
   `ci-templates/build-fap.yml` als CI ergänzen, oder dauerhaft in `applications_user/` meiner
   Firmware belassen.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/wiegand/` (Stand des Branches beim Durchsehen: Commit `b9629a6`).
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `jamisonderek/flipper-zero-tutorials`,
  Pfad `gpio/wiegand` (MIT-Lizenz, Copyright Derek Jamison).
