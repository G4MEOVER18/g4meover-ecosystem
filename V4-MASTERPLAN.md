# G4MEOVER V4 — „One Piece"

**Vision:** Der Heltec V4 wird das **eine Board, das alles kann** — besser als Marauder, USB Army Knife, Flipper-Satelliten einzeln. Direkt am Flipper (GPIO), mit eigenem Display; der Flipper ist Fernbedienung + SD-Speicher. Die anderen Boards (V3, LilyGo) sind sekundäre Ausführungspunkte.

## Hardware (V4 = ESP32-S3 + SX1262)
- **Funk:** WiFi 2.4G · BLE · SX1262 (LoRa + SubGHz-FSK/OOK)
- **GPS:** eingebautes GNSS **+ sekundäres GPS** über weitere Pins
- **I/O:** OLED (Status/Menü) · native USB (HID/BadUSB) · GPIO-UART zum Flipper
- **Flipper:** App-Steuerung + **SD** (Payloads, Config, Logs)

## Capability-Ziel (was das One Piece können soll)
| Bereich | Funktionen | Status |
|---|---|---|
| **WiFi (Marauder-Klasse)** | Scan · Deauth · Beacon-Spam · Handshake · Probe-Sniff · Karma · PacketMon · Pwnagotchi · Wardrive | Module vorhanden (V3), Port auf V4 offen |
| **Evil Twin** | Rogue-AP + Captive-Portal + Login-Harvest | Modul `evil_portal` vorhanden, Port offen |
| **Evil Repeater** | Ziel-AP klonen + Clients umleiten (MITM-Repeater) | **neu** |
| **Deauther** | gezielt/broadcast, Kanal-Hopping | in `wifi_attack` |
| **Jammer** | 2.4G (Deauth-Flood) + SubGHz (SX1262) + konfigurierbar | teilweise (SubGHz-Jammer neu) |
| **Mesh** | ESP-NOW-Mesh: V4 koordiniert V3/LilyGo/weitere | Broadcast vorhanden, echtes Mesh neu |
| **BLE** | Scan · Spam (Apple/Android/Samsung/Windows) · Sour Apple · Sniff | Modul `ble_spam` vorhanden, Port offen |
| **SubGHz/LoRa** | TX/RX · Scan · Replay · Jammer | Grundlage vorhanden |
| **LoRaWAN** | echtes LoRaWAN (OTAA/ABP) · **LoRaWAN-Emulator** (End-Node simulieren) · **LoRa-MQTT-Sniffer** (Uplinks mitlesen→MQTT) · **LoRa-Spammer** (LoRa/LoRaWAN-Traffic fluten) · Anbindung **Lorix One** (Outdoor-Gateway) als Ökosystem-Node | **neu, Phase 4** |
| **GPS** | primär + sekundär, Wardrive-Tagging | primär vorhanden, sekundär neu |
| **HID/BadUSB** | DuckyScript von Flipper-SD → streamen → tippen | **neu (Payload-Streaming)** |
| **Display** | OLED zeigt Live-Status/Aktion/Funk/Ergebnis | Grundmenü vorhanden |

## Steuerung (Flipper-App „USB Army Knife FE")
- Vollständiger Katalog aller obigen Funktionen, **pro Menüpunkt Zielgerät gekennzeichnet** (`[V4]`, `[V4·V3]`, `[HID]`).
- **Payload-Browser:** liest DuckyScripts von der Flipper-SD, sendet an ein HID-Gerät.
- Transport: App → GPIO-UART → V4 (führt aus + relayed per ESP-NOW an Satelliten).

## Build-Phasen
1. **V4-Executor** — WiFi/BLE/Recon/EvilPortal-Module in die V4-Firmware, `act()`-Dispatch, OLED-Feedback, weiterhin ESP-NOW-Relay. *(begonnen)*
2. **App-Katalog** — Geräte-Labels, Vollständigkeit, klare Struktur.
3. **Payloads** — SD-DuckyScript-Browser + Streaming-Protokoll (chunked ukfe_rf) + DuckyScript-Interpreter auf dem V4.
4. **Neue Angriffe + LoRaWAN** — Evil Repeater, SubGHz-Jammer, echtes ESP-NOW-Mesh, sekundäres GPS, LoRaWAN (echt/Emulator), LoRa-MQTT-Sniffer, Lorix-One-Anbindung.
5. **Politur** — OLED-UX, Logs auf SD, Autorisierungs-/Safety-Guards.

> Nur für autorisierte Security-Tests / eigene Hardware / CTF.
