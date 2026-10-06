# RollJam — "Exit"-Hinweis (Left-Taste) auf IDLE/DONE-Bildschirm ohne Wirkung

## Was
Meine RollJam-App zeigt auf zwei Bildschirmen einen Exit-Hinweis: `rj_draw_cb()`
(`rolljam_app.c`) ruft im Zustand `ROLLJAM_IDLE` und im Zustand `ROLLJAM_DONE` jeweils
`elements_button_left(canvas, "Exit")` auf — dasselbe Standard-UI-Element, das auch der
Rest des Codes (z. B. `elements_button_right(canvas, "Abort")` in den aktiven Phasen,
`elements_button_center(canvas, "Start"/"Next"/"Again")`) konsistent so verwendet, dass
der gezeichnete Hinweis exakt der D-Pad-Taste entspricht, die der Input-Callback dafür
auswertet.

Für "Exit" gilt das nicht: `rj_input_cb()` wertet `InputKeyLeft` an keiner Stelle aus.
Die einzige Taste, die auf `IDLE`/`DONE` tatsächlich etwas bewirkt, ist `InputKeyBack` —
die separate Hardware-Zurück-Taste, nicht die D-Pad-Links-Taste, die der Bildschirm
anzeigt. Drückt man auf dem IDLE- oder DONE-Bildschirm Links, passiert nichts; die App
lässt sich nur über die nirgends angezeigte Back-Taste verlassen. (Unverändert gültig
nach dem inzwischen gelieferten `abort-right-key-quits-app-fix`, der die Haupt-Loop von
`while(!app->abort)` auf eine Abort-Auflösung mit `app->quit`-Feld umgestellt hat — an
der Tatsache, dass `InputKeyLeft` nirgends ausgewertet wird, ändert das nichts.)

## Nutzen
- Der auf dem Bildschirm angezeigte Hinweis tut tatsächlich das, was er verspricht —
  wichtig gerade bei einer Forschungs-/Pentest-App, bei der ich mich beim Testen auf die
  UI-Beschriftung statt auf Auswendiglernen verlassen will.
- Reine Input-Callback-Logik, keine Änderung an RF-Verhalten, State-Machine-Übergängen
  oder den bereits bestehenden Tasten (Back/Right/Ok bleiben unverändert).
- Symmetrisch zum bereits vorhandenen Muster: Right/"Abort" löst zustandsabhängig
  `app->abort = true` aus (ohne `quit`, bricht nur die Phase ab), Back löst
  `app->abort = true; app->quit = true;` aus (beendet wirklich) — der neue Left-Zweig
  gehört semantisch zu Back, nicht zu Right, und setzt deshalb ebenfalls beide Felder.

## Betroffene Dateien
- `rolljam_app.c` — `rj_input_cb()`, neuer `InputKeyLeft`-Zweig direkt nach dem
  bestehenden `InputKeyBack`-Zweig, direkt vor dem (seit `abort-right-key-quits-app-fix`
  bereits geänderten) `InputKeyRight`-Zweig; keine weitere Datei betroffen (keine neuen
  Includes, keine geänderte Struktur/Signatur — `app->quit` existiert bereits seit dem
  genannten, vorausgesetzten Patch).

## Risiko
Sehr gering. Rein additiver Zweig in einer bereits zustandsabhängig verzweigenden
Funktion, nach exakt demselben Muster wie der bestehende `InputKeyRight`-Zweig. Wirkt
sich ausschließlich auf `InputKeyLeft`-Events aus, die die Funktion bisher komplett
ignoriert hat (kein bestehendes Verhalten geändert, keine bestehende Taste
umbelegt). In den vier übrigen Zuständen (`PHASE1_JAMMING`, `CAPTURED`,
`PHASE2_JAMMING`, `PHASE3_REPLAY`) — auf deren Bildschirmen kein Exit-Hinweis gezeichnet
wird — bleibt `InputKeyLeft` weiterhin wirkungslos, insbesondere kein versehentlicher
Abbruch einer laufenden Jam/Capture- oder Replay-Phase durch versehentliches
Links-Drücken.

## Port-Skizze / Fix
**Hinweis zur Reihenfolge:** Dieser Patch setzt den bereits gelieferten
`abort-right-key-quits-app-fix` voraus (führt dort `volatile bool quit;` in
`RollJamApp` sowie die Abort-Auflösung am Schleifenanfang von `rolljam_app()`
ein: `if(app->quit || app->state == ROLLJAM_IDLE || app->state == ROLLJAM_DONE) running = false; else app->state = ROLLJAM_IDLE;`).
Erst `abort-right-key-quits-app-fix` anwenden, dann diesen Patch.

In `rj_input_cb()`, direkt nach dem bestehenden `InputKeyBack`-Zweig und vor dem
bestehenden `InputKeyRight`-Zweig:

```c
if(ev->key == InputKeyBack) {
    app->abort = true;
    app->quit  = true;
    return;
}

// Left button = the "Exit" hint drawn via elements_button_left() on the
// IDLE and DONE screens. app->quit = true mirrors InputKeyBack above —
// both really mean "end the app", not "cancel the current phase". Without
// this branch InputKeyLeft does nothing at all — only the (unlabeled)
// hardware Back key actually exits from those screens, contradicting what
// the UI tells the user to press.
if(ev->key == InputKeyLeft) {
    if(app->state == ROLLJAM_IDLE || app->state == ROLLJAM_DONE) {
        app->abort = true;
        app->quit  = true;
    }
    return;
}

// Right button = Abort: bricht NUR die laufende Phase ab (zurueck ins Menu)
if(ev->key == InputKeyRight) {
    ...
```

`app->quit = true` ist hier streng genommen redundant zum bereits bestehenden
`app->state == ROLLJAM_IDLE || app->state == ROLLJAM_DONE`-Teil der
Abort-Auflösung (Left feuert ohnehin nur in genau diesen beiden Zuständen,
die App würde also auch ohne das zusätzliche Flag korrekt beendet). Ich setze
es trotzdem explizit, aus demselben Grund, aus dem `abort-right-key-quits-app-fix`
das Feld überhaupt eingeführt hat: `quit` soll die tatsächliche Absicht der
Taste ausdrücken ("wirklich beenden", exakt wie Back), statt sich auf eine
Nebenwirkung der aktuellen ODER-Verknüpfung zu verlassen, die bei einer
künftigen Änderung der Abort-Auflösung verschwinden könnte.

Unified-Diff gegen den Stand nach `abort-right-key-quits-app-fix` (`git apply
--check` vorab gegen eine frische Kopie von `G4MEOVER18/RollJam`, Commit
`1f90c00e1`, mit zuerst angewendetem `abort-right-key-quits-app-fix`-Patch
verifiziert, real angewendet und byte-für-byte mit der Arbeitskopie
abgeglichen — identisch) in
`app-improvements/RollJam/exit-button-left-key-unhandled-fix/rolljam_app_c.patch`.

## Testhinweis
Host-Repro (kein Flipper/keine furi-/GUI-Abhängigkeit — bildet nur die reine
Zustands-/Flag-Entscheidungslogik von `rj_input_cb()` plus die bereits bestehende
Abort-Auflösung der Hauptschleife nach; ALT entspricht dem Stand nach
`abort-right-key-quits-app-fix`, aber noch vor diesem Patch — `InputKeyLeft` wird
dort nirgends ausgewertet):

```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

`app-improvements/RollJam/repro-exit-button-left-key-unhandled/repro.c` — 23 Prüfungen,
lokal ausgeführt unter `-O0` und `-O1` (identischer Output): `ALL CHECKS PASSED
(0 Fehlschlaege)`, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen. Geprüft:
(1)/(2) Left-Taste in `IDLE`/`DONE` — ALT bleibt im jeweiligen Zustand stehen (Hinweis
wirkungslos), NEU beendet die App; (1b)/(2b) dasselbe direkt am `quit`-Flag nach
`rj_input_cb()` geprüft — ALT setzt es nie, NEU setzt es zusammen mit `abort`; (3)
Regression Left-Taste in den vier aktiven Phasen — ALT und NEU bleiben beide
unverändert bei `abort=false`/`quit=false`; (4) Regression Back-Taste in allen sechs
Zuständen — ALT und NEU identisch (`abort=true`, `quit=true`); (5) Regression
Right-Taste je Zustand — ALT und NEU identisch, `quit` bleibt dabei stets `false`.

Für den lokalen Funktionstest an der echten App: App auf dem IDLE-Bildschirm starten
("RollJam Research", Hinweis "Exit" unten links), Links-Taste drücken — vor dem Fix
passiert nichts, nach dem Fix beendet sich die App sofort. Denselben Test nach einem
abgeschlossenen Durchlauf auf dem DONE-Bildschirm ("Done!", ebenfalls "Exit" unten
links) wiederholen. Zusätzlich: während einer aktiven Phase (PHASE1/2 JAMMING oder
PHASE3 REPLAY, dort kein Exit-Hinweis sichtbar) Links drücken — App darf nicht
abbrechen, nur Rechts ("Abort") bzw. die Hardware-Back-Taste dürfen dort wirken.

## Quelle
- `G4MEOVER18/RollJam` (main, Commit `1f90c00e1`): `rolljam_app.c`
  (`rj_draw_cb`/`rj_input_cb`), eigener Fund beim vollständigen erneuten Durchgehen der
  App (623 Zeilen über `rolljam_app.c`/`.h`, `rolljam_rf.c`/`.h`, `rolljam_states.h`),
  ursprünglich im Anschluss an den bereits gelieferten Puffer-Reset-Fix
  (`app-improvements/RollJam/interlaced-capture-buffer-reset-fix.md`, inzwischen im
  Hauptzweig übernommen) gemacht.
- Diese Sitzung: Patch/Repro/Doku an den inzwischen ebenfalls gelieferten
  `app-improvements/RollJam/abort-right-key-quits-app-fix.md` angepasst (dort als
  offener Folgepunkt vermerkt) — frische, unabhängige Kopie von `G4MEOVER18/RollJam`
  (Commit `1f90c00e1`) geklont, den `abort-right-key-quits-app-fix`-Patch dagegen
  angewendet (`git apply --check` verifiziert) und den hier vorliegenden Patch gegen
  diesen Zwischenstand neu erzeugt und erneut `git apply --check`/real angewendet/
  byte-für-byte abgeglichen, statt den alten Patch unverändert gegen die unveränderte
  Basis stehen zu lassen (dort hätte er beim Zusammenführen mit
  `abort-right-key-quits-app-fix` nicht mehr konfliktfrei gepasst, da beide Patches
  dieselbe Stelle in `rj_input_cb()` betreffen).
- API-87-Einordnung: reine Input-Event-Verzweigung (`InputKey`-Enum, seit Jahren stabiler
  Teil der `input.h`-Kern-API), keine neue Firmware-API, kein Kompatibilitätsrisiko.
