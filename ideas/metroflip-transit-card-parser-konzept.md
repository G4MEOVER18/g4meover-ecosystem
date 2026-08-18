# NFC-Konzept: Metroflip Transit-Card-Parser (Ideen-Katalog-Auftrag abgearbeitet)

Aus `pentest-app-katalog.md` aufgegriffen: Metroflip stand dort bislang mit Status ❓
("prüfen") in der NFC/RFID-Tabelle. Diese Sitzung verifiziert den tatsächlichen Stand
(Aktivität, Bauweise, API-87-Kompatibilitätsrisiko) und kuratiert den Fund ausführlich.

## Konzept

**Quelle:** github.com/luu176/Metroflip (GPL-Lizenz, aktiv gepflegt, letzter Commit
2026-07-01), inspiriert vom PC-seitigen Metrodroid-Projekt. Reine Recherche anhand des
öffentlichen Quellcodes — keine echten Kartendaten aus dieser Quelle übernommen.

Metroflip ist ein eigenständiges `.fap` (kein Firmware-Fork) für Nahverkehrs-/Transit-Karten
weltweit. Architektur: eine Basis-App (`metroflip`, `FlipperAppType.EXTERNAL`) stellt über
eine eigene, im Repo mitgelieferte API-Tabelle (`api/metroflip/metroflip_api_table.cpp`)
Dienste für rund 20 Karten-Parser bereit, die selbst als `FlipperAppType.PLUGIN` mit
`fal_embedded=True` gebaut werden (dasselbe Muster wie offizielle Apps mit
Protokoll-Plugins, z. B. die Stock-NFC-App). Unterstützte Kartenfamilien/-technologien:
MIFARE Classic, MIFARE DESFire, MIFARE Ultralight, ISO14443-4 (A/B), FeliCa, ST25TB sowie
Calypso (eigener, app-interner Poller auf Basis von ISO14443-4, kein Firmware-HAL-Protokoll)
— je nach Stadt/Betreiber, u. a. Clipper (San Francisco), Opal (Sydney), myki (Melbourne),
Navigo (Paris, Calypso), Suica (Japan, FeliCa), Troika (Moskau) und weitere.

## Nutzen für mich

Deckt eine mir bislang komplett fehlende Kategorie ab: strukturiertes Parsen/Anzeigen von
Transit-Kartendaten (Guthaben, letzte Fahrten, Kartennummer) direkt auf dem Gerät, ohne
eigenen Parser je Betreiber schreiben zu müssen. Praxisrelevant für den Eigen-Check, welche
Daten die eigene Nahverkehrskarte kontaktlos preisgibt (Privacy-/Skimming-Audit der eigenen
Karte) — ergänzt die bereits vorhandene medienseitige Zutrittskontroll-Bewertung
(`access-audit-app.md`) um die Transit-/ÖPNV-Karten-Seite, die dort explizit nicht
abgedeckt ist. Reine Lese-/Anzeige-App (kein Schreiben/Klonen), passt damit sauber in den
defensiven Rahmen.

## Aufwand

Gering. Reine Zusatz-App, kein Firmware-Patch nötig — Installation identisch zu
ProtoPirate/RollJam/RollForge/g4meover-magspoof: entweder vorgebautes `metroflip.fap` aus
den GitHub-Releases auf die SD-Karte kopieren, oder lokal per `ufbt update && ufbt
fap_metroflip` gegen den eigenen mntm-012-SDK-Stand bauen. Keine zusätzlichen
Hardware-Module nötig (nutzt ausschließlich den vorhandenen NFC-Chip).

## API-87-Status

✅ Geringes Risiko, kein Firmware-seitiges Hindernis erkennbar. Begründung:
- Alle genutzten Includes (`nfc/nfc.h`, `nfc/nfc_poller.h`, `nfc/nfc_scanner.h`,
  `nfc/protocols/{mf_classic,mf_desfire,mf_ultralight,iso14443_4a,iso14443_4b,felica,
  st25tb}/...`) sind Standard-NFC-HAL-Header der offiziellen Firmware, seit vielen
  API-Ständen vor 87.1 stabil vorhanden — keiner der Verweise betrifft ein
  Momentum-spezifisches oder erst kürzlich hinzugefügtes Protokoll.
- Calypso (Navigo u. a.) ist als eigener, app-interner Poller implementiert
  (`api/calypso/`) und nutzt nur die generische ISO14443-4-Transaktionsschicht —
  kein zusätzliches Firmware-HAL-Protokoll nötig.
- Das Plugin-Lademuster (`FlipperAppType.PLUGIN` + `fal_embedded=True`, App-eigene
  API-Tabelle) ist dasselbe, das offizielle Apps seit Langem für ihre eigenen
  Protokoll-Plugins verwenden — kein experimentelles oder Momentum-inkompatibles Muster.
- Einzige im CHANGELOG dokumentierte API-bezogene Historie ("Fixed a major bug due to
  API symbol not existing") betraf einen bereits behobenen Altfall vor Einführung der
  eigenen API-Tabelle, kein offener Punkt im aktuellen Stand.
- Trotzdem vor Produktiveinsatz lokal per `ufbt fap_metroflip` gegen den eigenen
  mntm-012-Stand bauen und mit einer eigenen Testkarte verifizieren, da keine der
  Quellen eine explizite Momentum-Kompatibilitätsliste führt.

## Sicherheitsrahmen

Reine Lese-/Parser-App gegen eigene Transit-Karten — kein Schreiben, Klonen oder Emulieren
von Kartendaten. Einsatz ausschließlich gegen die eigene(n) Nahverkehrskarte(n) zum
Privacy-/Skimming-Audit (welche Daten sind kontaktlos ohne PIN auslesbar). Keine
Kartendaten Dritter, keine Sammlung/Weitergabe echter Fahrkarten-/Guthabendaten in dieses
Repo. Kein Bezug zu Massenangriffen, Fahrgeld-Betrug oder Detection-Evasion.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

`ufbt fap_metroflip` gegen den lokalen mntm-012-SDK-Stand bauen, `ufbt launch` gegen das
eigene Gerät, dann mit der eigenen Transit-Karte (falls vorhanden) den Parser-Output
gegenprüfen. Bei Erfolg als "läuft" in `pentest-app-katalog.md` bereits vorgemerkt (Status
unten aktualisiert).
