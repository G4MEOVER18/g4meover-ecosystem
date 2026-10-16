# Sentry Safe Opener (H4ckd4ddy) — elektronischer Tresor-Bypass über GPIO

## Quelle

`github.com/H4ckd4ddy/flipperzero-sentry-safe-plugin`, MIT-Lizenz (`LICENSE`-Datei im Repo). App-ID
`gpio_sentry_safe`, `fap_version="2.0"`, `fap_category="GPIO"`. Einzige Quelldatei `sentry_safe.c`
(333 Zeilen) vollständig gelesen, dazu `application.fam` und die Begleit-Dokumentation
(`README.md`) herangezogen. Separates Schwester-Repo des Autors (`H4ckd4ddy/bypass-sentry-safe`)
dokumentiert die eigentliche Schwachstelle (Credits an "ArsLock" im App-Hilfetext). Bereits Teil
der Standard-App-Sammlungen von Unleashed und Xtreme, dort unter `documentation/SentrySafe.md`
gelistet — kein experimenteller Nischencode.

## Was

Nutzt eine bekannte Schwachstelle in elektronischen Sentry-Safe- und Master-Lock-Tresorschlössern:
deren Tastatur-Platine kommuniziert mit der Schlossmechanik über eine einfache serielle Leitung
(4800 Baud, LPUART-kompatibel), die am Erweiterungsport des Flipper über zwei Krokodilklemmen
(GND + Signal) abgegriffen wird. Die App sendet dort direkt — ohne den PIN-Code der Tastatur zu
kennen — ein Kommando, das den hinterlegten Primär- oder Sekundärcode durch einen selbst
gewählten 5-stelligen Code überschreibt, gefolgt vom eigentlichen Entsperr-Kommando. Jedes Senden
beginnt mit einem kurzen Low-Puls auf der Leitung (Weck-/Sync-Signal vor dem eigentlichen
8-Byte-Rahmen: `0x00`, Kommando, 5 Code-Ziffern, Prüfsumme). UI: Fünf Ziffern + P/S-Auswahl über
die Pfeiltasten, Senden per OK, Code auf `00000` setzen löscht den hinterlegten Code.

## Nutzen

Bisher komplett fehlende Kategorie im eigenen Katalog: physischer Bypass eines elektronischen
Schlosses über eine freiliegende interne Steuerleitung, nicht über Funk/NFC/RFID. Ergänzt die
bestehende "Physical Access Control"-Kategorie (Wiegand Reader/H10301 Writer, dort Kartenleser-
seitig) um die Schloss-/Aktuator-Seite — relevant für autorisierte physische Pentests (z. B. Prüfung
eigener Büro-/Hotelsafes auf genau diese bekannte, seit Jahren öffentliche Schwachstelle, bevor sie
produktiv eingesetzt werden). Reine Software-Portierung ohne Firmware-Patch, Angriffsfläche ist
ausschließlich die bereits öffentlich dokumentierte Tresor-Elektronik, nicht der Flipper selbst.

## Aufwand

Sehr gering. Eine einzelne, kompakte Quelldatei (333 Zeilen), Standard-GUI-/Input-/Canvas-API,
keine Submodule, keine Firmware-Änderung. Benötigt am Zielgerät physischen Zugriff auf die
Tastatur-Platinen-Innenseite des Tresors (Gehäuse öffnen) sowie zwei Drähte (GND/C1) zum
Erweiterungsport — kein zusätzliches Flipper-Zubehör.

## API-87-Status

✅ Kompatibel, geprüft. Frischer, unabhängiger Checkout von `G4MEOVER18/G4MEOVER-FW` (Tag `v2.0.0`,
Commit `b359f6f`, `mntm-012`) gegen den tatsächlich genutzten Symbolsatz abgeglichen:

- `targets/f7/furi_hal/furi_hal_resources.c`: `gpio_ext_pc1` byteidentisch vorhanden — und zugleich
  (Zeile 71/471 in `furi_hal_serial.c`) bereits intern als TX-Pin für `FuriHalSerialIdLpuart`
  hinterlegt. Der App-eigene Wechsel zwischen rohem GPIO-Modus (Weck-Puls) und UART-Peripherie-
  Modus (eigentlicher Rahmen) auf **demselben** physischen Pin ist damit kein Bug, sondern
  exakt das von der Firmware vorgesehene Pin-Mapping für dieses Protokoll.
- `targets/f7/furi_hal/furi_hal_gpio.h`: `furi_hal_gpio_init_simple()`/`furi_hal_gpio_write()`/
  `furi_hal_gpio_init()` byteidentisch vorhanden.
- `targets/f7/furi_hal/furi_hal_serial.h` + `furi_hal_serial_control.h`:
  `furi_hal_serial_control_acquire()`/`_release()`, `furi_hal_serial_init()`/`_deinit()`/`_tx()`
  byteidentisch vorhanden (dieselben bereits bei UART Sniff/RS485 Modbus/SWD Probe verifizierten
  Signaturen).
- `applications/services/expansion/expansion.h`: `RECORD_EXPANSION`/`expansion_disable()`/
  `_enable()` byteidentisch vorhanden — die App ruft beide korrekt um den GPIO-Zugriff herum auf
  (anders als der früher gefundene CAN-Commander-Fall), kein Erweiterungsport-Konfliktrisiko.
- Standard-GUI-/Input-/Canvas-API (`ViewPort`, `FuriMessageQueue`, `FuriMutex`) unverändert, keine
  Besonderheit gegenüber bereits portierten GPIO-Apps.

## Code-Beobachtung (kein bestätigter Fund)

In `sentry_safe_app()` wird beim ersten kurzen OK-Druck (Status `idle → sending`) das
`state->mutex` einmal explizit vor dem blockierenden Sende-/Wartepfad freigegeben (um dem
GUI-Thread das Zeichnen von "Sending…" während der ~700 ms Blockierzeit zu erlauben), danach aber
am Ende desselben Schleifendurchlaufs ein zweites Mal unbedingt freigegeben — ein doppeltes
`furi_mutex_release()`. Gegen `furi/core/mutex.c` der eigenen `mntm-012`-Basis geprüft:
`furi_mutex_release()` ruft für einen Nicht-rekursiven Mutex lediglich `xSemaphoreGive()` auf; ein
zweiter Aufruf auf einem bereits freigegebenen Mutex liefert dort schlicht `errQUEUE_FULL`
zurück (Mutex-Queue ist bereits voll) — kein `furi_check()`/`furi_crash()` auf diesem Pfad, der
Rückgabewert wird im Original-Code ohnehin nicht ausgewertet. Es ist damit **kein** reproduzierbarer
Crash oder Fehlverhalten entstanden (anders als bei den bisherigen, per Host-Repro bestätigten
Funden) — daher hier nur als Beobachtung vermerkt, kein Fix-Vorschlag mit Host-Repro.

## Testhinweis

Lokal zu tun: App unverändert gegen die eigene `mntm-012`-Basis mit `ufbt`/`fbt` bauen (reiner
Compile-Test, keine Logikänderung nötig). Funktionstest ausschließlich am **eigenen** Sentry-Safe-
oder Master-Lock-Tresor (autorisierter Eigentest): Gehäuse öffnen, GND/C1 wie im Hilfebildschirm der
App an die Tastatur-Platine anklemmen, Zielcode einstellen, OK drücken, Tresor sollte piepen und
öffnen. Kein Hardware-Eigenbau nötig (reine Zwei-Draht-Klemmverbindung).

## Sicherheitsrahmen

Ausschließlich zur autorisierten Prüfung eigener oder im Rahmen eines Auftrags freigegebener
Tresore gedacht (Nachweis einer seit Jahren öffentlich dokumentierten Schwachstelle, bevor ein
solches Schloss produktiv für Werttransport/-lagerung genutzt wird) — passt in den defensiven
Rahmen aus `CONSTRAINTS.md` Punkt 5. Kein Massen-/Fernangriff möglich: erfordert in jedem Fall
physischen Zugriff auf das offene Gehäuseinnere des jeweiligen Tresors.
