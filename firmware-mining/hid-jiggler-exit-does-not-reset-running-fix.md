# HID Mouse Jiggler: Verlassen der View stoppt den Timer, lässt aber `model->running` stehen

## Was
Ich habe den nächsten offenen Teilfund aus dem bei einer früheren Firmware-Mining-Sitzung
zurückgestellten "restlichen Umfang von `8fdb70dd3`" (PR #1112, "HID: improve Mouse Jiggler
timers and Bluetooth controls") aufgegriffen: `hid_mouse_jiggler_exit_callback()` in
`applications/system/hid_app/views/hid_mouse_jiggler.c` (nicht-stealth Mouse Jiggler). Die
betroffene Datei ist bei mir (`mntm-012`) byteidentisch zum Unleashed-Vorher-Stand (vollständig
Zeile für Zeile gegengeprüft). Unabhängig vom bereits gelieferten Fix in
`hid-jiggler-stop-does-not-stop-timer-fix.md` (der betrifft `hid_mouse_jiggler_input_callback()`
und den OK-Tasten-Übergang) — beide Patches berühren unterschiedliche Funktionen in derselben
Datei und lassen sich unabhängig voneinander anwenden.

## Fund: Exit räumt den Timer, aber nicht das Anzeige-Flag
```c
static void hid_mouse_jiggler_exit_callback(void* context) {
    furi_assert(context);
    HidMouseJiggler* hid_mouse_jiggler = context;
    furi_timer_stop(hid_mouse_jiggler->timer);
}
```
`hid_mouse_jiggler_exit_callback()` wird aufgerufen, wenn die View verlassen wird (Back/"Quit"
aus dem Katalog dieser View, siehe Zeichnung in `hid_mouse_jiggler_draw_callback()`). Sie stoppt
korrekt den `FuriTimer`, fasst aber `model->running` nicht an. Da das View-Model die gesamte
App-Laufzeit über bestehen bleibt (erst `hid_mouse_jiggler_free()` beim App-Beenden gibt es frei,
nicht das Verlassen einer einzelnen View), bleibt `model->running == true` stehen, wenn beim
Verlassen gerade gejiggelt wurde.

**Praktische Auswirkung:** Betrete ich die View danach erneut (z. B. über das HID-Menü
zurücknavigiert), zeichnet `hid_mouse_jiggler_draw_callback()` sofort wieder die hervorgehobene
"Stop"-Box (`if(model->running) { elements_slightly_rounded_box(...); ... "Stop" ... }`) — obwohl
der Timer durch den Exit bereits sauber gestoppt wurde und tatsächlich nichts läuft. Drücke ich
darauf einmal OK, weil die Anzeige "Stop" suggeriert, dass ich gerade aktiv jiggle und ich es
beenden will, kippt der Toggle `model->running` lediglich von `true` auf `false` (ich hatte ja gar
nicht neu gestartet) — die Anzeige springt auf "Start", aber ich wollte eigentlich jiggeln. Erst
ein zweiter OK-Druck startet tatsächlich. Kein Speicherzugriffsfehler, reiner
Anzeige-/Bedienungsfehler, reproduzierbar bei jedem "View verlassen, während gerade gejiggelt
wird" ohne Sonderfall.

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten), bildet den OK-Toggle
(`ok_press()`, bereits inklusive des unbedingten `furi_timer_stop()` aus dem separat gelieferten
Stop-Fix, damit beide Funde nicht widersprüchlich wirken) sowie ALT-/NEU-Fassung von
`hid_mouse_jiggler_exit_callback()` nach — ein Mock-Timer mit `armed`-Flag plus Aufrufzählern
ersetzt die opaken `furi_timer_*`-HAL-Aufrufe, der Fund liegt ausschließlich darin, dass Exit ein
zweites Flag anfassen muss:
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt, identisch unter `-O0` gegengeprüft):
```
[group1] ALT: after Quit while running, timer.armed=false but model.running=true (re-entering the view misleadingly shows "Stop")
[group2] NEU: after Quit while running, timer.armed=false and model.running=false (re-entering the view correctly shows "Start")
[group3] one OK press after re-entry: ALT running=false armed=false (needs a second press), NEU running=true armed=true (jiggling as the user intended)
[group4] exit while not running: ALT and NEU agree (running=false, armed=false)
[group5] regression over 5000 start/quit cycles: NEU mismatches=0 (expected 0)
ALL CHECKS PASSED (5 checks)
```

## Betroffene Dateien
- `applications/system/hid_app/views/hid_mouse_jiggler.c` — nur `hid_mouse_jiggler_exit_callback()`.

## Risiko
Minimal. Der Fix ergänzt in `hid_mouse_jiggler_exit_callback()` lediglich einen zusätzlichen
`with_view_model(...)`-Block, der `model->running = false` setzt — dasselbe Zugriffsmuster, das
der Rest der Datei (z. B. `hid_mouse_jiggler_set_connected_status()`) bereits für einzelne
Feld-Updates verwendet, `true` am Ende erzwingt wie dort einen Redraw. Kein neues
Nebenläufigkeitsmuster, keine Änderung an `hid_mouse_jiggler_input_callback()` oder am
Timer-Callback selbst. Exit während `model->running` bereits `false` ist bleibt unverändert
folgenlos (Host-Repro Gruppe 4).

## Port-Skizze
```c
static void hid_mouse_jiggler_exit_callback(void* context) {
    furi_assert(context);
    HidMouseJiggler* hid_mouse_jiggler = context;
    furi_timer_stop(hid_mouse_jiggler->timer);
    with_view_model(
        hid_mouse_jiggler->view, HidMouseJigglerModel * model, { model->running = false; }, true);
}
```
Unified-Diff gegen die unveränderte `mntm-012`-Basis in
`hid-jiggler-exit-does-not-reset-running-fix/hid_mouse_jiggler_c.patch` (`patch --dry-run -p1`
vorab gegen eine frische Kopie verifiziert, real angewendet und Byte-für-Byte gegen die separat
erstellte Fix-Fassung abgeglichen — identisch).

## Testhinweis (lokal)
Patch anwenden, `fbt`/`ufbt` neu bauen und flashen. Mouse Jiggler öffnen, Start drücken (damit
gerade gejiggelt wird), über Back/"Quit" die View verlassen, danach im HID-Menü erneut in Mouse
Jiggler wechseln: vor dem Fix zeigt die View sofort wieder "Stop" hervorgehoben an, ein einzelner
OK-Druck schaltet nur auf "Start" um statt zu starten; nach dem Fix zeigt die View korrekt
"Start", ein OK-Druck startet direkt. Gegenprobe: View verlassen, während nicht gejiggelt wird —
Verhalten in beiden Fassungen unverändert (weiterhin "Start" beim erneuten Betreten).

## Quelle
Restlicher Teilfund aus `8fdb70dd3` (PR #1112, "HID: improve Mouse Jiggler timers and Bluetooth
controls", `DarkFlippers/unleashed-firmware`), unverändert nicht in `mntm-012` übernommen.
