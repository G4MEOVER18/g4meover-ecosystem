# heltec-v4-hub — Hub-Broadcast-Counter sperrt eigene Status-Broadcasts nach jedem Neustart aus

## Was
Ich habe den bereits in der letzten Sitzung als Folge-Kandidat vorgemerkten TX-seitigen
Counter von `heltec-v4-hub` geprüft (`src/main.cpp`, `hub_broadcast()`), nachdem die RX-
Richtung (`s_flipperCounter`) bereits in
`app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md` behoben
vorgeschlagen wurde:

```c
// ---- Hub-Zustand ----
static uint32_t s_ackFromSat = 0, s_txCounter = 100;
...
// Eigenen ukfe_rf-Frame bauen + per ESP-NOW broadcasten (fuer Menue-Aktionen)
static void hub_broadcast(uint8_t cmd) {
    UkfeRfMessage m; ukfe_rf_make_simple(&m, (UkfeRfCmd)cmd); m.counter = ++s_txCounter;
    uint8_t f[UKFE_RF_MAX_FRAME];
    size_t n = ukfe_rf_build_frame(UKFE_SECRET, &m, f, sizeof(f));
    if(n && esp_now_send(BROADCAST_MAC, f, n) == ESP_OK) { s_relayed++; toast("Broadcast gesendet"); }
    else toast("Broadcast FAIL");
}
```

`hub_broadcast()` wird über die Menü-Taste ausgelöst (`activate()`, Seiten `PG_STATUS`/
`PG_SATS`, Kommando `UkfeRfCmdStatus`) und baut dabei ein eigenes, neues Frame mit einem
eigenen, monoton steigenden Counter (`s_txCounter`, Startwert `100`) — getrennt vom
`s_flipperCounter`-Zählerraum, der für vom Flipper weitergeleitete Befehle (`relay_frame()`)
verwendet wird. `s_txCounter` ist wie `s_flipperCounter` ein reines RAM-Feld: `setup()` lädt
nirgends einen persistierten Stand (derselbe Baum-weite `Preferences`/`nvs_`-Suchbefund wie
beim RX-Fund).

Die Satelliten (`heltec-ukfe-rx`, `lilygo-ukfe-rx`) prüfen jeden per ESP-NOW empfangenen
Frame mit genau derselben strikten Anti-Replay-Regel aus `ukfe_rf_parse_frame()`:

```c
if(last_counter) {
    if(m.counter <= *last_counter) return false;   // verbraucht/zu alt
    *last_counter = m.counter;
}
```

(verifiziert gegen `heltec-ukfe-rx/src/main.cpp:433` — `enowCounter` als dort persistentes
`last_counter` für per ESP-NOW empfangene Frames). Jeder Neustart des Hubs (Crash, Akku
leer, Reset-Taste, OTA-Update) setzt `s_txCounter` wieder auf `100` zurück. Hat ein Satellit
vorher bereits einen höheren Hub-Broadcast-Counter akzeptiert (z. B. `105` nach fünf
Status-Abfragen in der vorherigen Sitzung), verwirft er jeden weiteren `hub_broadcast()`-
Frame als "nicht neu genug", bis `s_txCounter` den zuvor erreichten Stand wieder überholt
hat — der Hub kann sich dann für eine unbestimmte Zahl an Tastendrücken nicht mehr bei den
Satelliten melden, ohne dass das am Hub-Display sichtbar wäre (`toast("Broadcast gesendet")`
wird bereits beim erfolgreichen `esp_now_send()` angezeigt, unabhängig davon, ob der
Satellit den Frame später als zu alt verwirft).

Das ist exakt dasselbe Muster wie der ursprüngliche Fund in
`app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md` (dort: Flipper-App-Neustart
sperrt die RF-Console gegen den Heltec-Agenten aus) — hier in der Gegenrichtung (Hub gegen
Satelliten) und mit dem bereits bekannten Reserve-Fix-Muster aus den beiden bisherigen
`heltec-v4-hub`/`usb-army-penetrator`-RX-Funden behoben.

Im Gegensatz zum RX-Fund (`s_flipperCounter`) ist das hier **keine Sicherheitslücke**: ein
Angreifer gewinnt nichts dadurch, dass der Hub sich selbst vorübergehend aussperrt — es ist
ein reiner Verfügbarkeits-Bug (Selbstaussperrung), wie bereits in der letzten Sitzung als
"kein Sicherheitsrisiko" vorgemerkt.

**Zusätzlich festgestellt, nicht Gegenstand dieses Patches:** Satelliten nutzen für *alle*
per ESP-NOW empfangenen Frames ein einziges, gemeinsames `last_counter`-Fenster
(`enowCounter` in `heltec-ukfe-rx`), unabhängig davon, ob der Frame vom Hub per
`relay_frame()` (identischer Frame, also im `s_flipperCounter`-Zählerraum des Flipper) oder
per `hub_broadcast()` (eigener, unabhängiger `s_txCounter`-Zählerraum) stammt. Beide
Zählerräume teilen sich dasselbe Akzeptanzfenster auf Satellitenseite und können sich damit
gegenseitig blockieren — unabhängig von einem Neustart, sobald ein relayter Flipper-Frame
einen höheren Counter trägt als der nächste Hub-Broadcast. Dieser Patch behebt nur das
Neustart-Szenario innerhalb des `s_txCounter`-Zählerraums selbst; die Zählerraum-Kollision
zwischen Hub-Relay und Hub-Broadcast ist ein eigenständiger, größerer Architektur-Fund
(betrifft `heltec-ukfe-rx`/`lilygo-ukfe-rx`, nicht nur `heltec-v4-hub`) und als Kandidat für
eine künftige Sitzung vorgemerkt.

## Nutzen
- Behebt eine Selbstaussperrung der Hub→Satelliten-Statusmeldung bei *jedem* Neustart des
  Hubs — passt in den WORKFLOW-Rahmen "reine Logik, ohne Hardware verifizierbar" und
  schließt die in der letzten Sitzung bewusst zurückgestellte Lücke.
- Wiederverwendet exakt dieselbe, bereits dreimal verifizierte Reserve-mit-
  Sicherheitsspanne-Logik (`ukfe_tx_counter_initial_reserve()`/
  `ukfe_tx_counter_needs_bump()`, strukturell identisch zu den bereits in
  `usb-army-penetrator` und `heltec-v4-hub` (RX) verwendeten Funktionen) — keine neue Idee,
  dieselbe bewährte Lösung ein viertes Mal angewendet, diesmal auf die TX-Richtung.
- Nutzt denselben bereits etablierten `Preferences`-NVS-Namespace `g4meoverv4` wie der
  RX-Fix, nur einen eigenen Key (`txCtr` statt `rfCtr`) — kein neuer `lib_deps`-Eintrag,
  kein Konflikt mit dem RX-seitigen Key.
- Verändert `UKFE_TX_COUNTER_START`s Startverhalten bei Erst-Inbetriebnahme nicht: ist noch
  kein Stand gespeichert, bleibt `s_txCounter` wie bisher bei `100`.

## Betroffene Dateien
- `src/main.cpp` — neues `Preferences`-Objekt `s_txCtrPrefs` + `s_txCtrPersistedFloor`
  neben `s_txCounter`, zwei neue reine Hilfsfunktionen (`ukfe_tx_counter_initial_reserve()`/
  `ukfe_tx_counter_needs_bump()`), `setup()` lädt den persistierten Floor aus NVS und
  übernimmt ihn nur, wenn er über dem bisherigen Startwert `100` liegt, `hub_broadcast()`
  prüft nach jedem gesendeten Broadcast, ob die Reserve nachgezogen werden muss.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/heltec-v4-hub`-Basis (`main`, Commit
`258d4ad` — derselbe Stand wie beim bereits dokumentierten RX-Fund; `git apply --check`
zweimal gegen frische Klone verifiziert, einmal vor und einmal nach dem Kopieren in den
endgültigen Repo-Pfad, real angewendet und byte-für-byte gegen die beabsichtigte Endfassung
abgeglichen) in
[`tx-broadcast-counter-restart-lockout-fix/main_cpp.patch`](tx-broadcast-counter-restart-lockout-fix/main_cpp.patch).

Einziger inhaltlicher Eingriff: Ursprung und Fortschreibung von `s_txCounter` in
`main.cpp`. `ukfe_rf.c`/`.h` (Frame-Format, MAC, CRC, die `counter <= last_counter`-Prüfung
selbst) bleiben unverändert.

**Hinweis für den lokalen Build:** Dieser Patch wurde unabhängig vom bereits vorgeschlagenen
RX-Fix (`rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`) gegen denselben
pristinen Basis-Commit erzeugt. Beide ändern `src/main.cpp` in benachbarten, aber
unterschiedlichen Regionen (Include, Hub-Zustand, `setup()`); sie sind nicht als eine Kette
gegeneinander getestet. Beim Anwenden beider Patches: zuerst einen anwenden (`git apply`),
danach den zweiten per `git apply --3way` oder manuell einarbeiten — beide Diffs sind kurz
und die betroffenen Stellen klar benannt (`s_flipperCounter` vs. `s_txCounter`,
`UKFE_RF_COUNTER_PREF_KEY`/`"rfCtr"` vs. `UKFE_TX_COUNTER_PREF_KEY`/`"txCtr"`), sodass ein
manueller Merge lokal keinen nennenswerten Aufwand bedeutet.

## Testhinweis (ohne Hardware)
[`repro-tx-broadcast-counter-restart-lockout/repro.c`](repro-tx-broadcast-counter-restart-lockout/repro.c)
nutzt die unveränderten Original-Dateien `ukfe_rf.c`/`ukfe_rf.h` (1:1 aus `heltec-v4-hub`
übernommen) — keine ESP32/RadioLib/Arduino-Abhängigkeit nötig:

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c ukfe_rf.c
$ ./repro
Szenario 1 (Sitzung vor Neustart): 0/5 abgelehnt (sollte 0 sein), Satellit zuletzt akzeptiert: Counter 105
BUG:  Sitzung 2 nach Neustart: 5/20 Status-Broadcasts vom Satelliten verworfen (Hub kann sich erst nach Aufholen wieder melden)
Szenario 2 (Sitzung vor Neustart, mit Fix): 0/5 abgelehnt, 1 NVS-Schreibzugriffe
FIX:  Sitzung 2 nach Neustart: 0/20 Status-Broadcasts vom Satelliten verworfen
FIX:  nach unsauberem Absturz bei Counter 130 (Reserve war bei 140): erster Broadcast nach Neustart (Counter 141) sofort akzeptiert -> ja
Alle Szenarien bestaetigt (exit code 0)
```

Identisch unter `-O1` verifiziert, in beiden Fällen keine ASan-/UBSan-Meldungen und keine
Compiler-Warnungen. Das Repro baut mit den echten `ukfe_rf_build_frame()`/
`ukfe_rf_parse_frame()`-Funktionen simulierte Hub-Broadcasts, modelliert die Satelliten-
seitige `counter <= last_counter`-Prüfung (nachgestellt anhand von
`heltec-ukfe-rx/src/main.cpp`s `enowCounter`), simuliert dann einen Hub-Neustart
(`s_txCounter` zurück auf `100`) und zeigt sowohl den Ist-Zustand (Broadcasts werden
verworfen, bis der Counter aufgeholt hat) als auch die Fix-Logik (persistierter Floor, kein
Broadcast wird nach Neustart verworfen, inklusive Crash-Sicherheits-Szenario mit
Vorreservierung).

Der eigentliche Patch (`main.cpp`) wurde nicht gegen echte Hardware kompiliert (benötigt
PlatformIO + ESP32-S3-Toolchain), ist aber als rein additive Änderung an bereits bestehenden
Codepfaden gehalten (ein neues `Preferences`-Objekt, zwei neue freie Funktionen, ein
`begin()`/`getUInt()`/`putUInt()`-Aufruf in `setup()`, ein bedingter `putUInt()`-Aufruf in
`hub_broadcast()`) und folgt exakt demselben, bereits dreimal angewendeten Muster. Lokaler
Funktionstest: `ufbt`/PlatformIO-Build, Hub mit mindestens einem gepaarten Satelliten
betreiben, mehrfach "Status" über die Menü-Taste (Seite `PG_STATUS`/`PG_SATS`) auslösen, Hub
per Reset-Taste neu starten, erneut "Status" auslösen — vor dem Fix bleibt die Reaktion am
Satelliten für die ersten Versuche aus (sichtbar z. B. an einem fehlenden ACK/Log-Eintrag
auf Satellitenseite trotz `toast("Broadcast gesendet")` am Hub), nach dem Fix reagiert der
Satellit sofort wieder. Zusätzlich: Erst-Inbetriebnahme (NVS-Namespace `g4meoverv4` einmal
löschen, `s_txCounter` muss weiterhin bei `100` starten) sowie ein harter Reset mitten in
einer laufenden Sitzung mit mehreren Broadcasts dazwischen.

## Risiko
- Niedrig: rein additiv in `src/main.cpp` (ein neues `Preferences`-Objekt, zwei neue freie
  Funktionen, ein `begin()`/`getUInt()`/`putUInt()`-Aufruf in `setup()`, ein bedingter
  `putUInt()`-Aufruf in `hub_broadcast()`). Wire-Format, MAC/CRC-Berechnung und die
  Anti-Replay-Regel selbst (`ukfe_rf.c`) sind unverändert.
- Neue NVS-Nutzung über denselben bereits etablierten Namespace (`g4meoverv4`), eigener Key
  (`txCtr`) — kein Konflikt mit dem RX-seitigen `rfCtr`-Key, falls beide Fixes angewendet
  werden. `UKFE_TX_COUNTER_MARGIN = 10` hält die Schreibhäufigkeit gering (ein
  Schreibzugriff etwa alle 5 Broadcasts, siehe Repro-Ausgabe).
- Löst **nicht** die oben separat dokumentierte Zählerraum-Kollision zwischen `relay_frame()`
  (Flipper-Zählerraum) und `hub_broadcast()` (eigener `s_txCounter`-Zählerraum) auf
  Satellitenseite — dieser Patch macht `s_txCounter` nur neustartfest, ändert aber nichts
  daran, dass Satelliten beide Zählerräume in ein gemeinsames `last_counter`-Fenster
  mischen. Als eigenständiger Architektur-Fund vorgemerkt (siehe oben).
- Verändert nicht `activate()`/die Menüsteuerung selbst — ausschließlich die Frage, ob ein
  Status-Broadcast nach einem Neustart als "neu genug" gilt, bleibt unangetastet.
- Kein Sicherheitsrisiko in beide Richtungen: weder öffnet dieser Fix ein Replay-Fenster
  (der Counter bleibt strikt monoton steigend und MAC-authentifiziert), noch behebt er eins
  (`s_txCounter` schützt nur die Zustellbarkeit der eigenen Hub-Broadcasts, nicht vor
  Fremdbefehlen).

## Quelle
- `G4MEOVER18/heltec-v4-hub` (main, Commit `258d4ad`): `src/main.cpp` (`s_txCounter`,
  `hub_broadcast()`, `activate()`, `setup()`), `src/ukfe_rf.c`/`.h` (`ukfe_rf_parse_frame`,
  Anti-Replay-Prüfung).
- `G4MEOVER18/heltec-ukfe-rx` (main): `src/main.cpp` (`enowCounter`, ESP-NOW-Empfangspfad
  mit `ukfe_rf_parse_frame(..., &enowCounter)`) — zur Bestätigung, dass Satelliten
  Hub-Broadcasts mit derselben strikten Counter-Regel prüfen wie relayte Flipper-Frames, und
  Ursprung des oben separat vermerkten Zählerraum-Kollisionsfunds.
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md` (ursprünglicher Fund
  desselben Selbstaussperrungs-Musters, Ursprung der hier wiederverwendeten Reserve-Logik),
  `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md`
  (RX-seitiger Fund im selben Gerät, dort als Sicherheitslücke statt Verfügbarkeits-Bug
  eingeordnet, dort auch der ausdrückliche Vermerk, der diese Sitzung veranlasst hat).
