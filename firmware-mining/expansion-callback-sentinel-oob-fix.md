# F7 HAL: Sentinel-Wert in furi_hal_serial_control_set_expansion_callback() nicht abgewiesen

## Was
Ich habe aus der bei der letzten Sitzung aus `unlshd-093` vorgemerkten Restliste (Fund
`subghz-encoder-upload-buffer-overflow` war der erste bearbeitete Kandidat aus derselben Reihe)
den nächsten Kandidaten geprüft: `4b3c614c8` (PR #1108, "F7: reject sentinel for expansion
callbacks"), betrifft `furi_hal_serial_control_set_expansion_callback()` in
`targets/f7/furi_hal/furi_hal_serial_control.c`. Datei bei mir (`mntm-012`) byteidentisch zum
Unleashed-Vorher-Stand verifiziert (direkter Diff gegen eine frische `mntm-012`-Kopie).

## Fund: Off-by-eins in der Bound-Prüfung erlaubt den Sentinel-Wert selbst
```c
void furi_hal_serial_control_set_expansion_callback(
    FuriHalSerialId serial_id,
    FuriHalSerialControlExpansionCallback callback,
    void* context) {
    furi_check(serial_id <= FuriHalSerialIdMax);   // <-- lässt FuriHalSerialIdMax selbst durch
    furi_check(furi_hal_serial_control);
    ...
}
```
`FuriHalSerialId` ist als `{ FuriHalSerialIdUsart, FuriHalSerialIdLpuart, FuriHalSerialIdMax }`
definiert — `FuriHalSerialIdMax` ist ein reiner Sentinel-Wert ("eins hinter dem letzten echten
UART"), keine gültige Kanal-ID. Die interne `FuriHalSerialControl`-Struct hält dazu passend
`FuriHalSerialHandle handles[FuriHalSerialIdMax]` — ein Array mit genau zwei gültigen Indizes
(0, 1). Der Nachrichten-Handler, der diese Prüfung absichert, indiziert aber ungeprüft direkt mit
der übergebenen ID:
```c
static bool furi_hal_serial_control_handler_expansion_set_callback(void* input, void* output) {
    ...
    FuriHalSerialHandle* handle = &furi_hal_serial_control->handles[message_input->id];
    ...
    furi_hal_serial_control_enable_expansion_irq(handle, enable_irq);   // schreibt über handle
    ...
}
```
Mit `serial_id == FuriHalSerialIdMax` liefert das die Adresse `&handles[FuriHalSerialIdMax]` —
ein Element hinter dem Array-Ende. Da `handles[]` kein eigenständiger Heap-Block ist, sondern nur
das erste Feld einer größeren Struct, führt dieser Zugriff nicht zu einem Absturz/Fault, sondern
schreibt still in das direkt danach liegende Feld: `FuriMessageQueue* queue` — den
Nachrichten-Queue-Pointer, von dem der Serial-Control-Worker-Thread selbst abhängt, um überhaupt
weitere Nachrichten zu empfangen. Eine im Kommentar sichtbare, bewusst andere Nachbarfunktion
(die Logging-ID-Prüfung in derselben Datei, Zeile 355) behält ihr `serial_id <= FuriHalSerialIdMax`
korrekt bei — dort *ist* `FuriHalSerialIdMax` ein gültiger Eingabewert
("Use FuriHalSerialIdMax to disable logging", siehe Header-Doku). Der Expansion-Callback-Setter
hat aber keine "Max deaktiviert"-Semantik — deaktiviert wird ausschließlich über
`callback == NULL` — die `<=`-Prüfung hier ist ein Copy-Paste-Fehler aus genau dieser
Nachbarfunktion, nicht eine bewusste Design-Entscheidung.

**Praktische Erreichbarkeit:** Die vier Aufrufstellen im eigenen `expansion`-Dienst
(`applications/services/expansion/expansion.c`) übergeben ausschließlich bereits vorab gegen
`< FuriHalSerialIdMax` geprüfte `instance->serial_id`-Werte — über den Stock-Dienst selbst ist der
Sentinel-Wert also nicht erreichbar. `furi_hal_serial_control_set_expansion_callback()` ist aber
eine öffentliche, im SDK-Header exportierte HAL-Funktion (`furi_hal_serial_control.h`) — jede
eigene oder Dritt-App, die (wie meine bereits geprüften Erweiterungsport-Apps Rogue AP Detector/
BLE Scanner/Rayhunter Client/UART Sniff) einen Expansion-Callback direkt registriert und dabei
versehentlich `FuriHalSerialIdMax` statt einer echten Kanal-ID übergibt, löst die Korrumpierung
aus. Das ist ein API-Vertragsfehler (defensive Absicherung gegen Aufrufer-Fehlgebrauch), keine
über Funkinhalt/Kartendaten von außen auslösbare Lücke — anders eingeordnet als etwa der bereits
gelieferte `subghz-encoder-upload-buffer-overflow`-Fund aus derselben `unlshd-093`-Reihe.

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten), rekonstruiert exakt die
Feldreihenfolge/-größen der echten `FuriHalSerialControl`-Struct bis einschließlich `queue`, sowie
ALT- (`<=`) und NEU-Variante (`<`) der Prüfung selbst:
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt):
```
offsetof(handles)=0 offsetof(queue)=16 sizeof(handles)=16 (queue directly follows handles[]: yes)

regression: serial_id=0
  [ok] ALT accepts real serial id
  [ok] NEU accepts real serial id (unchanged behavior)
  [ok] ALT sets handle correctly
  [ok] NEU sets handle correctly
  [ok] ALT: queue pointer untouched by an in-range id
  [ok] NEU: queue pointer untouched by an in-range id
regression: serial_id=1
  [ok] ALT accepts real serial id
  [ok] NEU accepts real serial id (unchanged behavior)
  [ok] ALT sets handle correctly
  [ok] NEU sets handle correctly
  [ok] ALT: queue pointer untouched by an in-range id
  [ok] NEU: queue pointer untouched by an in-range id

BUG case: serial_id=FuriHalSerialIdMax (2)
  furi_check failed: serial_id < FuriHalSerialIdMax
  [ok] ALT: furi_check does NOT reject the sentinel (bug: proceeds to write)
  [ok] NEU: furi_check rejects the sentinel (process aborts BEFORE any OOB write)
  ALT: fc->queue before=0x5610a1ee5560 after=0x560100000002 (CORRUPTED)
  [ok] ALT corrupts the live queue pointer via the OOB handles[FuriHalSerialIdMax] write
  NEU: fc->queue before=0x5610a1ee5520 after=0x5610a1ee5520 (unchanged)
  [ok] NEU never touches queue -- rejected up front

ALL CHECKS PASSED
```
`offsetof(queue) == sizeof(handles)` bestätigt zuerst, dass `queue` in dieser Struct-Reihenfolge
tatsächlich lückenlos direkt auf `handles[]` folgt (keine Compiler-Padding-Überraschung). Die
beiden Regressionsfälle (echte IDs 0/1) zeigen: ALT und NEU verhalten sich für jeden bisherigen,
gültigen Aufrufer identisch, `queue` bleibt in beiden Fällen unangetastet. Der BUG-Fall zeigt den
Fund konkret: ALT lässt den Sentinel durch die Prüfung, und der anschließende, unveränderte
Handler-Zugriff überschreibt `queue` sichtbar mit Datenmüll (die unteren Bytes tragen erkennbar
die geschriebenen `id`/`in_use`-Werte); NEU weist denselben Aufruf per `furi_check` ab, bevor
`queue` je berührt wird — im forked, isolierten Kindprozess nachgewiesen (Elternprozess bleibt am
Leben, Exit-Code des Kindes zeigt den Abbruch).

## Betroffene Dateien
- `targets/f7/furi_hal/furi_hal_serial_control.c` — `furi_hal_serial_control_set_expansion_callback()`,
  Zeile 380, einzige Fix-Stelle. Keine Signatur-/Aufrufstellenänderung nötig, kein Einfluss auf die
  bewusst unveränderte `<=`-Prüfung der Logging-ID-Funktion (Zeile 355) oder auf sonstigen Code in
  dieser Datei.

## Risiko
Sehr gering für alle bestehenden Aufrufer (siehe Regressionsfälle im Host-Repro): der Fix ändert
eine einzelne Vergleichsoperation in einer bereits vorhandenen `furi_check`-Bedingung, kein neuer
Fehlerpfad, keine geänderte Semantik für jeden Aufruf mit einer echten Kanal-ID. Für den
Sentinel-Fall selbst ist die Verhaltensänderung beabsichtigt: statt eines stillen, für den
Aufrufer unsichtbaren Nachrichten-Queue-Pointer-Korrumpierens (mit unklaren, verzögerten
Folgefehlern im Serial-Control-Worker-Thread) löst der `furi_check` einen sofortigen, klar
diagnostizierbaren Abbruch aus — exakt das erwartete `furi_check`-Verhalten bei Verletzung eines
API-Vertrags, wie es auch die Nachbarfunktion für ihre eigene Prüfung bereits zeigt.

## Port-Skizze / Fix
```c
void furi_hal_serial_control_set_expansion_callback(
    FuriHalSerialId serial_id,
    FuriHalSerialControlExpansionCallback callback,
    void* context) {
    furi_check(serial_id < FuriHalSerialIdMax);
    furi_check(furi_hal_serial_control);
    ...
}
```
Einziger Unterschied zum Original: `<=` → `<`, 1:1 Unleasheds Fix übernommen (identischer
Ein-Zeichen-Diff). Unified-Diff gegen meine unveränderte `mntm-012`-Basis in
`firmware-mining/expansion-callback-sentinel-oob-fix/furi_hal_serial_control_c.patch`
(`patch --dry-run -p1` gegen eine frische Basiskopie vorab verifiziert, real angewendet und
Byte-für-Byte gegen die separat editierte Fassung abgeglichen — identisch).

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-expansion-callback-sentinel-oob-fix/repro.c`, host-kompilierbar, keine
Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Patch anwenden, `fbt` neu bauen und flashen,
danach ein reguläres Erweiterungsmodul (z. B. WiFi-Devboard/ESP32-Marauder) in einer der bereits
geprüften Erweiterungsport-Apps (Rogue AP Detector, BLE Scanner, Rayhunter Client, UART Sniff)
normal verbinden/trennen — Verhalten muss unverändert bleiben (Regressionsfall). Ein gezielter
Test des Sentinel-Falls selbst braucht einen absichtlich fehlerhaften Aufruf (z. B. testweise
`furi_hal_serial_control_set_expansion_callback(FuriHalSerialIdMax, cb, ctx)` aus einer eigenen
Test-App) — vor dem Fix bleibt das App-seitig unbemerkt, bis der Serial-Control-Worker-Thread den
korrumpierten Queue-Pointer verwendet; nach dem Fix meldet `furi_check` den Fehlaufruf sofort und
eindeutig.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `4b3c614c8` (PR #1108, "F7: reject
  sentinel for expansion callbacks"), Tag `unlshd-093`.
- API-87-Einordnung: reine F7-HAL-Datei, bei mir (`mntm-012`) byteidentisch zum
  Unleashed-Vorher-Stand vorhanden — kein Kompatibilitätsrisiko, 1:1 portierbar.
- Eigener Zusatzbefund (Erreichbarkeits-/Risikoeinordnung): die vier Aufrufstellen im eigenen
  `expansion`-Dienst sind bereits vorab abgesichert, die Lücke ist ausschließlich über die
  öffentliche HAL-Funktion selbst erreichbar (siehe "Praktische Erreichbarkeit" oben) — mit
  Host-Repro bestätigt.
- Aus derselben `unlshd-093`-Restliste weiterhin offen: `51a56fc40` (FeliCa-Lite-Dateilade-Bounds,
  #1106), `c742dfe6c` (Stealth-Jiggler-Bounds, #1111), `cc3a1bc79` (CUID-Dictionary-Zähler, #1115),
  `95c35fb82` (MIFARE-Classic-Schlüssel-Dictionary-Feature, #1118), `104d96f86`
  (Superrollo/GW60-SubGHz-Protokoll, #1068).
