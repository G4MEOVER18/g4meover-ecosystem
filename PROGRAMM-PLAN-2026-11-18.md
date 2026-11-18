# G4MEOVER — Firmware- & Ökosystem-Programm

**Maximaler Umsetzungsplan · Stand 2026-11-18**

Dieses Dokument ist der verbindliche Gesamtplan für den professionellen Ausbau der
G4MEOVER-Flipper-Firmware und des umgebenden Hardware-Ökosystems (Heltec LoRa V4,
Kali-Raspi, FlipRPI-Steuerung). Es ist die zentrale Steuerungsebene; einzelne Repos
führen ihre Detail-Aufgaben in eigenen `BACKLOG.md`/Wiki-Seiten.

---

## 0. Leitplanken (gelten für ALLES in diesem Programm)

Diese Regeln sind nicht verhandelbar und werden bei jedem Arbeitsblock geprüft:

1. **Eigenständige Identität, kein Fremd-Branding.** In allen öffentlichen G4MEOVER-Repos
   — besonders der Firmware — erscheint **kein** „Momentum", keine Link-Halden auf fremde
   Repos, keine kopierten Texte. READMEs/Wikis werden **selbst verfasst** und mit **eigenen
   Grafiken/Bannern** versehen. Lizenzpflichtige Herkunftsnennung (GPL-3.0) steht knapp und
   sachlich an genau einer Stelle (`CREDITS`/`LICENSE`), nicht als Werbung im Vordergrund.
2. **Spenden zeigen ausschließlich auf den Betreiber.** Jeder Donation-Hinweis in Code,
   README, Wiki, About-Screens oder Release-Notes verweist **nur** auf das eigene PayPal
   bzw. die eigene Bitcoin-Adresse. Fremde Spenden-/Sponsor-Links werden entfernt.
3. **Nur ein Urheber.** Commits und PRs laufen ausschließlich auf den Betreiber. **Kein**
   `Co-Authored-By`, kein „Claude Code"-Vermerk — nirgends.
4. **Umlaute echt.** Durchgängig ä ö ü, nie ae/oe/ue, nie ß.
5. **Verifikations-Disziplin (HOLD).** Nichts wird als „gefixt/funktioniert" deklariert,
   bevor es belegt ist (Build grün, On-Device getestet, oder Host-Repro mit Sanitizer).
   Unbelegte Dinge bleiben ausdrücklich „offen/unverifiziert".
6. **Sicherung.** Jeder Arbeitsblock = Commits + Patchsatz auf dem Stick + Sicherung;
   vor Umstellungen prüfen, ob der Arbeitsbaum Commits enthält, die das Repo nicht kennt.
7. **Autorisierter Rahmen.** Alle Offensiv-Werkzeuge sind für autorisierte Security-
   Forschung an eigener Hardware/eigenen Netzen. Test-/Autorisierungs-Marker bleiben drin;
   keine Malware-Bündelung (Ransomware/Keylogger/Stealer werden nicht importiert).

---

## 1. Lagebild (verifiziert am 2026-11-18)

| Thema | Befund |
|---|---|
| Baubare Firmware | `flipper/G4MEOVER-FW-v2` (Momentum-Fork, mntm-012-Basis), pusht nach `G4MEOVER18/G4MEOVER-FW` |
| Externe Apps | **Git-Submodul** `applications/external` → aktuell `Next-Flip/Momentum-Apps` (237 Apps) |
| Eigene Apps | `applications_user/`: flipperjammer, rollforge, rolljam, rolllab, protopirate, lora_ukfe, flipdeflock, wendigo, fz_nrf24_jammer |
| Toolchain | Python 3.12 + fbt + ARM-Toolchain lokal vorhanden → **lokaler Build machbar** |
| Flipper | **COM3** (STM32 VID 0483) angeschlossen |
| Heltec LoRa V4 | **COM5** (Espressif ESP32-S3, VID 303A) angeschlossen |
| Kali-Raspi | 192.168.0.18 — Ping OK, **SSH Port 22 Timeout** (blockiert, muss repariert werden) |
| Harvest-Quellen lokal | `Downloads/`: doctor-firmware-dev.zip, LakeShark-Flipper, lakeshark_p25.fap, zeromesh.fap |
| `g4meover-ecosystem` | Planungs-/Mining-Repo (dieses Dokument lebt hier), **keine** baubare FW |

**Konsequenz:** „Firmware bauen & flashen" = `G4MEOVER-FW-v2` bauen → `G4MEOVER-FW` releasen.

---

## 2. Workstreams

Jeder Workstream hat Phasen (P), Lieferobjekte (L) und Abnahmekriterien (A).

### WS-1 — Firmware-Stabilität (Crash-Audit & Fixes)  · Priorität: HOCH

Ziel: Keine abstürzenden Apps mehr; mfkey32 & Co. laufen stabil.

- **P1.1 mfkey-Crash** *(in Arbeit)* — Stack-Unterdimensionierung behoben:
  App-Stack 1 KB→2 KB, Worker-Thread 2 KB→4 KB. **A:** FAP baut grün; On-Device Nested-/
  Mfkey32-Recovery ohne Absturz.
- **P1.2 Systematisches Stack-/Heap-Audit** — Skript, das alle `application.fam` nach zu
  kleinen `stack_size` prüft (Heuristik: NFC/SubGHz/WiFi-Apps < 2 KB sind verdächtig) und
  alle `malloc` ohne NULL-Prüfung meldet. **L:** `reports/crash-audit-YYYY-MM-DD.md`.
- **P1.3 Priorisierte Fixes** — Top-Absturzkandidaten nach Audit abarbeiten, je Fix ein
  Commit + kurze Begründung. **A:** Build grün, stichprobenartig On-Device getestet.
- **P1.4 Regressionsliste** — Liste der getesteten Apps mit Status (OK / Crash / offen).

### WS-2 — App-Kompatibilität & Vielfalt (Harvesting)  · Priorität: HOCH

Ziel: Maximale FAP-Kompatibilität + starkes, gut organisiertes Pentest-Portfolio.

- **P2.1 Quellen-Inventar** — Abgleich der Apps aus: Momentum-Apps (Ist), doctor-firmware,
  LakeShark, Unleashed-/RogueMaster-/Xtreme-Kataloge, einschlägige Einzel-Repos.
  **L:** `firmware-mining/app-matrix.csv` (App, Quelle, Version lokal/upstream, Kategorie,
  Status: fehlt/veraltet/aktuell/Konflikt).
- **P2.2 Versions-Upgrade vorhandener Apps** — für jede bereits installierte App die
  **neuere/bessere** Version ermitteln und, wenn sauber baubar, übernehmen (Changelog-
  Vermerk). **A:** Build grün, keine Doppel-appids.
- **P2.3 Neue FAPs** — fehlende, hochwertige Pentest-/Recon-Apps ergänzen (kategorisiert:
  SubGHz, NFC/RFID, WiFi/BT, IR, GPIO/HW, iButton, Tools). **A:** je App .fam/Icon korrekt,
  baut, in Menü-Kategorie einsortiert.
- **P2.4 Kompatibilitäts-Layer** — API-Diffs der Firmware-Basis dokumentieren, damit
  Fremd-FAPs ohne Nachbau laufen; Shim/Hinweise wo nötig.
- **P2.5 Eigener Apps-Katalog** — Submodul `applications/external` auf einen **eigenen Fork**
  (`G4MEOVER18/g4meover-apps`) umhängen, damit der gesamte App-Katalog unter eigener
  Kontrolle steht (Branding, eigene Ergänzungen, kein Upstream-Zwang).

### WS-3 — Eigene Apps verbessern & erweitern  · Priorität: MITTEL

Ziel: Die G4MEOVER-Power-Suite wird spürbar stärker.

- **P3.1 Review** je App (rollforge, rolljam, rolllab, protopirate, flipperjammer,
  lora_ukfe, flipdeflock, wendigo, fz_nrf24_jammer): Funktionsstand, offene Bugs, UX.
- **P3.2 Feature-Ausbau** — pro App 1–3 sinnvolle neue Funktionen (siehe Ideen-Pool §4).
- **P3.3 lora_ukfe ↔ Heltec V4** — tiefe Kopplung (siehe WS-5).
- **A:** je Änderung Build grün + On-Device-Smoke-Test, Wiki-Seite aktualisiert.

### WS-4 — FlipRPI → G4MEOVER Kali-Control  · Priorität: HOCH (eigener Wunsch)

Ziel: `jblanked/FlipRPI` zu einem vollwertigen **Kali-Raspi-Fernsteuer-Frontend** auf dem
Flipper ausbauen — „fix fertig mit Menü und allem".

- **P4.1 Fork & Analyse** — FlipRPI klonen, Transport (USB-CDC/UART/WiFi) und Protokoll
  verstehen. **L:** Architektur-Notiz.
- **P4.2 Menü-Redesign** — kategorisiertes Menü auf dem Flipper: System (Status/Reboot/
  Updates), Netz (Interfaces/Scan-Status), Tool-Starter (vordefinierte, autorisierte
  Pentest-Workflows), Dateien/Logs, Konsole. Scrollbar, mit Rückkanal-Statusanzeige.
- **P4.3 Pi-Companion** — schlanker Dienst auf dem Kali-Pi, der Kommandos aus einer
  **Allowlist** entgegennimmt und Status/Ergebnisse zurückmeldet (kein frei einschleusbarer
  Shell-Zugriff; definierte, autorisierte Aktionen).
- **P4.4 Verschmelzung** — mit bestehendem Kali-Cockpit (`/home/kali/g4meover-cockpit/`)
  und `ukfe_rf` zusammenführen, statt Parallelwelt.
- **A:** End-to-End: Flipper-Menü löst Pi-Aktion aus, Ergebnis erscheint am Flipper.
  *(Abhängig von WS-6: SSH muss wieder laufen.)*

### WS-5 — Heltec LoRa V4 „volles Spektrum"  · Priorität: HOCH (eigene Hardware)

Ziel: Das eigene Flipper-Board maximal, sinnvoll und experimentell nutzbar machen.
Basis: `ecosystem/heltec-v4-hub` (bereits gebaut/geflasht, E2E verifiziert).

- **P5.1 Fähigkeits-Landkarte** — was kann die V4 (SX126x-LoRa, WiFi/BT via ESP32-S3,
  GPIO, Display, ggf. GNSS/Sensorik)? **L:** `V4-FAEHIGKEITEN.md`.
- **P5.2 Funktionsmodule** (je Modul Firmware + Flipper-Gegenstelle über `ukfe_rf`):
  - LoRa-Mesh/Relay (ESP-NOW ↔ LoRa Bridge, bereits im Ansatz vorhanden) vertiefen
  - SubGHz-Co-Prozessor: 868/915-Aufgaben auslagern, die der CC1101 nicht gut kann
  - WiFi/BT-Recon-Feed an den Flipper (Scan-Ergebnisse im Flipper-UI)
  - Sensor-/GNSS-Telemetrie (falls Hardware vorhanden) als Live-Panel
  - Experiment: LoRa-Spektrum-/RSSI-Sweep als „poor man's SDR"-Sicht
- **P5.3 Flipper-UI** — ein „V4-Panel" (Aktion · Gerät · Funk · Status), das alle Module
  bündelt. **A:** je Modul verdrahteter Round-Trip belegt (wie bisher Strang C).

### WS-6 — Kali-Raspi-Wachstum & SSH-Delegation  · Priorität: HOCH (Blocker zuerst)

Ziel: Auf dem Pi autonom weiterentwickeln (Claude Code dort), Tooling verschmelzen.

- **P6.1 SSH-Reparatur** *(Blocker)* — Port 22 Timeout diagnostizieren: Pi online (Ping OK),
  aber sshd/Firewall/Port offen? Prüfen: alternativer Port, `ufw`, sshd-Status, Netz-Route.
  Zugang laut Memory: `kali@192.168.0.18` (Cockpit) bzw. `yanis@…/Core2Quad` (Infra).
- **P6.2 Delegations-Setup** — prüfen, ob Claude Code auf dem Pi installiert/erreichbar ist;
  sonst Aufträge via Skript/Agent-Gateway. **A:** eine delegierte Aufgabe nachweislich
  auf dem Pi ausgeführt.
- **P6.3 Ausbau** — Cockpit, `ukfe_rf`, FlipRPI-Companion (WS-4) zusammenführen; Tool-
  Umgebung pflegen. **Produktion nicht brechen** (DogyTag-TTN-Kette ist read-only tabu).

### WS-7 — GitHub-Professionalisierung (Doku · Wiki · Grafik)  · Priorität: MITTEL (laufend)

Ziel: Jedes Projekt sauber dokumentiert, eigenes Wiki, eigene Optik — ohne Fremd-Ballast.

- **P7.1 README-Neufassung** je G4MEOVER-Repo: selbst verfasst, eigene Banner/Screenshots/
  Diagramme, klare Struktur (Was · Warum · Features · Build · Flash · Nutzung · Lizenz).
- **P7.2 Wiki je Repo** — Aufbau/Pflege, Querverweise, keine Fremd-Link-Halden.
- **P7.3 Spenden-Sektion** — einheitlicher Block (PayPal/BTC) als Vorlage, überall gleich.
- **P7.4 Grafik-Kit** — wiederverwendbares Banner-/Icon-Set im G4MEOVER-Arcade-Stil.
- **A:** Repo wirkt eigenständig, kein „momentum"-Treffer, Spenden zeigen nur auf Betreiber.

### WS-8 — Build & Release  · Priorität: HOCH (Abschluss jedes Zyklus)

- **P8.1 Reproduzierbarer Build** — `fbt updater_package` (voll) dokumentiert.
- **P8.2 Flash** — definierter, sicherer Ablauf (USB/DFU), Geräte-Zustand vorher prüfen.
  Flashen nur bei grünem Build und freiem Flipper; Brick-Risiko minimieren (DFU-Recovery
  dokumentiert).
- **P8.3 Release** — versionierte Assets (`.tgz`/`.dfu`) + selbst verfasste Release-Notes,
  GitHub-Release unter eigenem Account.

---

## 3. Phasenfolge (empfohlene Reihenfolge der Ausführung)

1. **Sofort / dieser Zyklus:** WS-1 P1.1 (mfkey, in Arbeit) → Build grün → WS-1 P1.2
   (Crash-Audit-Skript) → WS-6 P6.1 (SSH-Blocker lösen, da WS-4/WS-6 davon abhängen).
2. **Kurzfristig:** WS-2 P2.1 (App-Matrix) parallel zu WS-5 P5.1 (V4-Fähigkeiten) und
   WS-4 P4.1 (FlipRPI-Fork+Analyse).
3. **Mittelfristig:** WS-2 P2.2/P2.3 (Upgrades + neue FAPs), WS-3 (eigene Apps), WS-4
   P4.2–P4.4 (FlipRPI fertigstellen), WS-5 P5.2 (V4-Module).
4. **Laufend:** WS-7 (Doku/Wiki), WS-8 (Build/Release je Zyklus-Ende).

**Parallelisierbar (Subagenten):** App-Matrix-Recherche (WS-2.1), FlipRPI-Analyse (WS-4.1),
V4-Fähigkeits-Recherche (WS-5.1), Crash-Audit (WS-1.2) sind unabhängig und lassen sich
gleichzeitig vorantreiben.

---

## 4. Ideen-Pool (innovativ, für eigene Apps & Module)

Kandidaten zur Priorisierung — bewusst breit, Auswahl nach Machbarkeit:

- **G4MEOVER Command Deck (Flipper):** ein App-Hub, der Flipper + V4 + Pi als ein System
  zeigt (Aktion · Gerät · Funk · Status), inkl. Quick-Launch der Power-Suite.
- **lora_ukfe „Mesh-Karte":** empfangene Knoten/RSSI als kleine Kartenansicht am Flipper.
- **RollLab Analyse-Export:** erfasste SubGHz-Protokolle strukturiert auf SD exportieren
  (für autorisierte Nachanalyse am PC).
- **Wendigo Live-Feed:** V4-WiFi/BT-Recon streamt in ein Flipper-Panel statt Logdatei.
- **ProtoPirate Signatur-DB:** wachsende, eigene Protokoll-Signaturbibliothek.
- **V4 „Spektrum-Lite":** LoRa-RSSI-Sweep über Frequenzband als Balkenansicht.
- **FlipRPI Workflow-Presets:** kuratierte, autorisierte Pi-Aktionen als Ein-Klick-Kacheln.
- **Firmware Self-Test-App:** startet nacheinander kritische Apps und meldet Crashes
  (speist WS-1 Regressionsliste automatisch).

---

## 5. Risiken & Gegenmaßnahmen

| Risiko | Gegenmaßnahme |
|---|---|
| Voll-Build bricht wegen Umgebung | Erst Einzel-FAP-Builds (schnell), dann Voll-Build; Fehler isoliert beheben |
| Flash bricktt Gerät | Nur bei grünem Build; DFU-Recovery dokumentiert; Flipper-Zustand vorher prüfen |
| Fremd-FAP baut nicht gegen Basis-API | Kompatibilitäts-Layer (WS-2.4); App andernfalls zurückstellen, nicht erzwingen |
| SSH bleibt tot | WS-6 priorisiert; Fallback Agent-Gateway/lokale Skripte; WS-4/WS-6 bis dahin „Pi-seitig offen" |
| Upstream-Submodul-Umhängung bricht Build | Fork 1:1 spiegeln, Pin-Commit testen, erst dann umstellen |
| „Momentum"-Reste in Repos | Grep-Gate vor jedem Release (`grep -ri momentum` darf in Sichtflächen 0 treffen, außer Lizenz) |

---

## 6. Abnahme des Gesamtprogramms

Das Programm gilt je Zyklus als fortgeschritten, wenn:
- Build grün und (wo relevant) On-Device verifiziert ist,
- jede Änderung als Commit + Patchsatz gesichert ist,
- Doku/Wiki des betroffenen Repos nachgezogen wurde,
- die Leitplanken (§0) eingehalten sind (kein Fremd-Branding, Spenden nur eigen, kein
  Co-Autor, Umlaute, HOLD-Disziplin).

---

*Dieses Dokument wird je Zyklus fortgeschrieben. Es ersetzt keine Repo-spezifischen
Backlogs, sondern bündelt die übergeordnete Steuerung.*
