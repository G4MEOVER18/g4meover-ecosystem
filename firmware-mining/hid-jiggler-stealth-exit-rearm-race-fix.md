# HID Mouse Jiggler (Stealth): Verlassen der View kann eine bereits laufende Neubewaffnung nicht mehr einholen

## Was
Ich habe den nächsten offenen Teilfund aus dem bei einer früheren Firmware-Mining-Sitzung
zurückgestellten "restlichen Umfang von `8fdb70dd3`" (PR #1112, "HID: improve Mouse Jiggler
timers and Bluetooth controls") aufgegriffen: `hid_mouse_jiggler_stealth_exit_callback()` in
`applications/system/hid_app/views/hid_mouse_jiggler_stealth.c`. Die betroffene Datei ist bei mir
(`mntm-012`) in den relevanten Zeilen byteidentisch zum Unleashed-Vorher-Stand (vollständig Zeile
für Zeile gegengeprüft). Unabhängig von den beiden bereits gelieferten Fixes für die
*nicht*-stealth Variante (`hid-jiggler-stop-does-not-stop-timer-fix.md`,
`hid-jiggler-exit-does-not-reset-running-fix.md`) — eigenständige Datei, eigener View-Zustand,
kein gemeinsamer Code. Aus demselben `8fdb70dd3`-Restumfang weiterhin offen und bewusst nicht
mitbearbeitet: "Stealth-Bewegung kann 0 sein" und "Jiggler wartet über BLE nicht auf die
Verbindung" — beide unabhängig von diesem Fund, für künftige Sitzungen vorgemerkt.

## Fund: Exit synchronisiert überhaupt nicht mit der Selbst-Neubewaffnung des Timers
```c
static void hid_mouse_jiggler_stealth_timer_callback(void* context) {
    furi_assert(context);
    HidMouseJigglerStealth* hid_mouse_jiggler = context;
    with_view_model(
        hid_mouse_jiggler->view,
        HidMouseJigglerStealthModel * model,
        {
            if(model->running) {
                int randomIntervalMinutes =
                    model->min_interval + rand() % (model->max_interval - model->min_interval + 1);
                int move_x = (rand() % 2001) - 1000;
                int move_y = (rand() % 2001) - 1000;
                hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
                furi_timer_stop(hid_mouse_jiggler->timer);
                furi_timer_start(hid_mouse_jiggler->timer, randomIntervalMinutes * 60000);
            }
        },
        false);
}

static void hid_mouse_jiggler_stealth_exit_callback(void* context) {
    furi_assert(context);
    HidMouseJigglerStealth* hid_mouse_jiggler = context;
    furi_timer_stop(hid_mouse_jiggler->timer);
}
```
Anders als bei der nicht-stealth Variante ist das hier kein reiner Anzeigefehler, sondern eine
echte Race Condition mit sichtbarer Auswirkung: die Stealth-Variante randomisiert das Intervall
bei jedem Feuern neu, deshalb ruft der Timer-Callback selbst `furi_timer_stop()` +
`furi_timer_start()` auf, um sich mit dem neuen Intervall neu zu bewaffnen — und tut das
**innerhalb** desselben `with_view_model`-Locks, der auch `model->running` schützt.
`hid_mouse_jiggler_stealth_exit_callback()` nimmt diesen Lock beim Verlassen der View jedoch gar
nicht — sie ruft `furi_timer_stop()` direkt und ungeschützt auf.

Der Model-Lock serialisiert nur Zugriffe auf Model-Felder; er garantiert nichts über die
Reihenfolge der `furi_timer_stop()`/`furi_timer_start()`-Aufrufe selbst gegenüber einem Aufrufer,
der diesen Lock gar nicht nimmt. Feuert der Timer gerade (Timer-Service-Task ist mitten im
Callback, hat `model->running == true` bereits gelesen und ist auf dem Weg zu seinem eigenen
Stop+Start-Paar), kann die UI-Task in genau diesem Moment `hid_mouse_jiggler_stealth_exit_callback()`
ausführen (Back/"Quit"-Taste) und ihren `furi_timer_stop()`-Aufruf zwischen den Callback-internen
Stop- und Start-Aufruf schieben (oder noch davor) — ungehindert, da nichts diese beiden
Aufrufsequenzen gegeneinander ordnet. In beiden Fällen gewinnt der Callback-interne
`furi_timer_start()`, der zeitlich danach kommt: der Timer bleibt bewaffnet, obwohl die View
gerade verlassen wurde. **Praktische Auswirkung:** der Stealth-Jiggler bewegt die Maus im
Hintergrund weiter — ganz ohne UI, ohne Anzeige, die das erkennen ließe (die View ist ja bereits
verlassen) — bis entweder die App komplett beendet wird oder die View erneut betreten und der
Toggle zweimal gedrückt wird (einmal um den fälschlich noch "true" stehenden
Anzeigezustand — derselbe Fund wie bei der nicht-stealth Variante — zurückzusetzen, einmal um
tatsächlich zu stoppen). Für ein Stealth-Feature, dessen ganzer Zweck ist, unauffällig zu
jiggeln und sich zuverlässig wieder abschalten zu lassen, ist ein Timer, der nach dem
Verlassen der View unbemerkt weiterläuft, der schlechtestmögliche Fehlermodus.

Kein Speicherzugriffsfehler (beide Timer-Aufrufe sind für sich genommen gültige API-Aufrufe),
reiner Nebenläufigkeits-/Reihenfolgefehler zwischen zwei Tasks — reproduzierbar bei jedem
"View verlassen, während der Timer gerade feuert", kein Sonderfall.

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten). Da der Fund ausschließlich in
der *Reihenfolge* der Aufrufe zwischen zwei Tasks liegt, nicht in Berechnungslogik, modelliert das
Repro jede der beiden Varianten als eine Menge diskreter Ereignisse (ein Mock-Timer-Aufruf bzw.
ein gesperrter Model-Schreibzugriff) und zählt alle Verschachtelungsreihenfolgen durch, die die
reale Synchronisation erlaubt — statt eine Reihenfolge willkürlich herauszugreifen:
- **ALT:** der Callback-interne Stop/Start ist gegenüber Exit's Stop *ungesperrt*, Exit kann
  also vor, zwischen oder nach den beiden Callback-Aufrufen landen (3 mögliche Positionen).
- **NEU** (dieser Fix — Exit setzt `model->running = false` *unter demselben Model-Lock*, bevor
  sie ihren eigenen `furi_timer_stop()` aufruft): Exits gesperrter Schreibzugriff und der
  gesperrte Callback-Block teilen sich denselben Mutex und können sich gegenseitig nicht mehr
  verschachteln — es bleiben nur zwei Gesamtreihenfolgen, beide werden explizit durchgespielt.
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt, identisch unter `-O0` gegengeprüft):
```
[ALT order 0] timer.armed after the interleaving = true (BUG: still jiggling after Quit)
[ALT order 1] timer.armed after the interleaving = true (BUG: still jiggling after Quit)
[ALT order 2] timer.armed after the interleaving = false
[ALT] 2 of 3 possible interleavings leave the timer armed after the user exited
[NEU order A] exit-write-first: callback sees running=false, does nothing; timer.armed=false
[NEU order B] callback-first: it re-arms, but exit's own stop() always runs after it (same thread, program order) and wins; timer.armed=false
[regression] exit while not running: ALT and NEU agree (armed=false)
[regression] 5000 NEU start/exit cycles alternating both orders: mismatches=0 (expected 0)
ALL CHECKS PASSED (5 checks)
```
ALT: 2 von 3 möglichen Verschachtelungen lassen den Timer nach dem Verlassen bewaffnet. NEU: 0 von
2 möglichen Gesamtreihenfolgen tun das — der Fix beseitigt die Race vollständig, nicht nur
statistisch.

## Betroffene Dateien
- `applications/system/hid_app/views/hid_mouse_jiggler_stealth.c` — nur
  `hid_mouse_jiggler_stealth_exit_callback()`. Der Timer-Callback selbst bleibt unverändert.

## Risiko
Minimal. Der Fix ergänzt in `hid_mouse_jiggler_stealth_exit_callback()` einen zusätzlichen
`with_view_model(...)`-Block, der `model->running = false` setzt — dasselbe Zugriffsmuster, das
`hid_mouse_jiggler_stealth_set_connected_status()` in derselben Datei bereits für einzelne
Feld-Updates verwendet. Keine Änderung an `hid_mouse_jiggler_stealth_timer_callback()`, am
Timer-Typ (`FuriTimerTypePeriodic` bleibt unangetastet) oder an der Bewegungs-/
Intervall-Berechnung — bewusst nicht Unleasheds volle Umstrukturierung (`rand()`/
`hid_hal_mouse_move()` außerhalb des Locks, eigener One-Shot-Timer, erneute Prüfung von
`model->running` unmittelbar vor dem Neubewaffnen) übernommen, da diese primär eine unabhängige
Performance-Überlegung adressiert (blockierendes `rand()` über ein mit dem BLE-Core geteiltes
HSEM, während der Model-Lock gehalten wird) und nicht für den Fix dieser Race notwendig ist: die
Reihenfolge zwischen dem Callback-internen Stop+Start-Paar und Exit's Stop wird bereits dadurch
vollständig geordnet, dass Exit's Model-Schreibzugriff denselben Mutex wie der Callback-Block
nimmt (siehe Host-Repro-Argumentation oben) — der Callback prüft `model->running` ja bereits ganz
am Anfang seines eigenen gesperrten Blocks. Exit während der Timer nicht läuft bleibt unverändert
folgenlos.

**Wichtig, abweichend von der bereits gelieferten nicht-stealth-Fassung:** die Reihenfolge
innerhalb von Exit ist hier sicherheitsrelevant. Der bereits gelieferte nicht-stealth-Fix
(`hid-jiggler-exit-does-not-reset-running-fix.md`) setzt `model->running = false` *nach*
`furi_timer_stop()`, weil dort reiner Anzeigezustand betroffen ist und der dortige Timer sich
nicht selbst neu bewaffnet. Bei der Stealth-Variante muss das Schreiben von `model->running`
dagegen *vor* dem eigenen `furi_timer_stop()`-Aufruf erfolgen: würde Exit zuerst stoppen und erst
danach sperren, könnte ein zu diesem Zeitpunkt bereits mit `running == true` mitten im Callback
befindlicher Timer-Aufruf trotzdem noch neu bewaffnen, nachdem Exit's eigener Stop bereits
gelaufen ist — Exit hätte dann keine Gelegenheit mehr, diese verspätete Neubewaffnung
einzuholen, da sie ihren eigenen Stop nicht ein zweites Mal aufruft.

## Port-Skizze
```c
static void hid_mouse_jiggler_stealth_exit_callback(void* context) {
    furi_assert(context);
    HidMouseJigglerStealth* hid_mouse_jiggler = context;
    // Clear the flag under the model lock *before* stopping the timer: the timer callback
    // below re-arms itself from inside that same lock every time it fires, so unless this
    // write is committed first, a callback invocation that already read running==true can
    // still re-arm the timer after this function's furi_timer_stop() has already run.
    with_view_model(
        hid_mouse_jiggler->view,
        HidMouseJigglerStealthModel * model,
        { model->running = false; },
        false);
    furi_timer_stop(hid_mouse_jiggler->timer);
}
```
Unified-Diff gegen die unveränderte `mntm-012`-Basis in
`hid-jiggler-stealth-exit-rearm-race-fix/hid_mouse_jiggler_stealth_c.patch` (`patch --dry-run -p1`
vorab gegen eine frische Kopie verifiziert, real angewendet und Byte-für-Byte gegen die separat
erstellte Fix-Fassung abgeglichen — identisch). Als Nebeneffekt behebt derselbe Schreibzugriff
auch die zur nicht-stealth Variante analoge Anzeige-Unstimmigkeit (View erneut betreten zeigt
sonst fälschlich "Stop" hervorgehoben, selbst wenn der Timer durch Exit bereits sauber gestoppt
wurde) — das ist hier kein separater Fund, sondern zwingender Bestandteil des Race-Fixes selbst
(siehe Risiko-Abschnitt: genau dieses Feld muss ohnehin unter Lock geschrieben werden, damit der
Callback sie beim erneuten Prüfen sieht).

## Testhinweis (lokal)
Patch anwenden, `fbt`/`ufbt` neu bauen und flashen. Mouse Jiggler Stealth öffnen, Min/Max-Intervall
testweise auf 1 Minute setzen (kürzeste erreichbare Feuerrate), Start drücken, kurz vor dem
nächsten erwarteten Feuern über Back/"Quit" die View verlassen und den USB-/BLE-Host beobachten:
vor dem Fix kann der Mauszeiger nach dem Verlassen weiterhin sporadisch springen (Wettlauf-Fenster
ist klein, ggf. mehrere Versuche nötig, um ihn zu treffen); nach dem Fix bewegt sich die Maus nach
Verlassen der View nie mehr. Gegenprobe unabhängig vom Timing: View erneut betreten — vor dem Fix
zeigt sie bei getroffenem Wettlauf-Fenster weiterhin "Stop" hervorgehoben (obwohl "verlassen"
wurde), nach dem Fix immer korrekt "Start". Zusätzliche Gegenprobe: View verlassen, während nicht
gejiggelt wird — Verhalten in beiden Fassungen unverändert.

## Quelle
Restlicher Teilfund aus `8fdb70dd3` (PR #1112, "HID: improve Mouse Jiggler timers and Bluetooth
controls", `DarkFlippers/unleashed-firmware`), unverändert nicht in `mntm-012` übernommen. Fix
ist eine gezielt minimierte Eigenentwicklung (nur die für die Race relevante Synchronisation),
nicht Unleasheds vollständige Umstrukturierung derselben Datei.
