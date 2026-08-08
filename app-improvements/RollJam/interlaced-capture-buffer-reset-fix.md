# RollJam — Puffer-Reset-Bug in der interlaced Jam+RX-Aufnahme

## Was
Meine RollJam-App fängt Signal A/B in Phase 1/2 über eine "interlaced" Jam+RX-Schleife
ein (`rj_run_jam_capture_phase()` in `rolljam_app.c:141-200`): 180 ms jammen, 80 ms
lauschen, wiederholen, bis entweder ein vollständiges Signal erkannt oder das
9-Sekunden-Timeout erreicht ist.

Jedes Mal, wenn die Schleife von JAMMING zurück nach CAPTURING wechselt (jeder neue
80-ms-RX-Abschnitt), ruft sie `rolljam_capture_start(app, sig)` auf:

```c
if(want_rx) {
    if(app->rf_mode != RF_MODE_CAPTURING) {
        rolljam_jam_stop(app);
        rolljam_capture_start(app, sig);   // <-- bei JEDEM RX-Fenster erneut
    }
}
```

`rolljam_capture_start()` (`rolljam_rf.c:153-162`) setzt dabei unbedingt
`sig->count = 0; sig->ready = false;` — es unterscheidet nicht zwischen "neue Aufnahme
in dieser Phase beginnen" (einmalig sinnvoll) und "Lauschen nach einer Jam-Pause
fortsetzen" (der eigentliche Zweck der interlaced-Schleife). Der 260-ms-Zyklus
(180 ms Jam + 80 ms RX, `RJ_CYCLE_MS`) wird während der ganzen Phase mehrfach
durchlaufen; bei jedem erneuten Eintritt in den RX-Teil verwirft der Aufruf alle in
vorherigen RX-Fenstern bereits empfangenen Flanken.

Ein reales Keyfob-OOK-Signal überschreitet ein einzelnes 80-ms-Fenster häufig
(Präambel + mehrere Frame-Wiederholungen liegen bei vielen Fahrzeugsystemen im
Bereich von 100–300+ ms). Genau für diesen Fall ist die interlaced Jam+RX-Technik
gedacht: Fragmente aus mehreren RX-Fenstern sollen sich zu einem vollständigen Frame
zusammensetzen, während dazwischen gejammt wird. Mit dem aktuellen Reset-Verhalten
zählt aber effektiv nur das *letzte* RX-Fenster vor Abschluss — alles, was in
früheren Fenstern derselben Phase bereits empfangen wurde, ist beim nächsten
`capture_start`-Aufruf weg. Das Ergebnis: Phase 1/2 läuft in vielen Fällen einfach
ins 9-Sekunden-Timeout, weil `sig->count` nie über das hinauswächst, was ein
einzelnes 80-ms-Fenster zufällig eingefangen hat, statt sich über die Phase hinweg
aufzubauen.

`rolljam_capture_stop()` verschärft das zusätzlich: Beim Wechsel zurück zu JAMMING
setzt es `s_cap_sig = NULL` (`rolljam_rf.c:164-169`) — dieselbe Funktion wird sowohl
für "Aufnahme in dieser Phase endgültig beenden" (Timeout/Erfolg/Abort) als auch für
"kurz pausieren, weil als Nächstes gejammt wird" verwendet. Es gibt in der aktuellen
API keine Unterscheidung zwischen einem endgültigen Stop und einer Pause innerhalb
derselben laufenden Aufnahme.

## Nutzen
- Die interlaced Jam+RX-Technik funktioniert danach so, wie sie im README beschrieben
  ist und wie das Timing-Design (180 ms Jam / 80 ms RX) es nahelegt: Flanken aus
  mehreren RX-Fenstern akkumulieren sich innerhalb einer Phase, statt bei jedem
  Fenster-Wechsel verworfen zu werden.
- Höhere Erfolgsquote pro Phase innerhalb des bestehenden 9-Sekunden-Timeouts,
  besonders bei Fahrzeugen mit längeren Frames (Präambel + Wiederholungen) — genau
  der Fall, den die Interlacing-Technik überhaupt adressieren soll.
- Reine Zustands-/Pufferverwaltung in der Capture-Steuerung, keine Änderung an der
  eigentlichen RF-Konfiguration (Preset/Frequenz/Callback bleiben gleich) und keine
  Änderung an Phase 3 (Replay) oder der GUI/State-Machine in `rj_draw_cb`/`rj_input_cb`.

## Betroffene Dateien
- `rolljam_rf.h` — zwei neue Funktionsdeklarationen (`rolljam_capture_pause`,
  `rolljam_capture_resume`), bestehende Signaturen unverändert.
- `rolljam_rf.c` — `rolljam_capture_start()` implementiert über die neue
  `rolljam_capture_resume()`; zwei neue Funktionen für Pause/Resume ohne
  Pufferverlust. `rolljam_capture_stop()` unverändert (bleibt der "endgültige Stop").
- `rolljam_app.c` — `rj_run_jam_capture_phase()`: unterscheidet jetzt zwischen dem
  einmaligen initialen `rolljam_capture_start()` und den wiederholten
  Pause/Resume-Aufrufen bei jedem Zyklus-Wechsel.

## Port-Skizze / Patch

### `rolljam_rf.h`
```c
// Raw signal capture via async RX
void rolljam_capture_start(RollJamApp* app, RollJamSignal* sig);
void rolljam_capture_pause(RollJamApp* app);   // RX-Hardware anhalten, Puffer behalten
void rolljam_capture_resume(RollJamApp* app);  // RX-Hardware fortsetzen, Puffer behalten
void rolljam_capture_stop(RollJamApp* app);    // endgueltig: Puffer/State freigeben
bool rolljam_capture_done(const RollJamSignal* sig);
```

### `rolljam_rf.c`
```c
void rolljam_capture_start(RollJamApp* app, RollJamSignal* sig) {
    sig->count = 0;
    sig->ready = false;
    s_cap_sig = sig;
    rolljam_capture_resume(app);
}

// RX-Hardware anhalten, ohne die laufende Aufnahme aufzugeben: Puffer (sig->count/
// ready) und s_cap_sig bleiben unveraendert -- wird beim Wechsel zurueck zu JAMMING
// innerhalb derselben Phase verwendet.
void rolljam_capture_pause(RollJamApp* app) {
    if(app->device && app->rf_mode == RF_MODE_CAPTURING)
        subghz_devices_stop_async_rx(app->device);
    app->rf_mode = RF_MODE_IDLE;
}

// RX-Hardware nach einer Jam-Pause wieder scharf schalten, ohne den Puffer
// zurueckzusetzen -- Fortsetzung derselben Aufnahme, kein Neustart.
void rolljam_capture_resume(RollJamApp* app) {
    rj_stop_current(app);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_start_async_rx(app->device, rj_rx_cb, NULL);
    app->rf_mode = RF_MODE_CAPTURING;
}

void rolljam_capture_stop(RollJamApp* app) {
    s_cap_sig = NULL;
    if(app->device && app->rf_mode == RF_MODE_CAPTURING)
        subghz_devices_stop_async_rx(app->device);
    app->rf_mode = RF_MODE_IDLE;
}
```
(`rolljam_capture_done()` bleibt unverändert.)

### `rolljam_app.c` — `rj_run_jam_capture_phase()`
```c
static bool rj_run_jam_capture_phase(
    RollJamApp* app,
    RollJamSignal* sig,
    ViewPort* vp,
    NotificationApp* notif) {

    app->phase_start = (uint32_t)furi_get_tick();
    sig->count = 0;
    sig->ready = false;
    app->rf_mode = RF_MODE_IDLE;
    bool capture_started = false; // true, sobald die Aufnahme dieser Phase begonnen hat

    while(!app->abort) {
        uint32_t elapsed = (uint32_t)furi_get_tick() - app->phase_start;

        if(elapsed >= ROLLJAM_CAPTURE_TIMEOUT_MS) {
            snprintf(app->status, sizeof(app->status), "Timeout");
            break;
        }

        bool want_rx = ((elapsed % RJ_CYCLE_MS) >= ROLLJAM_JAM_BURST_MS);

        if(want_rx) {
            if(app->rf_mode != RF_MODE_CAPTURING) {
                rolljam_jam_stop(app);
                if(!capture_started) {
                    rolljam_capture_start(app, sig); // einmalig: Puffer initialisieren
                    capture_started = true;
                } else {
                    rolljam_capture_resume(app);     // Fortsetzung: Puffer bleibt erhalten
                }
            }
        } else {
            if(app->rf_mode != RF_MODE_JAMMING) {
                rolljam_capture_pause(app); // RX pausieren, Puffer NICHT verwerfen
                rolljam_jam_start(app);
            }
        }

        uint32_t remain_s = (ROLLJAM_CAPTURE_TIMEOUT_MS - elapsed + 999) / 1000;
        snprintf(app->status, sizeof(app->status),
                 "Listening... %lus  [%zu edges]",
                 (unsigned long)remain_s,
                 (size_t)sig->count);

        if(rolljam_capture_done(sig)) {
            rolljam_capture_stop(app);
            rolljam_jam_stop(app);
            notification_message(notif, &sequence_success);
            snprintf(app->status, sizeof(app->status),
                     "OK  %zu edges", (size_t)sig->count);
            return true;
        }

        view_port_update(vp);
        furi_delay_ms(12);
    }

    rolljam_capture_stop(app);
    rolljam_jam_stop(app);
    return false;
}
```

Einzige Verhaltensänderung: Innerhalb einer Phase wird der Signalpuffer nur beim
allerersten Eintritt in den RX-Zustand zurückgesetzt (`rolljam_capture_start`). Jeder
weitere Wechsel JAM→RX ruft `rolljam_capture_resume()` auf (Hardware neu scharf
schalten, Puffer unangetastet), jeder Wechsel RX→JAM ruft `rolljam_capture_pause()`
auf (Hardware anhalten, Puffer unangetastet). Erst am Phasenende (Erfolg, Timeout
oder Abort) räumt `rolljam_capture_stop()` endgültig auf. Phase 3 (Replay) und die
State-Machine/GUI sind unverändert.

## Testhinweis (ohne Hardware)
Reine Zustands-/Pufferverwaltung, host-kompilierbar nachvollziehbar. Das folgende
Programm bildet nur das für den Bug relevante Verhalten auf Fensterebene nach — wie
viele Flanken nach zwei aufeinanderfolgenden RX-Fenstern im Puffer stehen, je
nachdem ob bei jedem Fenstereintritt zurückgesetzt wird (alt) oder nur beim ersten
Eintritt der Phase (neu). Es reproduziert nicht die volle RF-Timing-/Interrupt-Kette
(die bleibt Hardware-Sache), sondern isoliert exakt die Reset-vs-Resume-Logik, die
der Patch ändert:

```c
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#define MIN_EDGES 16
#define SIG_BUF   2048

typedef struct {
    size_t count;
    bool   ready;
} Sig;

// Vorher: rolljam_capture_start() wird bei JEDEM RX-Fenster aufgerufen -> Reset.
static void old_capture_start(Sig* sig) {
    sig->count = 0;
    sig->ready = false;
}

// Nachher: nur beim allerersten RX-Fenster der Phase resetten (rolljam_capture_start),
// danach rolljam_capture_resume() -- Puffer bleibt erhalten.
static void new_capture_start(Sig* sig, bool first_entry) {
    if(first_entry) {
        sig->count = 0;
        sig->ready = false;
    }
}

static void feed_window(Sig* sig, size_t edges_in_window, bool trailing_silence) {
    for(size_t i = 0; i < edges_in_window && sig->count < SIG_BUF; i++) sig->count++;
    if(trailing_silence && sig->count >= MIN_EDGES) sig->ready = true;
}

int main(void) {
    Sig old_sig = {0}, new_sig = {0};

    // Fenster 1: 10 Flanken kommen an, dann unterbricht der Jam-Burst (keine Stille).
    old_capture_start(&old_sig);
    feed_window(&old_sig, 10, false);
    new_capture_start(&new_sig, true);
    feed_window(&new_sig, 10, false);
    printf("nach Fenster 1: alt count=%zu ready=%d | neu count=%zu ready=%d\n",
           old_sig.count, old_sig.ready, new_sig.count, new_sig.ready);

    // Fenster 2: weitere 10 Flanken + Stille (Signalende), nach erneutem RX-Eintritt.
    old_capture_start(&old_sig);   // BUG: verwirft die 10 Flanken aus Fenster 1
    feed_window(&old_sig, 10, true);
    new_capture_start(&new_sig, false);
    feed_window(&new_sig, 10, true);
    printf("nach Fenster 2: alt count=%zu ready=%d | neu count=%zu ready=%d\n",
           old_sig.count, old_sig.ready, new_sig.count, new_sig.ready);

    return 0;
}
```

Erwartete Ausgabe:
```
nach Fenster 1: alt count=10 ready=0 | neu count=10 ready=0
nach Fenster 2: alt count=10 ready=0 | neu count=20 ready=1
```

Nach Fenster 2 zeigt sich der Unterschied: "alt" verwirft beim erneuten
`capture_start`-Aufruf die 10 Flanken aus Fenster 1 und bleibt bei 10 (< 16
`MIN_EDGES`) — der Capture wird nicht als fertig erkannt, obwohl das Signal insgesamt
bereits 20 Flanken mit korrektem Silence-Trailer geliefert hätte. "Neu" akkumuliert
korrekt auf 20 Flanken und erkennt den Abschluss (`ready=1`).

Funktionaler Test danach lokal auf dem Flipper (eigenes Testfahrzeug/eigener
Empfänger, autorisiert): Phase 1 mit einem Keyfob starten, dessen Frame bekanntlich
länger als 80 ms ist (z. B. per SubGHz-Read-Vergleich vorher gemessen); vor dem Fix
läuft die Phase bei solchen Signalen deutlich häufiger ins Timeout, nach dem Fix
sollte "OK N edges" mit einer Flankenzahl erscheinen, die plausibel über das hinausgeht,
was ein einzelnes 80-ms-Fenster liefern könnte.

## Risiko
- Niedrig: Die Änderung bleibt vollständig innerhalb der Capture-Steuerung
  (`rolljam_rf.c`/`rj_run_jam_capture_phase`). Kein Eingriff in Preset/Frequenz/
  RX-Callback, keine neue globale State, keine Änderung an Phase 3 (Replay) oder an
  `rolljam_jam_start/stop`.
- `rolljam_capture_stop()` (der "endgültige" Stop) bleibt exakt so, wie er war, und
  wird weiterhin nur am Phasenende aufgerufen (Erfolg/Timeout/Abort) — dieselben
  Aufrufstellen wie vorher, keine neue Semantik dort.
- Einzig denkbare Nebenwirkung: Eine Phase, die vorher zufällig (weil das komplette
  Signal in ein einzelnes 80-ms-Fenster passte) bereits erfolgreich war, verhält sich
  unverändert — der Fix ändert nichts an bereits funktionierenden Einzel-Fenster-Fällen,
  er behebt ausschließlich das Verwerfen von Mehr-Fenster-Fällen.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko: Der Fix verbessert nur die
  Zuverlässigkeit der bereits vorhandenen, im README als "authorized testing only"
  deklarierten Forschungs-Funktion — keine neue Fähigkeit, kein verändertes
  Ziel-/Frequenzverhalten.

## Quelle
- `G4MEOVER18/RollJam` (main): `rolljam_app.c` (`rj_run_jam_capture_phase`,
  `RJ_CYCLE_MS`), `rolljam_rf.c` (`rolljam_capture_start/stop`, `s_cap_sig`),
  `rolljam_app.h` (`ROLLJAM_JAM_BURST_MS`, `ROLLJAM_RX_WINDOW_MS`,
  `ROLLJAM_CAPTURE_TIMEOUT_MS`, `ROLLJAM_MIN_EDGES`, `ROLLJAM_SILENCE_US`).
