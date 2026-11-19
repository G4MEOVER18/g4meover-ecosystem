# ProtoView — Status, Fund, Fix

## Was

[ProtoView](https://github.com/antirez/protoview) (Salvatore Sanfilippo/`antirez`, BSD-2-Clause,
Tag `fd2f6ed`, letzter Commit 2025-05-05) ist ein eigenständiges SubGHz-Werkzeug: zeigt rohe
High-/Low-Pulse eines empfangenen Funksignals direkt an (Zeitdiagramm, Bit-Grid), bevor/ohne dass
ein Protokoll-Decoder dafür existiert, erkennt heuristisch, ob ein Sample eine echte
Signal-Sequenz statt nur Rauschen ist, decodiert vier konkrete Protokolle nativ (TPMS
Renault/Toyota/Schrader/Citroen/Ford, Microchip-HSC200/300/301-KeeLoq, Oregon Thermometer
Protokoll 2, PT2262/SC5262-basierte Fernbedienungen), erlaubt das Editieren einzelner Bits eines
erfassten Signals über eine eigene Tastatur-Ansicht und das erneute Senden (auch auf einer
anderen Frequenz/Modulation als empfangen). 598 Sterne, 24 Forks, aktiv (14 offene Issues, 2 offene
PRs). 3701 Zeilen über 16 Quelldateien, vollständig gelesen.

## Nutzen

Ergänzt mein bestehendes SubGHz-Werkzeug (RollForge/ProtoPirate, Frequency Analyzer+) um die
Lücke, die beide bewusst nicht abdecken: ein unbekanntes Signal, für das weder RollForge noch die
Stock-SubGhz-App einen passenden Protokoll-Decoder mitbringen, lässt sich mit RollForge nur als
Rohpuls-Dump analysieren — ProtoView zeigt stattdessen sofort ein lesbares Zeitdiagramm plus
Bit-für-Bit-Grid direkt am Gerät, ohne Dump auf einen PC zu ziehen. Die vier nativ unterstützten
Protokolle (v. a. TPMS) sind zusätzlich sofort einsatzfähig als Erweiterung der eigenen passiven
Recon-Palette (`Weather Station`/`TPMS Reader` decken nur die Stock-Protokollliste ab). Der
Bit-Edit-und-Resend-Workflow ist für autorisierte Protokoll-Analyse eigener Signale gedacht
(z. B. ein einzelnes Bit eines eigenen Garagentor-Signals gezielt kippen, um dessen Bedeutung zu
verifizieren), kein Ersatz für RollForges Rolling-Code-Mechanik.

## Aufwand

Kein eigenständiger `.fap`-Download via App-Katalog (nicht im offiziellen Katalog gelistet,
`fap_category="Tools"` in `application.fam`), Bau lokal über `./fbt launch_app
APPSRC=protoview` innerhalb des eigenen Firmware-Baums nötig. Mit dem unten gelieferten Fix: drei
additive Patches, keine Firmware-Änderung, kein neuer Submodul-Pin.

## API-87-Status

Gegen eine frische, blobfreie `G4MEOVER18/G4MEOVER-FW`-Zweitkopie (`main`, Commit `b359f6f`)
geprüft. Alle von ProtoView genutzten `furi_hal_gpio_*`/`furi_hal_interrupt_set_isr`/
`furi_hal_power_suppress_charge_*`/`furi_hal_cortex_instructions_per_microsecond`-Symbole sowie
acht der neun genutzten `furi_hal_subghz_*`-Funktionen (`_reset`/`_idle`/`_rx`/`_sleep`/
`_is_frequency_valid`/`_set_frequency_and_path`/`_start_async_rx`/`_stop_async_rx`/
`_start_async_tx`/`_stop_async_tx`/`_is_async_tx_complete`/`_flush_rx`/`_load_custom_preset`)
byteidentisch vorhanden — **mit einer Ausnahme**.

## Fund: `furi_hal_subghz_load_preset()` existiert in meiner Firmware nicht mehr

`app_subghz.c::radio_begin()` wählt pro Modulation zwischen zwei CC1101-Preset-Pfaden: fünf der
neun Einträge in `ProtoViewModulations[]` (TPMS 1-3, OOK/FSK 40kBaud) liefern ein eigenes
Rohregister-Array über das unverändert vorhandene `furi_hal_subghz_load_custom_preset()`; die
übrigen vier (die beiden Stock-OOK- und beiden Stock-2FSK-Presets) rufen stattdessen
`furi_hal_subghz_load_preset(FuriHalSubGhzPreset preset)` — eine Enum-zu-Register-Komfortfunktion,
die in meiner Firmware komplett entfernt ist (volltextlich in `targets/f7/furi_hal/
furi_hal_subghz.h` bestätigt: nicht vorhanden). Grund: meine Firmware hat die
Geräteabstraktionsschicht `lib/subghz/devices/` (für austauschbare externe SubGhz-Module)
eingeführt; dieselbe Enum (`FuriHalSubGhzPreset`, inkl. aller vier bei ProtoView genutzten Werte)
existiert weiterhin unverändert in `lib/subghz/devices/preset.h`, wird jetzt aber nur noch über
`subghz_devices_load_preset(const SubGhzDevice*, FuriHalSubGhzPreset, uint8_t*)` aus `lib/subghz/
devices/devices.h` aufgelöst — exakt dieselbe Registertabellen-Zuordnung wie die alte Funktion,
nur um einen zusätzlichen Geräte-Zeiger-Parameter erweitert (bestätigt per Volltextvergleich
gegen `lib/subghz/devices/cc1101_int/cc1101_int_interconnect.c::
subghz_device_cc1101_int_interconnect_load_preset()`: `case`-für-`case` identisch zu dem, was
die alte Funktion intern tat). Konsequenz ohne Fix: `./fbt launch_app APPSRC=protoview` bricht am
Link (`undefined reference to furi_hal_subghz_load_preset`) ab, sobald eine der vier Stock-Presets
im Code-Pfad ist — reproduzierbar bereits beim Standardfall (Modulation-Index 0 = "OOK 650Khz",
der App-Default), kein Rand- oder Sonderfall. Kein eigener Logikfehler von ProtoView, reine
Firmware-API-Historie, analog zum bereits dokumentierten Evil-Portal-UART-Fall
(`furi_hal_uart_*` statt `furi_hal_serial_*`) — nur hier eine einzelne Funktion statt einer
ganzen Teilschicht.

## Fix

Additiv, drei Dateien, kein Verhaltensunterschied für die fünf bereits funktionierenden
Custom-Presets:

1. `app.h`: zwei neue Includes (`lib/subghz/devices/devices.h`,
   `lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h` — derselbe Include-Pfad, den auch
   die offizielle `applications/main/subghz/subghz_cli.c` für dieselbe Aufgabe nutzt, kein
   interner/instabiler Header).
2. `app_subghz.c::radio_begin()`: `furi_hal_subghz_load_preset(preset)` →
   `subghz_devices_load_preset(subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME), preset,
   NULL)`.
3. `app.c`: `subghz_devices_init()` einmalig in `protoview_app_alloc()` (das Geräte-Registry ist
   bei einer eigenständigen `.fap` wie ProtoView nicht garantiert bereits initialisiert, anders
   als innerhalb der Haupt-Subghz-App) und das passende `subghz_devices_deinit()` in
   `protoview_app_free()`, nach `radio_sleep(app)` — Lebensdauer exakt an die App gebunden, analog
   zum bereits vorhandenen `furi_hal_power_suppress_charge_enter()`/`_exit()`-Paar.

Patches gegen zwei frische, unabhängige Zweitkopien von `antirez/protoview`
(`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`) einzeln und zusammen mit `git apply --check`
verifiziert, real angewendet, alle drei Zieldateien per `md5sum` byte-für-byte abgeglichen —
identisch. Patches in `ideas/protoview-load-preset-removed-api-fix/` (`app_h.patch`,
`app_subghz_c.patch`, `app_c.patch`).

## Host-Repro

`ideas/repro-protoview-load-preset-removed-api/`: die exakte Entscheidungslogik aus
`radio_begin()` zweimal extrahiert (`radio_begin_ALT.c`/`radio_begin_NEU.c`, sonst byteidentischer
Aufbau), gegen eine aus den bestätigten Symbol-Listen gebaute Mock-Headerdatei (`mocks.h`/
`mocks.c`) plus eine byteidentische Kopie der neun-Einträge-`ProtoViewModulations[]`-Tabelle
(`modulations.c`). `./build.sh`:

- ALT gegen einen Mock mit (noch vorhandener) `furi_hal_subghz_load_preset()` (ältere Firmware):
  baut, läuft, 27 Prüfungen bestätigen für alle neun Modulationen den exakt erwarteten
  Aufrufpfad/Enum-Wert/Zeiger — reine Kontrollfluss-Verifikation, dass die ALT-Extraktion
  tatsächlich dem Original entspricht.
- ALT gegen einen Mock ohne diese Funktion (mein eigener Firmware-Stand): Build schlägt wie
  erwartet fehl (`implicit declaration`/undefined reference), **Finding reproduziert**.
- NEU gegen denselben Mock ohne die entfernte Funktion: baut, läuft, 37 Prüfungen bestätigen für
  alle neun Modulationen denselben Enum-Wert/Zeiger wie ALT, nur über den neuen
  Geräte-API-Aufrufpfad (inkl. Geräte-Namensauflösung `"cc1101_int"` vs. Gegenprobe
  `"cc1101_ext"` → `NULL`).

Beide Build-Stufen unter `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
-fno-sanitize-recover=undefined`: **ALL CHECKS PASSED**, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Alle drei Patches gegen die eigene `antirez/protoview`-Kopie anwenden, dann `./fbt launch_app
APPSRC=protoview` innerhalb des eigenen `G4MEOVER-FW`-Baums — vorher bricht derselbe Befehl exakt
mit dem oben reproduzierten Linkfehler ab. Nach dem Fix: App startet, Default-Ansicht zeigt den
Rohpuls-Bildschirm für Modulation 0 ("OOK 650Khz") ohne Absturz/Fehlermeldung — das war vor dem
Fix der erste erreichbare Codepfad, der den Build überhaupt verhindert hätte. Funktionstest:
eigenes 433-MHz-Signal (z. B. eigene TPMS-Reifendrucksensor-Attrappe oder eigene
SubGHz-Fernbedienung) mit Modulation "OOK 650Khz" empfangen, Rohpulse müssen im Zeitdiagramm
erscheinen; TPMS-1/2/3-Presets (unverändert über `furi_hal_subghz_load_custom_preset()`) als
Regressionsprobe gegenprüfen — müssen weiterhin wie vor dem Fix funktionieren.

## Risiko

Rein additiv, keine Firmware-Änderung, kein neuer Submodul-Pin. Sicherheitsrahmen: reine
Signal-*Analyse*-/Edit-Funktion auf bereits vorhandene Flipper-SubGhz-Sendefunktion aufgesetzt,
kein neuer Sende-/Jamming-Mechanismus — dieselben regulatorischen Grenzen wie beim Senden mit der
Stock-SubGhz-App oder RollForge gelten unverändert (`furi_hal_subghz_is_frequency_valid()`/
Regionssperre bleiben in meiner Firmware vollständig aktiv, von ProtoView nicht umgangen).

## Fortsetzung: Logikprüfung der vier nativen Protokoll-Decoder

Dieser Fund deckte ausschließlich die API-87-Kompatibilität ab, nicht die vier nativ
unterstützten Protokoll-Decoder selbst (TPMS Renault/Toyota/Schrader/Citroen/Ford,
KeeLoq, Oregon 2, PT2262/SC5262) oder die Bit-Edit-/Keyboard-Ansicht — eigener
Rückstellpunkt für künftige Sitzungen. Oregon 2 (kleinster der vier) jetzt
zeilenweise durchgesehen: zwei unabhängige Logikfehler inkl. Fix, Details in
`ideas/protoview-oregon2-decoder-logic-fixes.md`. B4B1 (PT2262/SC5262) jetzt
ebenfalls durchgesehen: Sync-Suche durchläuft durch eine zu niedrige Schleifengrenze
nur drei der sechs deklarierten Präambel-Muster, Details und Fix in
`ideas/protoview-b4b1-decoder-sync-loop-bound-fix.md`. KeeLoq (vierter und letzter der
vier nativ unterstützten Decoder) jetzt ebenfalls durchgesehen: Längen-Vorab-Check ist
durch sich aufhebende Addition/Subtraktion ein No-Op, wodurch Nachrichten teilweise aus
Speicher jenseits des tatsächlich empfangenen Signals zusammengesetzt werden können,
Details und Fix in `ideas/protoview-keeloq-decoder-length-check-bypass-fix.md`.
Vom TPMS-Cluster jetzt die kleinste Datei (Citroen) durchgesehen: eigener,
unabhängiger Fund derselben Fehlerklasse (Vorab-Check um Faktor 2 zu klein und am
falschen Zeitpunkt ausgewertet), nachweislich auch in Ford/Renault/
Schrader-EG53MA4 vorhanden, Details und Fix in
`ideas/protoview-citroen-tpms-decoder-length-check-fix.md`. Restlicher
TPMS-Cluster (Ford/Renault/Schrader/Schrader-EG53MA4/Toyota) sowie die
Bit-Edit-/Keyboard-Ansicht weiterhin offen.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05). Firmware-Vergleichsstand: `G4MEOVER18/G4MEOVER-FW`, `main`, Commit
`b359f6fc32f282ce7217dd2d48a4e3478771852e`.
