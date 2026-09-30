# G4MEOVER Fleet — Befehls- & Fähigkeits-Matrix

Autoritative Übersicht, welcher `ukfe_rf`-Befehl auf welchem Gerät läuft und in welchem Zustand.
Stand: 2026-08-18 (v1-Protokoll/-Matrix, weiterhin live).

> **Vorausschau v2:** [`UKFE-RF-V2-SPEC.md`](UKFE-RF-V2-SPEC.md) entwirft eine Frame-Erweiterung
> (Source-/Origin-ID, BLE-Transport, neue Commands `0x60`–`0x70`) für Masterplan-Phase 0/2b/8.
> Entwurfsstatus, noch nicht live — diese Matrix bleibt bis zur lokalen Umsetzung (P0.4f) gültig.

**Status-Legende:** ✅ live (auf Hardware verifiziert) · 🔨 in PR (gebaut & kompiliert, wartet auf Flash-Test) · 📋 geplant (nur Enum, kein Handler)

## Transporte
| Transport | Strecke | Träger |
|---|---|---|
| **868-FSK** | Flipper (CC1101) ↔ Heltec V3/V4 (SX1262) | störfreier Steuerkanal, unabhängig von 2,4 GHz |
| **ESP-NOW** | WROOM-Relay / Heltec-Hub → Satelliten (LilyGo, Heltec) | 2,4 GHz Broadcast, signiert |
| **UART** | Flipper GPIO ↔ Heltec-Hub / WROOM | Kabel, `rf_comm_build_frame` |

Alle Transporte tragen denselben signierten Frame: `[LEN][0x47][0x01][COUNTER(4)][CMD][ALEN][ARGS][MAC4][CRC16(2)]` (keyed MAC + CRC16 + Rolling-Counter).

## Geräte-Fähigkeiten
| Gerät | 868 | ESP-NOW | WiFi-Angriff | BLE | USB-HID | GPS | Display |
|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| **Heltec V3** (`heltec-ukfe-rx`) | ✅ | ✅ | ✅ | ✅ | ✅ | – | OLED |
| **Heltec V4-Hub** (`heltec-v4-hub`) | ✅ | ✅ | – | – | – | ✅ | OLED |
| **LilyGo S3** (`lilygo-ukfe-rx`) | – | ✅ | ✅ | ✅ | ✅ | – | TFT |
| **WROOM-Relay** (`g4meover-wifi-relay`) | – | ✅ (Koordinator) | – | – | – | – | – |

> **Vorausschau Phase 9 (WROOM Multi-Rolle):** `ecosystem/devices.json` führt `esp32-wroom` bereits
> mit `transport: [espnow, wifi, ble]` — die Hardware kann mehr als die Zeile oben zeigt, es fehlt
> nur der Rollen-Umschalt-Befehl (Vorschlag `0x62 RoleSet` im `0x60`–`0x7F`-v2-Block, siehe
> `G4MEOVER-ECOSYSTEM-MASTERPLAN.md` P9.1b). Diese Tabelle bekommt eine eigene WROOM-Spalte erst,
> wenn dieser Befehl tatsächlich existiert.

## Befehls-Matrix
| Code | Befehl | Args | Heltec V3 | LilyGo | Notiz |
|---|---|---|:--:|:--:|---|
| 0x01 | Status | – | 📋 | 📋 | Status-Response geplant |
| 0x02 | Trigger | id, delay | ✅ | ✅ | HID-Payload auslösen |
| 0x03 | Abort | – | 🔨 | 🔨 | stoppt BLE-Spam (BLE-PR) |
| 0x05 | PayloadRun | idx | ✅ | ✅ | HID-Payload nach Index |
| 0x10–0x16 | LoRa/SubGHz | div. | 📋 | 📋 | LoraScan/Tx/Jammer/RollForge… |
| 0x20 | WifiScan | – | 🔨 | 🔨 | AP-Scan → Serial |
| 0x21 | WifiDeauth | bssid, ch | 🔨 | 🔨 | Broadcast-Deauth, Kanal-Hopping |
| 0x22 | WifiStop | – | 🔨 | 🔨 | stoppt WiFi-Angriff + Portal |
| 0x23 | EvilPortal | portal_id | 🔨 | – | SoftAP + Captive-DNS + Login-Harvest |
| 0x24 | BeaconSpam | mode | 🔨 | 🔨 | Fake-APs, Kanal-Hopping |
| 0x25 | Handshake | bssid, ch | 🔨 | – | WPA-EAPOL-Capture (+Deauth) |
| 0x26 | Wardrive | – | 🔨 | – | AP-Scan → WiGLE-CSV (V3 ohne GPS) |
| 0x27 | ProbeSniff | – | 🔨 | – | Probe-Requests → SSID/Client-MAC |
| 0x28 | Karma | – | 📋 | – | Probe→Fake-AP-Antwort |
| 0x29 | PacketMon | – | 🔨 | – | Promiscuous-Paketstatistik |
| 0x2A | Pwnagotchi | – | 🔨 | – | Pwnagotchi-Beacon-Detektor |
| 0x30 | BleScan | dur | 🔨 | 🔨 | aktiver BLE-Scan |
| 0x31 | BleSpam | mode | 🔨 | 🔨 | Apple/Windows/Android, Zufalls-MAC |
| 0x32 | SourApple | – | 🔨 | 🔨 | Apple-only Proximity-Spam |
| 0x33 | BleSniff | – | 📋 | 📋 | passiver BLE-Logger |
| 0x40–0x41 | GPS | – | – | – | am V4-Hub (`heltec-v4-hub`) geplant |
| 0x50 | HidPayload | idx | ✅ | ✅ | benannte HID-Payload |
| 0x51 | HidDucky | script_id | 📋 | 📋 | DuckyScript geplant |

## Offene Pull Requests (warten auf Flash-Test)
| Repo | PR | Inhalt |
|---|---|---|
| `G4MEOVER-FW` | #2 | Firmware v2.0.0 (Momentum mntm-012 Rebrand) |
| `heltec-ukfe-rx` | #1 | WiFi: Deauth/Beacon/Scan/Stop + EvilPortal (+ Recon 0x25–0x2A) |
| `heltec-ukfe-rx` | #2 | BLE: Spam/SourApple/Scan + Enum-Sync |
| `lilygo-ukfe-rx` | #1 | WiFi: Deauth/Beacon/Scan/Stop |
| `lilygo-ukfe-rx` | #2 | BLE: Spam/SourApple/Scan + Enum-Sync |

**Merge-Reihenfolge je Repo:** erst WiFi-PR (#1), dann BLE-PR (#2) — beide ändern `act()`.

Test-Anweisungen: siehe [`TEST-PLAN.md`](TEST-PLAN.md).
