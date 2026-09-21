# HID Mouse Jiggler: "Stop" setzt nur das Anzeige-Flag, der Timer bleibt bis zum Verlassen der View bewaffnet

## Was
Ich habe den nächsten offenen Teilfund aus dem bei der letzten Firmware-Mining-Sitzung
zurückgestellten "restlichen Umfang von `8fdb70dd3`" (PR #1112, "HID: improve Mouse Jiggler
timers and Bluetooth controls") aufgegriffen: `hid_mouse_jiggler_input_callback()` in
`applications/system/hid_app/views/hid_mouse_jiggler.c` (nicht-stealth Mouse Jiggler). Die
betroffenen Zeilen bei mir (`mntm-012`) sind byteidentisch zum Unleashed-Vorher-Stand (gesamte
Datei Zeile für Zeile gegengeprüft, keine sonstigen Abweichungen).

## Fund: Pressing "Stop" räumt `model->running`, aber nie den Timer selbst
```c
if(event->type == InputTypePress && event->key == InputKeyOk) {
    model->running = !model->running;
    if(model->running) {
        furi_timer_stop(hid_mouse_jiggler->timer);
        furi_timer_start(hid_mouse_jiggler->timer, intervals[model->interval_idx]);
    };
    consumed = true;
}
```
Der `if(model->running)`-Block behandelt ausschließlich den Übergang zu "läuft": nur dort wird
`furi_timer_stop()`/`furi_timer_start()` aufgerufen. Der umgekehrte Übergang (Stop-Druck,
`model->running` wird `false`) fasst den Timer überhaupt nicht an — der zuvor mit
`furi_timer_start()` bewaffnete periodische `FuriTimer` bleibt bewaffnet und feuert unverändert im
zuletzt gewählten Intervall (500 ms bis 60 s) weiter, solange die View geöffnet bleibt.
`hid_mouse_jiggler_timer_callback()` nimmt bei jedem dieser Ticks weiterhin den View-Model-Lock
(`with_view_model(...)`), prüft dort `model->running`, findet es `false` und tut sonst nichts —
kein Speicherfehler, aber jeder Tick ist vollständig verschwendete Arbeit (Timer-Daemon-Weckung,
Mutex-Aufnahme/-Freigabe, Funktionsaufruf), die die App durch das Anzeigen von "Start" (statt
"Stop") dem Nutzer gerade als beendet ausgibt. Einzig `hid_mouse_jiggler_exit_callback()` (View
verlassen) und `hid_mouse_jiggler_free()` rufen `furi_timer_stop()` bedingungslos auf — solange man
auf der View bleibt, bleibt der Timer nach einem Stop-Druck unbegrenzt bewaffnet, nicht nur für
einen einzelnen überzähligen Tick.

**Praktische Auswirkung:** Kein Speicherzugriffsfehler, kein Angriffsvektor — reiner
Ressourcen-/Korrektheitsfehler. Erreichbar bei jedem einzelnen Stop-Druck (Normalbetrieb, kein
Sonderfall), nicht nur in einem Randfall. Widerspricht der von der eigenen UI kommunizierten
Absicht ("Stop" zeigt an, dass jiggeln beendet ist) und hält den Gerätetimer unnötig aktiv, solange
die View offen bleibt.

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten), bildet die beiden
OK-Druck-Zweige aus `hid_mouse_jiggler_input_callback()` 1:1 nach (`model->running`-Umschaltung
identisch zum echten Code) und ersetzt die opaken `furi_timer_start()`/`furi_timer_stop()`-HAL-
Aufrufe durch einen minimalen Mock-Timer mit `armed`-Flag plus Aufrufzählern — der Fund liegt
ausschließlich darin, *welcher* Zweig die Timer-Funktionen aufruft, nicht darin, was die
Timer-Funktionen selbst intern tun:
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt, identisch unter `-O0` gegengeprüft):
```
[group1] ALT: after pressing Stop the timer is still armed (armed=true), 1000/1000 further ticks are serviced into a no-op callback
[group2] NEU: after pressing Stop the timer is armed=false, 0/1000 further ticks are serviced (expected 0)
[group3] regression over 5000 presses: running-flag mismatches ALT-vs-NEU=0, NEU armed!=running mismatches=0
[group4] ALT: armed stays stuck true indefinitely once Stop is pressed once
ALL CHECKS PASSED (9 checks)
```
Gruppe 1 belegt den Fund direkt: nach Start+Stop bleibt der Mock-Timer bei ALT bewaffnet, 1000
simulierte weitere Ticks werden alle noch "bedient" (Lock-Aufnahme + No-op), obwohl die UI "Start"
anzeigt. Gruppe 2 bestätigt, dass NEU denselben Ablauf mit 0 verschwendeten Ticks abschließt — der
Timer ist nach Stop tatsächlich angehalten. Gruppe 3 überstreicht 5000 pseudozufällige OK-Drücke
und bestätigt zwei Invarianten gleichzeitig: das `model->running`-Umschaltverhalten selbst ist
zwischen ALT und NEU identisch (der Fix ändert nur die Timer-Ansteuerung, nicht die
Sichtbarkeits-/Zustandslogik), und bei NEU gilt durchgehend `timer.armed == model.running` — keine
einzige der 5000 Iterationen weicht ab. Gruppe 4 zeigt, dass der ALT-Zustand kein einmaliger
Ausreißer ist, sondern dauerhaft hängen bleibt, bis erneut Start gedrückt wird. Unter
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined` und separat unter `-O0`
gegengeprüft: keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis.

## Betroffene Dateien
- `applications/system/hid_app/views/hid_mouse_jiggler.c` —
  `hid_mouse_jiggler_input_callback()`, einzige Fix-Stelle (drei geänderte Zeilen: ein
  zusätzlicher unbedingter `furi_timer_stop()`-Aufruf, der bisherige bedingte Aufruf entfällt).
  Keine Signatur-/Aufrufstellenänderung, keine Auswirkung auf `hid_mouse_jiggler_timer_callback()`,
  `hid_mouse_jiggler_exit_callback()` oder die (bei mir separat als eigener Fund bereits
  dokumentierte) Stealth-Variante derselben App.

## Risiko
Sehr gering. Reine Logikänderung, kein neuer Fehlerpfad, keine geänderte Funktionssignatur. Das
bereits im selben Codepfad existierende Muster — `furi_timer_stop()`/`furi_timer_start()` von
`hid_mouse_jiggler_input_callback()` aus, innerhalb desselben `with_view_model`-Locks, aufrufen —
wird lediglich um den bislang fehlenden Zweig ergänzt, kein neues Nebenläufigkeits-/Sperrmuster.
Bewusst *nicht* Unleasheds volle Umstrukturierung übernommen (dort werden die Timer-Aufrufe aus
dem Lock heraus verschoben, um ein Wettlauf-Risiko im Stealth-Timer-Neubewaffnungspfad zu
vermeiden) — dieser nicht-stealth Jiggler hat keinen selbst-neubewaffnenden One-Shot-Timer
(`FuriTimerTypePeriodic`, keine Neubewaffnung aus dem Callback heraus), das dortige Wettlauf-Risiko
besteht hier nicht; die minimale, auf diesen einen Fund begrenzte Änderung genügt.

## Port-Skizze / Fix
```c
if(event->type == InputTypePress && event->key == InputKeyOk) {
    model->running = !model->running;
    // Stop unconditionally first: previously only the "now running" branch touched
    // the timer, so pressing Stop left it armed and ticking into a no-op callback.
    furi_timer_stop(hid_mouse_jiggler->timer);
    if(model->running) {
        furi_timer_start(hid_mouse_jiggler->timer, intervals[model->interval_idx]);
    }
    consumed = true;
}
```
Unified-Diff gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab gegen eine
frische Kopie verifiziert, real angewendet und Byte-für-Byte gegen die separat host-getestete
Fassung abgeglichen — identisch) in
`firmware-mining/hid-jiggler-stop-does-not-stop-timer-fix/hid_mouse_jiggler_c.patch`.

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-hid-jiggler-stop-does-not-stop-timer-fix/repro.c`, host-kompilierbar, keine
Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Patch anwenden, `fbt` neu bauen und flashen,
danach die (nicht-stealth) Mouse-Jiggler-View öffnen, Start drücken, kurz warten, dann Stop
drücken und auf der View verbleiben (nicht verlassen) — vor dem Fix lässt sich der weiterhin
bewaffnete Timer nur indirekt beobachten (z. B. über einen Debug-Build mit Logging im
Timer-Callback oder einen Stromverbrauchsvergleich über mehrere Minuten im gestoppten Zustand);
nach dem Fix bleibt der Timer nach Stop nachweislich inaktiv. Einfacher direkt sichtbarer Test:
Stop drücken, danach mit Links/Rechts das Intervall ändern (vor dem Fix wie nach dem Fix erlaubt,
da `!model->running`), erneut Start drücken — beide Varianten starten dabei korrekt neu (kein
Regressionsrisiko für den Start-Pfad selbst), der Unterschied liegt ausschließlich in der Zeit
*zwischen* Stop und dem nächsten Start.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `8fdb70dd3` (PR #1112, "HID: improve Mouse
  Jiggler timers and Bluetooth controls"), Teilausschnitt betreffend
  `applications/system/hid_app/views/hid_mouse_jiggler.c`.
- Aus demselben Commit bereits als eigener Fund portiert: die BLE-Status-Callback-Registrierungs-
  reihenfolge in `applications/system/hid_app/hid.c`, siehe
  `firmware-mining/hid-ble-status-callback-registered-after-advertising-fix.md`.
- Weiterhin aus demselben Commit zurückgestellt: `hid_mouse_jiggler_exit_callback()` setzt
  `model->running` beim Verlassen der View nicht zurück (führt beim erneuten Öffnen zu einem
  fälschlich hervorgehobenen "Stop", obwohl der Jiggler durch das Verlassen bereits gestoppt
  wurde); Stealth-Neubewaffnungs-Wettlauf und BLE-Verbindungswarten (beide in
  `hid_mouse_jiggler_stealth.c`) — für eine künftige Sitzung vorgemerkt.
