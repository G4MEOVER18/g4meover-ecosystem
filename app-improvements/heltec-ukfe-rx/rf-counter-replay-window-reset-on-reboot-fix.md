# heltec-ukfe-rx — beide Anti-Replay-Fenster der `ukfe_rf`-Konsole (868-FSK UND ESP-NOW) öffnen sich nach jedem Neustart erneut

## Was

Ich habe `heltec-ukfe-rx` (Heltec WiFi LoRa 32 V3, 868-FSK-Empfänger für die Flipper-RF-
Console UND ESP-NOW-Bridge/-Satellit für das WROOM-Relay) vollständig gelesen. Dieses Gerät
hat eine Doppelrolle und führt deshalb laut eigenem Code-Kommentar in `src/main.cpp` bewusst
**zwei getrennte** Anti-Replay-Fenster:

```c
static uint32_t lastCounter = 0;     // Anti-Replay-Fenster (868-Funk)
...
static uint32_t enowCounter = 0;     // SEPARATES Anti-Replay-Fenster (WiFi)
```

Diese Trennung selbst ist bereits in der Sitzung vom 2026-10-01 geprüft und für korrekt
befunden worden (`app-improvements/ukfe-esp-now-mesh/espnow-origin-tag-counter-window-split-fix.md`,
dort ausdrücklich vermerkt: „nutzt aber für die 868-Strecke bereits ein separates
`lastCounter` — der Fund betrifft ausschließlich den ESP-NOW-Empfangspfad"). Diese Sitzung
griff dort gezielt nur den ESP-NOW-Pfad auf; der 868-Pfad wurde als „von diesem Fund nicht
betroffen" ausdrücklich zurückgestellt und als „Kandidat für eine künftige
App-Verbesserungs-Sitzung" vermerkt — genau das hole ich hier nach, zusammen mit einem
zweiten, bisher unabhängig davon offenen Fund im selben Gerät.

Beide Felder sind reine RAM-Variablen (statische globale `uint32_t`, Startwert `0`). Ich habe
den gesamten Baum nach `Preferences`/`nvs_` durchsucht — kein Treffer, `setup()` liest nirgends
einen persistierten Stand. Das ist exakt derselbe Fund wie bereits viermal dokumentiert
(`usb-army-penetrator`, `heltec-v4-hub`, `lilygo-ukfe-rx`, `g4meover-wifi-relay`), hier aber in
einem Gerät, das davon **gleich zweimal unabhängig** betroffen ist:

- **868-FSK-Pfad** (`lastCounter`, `loop()` Zeile ~461): jeder Neustart öffnet das
  Replay-Fenster für jedes zuvor per SDR mitgeschnittene, gültig signierte Flipper-Frame
  erneut. Besonders relevant, weil dieses Gerät der **Hub** der ESP-NOW-Flotte ist — ein
  akzeptierter 868-Frame wird nicht nur lokal ausgeführt (`act()`), sondern per
  `esp_now_send(BROADCAST_MAC, ...)` an alle Satelliten weitergefunkt (Zeile ~467). Ein
  erfolgreicher Replay trifft damit potenziell die gesamte Flotte gleichzeitig.
- **ESP-NOW-Pfad** (`enowCounter`, `loop()` Zeile ~433): jeder Neustart öffnet unabhängig
  davon das Replay-Fenster für jedes zuvor mitgeschnittene, gültig signierte WROOM-Relay-Frame
  erneut. Dieses Gerät führt HID-/WiFi-/BLE-Befehle (`act()`) direkt selbst aus — ein Replay
  hier löst denselben Befehl am Gerät erneut aus, ohne dass der Flipper oder das WROOM-Relay
  beteiligt sind.

Beide Fenster sind laut Code bewusst unabhängig voneinander — ein Fix für nur eines der beiden
würde das andere unverändert verwundbar lassen. Mein Host-Repro (unten, Szenario C) bestätigt
zusätzlich, dass beide Fenster auch nach dem Fix unabhängig bleiben (kein gegenseitiges
Sperren bei gleichem Counter-Wert in beiden Fenstern).

## Nutzen

- Schließt dieselbe, bereits viermal verifizierte Sicherheitslücke im eigenen
  Anti-Replay-Mechanismus ein fünftes und sechstes Mal — hier konsequent für **beide**
  unabhängigen Fenster desselben Geräts, statt nur eines davon (passt in den in CONSTRAINTS
  Punkt 5 verlangten defensiven Rahmen: das eigene Pentest-Werkzeug soll selbst keine
  unnötig ausnutzbare Schwachstelle enthalten).
- Wiederverwendet exakt dieselbe, bereits viermal verifizierte Reserve-mit-
  Sicherheitsspanne-Logik (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`)
  — keine neue Krypto, kein neues Protokollfeld, dieselbe bewährte Idee ein fünftes Mal
  angewendet, hier zweifach instanziiert (ein Satz Funktionen, zwei unabhängige Floors/Keys).
- Nutzt die ESP32-Arduino-Standardbibliothek `Preferences` (NVS), die bereits in der
  Toolchain verfügbar ist (kein zusätzlicher `lib_deps`-Eintrag in `platformio.ini` nötig).
- Das Wire-Format, `ukfe_rf_mac()`/`ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()` und die
  `counter <= last_counter`-Regel selbst bleiben unverändert — weder der Flipper (`lora-ukfe`)
  noch das WROOM-Relay (`g4meover-wifi-relay`) benötigen eine Anpassung.
- Beide Floors teilen sich einen `Preferences`-Namensraum (`g4meoverHtk`), aber je einen
  eigenen Key (`rf868Ctr`/`rfEnowCtr`) — ein `begin()`-Aufruf statt zwei, keine Kollisionsgefahr.

## Betroffene Dateien

- `src/main.cpp` — neues `#include <Preferences.h>`, neues `Preferences`-Objekt
  `s_rfCtrPrefs` + zwei unabhängige `s_rf868PersistedFloor`/`s_rfEnowPersistedFloor` neben
  `lastCounter`/`enowCounter`, `setup()` lädt beide persistierten Stände aus NVS statt beide
  Felder bei `0` zu belassen, `loop()` prüft nach jedem akzeptierten 868- bzw. ESP-NOW-Frame
  unabhängig, ob die jeweilige Reserve nachgezogen werden muss.

## Patch

Unified-Diff gegen die unveränderte `G4MEOVER18/heltec-ukfe-rx`-Basis (`main`, Commit
`23e1f40`; `git apply --check` vorab gegen eine frische, unabhängige Zweitkopie verifiziert,
real angewendet und byte-für-byte gegen die beabsichtigte Endfassung abgeglichen) in
[`rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`](rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch).

Einziger inhaltlicher Eingriff: Ursprung und Fortschreibung von `lastCounter`/`enowCounter`
in `main.cpp`. `ukfe_rf.c`/`.h` (Frame-Format, MAC, CRC, die `counter <= last_counter`-Prüfung
selbst) bleiben unverändert — byte-für-byte identisch mit der bereits in
`usb-army-penetrator`/`heltec-v4-hub`/`lilygo-ukfe-rx`/`g4meover-wifi-relay` geprüften Fassung
(einzig `ukfe_rf.h`s Kommando-Enum hat hier zusätzliche, neuere Werte — ohne Einfluss auf
diesen Fund, da nur das Frame-Format/die Anti-Replay-Prüfung selbst relevant sind).

## Testhinweis (ohne Hardware)

[`repro-rf-counter-replay-window-reset/repro.c`](repro-rf-counter-replay-window-reset/repro.c)
nutzt die unveränderten Original-Dateien `ukfe_rf.c`/`ukfe_rf.h` (1:1 aus `heltec-ukfe-rx`
übernommen) — keine ESP32/RadioLib/Arduino-Abhängigkeit nötig:

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c ukfe_rf.c
$ ./repro
== 868-FSK-Fenster (lastCounter, vom Flipper) ==
Szenario A (Sitzung vor Neustart): 0/50 abgelehnt (sollte 0 sein)
BUG:  captured WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> ANGENOMMEN (Replay erfolgreich!)
Szenario B (Sitzung vor Neustart, mit Fix): 0/50 abgelehnt, 10 NVS-Schreibzugriffe (statt 50)
FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> abgelehnt (Replay verhindert)
FIX:  neuer, legitimer Befehl (Counter 61) nach Neustart -> angenommen
== ESP-NOW-Fenster (enowCounter, vom WROOM-Relay) ==
Szenario A (Sitzung vor Neustart): 0/50 abgelehnt (sollte 0 sein)
BUG:  captured WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> ANGENOMMEN (Replay erfolgreich!)
Szenario B (Sitzung vor Neustart, mit Fix): 0/50 abgelehnt, 10 NVS-Schreibzugriffe (statt 50)
FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> abgelehnt (Replay verhindert)
FIX:  neuer, legitimer Befehl (Counter 61) nach Neustart -> angenommen
Szenario C: identischer Counter (5) in beiden getrennten Fenstern -> 868=angenommen, ESPNOW=angenommen
Szenario C: Replay im 868-Fenster sperrt ESP-NOW-Fenster NICHT -> naechster ESPNOW-Befehl: angenommen
Alle Szenarien bestaetigt (exit code 0)
```

Identisch unter `-O1` verifiziert, in beiden Fällen keine ASan-/UBSan-Meldungen und keine
Compiler-Warnungen. Das Repro baut für jedes der beiden Fenster separat 50 gültig signierte
`WifiDeauth`-Frames (simuliert eine normale Sitzung), schneidet eines davon als "vom
Angreifer mitgeschnitten" mit, simuliert dann einen Geräte-Neustart (Fenster zurück auf den
jeweiligen Startwert) und spielt das mitgeschnittene Frame erneut ein — einmal mit dem
Ist-Zustand (`0`, Replay gelingt) und einmal mit der vorgeschlagenen Fix-Logik (persistierter
Floor, Replay wird abgelehnt, neue legitime Befehle funktionieren weiterhin). Ein drittes
Szenario (C) bestätigt zusätzlich, dass beide Fenster bei identischem Counter-Wert unabhängig
voneinander akzeptieren bzw. sperren — kein Cross-Window-Lockout, keine versehentliche
Vermischung der beiden Floors.

Der eigentliche Patch (`main.cpp`) wurde nicht gegen echte Hardware kompiliert (benötigt
PlatformIO + ESP32-S3-Toolchain), ist aber als rein additive Änderung an bereits bestehenden
Codepfaden gehalten (ein neues Include, ein neues `Preferences`-Objekt, zwei neue freie
Funktionen, zwei `begin()`/`getUInt()`/`putUInt()`-Paare in `setup()`, je ein bedingter
`putUInt()`-Aufruf an den beiden bestehenden Annahme-Stellen in `loop()`) und folgt exakt
demselben, bereits viermal angewendeten Muster. Lokaler Funktionstest: PlatformIO-Build, dann
je einen Befehl über 868 (Flipper) und über ESP-NOW (WROOM-Relay) senden, Gerät per
Reset-Taste neu starten, mit einem zweiten Flipper/SDR bzw. über das Relay das zuvor
mitgeschnittene Frame jeweils erneut aussenden — vor dem Fix löst das Gerät den Befehl erneut
aus (OLED-Anzeige + ggf. ESP-NOW-Broadcast an die Satelliten), nach dem Fix bleibt die Reaktion
in beiden Fällen aus. Zusätzlich: Erst-Inbetriebnahme (NVS-Namensraum `g4meoverHtk` einmal
löschen, beide `getUInt(..., 0)`-Fälle durchlaufen) sowie ein harter Reset mitten in einer
laufenden Sitzung auf jedem der beiden Pfade einzeln.

## Risiko

- Niedrig: rein additiv in `src/main.cpp` (ein neues Include, ein neues `Preferences`-Objekt,
  zwei neue freie Funktionen, zwei Lade-Blöcke in `setup()`, zwei bedingte `putUInt()`-Aufrufe
  in `loop()`). Wire-Format, MAC/CRC-Berechnung und die Anti-Replay-Regel selbst (`ukfe_rf.c`)
  sind unverändert.
- Neue NVS-Nutzung über einen neuen, dedizierten Namespace (`g4meoverHtk`) — kein Konflikt mit
  vorhandenem Code, da `heltec-ukfe-rx` bislang keine `Preferences` verwendet (gesamter Baum
  durchsucht, kein Treffer). `UKFE_RF_COUNTER_MARGIN = 10` hält die Schreibhäufigkeit je
  Fenster gering (ein Schreibzugriff etwa alle 5 akzeptierten Frames, unabhängig je Fenster).
- Bewusst nicht behoben: ein *echter* Desync (z. B. Secret-Rotation oder Austausch des
  Flipper- oder WROOM-Geräts ohne die gespeicherte Datei) bleibt weiterhin ohne
  Re-Pair-Mechanismus — dieser Patch schließt nur das Zeitfenster, das *jeder* Neustart
  bislang unabhängig vom Betriebsszenario für beide Fenster öffnet.
- Der TX-seitige `respCounter` (Antwort-ACKs an das WROOM-Relay, `send_espnow_ack()`) bleibt
  bewusst unangetastet: ein RAM-Reset führt dort höchstens zu einer vorübergehenden
  Selbstaussperrung der eigenen Antwort (Verfügbarkeits-Bug, kein Sicherheitsproblem, da das
  Relay laut `app-improvements/g4meover-wifi-relay/sat-resp-ack-replay-missing-counter-check-fix.md`
  Antwort-Frames ohnehin mit einem eigenen, satellitenspezifischen Fenster prüft) — kein
  Gegenstand dieser Sitzung.
- `act()`/die Angriffsausführung selbst bleibt unverändert — ausschließlich die Frage, ob ein
  868- oder ESP-NOW-Frame als "neu genug" akzeptiert wird, ist betroffen.

## Quelle

- `G4MEOVER18/heltec-ukfe-rx` (main, Commit `23e1f40`): `src/main.cpp` (`lastCounter`,
  `enowCounter`, `setup()`, `loop()`), `src/ukfe_rf.c`/`.h` (`ukfe_rf_parse_frame`,
  Anti-Replay-Prüfung).
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
  (Ursprung der hier wiederverwendeten Reserve-Logik, erstes geprüftes Gerät),
  `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md` (drittes
  Gerät), `app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md`
  (viertes Gerät), `app-improvements/g4meover-wifi-relay/rx-counter-replay-window-reset-on-reboot-fix.md`
  (fünftes Gerät — RX-Richtung), `app-improvements/ukfe-esp-now-mesh/espnow-origin-tag-counter-window-split-fix.md`
  (Sitzung vom 2026-10-01: dort wurde `heltec-ukfe-rx`s ESP-NOW-Pfad erstmals geprüft und der
  868-Pfad ausdrücklich als „Kandidat für eine künftige App-Verbesserungs-Sitzung"
  zurückgestellt — hier aufgegriffen).
