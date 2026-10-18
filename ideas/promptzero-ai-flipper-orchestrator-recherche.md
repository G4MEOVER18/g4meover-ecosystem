# Ideen-Recherche: PromptZero — Sprachgesteuerter Flipper-Orchestrator (PC-Tool)

Gezielt nach einer Ergänzung für die „Eigene Kreationen"-Rubrik gesucht: die geplante
`Vault`/`Capture-Dedup-Indexer`-Linie (siehe `ideas/companion-tool-recherche.md`) deckt die
Datei-/Capture-Verwaltung ab, aber keine Kommandoebene oberhalb der einzelnen
`g4meover-deck`-Module (`flipper_link.py`, `hackrf_link.py`, `lorix_link.py`, `zigbee_link.py`,
`wpa_crack.py` — jedes für sich bereits geprüft/gepatcht, siehe jeweilige
App-Verbesserungs-Einträge). Nach einem existierenden Baustein für genau diese Lücke gesucht,
statt sie selbst zu entwerfen.

## Fund: PromptZero (xunholy/promptzero)

**Quelle:** github.com/xunholy/promptzero, AGPL-3.0-or-later, PC-seitiges CLI-Tool (Go), kein
Flipper-FAP. Reifegrad: 246 Sterne, 2347+ Commits, aktive Entwicklung, dokumentiertes
Sicherheitsmodell samt eigenem Adversarial-Test-Verzeichnis.

**Was:** Natürlichsprachlicher Operator für den Flipper Zero — verbindet sich per USB-Seriell
oder BLE mit dem Gerät (optional zusätzlich ein ESP32-Marauder-WiFi-Devboard über USB-CDC-ACM,
Firmware ≥ v1.11.1) und übersetzt Texteingaben in konkrete Aktionen über ein Register von
laut eigener Doku 700+ Einzel-Tools: Geräte-/Energie-Info, SubGHz (RX/TX/Bruteforce/Chat),
Infrarot (RX/TX/Bruteforce, NEC/Samsung32/RC6/SIRC), NFC (Detect/Dump/Emulate/APDU/Mifare-
Ultralight-Schreiben), 125-kHz-RFID (Read/Emulate/T5577-Write), iButton (Dallas/Cyfral/Metakom),
GPIO/I2C, BadUSB (DuckyScript-Validierung + Ausführung), Storage, FAP-Launcher für ~15 externe
Apps (u. a. ProtoView, Signal Generator, MFKey, Mifare Nested, PicoPass, Seader, T5577
Multiwriter, SubGHz Bruteforcer/Playlist, nRF24 Mousejacker) sowie — bei angeschlossenem
Marauder-Board — WiFi-Scan/Deauth/Beacon-Spam/PMKID-Capture/Evil-Portal/Wardriving. Jedes Tool
trägt eine feste Risikostufe (low/medium/high/critical). Als Sprachmodell-Backend nutzt es die
Anthropic-API (Claude, Modellauswahl nach Kostenstufe: schnelleres Modell für Recon, stärkeres
für Exploit-Generierung) — eigener `ANTHROPIC_API_KEY` ist Voraussetzung. Vier Betriebsarten:
CLI-REPL (Standard), Web-UI (`--web`, Port 8080, Token-Auth), Sprachsteuerung (Whisper-
Transkription) und ein MCP-Server (stdio).

**Nutzen für mich:** Direkte Inspiration für eine künftige Kommandoebene über `g4meover-deck`:
statt jedes Modul (Flipper/HackRF/LORIX/Zigbee/WPA-Cracking) einzeln per Skript aufzurufen,
zeigt PromptZero ein funktionierendes Muster für eine natürlichsprachliche Fassade mit
abgestuften Risikostufen pro Aktion — passt zum eigenen Sicherheitsrahmen (defensiv/autorisiert)
besser als ein undifferenzierter "mach alles"-Agent. Als eigenständiges Werkzeug direkt nutzbar:
deckt u. a. SubGHz-Bruteforce/-Chat, NFC-APDU/Mifare-Ultralight-Schreiben und BadUSB-Ausführung
ab, die in `g4meover-deck` bislang nicht orchestriert sind, ohne dass ich das selbst bauen müsste.

**Firmware-/Kompatibilitäts-Status:** ❓→🔧 wahrscheinlich größtenteils lauffähig, nicht
Byte-für-Byte gegen `mntm-012` verifiziert (kein Flipper-C-Quelltext, sondern PC-seitiges
CLI-Tool, das über die serielle Flipper-CLI mit dem Gerät spricht — andere Prüftiefe als ein
Firmware-/App-Port). Zielt laut eigener Doku explizit primär auf **Momentum** (zusätzlich
Unleashed/RogueMaster, offizielle Firmware nur mit reduziertem Funktionsumfang — SubGHz-TX
regionsgesperrt, Rolling-Code-Protokolle blockiert) — exakt meine eigene Firmware-Familie. Zwei
Tools setzen Momentum-spezifische Erweiterungen voraus, die in meinem `mntm-012`-Stand bereits
enthalten sind: `subghz_rx_raw` (nur Momentum/Xtreme/Rogue) und `js_run` (JS-Laufzeit, ebenfalls
nur Momentum/Xtreme/Rogue) — beide als `critical` eingestuft. Keine konkrete Mindest-Buildnummer
dokumentiert; vor dem ersten Einsatz lokal gegen das eigene `v2.0.0`/`mntm-012` prüfen, ob die
von den genutzten Tools erwartete Standard-CLI-Befehlsmenge (Flipper-Kern-CLI, nicht
Momentum-exklusiv) unverändert vorhanden ist.

**Sicherheitsrahmen:** Deckt sich in der Grundhaltung mit meinem eigenen Rahmen — explizites
"Authorised use only", Hinweis, dass erzeugte RF-/NFC-/RFID-/HID-Payloads außerhalb
autorisierter eigener/beauftragter Tests illegal sind, sowie die ausdrückliche Empfehlung,
generierte Payloads vor dem Ausführen selbst zu prüfen. Technische Durchsetzung: High-/
Critical-Tools verlangen im REPL eine Bestätigung (Vorschau von Frequenz/Modulation/Hexdump plus
2-Sekunden-Verzögerung vor einer positiven Bestätigung, Ablehnung sofort wirksam); im MCP-Modus
sind High/Critical standardmäßig verweigert und nur über die expliziten Umgebungsvariablen
`PROMPTZERO_MCP_ALLOW_HIGH`/`PROMPTZERO_MCP_ALLOW_CRITICAL` freischaltbar; ohne initialisiertes
Audit-Log verweigert der Agent High+-Aktionen komplett; zusätzlich ein `--read-only`-Modus, der
jedes schreibende/sendende/ausführende Tool pauschal verweigert. Alle Audit-Events laufen in ein
lokales SQLite-Log mit MITRE-ATT&CK-Tags.

**Risiko:**
- **Lizenz (AGPL-3.0-or-later):** bei eigenem Einsatz als eigenständiger Prozess neben
  `g4meover-deck` unkritisch (reine Nutzung, kein Vendoring); bei etwaiger späterer Übernahme
  von Code-Teilen in `g4meover-deck` würde der Copyleft-/Netzwerk-Nutzungs-Passus greifen — dann
  nur als separater, eigenständig gestarteter Subprozess einbinden, nicht einverleiben.
- **Dokumentationslücke beim REPL-Konsens:** die README belegt nicht, ob die
  2-Sekunden-Bestätigung sich per Flag für unbeaufsichtigten/nicht-interaktiven Betrieb umgehen
  lässt — vor jedem automatisierten (Cron-/Skript-)Einsatz lokal verifizieren, dass High-/
  Critical-Aktionen ohne explizite menschliche Bestätigung niemals auslösen, sonst widerspricht
  das meinem Nicht-destruktiv-/Autorisierungs-Rahmen.
- **`subghz_bruteforce`/`ir_bruteforce`/`generate_deploy_run`/`workflow_rolljam_lab_demo`** sind
  selbst als `critical` eingestuft — deckungsgleich mit meinem eigenen Rahmen "keine Anleitungen
  für Massenangriffe": nur gegen eigene Geräte/autorisierte Ziele einsetzen, Konsens-Gate aktiv
  lassen.
- Benötigt einen eigenen Anthropic-API-Key (laufende Kosten je nach Nutzung) und optional
  `sox` (Sprachmodus) bzw. ein ESP32-Marauder-Board (WiFi-Funktionen) als Zusatzvoraussetzungen.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

Installations-Skript gegen eine eigene, isolierte Umgebung prüfen (nicht das `curl`-Skript
blind gegen das Produktivsystem laufen lassen — Checksumme/Quelltext vorher ansehen), danach im
`--read-only`-Modus gegen den eigenen Flipper (`mntm-012`) verbinden und `device_info`/
`storage_list` als harmlosen Rauchtest fahren. Erst danach, mit aktivem Konsens-Gate, einzelne
Low-/Medium-Risiko-Tools (z. B. `subghz_receive`, `nfc_detect`) gegen eigene Geräte testen. Vor
jedem High-/Critical-Tool-Einsatz das in README/`SECURITY.md` beschriebene Konsens-Verhalten am
eigenen Terminal selbst nachvollziehen, nicht nur der Dokumentation vertrauen.
