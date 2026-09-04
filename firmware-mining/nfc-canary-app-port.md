# NFC Canary — externe App aus RogueMaster-Bündel (passiver Skimmer-/Reader-Detektor)

## Was
Ich habe den weiterhin offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster aus dem
RogueMaster-Bündel" (`ideas/pentest-app-katalog.md`) fortgesetzt und **NFC Canary**
(`applications/external/nfc_canary/`, Autor `antitree`, v0.2) geprüft — mit 1608 Zeilen über
12 C-Dateien vollständig gelesen. Zwei weitere Kandidaten aus demselben Rückstellpunkt habe ich
dabei bewusst zurückgestellt statt weiter zu vertiefen (siehe Abschnitt "Zusätzlich geprüft,
zurückgestellt" unten).

Die App ist ein reiner Defensiv-Detektor: sie überwacht das eingebaute ST25R3916-Feld-
Detektor-Bit (`furi_hal_nfc_field_is_present()`) und alarmiert (Vibration/Ton/LED), sobald ein
13,56-MHz-Reader-Feld anliegt — Zweck ist, unbemerktes Auslesen der eigenen Wallet/Karten in
Tasche oder Rucksack zu erkennen (klassischer "NFC-Skimming"-Gegenfall). Eine passive Karte
selbst löst nichts aus, da sie kein eigenes Feld erzeugt — das ist im Quelltext-Kommentar
korrekt dokumentiert und deckt sich mit der ST25R3916-Hardware (External Field Detector, rein
passiv, sendet nichts). Damit ergänzt sie die bereits verifizierten defensiven Kategorien
(SubGHz Jammer Detector, Rogue AP Detector, UART Sniff) um eine vierte: passive
NFC-Interrogations-Erkennung.

Architektonisch sauber: eigener Worker-Thread pollt alle 20 ms, asymmetrisches Latch
(sofortiger Trigger bei erster steigender Flanke, Halten bis 750 ms Feldstille) statt
symmetrischer Entprellung — im Quelltext-Kommentar begründet mit einer echten Beobachtung
("raw edges climbed while detections stayed at 0"), weil reale Reader das Feld in Bursts
pulsen statt konstant zu senden. Der `furi_hal_nfc_acquire()`-Rückgabewert wird korrekt geprüft
(anders als beim zuvor gefundenen NFC/RFID-Detector-Fehler, `firmware-mining/nfc-rfid-detector-
app-port.md`) und bei Fehlschlag sauber auf einen "NFC nicht verfügbar"-Status abgebildet statt
blind weiterzumachen. Beim App-Beenden wird das Feld immer sauber abgeschaltet
(`furi_hal_nfc_field_detect_stop()` + `low_power_mode_start()` + `release()`), auch aus dem
Worker-Thread-Exit-Pfad.

## API-87-Kompatibilität (mntm-012)
Alle sechs verwendeten NFC-HAL-Funktionen habe ich gegen den eigenen `mntm-012`-Tag-Stand
geprüft (`targets/furi_hal_include/furi_hal_nfc.h`, Sparse-Checkout):

```
furi_hal_nfc_acquire(void)              -> FuriHalNfcError   (Zeile 121)
furi_hal_nfc_release(void)              -> FuriHalNfcError   (Zeile 132)
furi_hal_nfc_low_power_mode_start(void) -> FuriHalNfcError   (Zeile 142)
furi_hal_nfc_low_power_mode_stop(void)  -> FuriHalNfcError   (Zeile 152)
furi_hal_nfc_field_detect_start(void)   -> FuriHalNfcError   (Zeile 177)
furi_hal_nfc_field_detect_stop(void)    -> FuriHalNfcError   (Zeile 184)
furi_hal_nfc_field_is_present(void)     -> bool               (Zeile 191)
```

Identische Signaturen, kein Firmware-Patch nötig. `application.fam` zielt bereits auf `f7`,
keine Zusatzhardware (nur eingebaute NFC-Antenne). Restliche Abhängigkeiten
(`storage/storage.h`, `toolbox/stream/file_stream.h`, `toolbox/saved_struct.h`,
`notification/notification.h`, `gui/gui.h`) sind Standard-Firmware-APIs, die ich bereits bei
mehreren vorherigen Ports (u. a. NFC Sniffer, SubGHz Jammer Detector) gegen denselben
`mntm-012`-Stand verifiziert habe.

## Fund: "Sound"-Alarmkanal ist mit Standardeinstellung bei echter Erkennung nie erreichbar
`nfc_canary_i.h` definiert vier Bedrohungsstufen (`ThreatTier`): `None` → `Info` (kurzer
Feldimpuls, nur geloggt) → `Warn` (anhaltendes Feld, wahrscheinlich auf einen gerichtet) →
`Alarm` (Reader hat tatsächlich adressiert/Kommando gesendet — reserviert für den geplanten
"Decoy"-Modus). Der `Decoy`-Modus (aktives Card-Emulieren zum Mitschneiden von Protokoll/
Kommando) ist im Quelltext vorbereitet (`DetectMode`-Enum, `AlertEvent.protocol`/`cmd`-Felder,
vollständige Kommando-Namenstabelle in `eventlog.c`), aber im Worker (`nfc_canary.c`) **nicht
verdrahtet** — der Entwickler hat das selbst sauber dokumentiert und den Modus-Umschalter
deshalb bewusst aus dem Einstellungsmenü entfernt (`settings_menu.c`, Kommentar: "Decoy is not
wired into the detection worker... Re-add when the listener path lands (v0.3)"). Das ist kein
Fund — sauber gehandhabt.

Der tatsächliche Fund liegt in den **Standardwerten** für die beiden Alarmkanal-Schwellen
(`settings.c`, `settings_defaults()`):

```c
s->min_tier_vibro = TierWarn;   // Vibration ab "anhaltendes Feld"
s->min_tier_sound = TierAlarm;  // Ton erst ab "Reader hat adressiert" — Decoy-only!
```

Da der Worker im aktuell ausgelieferten Sentinel-only-Build (0.2) `TierAlarm` **niemals**
erreicht (nur `TierNone`/`TierInfo`/`TierWarn` werden gesetzt, siehe `nfc_canary.c` Zeilen
96–176), ist `min_tier_sound = TierAlarm` bei jeder echten Erkennung unerreichbar — der
Ton-Kanal ist mit den Standardeinstellungen dauerhaft stumm, obwohl `sound_pattern` (Standard:
`PatternPulse`) und `volume` (Standard: 10) so aussehen, als sei er vollständig konfiguriert.

Verschärfend kommt hinzu, wie die Test-Funktion im Einstellungsmenü (`alert_test()`, `alert.c`)
funktioniert: sie setzt `a->tier` direkt auf `TierAlarm`, unabhängig von jeder echten Erkennung
— `tier_enabled(min_tier, tier)` prüft nur `tier >= min_tier`, und `TierAlarm` ist der höchste
Enum-Wert, also `tier_enabled(x, TierAlarm)` ist für **jeden** gültigen `min_tier_sound`-Wert
wahr. Wer im Einstellungsmenü auf OK drückt, um den Alarm zu testen ("Audition"), hört den Ton
**immer** — unabhängig davon, ob eine echte Erkennung ihn je auslösen könnte. Das
Einstellungsmenü vermittelt damit aktiv falsches Vertrauen: der Test "funktioniert", aber im
Feld (Tasche, echte Interrogation) bleibt der Ton für die gesamte Sentinel-only-Laufzeit der
App stumm — nur Vibration und LED lösen tatsächlich aus. Für ein Gerät, dessen einziger Zweck
das Alarmieren ist, ist ein Kanal, der beim Selbsttest funktioniert und live nie feuert, der
schlechteste Fehlermodus: er fällt nicht auf, bis er gebraucht wird.

## Host-Repro
`firmware-mining/repro-nfc-canary-sound-tier-gate/repro.c` bildet `tier_enabled()`
(`alert.c`) unverändert nach und spielt beide real erreichbaren Stufen (`Info`, `Warn`) sowie
die Test-Audition-Stufe (`Alarm`) gegen ALT- und NEU-Standardwert von `min_tier_sound` durch.
Mit `gcc -O1 -Wall -Wextra -fsanitize=address,undefined` gebaut und ausgeführt:

- ALT (`min_tier_sound = Alarm`): 0/2 echte Erkennungsstufen lösen Ton aus, Audition löst
  trotzdem aus (`tier_enabled` bekommt `Alarm` direkt injiziert) — bestätigt den blinden Fleck.
- NEU (`min_tier_sound = Warn`): 1/2 (die `Warn`-Stufe, dieselbe, bei der auch Vibration
  auslöst) löst Ton aus, Audition weiterhin erfolgreich — bestätigt die Reparatur.

## Betroffene Datei
- `applications/external/nfc_canary/settings.c` — `settings_defaults()`, ein Feld.

## Port-Skizze (minimaler Fix, ein Zeile geändert)
```c
// settings.c, settings_defaults() — ALT:
s->min_tier_sound = TierAlarm;

// NEU:
s->min_tier_sound = TierWarn; /* Alarm ist im Sentinel-only-Build (v0.2) unerreichbar --
                                * Ton triggert jetzt zusammen mit Vibration bei Warn, statt
                                * dauerhaft stumm zu bleiben. Sobald der Decoy-Listener-Pfad
                                * (v0.3) landet und Alarm real erreichbar wird, hier erneut
                                * prüfen, ob eine eigene, spätere Ton-Schwelle wieder sinnvoll
                                * ist. */
```
Restlicher Funktionskörper unverändert. Kein Firmware-Patch, reine App-Konfiguration — beim
eigenen lokalen Bau von `nfc_canary.fap` einzubringen (Datei liegt nicht im eigenen
`mntm-012`-Firmware-Baum, sondern wird als externe App aus dem RogueMaster-Bündel übernommen).

## Testhinweis (lokal, ohne Zusatzhardware)
1. App unverändert bauen/flashen, Einstellungen öffnen, "Sound" auf einen hörbaren Pattern/
   Volume stellen, "Sound from" ist in v0.2 nicht im Menü sichtbar (siehe oben) — mit einem
   NFC-fähigen Smartphone (Reader-Modus an) kurz ans Antennenfeld halten und wieder wegnehmen:
   Vibration löst aus (>= Warn nach ca. 300 ms anhaltendem Feld), Ton bleibt stumm.
2. Fix einbringen, neu bauen/flashen, denselben Test wiederholen: Ton löst jetzt gleichzeitig
   mit der Vibration aus, sobald das Feld die `TIER_WARN_MS`-Schwelle (300 ms) überschreitet.
3. Test-Audition in den Einstellungen (OK-Taste) bleibt in beiden Fällen unverändert hörbar —
   das war schon vorher so und ist kein Regressionsrisiko.

## Risiko
Sehr gering: einwertige Konstantenänderung, kein Einfluss auf Erkennungslogik, Feld-Handling
oder Persistenz. Einzige Verhaltensänderung: der Ton-Kanal löst jetzt tatsächlich bei echter
Erkennung aus (das ist der Zweck des Fixes, kein Nebeneffekt). Keine Auswirkung auf bereits
gespeicherte `settings.bin`-Dateien, da `saved_struct_load()` nur beim allerersten Start ohne
vorhandene Datei die neuen Defaults zieht — bestehende Installationen mit bereits gespeicherten
(alten) Einstellungen sind vom Fix nicht betroffen und müssten die Schwelle manuell anpassen,
sobald "Sound from" im Menü wieder freigeschaltet wird.

## Quelle
`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`, Pfad
`applications/external/nfc_canary/` (Sparse-Checkout, Commit-Stand dieser Sitzung). Oberstrom-
Repo laut `application.fam`/`fap_weburl`: `antitree/nfc_canary`.

## Zusätzlich geprüft, zurückgestellt
Beim Durchsehen des restlichen Cluster-Rückstands zwei weitere Kandidaten kurz geprüft und
bewusst nicht vertieft:
- **Chameleon** (`applications/external/chameleon/`, Autor `Chameleon-Ultra`) — laut
  eigenem README explizit ein USB/BLE-Fernsteuerungsclient für ein externes Chameleon-Ultra-
  Gerät, kein eigenständiges NFC-Werkzeug über die Flipper-Antenne. Gleiche Kategorie wie
  `can_commander` (bereits zurückgestellt wegen Fremdhardware) — kein Portaufwand ohne eigenes
  Chameleon-Ultra-Gerät gerechtfertigt.
- **NFC Relay** (`applications/external/nfc_relay/`) — benötigt laut eigenem README zwei
  Flipper Zero, verbunden per UART (fest verdrahtet auf Pin 15/16), zum Weiterleiten von
  NFCA-APDU/Response zwischen beiden Geräten. Kein Fremdgerät im engeren Sinn, aber ein zweites
  eigenes Flipper-Gerät als Voraussetzung — aus Ressourcengründen vorerst zurückgestellt statt
  wie üblich vollständig geprüft.

Restlicher Cluster (`mifare_fuzzer`, `nfc_comparator`, `nfctools`, `uhf_rfid`,
`simultaneous_rfid_reader`) weiterhin offen — `uhf_rfid` und `simultaneous_rfid_reader`
benötigen laut eigenem README zusätzlich ein externes UHF-Lesegerät (M6e Nano/M7E Hecto/
YRM100), vermutlich ebenfalls Fremdhardware-Fall wie Chameleon — noch nicht abschließend
geprüft.
