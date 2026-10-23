# RollLab — RLAB_CAP_TIMEOUT_MS deklariert, aber nie durchgesetzt: Signal-Aufnahme hängt bei Ausbleiben eines Signals unbegrenzt

## Was
`rolllab_app.h` deklariert `#define RLAB_CAP_TIMEOUT_MS 9000U // 9 s per capture
attempt` — der Kommentar sagt ausdrücklich, dass jeder Aufnahmeversuch nach 9
Sekunden enden soll. Die Konstante wird aber nirgends im gesamten Repo
tatsächlich gelesen (nur diese eine Definitionsstelle existiert). Der
`RLAB_CAPTURING`-Fall in der Hauptschleife (`rolllab_app()`, `rolllab_app.c`)
prüft ausschließlich, ob ein Signal fertig erfasst wurde:

```c
case RLAB_CAPTURING: {
    if(app->rf_mode != RLAB_RF_CAPTURING) {
        rlab_capture_ref_start(app);
        snprintf(app->status, sizeof(app->status), "Listening...");
    }
    snprintf(app->status, sizeof(app->status), "Edges: %zu", app->ref_sig.count);
    view_port_update(vp);

    if(rlab_capture_ref_done(app)) {
        ...
    }

    furi_delay_ms(20);
    break;
}
```

Drücke ich in keinem der vier Modi (Analyze/Replay/Rollback/Sync Window) den
Keyfob — weil er außer Reichweite ist, die Batterie leer ist, oder ich mich
einfach vertan habe —, bleibt `rlab_capture_ref_done()` dauerhaft `false`
(`RLAB_MIN_EDGES` von 16 Flanken wird nie erreicht). Der Zustand verharrt dann
unbegrenzt in `RLAB_CAPTURING`, sichtbar nur als eine stehenbleibende
`"Edges: 0"`-Zeile — kein Hinweis, keine automatische Rückkehr. Der einzige
Ausweg ist ein manueller Tastendruck (Rechts/"Abort" oder die Hardware-
Zurück-Taste), den ich selbst erkennen und auslösen muss. Für eine
Forschungs-/Lehr-App, die genau diesen Signalerfassungs-Schritt wiederholt
in allen vier Betriebsarten durchläuft, ist das unnötige Reibung: die App
selbst benennt bereits (im eigenen Quelltext) ein Zeitbudget für genau diesen
Fall, setzt es aber nie um.

## Nutzen
- Nach einem erfolglosen Aufnahmeversuch (kein Signal innerhalb von 9 s)
  kehrt die App von selbst zum Hauptmenü zurück, statt auf einen manuellen
  Abbruch zu warten — konsistent mit dem bereits vorhandenen
  "ERR: CC1101 not found"-Pfad im selben Zustand, der genau dasselbe Muster
  (Status setzen, `app->state = RLAB_MENU`) schon für den Fall eines fehlenden
  Moduls verwendet.
- Setzt lediglich eine bereits im eigenen Code deklarierte Absicht um — keine
  neue Konstante, kein neues Zeitbudget erfunden.
- Reine Zustandsmaschinen-/Zeitlogik (ein neues Feld, ein `furi_get_tick()`-
  Aufruf, ein zusätzlicher `else if`-Zweig) — keine Änderung an RF-Timing,
  Capture-Callback (`rolllab_rf.c` bleibt unberührt) oder der
  Erfolgs-Verzweigung je Modus.
- Der Erfolgsfall (Signal rechtzeitig erfasst) bleibt unverändert Vorrang
  (`if`-Zweig vor `else if`) — selbst wenn beide Bedingungen rechnerisch im
  selben Schleifendurchlauf zuträfen, gewinnt immer das erfasste Signal, nie
  der Timeout.

## Betroffene Dateien (lokal, in meinem RollLab-Repo)
- `rolllab_app.h` — `RollLabApp`-Struct: neues Feld
  `uint32_t cap_start_tick;` (Zeitstempel des aktuellen Aufnahmeversuchs).
- `rolllab_app.c` — `rolllab_app()`, Fall `RLAB_CAPTURING`:
  - beim Start eines neuen Aufnahmeversuchs (`if(app->rf_mode !=
    RLAB_RF_CAPTURING) { ... }`) zusätzlich `app->cap_start_tick =
    furi_get_tick();`.
  - neuer `else if`-Zweig nach der bestehenden `if(rlab_capture_ref_done(app))`-
    Prüfung: ist `furi_get_tick() - app->cap_start_tick >=
    RLAB_CAP_TIMEOUT_MS`, wird die laufende Aufnahme gestoppt
    (`rlab_capture_ref_stop(app)`), ein Statustext gesetzt und
    `app->state = RLAB_MENU` gesetzt — exakt dasselbe Rückkehr-Muster wie der
    bereits bestehende Geräte-Guard für den Fall "CC1101 nicht gefunden".

Kein Eingriff in `rolllab_rf.c`, `rolllab_states.h`, die drei anderen
Hauptschleifen-Fälle (`RLAB_ADVANCE`/`RLAB_PROBING`/`RLAB_REPLAYING`, die kein
analoges deklariertes Zeitbudget besitzen) oder `rlab_analyze()`.

## Patch
Siehe `capture-timeout-not-wired-fix/rolllab_app_h.patch` und
`capture-timeout-not-wired-fix/rolllab_app_c.patch` (`git apply --check`
vorab gegen zwei frische, unabhängige Kopien von `G4MEOVER18/RollLab`
(`main`, Commit `4ea80481`) verifiziert, real angewendet und per `md5sum`
byte-für-byte abgeglichen — beide Zweitkopien identisch zu den mitgelieferten
Zieldateien `rolllab_app_h.target`/`rolllab_app_c.target`). Beide Patches
zusammen anwenden, Reihenfolge beliebig (unabhängige Stellen in zwei
verschiedenen Dateien).

## Testhinweis (ohne Hardware)
Eigenständiges, hostseitiges Reproduktionsprogramm der reinen
Zustandsübergangs-Logik des `RLAB_CAPTURING`-Falls (ALT ohne Timeout-Zweig,
NEU mit dem Patch 1:1 transkribiert), gegen eine vollständig kontrollierbare
Mock-Uhr anstelle von `furi_get_tick()`:
`app-improvements/RollLab/repro-capture-timeout-not-wired/repro.c`

```sh
gcc -std=c17 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
./repro
gcc -std=c17 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c
./repro
```

Bei mir lokal reproduziert (beide Optimierungsstufen `-O0`/`-O1`, ASan/UBSan):
12 Prüfungen — ALT bleibt nach simulierten 40 s ohne Signal unverändert in
`RLAB_CAPTURING` stecken (der Fund selbst, reproduziert); NEU kehrt nach
exakt 450 simulierten 20-ms-Schleifendurchläufen (= 9000 ms) zu `RLAB_MENU`
zurück; ein rechtzeitig (bei 500 ms) eintreffendes Signal erreicht bei NEU
weiterhin `RLAB_ANALYZE_VIEW`, nicht den Timeout-Pfad; trifft das Signal
exakt auf demselben Tick ein, an dem auch der Timeout auslösen würde, gewinnt
weiterhin der Erfolgsfall; ein zweiter Aufnahmeversuch nach einem Timeout
nimmt einen frischen Startzeitstempel und läuft erneut die vollen 9 s, statt
das alte Zeitbudget fortzusetzen — `ALL CHECKS PASSED`, keine
Compiler-Warnungen, keine Sanitizer-Befunde.

Funktionaler Test nach dem Patch, lokal auf dem Flipper: einen beliebigen
Aufnahme-Modus starten ("Analyze Signal" reicht), danach absichtlich **keinen**
Keyfob drücken — nach 9 Sekunden muss die App von selbst zum
"RollLab Research"-Hauptmenü zurückkehren, ohne manuellen Tastendruck.
Regressionsfall: denselben Modus starten und den Keyfob sofort drücken — das
gewohnte Verhalten (Erfassung, Weiterschalten in den jeweiligen
Analyse-/Replay-/Advance-Zustand) muss unverändert bleiben.

## Risiko
- Sehr gering: rein additiv (ein neues Feld, ein zusätzlicher `else if`-Zweig
  in genau einem der acht Hauptschleifen-Fälle). Keine bestehende Bedingung,
  kein bestehender Rückgabewert wird verändert.
- Der Erfolgspfad bleibt durch die `if`/`else if`-Reihenfolge unangetastet —
  durch den Host-Repro-Grenzfall (Szenario 4: Signal und Timeout auf
  demselben Tick) ausdrücklich gegengeprüft.
- `furi_get_tick()`-Überlauf (Wrap nach ca. 49 Tagen Laufzeit) ist durch die
  vorzeichenlose Subtraktion `furi_get_tick() - app->cap_start_tick`
  unproblematisch (Standard-Idiom, liefert auch über die Überlauf-Grenze
  hinweg die korrekte Differenz).
- Keine Änderung an `rolllab_rf.c` (Capture-Start/-Stop/-Callback bleiben
  byte-identisch) — der Patch konsumiert nur den bereits vorhandenen
  `rlab_capture_ref_stop()`-Aufruf ein zweites Mal, an einer neuen Stelle,
  genau wie der bestehende Geräte-Guard es für seinen eigenen Fall bereits
  tut.

## Quelle
- `G4MEOVER18/RollLab` (main, Commit `4ea80481`): `rolllab_app.h`
  (`RLAB_CAP_TIMEOUT_MS`, `RollLabApp`-Struct), `rolllab_app.c`
  (`rolllab_app()`, Fall `RLAB_CAPTURING`).
- Vergleichsmuster für den Rückkehr-Pfad: derselbe `rolllab_app()`, der
  bestehende Geräte-Guard-Block ("ERR: CC1101 not found" →
  `app->state = RLAB_MENU`), unverändert.
