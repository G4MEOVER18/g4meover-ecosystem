# SWD Probe — API-87-Status

Neue Ideen-Recherche (kein Rückstellpunkt aus einer früheren Sitzung): gezielt nach einer
bislang komplett fehlenden Kategorie gesucht — Hardware-Debug-Schnittstellen (JTAG/SWD) als
Angriffs-/Auditfläche, über die bestehende GPIO/Serial-Bus-Sammlung (Bus-Analyse: UART Sniff,
GPIO Logic Analyzer, i2c Tools, RS485 Modbus) und den reinen Chip-Dump (SPI Flash Dump) hinaus.

## Quelle

Repo: `g3gg0/flipper-swd_probe` (GPL-3.0, derselbe Autor wie der bereits katalogisierte GPIO
Logic Analyzer). Vier Quelldateien (`swd_probe_app.c/.h`, `adi.c/.h`, `usb_uart.c/.h`), ca. 4400
Zeilen, vollständig gelesen. `application.fam`: `appid="swd_probe"`, `fap_category="Tools"`,
`requires=["notification", "gui", "storage", "dialogs", "cli"]`.

## Was

Probiert ARM-SWD-Signale (Debug-Port der meisten modernen Cortex-M-Mikrocontroller, u. a.
STM32) über acht wählbare Erweiterungsport-Pin-Paare per Bisektion durch und piept bei
erkanntem, gültigem SWD-`IDCODE` — zeigt dabei JEP106-Herstellerkürzel und erkannten Pinout an.
Nach Erkennung: vollwertige ARM-Debug-Interface-Implementierung (Debug-Port-/Access-Port-
Register, CoreSight-ROM-Table-Walk, Speicher über Memory-AP lesen/schreiben, Cortex-M-Core-
Register über DCRSR/DCRDR, Haltepunkte) plus eine eigene kleine Skriptsprache (`.swd`-Dateien
von SD, Befehle wie `halt`/`mem_dump`/`mem_write`/`call`) für wiederkehrende Abläufe ohne PC/
OpenOCD. Zweiter virtueller COM-Port (parallel zur CLI) erlaubt zusätzlich Steuerung per
Skript-Upload vom PC.

## Nutzen für mich

Deckt die bisher fehlende erste Phase eines Hardware-Assessments ab: vor dem gezielten Einsatz
von SPI Flash Dump (bekannter Flash-Chip) oder GPIO Logic Analyzer/i2c Tools/UART Sniff/RS485
Modbus (bekannter Bus) steht bei einer unbekannten Platine oft zuerst die Frage, ob ein offener
SWD-Debug-Port vorhanden ist, auf welchen zwei Pins er liegt und welcher Chip dahintersteckt.
Eigenständige, neue Kategorie ("Hardware Debug Interface"), keine Überschneidung mit
bestehenden Einträgen.

## API-87-Status

✅ Kompatibel (über den bereits vorhandenen CLI-Kompat-Shim), geprüft gegen den eigenen
`mntm-012`-Tag-Stand (frischer, blobloser Checkout). Details, inkl. einer bislang bei mir noch
nicht explizit dokumentierten Besonderheit (`usb_uart.c` nutzt die *alte* `cli_session_open/
close`-CLI-API statt der aktuellen `cli_vcp_enable/disable`-API — läuft bei mir firmwareseitig
bereits über den in `BACKLOG.md` unter "Kompatibilität" dokumentierten Alt-App-Shim, kein neuer
Patch dafür nötig, aber auch kein versehentliches "Reparieren" beim eigenen Bau) sowie der
API-87-Abgleich aller acht Erweiterungsport-Pins und der USB-CDC-Zweitkanal-Technik in
`firmware-mining/swd-probe-app-port.md`.

## Quelltext-Review (Speichersicherheit) und gefundene Einschränkung

Fund: zwei vom Dateibrowser gelieferte Pfade (unbegrenzte Länge bei verschachtelten SD-Ordnern)
landen ungeprüft per `strcpy()` in feste `char[128]`-Felder zweier `malloc()`-allozierter
Structs (`ScriptContext.filename` in `swd_execute_script()`, `AppFSM.script_detected` im
Dateibrowser-"Down"-Handler) — ein echter Heap-Buffer-Overflow ab 128 Zeichen Pfadlänge, kein
reiner `furi_check()`-Abbruch. Die beiden anderen `strcpy()`/`strcat()`-Stellen im selben File
sind bereits sauber längengeprüft. Port-Vorschlag inkl. Fix (additive Längenprüfung vor beiden
Stellen, analog zum bereits vorhandenen Muster im selben File) und Host-Repro unter ASan/UBSan
(Fund reproduziert: `heap-buffer-overflow`, Fix verifiziert: 0 Meldungen, Regressions-/
Grenzfall identisch) siehe `firmware-mining/swd-probe-app-port.md`.

## Aufwand

Gering für die Software (vier Dateien, reine externe `.fap`, keine Firmware-Änderung, keine
Zusatzhardware über den vorhandenen Erweiterungsport hinaus). Für den vollen Funktionsumfang am
Gerät zusätzlich ein eigenes Zielboard mit offenem SWD-Port nötig (z. B. ein eigenes
STM32-Nucleo/Discovery-Board) — ohne eigene Zielhardware lässt sich nur der reine
Pin-Scan-Ablauf (kein `IDCODE`-Treffer) sowie der hier gefundene Pfadlängen-Fix prüfen.

## Sicherheitsrahmen

Aktive Schnittstelle — anders als die bisher katalogisierten, rein passiven GPIO/Serial-Funde
(UART Sniff/GPIO Logic Analyzer/i2c Tools) erlaubt ein erkannter, offener SWD-Port vollen
Lese-/Schreibzugriff auf Speicher und Register des Zielchips, je nach Ziel auch
Firmware-Extraktion oder Debug-Halt. Nur an eigenen Geräten oder im Rahmen autorisierter
Hardware-Pentests einsetzen (CONSTRAINTS Punkt 5); bei Drittsystemen vorab die Berechtigung
klären.

## Offen

- Lokale Bauprobe gegen die eigene `mntm-012`-Basis, danach Funktionstest an einem eigenen
  Cortex-M-Board mit bekanntem, offenem SWD-Port (Pin-Erkennung, `IDCODE`/JEP106-Anzeige,
  Speicherlesen über die ADI-Implementierung).
- Gezielt den Pfadlängen-Fix am Gerät nachvollziehen: ein `.swd`-Skript in einem absichtlich
  tief verschachtelten SD-Ordner ablegen (Gesamtpfad ≥ 128 Zeichen) und über den Dateibrowser
  auswählen — muss "Path too long" zeigen statt abzustürzen.
- Die eigene Skriptsprache (`.swd`-Dateien) für wiederkehrende Abläufe (z. B. Speicher-Dump
  direkt nach dem Boot eines eigenen Zielboards) noch nicht praktisch erprobt.
