# lilygo-ukfe-rx — hoppende WiFi-Angriffe setzen den Funkkanal beim Start nicht, erster Burst geht auf dem Kanal des vorherigen Angriffs raus

## Was
Ich habe `src/wifi_attack.cpp` von `lilygo-ukfe-rx` nach der letzten Sitzung (Scan-während-
Festkanal-Deauth-Fund) noch einmal vollständig gelesen, dieses Mal mit Fokus auf die beiden
Start-Funktionen `wifi_attack_deauth()` und `wifi_attack_beacon()` selbst statt auf
`wifi_attack_scan()`.

`wifi_attack_deauth()` setzt den Funkkanal beim Start nur im Festkanal-Fall:

```cpp
void wifi_attack_deauth(const uint8_t bssid[6], uint8_t channel, uint32_t dur_ms) {
    ...
    s_fixedCh = channel;                 // 0 => hoppen
    s_hopCh   = channel ? channel : 1;
    if(channel) set_channel(channel);    // <- nur hier, nur wenn channel != 0
    ...
}
```

Im Hopping-Fall (`channel == 0`) wird `s_hopCh` zwar schon auf den Start-Hop-Kanal `1`
gesetzt, aber **kein** `set_channel()`-Aufruf stellt den tatsächlichen Funk-Chip auch
wirklich auf diesen Kanal. Das holt erst `wifi_attack_tick()` nach — und zwar frühestens
nach `ATK_HOP_MS` (180 ms), weil das Hopping zeitgesteuert ist:

```cpp
if((s_mode == M_BEACON || (s_mode == M_DEAUTH && s_fixedCh == 0)) &&
   (now - s_lastHop) >= ATK_HOP_MS) {
    s_hopCh = (s_hopCh % 13) + 1;
    set_channel(s_hopCh);
    ...
}
```

`wifi_attack_beacon()` hat denselben Fund in verschärfter Form: Beacon-Spam hoppt *immer*
(`s_fixedCh = 0` ist dort hart codiert), setzt aber beim Start **überhaupt keinen**
`set_channel()`-Aufruf — weder bedingt noch unbedingt:

```cpp
void wifi_attack_beacon(uint8_t mode, uint32_t dur_ms) {
    (void)mode;
    s_fixedCh = 0;                       // Beacon-Spam hoppt immer
    s_hopCh   = 1;
    // kein set_channel() hier
    ...
}
```

`main.cpp`s Befehls-Dispatcher ruft beide Funktionen ohne vorheriges `wifi_attack_stop()`
oder eine `wifi_attack_busy()`-Prüfung auf (`UkfeRfCmdWifiDeauth`/`UkfeRfCmdBeaconSpam`,
`act()` in `src/main.cpp`) — ein Operator kann also jederzeit direkt von einem laufenden
Festkanal-Deauth (z. B. Kanal 6, über `wifi_attack_deauth(bssid, 6, 0)`) in einen Beacon-Spam
oder einen hoppenden Deauth wechseln, ohne dass der Funk-Chip zwischendurch zurückgesetzt
wird. Der Chip bleibt dann bis zum ersten Hop-Tick — bis zu 180 ms, bei `ATK_BURST = 6`
Frames pro `loop()`-Tick potenziell mehrere ganze Bursts — auf dem alten Kanal 6 stehen,
während `send_beacon()`/`send_deauth()` bereits für den neuen Angriff aufgerufen werden.
Bei Beacon-Spam kommt dazu, dass `send_beacon(ssid, ch)` den an die Hardware übergebenen
`ch`-Parameter (= `s_hopCh`, hier `1`) unabhängig vom tatsächlichen Funkkanal in das
DS-Parameter-Tag des Frames schreibt — der erste gefälschte AP behauptet also, auf Kanal 1
zu senden, während das Frame in Wirklichkeit auf Kanal 6 herausgeht. Ein Scanner/Client, der
auf dem behaupteten Kanal 1 nach dem beworbenen Netz sucht, findet es dort nicht; `s_txOk`
zeigt in beiden Fällen unverändert "erfolgreich" an (`esp_wifi_80211_tx()` liefert `ESP_OK`
unabhängig vom Kanal), ohne dass der Operator etwas davon sieht.

## Nutzen
- Schließt dieselbe Art stiller Funktionslücke wie der vorherige Scan-Fund (ein Angriffs-
  Wechsel darf nicht unbemerkt auf dem falschen Kanal starten) — passt in den in CONSTRAINTS
  Punkt 5 verlangten defensiven Rahmen: das eigene Pentest-Werkzeug soll zuverlässig das tun,
  was der Operator befiehlt, gerade während des 20-s-Sicherheitsfensters ohne Rückkanal
  (lt. `main.cpp`-Kommentar: "kein 868-Backchannel -> Stop erreicht die LilyGo erst nach dem
  20-s-Sicherheits-Timeout").
- Rein additiv und minimal: ein bestehender bedingter `set_channel()`-Aufruf wird um den
  Hopping-Fall erweitert bzw. ein fehlender `set_channel()`-Aufruf ergänzt — keine neue
  Variable, keine neue Funktion, keine geänderte Kontrollstruktur, kein Einfluss auf das
  Hopping-Timing selbst.
- Ergänzt den bereits vorgeschlagenen `wifi-attack-scan-channel-restore-fix` (behebt den
  Kanal-Reset durch einen *dazwischengefunkten Scan* während eines laufenden Angriffs) um den
  bislang offenen zweiten Fall: den fehlenden Kanal-Reset beim *Start* eines neuen
  Hopping-Angriffs direkt nach einem anderen Angriff — dieselbe Ursachenfamilie
  (`set_channel()` wird nicht konsistent an jeder Stelle aufgerufen, an der der Angriffs-
  Zustand wechselt), zwei unabhängige Lücken.

## Betroffene Dateien
- `src/wifi_attack.cpp`:
  - `wifi_attack_deauth()`: `if(channel) set_channel(channel);` wird zu
    `set_channel(s_hopCh);` (deckt beide Fälle ab, da `s_hopCh == channel`, wenn
    `channel != 0`, sonst `1` — identisches Verhalten im Festkanal-Fall, zusätzlich korrekt
    im Hopping-Fall).
  - `wifi_attack_beacon()`: neuer `set_channel(s_hopCh);`-Aufruf direkt nach
    `s_hopCh = 1;`.
  - Keine weitere Datei betroffen; `wifi_attack_tick()`, `send_deauth()`, `send_beacon()`,
    `wifi_attack_stop()`, `wifi_attack_scan()` bleiben unverändert.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/lilygo-ukfe-rx`-Basis (`main`, Commit
`aa8a285`; `patch -p0 --dry-run` vorab gegen eine frische Kopie verifiziert, real
angewendet und per `md5sum` byte-für-byte gegen die beabsichtigte Endfassung abgeglichen) in
[`wifi-attack-hop-start-channel-not-set-fix/wifi_attack_cpp.patch`](wifi-attack-hop-start-channel-not-set-fix/wifi_attack_cpp.patch).

## Testhinweis (ohne Hardware)
[`repro-wifi-attack-hop-start-channel-not-set/repro.c`](repro-wifi-attack-hop-start-channel-not-set/repro.c)
übernimmt die Entscheidungslogik aus `set_channel()`/`wifi_attack_deauth()`/
`wifi_attack_beacon()`/`wifi_attack_tick()` Zeile für Zeile (als ALT/NEU-Variantenpaar über
ein Laufzeit-Flag, da beide Fassungen für den Vergleich im selben Repro-Lauf gebraucht
werden) und ersetzt `esp_wifi_set_channel()`/`esp_wifi_80211_tx()` durch instrumentierte
Mocks, die den aktuellen Funkkanal sowie den Kanal jedes tatsächlich gesendeten Frames
mitprotokollieren — keine ESP32-/Arduino-Abhängigkeit nötig:

```
$ gcc -std=c17 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c && ./repro
ok  [ALT: Setup -- Radio steht noch auf Kanal 6 vom vorherigen Angriff]
ok  [BUG ALT: erster Beacon-Burst geht auf Kanal 6 raus, nicht auf den behaupteten Kanal 1]
ok  [ALT: DS-Parameter-Tag behauptet bereits Kanal 1 (claimed_ch==s_hopCh)]
ok  [BUG ALT: zweiter Burst ebenfalls noch auf Kanal 6 (Fenster bis ATK_HOP_MS=180ms)]
ok  [ALT: erst nach dem ersten Hop-Tick stimmen Funkkanal und Tag wieder zusammen]
ok  [BUG ALT: hoppender Deauth sendet ersten Burst auf Kanal 11 statt Kanal 1]
ok  [FIX: set_channel(s_hopCh) beim Start raeumt den Leftover-Kanal sofort weg]
ok  [FIX: erster Beacon-Burst geht korrekt auf Kanal 1 raus (passt zum Tag)]
ok  [FIX: Hopping danach unveraendert (Kanal 2 nach erstem Hop)]
ok  [FIX: hoppender Deauth sendet ersten Burst korrekt auf Kanal 1]
ok  [Regression FIX: Festkanal-Deauth setzt weiterhin sofort auf den Zielkanal 6]
ok  [Regression FIX: erster Burst bleibt auf Zielkanal 6]

ALL CHECKS PASSED (12 total)
```

Identisch unter `-O1` verifiziert, in beiden Fällen keine ASan-/UBSan-Meldungen und keine
Compiler-Warnungen. Lokaler Hardware-Funktionstest: `pio run`-Build, dann nacheinander ohne
`WifiStop` dazwischen einen Festkanal-Deauth (z. B. Kanal 6) starten und direkt danach einen
`BeaconSpam`- oder hoppenden `WifiDeauth`-Befehl senden — vor dem Fix zeigt ein zweiter
WLAN-Adapter im Monitor-Modus auf Kanal 6 den ersten gefälschten Beacon (DS-Tag behauptet
Kanal 1), nach dem Fix erscheint er sofort korrekt auf Kanal 1. Regressionsfall: ein
Festkanal-Deauth direkt nach dem Boot (kein vorheriger Angriff) muss weiterhin exakt wie vor
dem Patch sofort auf dem Zielkanal senden.

## Risiko
- Sehr gering: beide Änderungen ersetzen bzw. ergänzen genau einen bestehenden
  `set_channel()`-Aufruf an der jeweiligen Start-Stelle, ohne neue Zustandsvariablen oder
  geänderte Kontrollstrukturen. Im bisher schon korrekt behandelten Festkanal-Fall ist das
  Ergebnis identisch (`set_channel(s_hopCh)` mit `s_hopCh == channel`).
- Ändert nichts an `wifi_attack_tick()`s Hopping-Timing (`ATK_HOP_MS`), an
  `send_deauth()`/`send_beacon()` selbst oder am 20-s-Sicherheitstimeout.
- Unabhängig vom bereits vorgeschlagenen `wifi-attack-scan-channel-restore-fix` (andere
  Funktion, anderer Auslöser) — beide Patches überschneiden sich nicht und können in
  beliebiger Reihenfolge angewendet werden.

## Quelle
- `G4MEOVER18/lilygo-ukfe-rx` (main, Commit `aa8a285`): `src/wifi_attack.cpp`
  (`wifi_attack_deauth`, `wifi_attack_beacon`, `wifi_attack_tick`, `send_beacon`), `src/main.cpp`
  (`act()`, `UkfeRfCmdWifiDeauth`/`UkfeRfCmdBeaconSpam`-Handler ohne vorherigen
  `wifi_attack_stop()`-Aufruf).
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/lilygo-ukfe-rx/wifi-attack-scan-channel-restore-fix.md` (vorheriger,
  ursächlich verwandter Fund in derselben Datei).
