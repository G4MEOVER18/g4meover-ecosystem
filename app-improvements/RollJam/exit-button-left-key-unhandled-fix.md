# RollJam — "Exit"-Hinweis (Left-Taste) auf IDLE/DONE-Bildschirm ohne Wirkung

## Was
Meine RollJam-App zeigt auf zwei Bildschirmen einen Exit-Hinweis: `rj_draw_cb()`
(`rolljam_app.c`) ruft im Zustand `ROLLJAM_IDLE` und im Zustand `ROLLJAM_DONE` jeweils
`elements_button_left(canvas, "Exit")` auf — dasselbe Standard-UI-Element, das auch der
Rest des Codes (z. B. `elements_button_right(canvas, "Abort")` in den aktiven Phasen,
`elements_button_center(canvas, "Start"/"Next"/"Again")`) konsistent so verwendet, dass
der gezeichnete Hinweis exakt der D-Pad-Taste entspricht, die der Input-Callback dafür
auswertet.

Für "Exit" gilt das nicht: `rj_input_cb()` (`rolljam_app.c:99-135`) wertet `InputKeyLeft`
an keiner Stelle aus. Die einzige Taste, die tatsächlich `app->abort = true` setzt und
damit die Haupt-Loop (`while(!app->abort)` in `rolljam_app()`) beendet, ist
`InputKeyBack` — die separate Hardware-Zurück-Taste, nicht die D-Pad-Links-Taste, die der
Bildschirm anzeigt. Drückt man auf dem IDLE- oder DONE-Bildschirm Links, passiert nichts;
die App lässt sich nur über die nirgends angezeigte Back-Taste verlassen.

## Nutzen
- Der auf dem Bildschirm angezeigte Hinweis tut tatsächlich das, was er verspricht —
  wichtig gerade bei einer Forschungs-/Pentest-App, bei der ich mich beim Testen auf die
  UI-Beschriftung statt auf Auswendiglernen verlassen will.
- Reine Input-Callback-Logik, keine Änderung an RF-Verhalten, State-Machine-Übergängen
  oder den bereits bestehenden Tasten (Back/Right/Ok bleiben unverändert).
- Symmetrisch zum bereits vorhandenen Right-Taste-Muster: dort löst
  `elements_button_right(canvas, "Abort")` ebenfalls zustandsabhängig `app->abort = true`
  aus — der neue Left-Zweig folgt exakt demselben Muster, nur für die beiden anderen
  Zustände.

## Betroffene Dateien
- `rolljam_app.c` — `rj_input_cb()`, neuer `InputKeyLeft`-Zweig direkt nach dem
  bestehenden `InputKeyBack`-Zweig (Zeile ~105-116 im Stand dieser Sitzung); keine
  weitere Datei betroffen (keine neuen Includes, keine geänderte Struktur/Signatur).

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
In `rj_input_cb()`, direkt nach dem bestehenden `InputKeyBack`-Zweig und vor dem
bestehenden `InputKeyRight`-Zweig:

```c
if(ev->key == InputKeyBack) {
    app->abort = true;
    return;
}

// Left button = the "Exit" hint drawn via elements_button_left() on the
// IDLE and DONE screens. Without this branch that hint does nothing —
// only the (unlabeled) hardware Back key actually exits from those
// screens, contradicting what the UI tells the user to press.
if(ev->key == InputKeyLeft) {
    if(app->state == ROLLJAM_IDLE || app->state == ROLLJAM_DONE) {
        app->abort = true;
    }
    return;
}

// Right button = abort during active phases
if(ev->key == InputKeyRight) {
    ...
```

Unified-Diff gegen die unveränderte `G4MEOVER18/RollJam`-Basis (`git apply --check`
vorab gegen eine frische Kopie verifiziert, real angewendet und byte-für-byte mit der
Arbeitskopie abgeglichen — identisch) in
`app-improvements/RollJam/exit-button-left-key-unhandled-fix/rolljam_app_c.patch`.

## Testhinweis
Host-Repro (kein Flipper/keine furi-/GUI-Abhängigkeit — bildet nur die reine
Zustands-/Flag-Entscheidungslogik von `rj_input_cb()` nach, mit ALT wortgleich der
aktuellen, unveränderten Funktion):

```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

`app-improvements/RollJam/repro-exit-button-left-key-unhandled/repro.c` — 22 Prüfungen,
lokal ausgeführt: `ALL CHECKS PASSED (0 Fehlschlaege)`, keine ASan-/UBSan-Meldungen.
Geprüft: (1) Left-Taste in `IDLE`/`DONE` — ALT bleibt bei `abort=false` (Hinweis
wirkungslos), NEU setzt `abort=true`; (2) Regression Left-Taste in den vier übrigen
Zuständen — ALT und NEU bleiben beide unverändert bei `abort=false`; (3) Regression
Back-Taste in allen sechs Zuständen — ALT und NEU identisch (`abort=true`); (4)
Regression Right-Taste — ALT und NEU identisch je Zustand; (5) Regression
Ok-Taste-Zustandsübergänge — ALT und NEU identisch für alle sechs Zustände; (6)
`InputTypeLong`/andere Event-Typen bleiben in beiden Fassungen ignoriert.

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
  App (623 Zeilen über `rolljam_app.c`/`.h`, `rolljam_rf.c`/`.h`, `rolljam_states.h`) im
  Anschluss an den bereits gelieferten Puffer-Reset-Fix
  (`app-improvements/RollJam/interlaced-capture-buffer-reset-fix.md`, inzwischen im
  Hauptzweig übernommen — der aktuelle Stand von `rolljam_rf.c` enthält bereits die dort
  vorgeschlagenen `rolljam_capture_pause()`/`_resume()`-Funktionen). Restlicher
  Codeumfang bei dieser Sitzung ebenfalls durchgesehen, kein weiterer Fund: RF-Layer
  (`rolljam_rf.c`) — Jam-/Capture-/Replay-Callbacks, OTG-Power-Handling, Zustandsübergänge
  zwischen `RF_MODE_*` — unauffällig; Signalpuffer-Packing (`sig->buf[]`, Bit 31 =
  Pegel, Bits 0-30 = Dauer in µs) konsistent zwischen RX-Callback und Replay-TX-Callback;
  `rolljam_replay_done()` prüft sowohl `tx_pos`-Erschöpfung als auch
  `subghz_devices_is_async_complete_tx()` — kein erkennbarer Deadlock-/Hänger-Fall.
- API-87-Einordnung: reine Input-Event-Verzweigung (`InputKey`-Enum, seit Jahren stabiler
  Teil der `input.h`-Kern-API), keine neue Firmware-API, kein Kompatibilitätsrisiko.
