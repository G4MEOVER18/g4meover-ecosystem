# G4MEOVER Fleet — Flash-Test-Plan

Konkrete Test-Aufträge für die offenen PRs. Alles kompiliert grün, aber **nichts ist auf Hardware verifiziert** — diese Tests schließen die Lücke. Nach bestandenem Test wird der jeweilige PR gemergt.

**Ports (aus Memory, bitte bestätigen):** Heltec V3 = `COM26` · WROOM-Relay = `COM8` · Flipper = `COM3` · LilyGo = `COM?`

> ⚠️ **Satelliten zum Flashen vom Flipper trennen** (UART-Querverdrahtung blockiert sonst esptool). Einzeln per USB flashen.

---

## T1 — Firmware v2.0.0 (`G4MEOVER-FW` #2)  🔴 höchste Priorität
**Gerät:** Flipper Zero
**Bauen/Flashen:** Branch `release/2.0.0` → `./fbt COMPACT=1 DEBUG=0 updater_package`, dann per qFlipper/SD flashen.
**Prüfen:**
- [ ] Build fehlerfrei
- [ ] Boot-Sequenz + Passport zeigen G4MEOVER-Branding
- [ ] Hauptmenü-Reihenfolge + Power-Keybinds (▲RollForge ▼FlipperJammer ▶ProtoPirate)
- [ ] NFC erweitertes MIFARE-Dict lädt · SubGHz extend_range aktiv · MTP funktioniert
→ bei OK: **merge PR #2** + Release v2.0.0

---

## T2 — WiFi-Handler Heltec V3 (`heltec-ukfe-rx` #1)
**Gerät:** Heltec V3 (`COM26`), standalone.
**Bauen/Flashen:** `git checkout feature/wifi-attack-handlers && pio run -t upload --upload-port COM26`
**Prüfen (Befehle via Flipper 868 ODER WROOM-CLI `ping`/`hex`):**
- [ ] Boot OK, OLED „Bereit", 868 + ESP-NOW empfangen weiter
- [ ] **WifiScan (0x20)** → APs erscheinen im Serial-Monitor, danach wieder empfangsbereit
- [ ] **WifiDeauth (0x21)** gegen **eigenen** Test-AP → Clients verlieren Verbindung
- [ ] **WifiStop (0x22)** über 868 → Angriff endet **sofort**, ESP-NOW kehrt zurück
- [ ] **BeaconSpam (0x24)** → Fake-SSIDs sichtbar, Auto-Stop nach 20 s
- [ ] **EvilPortal (0x23)** → SSID „Free WiFi" erscheint, Handy verbindet → Login-Seite, Eingabe landet im Serial; `WifiStop` beendet Portal
- [ ] **Recon 0x25–0x2A** (falls schon im PR): Handshake/ProbeSniff/PacketMon/Pwnagotchi → jeweils Serial-Ausgabe
→ bei OK: **merge PR #1 zuerst**

## T3 — BLE-Handler Heltec V3 (`heltec-ukfe-rx` #2)
**Bauen/Flashen:** `git checkout feature/ble-handlers && pio run -t upload --upload-port COM26`
**Prüfen:**
- [ ] Boot OK, 868 + ESP-NOW weiter
- [ ] **BleScan (0x30)** → Geräte im Serial
- [ ] **BleSpam (0x31)/SourApple (0x32)** → Pairing-Popups auf Test-iPhone/-Android/-Windows
- [ ] **Abort (0x03)** stoppt Spam sauber
→ bei OK: **merge PR #2 nach #1** (evtl. triviale `act()`-Konfliktauflösung)

---

## T4 — WiFi-Handler LilyGo (`lilygo-ukfe-rx` #1)
**Gerät:** LilyGo T-Dongle S3, standalone.
**Bauen/Flashen:** `git checkout feature/wifi-attack-handlers && pio run -t upload`
**Prüfen:**
- [ ] Boot OK, TFT „BEREIT", ESP-NOW empfängt
- [ ] WifiScan/Deauth/BeaconSpam wie T2 (⚠️ **kein Stop mid-attack** — nur 20-s-Timeout, LilyGo hat keinen 868-Kanal)
- [ ] USB-HID/Trigger unverändert
→ bei OK: **merge PR #1**

## T5 — BLE-Handler LilyGo (`lilygo-ukfe-rx` #2)
**Bauen/Flashen:** `git checkout feature/ble-handlers && pio run -t upload`
**Prüfen:**
- [ ] BleScan/BleSpam/SourApple wie T3
- [ ] **Abort (0x03)** greift **sofort** (BLE koexistiert mit ESP-NOW — Vorteil ggü. WiFi-Deauth)
→ bei OK: **merge PR #2 nach #1**

---

## Rückmeldung
Pro Test: ✅/❌ + bei ❌ die Serial-Ausgabe. Ich merge nach OK, fixe bei ❌ und unterstütze beim Flashen/Debuggen.
Fähigkeits-Überblick: [`FLEET-COMMANDS.md`](FLEET-COMMANDS.md).
