// Host-Repro fuer app-improvements/RollJam/exit-button-left-key-unhandled-fix.md
// (Stand nach dem bereits gelieferten abort-right-key-quits-app-fix, auf dem
// dieser Patch aufbaut -- app->quit existiert bereits, Right setzt es
// bewusst NICHT, nur Back tut das).
//
// Isoliert erneut nur die reine Flag-/Zustandsuebergangslogik aus
// rj_input_cb() plus die Abort-Aufloesung der Hauptschleife -- keine Canvas-/
// GUI-/furi-/RF-Aufrufe, da fuer diesen Fund irrelevant.
//
// ALT ist der Stand direkt nach dem abort-right-key-quits-app-fix (Left wird
// in rj_input_cb() nirgends ausgewertet -- der gezeichnete "Exit"-Hinweis auf
// IDLE/DONE ist wirkungslos, nur die Hardware-Back-Taste beendet die App).
// NEU ist dieser Patch: InputKeyLeft setzt auf IDLE/DONE sowohl abort als
// auch quit.
//
// Der Fund selbst (Faelle 1/2): ALT wertet InputKeyLeft gar nicht aus, der
// gezeichnete "Exit"-Hinweis auf IDLE/DONE tut buchstaeblich nichts; NEU
// beendet die App wie versprochen. Zusaetzlich pruefen Faelle 1b/2b das
// quit-Flag direkt nach rj_input_cb(): NEU setzt es bewusst zusammen mit
// abort (gleiche Semantik wie Back, "wirklich beenden" statt nur "Phase
// abbrechen") -- auch wenn die Hauptschleife den End-Zustand fuer Left schon
// allein ueber ihren state==IDLE/DONE-Teil der ODER-Bedingung korrekt
// aufloesen wuerde, haelt das explizite quit=true dieses Verhalten davon
// unabhaengig, sollte diese ODER-Bedingung kuenftig einmal auf reines quit
// umgestellt werden.
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

typedef struct {
    RollJamStateStub state;
    bool             abort;
    bool             quit;
} App;

#define QUIT_SENTINEL ((RollJamStateStub)0xFF)

// Gemeinsame Hauptschleifen-Aufloesung (identisch ALT/NEU -- Stand nach dem
// abort-right-key-quits-app-fix, von diesem Patch nicht veraendert):
// rolljam_app.c, Top-of-loop-Block.
static RollJamStateStub resolve_abort(App app) {
    if(app.abort) {
        if(app.quit || app.state == ROLLJAM_IDLE || app.state == ROLLJAM_DONE) {
            return QUIT_SENTINEL;
        }
        return ROLLJAM_IDLE;
    }
    return app.state;
}

// ---------------------------------------------------------------------------
// ALT (Stand nach abort-right-key-quits-app-fix, vor diesem Patch):
// InputKeyLeft wird nirgends ausgewertet.
// ---------------------------------------------------------------------------
static App rj_input_cb_old(InputKeyStub key, RollJamStateStub start) {
    App app = {.state = start, .abort = false, .quit = false};
    if(key == InputKeyBack) { app.abort = true; app.quit = true; return app; }
    if(key == InputKeyRight) {
        if(app.state == ROLLJAM_PHASE1_JAMMING || app.state == ROLLJAM_CAPTURED ||
           app.state == ROLLJAM_PHASE2_JAMMING || app.state == ROLLJAM_PHASE3_REPLAY) {
            app.abort = true; // quit bleibt false
        }
        return app;
    }
    // InputKeyLeft: kein Zweig -- fuehrt zu nichts (Fund).
    return app;
}

// ---------------------------------------------------------------------------
// NEU (dieser Patch): InputKeyLeft auf IDLE/DONE setzt abort UND quit.
// ---------------------------------------------------------------------------
static App rj_input_cb_new(InputKeyStub key, RollJamStateStub start) {
    App app = {.state = start, .abort = false, .quit = false};
    if(key == InputKeyBack) { app.abort = true; app.quit = true; return app; }
    if(key == InputKeyLeft) {
        if(app.state == ROLLJAM_IDLE || app.state == ROLLJAM_DONE) {
            app.abort = true;
            app.quit  = true;
        }
        return app;
    }
    if(key == InputKeyRight) {
        if(app.state == ROLLJAM_PHASE1_JAMMING || app.state == ROLLJAM_CAPTURED ||
           app.state == ROLLJAM_PHASE2_JAMMING || app.state == ROLLJAM_PHASE3_REPLAY) {
            app.abort = true; // quit bleibt false
        }
        return app;
    }
    return app;
}

static int failures = 0;
static void check(bool cond, const char* label) {
    printf("%-78s %s\n", label, cond ? "OK" : "FAIL");
    if(!cond) failures++;
}

int main(void) {
    printf("== Fall 1: Left (\"Exit\"-Hinweis) auf IDLE -- End-Zustand ==\n");
    {
        App old_app = rj_input_cb_old(InputKeyLeft, ROLLJAM_IDLE);
        App new_app = rj_input_cb_new(InputKeyLeft, ROLLJAM_IDLE);
        RollJamStateStub old_result = resolve_abort(old_app);
        RollJamStateStub new_result = resolve_abort(new_app);
        char label[180];
        snprintf(label, sizeof(label),
                 "ALT -> %s (Exit-Hinweis wirkungslos) / NEU -> %s (App beendet sich)",
                 old_result == QUIT_SENTINEL ? "QUIT" : state_name(old_result),
                 new_result == QUIT_SENTINEL ? "QUIT" : state_name(new_result));
        check(old_result == ROLLJAM_IDLE && new_result == QUIT_SENTINEL, label);
    }

    printf("\n== Fall 1b: Left auf IDLE -- quit-Flag direkt nach rj_input_cb() (der eigentliche Fund) ==\n");
    {
        App old_app = rj_input_cb_old(InputKeyLeft, ROLLJAM_IDLE);
        App new_app = rj_input_cb_new(InputKeyLeft, ROLLJAM_IDLE);
        char label[180];
        snprintf(label, sizeof(label),
                 "ALT: abort=%d quit=%d (quit nie gesetzt) / NEU: abort=%d quit=%d",
                 old_app.abort, old_app.quit, new_app.abort, new_app.quit);
        check(old_app.abort == false && old_app.quit == false &&
              new_app.abort == true && new_app.quit == true, label);
    }

    printf("\n== Fall 2: Left auf DONE -- End-Zustand ==\n");
    {
        App old_app = rj_input_cb_old(InputKeyLeft, ROLLJAM_DONE);
        App new_app = rj_input_cb_new(InputKeyLeft, ROLLJAM_DONE);
        RollJamStateStub old_result = resolve_abort(old_app);
        RollJamStateStub new_result = resolve_abort(new_app);
        char label[180];
        snprintf(label, sizeof(label),
                 "ALT -> %s (Exit-Hinweis wirkungslos) / NEU -> %s (App beendet sich)",
                 old_result == QUIT_SENTINEL ? "QUIT" : state_name(old_result),
                 new_result == QUIT_SENTINEL ? "QUIT" : state_name(new_result));
        check(old_result == ROLLJAM_DONE && new_result == QUIT_SENTINEL, label);
    }

    printf("\n== Fall 2b: Left auf DONE -- quit-Flag direkt nach rj_input_cb() ==\n");
    {
        App old_app = rj_input_cb_old(InputKeyLeft, ROLLJAM_DONE);
        App new_app = rj_input_cb_new(InputKeyLeft, ROLLJAM_DONE);
        char label[180];
        snprintf(label, sizeof(label),
                 "ALT: abort=%d quit=%d / NEU: abort=%d quit=%d",
                 old_app.abort, old_app.quit, new_app.abort, new_app.quit);
        check(old_app.abort == false && old_app.quit == false &&
              new_app.abort == true && new_app.quit == true, label);
    }

    printf("\n== Fall 3: Regression -- Left bleibt in den vier aktiven Phasen wirkungslos ==\n");
    {
        const RollJamStateStub states[] = {
            ROLLJAM_PHASE1_JAMMING, ROLLJAM_CAPTURED, ROLLJAM_PHASE2_JAMMING, ROLLJAM_PHASE3_REPLAY,
        };
        for(size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
            App old_app = rj_input_cb_old(InputKeyLeft, states[i]);
            App new_app = rj_input_cb_new(InputKeyLeft, states[i]);
            char label[180];
            snprintf(label, sizeof(label),
                     "state=%s: Left -> ALT abort=%d quit=%d, NEU abort=%d quit=%d (beide unveraendert)",
                     state_name(states[i]), old_app.abort, old_app.quit, new_app.abort, new_app.quit);
            check(old_app.abort == false && old_app.quit == false &&
                  new_app.abort == false && new_app.quit == false, label);
        }
    }

    printf("\n== Fall 4: Regression -- Back bleibt in allen sechs Zustaenden unveraendert (abort=quit=true) ==\n");
    {
        const RollJamStateStub states[] = {
            ROLLJAM_IDLE, ROLLJAM_PHASE1_JAMMING, ROLLJAM_CAPTURED,
            ROLLJAM_PHASE2_JAMMING, ROLLJAM_PHASE3_REPLAY, ROLLJAM_DONE,
        };
        for(size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
            App old_app = rj_input_cb_old(InputKeyBack, states[i]);
            App new_app = rj_input_cb_new(InputKeyBack, states[i]);
            char label[180];
            snprintf(label, sizeof(label),
                     "state=%s: Back -> ALT abort=%d quit=%d, NEU abort=%d quit=%d",
                     state_name(states[i]), old_app.abort, old_app.quit, new_app.abort, new_app.quit);
            check(old_app.abort == true && old_app.quit == true &&
                  new_app.abort == true && new_app.quit == true, label);
        }
    }

    printf("\n== Fall 5: Regression -- Right bleibt je Zustand identisch (abort gesetzt, quit immer false) ==\n");
    {
        const RollJamStateStub states[] = {
            ROLLJAM_IDLE, ROLLJAM_PHASE1_JAMMING, ROLLJAM_CAPTURED,
            ROLLJAM_PHASE2_JAMMING, ROLLJAM_PHASE3_REPLAY, ROLLJAM_DONE,
        };
        for(size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
            App old_app = rj_input_cb_old(InputKeyRight, states[i]);
            App new_app = rj_input_cb_new(InputKeyRight, states[i]);
            char label[180];
            snprintf(label, sizeof(label),
                     "state=%s: Right -> ALT abort=%d quit=%d, NEU abort=%d quit=%d",
                     state_name(states[i]), old_app.abort, old_app.quit, new_app.abort, new_app.quit);
            check(old_app.abort == new_app.abort && old_app.quit == new_app.quit &&
                  new_app.quit == false, label);
        }
    }

    printf("\n%s (%d Fehlschlaege)\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
