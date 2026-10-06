// Host-Repro fuer app-improvements/RollJam/abort-right-key-quits-app-fix.md
//
// Isoliert die Zustandsmaschine aus rolljam_app.c (rj_input_cb() + die
// Abort-Behandlung der Hauptschleife rolljam_app()) auf reine Flag-/
// Zustandsuebergaenge -- keine Canvas-/GUI-/furi-/RF-Aufrufe, da fuer den
// Fund irrelevant (reiner Steuerfluss, kein Timing, kein Hardwarezugriff).
// rj_run_jam_capture_phase() wird NICHT nachgebaut: sie wird hier durch
// run_phase_stub() ersetzt, die genau das tut, was fuer diesen Fund zaehlt
// -- bei gesetztem app->abort sofort zurueckkehren, ohne das Ergebnisfeld
// zu aendern -- und sonst "captured" liefert. Das bildet 1:1 ab, wie die
// echte Funktion sich verhaelt, wenn waehrend ihrer eigenen inneren
// while(!app->abort)-Schleife (rolljam_app.c:153) abgebrochen wird.
//
// ALT ist Zeile fuer Zeile der tatsaechliche, unveraenderte Stand (Commit
// 1f90c00e1): ein einziges app->abort-Flag, Right in PHASE1/2/3_REPLAY
// gleichgesetzt mit Back, Hauptschleife `while(!app->abort)`. NEU ist der
// vorgeschlagene Patch (app->quit zusaetzlich, Right in allen vier
// "Abort"-Bildschirmen inkl. CAPTURED, Hauptschleife loest abort zu
// IDLE/running=false auf statt den Zustand einfach stehen zu lassen).
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
// Run:   ./repro

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum { InputKeyLeft, InputKeyRight, InputKeyOk, InputKeyBack } InputKeyStub;
typedef enum { InputTypeShort, InputTypeLong } InputTypeStub;
typedef struct {
    InputTypeStub type;
    InputKeyStub  key;
} InputEventStub;

typedef enum {
    ROLLJAM_IDLE,
    ROLLJAM_PHASE1_JAMMING,
    ROLLJAM_CAPTURED,
    ROLLJAM_PHASE2_JAMMING,
    ROLLJAM_PHASE3_REPLAY,
    ROLLJAM_DONE,
} RollJamStateStub;

static const char* state_name(RollJamStateStub s) {
    switch(s) {
    case ROLLJAM_IDLE: return "IDLE";
    case ROLLJAM_PHASE1_JAMMING: return "PHASE1_JAMMING";
    case ROLLJAM_CAPTURED: return "CAPTURED";
    case ROLLJAM_PHASE2_JAMMING: return "PHASE2_JAMMING";
    case ROLLJAM_PHASE3_REPLAY: return "PHASE3_REPLAY";
    case ROLLJAM_DONE: return "DONE";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// ALT (rolljam_app.c, Commit 1f90c00e1, Stand vor diesem Patch)
// ---------------------------------------------------------------------------
typedef struct {
    RollJamStateStub state;
    bool             abort;
} AppOld;

static void rj_input_cb_old(InputEventStub* ev, AppOld* app) {
    if(ev->type != InputTypeShort) return;
    if(ev->key == InputKeyBack) { app->abort = true; return; }
    if(ev->key == InputKeyRight) {
        if(app->state == ROLLJAM_PHASE1_JAMMING || app->state == ROLLJAM_PHASE2_JAMMING ||
           app->state == ROLLJAM_PHASE3_REPLAY) {
            app->abort = true;
        }
        return;
    }
    if(ev->key == InputKeyOk) {
        switch(app->state) {
        case ROLLJAM_IDLE: app->state = ROLLJAM_PHASE1_JAMMING; break;
        case ROLLJAM_CAPTURED: app->state = ROLLJAM_PHASE2_JAMMING; break;
        case ROLLJAM_DONE: app->state = ROLLJAM_IDLE; break;
        default: break;
        }
    }
}

// run_phase_stub(): Ersatz fuer rj_run_jam_capture_phase(). "captured"
// haette ohne Abbruch den Wert echt_captured; bei app->abort==true (waehrend
// der Phase gesetzt) kehrt die echte Funktion ueber ihre innere
// while(!app->abort)-Schleife zurueck, OHNE *captured zu setzen -- 1:1
// nachgebildet.
static void run_phase_stub_old(AppOld* app, bool echt_captured, bool* captured) {
    if(app->abort) return; // wie die echte Funktion: sofortiger Rueckzug, *captured unveraendert
    *captured = echt_captured;
}

// Hauptschleife, nur die fuer den Fund relevanten PHASE1/PHASE3-Zweige
// (PHASE2 verhaelt sich analog zu PHASE1, siehe Doku) -- 1:1 Steuerfluss aus
// rolljam_app(), Zeilen 238-321 vor diesem Patch. Der entscheidende Punkt:
// `while(!app->abort)` hat KEINEN Wiederherstellungs-Zweig -- welcher
// Zustand am Ende auch vorliegt, ist irrelevant, sobald app->abort true ist,
// beendet sich die App beim naechsten Schleifendurchlauf unbedingt.
static RollJamStateStub run_old(RollJamStateStub start, InputKeyStub abort_key) {
    AppOld app = {.state = start, .abort = false};

    InputEventStub ev = {.type = InputTypeShort, .key = abort_key};
    rj_input_cb_old(&ev, &app);

    if(app.state == ROLLJAM_PHASE1_JAMMING) {
        bool captured = false;
        run_phase_stub_old(&app, true, &captured); // bei abort==true: no-op, *captured bleibt false
        if(!app.abort) app.state = captured ? ROLLJAM_CAPTURED : ROLLJAM_IDLE;
    } else if(app.state == ROLLJAM_PHASE3_REPLAY) {
        // Die innere Replay-Warteschleife (while(!app->abort && !done)) bricht
        // sofort ab, sobald abort true ist -- ohne Abbruch waere done
        // irgendwann true geworden, hier simuliert als "waere fertig".
        if(!app.abort) app.state = ROLLJAM_DONE;
    }
    // ROLLJAM_CAPTURED: kein Phasen-Koerper (reiner Wartezustand) -- Right
    // steht dort gar nicht in ALTs Zustandsliste, also bleibt abort false
    // und der Zustand unveraendert.

    if(app.abort) return (RollJamStateStub)0xFF; // Sentinel: while(!app->abort) traf zu -> App beendet sich
    return app.state;
}

// ---------------------------------------------------------------------------
// NEU (dieser Patch)
// ---------------------------------------------------------------------------
typedef struct {
    RollJamStateStub state;
    bool             abort;
    bool             quit;
} AppNew;

static void rj_input_cb_new(InputEventStub* ev, AppNew* app) {
    if(ev->type != InputTypeShort) return;
    if(ev->key == InputKeyBack) { app->abort = true; app->quit = true; return; }
    if(ev->key == InputKeyRight) {
        if(app->state == ROLLJAM_PHASE1_JAMMING || app->state == ROLLJAM_CAPTURED ||
           app->state == ROLLJAM_PHASE2_JAMMING || app->state == ROLLJAM_PHASE3_REPLAY) {
            app->abort = true; // quit bleibt false
        }
        return;
    }
    if(ev->key == InputKeyOk) {
        switch(app->state) {
        case ROLLJAM_IDLE: app->state = ROLLJAM_PHASE1_JAMMING; break;
        case ROLLJAM_CAPTURED: app->state = ROLLJAM_PHASE2_JAMMING; break;
        case ROLLJAM_DONE: app->state = ROLLJAM_IDLE; break;
        default: break;
        }
    }
}

static void run_phase_stub_new(AppNew* app, bool echt_captured, bool* captured) {
    if(app->abort) return;
    *captured = echt_captured;
}

// Neue Hauptschleife: abort wird VOR dem switch aufgeloest (IDLE/running=false
// statt den Zustand von der Phase unveraendert stehen zu lassen).
static RollJamStateStub run_new(RollJamStateStub start, InputKeyStub abort_key) {
    AppNew app = {.state = start, .abort = false, .quit = false};

    InputEventStub ev = {.type = InputTypeShort, .key = abort_key};
    rj_input_cb_new(&ev, &app);

    // Phasen-Koerper laeuft (falls zutreffend) wie bisher...
    if(app.state == ROLLJAM_PHASE1_JAMMING && !app.abort) {
        bool captured = false;
        run_phase_stub_new(&app, true, &captured);
        app.state = captured ? ROLLJAM_CAPTURED : ROLLJAM_IDLE;
    } else if(app.state == ROLLJAM_PHASE3_REPLAY && !app.abort) {
        app.state = ROLLJAM_DONE;
    }
    // ...dann die neue Top-of-loop-Abort-Aufloesung, exakt wie im Patch:
    if(app.abort) {
        if(app.quit || app.state == ROLLJAM_IDLE || app.state == ROLLJAM_DONE) {
            return (RollJamStateStub)0xFF; // running = false
        }
        app.state = ROLLJAM_IDLE;
        app.abort = false;
    }
    return app.state;
}

static int failures = 0;
static void check(bool cond, const char* label) {
    printf("%-78s %s\n", label, cond ? "OK" : "FAIL");
    if(!cond) failures++;
}

int main(void) {
    printf("== Fall 1: Right (\"Abort\") waehrend PHASE1_JAMMING ==\n");
    {
        RollJamStateStub old_result = run_old(ROLLJAM_PHASE1_JAMMING, InputKeyRight);
        RollJamStateStub new_result = run_new(ROLLJAM_PHASE1_JAMMING, InputKeyRight);
        char label[160];
        snprintf(label, sizeof(label),
                 "ALT -> %s (App beendet sich!) / NEU -> %s (zurueck ins Menu)",
                 old_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(old_result),
                 new_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(new_result));
        // Der Fund: ALT beendet die App komplett. NEU bleibt im Programm, zurueck zu IDLE.
        check(old_result == (RollJamStateStub)0xFF && new_result == ROLLJAM_IDLE, label);
    }

    printf("\n== Fall 2: Right (\"Abort\") waehrend PHASE3_REPLAY ==\n");
    {
        RollJamStateStub old_result = run_old(ROLLJAM_PHASE3_REPLAY, InputKeyRight);
        RollJamStateStub new_result = run_new(ROLLJAM_PHASE3_REPLAY, InputKeyRight);
        char label[160];
        snprintf(label, sizeof(label),
                 "ALT -> %s (App beendet sich!) / NEU -> %s (zurueck ins Menu)",
                 old_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(old_result),
                 new_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(new_result));
        check(old_result == (RollJamStateStub)0xFF && new_result == ROLLJAM_IDLE, label);
    }

    printf("\n== Fall 3: Right (\"Abort\"-Hinweis wird gezeichnet) waehrend CAPTURED ==\n");
    {
        RollJamStateStub old_result = run_old(ROLLJAM_CAPTURED, InputKeyRight);
        RollJamStateStub new_result = run_new(ROLLJAM_CAPTURED, InputKeyRight);
        char label[160];
        snprintf(label, sizeof(label),
                 "ALT -> %s (Taste wirkungslos) / NEU -> %s (zurueck ins Menu)",
                 old_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(old_result),
                 new_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(new_result));
        // Der zweite Fund: ALT bleibt in CAPTURED stehen (abort war nie gesetzt
        // worden, CAPTURED fehlte in der Right-Zustandsliste) -- der gezeichnete
        // "Abort"-Hinweis tut nichts. NEU bricht ab wie auf jedem anderen
        // Bildschirm mit demselben Hinweis.
        check(old_result == ROLLJAM_CAPTURED && new_result == ROLLJAM_IDLE, label);
    }

    printf("\n== Fall 4: Regression -- Back beendet die App weiterhin vollstaendig ==\n");
    {
        const RollJamStateStub states[] = {
            ROLLJAM_IDLE, ROLLJAM_PHASE1_JAMMING, ROLLJAM_CAPTURED,
            ROLLJAM_PHASE2_JAMMING, ROLLJAM_PHASE3_REPLAY, ROLLJAM_DONE,
        };
        for(size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
            RollJamStateStub old_result = run_old(states[i], InputKeyBack);
            RollJamStateStub new_result = run_new(states[i], InputKeyBack);
            char label[160];
            snprintf(label, sizeof(label),
                     "state=%s: Back -> ALT %s, NEU %s",
                     state_name(states[i]),
                     old_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(old_result),
                     new_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(new_result));
            check(old_result == (RollJamStateStub)0xFF && new_result == (RollJamStateStub)0xFF, label);
        }
    }

    printf("\n== Fall 5: Regression -- Right bleibt in IDLE/DONE wirkungslos (kein neuer Abbruchweg) ==\n");
    {
        const RollJamStateStub states[] = {ROLLJAM_IDLE, ROLLJAM_DONE};
        for(size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
            RollJamStateStub old_result = run_old(states[i], InputKeyRight);
            RollJamStateStub new_result = run_new(states[i], InputKeyRight);
            char label[160];
            snprintf(label, sizeof(label),
                     "state=%s: Right -> ALT %s, NEU %s (beide unveraendert)",
                     state_name(states[i]),
                     old_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(old_result),
                     new_result == (RollJamStateStub)0xFF ? "QUIT" : state_name(new_result));
            check(old_result == states[i] && new_result == states[i], label);
        }
    }

    printf("\n== Fall 6: Erfolgsfall ohne Abbruch bleibt unveraendert (PHASE1 -> CAPTURED) ==\n");
    {
        // Keine Taste gedrueckt -- echt_captured=true, kein abort.
        AppOld old_app = {.state = ROLLJAM_PHASE1_JAMMING, .abort = false};
        bool old_captured = false;
        run_phase_stub_old(&old_app, true, &old_captured);
        old_app.state = old_captured ? ROLLJAM_CAPTURED : ROLLJAM_IDLE;

        AppNew new_app = {.state = ROLLJAM_PHASE1_JAMMING, .abort = false, .quit = false};
        bool new_captured = false;
        run_phase_stub_new(&new_app, true, &new_captured);
        new_app.state = new_captured ? ROLLJAM_CAPTURED : ROLLJAM_IDLE;

        char label[160];
        snprintf(label, sizeof(label), "ALT -> %s, NEU -> %s (beide CAPTURED)",
                 state_name(old_app.state), state_name(new_app.state));
        check(old_app.state == ROLLJAM_CAPTURED && new_app.state == ROLLJAM_CAPTURED, label);
    }

    printf("\n%s (%d Fehlschlaege)\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
