# Benutzer-Handbuch — Gliederung (P12.3a)

Reine Kapitelstruktur, noch ohne Inhalt. Inhalt wird erst befüllt, sobald die jeweiligen
Cockpit-Tabs (Phase 1) real funktionieren — ein vorab geschriebener Inhaltstext würde sonst
Platzhalter statt echtem Verhalten dokumentieren. Quelle für jedes Kapitel ist jeweils ein
bestehendes, versioniertes Artefakt (kein zweites, separat gepflegtes Verzeichnis).

## 1. Einleitung & Sicherheitsrahmen
- Zielgruppe: ich selbst als Projektinhaber, autorisierte Pentests/CTF.
- Verweis auf `CONSTRAINTS.md` (verbindliche Regeln) statt Duplikat.

## 2. Geräte-Setup
Ein Unterkapitel pro Eintrag aus `ecosystem/devices.json` (`devices[].id`), Reihenfolge wie dort:
1. Flipper Zero (`flipper`)
2. Heltec V4 Hub (`heltec-v4-hub`)
3. Heltec V3 (`heltec-v3`)
4. ESP32 WROOM (`esp32-wroom`)
5. LilyGo T-Display (`lilygo-t-display`)
6. DogyTag (`dogytag`)
7. CC2531 Zigbee-Dongle (`cc2531-zigbee`)
8. Raspberry Pi 4 (`raspberry-pi-4`)
9. LORIX One Gateway (`lorix-one-gw`)
10. *Geplant (Phase 11), Kapitel erst bei Status `active`:* ESP32-CAM, RA08, CubeCell HTCC-AB01, HackRF One

Je Kapitel: Anschluss/Port (`last_known_port`/`usb_id`/`mac`), Ersteinrichtung, Verifikationsschritt
aus Masterplan P0.2c (derselbe Befehl, nicht neu aufschreiben).

## 3. Cockpit-Bedienung je Tab
Ein Unterkapitel pro Tab aus Phase 1 (Abschnitt 4 des Masterplans), in Bau-Reihenfolge:
1. Geräte-Tab (P1.3) — Live-Liste, Status
2. Flipper-Tab (P1.4) — Katalog + Raw-Command
3. Heltec/V4-Tab (P1.5) — Befehlskatalog (Quelle: `FLEET-COMMANDS.md`-Matrix) + Raw-Frame-Sender
4. Log-Panel (P1.6) — Herkunftsgerät, Zeit, Inhalt
5. *Sobald vorhanden:* LoRaWAN/DogyTag-Tab (Phase 4/5), Karten-Tab (Phase 6), Szenario-Visualizer
   (Phase 7), Zigbee-Tab (Phase 10)

Je Kapitel nur real existierende Bedienelemente beschreiben (kein Soll-Verhalten vorwegnehmen).

## 4. Szenario-Handbuch (P12.3b)
Eins-zu-eins die vier Presets aus `ecosystem/topology.json` (Feld `presets`), keine zweite,
separat gepflegte Szenario-Liste:
1. `flipper-to-ttn-relay` — Flipper → V4 → ESP-NOW → V3 → TTN
2. `kali-direct-flipper` — Kali-GUI → Flipper (USB-CLI, ohne V4)
3. `apk-ble-to-fleet` — APK → V4 (BLE) → ESP-NOW-Flotte
4. `dogytag-lorawan-uplink` — DogyTag → TTN (LoRaWAN-Uplink, additiv/read-only)

Je Preset: beteiligte Geräte (Kapitel-2-Verweis), genutzter Tab (Kapitel-3-Verweis), erwartetes
Ergebnis/Anzeigeort.

## 5. Befehlsreferenz
Verweis statt Duplikat: `FLEET-COMMANDS.md` (v1-Matrix) + `UKFE-RF-V2-SPEC.md` (v2-Erweiterung,
sobald live). Kein eigener dritter Befehlskatalog im Handbuch.

## 6. Troubleshooting
Bekannte Stolpersteine, nur sammeln, sobald sie real auftreten (z. B. Heltec-V4-Native-USB-Replug
nach esptool-Reset, siehe `CONSTRAINTS.md`/AUTO-FORTSETZUNG Regel 4) — kein spekulativer Eintrag.

---
*Status: Gliederung steht (P12.3a). Inhalt folgt kapitelweise, sobald der jeweilige Tab/Preset real
funktioniert — siehe `G4MEOVER-ECOSYSTEM-MASTERPLAN.md` Abschnitt 4/5.*
