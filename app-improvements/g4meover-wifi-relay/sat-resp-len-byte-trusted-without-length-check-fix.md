# g4meover-wifi-relay — ESP-NOW-Antwortpfad liest bis zu 54 Bytes hinter dem tatsächlich empfangenen Puffer

## Was
Ich habe den bereits dokumentierten Satelliten-Antwortpfad
(`sat-resp-ack-replay-missing-counter-check-fix.md`, per-MAC-Anti-Replay-Fenster) noch einmal
genauer angesehen, weil mich interessiert hat, wie `in_len` für
`ukfe_rf_parse_frame()` dort eigentlich zustande kommt — und dabei einen zweiten, unabhängigen
Fund gemacht, der auch in meinem eigenen, schon korrigierten Zwischenstand noch steckt:

```cpp
static void on_espnow_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len <= 0 || len > (int)UKFE_RF_MAX_FRAME) return;
    UkfeRfMessage msg;
    if (ukfe_rf_parse_frame(RELAY_SECRET, data, (size_t)data[0] + 1, &msg, sat_counter_slot(mac))) {
        ...
```

`len` ist die vom ESP-NOW-Treiber gelieferte, tatsächlich empfangene Länge — die einzige Länge,
für die `data` überhaupt gültig ist. Die an `ukfe_rf_parse_frame()` übergebene Länge wird aber
**nicht** aus `len` gebildet, sondern ausschließlich aus `data[0]` — dem LEN-Byte, das Teil der
vom Absender gesendeten (und damit nicht vertrauenswürdigen) Nutzlast ist. Beide Werte werden
nirgends gegeneinander geprüft.

`ukfe_rf_parse_frame()` selbst verlässt sich für seine interne Konsistenzprüfung ebenfalls nur
auf das übergebene `in_len`:

```c
uint8_t len = in[0];
if((size_t)len + 1 != in_len) return false;   // in_len kommt hier bereits von data[0] selbst
```

Das ist tautologisch, sobald der Aufrufer `in_len` schon aus `data[0]` gebildet hat — die
Prüfung kann dadurch nie greifen. Im Anschluss liest `ukfe_rf_parse_frame()` `in[1]`, `in[2]`,
`in[8]`, die Argumentbytes und schließlich MAC/CRC bis zu Offset `in_len - 1`, als wäre der
komplette, bis zu `UKFE_RF_MAX_FRAME` (55) Byte große Puffer tatsächlich zugestellt worden —
unabhängig davon, wie kurz `len` in Wirklichkeit war. Ein ESP-NOW-Frame von nur **einem** Byte
mit dem Inhalt `0x36` (54 → "LEN=54") reicht bereits, um `ukfe_rf_parse_frame()` zu einem Lesen
weit hinter der tatsächlichen Empfangspufferlänge zu bewegen.

Das trifft nicht nur auf absichtlich präparierte Frames zu: dieselbe Lücke öffnet sich bei jeder
echten Satelliten-Antwort, die durch Funkstörung/Kollision verkürzt beim Relay ankommt (`len` <
dem, was `data[0]` behauptet) — der bislang einzige Schutz dagegen war Zufall (dass die
verbleibenden Nutzdaten hinter dem echten Frameende zufällig als MAC/CRC-Mismatch auffallen,
bevor ein Speicherzugriffsfehler auftritt).

Der Flipper→Relay-Pfad (`scan_and_relay()`) hat genau dasselbe Muster (`real_len` aus
`s_buf[0]`), prüft aber vor dem Parsen explizit `if (s_len < real_len) return;` — der
ESP-NOW-Antwortpfad hat diese Prüfung nie bekommen.

## Nutzen
- Schließt einen Lesezugriff außerhalb der gültigen Pufferlänge (bis zu `UKFE_RF_MAX_FRAME - 1`
  = 54 Byte über das tatsächlich von ESP-NOW zugestellte Ende hinaus) im Antwortpfad — passt in
  den defensiven Rahmen aus `CONSTRAINTS.md` Punkt 5: das eigene Pentest-Werkzeug soll selbst
  keinen durch kurze/präparierte Frames auslösbaren Speicherfehler enthalten.
- Repariert nebenbei auch die reine Robustheit gegenüber echten, durch Funkstörung verkürzten
  Antwort-Frames (bisher: Zufallsverhalten je nach Speicherinhalt hinter dem Puffer; danach:
  definiert und sicher verworfen, `s_satRejected` steigt).
- Bringt den Antwortpfad auf dasselbe Sicherheitsniveau wie der bereits korrekt
  längengeprüfte Flipper→Relay-Pfad (`scan_and_relay()`).

## Betroffene Dateien
- `src/main.cpp`: `on_espnow_recv()` — ein zusätzlicher Längenabgleich vor dem
  `ukfe_rf_parse_frame()`-Aufruf, der jetzt die tatsächlich empfangene Länge (`len`) statt der
  aus `data[0]` abgeleiteten Länge übergibt.
- `src/ukfe_rf.c`/`.h` unverändert — das Wire-Format und `ukfe_rf_parse_frame()` selbst sind
  korrekt, sobald der Aufrufer ihm eine tatsächlich gültige `in_len` übergibt.

## Patch
`sat-resp-len-byte-trusted-without-length-check-fix/main_cpp.patch` — baut auf dem bereits
dokumentierten `sat-resp-ack-replay-missing-counter-check-fix/main_cpp.patch` auf (dort wurde
`on_espnow_recv()` zuletzt geändert; dieser Patch geht von diesem Zwischenstand aus, nicht vom
unveränderten Repo-Original). Zusätzlich liegt `main_cpp.target` als fertige Zieldatei bei (`src/main.cpp`
nach Anwendung beider Patches in Folge) zum direkten Ersetzen, falls die Reihenfolge lokal nicht
mehr nachvollzogen werden soll.

```diff
     if (len <= 0 || len > (int)UKFE_RF_MAX_FRAME) return;
+    if ((size_t)data[0] + 1 != (size_t)len) return;
     UkfeRfMessage msg;
     ...
-    if (ukfe_rf_parse_frame(RELAY_SECRET, data, (size_t)data[0] + 1, &msg, sat_counter_slot(mac))) {
+    if (ukfe_rf_parse_frame(RELAY_SECRET, data, (size_t)len, &msg, sat_counter_slot(mac))) {
```

Angewendet (und gegen eine unabhängige Zweitkopie desselben Zwischenstands erneut angewendet)
per `patch -p1`; beide Ergebnisse sind laut `md5sum` byte-identisch zur beigelegten
`main_cpp.target`.

## Host-Repro
`repro-sat-resp-len-byte-oob-read/repro.c` bindet die unveränderte `ukfe_rf.c`/`.h` (Byte-für-
Byte-Kopie aus dem Repo) ein und reproduziert exakt die `on_espnow_recv()`-Zeile vor und nach
dem Fix, ohne deren Semantik nachzubilden:
- Ein 1-Byte-"ESP-NOW-Frame" mit gelogenem LEN-Byte (behauptet 55 Byte Nutzlast) wird in einem
  isolierten Kindprozess gegen den unveränderten Codepfad ausgeführt. Gebaut mit
  `gcc -std=c17 -O0`/`-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined`:
  AddressSanitizer meldet in beiden Optimierungsstufen einen `heap-buffer-overflow`-Lesezugriff
  in `ukfe_rf_parse_frame()` (zuerst bei `in[1]`, in einer zweiten Variante bei `in[8]`), der
  Kindprozess bricht per `SIGABRT` ab.
- Derselbe Puffer gegen den reparierten Codepfad: sicher abgelehnt, kein Zugriff auf `data[1..]`.
- Ein echtes, vollständig signiertes 0-Argument-Antwort-Frame, um 1 Byte gestutzt zugestellt
  (simuliert ein durch Funkstörung verkürztes ESP-NOW-Paket): löst denselben Sanitizer-Abbruch
  aus wie der absichtlich präparierte Fall — der Fund betrifft nicht nur vorsätzliche
  Präparation.
- Regression: ein echtes, vollständig zugestelltes Antwort-Frame (0 Argumente sowie mit
  Argumenten, `ALEN > 0`) wird nach dem Fix weiterhin korrekt angenommen.
- Ergebnis: 7/7 Prüfungen bestanden, identischer Output bei `-O0` und `-O1`, keine
  Compiler-Warnungen (`-Wall -Wextra`), keine UBSan-Befunde.

## Testhinweis (lokal)
Patch gegen die eigene `src/main.cpp`-Kopie anwenden (oder `main_cpp.target` einsetzen), neu
flashen. Am PC/Pi mit `esptool`/eigenem ESP-NOW-Sender ein 1-Byte-Testframe wie im Repro an die
Relay-MAC senden (z. B. per zweitem ESP32 mit `esp_now_send()`) — vor dem Fix ist das Verhalten
plattformabhängig undefiniert (potenziell Absturz/Neustart des Relays je nach tatsächlichem
Speicherinhalt hinter dem Empfangspuffer), nach dem Fix bleibt `resp<-Sat` unverändert und
`resp rejected` (aus dem vorherigen Fix) steigt um eins, kein Absturz. Danach eine echte
Satelliten-Antwort (`lilygo-ukfe-rx`/`heltec-ukfe-rx`, normaler Betrieb) gegenprüfen: muss
weiterhin unverändert durchgereicht werden (`resp<-Sat` steigt, Frame kommt unverändert am
Flipper-UART an).

## Risiko
- Niedrig: additiv, eine zusätzliche frühe Rückkehr in `on_espnow_recv()`, ein geänderter
  Parameter im bestehenden `ukfe_rf_parse_frame()`-Aufruf (`len` statt `data[0] + 1`). Wire-
  Format, MAC/CRC, Anti-Replay-Logik und der bereits dokumentierte per-MAC-Zählerfix sind
  unverändert.
- Verändert nicht die Flipper→Relay-Richtung (`scan_and_relay()`), die bereits korrekt
  längengeprüft ist, und nicht `relay_frame()`.
- Verwirft künftig zusätzlich echte, aber durch Funkstörung verkürzt zugestellte
  Antwort-Frames (vorher: zufallsabhängiges Verhalten) — das ist beabsichtigt und deckungsgleich
  mit dem bereits etablierten Verwerfungsverhalten bei MAC/CRC-Mismatch.

## Quelle
- `G4MEOVER18/g4meover-wifi-relay` (main, Commit `9b4a9e1`): `src/main.cpp`
  (`on_espnow_recv()`), `src/ukfe_rf.c`/`.h` (`ukfe_rf_parse_frame()`).
- `app-improvements/g4meover-wifi-relay/sat-resp-ack-replay-missing-counter-check-fix.md`:
  dokumentiert den unmittelbar vorangehenden Zwischenstand von `on_espnow_recv()`, auf dem
  dieser Fund und Patch aufbauen.
