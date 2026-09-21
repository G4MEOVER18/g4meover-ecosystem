// Host repro fuer app-improvements/RollJam/exit-button-left-key-unhandled-fix.md
//
// Isoliert die Entscheidungslogik von rj_input_cb() (rolljam_app.c) auf reine
// Zustands-/Flag-Effekte -- Canvas-/GUI-/furi-Aufrufe entfernt, da fuer den
// Fund irrelevant. ALT ist Zeile fuer Zeile die tatsaechliche, unveraenderte
// Funktion (Stand G4MEOVER18/RollJam, Commit 1f90c00e1); NEU ergaenzt exakt
// den vorgeschlagenen InputKeyLeft-Zweig.
//
// Build:  gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
// Run:    ./repro

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    InputKeyUp,
    InputKeyDown,
    InputKeyLeft,
    InputKeyRight,
    InputKeyOk,
    InputKeyBack,
} InputKeyStub;

typedef enum {
    InputTypePress,
    InputTypeRelease,
    InputTypeShort,
    InputTypeLong,
    InputTypeRepeat,
} InputTypeStub;

typedef struct {
    InputTypeStub type;
    InputKeyStub key;
} InputEventStub;

typedef enum {
    ROLLJAM_IDLE,
    ROLLJAM_PHASE1_JAMMING,
    ROLLJAM_CAPTURED,
    ROLLJAM_PHASE2_JAMMING,
    ROLLJAM_PHASE3_REPLAY,
    ROLLJAM_DONE,
} RollJamStateStub;

typedef struct {
    RollJamStateStub state;
    bool abort;
} AppStub;

// ALT: verbatim aus rolljam_app.c, rj_input_cb() (Zeilen 99-135) -- nur die
// GUI-fremden Zustands-/Flag-Effekte behalten, phase_start-Zuweisungen
// entfernt (fuer diesen Fund irrelevant, aendert nichts an der Fragestellung).
static void rj_input_cb_old(InputEventStub* ev, AppStub* app) {
    if(ev->type != InputTypeShort) return;

    if(ev->key == InputKeyBack) {
        app->abort = true;
        return;
    }

    if(ev->key == InputKeyRight) {
        if(app->state == ROLLJAM_PHASE1_JAMMING || app->state == ROLLJAM_PHASE2_JAMMING ||
           app->state == ROLLJAM_PHASE3_REPLAY) {
            app->abort = true;
        }
        return;
    }

    if(ev->key == InputKeyOk) {
        switch(app->state) {
        case ROLLJAM_IDLE:
            app->state = ROLLJAM_PHASE1_JAMMING;
            break;
        case ROLLJAM_CAPTURED:
            app->state = ROLLJAM_PHASE2_JAMMING;
            break;
        case ROLLJAM_DONE:
            app->state = ROLLJAM_IDLE;
            break;
        default:
            break;
        }
    }
}

// NEU: ALT plus dem vorgeschlagenen InputKeyLeft-Zweig (siehe Patch) --
// fasst genau die Zustaende ab, in denen der Bildschirm per
// elements_button_left(canvas, "Exit") den Exit-Hinweis zeichnet.
static void rj_input_cb_new(InputEventStub* ev, AppStub* app) {
    if(ev->type != InputTypeShort) return;

    if(ev->key == InputKeyBack) {
        app->abort = true;
        return;
    }

    if(ev->key == InputKeyLeft) {
        if(app->state == ROLLJAM_IDLE || app->state == ROLLJAM_DONE) {
            app->abort = true;
        }
        return;
    }

    if(ev->key == InputKeyRight) {
        if(app->state == ROLLJAM_PHASE1_JAMMING || app->state == ROLLJAM_PHASE2_JAMMING ||
           app->state == ROLLJAM_PHASE3_REPLAY) {
            app->abort = true;
        }
        return;
    }

    if(ev->key == InputKeyOk) {
        switch(app->state) {
        case ROLLJAM_IDLE:
            app->state = ROLLJAM_PHASE1_JAMMING;
            break;
        case ROLLJAM_CAPTURED:
            app->state = ROLLJAM_PHASE2_JAMMING;
            break;
        case ROLLJAM_DONE:
            app->state = ROLLJAM_IDLE;
            break;
        default:
            break;
        }
    }
}

static const char* state_name(RollJamStateStub s) {
    switch(s) {
    case ROLLJAM_IDLE:
        return "IDLE";
    case ROLLJAM_PHASE1_JAMMING:
        return "PHASE1_JAMMING";
    case ROLLJAM_CAPTURED:
        return "CAPTURED";
    case ROLLJAM_PHASE2_JAMMING:
        return "PHASE2_JAMMING";
    case ROLLJAM_PHASE3_REPLAY:
        return "PHASE3_REPLAY";
    case ROLLJAM_DONE:
        return "DONE";
    }
    return "?";
}

static int failures = 0;

static void check(bool cond, const char* label) {
    printf("%-70s %s\n", label, cond ? "OK" : "FAIL");
    if(!cond) failures++;
}

int main(void) {
    const RollJamStateStub all_states[] = {
        ROLLJAM_IDLE,
        ROLLJAM_PHASE1_JAMMING,
        ROLLJAM_CAPTURED,
        ROLLJAM_PHASE2_JAMMING,
        ROLLJAM_PHASE3_REPLAY,
        ROLLJAM_DONE,
    };
    const size_t n_states = sizeof(all_states) / sizeof(all_states[0]);

    printf("== Fall 1: Left-Taste auf den beiden Bildschirmen mit \"Exit\"-Hinweis ==\n");
    for(size_t i = 0; i < n_states; i++) {
        RollJamStateStub s = all_states[i];
        bool shows_exit_hint = (s == ROLLJAM_IDLE || s == ROLLJAM_DONE);
        if(!shows_exit_hint) continue;

        AppStub app_old = {.state = s, .abort = false};
        AppStub app_new = {.state = s, .abort = false};
        InputEventStub ev = {.type = InputTypeShort, .key = InputKeyLeft};
        rj_input_cb_old(&ev, &app_old);
        rj_input_cb_new(&ev, &app_new);

        char label[128];
        snprintf(
            label,
            sizeof(label),
            "state=%s: ALT abort=%d (Hinweis wirkungslos) / NEU abort=%d",
            state_name(s),
            app_old.abort,
            app_new.abort);
        // Der Fund: ALT bleibt bei "Exit" angezeigtem Bildschirm untaetig (abort=false),
        // NEU setzt abort korrekt, genau wie der bereits vorhandene Back-Zweig es tut.
        check(app_old.abort == false && app_new.abort == true, label);
    }

    printf("\n== Fall 2: Regression -- Left-Taste auf Bildschirmen OHNE \"Exit\"-Hinweis ==\n");
    for(size_t i = 0; i < n_states; i++) {
        RollJamStateStub s = all_states[i];
        bool shows_exit_hint = (s == ROLLJAM_IDLE || s == ROLLJAM_DONE);
        if(shows_exit_hint) continue;

        AppStub app_old = {.state = s, .abort = false};
        AppStub app_new = {.state = s, .abort = false};
        InputEventStub ev = {.type = InputTypeShort, .key = InputKeyLeft};
        rj_input_cb_old(&ev, &app_old);
        rj_input_cb_new(&ev, &app_new);

        char label[128];
        snprintf(
            label,
            sizeof(label),
            "state=%s: ALT abort=%d / NEU abort=%d (beide unveraendert falsch=false)",
            state_name(s),
            app_old.abort,
            app_new.abort);
        check(app_old.abort == false && app_new.abort == false, label);
    }

    printf("\n== Fall 3: Regression -- Back-Taste bleibt in jedem Zustand unveraendert ==\n");
    for(size_t i = 0; i < n_states; i++) {
        RollJamStateStub s = all_states[i];
        AppStub app_old = {.state = s, .abort = false};
        AppStub app_new = {.state = s, .abort = false};
        InputEventStub ev = {.type = InputTypeShort, .key = InputKeyBack};
        rj_input_cb_old(&ev, &app_old);
        rj_input_cb_new(&ev, &app_new);

        char label[128];
        snprintf(
            label,
            sizeof(label),
            "state=%s: Back -> ALT abort=%d, NEU abort=%d",
            state_name(s),
            app_old.abort,
            app_new.abort);
        check(app_old.abort == true && app_new.abort == true, label);
    }

    printf("\n== Fall 4: Regression -- Right-Taste (Abort waehrend aktiver Phasen) unveraendert ==\n");
    for(size_t i = 0; i < n_states; i++) {
        RollJamStateStub s = all_states[i];
        bool should_abort = (s == ROLLJAM_PHASE1_JAMMING || s == ROLLJAM_PHASE2_JAMMING ||
                              s == ROLLJAM_PHASE3_REPLAY);
        AppStub app_old = {.state = s, .abort = false};
        AppStub app_new = {.state = s, .abort = false};
        InputEventStub ev = {.type = InputTypeShort, .key = InputKeyRight};
        rj_input_cb_old(&ev, &app_old);
        rj_input_cb_new(&ev, &app_new);

        char label[128];
        snprintf(
            label,
            sizeof(label),
            "state=%s: Right -> ALT abort=%d, NEU abort=%d (erwartet %d)",
            state_name(s),
            app_old.abort,
            app_new.abort,
            should_abort);
        check(app_old.abort == should_abort && app_new.abort == should_abort, label);
    }

    printf("\n== Fall 5: Regression -- Ok-Taste-Zustandsuebergaenge unveraendert ==\n");
    {
        struct {
            RollJamStateStub from;
            RollJamStateStub to;
        } transitions[] = {
            {ROLLJAM_IDLE, ROLLJAM_PHASE1_JAMMING},
            {ROLLJAM_CAPTURED, ROLLJAM_PHASE2_JAMMING},
            {ROLLJAM_DONE, ROLLJAM_IDLE},
            {ROLLJAM_PHASE1_JAMMING, ROLLJAM_PHASE1_JAMMING}, // default: unveraendert
            {ROLLJAM_PHASE2_JAMMING, ROLLJAM_PHASE2_JAMMING},
            {ROLLJAM_PHASE3_REPLAY, ROLLJAM_PHASE3_REPLAY},
        };
        for(size_t i = 0; i < sizeof(transitions) / sizeof(transitions[0]); i++) {
            AppStub app_old = {.state = transitions[i].from, .abort = false};
            AppStub app_new = {.state = transitions[i].from, .abort = false};
            InputEventStub ev = {.type = InputTypeShort, .key = InputKeyOk};
            rj_input_cb_old(&ev, &app_old);
            rj_input_cb_new(&ev, &app_new);

            char label[128];
            snprintf(
                label,
                sizeof(label),
                "Ok in %s: ALT->%s, NEU->%s (erwartet %s)",
                state_name(transitions[i].from),
                state_name(app_old.state),
                state_name(app_new.state),
                state_name(transitions[i].to));
            check(
                app_old.state == transitions[i].to && app_new.state == transitions[i].to,
                label);
        }
    }

    printf("\n== Fall 6: Regression -- Long-Press/Repeat-Events bleiben ignoriert ==\n");
    {
        AppStub app_old = {.state = ROLLJAM_IDLE, .abort = false};
        AppStub app_new = {.state = ROLLJAM_IDLE, .abort = false};
        InputEventStub ev = {.type = InputTypeLong, .key = InputKeyLeft};
        rj_input_cb_old(&ev, &app_old);
        rj_input_cb_new(&ev, &app_new);
        check(app_old.abort == false && app_new.abort == false, "InputTypeLong wird weiterhin ignoriert (nicht InputTypeShort)");
    }

    printf("\n%s (%d Fehlschlaege)\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
