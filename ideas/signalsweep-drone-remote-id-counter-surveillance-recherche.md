# Ideen-Recherche: SignalSweep — Drohnen-Remote-ID-Erkennung (ESP32-Companion-Gerät)

Gezielt nach einer Ergänzung für meine bestehende Gegenüberwachungs-/TSCM-Sammlung gesucht
(Nyx/Specter/GhostTag/Argus/Sobe). Dort decke ich bereits Infrarot (Nyx/Sobe), aktive
NFC-Leser (Specter), BLE-Tracker über Verweilzeit (GhostTag) und WiFi-Deauth/Evil-Twin (Argus)
ab — eine Drohnen-/Remote-ID-Erkennung fehlt komplett, ebenso eine zweite, unabhängige
Erkennungsmethode für Flock/Axon-Überwachungsgeräte neben der bereits vorhandenen
`FlipDeFlock`-App. Nach einem existierenden Baustein für genau diese Lücke gesucht statt sie
selbst zu entwerfen.

## Fund: SignalSweep (B0z0dcl0wn/SignalSweep)

**Quelle:** github.com/B0z0dcl0wn/SignalSweep, GPL-3.0-or-later (vendort zusätzlich
`opendroneid-core-c`-Teile wie `opendroneid.c`/`odid_wifi.c` unter Apache-2.0). 372 Commits,
106 Sterne, 16 Forks, eigenes `CHANGELOG.md` — aktiv gepflegt, mehrere aktuelle Forks
(`feralproducts/SignalSweep`, `pbarsamian/SignalSweep-CYD`). Kein Flipper-FAP: eigenständige
Firmware für ein separates ESP32-Board, keine Flipper-API-Berührung.

**Was:** Passiver BLE-/WiFi-Scanner auf einem Seeed Studio XIAO ESP32-C5 (empfohlen,
Dual-Band 2,4/5 GHz plus externe Dual-Band-U.FL-Antenne) bzw. günstiger auf einem XIAO
ESP32-S3 (nur 2,4 GHz). Erkennt drei Kategorien:
- **Tracker:** Apple Find My, Google, Samsung, Tile — Schwärme gefälschter Tags werden zu
  einer Zeile zusammengefasst statt Dauer-Alarm.
- **Überwachungsgeräte:** Flock-Safety-Geräte (während aktiver Übertragung), Axon-Bodycams,
  Smart Glasses — über eine im eigenen Web-/Android-Companion editierbare Signaturliste, kein
  Neu-Flashen für Updates nötig.
- **Drohnen (Remote ID):** dekodiert ASTM-F3411-Remote-ID-Broadcasts über Bluetooth und WiFi,
  inkl. Drohnen-Identität, Höhe, Geschwindigkeit, Kurs und laut eigener Doku auch
  Piloten-Standort.

Kein Display nötig — Alarmierung über passiven Buzzer (vier Muster, drei Lautstärken) plus
optionalen WS2812-LED-Streifen, je Kategorie eigenes Ton-/Lichtmuster (ALPR/Kamera: zwei lange
Töne; Bodycam: lang-kurz-kurz; Drohne: aufsteigender Triller; Tracker: schnelles Ticken).
Konfiguration/Auswertung über eine Chrome-/Edge-WebApp (WebSerial oder BLE) oder eine
Android-APK; Logging standardmäßig aus, Sitzungs-Logs/Paketmitschnitte optional auf
Micro-SD, Export über `pull.py`/eine Datenabruf-Seite per USB.

**Nutzen für mich:** Schließt eine komplette Erkennungslücke (Drohnen-Remote-ID), die in
meiner gesamten TSCM-Sammlung bisher nicht existiert — IR/NFC/BLE/WiFi sind abgedeckt, Luftraum
nicht. Zusätzlich eine zweite, methodisch unabhängige Erkennung für Flock/Axon-Geräte neben
`FlipDeFlock` (dort Eigenbau-Heuristik, hier aktiv gepflegtes Fremdprojekt mit editierbarer
Signaturliste) sowie für BLE-Tracker neben `GhostTag` (dort Verweilzeit-basiert, hier
Direkterkennung über Hersteller-Payload). Sofort als eigenständiges Zweitgerät neben dem
Flipper nutzbar, ganz ohne eigenen Firmware-/App-Aufwand.

**Firmware-/Kompatibilitäts-Status:** ❓ kein Flipper-Bezug — läuft vollständig auf einem
eigenen ESP32 (XIAO C5/S3), kein `.fap`, keine Berührung mit `mntm-012`/API 87.1. Eine
künftige Anbindung nach demselben Muster wie bei `Argus` (ESP32-WiFi-Devboard als
UART-Companion, Flipper übernimmt nur die Anzeige) wäre denkbar, ist aber aus der
vorhandenen Dokumentation (`docs/README-full.md`) nicht ableitbar: dort ist nur ein
zeilenbasiertes JSON-/Rohbefehls-Protokoll über native USB-CDC beschrieben (`CMD:CFG`,
`CMD:SIGS:RESET`, `CMD:RXONLY:ON|OFF`, `CMD:BLE_SCAN:ON|OFF`, `CMD:WIFI_SCAN:ON|OFF`,
`CMD:HOST`-Keepalive alle 2 s, `CMD:HOST:BYE`, `{"beep_mask":N}`/`{"led":0..3}`-Settings,
beide Felder bleiben über einen Stromzyklus erhalten) — ein Antwortformat für einzelne
Erkennungs-Events (Drohne/Tracker/Kamera, mit welchen Feldern) ist in der Doku nirgends
spezifiziert, einzige dokumentierte Antwort ist `CMD:CFG` (Gerätename, Adressmodus,
Hunt-Ziel/-Filter, Receive-Only-Status, Scan-Status je Funkmodul, Beep-Maske, Buzzer-/
LED-Modus, Alarmzähler, Uptime). Baudrate/UART-Pin-Belegung für eine Nicht-USB-Anbindung
ebenfalls nicht dokumentiert (Verweis auf `firmware/WIRING.md`, nicht eingesehen). Eine
Flipper-Anbindung wäre also nur nach eigener Quelltextsichtung der tatsächlichen Firmware
(nicht nur der Doku) seriös planbar — für diese Sitzung bewusst nicht vertieft, siehe unten.

**Sicherheitsrahmen:** Deckt sich mit meinem eigenen Rahmen — das Projekt bezeichnet sich
selbst ausdrücklich als reinen Empfänger ("It's a receiver", "The only winning move is not to
transmit"), keine Jamming-/Deauth-/Injection-/Spoofing-Funktion. Die wenigen eigenen
Sendevorgänge (abschaltbares BLE-Advertisement, WiFi-/BLE-Scan-Requests, ein einzelner
Schreibzugriff beim "Ring"-Feature für Tracker — für die Drohnenerkennung irrelevant) sind
dokumentiert und begrenzt. Das Projekt weist selbst darauf hin, dass Zuhören an den meisten
Orten legal, Senden dagegen meist nicht ist — passt zu meinem eigenen defensiven/autorisierten
Rahmen (reine Erkennung statt Angriff).

**Risiko:**
- **Lizenz:** GPL-3.0-or-later (eigene Firmware) plus Apache-2.0-vendorte
  `opendroneid-core-c`-Teile — bei reiner Standalone-Nutzung als eigenständiges Zweitgerät
  unkritisch (kein Quelltext-Vendoring in dieses Repo); erst relevant, falls ich jemals Teile
  des Quelltexts selbst übernehmen wollte.
- **Dokumentationslücke beim Event-Protokoll:** wie oben beschrieben kein spezifiziertes
  Pro-Erkennung-Ausgabeformat über die serielle/USB-CDC-Schnittstelle — vor jeder eigenen
  Integrationsidee (Flipper-UART-Companion o. Ä.) erst den tatsächlichen Firmware-Quelltext
  lesen, nicht nur die Doku.
- **Hardwarebeschaffung:** benötigt ein noch nicht vorhandenes XIAO-ESP32-C5-Board (oder
  günstiger S3, dann nur 2,4 GHz, kein WiFi-Remote-ID über 5-GHz-NAN) plus externe
  Dual-Band-Antenne — reine Anschaffungsfrage, kein technischer Fund.
- **Jung/schnell bewegtes Projekt:** sichtbare Fork-Aktivität mit teils noch nicht an echter
  Hardware verifizierten Änderungen (z. B. `feralproducts/SignalSweep` PR #1 selbst als
  "noch nicht auf echter Hardware gelaufen" markiert) — vor Einsatz den tatsächlich
  verwendeten Commit festhalten und bei einem Update erneut prüfen.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

Anschaffung eines XIAO ESP32-C5 (oder ersatzweise S3) plus Antenne, Firmware vom
Haupt-Repository (nicht von einem experimentellen Fork) flashen, danach als eigenständiges
Zweitgerät neben dem Flipper bei einer autorisierten TSCM-Begehung gegen ein eigenes/
autorisiertes Testdrohnenflugzeug und einen eigenen BLE-Tracker verifizieren. Eine engere
Flipper-Anbindung (UART-Companion nach `Argus`-Muster) erst nach eigener Sichtung des
tatsächlichen Firmware-Quelltexts (nicht nur der Doku) als eigenständigen Rechercheauftrag
für eine künftige Sitzung aufgreifen.
