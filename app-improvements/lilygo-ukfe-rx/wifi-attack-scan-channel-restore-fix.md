# lilygo-ukfe-rx — ein dazwischengefunkter WiFi-Scan reißt einen laufenden Festkanal-Deauth dauerhaft vom Zielkanal ab

## Was
Ich habe `src/wifi_attack.cpp` von `lilygo-ukfe-rx` zum ersten Mal vollständig gelesen (bisher
nur `src/main.cpp`/`src/ukfe_rf.c`/`.h` sowie das geteilte `src/ducky.h` in früheren Sitzungen
geprüft; `wifi_attack.cpp`/`.h` und `ble_spam.cpp`/`.h` waren noch in keinem meiner
`app-improvements/`-Durchgänge für dieses Gerät erfasst).

`wifi_attack_deauth()` unterscheidet zwei Modi über `s_fixedCh`: Festkanal (`channel != 0`,
Angriff bleibt die ganze Laufzeit auf genau diesem Kanal) und Hopping (`channel == 0`,
`wifi_attack_tick()` wechselt alle `ATK_HOP_MS` auf den nächsten Kanal). Im Festkanal-Fall wird
der Kanal genau einmal gesetzt — direkt beim Start des Angriffs:

```cpp
void wifi_attack_deauth(const uint8_t bssid[6], uint8_t channel, uint32_t dur_ms) {
    ...
    s_fixedCh = channel;                 // 0 => hoppen
    s_hopCh   = channel ? channel : 1;
    if(channel) set_channel(channel);
    ...
}
```

Danach setzt **nichts** den Kanal während eines Festkanal-Deauths je erneut — das Hopping in
`wifi_attack_tick()` greift ausdrücklich nur, wenn `s_fixedCh == 0` ist:

```cpp
if((s_mode == M_BEACON || (s_mode == M_DEAUTH && s_fixedCh == 0)) &&
   (now - s_lastHop) >= ATK_HOP_MS) { ... }
```

`send_deauth()` selbst übergibt `esp_wifi_80211_tx()` keinen Kanal-Parameter — das Frame geht
immer auf dem Kanal raus, auf den der Funk-Chip gerade eingestellt ist (Hardware-Register, kein
Software-Zustand des Frames). Genau dieses globale Funk-Register setzt aber auch
`wifi_attack_scan()`, bedingungslos und unabhängig davon, ob gerade ein Angriff läuft:

```cpp
uint8_t wifi_attack_scan() {
    // Synchroner Scan (zeigt versteckte). Danach ESP-NOW-Kanal wiederherstellen.
    int n = WiFi.scanNetworks(false, true);
    ...
    WiFi.scanDelete();
    set_channel(s_espnowCh);
    return s_scanCount;
}
```

`main.cpp`s `UkfeRfCmdWifiScan`-Handler ruft `wifi_attack_scan()` ohne jede Prüfung auf
`wifi_attack_busy()` auf. Löst der Operator während eines laufenden Festkanal-Deauths (z. B.
Kanal 6) zusätzlich einen `WifiScan`-Befehl aus, setzt der anschließende
`set_channel(s_espnowCh)`-Aufruf den Funk-Chip auf den ESP-NOW-Kontrollkanal (Standard-Kanal
1) zurück — und **nichts holt das je wieder zurück**, weil das Hopping (die einzige Stelle, die
sonst noch `set_channel()` aufruft) für `s_fixedCh != 0` niemals greift. Jeder weitere
Deauth-Burst dieses Angriffs geht ab diesem Zeitpunkt bis zum 20-s-Sicherheits-Timeout (oder
einem erneuten `WifiDeauth`-Befehl) auf dem falschen Kanal heraus — wirkungslos gegen den
eigentlichen Zielkanal 6, ohne dass `s_txOk` das anzeigt (`esp_wifi_80211_tx()` liefert `ESP_OK`
unabhängig vom Kanal, der Zähler steigt also weiter, als würde der Angriff normal laufen).

## Nutzen
- Schließt eine stille Funktionslücke im eigenen Pentest-Werkzeug: ein harmloser, rein
  informativer Zusatzbefehl (WiFi-Scan) darf einen bereits laufenden, gezielten Angriff nicht
  unbemerkt wirkungslos machen — passt in den in CONSTRAINTS Punkt 5 verlangten Rahmen
  (eigenes Werkzeug soll zuverlässig das tun, was der Operator erwartet, gerade bei einem
  zeitlich begrenzten 20-s-Fenster ohne Rückkanal).
- Rein additiv: ein bestehender `set_channel()`-Aufruf bekommt ein kontextabhängiges Argument
  statt einer festen Konstante — keine neue Variable, keine neue Funktion, keine geänderte
  Kontrollstruktur.
- Verbessert nebenbei auch den Hopping-Fall: bisher riss ein Scan auch eine laufende
  Hopping-Attacke (Deauth mit `channel=0` oder Beacon-Spam) auf den ESP-NOW-Kanal, bis der
  nächste Hop-Tick (`ATK_HOP_MS` = 180 ms) das stillschweigend korrigiert hätte — mit dem Fix
  bleibt auch dieser Fall lückenlos auf dem zuletzt aktiven Hop-Kanal.

## Betroffene Dateien
- `src/wifi_attack.cpp` — `wifi_attack_scan()`, letzte Zeile vor dem `return`: aus
  `set_channel(s_espnowCh)` wird `set_channel(s_mode == M_NONE ? s_espnowCh : (s_fixedCh ?
  s_fixedCh : s_hopCh))`. Keine weitere Datei betroffen (`s_mode`/`s_fixedCh`/`s_hopCh`/
  `s_espnowCh` sind bereits vorhandene Datei-lokale `static`-Variablen, keine neue
  Schnittstelle nach außen).

## Risiko
Sehr gering. Einzeiliger Austausch eines festen Arguments gegen einen Ausdruck, der denselben
Werteraum nutzt wie das bereits bestehende, identische Muster in `wifi_attack_tick()`
(`uint8_t ch = s_fixedCh ? s_fixedCh : s_hopCh;` für den Beacon-Pfad). Ohne laufenden Angriff
(`s_mode == M_NONE`) bleibt das Verhalten exakt wie bisher (`s_espnowCh`). Ändert nichts an
`send_deauth()`/`send_beacon()`, dem Hopping-Timing oder dem 20-s-Sicherheitstimeout selbst.

## Port-Skizze / Fix
In `wifi_attack_scan()`, letzte Zeile vor `return s_scanCount;`:

```c
WiFi.scanDelete();
// Nicht blind den ESP-NOW-Kontrollkanal setzen: laeuft gerade ein Festkanal-Deauth
// (s_fixedCh != 0), wuerde das den Angriff dauerhaft auf den falschen Kanal verschieben,
// da das Hopping in wifi_attack_tick() nur bei s_fixedCh == 0 greift und ihn sonst nie
// zurueckholt. Waehrend eines laufenden Angriffs den dafuer vorgesehenen Kanal
// wiederherstellen, sonst wie bisher den ESP-NOW-Kanal.
set_channel(s_mode == M_NONE ? s_espnowCh : (s_fixedCh ? s_fixedCh : s_hopCh));
return s_scanCount;
```

Unified-Diff gegen die unveränderte `G4MEOVER18/lilygo-ukfe-rx`-Basis (`main`, Commit
`aa8a285`) in
[`wifi-attack-scan-channel-restore-fix/wifi_attack_c.patch`](wifi-attack-scan-channel-restore-fix/wifi_attack_c.patch)
— `git apply --check` gegen zwei frische, unabhängige Zweitkopien verifiziert, real
angewendet, beide Ergebnisse per `md5sum` gegeneinander und gegen die beabsichtigte
Endfassung abgeglichen — alle drei identisch (`156e92ecc1104abc53452f9aed35ade4`).

## Testhinweis
Host-Repro (kein Flipper/ESP32/Arduino/WiFi-Abhängigkeit — bildet `set_channel()`,
`wifi_attack_deauth()`, `wifi_attack_tick()` sowie ALT- und NEU-Fassung von
`wifi_attack_scan()` Zeile für Zeile nach; `esp_wifi_set_channel()`/`esp_wifi_80211_tx()`
werden durch instrumentierte Mocks ersetzt, die den aktuell eingestellten Funkkanal bzw. den
Kanal jedes tatsächlich gesendeten Deauth-Bursts mitprotokollieren):

```
gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro
./repro
```

[`repro-wifi-attack-scan-channel-restore/repro.c`](repro-wifi-attack-scan-channel-restore/repro.c)
— **13 Prüfungen**, lokal unter `-O0` und `-O1` ausgeführt (identischer Output, keine
ASan-/UBSan-Meldungen, keine Compiler-Warnungen): **ALL CHECKS PASSED (13 total)**. Geprüft:
(1) Festkanal-Deauth (Kanal 6), ein dazwischengefunkter Scan — ALT verschiebt den Funkkanal auf
den ESP-NOW-Kanal und der nächste wie auch jeder weitere Deauth-Burst geht fälschlich dort
heraus (keine Selbstheilung über fünf weitere Ticks); NEU stellt nach dem Scan korrekt Kanal 6
wieder her, alle folgenden Bursts bleiben auf Kanal 6; (2) Regression Hopping-Deauth
(`channel=0`): ein Scan nach einem bereits erfolgten Hop stellt den *aktuellen* Hop-Kanal
wieder her, nicht Kanal 1 oder den ESP-NOW-Kanal; (3) Regression ohne laufenden Angriff
(`s_mode == M_NONE`): Verhalten bleibt unverändert beim ESP-NOW-Kanal; (4) Regression
Beacon-Spam (immer hoppend): Scan stellt ebenfalls den aktuellen Hop-Kanal wieder her.

Für den lokalen Funktionstest an der echten Hardware: `WifiDeauth` mit festem Kanal (z. B.
Kanal 6, eigener autorisiert getesteter AP/Client) auslösen, mit einem zweiten Gerät im
Monitor-Modus auf Kanal 6 den eingehenden Deauth-Burst bestätigen; während der Angriff noch
läuft zusätzlich `WifiScan` auslösen; vor dem Fix bricht der Deauth-Empfang auf Kanal 6 danach
sofort ab (die Frames gehen fortan auf dem ESP-NOW-Kanal heraus), nach dem Fix läuft er
unverändert bis zum 20-s-Timeout weiter. Regressionsfall: denselben Ablauf mit
Hopping-Deauth (`channel=0`) und mit Beacon-Spam wiederholen — Scan darf das Hopping-Muster in
beiden Fällen nicht unterbrechen.

## Quelle
- `G4MEOVER18/lilygo-ukfe-rx` (main, Commit `aa8a285`): `src/wifi_attack.cpp`
  (`wifi_attack_scan()`, `wifi_attack_deauth()`, `wifi_attack_tick()`, `set_channel()`),
  eigener Fund bei der ersten vollständigen Durchsicht dieser Datei.
- API-87-Einordnung: reine ESP32-Arduino-/ESP-IDF-Anwendungslogik (`esp_wifi_set_channel`),
  keine Flipper-Firmware-API betroffen, kein Kompatibilitätsrisiko.
