# RollJam — "Abort" (Right-Taste) beendet die ganze App statt nur die laufende Phase, "Abort"-Hinweis in CAPTURED wirkungslos

## Was
Meine `RollJam`-App (eigenes Repo) zeigt auf vier Bildschirmen `elements_button_right(canvas, "Abort")`:
`PHASE1_JAMMING`, `CAPTURED`, `PHASE2_JAMMING` (alle drei in `rj_draw_cb()`,
`rolljam_app.c`) — `PHASE3_REPLAY` fehlte der Hinweis bisher ganz, obwohl die Taste
dort bereits wirkt (siehe unten). Ich habe diese vier Bildschirme gegen den
tatsächlichen Input- und Hauptschleifen-Code geprüft und zwei getrennte Fehler
gefunden, beide in derselben `app->abort`-Buchhaltung.

**Fund 1 — "Abort" beendet die App komplett statt zur Menü-Übersicht zurückzukehren.**
`rj_input_cb()` (`rolljam_app.c:99-135`) setzt für `InputKeyRight` in
`PHASE1_JAMMING`/`PHASE2_JAMMING`/`PHASE3_REPLAY` exakt dasselbe Flag wie
`InputKeyBack`:

```c
if(ev->key == InputKeyBack) {
    app->abort = true;
    return;
}

// Right button = abort during active phases
if(ev->key == InputKeyRight) {
    if(app->state == ROLLJAM_PHASE1_JAMMING ||
       app->state == ROLLJAM_PHASE2_JAMMING ||
       app->state == ROLLJAM_PHASE3_REPLAY) {
        app->abort = true;
    }
    return;
}
```

Die Hauptschleife in `rolljam_app()` (`rolljam_app.c:238`) kennt aber nur ein
einziges Verhalten für dieses Flag: `while(!app->abort) { ... }` — sobald
`app->abort` wahr ist, beendet sich die Schleife unbedingt, unabhängig davon,
welcher Zustand gerade aktiv war und unabhängig davon, welche Taste das Flag
gesetzt hat. Innerhalb der Phasen-Fälle gibt es zwar einzelne
`if(app->abort) break;`-Prüfungen (verhindern nur, dass der Zustand noch auf
`CAPTURED`/`DONE` weitergeschaltet wird) — aber keine davon führt zurück zu
`ROLLJAM_IDLE`; sie lassen die Schleife beim nächsten Durchlauf einfach
auslaufen.

Das bedeutet konkret: Drücke ich während Phase 1 (Jam+RX für Signal A) auf
„Abort", weil das Timing nicht passt oder ich den Versuch neu starten will,
beendet sich die gesamte App — ich lande auf dem Flipper-Hauptmenü statt auf
dem RollJam-Startbildschirm, und ein bereits erfolgreich erfasstes Signal A
(falls ich in Phase 2 abbreche) ist weg, weil `app` beim nächsten Start neu
alloziert wird. Das widerspricht sowohl der eigenen Bildschirmbeschriftung
("Abort" statt "Exit", die einzige andere Beschriftung, die ich in dieser App
für "Taste beendet die App" verwende — auf `IDLE`/`DONE` zeigt exakt dafür
`elements_button_left(canvas, "Exit")`) als auch dem Verhalten meiner
Schwester-App `RollLab` (`rolllab_app.c`, Hauptschleife): dort führt genau
derselbe Fall — Abort während einer aktiven RF-Phase — zurück zum Menü
(`RLAB_MENU`), und nur ein Abort *im* Menü selbst beendet die App:

```c
// rolllab_app.c (RollLab), zum Vergleich
if(app->abort) {
    if(app->device && app->rf_mode != RLAB_RF_IDLE) { ... }
    if(app->state == RLAB_MENU) {
        running = false;
        break;
    }
    app->state = RLAB_MENU;
    app->abort = false;
}
```

**Fund 2 — der "Abort"-Hinweis in `CAPTURED` ist wirkungslos.** `rj_draw_cb()`
zeichnet `elements_button_right(canvas, "Abort")` auch im Zustand
`ROLLJAM_CAPTURED` (Zeile 63) — aber `rj_input_cb()`s `InputKeyRight`-Zweig
(oben zitiert) prüft nur `PHASE1_JAMMING`/`PHASE2_JAMMING`/`PHASE3_REPLAY`.
`ROLLJAM_CAPTURED` fehlt in dieser Liste. Drücke ich auf dem
"Signal A captured!"-Bildschirm Rechts, passiert buchstäblich nichts — der
angezeigte Hinweis verspricht etwas, das die Taste nicht tut. Das ist
derselbe Fehlertyp (angezeigter Button-Hinweis ohne zugehörige Input-Behandlung)
wie mein bereits dokumentierter Fund
`exit-button-left-key-unhandled-fix.md` (dort: "Exit" auf `IDLE`/`DONE`), nur
beim "Abort"-Hinweis statt beim "Exit"-Hinweis.

Mit einem eigenständigen, hostseitigen Nachbau der Zustandsmaschine (reine
Flag-/Zustandsübergänge aus `rj_input_cb()` plus der Abort-Auflösung der
Hauptschleife, ohne Canvas-/GUI-/RF-Code) verifiziert: ALT beendet die
komplette App bei Right in `PHASE1_JAMMING`/`PHASE3_REPLAY` und tut nichts bei
Right in `CAPTURED`; NEU kehrt in allen drei Fällen zu `ROLLJAM_IDLE` zurück
(siehe Testhinweis).

## Nutzen
- "Abort" tut danach, was es auf dem Bildschirm verspricht: die laufende
  Jam/Capture- oder Replay-Phase abbrechen und zur Übersicht zurückkehren —
  nicht die ganze App verlassen. Ich kann einen fehlgeschlagenen Versuch
  (z. B. Phase 1 nochmal starten, weil der Keyfob nicht rechtzeitig gedrückt
  wurde) ohne App-Neustart wiederholen.
- Der "Abort"-Hinweis in `CAPTURED` wird funktional, statt ein wirkungsloses
  UI-Versprechen zu sein — konsistent mit den drei anderen Bildschirmen, die
  denselben Hinweis zeigen.
- `PHASE3_REPLAY` bekommt den bisher fehlenden "Abort"-Hinweis, obwohl Right
  dort schon vorher wirkte — jetzt stimmen Anzeige und Verhalten auf allen
  vier Bildschirmen überein.
- `InputKeyBack` ("Zurück"-Hardwaretaste) bleibt unverändert die einzige
  Taste, die die App wirklich vollständig beendet — unabhängig vom
  aktuellen Zustand, exakt wie bisher.
- Reine Zustandsmaschinen-/Input-Logik, keine Änderung an der RF-Konfiguration,
  den Jam-/Capture-/Replay-Callbacks (`rolljam_rf.c` bleibt unberührt) oder dem
  bereits separat dokumentierten, noch offenen `InputKeyLeft`-Fund
  (`exit-button-left-key-unhandled-fix.md` — dieser Patch baut nicht darauf
  auf und ändert nichts an dessen offenem Zustand).

## Betroffene Dateien (lokal, in meinem RollJam-Repo)
- `rolljam_app.h` — `RollJamApp`-Struct: neues Feld `volatile bool quit;` direkt
  nach dem bestehenden `abort`-Feld.
- `rolljam_app.c`:
  - `rj_input_cb()`: `InputKeyBack`-Zweig setzt zusätzlich `app->quit = true`
    (Back bleibt der einzige echte "Beenden"-Weg). `InputKeyRight`-Zweig
    bekommt `ROLLJAM_CAPTURED` in die Zustandsliste (Fund 2); `app->quit`
    bleibt dort bewusst `false`.
  - `rolljam_app()`: Hauptschleife von `while(!app->abort)` auf
    `bool running = true; while(running)` umgestellt, mit einem neuen Block
    direkt am Schleifenanfang, der `app->abort` auflöst: `quit` (oder
    bereits `IDLE`/`DONE`) beendet die App (`running = false`), sonst
    zurück zu `ROLLJAM_IDLE`; danach `app->abort = false; continue;`. Die
    bestehenden `if(app->abort) break;`-Prüfungen innerhalb der
    Phasen-Fälle bleiben unverändert (verhindern weiterhin, dass ein
    abgebrochener Capture-/Replay-Versuch fälschlich als Erfolg gewertet
    wird) — sie laufen jetzt nur noch in einen funktionierenden
    Wiederherstellungspfad statt ins Programmende.
  - `rj_draw_cb()`: `ROLLJAM_PHASE3_REPLAY`-Fall bekommt zusätzlich
    `elements_button_right(canvas, "Abort")` (Fund 2, Gegenstück: Anzeige
    nachgezogen, wo die Taste bereits wirkte).

Kein Eingriff in `rolljam_rf.c`, `rolljam_states.h` oder
`rj_run_jam_capture_phase()` selbst — deren `while(!app->abort)`-Innenschleife
und der Zustandsautomat für Jam/Capture/Replay bleiben byte-identisch.

## Patch
Siehe `abort-right-key-quits-app-fix/rolljam_app_c.patch` (`git apply --check`
vorab gegen eine frische, unabhängige Kopie von `G4MEOVER18/RollJam`
(`main`, Commit `1f90c00e1`) verifiziert, real angewendet und byte-für-byte mit
der Arbeitskopie abgeglichen).

## Testhinweis (ohne Hardware)
Eigenständiges, hostseitiges Reproduktionsprogramm der reinen
Zustandsmaschinen-Logik (`rj_input_cb()` plus die neue Abort-Auflösung der
Hauptschleife, 1:1 transkribiert; `rj_run_jam_capture_phase()` durch einen
Stub ersetzt, der sich bei bereits gesetztem `abort` exakt wie das Original
verhält — sofortige Rückkehr ohne das Ergebnis zu setzen):
`app-improvements/RollJam/repro-rolljam-abort-right-key-quits-app/repro.c`

```sh
gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
./repro
```

Bei mir lokal reproduziert (beide Optimierungsstufen `-O0`/`-O1`, ASan/UBSan):
sechs Szenarien (Right in `PHASE1_JAMMING`/`PHASE3_REPLAY`: ALT beendet die
App vollständig, NEU kehrt zu `IDLE` zurück; Right in `CAPTURED`: ALT
wirkungslos, NEU kehrt zu `IDLE` zurück; Regression Back in allen sechs
Zuständen: beide beenden die App vollständig; Regression Right in
`IDLE`/`DONE`: beide weiterhin wirkungslos; Regression Erfolgsfall ohne
Abbruch: Phase 1 erfolgreich → `CAPTURED`, unverändert) — `ALL CHECKS PASSED`,
keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen.

Funktionaler Test nach dem Patch, lokal auf dem Flipper (kein Sender/Empfänger
nötig, reine UI-/Zustandsprüfung):
1. `ufbt` bauen, App starten.
2. „Start" drücken (→ `PHASE1_JAMMING`), dann sofort Rechts ("Abort") drücken:
   die App muss zum `RollJam Research`-Startbildschirm zurückkehren, nicht
   zum Flipper-Hauptmenü.
3. Phase 1 einmal erfolgreich durchlaufen (Keyfob drücken) bis `CAPTURED`,
   dann Rechts ("Abort") drücken: muss jetzt ebenfalls zum Startbildschirm
   zurückkehren (vorher: keine Reaktion).
4. Einmal bis `PHASE3_REPLAY` durchlaufen und dort Rechts ("Abort") drücken:
   muss zum Startbildschirm zurückkehren; der Bildschirm muss den
   "Abort"-Hinweis jetzt auch anzeigen (vorher: kein Hinweis, obwohl die
   Taste schon wirkte).
5. Regressionsfall: auf jedem Bildschirm die Zurück-Hardwaretaste drücken —
   die App muss weiterhin vollständig beendet werden (zurück zum
   Flipper-Hauptmenü), in jedem der sechs Zustände.
6. Regressionsfall: ein vollständiger, nicht abgebrochener Durchlauf
   (Phase 1 → Phase 2 → Phase 3 → „Done!") bleibt in Ablauf und Anzeige
   unverändert.

## Risiko
- Niedrig bis mittel: ändert die Semantik einer bereits existierenden Taste
  (Right während aktiver Phasen) von "App beenden" zu "Phase abbrechen,
  zurück ins Menü" — das ist eine Verhaltensänderung, aber eine, die die
  bisherige (fehlerhafte) Semantik durch die auf dem Bildschirm selbst
  versprochene ersetzt, nicht durch eine neu erfundene.
- `InputKeyBack` bleibt in jedem Zustand unverändert der vollständige
  Beenden-Weg — durch den Host-Repro-Regressionsfall (Fall 4, alle sechs
  Zustände) bestätigt.
- Die bestehenden `if(app->abort) break;`-Prüfungen in den Phasen-Fällen
  (`PHASE1_JAMMING`/`PHASE2_JAMMING`) bleiben unverändert erhalten — sie
  verhindern weiterhin, dass ein durch Abbruch beendeter Capture-Versuch
  fälschlich als `captured`-Erfolg gewertet wird. Der neue Abort-Auflösungs-
  Block am Schleifenanfang setzt `app->abort` erst zurück, nachdem der
  aktuelle Zustand bereits auf `ROLLJAM_IDLE` gesetzt (oder die App beendet)
  wurde — kein Fenster, in dem ein alter, noch nicht aufgelöster
  Abort-Zustand einen weiteren Schleifendurchlauf im alten Zustand
  verursachen könnte (per Host-Repro über alle sechs Zustände × beide
  Abbruchtasten gegengeprüft).
- `rj_run_jam_capture_phase()` selbst (die eigentliche Jam/Capture-Logik,
  Timing, RF-Zustandswechsel) ist von diesem Patch nicht betroffen — nur der
  Rückgabewert-Konsument in der Hauptschleife ändert sich.
- Kein Einfluss auf den separat dokumentierten, weiterhin offenen
  `InputKeyLeft`-Fund (`exit-button-left-key-unhandled-fix.md`): dessen
  vorgeschlagener Patch setzt nur `app->abort` auf `IDLE`/`DONE`-Bildschirmen;
  wird er später ergänzt, sollte er dort konsequenterweise ebenfalls
  `app->quit = true` setzen (gleiche Semantik wie Back), da "Exit" dort
  wirklich die App beenden soll — als Hinweis für die künftige Sitzung,
  die diesen Fund aufgreift, hier vermerkt, aber nicht Teil dieses Patches.

## Quelle
- `G4MEOVER18/RollJam` (main, Commit `1f90c00e1`): `rolljam_app.c`
  (`rj_draw_cb`, `rj_input_cb`, `rolljam_app`), `rolljam_app.h`
  (`RollJamApp`-Struct).
- Vergleichsquelle für das korrekte Abort-zu-Menü-Verhalten:
  `G4MEOVER18/RollLab` (main, Commit `4ea80481`): `rolllab_app.c`
  (`rolllab_app()`-Hauptschleife, `if(app->abort) { ... }`-Block).
- Verwandter, weiterhin offener eigener Fund (nicht Teil dieses Patches):
  `app-improvements/RollJam/exit-button-left-key-unhandled-fix.md`.
