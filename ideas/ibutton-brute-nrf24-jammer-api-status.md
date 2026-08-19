# Recherche-Auftrag abgearbeitet: iButton-Brute / nRF24-Jammer API-87-Status

Aus `pentest-app-katalog.md` aufgegriffen: "iButton-Brute / nRF24-Apps: exakten API-87-Status
noch verifizieren (klonen + ufbt)" stand seit der Metroflip-Sitzung offen. Diese Sitzung
verifiziert beide Kandidaten anhand des öffentlichen Quellcodes (keine echten Kartendaten
übernommen).

## nRF24 Jammer (W0rthlessS0ul)

**Quelle:** `github.com/W0rthlessS0ul/FZ_nRF24_jammer` (MIT-Lizenz, aktiv gepflegt, letzter
Commit 2026-08-16 — drei Tage vor dieser Sitzung).

Eigenständiges `.fap` (`FlipperAppType.EXTERNAL`, Kategorie GPIO, App-ID `fz_nrf24_jammer`),
kein Firmware-Fork. Nutzt ein bis vier extern angeschlossene nRF24L01+-Module (SPI/GPIO,
Lötarbeit für die Verkabelung nötig) zur Störung von Bluetooth/BLE, WLAN, Zigbee und
2,4-GHz-Drohnenfunk über Kanal-Sweeping. Reine App-Ebene (`nrf24.c`-Bibliothek für die
Modul-Ansteuerung) — kein Eingriff in Firmware-interne SubGHz-/NFC-Protokoll-Stacks.

**Nutzen für mich:** Deckt 2,4-GHz-Störtests ab, die mein bisheriges Jammer-Repertoire
(RollForge-Jammer-Modul, FlipperJammer `subghz_jam.c`/`nrf24_jam.c`/`wifi_jam.c`) nicht
abdeckt — dort fehlt bislang ein dediziertes, mehrkanaliges nRF24-Sweep-Tool mit fertiger
1-/2-/4-Modul-Verkabelung. Einsatz ausschließlich zur RF-Belastbarkeitsprüfung eigener
Geräte in isolierter/autorisierter Umgebung (siehe Sicherheitsrahmen unten).

**Aufwand:** Gering. Vorgebaute Releases liegen pro Firmware-Familie vor
(`fz_nrf24_jammer_M.fap` für Momentum, `_R` für RogueMaster, `_U` für Unleashed) — keine
explizite Bindung an eine einzelne mntm-Buildnummer, nur an die Firmware-Familie. Alternativ
lokal per `ufbt` gegen den eigenen mntm-012-SDK-Stand bauen. Zusätzliche Hardware nötig:
mindestens ein nRF24L01+-Modul plus Spannungsregler (AMS1117 3,3 V) und Lötarbeit für die
GPIO-Anbindung — kein reines Software-Upgrade wie bei den bisherigen `.fap`-Funden.

**API-87-Status:** 🔧→überwiegend ✅, lokale Bauprobe empfohlen. Begründung:
- `application.fam` deklariert eine reine GPIO-Kategorie-App ohne erkennbare Abhängigkeit
  von NFC-/SubGHz-Protokoll-Headern — nutzt nur generische FuriHal-/GPIO-/View-APIs, die seit
  Langem stabil sind (dasselbe Architekturmuster wie Metroflip: reine `.fap`-Ebene, kein
  Firmware-Patch).
- Aktivität und Release-Kadenz (letzter Commit vor 3 Tagen, Umbenennung der App-ID erst am
  2026-07-15 zwecks Einordnung in Firmware-Bundle-App-Listen) zeigen laufende Pflege gegen
  aktuelle Firmware-Stände, kein verwaistes Projekt.
- Einzige verbleibende Unsicherheit: die Release-Beschreibungen selbst nennen keine exakte
  mntm-Buildnummer, nur die Firmware-Familie ("Momentum") — die genaue mntm-012-Kompatibilität
  ist damit plausibel, aber nicht explizit belegt. Vor Produktiveinsatz `fz_nrf24_jammer_M.fap`
  lokal laden oder per `ufbt` gegen den eigenen SDK-Stand bauen und mit einem eigenen nRF24-
  Modul verifizieren.

## iButton Brute (W0rthlessS0ul) — Katalog-Eintrag korrigiert

**Befund:** Der Katalog-Eintrag "iButton Brute (W0rthlessS0ul) — Dallas/Cyfral/Metakom-Brute,
Status 🔧" bezieht sich auf `github.com/W0rthlessS0ul/iButton-brute` — das ist **keine
Flipper-Zero-App**, sondern eigenständige Arduino-Firmware für ein separates Gerät (Arduino
Nano, 128×32-OLED, drei Taster, OneWire-Verkabelung). Sie emuliert iButton-Schlüssel nach dem
Dallas-1990-Protokoll aus einer mitgelieferten Codeliste, um damit ausgestattete Türsprechanlagen
zu testen. Kein `application.fam`, kein `.fap`, kein Bezug zu Flipper-Firmware oder API-Stand
— ein Port auf Flipper-Firmware-Ebene ist für dieses Projekt kategorisch nicht zutreffend, der
🔧-Status im Katalog war damit missverständlich formuliert (suggerierte Flipper-API-Migration
statt "separates Zusatzgerät").

**Tatsächliches Flipper-natives Gegenstück gefunden:** `github.com/xMasterX/ibutton-fuzzer`
deckt exakt die im Katalog genannten Protokolle ab (DS1990, Metakom, Cyfral) als echtes
Flipper-`.fap`-Plugin (Kategorie iButton, Einzeldatei-App `ibtnfuzzer.c`/`.h` + `scene/`).
Laut eigenem README "Made for Unleashed firmware, but will work on original firmware too".
Anders als der Name vermuten lässt, ist es ein **Fuzzer** (generiert/sendet variierende
Signale je Protokoll), kein sequenzieller Bruteforce über einen vollständigen Codebereich —
funktional näher an einem De-Bruijn-/Zufalls-Sendetest als an der binärsuch-artigen
Coderaum-Erschöpfung aus dem bereits dokumentierten
`ideas/subghz-fixed-code-bruteforce-konzept.md`.

**Nutzen für mich:** Ergänzt die vorhandene LF-RFID-Fuzzing-Abdeckung (laut Katalog bereits
✅ für T5577/EM4100/HID) um die iButton-Seite (Dallas/Metakom/Cyfral-Türsprechanlagen) — bisher
komplett fehlende Kategorie auf dem Flipper selbst, unabhängig vom separaten Arduino-Projekt.

**Aufwand:** Mittel-hoch, kein reiner Rebuild. Letzter Commit April 2023 — seit über drei
Jahren unverändert, stammt aus der Vor-uFBT-Ära der Flipper-App-Entwicklung. Einzeldatei-Umfang
(`ibtnfuzzer.c`/`.h`, kein größeres Modul) begrenzt den Portaufwand, aber ein einfacher
Neu-Build gegen API 87.1 ist ohne Quellcode-Anpassung unwahrscheinlich erfolgreich — echte
Migrationsarbeit nötig (API-Diff prüfen, gegen aktuelles SDK bauen, iButton-HAL-Aufrufe
gegenprüfen), kein reiner "existiert bereits als .fap"-Fund wie bei Metroflip/nRF24-Jammer.

**API-87-Status:** 🔧 Migration nötig, bestätigt (aber anderes Projekt als im Katalog benannt).
Über drei Jahre ohne Update ist bei der Geschwindigkeit der Flipper-API-Entwicklung ein
starkes Signal für nicht-triviale Breaking Changes seit damals.

## Sicherheitsrahmen

Beide Funde bleiben im bestehenden Rahmen: nRF24-Jammer ausschließlich zur eigenen
RF-Belastbarkeitsprüfung in isolierter/autorisierter Umgebung (keine Anleitung für
Massenstörung Dritter, keine Reichweiten-/Wirksamkeitsangaben Dritter in diesem Repo);
iButton-Fuzzer ausschließlich gegen eigene/autorisierte Türsprechanlagen-Testaufbauten, keine
fremden Zugangscodes oder Capture-Daten. Kein Bezug zu DoS gegen Dritte oder
Detection-Evasion.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

- nRF24-Jammer: `fz_nrf24_jammer_M.fap` lokal auf mntm-012 laden bzw. per `ufbt` bauen, mit
  eigenem nRF24-Modul (siehe `schemes/` im Quellrepo für die Verkabelung) gegenprüfen.
- iButton-Fuzzer: `ibutton-fuzzer` klonen, `ibtnfuzzer.c`/`application.fam` gegen den
  aktuellen mntm-012-SDK-Stand per `ufbt` bauen, API-Diff-Fehler beheben, danach gegen ein
  eigenes DS1990-/Metakom-/Cyfral-Testgerät verifizieren. Bei Erfolg im Katalog als eigener
  Port-Kandidat für `firmware-mining/` bzw. als "läuft" nachtragen.
