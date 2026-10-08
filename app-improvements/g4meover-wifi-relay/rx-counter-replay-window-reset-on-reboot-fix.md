# g4meover-wifi-relay — Flipper→Relay-Anti-Replay-Counter öffnet das Replay-Fenster nach jedem Neustart

## Was
Ich habe `g4meover-wifi-relay` geprüft — den ESP32-WROOM-Koordinator, der die signierten
`ukfe_rf`-Befehle vom Flipper per UART entgegennimmt, verifiziert und unverändert per ESP-NOW
an alle Satelliten (LilyGo T-Dongle S3, Heltec LoRa v3) rebroadcastet (`src/main.cpp`,
`scan_and_relay()`/`relay_frame()`). Das Repo war bisher in keinem meiner
`app-improvements/`-Durchgänge erfasst — ich habe es dieses Mal gezielt aufgegriffen, weil es
seit der `lilygo-ukfe-rx`-Sitzung als einziges noch nicht auf den dort (und zuvor in
`usb-army-penetrator`/`heltec-v4-hub`) gefundenen `ukfe_rf`-Anti-Replay-Fund geprüftes Gerät
offen stand.

Das gemeinsame `ukfe_rf`-Protokoll (`src/ukfe_rf.c`/`.h`, laut eigenem Dateikommentar
"plattformunabhaengig, ohne HW-Abhaengigkeit", byte-für-byte identisch mit der bereits in
`usb-army-penetrator`/`heltec-v4-hub`/`lilygo-ukfe-rx` geprüften Fassung) erzwingt einen strikt
monoton steigenden, MAC-authentifizierten Rolling-Counter als Anti-Replay-Schutz:

```c
if(last_counter) {                                        // Anti-Replay
    if(m.counter <= *last_counter) return false;          // verbraucht/replay
    *last_counter = m.counter;
}
```

In `g4meover-wifi-relay` wird dieser Mechanismus für die Flipper→Relay-Richtung so angebunden
(`src/main.cpp`):

```cpp
static uint32_t s_rxCounter = 0;     // Anti-Replay-Fenster (Flipper -> Relay)
...
static void scan_and_relay() {
    ...
    UkfeRfMessage msg;
    if (ukfe_rf_parse_frame(RELAY_SECRET, s_buf, real_len, &msg, &s_rxCounter)) {
        s_rxFromFlipper++;
        relay_frame(s_buf, real_len, &msg, "Flipper");
    } else {
        s_rejected++;
    }
    ...
}
```

`s_rxCounter` ist ein reines RAM-Feld (statische globale Variable, Startwert `0`). Interessant:
derselbe Code persistiert bereits einen *anderen* Counter — `s_txCounter` (für selbst erzeugte
CLI-`ping`-Frames) — korrekt über NVS mit einer Sicherheitsspanne:

```cpp
g_prefs.begin("g4meover", false);
s_txCounter = g_prefs.getUInt("txctr", 0) + TXCTR_MARGIN;
g_prefs.putUInt("txctr", s_txCounter);
g_prefs.end();
```

`s_rxCounter` — das eigentliche Anti-Replay-Fenster für die vom Flipper empfangenen, über alle
Satelliten hinweg sicherheitsrelevanten Befehle — bleibt dagegen komplett außen vor; `grep -rn
"s_rxCounter" src/` zeigt nur die Deklaration, die Nutzung als `last_counter`-Pointer und die
Stelle in `scan_and_relay()`, nirgends einen NVS-Zugriff. Das ist derselbe Fund wie bereits in
`app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`,
`app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md` und
`app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md` dokumentiert,
hier im fünften geprüften Gerät: Jeder Neustart des Relays (Absturz, Stromausfall, versehentlicher
Reset, OTA-Update) setzt `s_rxCounter` auf `0` zurück. Ein Angreifer, der zuvor ein gültiges,
signiertes Flipper→Relay-Frame mitgeschnitten hat (physischer UART-Zugriff auf die
Flipper↔Relay-Verbindung, oder — da der Relay dasselbe Frame unverändert weiter per ESP-NOW
broadcastet, `esp_now_add_peer(..., .encrypt = false)` — per ESP-NOW-Sniffer auf der
Relay→Satelliten-Strecke), kann dieses Frame nach jedem Neustart des Relays erneut einspielen:
`25 <= 0` ist falsch, das Frame wird als "neu" akzeptiert.

**Warum das hier besonders schwer wiegt:** Der Relay ist die zentrale Weiterleitungsstelle für
alle vom Flipper ausgelösten Befehle an *alle* Satelliten gleichzeitig (`espnow_broadcast()` mit
Broadcast-MAC) — `UkfeRfCmdWifiDeauth`/`UkfeRfCmdBeaconSpam`/`UkfeRfCmdEvilPortal` lösen sofort
einen Funkangriff aus, `UkfeRfCmdTrigger`/`UkfeRfCmdPayloadRun` starten einen vorbereiteten
Payload-Lauf. Ein wiedergespielter, bereits einmal benutzter Befehl wird also nicht nur vom Relay
selbst erneut akzeptiert, sondern unverändert an jeden erreichbaren Satelliten weitergereicht —
ob der jeweilige Satellit den Replay danach noch abfängt, hängt allein von dessen *eigenem*
Anti-Replay-Fenster ab (bei `usb-army-penetrator`/`heltec-v4-hub`/`lilygo-ukfe-rx` inzwischen als
Fix vorgeschlagen, lokal aber noch nicht zwingend eingebaut/geflasht). Ein Relay-Neustart allein
reicht somit aus, um einen zuvor beobachteten Angriffsbefehl erneut auszulösen, ohne dass der
Operator ihn ein zweites Mal gesendet hat.

## Nutzen
- Schließt dieselbe Sicherheitslücke im eigenen Anti-Replay-Mechanismus wie bereits bei
  `usb-army-penetrator`, `heltec-v4-hub` und `lilygo-ukfe-rx` behoben, diesmal am zentralen
  Weiterleitungspunkt des Ökosystems — passt in den in CONSTRAINTS Punkt 5 verlangten defensiven
  Rahmen (eigenes Pentest-Werkzeug soll selbst keine unnötig ausnutzbare Schwachstelle
  enthalten).
- Wiederverwendet exakt dieselbe, bereits dreimal verifizierte Reserve-mit-Sicherheitsspanne-
  Logik (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`) — keine neue
  Krypto, kein neues Protokollfeld, dieselbe bewährte Idee ein fünftes Mal angewendet.
- Nutzt dasselbe `Preferences`-Objekt (`g_prefs`, Namespace `"g4meover"`), das der Relay bereits
  für `s_txCounter` verwendet — nur ein neuer Key (`"rxctr"`), kein neuer `lib_deps`-Eintrag,
  keine neue NVS-Partition.
- Das Wire-Format, `ukfe_rf_mac()`/`ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()` und die
  `counter <= last_counter`-Regel selbst bleiben unverändert — weder der sendende Flipper noch
  die empfangenden Satelliten benötigen eine Anpassung.
- `s_txCounter` (CLI-Diagnose-Ping) bleibt unangetastet — bereits korrekt per NVS-Margin
  abgesichert, nicht Gegenstand dieser Sitzung.

## Betroffene Dateien
- `src/main.cpp` — neues `s_rxCtrFloor`-Feld neben `s_rxCounter`, zwei neue freie Funktionen
  (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`), `setup()` lädt den
  persistierten Stand aus NVS (im bereits vorhandenen `g_prefs.begin("g4meover", false)`-Block,
  derselbe Aufruf, der auch `s_txCounter` lädt) statt `s_rxCounter` bei `0` zu belassen,
  `scan_and_relay()` prüft nach jedem akzeptierten Flipper-Frame, ob die Reserve nachgezogen
  werden muss (eigener, kurzer `g_prefs.begin()/putUInt()/end()`-Block, passend zum bereits in
  dieser Datei etablierten Kurz-Transaktions-Stil statt eines durchgehend offenen Handles).

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-wifi-relay`-Basis (`main`, Commit
`9b4a9e1`; `git apply --check` vorab gegen eine frische, unabhängige Zweitkopie verifiziert,
real angewendet und byte-für-byte gegen die beabsichtigte Endfassung abgeglichen — identisch) in
[`rx-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`](rx-counter-replay-window-reset-on-reboot-fix/main_cpp.patch).

Einziger inhaltlicher Eingriff: Ursprung und Fortschreibung von `s_rxCounter` in `main.cpp`.
`ukfe_rf.c`/`.h` (Frame-Format, MAC, CRC, die `counter <= last_counter`-Prüfung selbst) bleiben
unverändert.

## Testhinweis (ohne Hardware)
[`repro-rx-counter-replay-window-reset/repro.c`](repro-rx-counter-replay-window-reset/repro.c)
nutzt die unveränderten Original-Dateien `ukfe_rf.c`/`ukfe_rf.h` (1:1 aus `g4meover-wifi-relay`
übernommen) — keine ESP32/Arduino/ESP-NOW-Abhängigkeit nötig:

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c ukfe_rf.c
$ ./repro
Szenario 1 (Sitzung vor Neustart): 0/50 abgelehnt (sollte 0 sein)
BUG:  mitgeschnittenes WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> ANGENOMMEN (Replay erfolgreich -> wird an alle Satelliten rebroadcastet!)
Szenario 2 (Sitzung vor Neustart, mit Fix): 0/50 abgelehnt, 10 NVS-Schreibzugriffe (statt 50)
FIX:  dasselbe mitgeschnittene Frame (Counter 25) nach Neustart erneut gesendet -> abgelehnt (Replay verhindert)
FIX:  neuer, legitimer Befehl (Counter 61) nach Neustart -> angenommen
Alle Szenarien bestaetigt (exit code 0)
```

Identisch unter `-O1` verifiziert, in beiden Fällen keine ASan-/UBSan-Meldungen und keine
Compiler-Warnungen. Das Repro baut mit den echten `ukfe_rf_build_frame()`/
`ukfe_rf_parse_frame()`-Funktionen 50 gültig signierte `UkfeRfCmdWifiDeauth`-Frames (simuliert
eine normale Sitzung), schneidet eines davon (Counter 25) als "vom Angreifer mitgeschnitten" mit,
simuliert dann einen Relay-Neustart (`s_rxCounter` zurück auf den jeweiligen Startwert) und
spielt das mitgeschnittene Frame erneut ein — einmal mit dem Ist-Zustand (`0`, Replay gelingt, der
Deauth-Befehl würde erneut an alle Satelliten rebroadcastet) und einmal mit der vorgeschlagenen
Fix-Logik (persistierter Floor, Replay wird abgelehnt, neue legitime Befehle funktionieren
weiterhin, NVS-Schreibzugriffe bleiben selten).

Der eigentliche Patch (`main.cpp`) wurde nicht gegen echte Hardware kompiliert (benötigt
PlatformIO + ESP32-Toolchain inkl. `WiFi`/`esp_now`/`esp_wifi`/`Preferences`), ist aber als rein
additive Änderung an bereits bestehenden Codepfaden gehalten (ein neues Feld, zwei neue freie
Funktionen, zwei zusätzliche Zeilen im bereits vorhandenen NVS-Block in `setup()`, ein bedingter
`begin()/putUInt()/end()`-Block in `scan_and_relay()`) und folgt exakt demselben, bereits in
`usb-army-penetrator`/`heltec-v4-hub`/`lilygo-ukfe-rx` angewendeten Muster. Lokaler Funktionstest:
PlatformIO-Build, auf den WROOM flashen, Befehl vom Flipper senden (z. B. `WifiDeauth`), den
Relay per Reset-Taste oder Stromtrennen neu starten, mit einem zweiten ESP32 im Promiscuous-Modus
das zuvor mitgeschnittene Frame erneut per `esp_wifi_80211_tx()`/ESP-NOW (oder direkt über die
Flipper-UART-Leitung) einspielen — vor dem Fix relayt der WROOM den Befehl erneut an alle
Satelliten, nach dem Fix bleibt die Reaktion aus (`s_rejected`-Zähler über `status` in der CLI
gegenprüfbar). Zusätzlich: Erst-Inbetriebnahme (NVS-Namespace `g4meover` einmal löschen,
`loaded == 0` prüfen) sowie ein harter Reset mitten in einer laufenden Sitzung (der zuletzt
gespeicherte `s_rxCtrFloor` muss danach weiterhin über jedem tatsächlich akzeptierten Counter
liegen).

## Risiko
- Niedrig: rein additiv in `src/main.cpp` (ein neues Feld, zwei neue freie Funktionen, zwei
  zusätzliche Zeilen im bereits vorhandenen NVS-Block in `setup()`, ein bedingter
  `begin()/putUInt()/end()`-Block in `scan_and_relay()`). Wire-Format, MAC/CRC-Berechnung und die
  Anti-Replay-Regel selbst (`ukfe_rf.c`) sind unverändert.
- Nutzt den bereits vorhandenen NVS-Namespace `"g4meover"` mit, nur ein neuer Key (`"rxctr"`) —
  kein Konflikt mit dem bestehenden `"txctr"`-Key. `RXCTR_MARGIN = 10` (wie bei den drei anderen
  Geräten) hält die Schreibhäufigkeit gering (ein Schreibzugriff etwa alle 5 akzeptierten
  Funkbefehle), deutlich seltener als `TXCTR_MARGIN = 100` beim CLI-Ping, da `s_rxCounter`
  deutlich häufiger fortschreitet als `s_txCounter`.
- Bewusst nicht behoben: ein *echter* Desync (z. B. Secret-Rotation oder Austausch des Flipper-
  oder Relay-Geräts ohne die gespeicherte NVS-Datei) bleibt weiterhin ohne Re-Pair-Mechanismus —
  dieser Patch schließt nur das Zeitfenster, das *jeder* Relay-Neustart bislang unabhängig vom
  Betriebsszenario öffnet.
- Verändert nicht `relay_frame()`/`espnow_broadcast()` selbst — ausschließlich die Frage, ob ein
  vom Flipper empfangener Befehl als "neu genug" akzeptiert und weitergereicht wird, bleibt
  angefasst.
- Löst nicht das in den Geschwister-Funden offene, hier nicht erneut geprüfte Restrisiko: ob die
  jeweiligen Satelliten selbst (ohne den dort vorgeschlagenen Fix) den vom Relay weitergereichten
  Replay noch über ihr eigenes Anti-Replay-Fenster abfangen — das bleibt Gegenstand der
  jeweiligen Geräte-eigenen Dokumente.

## Quelle
- `G4MEOVER18/g4meover-wifi-relay` (main, Commit `9b4a9e1`): `src/main.cpp` (`s_rxCounter`,
  `scan_and_relay()`, `relay_frame()`, `setup()`), `src/ukfe_rf.c`/`.h` (`ukfe_rf_parse_frame`,
  Anti-Replay-Prüfung, Header-Kommentar "auf beiden Seiten identisch implementieren").
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
  (ursprünglicher Fund derselben Protokoll-Anbindungslücke, Ursprung der hier wiederverwendeten
  Reserve-Logik), `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md`
  und `app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md`
  (identischer Fund im dritten und vierten geprüften Gerät).
