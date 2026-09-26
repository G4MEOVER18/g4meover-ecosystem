# HID Mouse Jiggler (Stealth): Bewegung kann auf einer oder beiden Achsen 0 sein

## Was
Ich habe den nächsten der aus `8fdb70dd3` (PR #1112, "HID: improve Mouse Jiggler timers and
Bluetooth controls", `unlshd`-`dev`) zurückgestellten Teilfunde bearbeitet: "Stealth movement is
never zero on an axis". Betrifft `hid_mouse_jiggler_stealth_timer_callback()` in
`applications/system/hid_app/views/hid_mouse_jiggler_stealth.c`. Dieser Fund baut, wie bereits bei
der Zurückstellung vermerkt, sinnvoll auf dem zuvor vorgeschlagenen, bei mir noch nicht
angewendeten `hid-jiggler-stealth-move-bounds-fix.md` auf (der `move_x`/`move_y` von `int`
`-1000..1000` auf `int8_t` `-127..127` umstellt) — die beiden Patches sind daher **in dieser
Reihenfolge** anzuwenden: zuerst `hid-jiggler-stealth-move-bounds-fix`, danach dieser hier. Ohne
den ersten Patch fehlt `INT8_MAX` als Bezugsgröße, und die Datei hat an den betroffenen Zeilen
noch den `int -1000..1000`-Vorzustand.

## Fund: Bewegungswert kann pro Achse (und damit auch auf beiden zugleich) exakt 0 sein
```c
// HID mouse reports use signed 8-bit movement deltas
int8_t move_x = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;
int8_t move_y = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;

// Perform the mouse move with the randomized values
hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
```
`rand() % (2*INT8_MAX+1)` deckt gleichverteilt 255 Werte (`0..254`) ab, nach dem Versatz um
`-INT8_MAX` also `-127..127` — einschließlich `0`. Jede der beiden Achsen kann also unabhängig
exakt `0` ziehen; mit dem Host-Repro über den vollständigen Zwei-Achsen-Eingaberaum
(255 × 255 = 65025 mögliche Ziehungen) bestätigt: **1 von 65025 Ziehungen liefert `(0, 0)`** (die
Bewegung dieses Timer-Ticks bewirkt am Host überhaupt nichts), **508 von 65025 Ziehungen liefern
genau eine Achse als `0`** (die andere Achse bewegt sich, aber nur in einer Dimension). Ein an
Windows/macOS/Linux ankommender HID-Maus-Report mit Delta `(0, 0)` ist für den Host ein gültiges,
aber wirkungsloses Ereignis — kein Fehler, aber je nach Betriebssystem/Bildschirmsperre auch kein
Reset des Inaktivitäts-/Sperr-Timers, da manche Idle-Erkennungen speziell auf eine tatsächliche
Cursor-Positionsänderung statt auf den bloßen Eingang eines HID-Reports abstellen.

**Praktische Auswirkung:** Kein Speicherfehler, rein funktional. Der einzige Zweck des Stealth
Mouse Jigglers ist, durch periodische Mausbewegung eine Inaktivitätssperre des verbundenen Hosts
zu verhindern — bei `(0, 0)` bleibt genau dieser Timer-Tick wirkungslos, bei einer Ziehung mit nur
einer Achse `0` ist die Bewegung real, aber schwächer als beabsichtigt. Bei einem
Minimalintervall von 1 Minute (kleinstmöglicher `model->min_interval`) und einer
`1/65025`-Trefferquote für den Totalausfall ist ein einzelner Vorfall selten, aber bei
stundenlangem Betrieb (der erklärte Anwendungsfall einer Anti-Idle-Funktion) über viele Ticks
hinweg nicht vernachlässigbar — und der Fund ist unabhängig von der Trefferwahrscheinlichkeit
ohnehin unbeabsichtigtes Verhalten.

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten), reproduziert die exakten
ALT-Ausdrücke aus meiner Basis nach Anwendung von `hid-jiggler-stealth-move-bounds-fix` sowie die
NEU-Ausdrücke aus Unleashed `8fdb70dd3`s eigener
`hid_mouse_jiggler_stealth_random_move()`-Hilfsfunktion (verbatim übernommen):
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt, identisch unter `-O0` und `-O1`):
```
[group1] ALT: 1/255 single-axis draws (0.39%) are exactly 0
[group2] ALT: (0,0) reachable (1/65025 draws), one-axis-silent reachable (508/65025 draws)
[group3] NEU: 0/254 single-axis draws are 0, range [-127, 127]
[group4] NEU covers all 254 nonzero values in [-127,127] exactly once (bijection confirmed)
[group5] NEU: 0/64516 draws have any silent axis (both-zero and one-axis-zero both eliminated)
ALL CHECKS PASSED (520 checks)
```
Gruppe 1/2 überstreichen ALTs vollständigen Ein- bzw. Zwei-Achsen-Eingaberaum und bestätigen die
oben genannten Trefferzahlen exakt (nicht nur stichprobenartig). Gruppe 3 überstreicht NEUs
vollständigen Eingaberaum und bestätigt: kein einziger möglicher Zug liefert `0`, der volle
`±INT8_MAX`-Bereich bleibt aber erreichbar (kein neuer Wertebereichsfehler durch die
`+1`-Korrektur). Gruppe 4 bestätigt, dass NEU eine Bijektion auf seinem Eingaberaum ist — jeder
der 254 von Null verschiedenen Werte in `[-127, 127]` wird von genau einem Eingabewert erreicht,
die Verschiebung um `0` bündelt also keine zwei Werte künstlich zusammen. Gruppe 5 überstreicht
NEUs vollständigen Zwei-Achsen-Raum und bestätigt: **0 von 64516 Ziehungen** haben eine stille
Achse — der Fund ist vollständig beseitigt, nicht nur seltener gemacht. Unter
`gcc -std=c11 -O1/-O0 -Wall -Wextra -fsanitize=address,undefined`: keine ASan-/UBSan-Meldungen,
keine Compiler-Warnungen.

## Betroffene Dateien
- `applications/system/hid_app/views/hid_mouse_jiggler_stealth.c` —
  `hid_mouse_jiggler_stealth_timer_callback()` (Aufrufstelle, zwei Zeilen geändert) plus eine neue
  statische Hilfsfunktion `hid_mouse_jiggler_stealth_random_move()`. Setzt den bereits
  vorgeschlagenen `hid-jiggler-stealth-move-bounds-fix` voraus (siehe oben). Keine
  Signaturänderung, kein Einfluss auf Intervall-Berechnung, BLE-Status oder die übrigen, in
  separaten Sitzungen bereits bearbeiteten Teilfunde derselben Datei (Stop-Timer-Fix,
  Exit-Reset-Fix, Exit-Neubewaffnung-Race-Fix).

## Risiko
Sehr gering. Reine Logikänderung an einer bereits lokal berechneten, unmittelbar verwendeten
Zufallsgröße — keine geänderte Funktionssignatur, kein neuer Fehlerpfad, kein Einfluss auf
Locking/Timer-Handling (dort unverändert). Nicht 1:1 der volle Unleashed-Commit `8fdb70dd3`,
sondern bewusst nur die eine unabhängig portierbare Verbesserung daraus — die dortige
Restrukturierung (RNG/HID-Aufruf außerhalb des Model-Locks, `FuriTimerTypeOnce` statt
`FuriTimerTypePeriodic`) sowie der BLE-Warten-auf-Verbindung-Zusatz sind eigenständige, größere
Änderungen und bleiben bewusst als eigene Rückstellpunkte offen.

## Port-Skizze / Fix
```c
// HID mouse reports carry signed 8-bit deltas; skip 0 on each axis so every jiggle
// timer tick actually moves the pointer instead of occasionally reaching the host as a
// no-op HID report that does not reset its idle/lock timer.
static int8_t hid_mouse_jiggler_stealth_random_move(void) {
    const int8_t delta = (int8_t)(rand() % (2 * INT8_MAX) - INT8_MAX);
    return delta >= 0 ? delta + 1 : delta;
}
```
platziert direkt nach `HidMouseJigglerStealthModel` (vor `hid_mouse_jiggler_stealth_draw_callback`,
wie im Unleashed-Original). Aufrufstelle in
`hid_mouse_jiggler_stealth_timer_callback()`:
```c
// Never zero on either axis (see hid_mouse_jiggler_stealth_random_move())
int8_t move_x = hid_mouse_jiggler_stealth_random_move();
int8_t move_y = hid_mouse_jiggler_stealth_random_move();
```
1:1 Unleasheds Ausdruck für die Hilfsfunktion übernommen (`rand() % (2 * INT8_MAX) - INT8_MAX`,
dann `+1` verschieben, falls nicht-negativ) — nur die Aufrufstelle an meine (noch nicht auf den
vollen Commit restrukturierte) Callback-Form angepasst. Unified-Diff **gegen den Zustand nach
Anwendung von `hid-jiggler-stealth-move-bounds-fix`** (nicht gegen die unveränderte `mntm-012`-
Basis — siehe "Was" oben zur Anwendungsreihenfolge; `patch --dry-run -p1` gegen eine frische Kopie
mit bereits angewendetem Bounds-Fix vorab verifiziert, real angewendet und Byte-für-Byte gegen die
separat erstellte Fix-Fassung abgeglichen — identisch, zusätzlich die volle Kette
Pristine→Bounds-Fix→dieser Fix Byte-für-Byte gegen die beabsichtigte Endfassung geprüft) in
`firmware-mining/hid-jiggler-stealth-zero-movement-fix/hid_mouse_jiggler_stealth_c.patch`.

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-hid-jiggler-stealth-zero-movement-fix/repro.c`, host-kompilierbar, keine
Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: zuerst `hid-jiggler-stealth-move-bounds-fix`
anwenden, danach diesen Patch, `fbt` neu bauen und flashen. Da der Fund selten und nicht gezielt
auslösbar ist (Zufallsziehung), ist ein direkter Vorher/Nachher-Vergleich am Gerät unpraktisch —
der Host-Repro deckt den vollständigen Eingaberaum ab und ist der maßgebliche Nachweis. Am Gerät
reicht eine Regressionsprüfung: Stealth-Jiggler starten, Min/Max-Intervall auf 1 Minute setzen,
über mehrere Ticks hinweg beobachten, dass sich die Maus wie zuvor bewegt (kein Verhaltensbruch
durch die geänderte Berechnung) und die Bewegungsgröße weiterhin im erwarteten `±127`-Bereich
bleibt.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `8fdb70dd3` (PR #1112, "HID: improve Mouse
  Jiggler timers and Bluetooth controls"), `unlshd`-`dev` — nur die Teilverbesserung "Stealth
  movement is never zero on an axis" 1:1 übernommen.
- Voraussetzung: `firmware-mining/hid-jiggler-stealth-move-bounds-fix.md` (muss zuerst angewendet
  werden).
- Aus derselben `8fdb70dd3`-Restliste weiterhin offen: Jiggler wartet über BLE nicht auf die
  Verbindung vor dem Senden.
