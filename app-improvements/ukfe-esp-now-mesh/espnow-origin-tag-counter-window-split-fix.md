# ESP-NOW-Mesh (heltec-v4-hub -> heltec-ukfe-rx/lilygo-ukfe-rx) — Hub-Broadcasts und relayte
# Flipper-Frames teilen sich ein gemeinsames Anti-Replay-Fenster

## Was

Ich habe den in der letzten Sitzung als eigenständigen Architektur-Fund vorgemerkten
Befund aufgegriffen (siehe `app-improvements/heltec-v4-hub/tx-broadcast-counter-restart-
lockout-fix.md`, Abschnitt "Offen"): die Satelliten `heltec-ukfe-rx` und `lilygo-ukfe-rx`
prüfen **jeden** per ESP-NOW empfangenen `ukfe_rf`-Frame mit genau **einem** gemeinsamen
Anti-Replay-Fenster, unabhängig davon, ob der Frame vom Hub (`heltec-v4-hub`) selbst
erzeugt wurde oder ob der Hub ihn nur unverändert vom Flipper weiterfunkt:

```c
// heltec-v4-hub/src/main.cpp
static uint32_t s_flipperCounter = 0;      // Zaehlerraum: Flipper -> Hub (868 MHz)
static uint32_t s_ackFromSat = 0, s_txCounter = 100;   // Zaehlerraum: Hub -> Satelliten (eigen)

static void hub_broadcast(uint8_t cmd) {               // Menue-Aktion (PG_STATUS/PG_SATS)
    UkfeRfMessage m; ukfe_rf_make_simple(&m, (UkfeRfCmd)cmd); m.counter = ++s_txCounter;
    ...
    esp_now_send(BROADCAST_MAC, f, n);                 // neues Frame, EIGENER Counter
}

static void relay_frame(const uint8_t* frame, size_t real_len, const UkfeRfMessage* msg) {
    ...
    esp_now_send(BROADCAST_MAC, frame, real_len);       // unveraendertes Flipper-Frame, FREMDER Counter
}
```

```c
// heltec-ukfe-rx/src/main.cpp:433 (lilygo-ukfe-rx/src/main.cpp:269 identisch)
static uint32_t enowCounter = 0;   // EIN Fenster fuer BEIDE obigen Quellen
...
if(ukfe_rf_parse_frame(UKFE_SECRET, frame, real_len, &msg, &enowCounter)) { ... }
```

`hub_broadcast()` zählt in einem eigenen Zählerraum ab Startwert `100` (Hub-eigene
Menü-Aktionen), `relay_frame()` funkt das unveränderte Flipper-Frame samt dessen eigenem,
unabhängigen `s_flipperCounter`-Wert weiter (typischerweise weit höher nach längerem
Gebrauch der RF-Console). Beide Nachrichtentypen landen beim Satelliten jedoch im
**selben** `enowCounter`. Da `ukfe_rf_parse_frame()` jeden akzeptierten Counter als neuen
Mindeststand übernimmt (`*last_counter = m.counter`), sperrt ein einziger hoher
Flipper-Frame-Counter (relayt) ab sofort **jeden** nachfolgenden Hub-Broadcast, dessen
eigener, viel niedrigerer `s_txCounter`-Wert jetzt unter dem gemeinsamen Fenster liegt —
und umgekehrt: ein zwischenzeitlich hoher Hub-Broadcast kann relayte Flipper-Befehle
sperren, falls der Flipper seinerseits gerade neu gepaart wurde und wieder niedrig zählt.

Das ist ein eigenständiger Fund, unabhängig von den beiden bereits vorgeschlagenen
Neustart-Fixes (`rf-counter-replay-window-reset-on-reboot-fix.md` für `s_flipperCounter`,
`tx-broadcast-counter-restart-lockout-fix.md` für `s_txCounter`): er tritt **auch ohne
jeden Geräte-Neustart** auf, reine Verwechslung zweier unabhängiger Zählerräume in einem
gemeinsamen Fenster. Betrifft **drei** Repos: `heltec-v4-hub` (Sender, beide Funktionen),
`heltec-ukfe-rx` und `lilygo-ukfe-rx` (Empfänger, jeweils dieselbe `enowCounter`-Stelle).
Kein `Preferences`/NVS-Bezug — reiner RAM-Logikfehler, der einen dritten Fix-Stil
braucht (Quellentrennung statt Persistenz).

## Nutzen

Verhindert, dass Hub-Menü-Aktionen (Status-Broadcast) und vom Flipper über den Hub
weitergeleitete Befehle sich gegenseitig blockieren können, ohne dass dafür ein
Neustart eines der beteiligten Geräte nötig wäre. Reine Verfügbarkeits-Verbesserung
(kein Sicherheitsgewinn, siehe Risiko) im bereits produktiv genutzten Hub-Satelliten-Mesh.

## Betroffene Dateien

- `heltec-v4-hub/src/main.cpp` (`hub_broadcast()`, `relay_frame()`) — Patch:
  `espnow-origin-tag-counter-window-split-fix/heltec-v4-hub_main_cpp.patch`
- `heltec-ukfe-rx/src/main.cpp` (`onEspNowRecv()`, ESP-NOW-Empfangsblock in `loop()`) —
  Patch: `espnow-origin-tag-counter-window-split-fix/heltec-ukfe-rx_main_cpp.patch`
- `lilygo-ukfe-rx/src/main.cpp` (`onEspNowRecv()`, ESP-NOW-Empfangsblock in `loop()`) —
  Patch: `espnow-origin-tag-counter-window-split-fix/lilygo-ukfe-rx_main_cpp.patch`
- `ukfe_rf.c`/`ukfe_rf.h` (gemeinsames Protokoll, auf allen drei Geräten sowie dem Flipper
  identisch) bleiben **unverändert** — der Fix fügt ausschließlich ein zusätzliches
  Envelope-Byte außerhalb des MAC/CRC-geschützten `ukfe_rf`-Frames ein, bevor es per
  ESP-NOW gesendet wird. Die eigentliche Frame-Authentizität (keyed MAC + CRC16) bleibt
  vollständig Sache von `ukfe_rf_parse_frame()`, unangetastet.

## Fix-Skizze

Ein neues, 1 Byte großes Routing-Tag VOR dem eigentlichen `ukfe_rf`-Frame, ausschließlich
für die ESP-NOW-Übertragung (die 868-MHz-Funkstrecke Flipper↔Hub ist davon nicht
betroffen und bleibt unverändert):

```c
#define ESPNOW_ORIGIN_RELAY   0x00   // relay_frame(): unveraendert weitergefunkter Flipper-Frame
#define ESPNOW_ORIGIN_HUBCMD  0x01   // hub_broadcast(): eigener Hub-Befehl (Menue-Aktion)
```

**Hub** (`heltec-v4-hub`): `hub_broadcast()` und `relay_frame()` setzen vor dem
`esp_now_send(BROADCAST_MAC, ...)` jeweils ihr eigenes Tag voran (`env[0] = ORIGIN; memcpy
(env + 1, frame, len);`), senden `len + 1` Bytes statt `len`.

**Satelliten** (`heltec-ukfe-rx`, `lilygo-ukfe-rx`, identisches Muster in beiden): zwei
statt einem Fenster (`enowCounter` bleibt für `ESPNOW_ORIGIN_RELAY` zuständig, neu dazu
`enowBroadcastCounter` für `ESPNOW_ORIGIN_HUBCMD`). `onEspNowRecv()` liest `data[0]` als
Tag, verwirft unbekannte Werte sofort (bevor überhaupt geparst wird) und puffert nur noch
`data[1..len-1]` als eigentlichen Frame plus das Tag separat. Der ESP-NOW-Empfangsblock in
`loop()` wählt anhand des gepufferten Tags den passenden Fenster-Zeiger, bevor er
`ukfe_rf_parse_frame()` aufruft:

```c
uint32_t* win = (origin == ESPNOW_ORIGIN_HUBCMD) ? &enowBroadcastCounter : &enowCounter;
if(ukfe_rf_parse_frame(UKFE_SECRET, frame, real_len, &msg, win)) { ... }
```

Das Tag selbst ist unauthentifiziertes Routing-Metadatum (kein Teil von MAC/CRC) — das
ist bewusst so und unkritisch: ein Angreifer, der das Tag fälscht, kann damit höchstens
ein gültig signiertes Frame ins jeweils andere Fenster einsortieren, gewinnt dadurch aber
keine Fälschungsmöglichkeit, da `ukfe_rf_parse_frame()` MAC+CRC weiterhin unabhängig vom
Tag prüft (siehe Szenario 5 im Host-Repro). Patches sind unabhängig voneinander anwendbar,
müssen aber **gemeinsam** geflasht werden — ein Satellit mit altem, ungepatchtem Code
erwartet weiterhin das rohe `ukfe_rf`-Frame ohne Envelope-Byte und würde das neue
Tag-Byte als `LEN`-Feld fehlinterpretieren (Parse schlägt dann sauber über die
LEN/CRC-Prüfung fehl, kein Absturz, aber auch keine Funktion mehr) — und umgekehrt
interpretiert ein gepatchter Satellit mit einem ungepatchten Hub das erste Byte des
rohen Frames fälschlich als Tag. Vor dem Flashen also **alle drei** betroffenen Geräte
(Hub + beide Satelliten-Typen) mit demselben Patch-Stand versehen.

## Risiko

Verfügbarkeits-, kein Sicherheitsrisiko: die Frame-Authentizität (keyed MAC + CRC16,
`ukfe_rf.c` unverändert) ist von diesem Fund und Fix vollständig unberührt — ein
Angreifer ohne das gemeinsame Geheimnis gewinnt durch das neue Tag nichts. Das einzige
operative Risiko ist der oben genannte Versions-Mismatch beim Rollout (alle drei Geräte
müssen zusammen aktualisiert werden); solange das beachtet wird, ist der Patch
rückwirkungsfrei zum bestehenden Verhalten innerhalb jedes einzelnen Zählerraums
(Replay-Schutz pro Quelle bleibt exakt so strikt wie vorher, siehe Szenario 3 im Repro).

## Testhinweis

Lokal (PlatformIO, alle drei betroffenen Projekte):
1. `heltec-v4-hub`: Patch anwenden, bauen, auf den Heltec V4 flashen.
2. `heltec-ukfe-rx` und/oder `lilygo-ukfe-rx`: jeweiligen Patch anwenden, bauen, flashen.
3. Funktionstest wie in der vorigen Sitzung angelegt: vom Flipper einen Befehl senden
   (Flipper-Zählerraum deutlich über dem Hub-eigenen Stand, z. B. nach vielen
   RF-Console-Sitzungen), dann über die Hub-Menütaste (`PG_STATUS`/`PG_SATS`) einen
   Status-Broadcast auslösen — vor dem Fix bleibt die Hub-LED am Satelliten nach dem
   relayten Flipper-Befehl für den nächsten Status-Broadcast dunkel (ESPNOW PARSE FAIL
   im Serial-Log), nach dem Fix reagiert der Satellit auf beide Quellen unabhängig
   voneinander. Zusätzlich: Satellit mit noch ungepatchtem Code gegen den gepatchten Hub
   testen (sollte sauber "PARSE FAIL" loggen, kein Absturz) — bestätigt die oben
   beschriebene Versions-Mismatch-Erwartung, bevor alle drei Geräte synchron geflasht
   werden.
4. Reine Logik bereits hier per Host-Repro verifiziert (siehe unten) — nicht gegen die
   ESP32-S3-Toolchain kompiliert.

Host-Repro (plattformunabhängig, nutzt die unveränderten `ukfe_rf.c`/`.h` aus
`heltec-ukfe-rx`) in `repro-espnow-origin-counter-window-split/repro.c`:

```
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro repro.c ukfe_rf.c && ./repro
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro repro.c ukfe_rf.c && ./repro
```

Fünf Szenarien: (1) Ist-Zustand reproduziert den Cross-Source-Lockout (ein relayter
Flipper-Frame mit hohem Counter sperrt den nächsten, eigentlich gültigen Hub-Broadcast);
(2) Fix zeigt beide Zählerräume unabhängig voneinander funktionsfähig trotz desselben
Ablaufs; (3) Replay innerhalb eines Fensters bleibt weiterhin zuverlässig gesperrt; (4)
unbekanntes/zu kurzes Envelope-Byte wird verworfen, bevor überhaupt geparst wird, ohne
eines der beiden Fenster zu beeinflussen; (5) ein manipuliertes Frame mit gültigem
Envelope-Byte wird weiterhin über MAC/CRC abgelehnt, keine Fensterverschiebung. Unter
`-O0` und `-O1`: ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine
Compiler-Warnungen.
