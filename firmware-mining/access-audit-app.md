# Access Audit — externe App aus RogueMaster-Bündel (NFC/RFID/iCLASS-Risikoscoring)

## Was
Weitere Durchsicht des RogueMaster-App-Bündels (`flipperzero-firmware-wPlugins`, Branch `420`,
`applications/external/`) laut Backlog-Punkt „RogueMaster-Bündel weiter durchsehen" — konkret
den dort schon vorgemerkten Kandidaten `access_audit` geprüft. **Access Audit**
(`applications/external/access_audit`, Autor matthewkayne, Quelle
`matthewkayne/flipper-access-audit`, MIT-Lizenz, aktuelle Version 1.13.1) ist eine dedizierte
Audit-App für Zutrittskontroll-Medien: Karte/Tag antippen → sofortiges Risiko-Scoring (0–100,
OWASP-Risk-Rating-Framing) + Klartext-Empfehlung, optional als Session-Report auf SD gespeichert.

Erkennt und klassifiziert:
- **13,56 MHz NFC:** MIFARE Classic 1K/4K/Mini, DESFire EV1/EV2/EV3/Light, MIFARE Plus SL1-SL3,
  Ultralight/Ultralight C, NTAG203/213/215/216, NTAG I2C, ISO14443-A/B, ISO15693, FeliCa
  (Standard/Lite), SLIX, ST25TB, **HID Seos** (per ISO-7816-`SELECT` der Seos-AID über den
  ISO14443-4A-Poller).
- **125 kHz LF-RFID:** EM4100, HID H10301, HID generisch, Indala — über den nativen
  `lfrfid_worker`.
- **HID iCLASS (Legacy):** eigener minimaler Poller auf Basis des ISO15693_3-Pollers, der das
  proprietäre ACTALL/IDENTIFY/SELECT/READ-Kommandoset direkt per `nfc_poller_trx()` injiziert
  (kein volles iCLASS-Crypto/-ASN.1 nötig) und daraus CSN + 2k/16k/32k unterscheidet.
- **Aktive Default-Key-Prüfung:** MIFARE-Classic-Sektor-0 (bei Treffer Sweep über alle Sektoren)
  gegen 8 bekannte Public-Keys (Werks-Transport, NXP MAD, NFC-Forum-NDEF, gängige
  Hersteller-Defaults) sowie Ultralight/NTAG-Factory-Passwort `FFFFFFFF` (nur config-gated,
  ohne Lockout-Risiko) — beides nicht-destruktiv (kein Sektor-Read, sofortiges Halt).

Ich habe geprüft, was das Scoring inhaltlich macht (`core/scoring.c`, `core/rules.c`): reine
Regelauswertung auf einer selbst erhobenen `AccessObservation`-Struktur (Technologie, Kartentyp,
UID/Metadaten-Vollständigkeit, Speicherschutz, Default-Keys) → Severity (Info/Low/Medium/High)
→ Score + Verdict-Label (SECURE/LOW RISK/MODERATE/HIGH RISK), explizit als
**Likelihood-Rating** dokumentiert (OWASP Risk Rating Methodology) — Impact bleibt bewusst dem
Einsatzkontext vorbehalten. Kein Angriffscode: reine Klassifikation + Bewertung + Beratungstext.

## Nutzen
- Ergänzt mein Physical-Access-Toolkit sinnvoll zur bereits vorgeschlagenen Wiegand-Reader-App
  (`firmware-mining/wiegand-reader-app.md`): Wiegand deckt die Leser/Controller-Leitung ab,
  Access Audit die **Medienseite** (Karte/Tag selbst) — zusammen ein durchgängiger Workflow für
  autorisierte physische Zutrittskontroll-Audits (erst Medium bewerten, dann bei Bedarf die
  Wiegand-Leitung prüfen).
- Direkt nutzbar für den in CONSTRAINTS/WORKFLOW verlangten defensiven Rahmen: liefert pro Karte
  eine dokumentierte, nachvollziehbare Risikoeinschätzung samt SD-Report — genau das Artefakt,
  das ein autorisierter Audit-Auftrag als Nachweis braucht, ohne dass ich selbst Scoring-Logik
  neu bauen muss.
- Schließt eine Lücke in meinem NFC/RFID-Werkzeugkasten (MFKey/Nested für Crypto1-Recovery,
  NFC Magic zum Klonen, LF-RFID-Fuzzer) um die **Bewertungs**-Ebene: „ist dieses Medium
  überhaupt schützenswert kompromittiert" statt nur „kann ich es klonen/fuzzen".
- iCLASS-Erkennung ohne den Portaufwand von `picopass`/`seader`: liest nur die öffentliche CSN
  + Konfigurationsblock (kein Crypto, keine ASN.1/mbedTLS-Abhängigkeit) — genau der leichte Weg,
  den ich beim Wiegand-Fund als Entscheidungskriterium gegen die schwereren iCLASS-Kandidaten
  genannt hatte, hier bereits fertig umgesetzt.
- Aktiv gepflegt (Changelog bis 1.13.1, u. a. ein Crash-Fix beim 125-kHz-Lesen erweiterter
  Protokolle, Scoring-Korrekturen für MIFARE Plus SL2/FeliCa, Report/Screen-Konsistenz-Fixes) —
  kein verwaistes Bündel-Beiwerk, sondern ein Projekt mit eigenem Issue-Tracker/CI/Lint.
- Kleiner, sauber geschnittener Port: 9 `.c`/`.h`-Dateien (~3400 Zeilen, davon Kern-Logik in
  `core/`), keine externen Libraries/Submodule, keine GPIO-Hardware nötig — nutzt ausschließlich
  vorhandene NFC/LFRFID-Poller/-Worker-APIs meiner Firmware.

## Betroffene Dateien (lokal)
Neu, unverändert aus der Quelle übernehmbar (eigenständiger externer App-Ordner, kein Eingriff
in Firmware-Kernbestand nötig):
- `applications_user/access_audit/application.fam`
- `applications_user/access_audit/access_audit.c`
- `applications_user/access_audit/access_audit.h`
- `applications_user/access_audit/icon.png`
- `applications_user/access_audit/LICENSE`
- `applications_user/access_audit/core/observation.h`
- `applications_user/access_audit/core/observation_provider.c` / `.h`
- `applications_user/access_audit/core/rfid_provider.c` / `.h`
- `applications_user/access_audit/core/iclass_provider.c` / `.h`
- `applications_user/access_audit/core/rules.c` / `.h`
- `applications_user/access_audit/core/scoring.c` / `.h`
- `applications_user/access_audit/core/report.c` / `.h`
- `applications_user/access_audit/core/session.c` / `.h`

Bewusst **nicht** übernehmen: `screenshots/` und `docs/*.png` (nur Repo-Dokumentation, für den
Build irrelevant) sowie jegliche selbst erzeugten `.txt`-Session-Reports aus lokalen Tests —
CONSTRAINTS verbieten mir generell echte Karten-/Zugangsdaten in diesem Repo; ein Session-Report
mit echten UIDs/Kartentypen fällt sinngemäß unter das Capture-Verbot (Punkt 2/3), auch wenn das
Dateiformat `.txt` statt `.nfc`/`.rfid` ist.

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (baut jede App per `ufbt` gegen
das Momentum-SDK mntm-012/API 87.1 inkl. APPCHK).

## Risiko
- Niedrig: additiver externer App-Ordner, keine Änderung an Firmware-Kernbestand, kein
  gemeinsamer State mit anderen Apps, keine GPIO-Pin-Nutzung. Fehlschlag zeigt sich beim Build
  (`ufbt`/APPCHK) als Compile-Fehler, falls einzelne NFC-Poller-APIs zwischen dem
  RogueMaster-Basisstand und meiner mntm-012/API-87.1-Basis abweichen sollten — laut
  Quelldurchsicht ausschließlich aktuelle, stabile `nfc_poller_*`/`nfc_scanner_*`/
  `lfrfid_worker_*`-APIs, keine als deprecated markierten Aufrufe erkennbar.
- Der iCLASS-Provider injiziert proprietäre iCLASS-Kommandobytes (ACTALL/IDENTIFY/SELECT/READ
  Block 1) direkt über `nfc_poller_trx()` auf einem laufenden ISO15693_3-Poller. Das ist
  Lesezugriff auf öffentlich dokumentierte, unverschlüsselte Felder (CSN, Konfigurationsblock)
  — kein Schreiben, kein Auth-Bypass-Versuch, kein Crypto-Angriff. Trotzdem: nur an
  eigener/autorisierter iCLASS-Hardware einsetzen (CONSTRAINTS Punkt 5).
- Default-Key- und Factory-Passwort-Prüfungen sind laut Quelldurchsicht bewusst nicht-destruktiv
  umgesetzt (Sektor-0-Test ohne Datenlesen + sofortiges Halt; Ultralight-Passwort nur bei
  fehlendem Lockout-Zähler) — beim lokalen Test trotzdem zuerst an eigenen/Testkarten
  verifizieren, dass kein Lockout ausgelöst wird, bevor ich es an fremden autorisierten Medien
  einsetze.
- Sicherheitsrahmen: ausschließlich an eigenen oder vertraglich autorisierten
  Zutrittskontroll-Medien einsetzen — die App liefert selbst nur Klassifikation + Bewertung,
  keine Klon-/Angriffsfunktion, passt damit direkt in den defensiven Rahmen aus CONSTRAINTS
  Punkt 5.
- Kein Datenschutz-/Fremddaten-Risiko für dieses Repo, solange keine Session-Reports (`.txt`
  unter `apps_data/access_audit/`) aus echten Tests gepusht werden (siehe „Betroffene Dateien").

## Port-Skizze
1. App-Ordner `access_audit` (alle unter „Betroffene Dateien" gelisteten Dateien, inkl.
   `core/`-Unterordner, ohne `screenshots/`/`docs/`) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/access_audit/`) unverändert nach `applications_user/access_audit/`
   in meiner Firmware kopieren.
2. `ufbt launch_app APPSRC=applications_user/access_audit` bzw. `fbt launch_app
   APPSRC=applications_user/access_audit` — deckt etwaige API-Abweichungen sofort als
   Compile-Fehler auf (laut Quelldurchsicht nicht erwartet, siehe Risiko).
3. Funktionstest lokal, gestaffelt nach Technologie, jeweils an eigenen/autorisierten
   Testmedien:
   - 125-kHz-Modus: eigene EM4100/HID-Prox-/Indala-Tags antippen, Klassifikation + Score prüfen.
   - NFC-Modus: eigene MIFARE-Classic-/Ultralight-/NTAG-/DESFire-Karten antippen, insbesondere
     eine Karte mit bekanntem Default-Key (z. B. Werks-Transportschlüssel) zur Verifikation der
     Default-Key-Erkennung.
   - iCLASS-Modus: nur falls eigene/autorisierte HID-iCLASS-Legacy-Karte vorhanden — CSN-Read +
     2k/16k/32k-Klassifikation verifizieren.
4. Session-Report-Funktion prüfen (Speichern/Laden/Löschen unter `apps_data/access_audit/`) —
   erzeugte `.txt`-Reports bleiben lokal (siehe Risiko/Betroffene Dateien), nicht in dieses
   Repo übernehmen.
5. Bei erfolgreichem Test: App dauerhaft in `applications_user/` meiner Firmware belassen oder
   als eigenständiges Fork-Repo führen; `ci-templates/build-fap.yml` optional als CI ergänzen.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/access_audit/` (Stand des Branches beim Durchsehen: aktuellster
  Commit auf `420`).
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `matthewkayne/flipper-access-audit`,
  Version 1.13.1, MIT-Lizenz, Copyright (c) 2026 Matthew Kayne.
