# Recherche-Auftrag abgearbeitet: nRF24-Sniffer/Mousejack API-87-Status

Aus `pentest-app-katalog.md` aufgegriffen: Zeile "nRF24 Sniffer/Mousejack | 2.4G-Tastatur/Maus-
Angriffe | 🔧 | Modul" stand seit Katalog-Erstellung offen. Diese Sitzung verifiziert Quelle,
Pflegestand und API-Oberfläche anhand des öffentlichen Quellcodes (keine echten Kartendaten/
Captures übernommen) und grenzt den Fund gegen den bereits dokumentierten nRF24-Jammer
(`ideas/ibutton-brute-nrf24-jammer-api-status.md`) ab — dort ging es um 2,4-GHz-Störsendung
(`W0rthlessS0ul/FZ_nRF24_jammer`), hier um Adress-Sniffing plus HID-Injection gegen unverschlüsselte
Logitech-/Microsoft-Funkmäuse und -tastaturen (Mousejack-Schwachstellenklasse).

## Ursprungsquelle vs. tatsächlich relevante Quelle

`mothball187/flipperzero-nrf24` (die ursprüngliche, oft zitierte Quelle für „NRF24 Sniffer" +
„Mouse Jacker") ist seit dem letzten Commit vom 27.08.2022 unverändert und besitzt **keine**
`application.fam` — vor-Manifest-Ära der Flipper-App-Entwicklung, kein direkter `ufbt`-Build
gegen API 87.1 zu erwarten.

Die tatsächlich relevante, weiterhin gepflegte Quelle ist der RogueMaster-Firmware-Baum
(`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`, `applications/external/`): dort
liegen beide Apps als eigenständige, vollwertige `.fap`-Ziele mit aktuellem `application.fam`
vor (`nrf24sniff`, App-ID `sniffer`; `nrf24mousejacker`, App-ID `mouse_jacker`; beide
`fap_author="mothball187 & xMasterX"`, Kategorie `GPIO/NRF24`) — xMasterX hat den Treiber/die
Apps offensichtlich unabhängig vom erloschenen Upstream-Repo weitergepflegt und ans aktuelle
Firmware-Manifest-Format angepasst.

Im selben Bündel vier weitere, eigenständige nRF24-Apps mit derselben Kategorie gefunden
(bislang nicht im Katalog erfasst):

| App (Ordner) | Autor/Quelle | Funktion | Letzter Upstream-Commit |
|---|---|---|---|
| `nrf24channelscanner` | htotoo | Kanalbelegung/RSSI-Balken über alle 2,4-GHz-Kanäle | Upstream-Repo `htotoo/NRF24ChannelScanner` inzwischen **gelöscht/verschoben** (HTTP 404) — nur noch die RogueMaster-Kopie erhalten |
| `nrf24scan` | vad7 | Sniff+Scan mit Logging (`log-xx.txt`) und Resend, eigener 555-Zeilen-Treiber | 2023-06-10 |
| `nrf24batch` | vad7 | Batch-Kommandos (Read/Write/Set/Listen) gegen eigene AVR-Gegenstellen, konfigdatei-gesteuert | 2024-11-02 |
| `nrf24monitor` | CyberDemon73 | reiner SPI-Verbindungstest/Register-Reader (kein Sniff/Jack) | 2025-04-02 (frischeste Quelle der Gruppe) |

## Nutzen für mich

Schließt die im Katalog als "Modul" vage stehende 2,4-GHz-HID-Angriffslücke konkret:
- **Sniffer+Mouse Jacker** decken die eigentliche Mousejack-Kette ab (Adresse sniffen → HID-
  Report-Injection über dieselbe Adresse, inkl. eigenem Ducky-Payload-Support laut
  `mousejacker_ducky.c/.h`) — Ergänzung zu meinem bestehenden BadUSB/Mouse-Click-Recorder-
  Repertoire um eine funkbasierte statt kabelgebundene HID-Angriffsfläche, ausschließlich zum
  Testen eigener/autorisierter Funkperipherie auf die seit 2016 bekannte, bei vielen günstigen
  Nicht-Logitech-Unifying-Empfängern bis heute unbehobene Mousejack-Schwachstellenklasse.
- **Channel Scanner** und **Monitor** sind reine Diagnose-Apps ohne Angriffsfunktion — sinnvoll
  als Vorstufe, um ein neu verkabeltes nRF24-Modul (Lötarbeit, siehe Pinout unten) erst auf
  korrekte SPI-Verbindung zu prüfen, bevor Sniffer/Mouse-Jacker überhaupt sinnvoll starten
  können.
- **Batch** ist für mich aktuell nicht relevant (zielt auf eigene AVR-Mikrocontroller-Projekte
  als Gegenstelle, nicht auf Pentest-Ziele) — der Vollständigkeit halber im Katalog vermerkt,
  aber kein Portkandidat für diese Sitzung.

## API-87-Status: 🔧→überwiegend ✅, lokale Bauprobe empfohlen

Begründung, nach demselben Muster wie beim bereits verifizierten nRF24-Jammer:
- Alle vier geprüften Treiber-Varianten (`nrf24sniff`/`nrf24mousejacker` identischer Treiber,
  `nrf24channelscanner` eigener schlanker 126-Zeilen-Treiber, `nrf24scan` eigener 555-Zeilen-
  Treiber, `nrf24monitor` direkt in `application.c`) nutzen ausschließlich dieselbe, seit Jahren
  stabile Low-Level-API: `furi_hal_spi_bus_handle_init/deinit`, `furi_hal_spi_acquire/release`,
  `furi_hal_spi_bus_trx`, `furi_hal_gpio_init(_simple)`, `furi_hal_gpio_write`, über ein
  `furi_hal_spi_bus_handle_external`-Handle (externer SPI-Bus, kein interner Onboard-Bus) — exakt
  dasselbe Architekturmuster, das ich beim CAN-Bus-Port bereits gegengeprüft und für mntm-012
  bestätigt habe (`firmware-mining/canbus-attack-app-port.md`: "Standard-`furi_hal_spi_bus_r`/
  `FuriHalSpiBusHandle`-Mechanismus, identisch zum Muster, das auch offizielle Firmware-Apps für
  externe SPI-Module verwenden").
- Alle vier Apps besitzen ein vollständiges, aktuelles `application.fam` (Pflicht-Manifest der
  heutigen `ufbt`/App-Store-Ära) — im Gegensatz zum manifestlosen Original-Upstream-Repo.
- Reale Nutzungsbestätigung auf meiner exakten Firmware-Familie: GitHub-Issue #329 im
  `Next-Flip/Momentum-Firmware`-Repo dokumentiert einen Anwender, der `nrf24sniff.fap` unter
  `/ext/apps/GPIO/NRF24/nrf24sniff.fap` auf Momentum installiert und gestartet hat — die App
  lädt und reagiert auf Tastendruck (kein Absturz/Fehlermeldung beim Start selbst), reagiert beim
  Sniff-Versuch aber mit Buzz+rotem LED statt Sniffing. Das liest sich für mich eher nach einer
  fehlgeschlagenen Funkchip-Erkennung (falsche Verkabelung/fehlender Kondensator/Modul nicht
  erkannt) als nach einem API-Bruch, da der App-Start selbst funktioniert — ich konnte den
  vollständigen Kommentarverlauf des Issues aber nicht einsehen und werte das daher nur als
  Indiz, nicht als bestätigte Ursache.
- `mntm-012` als Tag im `Next-Flip/Momentum-Firmware`-Repo gegengeprüft (existiert, per
  Shallow-Fetch bestätigt) — die Apps selbst liegen dort nicht im Firmware-Quellbaum
  (`applications/`/`applications_user/`), sondern werden separat über den App-Katalog installiert
  (`.fap`-Store, wie beim Issue-Nutzer zu sehen), analog zu vielen anderen GPIO-Modul-Apps.
- Einzige verbleibende Unsicherheit: keine der vier Quellen nennt eine exakte mntm-Buildnummer,
  nur das allgemeine Architekturmuster und die reale Momentum-Installation aus dem Issue.

## Aufwand

Gering für Sniffer/Mouse-Jacker/Channel-Scanner/Monitor: entweder direkt über den offiziellen
App-Katalog auf mntm-012 installieren (wie im Issue-Beispiel gesehen) oder die vier
RogueMaster-Ordner per `ufbt build` gegen den eigenen mntm-012-SDK-Stand lokal bauen — reine
Software-App-Ebene, kein Firmware-Patch. Zusätzliche Hardware nötig: ein nRF24L01(+)-Modul,
Verkabelung laut Pinout in `nrf24sniff`/`nrf24mousejacker` (2/A7→MOSI, 3/A6→MISO, 4/A4→CSN,
5/B3→SCK, 6/B2→CE, GND, 3V3), optional Kondensator (3,3–10 µF + 0,1 µF parallel) bei
Instabilität — identische Verkabelungsklasse wie beim bereits dokumentierten nRF24-Jammer.

## Sicherheitsrahmen

Bleibt im bestehenden Rahmen: Sniffer/Mouse-Jacker ausschließlich gegen eigene/autorisierte
Funkmäuse und -tastaturen in isolierter Testumgebung, keine Anleitung für Massenangriffe gegen
Dritte, keine echten gesniffeten Geräteadressen oder Payload-Inhalte Dritter in diesem Repo.
Channel Scanner/Monitor sind reine Diagnosewerkzeuge ohne Angriffsfunktion.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

- `nrf24sniff.fap`/`nrf24mousejacker.fap` zuerst über den offiziellen App-Katalog auf mntm-012
  installieren und mit `nrf24monitor`/`nrf24channelscanner` die SPI-Verkabelung des eigenen
  Moduls verifizieren, bevor der eigentliche Sniff/Jack-Versuch läuft (Lehre aus Issue #329:
  Verkabelung zuerst prüfen, um denselben Buzz+Rot-LED-Fehlschlag zu vermeiden).
- Falls der Katalog keine mntm-012-Variante anbietet: die vier Ordner aus
  `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`) per `ufbt` gegen den eigenen
  mntm-012-SDK-Stand bauen, danach gegen eine eigene Funkmaus/-tastatur verifizieren.
- Bei Erfolg im Katalog (`ideas/pentest-app-katalog.md`) von 🔧 auf ✅ korrigieren, analog zum
  bereits erledigten Subbrute-/Metroflip-Eintrag.
