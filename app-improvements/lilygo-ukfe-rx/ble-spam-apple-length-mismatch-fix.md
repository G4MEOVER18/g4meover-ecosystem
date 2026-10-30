# lilygo-ukfe-rx — Apple-BLE-Spam-Advertisement trägt ein falsches Längen-Feld, Pairing-Popup bleibt auf echten Geräten wahrscheinlich aus

## Was
Ich habe `src/ble_spam.cpp`/`.h` von `lilygo-ukfe-rx` zum ersten Mal vollständig gelesen (bisher
nur `src/main.cpp`/`src/ukfe_rf.c`/`.h`, das geteilte `src/ducky.h` sowie — in der letzten
Sitzung zu diesem Gerät — `src/wifi_attack.cpp`/`.h` geprüft; `ble_spam.cpp`/`.h` war seitdem als
"weiterhin ungeprüft" zurückgestellt).

`adv_apple()` baut das Apple-Continuity-"Proximity Pairing"-Advertisement (Typ `0x07`, das
Popup "Möchtest du dich mit … verbinden?") als festen 17-Byte-Rohblock zusammen und übergibt ihn
unverändert an NimBLE:

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

Der Aufbau folgt der bei Apples Continuity-Nachrichten üblichen TLV-Struktur
`Company-ID(2 Byte) Typ(1 Byte) Länge(1 Byte) Payload(Länge Byte)` — dieselbe Struktur, die auch
die öffentlich bekannte Referenz-Payload des Projekts `ECTO-1A/AppleJuice` für denselben Typ
`0x07` verwendet (dort: Länge `0x19` = 25, danach folgen exakt 25 Payload-Bytes bis zum Ende des
Blocks — die Länge zählt also die Bytes NACH dem Längen-Byte selbst).

In `adv_apple()` steht an der Längen-Position (Index 3) der Wert `0x0F` (15). Tatsächlich folgen
dem Längen-Byte im 17-Byte-Array aber nur `17 - 2 (Company-ID) - 1 (Typ) - 1 (Länge) = 13` weitere
Bytes (Index 4 bis Index 16: Sub-Typ/Prefix, 2 Modell-Bytes, Status, 3 Batterie/Flags-Bytes, 1
weiteres Byte, 3 Zufallsbytes, 1 Schlussbyte). Das deklarierte Längen-Feld (`0x0F` = 15) und die
tatsächlich vorhandene Byte-Zahl (13) stimmen nicht überein — ein Unterschied von genau 2 Bytes, vermutlich
ein Überrest aus einer früheren, zwei Bytes längeren Payload-Variante, bei der die Länge beim
Kürzen nicht mit angepasst wurde.

Die Auswirkung: jedes gesendete Advertisement ist als Apple-Continuity-Nachricht intern
inkonsistent. Ein TLV-konform arbeitender Empfänger (ein echtes iPhone/iPad/Mac, aber auch jedes
Analyse-Werkzeug wie Wireshark mit Apple-Continuity-Dissector, mit dem man den Angriff überhaupt
erst verifizieren würde) liest nach dem Längen-Byte so viele Bytes, wie dieses Feld angibt — hier
15 statt der tatsächlich vorhandenen 13. Das Advertisement ist damit als Proximity-Pairing-
Nachricht fehlerhaft strukturiert; ein spezifikationstreuer Parser verwirft eine solche Nachricht
typischerweise als ungültig, statt das (nicht vorhandene) 14. und 15. Byte zu erraten. Der
Operator sieht davon nichts: `ble_spam_tick()` meldet keinen Fehler, die TFT-Anzeige bleibt bei
"BLE SPAM laeuft", `adv->start()` liefert unabhängig vom Payload-Inhalt Erfolg — das Popup auf dem
Zielgerät bleibt aber mit hoher Wahrscheinlichkeit aus, ohne dass das Werkzeug das anzeigt. Genau
dieselbe Fundart wie die in der letzten Sitzung behobene `wifi_attack_scan()`-Kanalverschiebung:
ein scheinbar laufender Angriff, der nach außen hin funktioniert, aber am Ziel wirkungslos bleibt.

Die anderen beiden Advertisement-Bausteine derselben Datei sind von diesem Fund nicht betroffen:
`adv_windows()` und `adv_android()` bauen ihre Manufacturer-/Service-Data dynamisch per
`std::string` ohne ein selbst deklariertes inneres Längen-Feld — NimBLE berechnet die äußere
AD-Struktur-Länge dort automatisch aus der tatsächlichen String-Länge, ein manuelles
Längen-Byte, das aus der Reihe fallen könnte, existiert dort schlicht nicht.

## Nutzen
- Schließt dieselbe Art stiller Funktionslücke wie der vorherige `wifi_attack_scan()`-Fund
  (ein Angriffsmodus darf dem Operator nicht als aktiv/erfolgreich angezeigt werden, wenn das
  gesendete Signal am Ziel strukturell ungültig ist) — passt in den in CONSTRAINTS Punkt 5
  verlangten Rahmen: das eigene Pentest-Werkzeug soll zuverlässig das tun, was es dem Operator
  anzeigt zu tun.
- Rein additiv und minimal: ein einziges falsches Literal (`0x0F` → `0x0D`) wird korrigiert,
  keine neue Variable, keine neue Funktion, keine geänderte Kontrollstruktur, kein Einfluss auf
  Modell-Auswahl, Zufalls-MAC-Rotation oder das Advertising-Timing.
- Verbessert konkret den `SourApple`-Befehl (`ble_spam_start(1)`, reiner Apple-Modus) sowie den
  Apple-Anteil der rotierenden `BleSpam`-Befehle (Modus 0) — beide senden ausschließlich bzw.
  anteilig über `adv_apple()`.

## Betroffene Dateien
- `src/ble_spam.cpp` — `adv_apple()`, Index 3 des lokalen `d[17]`-Arrays: aus `0x0F` wird `0x0D`
  (13 Bytes folgen tatsächlich nach dem Längen-Byte). Keine weitere Datei betroffen — das Array
  ist rein lokal, keine Schnittstelle nach außen, kein Zugriff aus `main.cpp` oder `ble_spam.h`.

## Risiko
Sehr gering. Einzelnes Byte-Literal in einem bereits bestehenden, rein lokalen Array geändert;
Array-Größe (17), alle übrigen 16 Bytes, die Modell-Auswahl aus `APPLE_MODELS`, die drei
Zufallsbytes sowie der nachfolgende `setManufacturerData()`/`setAdvertisementData()`-Aufruf
bleiben unverändert. Ändert nichts an `adv_windows()`, `adv_android()`, `randomize_mac()`,
`ble_spam_tick()`s Rotations- oder Timing-Logik oder `ble_scan()`.

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

Unified-Diff gegen die unveränderte `G4MEOVER18/lilygo-ukfe-rx`-Basis (`main`, Commit `aa8a285`)
in
[`ble-spam-apple-length-mismatch-fix/ble_spam_c.patch`](ble-spam-apple-length-mismatch-fix/ble_spam_c.patch)
— per `patch -p0` sowie unabhängig per `git apply --check`/`git apply` gegen zwei frische,
unabhängige Zweitkopien angewendet; beide Ergebnisse per `md5sum` gegeneinander abgeglichen —
identisch (`b62783105bde334c30ac03b2e384b7e1`).

## Testhinweis
Host-Repro (kein Arduino/ESP32/NimBLE-Abhängigkeit — bildet den `d[17]`-Aufbau von `adv_apple()`
in ALT- und NEU-Fassung Byte für Byte nach; die Prüfung selbst ist reine Struktur-Arithmetik,
unabhängig vom tatsächlichen Funkverhalten):

```
gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro
./repro
```

[`repro-ble-spam-apple-length-mismatch/repro.c`](repro-ble-spam-apple-length-mismatch/repro.c)
— **23 Prüfungen**, lokal unter `-O0` und `-O1` ausgeführt (identischer Output, keine
ASan-/UBSan-Meldungen, keine Compiler-Warnungen): **ALL CHECKS PASSED (23 total)**. Geprüft, je
für drei verschiedene Modell-/Zufallsbyte-Kombinationen: (1) die tatsächliche Byte-Zahl nach dem
Längen-Feld ist 13; (2) ALT deklariert `0x0F` (15) und weicht damit nachweislich von den 13
tatsächlichen Bytes ab; (3) NEU deklariert `0x0D` (13) und stimmt exakt; (4) Regression — ALT
und NEU unterscheiden sich in keinem einzigen der 17 Bytes außer Index 3 (Modell-, Status-,
Batterie- und Zufallsbytes bleiben unangetastet); (5) Regression — Array-Größe bleibt 17 Bytes;
(6) Regression — Company-ID/Typ-Bytes bleiben `0x4C, 0x00, 0x07`; (7) der exakte Literalwert des
korrigierten Längen-Felds ist `0x0D`.

Für den lokalen Funktionstest an echter Hardware: `BleSpam`-Befehl mit Modus 1 (bzw.
`SourApple`) auf ein eigenes, autorisiert getestetes iPhone/iPad mit aktivem Bluetooth richten;
vor dem Fix bleibt das "Möchtest du dich verbinden?"-Popup aus (bzw. erscheint höchstens
unzuverlässig/kurzzeitig, je nach iOS-Version und deren Fehlertoleranz beim Parsen), nach dem
Fix sollte es zuverlässig und wiederholt erscheinen, solange `ble_spam_tick()` rotiert. Parallel
mit einem zweiten Gerät im BLE-Sniffer/Wireshark-Mitschnitt (eigene, autorisierte Testumgebung)
die gesendeten Advertisements inspizieren und das Längen-Byte gegen die tatsächliche
Payload-Länge der Nachricht gegenprüfen.

## Quelle
- `G4MEOVER18/lilygo-ukfe-rx` (main, Commit `aa8a285`): `src/ble_spam.cpp` (`adv_apple()`,
  `adv_windows()`, `adv_android()`, `ble_spam_tick()`, `ble_scan()`), eigener Fund bei der
  ersten vollständigen Durchsicht dieser Datei.
- Zur Einordnung der TLV-Konvention (Längen-Byte zählt die Bytes nach sich selbst) gegen die
  öffentlich bekannte Referenz-Struktur desselben Apple-Continuity-Nachrichtentyps `0x07`
  geprüft: `ECTO-1A/AppleJuice` (`ESP32-Arduino/applejuice.ino`, Airpods-Payload: Länge `0x19`
  = 25, danach exakt 25 Bytes bis Blockende) sowie die zugrunde liegende Reverse-Engineering-
  Literatur zum Apple-Continuity-Protokoll (Martin et al., "Handoff All Your Privacy", PoPETs
  2019; TLV-Rahmenstruktur der Continuity-Nachrichten).
- API-87-Einordnung: reine ESP32-Arduino-/NimBLE-Anwendungslogik, keine Flipper-Firmware-API
  betroffen, kein Kompatibilitätsrisiko.
