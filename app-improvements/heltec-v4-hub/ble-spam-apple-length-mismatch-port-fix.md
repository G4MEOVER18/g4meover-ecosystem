# heltec-v4-hub — Apple-BLE-Spam-Advertisement trägt denselben falschen Längen-Fehler wie zuvor bei lilygo-ukfe-rx, Fix nie zurückportiert

## Was
Ich habe `src/ble_spam.cpp` von `heltec-v4-hub` zum ersten Mal vollständig gelesen (bisher nur
`src/main.cpp`/`src/ukfe_rf.h` geprüft; dieses Gerät hatte noch keine eigene App-Verbesserungs-
Sitzung). Dabei zeigte sich, dass `adv_apple()` in diesem Gerät **byte-identisch** zu der
Fassung ist, die ich in einer früheren Sitzung bereits in `lilygo-ukfe-rx` gefunden und
korrigiert habe (`app-improvements/lilygo-ukfe-rx/ble-spam-apple-length-mismatch-fix.md`):

```cpp
static void adv_apple() {
    uint16_t model = APPLE_MODELS[esp_random() % APPLE_MODEL_COUNT];
    uint8_t d[17] = {
        0x4C, 0x00,             // Apple Company ID
        0x07, 0x0F,             // Typ 0x07 (Proximity Pairing), Laenge
        0x05, 0xC1,             // Sub-Typ + Prefix
        (uint8_t)(model >> 8), (uint8_t)(model & 0xFF),
        0x00,                   // Status
        0x00, 0x00, 0x00,       // Batterie/Flags
        0x00,
        (uint8_t)esp_random(), (uint8_t)esp_random(), (uint8_t)esp_random(),
        0x00,
    };
    NimBLEAdvertisementData ad;
    ad.setManufacturerData(std::string((char*)d, sizeof(d)));
    NimBLEDevice::getAdvertising()->setAdvertisementData(ad);
}
```

Dieselbe TLV-Struktur wie beim damaligen Fund: `Company-ID(2 Byte) Typ(1 Byte) Länge(1 Byte)
Payload(Länge Byte)`. Dem Längen-Byte (Index 3, Wert `0x0F` = 15) folgen im 17-Byte-Array
tatsächlich nur `17 - 2 - 1 - 1 = 13` weitere Bytes (Index 4 bis 16). Das deklarierte Längen-Feld
weicht also exakt wie bei `lilygo-ukfe-rx` um 2 Bytes von der tatsächlichen Payload-Länge ab.

**Warum dasselbe hier nochmal auftaucht:** `heltec-v4-hub` und `lilygo-ukfe-rx` sind zwei
getrennte Satelliten-Repos (eigene `G4MEOVER18/...`-Quelle, eigene Git-Historie), die aber beim
Aufbau offensichtlich vom selben `ble_spam.cpp`-Ausgangsstand abgezweigt wurden — nicht nur die
Struktur, sondern auch Kommentare, Variablennamen und das falsche Längen-Byte sind Zeile für
Zeile identisch. Der Fix von damals wurde nur in `lilygo-ukfe-rx` eingepflegt; in
`heltec-v4-hub` (eigenständiges Repo, eigener Build) ist der ursprüngliche Fehler unverändert
vorhanden, Commit `258d4adb8a6d977255c72f72780039ac13a35a05` (aktueller `main`-Stand).

**Auswirkung (identisch zur damaligen Einordnung):** jedes gesendete Advertisement ist als
Apple-Continuity-Nachricht intern inkonsistent. Ein TLV-konformer Empfänger (ein echtes
iPhone/iPad/Mac, aber auch ein Analyse-Werkzeug wie Wireshark mit Apple-Continuity-Dissector)
liest nach dem Längen-Byte 15 statt der tatsächlich vorhandenen 13 Bytes und verwirft eine
solche Nachricht typischerweise als ungültig. `ble_spam_tick()` meldet dabei keinen Fehler,
`adv->start()` liefert unabhängig vom Payload-Inhalt Erfolg — das Popup auf dem Zielgerät bleibt
mit hoher Wahrscheinlichkeit aus, ohne dass das Werkzeug das anzeigt. Betrifft denselben Umfang
wie damals: den reinen Apple-Modus (`ble_spam_start(1)`) sowie den Apple-Anteil der
rotierenden Modus-0-Advertisements. `adv_windows()`/`adv_android()` sind unverändert nicht
betroffen (kein eigenes, manuell gepflegtes Längen-Byte).

## Nutzen
- Schließt dieselbe stille Funktionslücke wie bei `lilygo-ukfe-rx`, jetzt aber für das zweite
  Gerät, das denselben Code-Ausgangsstand verwendet — ohne diesen Nachzug würde `heltec-v4-hub`
  dauerhaft hinter einem bereits bekannten und gelösten Fund zurückbleiben.
- Rein additiv und minimal: ein einziges falsches Literal (`0x0F` → `0x0D`), identisch zum
  bereits verifizierten `lilygo-ukfe-rx`-Fix — keine neue Variable, keine neue Funktion, keine
  geänderte Kontrollstruktur.
- Da beide Geräte (`heltec-v4-hub`, `lilygo-ukfe-rx`, und laut `ukfe-esp-now-mesh`-Funden auch
  `heltec-ukfe-rx`) über ESP-NOW im selben Mesh hängen, lohnt es sich, bei zukünftigen Funden in
  einer dieser Dateien jeweils gegen die anderen Satelliten-Repos gegenzuprüfen, bevor ein Fund
  als "nur dieses eine Gerät betreffend" abgelegt wird — vorgemerkt für kommende Sitzungen
  (`heltec-ukfe-rx/src/ble_spam.cpp` ist noch nicht gegen diesen konkreten Fund geprüft, siehe
  "Offen").

## Betroffene Dateien
- `src/ble_spam.cpp` (`heltec-v4-hub`) — `adv_apple()`, Index 3 des lokalen `d[17]`-Arrays: aus
  `0x0F` wird `0x0D` (13 Bytes folgen tatsächlich nach dem Längen-Byte). Keine weitere Datei
  betroffen — das Array ist rein lokal, keine Schnittstelle nach außen, kein Zugriff aus
  `main.cpp` oder `ble_spam.h`.

## Risiko
Sehr gering, exakt wie beim ursprünglichen `lilygo-ukfe-rx`-Fund: einzelnes Byte-Literal in
einem bereits bestehenden, rein lokalen Array geändert; Array-Größe (17), alle übrigen 16 Bytes,
die Modell-Auswahl aus `APPLE_MODELS`, die drei Zufallsbytes sowie der nachfolgende
`setManufacturerData()`/`setAdvertisementData()`-Aufruf bleiben unverändert. Ändert nichts an
`adv_windows()`, `adv_android()`, `randomize_mac()`, `ble_spam_tick()`s Rotations- oder
Timing-Logik oder `ble_scan()`.

## Port-Skizze / Fix
In `adv_apple()`, zweite Zeile der Array-Initialisierung:

```c
uint8_t d[17] = {
    0x4C, 0x00,             // Apple Company ID
    0x07, 0x0D,             // Typ 0x07 (Proximity Pairing), Laenge (13 Bytes nach diesem Feld)
    0x05, 0xC1,             // Sub-Typ + Prefix
    ...
};
```

Unified-Diff gegen die unveränderte `G4MEOVER18/heltec-v4-hub`-Basis (`main`, Commit
`258d4adb8a6d977255c72f72780039ac13a35a05`) in
[`ble-spam-apple-length-mismatch-port-fix/ble_spam_cpp.patch`](ble-spam-apple-length-mismatch-port-fix/ble_spam_cpp.patch)
— per `patch -p1 --dry-run`/`patch -p1` sowie unabhängig per `git apply --check`/`git apply`
gegen zwei frische, unabhängige Zweitkopien angewendet; beide Ergebnisse per `md5sum`
gegeneinander abgeglichen — identisch (`b62783105bde334c30ac03b2e384b7e1`, derselbe Hash wie
beim ursprünglichen `lilygo-ukfe-rx`-Fix, da beide Dateien bis auf dieses eine Byte
byte-identisch sind).

## Testhinweis
Host-Repro (kein Arduino/ESP32/NimBLE-Abhängigkeit — bildet den `d[17]`-Aufbau von `adv_apple()`
in ALT- und NEU-Fassung Byte für Byte nach; die Prüfung selbst ist reine Struktur-Arithmetik,
unabhängig vom tatsächlichen Funkverhalten, identisch zum damaligen `lilygo-ukfe-rx`-Repro, nur
mit dem aktuellen `heltec-v4-hub`-Commit referenziert):

```
gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro
./repro
```

[`repro-ble-spam-apple-length-mismatch-port/repro.c`](repro-ble-spam-apple-length-mismatch-port/repro.c)
— **23 Prüfungen**, lokal unter `-O0` und `-O1` ausgeführt (identischer Output, keine
ASan-/UBSan-Meldungen, keine Compiler-Warnungen): **ALL CHECKS PASSED (23 total)**. Geprüft, je
für drei verschiedene Modell-/Zufallsbyte-Kombinationen: (1) die tatsächliche Byte-Zahl nach dem
Längen-Feld ist 13; (2) ALT deklariert `0x0F` (15) und weicht damit nachweislich von den 13
tatsächlichen Bytes ab; (3) NEU deklariert `0x0D` (13) und stimmt exakt; (4) Regression — ALT
und NEU unterscheiden sich in keinem einzigen der 17 Bytes außer Index 3; (5) Regression —
Array-Größe bleibt 17 Bytes; (6) Regression — Company-ID/Typ-Bytes bleiben `0x4C, 0x00, 0x07`;
(7) der exakte Literalwert des korrigierten Längen-Felds ist `0x0D`.

Für den lokalen Funktionstest an echter Hardware: `BleSpam`-Befehl mit Modus 1 (bzw. der
entsprechende Apple-Einzelmodus-Befehl) auf ein eigenes, autorisiert getestetes iPhone/iPad mit
aktivem Bluetooth richten; vor dem Fix bleibt das "Möchtest du dich verbinden?"-Popup aus (bzw.
erscheint höchstens unzuverlässig), nach dem Fix sollte es zuverlässig und wiederholt
erscheinen, solange `ble_spam_tick()` rotiert. Parallel mit einem zweiten Gerät im
BLE-Sniffer/Wireshark-Mitschnitt (eigene, autorisierte Testumgebung) die gesendeten
Advertisements inspizieren und das Längen-Byte gegen die tatsächliche Payload-Länge der
Nachricht gegenprüfen.

## Quelle
- `G4MEOVER18/heltec-v4-hub` (main, Commit `258d4adb8a6d977255c72f72780039ac13a35a05`):
  `src/ble_spam.cpp` (`adv_apple()`, `adv_windows()`, `adv_android()`, `ble_spam_tick()`,
  `ble_scan()`), eigener Fund bei der ersten vollständigen Durchsicht dieser Datei in diesem
  Repo.
- `app-improvements/lilygo-ukfe-rx/ble-spam-apple-length-mismatch-fix.md` — ursprünglicher Fund
  und Herleitung der TLV-Konvention (Längen-Byte zählt die Bytes nach sich selbst) gegen die
  öffentlich bekannte Referenz-Payload des Projekts `ECTO-1A/AppleJuice` für denselben Typ
  `0x07`, sowie die zugrunde liegende Reverse-Engineering-Literatur zum Apple-Continuity-
  Protokoll (Martin et al., "Handoff All Your Privacy", PoPETs 2019).
- API-87-Einordnung: reine ESP32-Arduino-/NimBLE-Anwendungslogik, keine Flipper-Firmware-API
  betroffen, kein Kompatibilitätsrisiko.

## Offen
- `heltec-ukfe-rx/src/ble_spam.cpp` ist noch nicht gegen denselben konkreten Fund geprüft —
  da dieses Repo laut bisherigen Funden (`wifi-recon-probe-ssid-tag-oob-read-fix`) vom selben
  Code-Ausgangsstand wie `heltec-v4-hub`/`lilygo-ukfe-rx` abstammt, ist dort derselbe Fehler
  wahrscheinlich ebenfalls vorhanden — für eine künftige Sitzung vorgemerkt, statt hier
  ungeprüft mitzuportieren.
