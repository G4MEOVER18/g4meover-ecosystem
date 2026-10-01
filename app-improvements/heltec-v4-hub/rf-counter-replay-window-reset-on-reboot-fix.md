# heltec-v4-hub — Hub-seitiger Anti-Replay-Counter der `ukfe_rf`-Konsole öffnet das Replay-Fenster nach jedem Neustart

## Was
Ich habe `heltec-v4-hub` geprüft — den Heltec-WiFi-LoRa-32-V4-"Masterpiece"-Hub, der vom
Flipper per GPIO-UART (Pin 13/14 → GPIO47/48) gesteuert wird, signierte `ukfe_rf`-Befehle
selbst ausführt ("One Piece"-Executor für WiFi/BLE/LoRaWAN) und zusätzlich per ESP-NOW an
die Satelliten weiterfunkt (`src/main.cpp`, `scan_and_relay()`/`relay_frame()`/`act()`).
`heltec-v4-hub` ist bislang nicht im eigenen App-Verbesserungs-Katalog geführt; ich habe die
drei eigenständigen Repos (`RollJam`, `RollLab`, `lora-ukfe`) in der letzten Sitzung bereits
unverändert vorgefunden und daher stattdessen hier nach bislang nicht erfasstem eigenen Code
gesucht.

Das gemeinsame `ukfe_rf`-Protokoll (`src/ukfe_rf.c`/`.h`, wortgleich mit der bereits
geprüften Fassung aus `usb-army-penetrator` — Byte-für-Byte-Diff gegenübergestellt, nur der
Kommando-Enum in `ukfe_rf.h` hat zusätzliche Werte für die V4-eigenen Befehle) erzwingt einen
strikt monoton steigenden, MAC-authentifizierten Rolling-Counter als Anti-Replay-Schutz:

```c
if(last_counter) {                                        // Anti-Replay
    if(m.counter <= *last_counter) return false;          // verbraucht/replay
    *last_counter = m.counter;
}
```

In `heltec-v4-hub` wird dieser Mechanismus für die Flipper→Hub-Richtung so angebunden:

```c
// ---- Hub-Zustand ----
static uint32_t s_flipperCounter = 0;
...
if(ukfe_rf_parse_frame(UKFE_SECRET, s_buf, real_len, &msg, &s_flipperCounter)) {
    s_rxFromFlipper++;
    relay_frame(s_buf, real_len, &msg);   // an Satelliten weiterfunken
    act(&msg);                            // + V4 fuehrt selbst aus (One Piece)
}
```

`s_flipperCounter` ist ein reines RAM-Feld (statische globale Variable, Startwert `0`,
`setup()` ruft nirgends ein `prefs`/NVS-Lesen auf — ich habe den gesamten Baum nach
`Preferences`/`nvs_` durchsucht, kein Treffer). Das ist exakt derselbe Fund wie bereits in
`app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
dokumentiert, hier aber in einem dritten, bislang ungeprüften Gerät: Jeder Neustart des Hubs
(Absturz, Akku leer, versehentlicher Druck auf die Reset-Taste, OTA-Update) setzt
`s_flipperCounter` auf `0` zurück. Ein Angreifer, der zuvor per SDR ein gültiges, signiertes
Flipper→Hub-Frame mitgeschnitten hat (z. B. einen `WifiDeauth`- oder `EvilPortal`-Befehl mit
Counter 25), kann dieses Frame nach jedem Neustart des Hubs erneut senden — `25 <= 0` ist
falsch, das Frame wird als "neu" akzeptiert, obwohl es bereits einmal verwendet wurde. Der
Hub führt den wiedergespielten Befehl dann sowohl selbst aus (`act()`) als auch an alle
Satelliten weiter (`relay_frame()` per ESP-NOW-Broadcast) — der Schaden eines erfolgreichen
Replays trifft hier also potenziell die gesamte Flotte auf einmal, nicht nur ein einzelnes
Gerät.

Der Fund liegt wieder nicht im Protokoll selbst (unverändert korrekt, siehe oben), sondern
in seiner Anbindung an genau dieses dritte Gerät.

## Nutzen
- Schließt dieselbe Sicherheitslücke im eigenen Anti-Replay-Mechanismus wie bereits bei
  `usb-army-penetrator` behoben, diesmal für den Hub, der zusätzlich per ESP-NOW an die
  gesamte Satelliten-Flotte weiterfunkt — passt in den in CONSTRAINTS Punkt 5 verlangten
  defensiven Rahmen (eigenes Pentest-Werkzeug soll selbst keine unnötig ausnutzbare
  Schwachstelle enthalten).
- Wiederverwendet exakt dieselbe, bereits zweimal verifizierte Reserve-mit-
  Sicherheitsspanne-Logik (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`)
  — keine neue Krypto, kein neues Protokollfeld, dieselbe bewährte Idee ein drittes Mal
  angewendet.
- Nutzt die ESP32-Arduino-Standardbibliothek `Preferences` (NVS), die bereits in der
  Toolchain verfügbar ist (kein zusätzlicher `lib_deps`-Eintrag in `platformio.ini` nötig) —
  genau der Mechanismus, den auch die Fix-Vorlage in `usb-army-penetrator` verwendet.
- Das Wire-Format, `ukfe_rf_mac()`/`ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()` und die
  `counter <= last_counter`-Regel selbst bleiben unverändert — ein bereits gepaarter Flipper
  (`lora-ukfe`) benötigt keine Anpassung.

## Betroffene Dateien
- `src/main.cpp` — neues `#include <Preferences.h>`, neues `Preferences`-Objekt
  `s_rfCtrPrefs` + `s_rfCtrPersistedFloor` neben `s_flipperCounter`, `setup()` lädt den
  persistierten Stand aus NVS statt `s_flipperCounter` bei `0` zu belassen, `scan_and_relay()`
  prüft nach jedem akzeptierten Flipper-Frame, ob die Reserve nachgezogen werden muss.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/heltec-v4-hub`-Basis (`main`, Commit
`258d4ad`; `git apply --check` vorab gegen eine frische Kopie verifiziert, real angewendet
und byte-für-byte gegen die beabsichtigte Endfassung abgeglichen) in
[`rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`](rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch).

Einziger inhaltlicher Eingriff: Ursprung und Fortschreibung von `s_flipperCounter` in
`main.cpp`. `ukfe_rf.c`/`.h` (Frame-Format, MAC, CRC, die `counter <= last_counter`-Prüfung
selbst) bleiben unverändert.

## Testhinweis (ohne Hardware)
[`repro-rf-counter-replay-window-reset/repro.c`](repro-rf-counter-replay-window-reset/repro.c)
nutzt die unveränderten Original-Dateien `ukfe_rf.c`/`ukfe_rf.h` (1:1 aus `heltec-v4-hub`
übernommen, laut eigenem Dateikommentar "plattformunabhaengig, ohne HW-Abhaengigkeit") — keine
ESP32/RadioLib/Arduino-Abhängigkeit nötig:

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c ukfe_rf.c
$ ./repro
Szenario 1 (Sitzung vor Neustart): 0/50 abgelehnt (sollte 0 sein)
BUG:  captured Flipper->Hub-Frame (Counter 25) nach Neustart erneut gesendet -> ANGENOMMEN (Replay erfolgreich!)
Szenario 2 (Sitzung vor Neustart, mit Fix): 0/50 abgelehnt, 10 NVS-Schreibzugriffe (statt 50)
FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> abgelehnt (Replay verhindert)
FIX:  neuer, legitimer Befehl (Counter 61) nach Neustart -> angenommen
Alle Szenarien bestaetigt (exit code 0)
```

Identisch unter `-O1` verifiziert, in beiden Fällen keine ASan-/UBSan-Meldungen und keine
Compiler-Warnungen. Das Repro baut mit den echten `ukfe_rf_build_frame()`/
`ukfe_rf_parse_frame()`-Funktionen 50 gültig signierte Frames (simuliert eine normale
Sitzung), schneidet eines davon als "vom Angreifer mitgeschnitten" mit, simuliert dann einen
Hub-Neustart (`s_flipperCounter` zurück auf den jeweiligen Startwert) und spielt das
mitgeschnittene Frame erneut ein — einmal mit dem Ist-Zustand (`0`, Replay gelingt) und
einmal mit der vorgeschlagenen Fix-Logik (persistierter Floor, Replay wird abgelehnt, neue
legitime Befehle funktionieren weiterhin, NVS-Schreibzugriffe bleiben selten).

Der eigentliche Patch (`main.cpp`) wurde nicht gegen echte Hardware kompiliert (benötigt
PlatformIO + ESP32-S3-Toolchain), ist aber als rein additive Änderung an bereits bestehenden
Codepfaden gehalten (ein neues Include, ein neues `Preferences`-Objekt, zwei neue freie
Funktionen, ein `begin()`/`getUInt()`/`putUInt()`-Aufruf in `setup()`, ein bedingter
`putUInt()`-Aufruf in `scan_and_relay()`) und folgt exakt demselben, bereits in
`usb-army-penetrator` angewendeten und dort als "lokal funktional getestet" vermerkten
Muster. Lokaler Funktionstest: `ufbt`/PlatformIO-Build, dann Befehl vom Flipper senden,
Hub per Reset-Taste neu starten, mit einem SDR/zweiten Empfänger das zuvor mitgeschnittene
Frame erneut aussenden — vor dem Fix löst der Hub den Befehl erneut aus (Toast + ESP-NOW-
Broadcast an die Satelliten), nach dem Fix bleibt die Reaktion aus. Zusätzlich: Erst-
Inbetriebnahme (NVS-Namespace `g4meoverv4` einmal löschen, `loaded == 0` prüfen) sowie ein
harter Reset mitten in einer laufenden Sitzung (der zuletzt gespeicherte
`s_rfCtrPersistedFloor` muss danach weiterhin über jedem tatsächlich akzeptierten Counter
liegen).

## Risiko
- Niedrig: rein additiv in `src/main.cpp` (ein neues Include, ein neues Preferences-Objekt,
  zwei neue freie Funktionen, ein `begin()`/`getUInt()`/`putUInt()`-Aufruf in `setup()`, ein
  bedingter `putUInt()`-Aufruf in `scan_and_relay()`). Wire-Format, MAC/CRC-Berechnung und die
  Anti-Replay-Regel selbst (`ukfe_rf.c`) sind unverändert.
- Neue NVS-Nutzung über einen neuen, dedizierten Namespace (`g4meoverv4`/`rfCtr`) — kein
  Konflikt mit vorhandenen NVS-Zugriffen, da `heltec-v4-hub` bislang gar keine `Preferences`
  verwendet hat. `UKFE_RF_COUNTER_MARGIN = 10` hält die Schreibhäufigkeit gering (ein
  Schreibzugriff etwa alle 5 akzeptierten Funkbefehle).
- Bewusst nicht behoben: ein *echter* Desync (z. B. Secret-Rotation oder Austausch des
  Flipper- oder Hub-Geräts ohne die gespeicherte Datei) bleibt weiterhin ohne
  Re-Pair-Mechanismus — dieser Patch schließt nur das Zeitfenster, das *jeder* Hub-Neustart
  bislang unabhängig vom Betriebsszenario öffnet.
- Verändert nicht `act()`/`relay_frame()`/die Angriffsausführung selbst — ausschließlich die
  Frage, ob ein Funkbefehl als "neu genug" akzeptiert wird, bleibt unangetastet.
- Der TX-seitige `s_txCounter` (Hub → Satelliten, `hub_broadcast()`) bleibt bewusst
  unangetastet: dort führt ein RAM-Reset nur zu einer vorübergehenden Selbstaussperrung
  (Verfügbarkeits-Bug wie beim ursprünglichen `lora-ukfe`-Fund), nicht zu einer
  Sicherheitslücke, und ist kein Gegenstand dieser Sitzung.

## Quelle
- `G4MEOVER18/heltec-v4-hub` (main, Commit `258d4ad`): `src/main.cpp`
  (`s_flipperCounter`, `scan_and_relay()`, `setup()`), `src/ukfe_rf.c`/`.h`
  (`ukfe_rf_parse_frame`, Anti-Replay-Prüfung, Header-Kommentar "auf beiden Seiten identisch
  implementieren").
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
  (identischer Fund im zweiten geprüften Gerät, Ursprung der hier wiederverwendeten
  Reserve-Logik), `app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md`
  (ursprünglicher TX-seitiger Fund derselben Protokoll-Anbindungslücke).
