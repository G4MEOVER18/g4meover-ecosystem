# Idee-Status: Badge Audit (NFC-Zutrittsmedien-Auditor, RogueMaster-Bündel)

Backlog-Auftrag "RogueMaster-Bündel weiter durchsehen" fortgesetzt (`flipperzero-firmware-wPlugins`,
`applications/external/`, vollständige Verzeichnisliste diesmal direkt per `git`-Sparse-Checkout
gegen den echten Baum geprüft statt aus Sekundärquellen zu zitieren — 762 App-Ordner). Kandidat
`badge_audit` geprüft, weil der Name stark an den bereits als Firmware-Mining-Kandidaten
vorliegenden `access_audit`-Port (`firmware-mining/access-audit-app.md`) erinnert.

## Fund

**Quelle:** `applications/external/badge_audit`, Autor ViperDroid (Zakariya Jabbar), Ursprung
`ViperDroid/flipper-badge-audit`, MIT-Lizenz, Version 1.0, 6 GitHub-Stars, 8 Commits, eigene
`SECURITY.md`/`CODE_OF_CONDUCT.md` — kleines, aber ernsthaft geführtes Ein-Autor-Projekt, explizit
als "defensive / read-only" beworben.

**Was die App tut:** Karte an die Rückseite halten → Protokoll-Erkennung (MIFARE Classic/
Ultralight/DESFire/Plus, ISO14443-3A/4A/4B/3B, ISO15693, SLIX, FeliCa, ST25TB) über
`nfc_scanner_*`, UID/SAK/ATQA-Anzeige, bei MIFARE Classic Sektor-für-Sektor-Test gegen 6 bekannte
Werks-/Default-Schlüssel (`mf_classic_poller_sync_auth`), daraus ein Clone-Resistance-Score
(0–100) mit WEAK/MEDIUM/STRONG/CRITICAL-Label plus SD-Scorecard-Export. Einzige Quelldatei
(`badge_audit.c`, ~640 Zeilen) geprüft: reine `nfc`/`mf_classic`-HAL-Aufrufe, kein Schreiben, kein
Emulieren, kein Auth-Bypass-Versuch — Selbstbeschreibung als read-only trifft auf den Code zu.

## Verhältnis zu Access Audit (bereits vorgeschlagener Port)

`access_audit` (`firmware-mining/access-audit-app.md`, Autor matthewkayne) deckt exakt dieselbe
Aufgabenstellung ab und ist in jeder Dimension umfangreicher:
- **Kartentypen:** Access Audit erkennt zusätzlich NTAG203/213/215/216, NTAG I2C, FeliCa Lite und
  **HID Seos** — Badge Audit nur die Basisliste oben.
- **Technologien insgesamt:** Access Audit deckt zusätzlich **125-kHz-LF-RFID** (EM4100, HID
  H10301, Indala über den nativen `lfrfid_worker`) und **HID iCLASS Legacy** (CSN-Read über
  eigenen ISO15693-Poller) ab. Badge Audit ist ausschließlich 13,56-MHz-NFC; sein eigenes
  `README.md`-Roadmap-Kapitel listet "125 kHz LF badge support (EM4100 / HID Prox)" selbst noch
  als offenen Punkt.
- **Default-Key-Prüfung:** Access Audit testet zusätzlich Ultralight/NTAG-Factory-Passwort
  `FFFFFFFF` (config-gated); Badge Audit deckt nur MIFARE-Classic-Sektorschlüssel ab, dafür mit
  zwei zusätzlichen Schlüsseln in der Liste (6 statt Access Audits 8 an anderer Stelle — im Kern
  identischer Werkskeys-Satz, keine Ergänzung, die Access Audit fehlt).
- **Scoring:** beide liefern 0–100 + Verdict-Label; Access Audit dokumentiert seine Regelbasis
  explizit als OWASP-Risk-Rating-Likelihood (`core/scoring.c`/`core/rules.c`), Badge Audit nutzt
  eine einfachere, fest verdrahtete Tabelle in einer einzigen Funktion (`posture_for()`) —
  inhaltlich vergleichbare Einstufungen (z. B. DESFire STRONG/90, MIFARE Classic mit Default-Keys
  CRITICAL), aber ohne die nachvollziehbare Regeltrennung.

Kein einziges Badge-Audit-Feature liegt außerhalb dessen, was Access Audit bereits abdeckt oder in
dessen bereits vorliegendem Port-Vorschlag mit portiert würde. Ein eigener Badge-Audit-Port wäre
damit eine reine Funktionsteilmenge ohne Mehrwert.

## API-87-Status

Nicht geprüft/nicht relevant — siehe Fazit unten, kein Port vorgesehen. Für die Vollständigkeit:
Quellcode nutzt ausschließlich dieselbe `nfc_scanner_*`/`nfc_protocol_*`/`mf_classic_poller_sync_*`-
API-Familie, die beim `access_audit`-Review bereits als aktuell/stabil gegen `mntm-012`/API 87.1
verifiziert wurde (kein separates Risiko zu erwarten, falls doch benötigt).

## Fazit

Katalog-Status: kein eigenständiger Port-Kandidat. Falls der `access_audit`-Port lokal umgesetzt
wird, deckt er Badge Audits gesamten Funktionsumfang mit ab. `ideas/pentest-app-katalog.md`
Access-Audit-Zeile um den Hinweis ergänzt, dass `badge_audit` als engerer Alternativkandidat
geprüft und wegen vollständiger Funktionsüberschneidung verworfen wurde, damit dieser Name bei
künftigen RogueMaster-Bündel-Durchsichten nicht erneut aufgegriffen wird.

## Sicherheitsrahmen

Beide Apps (Access Audit wie Badge Audit) sind reine Lese-/Bewertungswerkzeuge ohne Klon-/
Schreib-/Emulationsfunktion — passen unverändert in den defensiven Rahmen aus CONSTRAINTS Punkt 5.
Kein Einsatz an fremden, nicht ausdrücklich autorisierten Zutrittsmedien.
