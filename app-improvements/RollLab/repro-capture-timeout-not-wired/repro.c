// Host repro for RollLab: RLAB_CAP_TIMEOUT_MS is declared (rolllab_app.h) but never
// enforced in the RLAB_CAPTURING case of rolllab_app() (rolllab_app.c). Without the
// fix, a capture that never reaches RLAB_MIN_EDGES/silence (no keyfob pressed, out of
// range) waits in RLAB_CAPTURING forever with no automatic recovery.
//
// This transcribes only the state-transition slice of the RLAB_CAPTURING case (no
// furi/GUI/RF dependency): the "did a signal complete" check plus, in NEU, the
// elapsed-time check against RLAB_CAP_TIMEOUT_MS, against a fully controllable mock
// clock. ALT is the original logic (no timeout branch at all); NEU adds the
// `cap_start_tick` field and the else-if from the real patch, transcribed verbatim.
//
// Build: gcc -std=c17 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
//        gcc -std=c17 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
// Run:   ./repro

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define RLAB_CAP_TIMEOUT_MS 9000U
#define LOOP_DELAY_MS       20U /* furi_delay_ms(20) at the end of the real case */

typedef enum { ST_CAPTURING, ST_MENU, ST_ANALYZE_VIEW } State;

// -- mock clock, fully controlled by the test, standing in for furi_get_tick() --
static uint32_t g_mock_tick = 0;
static uint32_t mock_get_tick(void) { return g_mock_tick; }

typedef struct {
    State    state;
    bool     rf_capturing;    // app->rf_mode == RLAB_RF_CAPTURING
    uint32_t cap_start_tick;  // NEU only
    bool     signal_ready;    // simulated rlab_capture_ref_done() result, test-controlled
    int      stop_calls;      // counts rlab_capture_ref_stop() invocations
    int      start_calls;     // counts rlab_capture_ref_start() invocations
} App;

static void app_init(App* app) {
    app->state          = ST_CAPTURING;
    app->rf_capturing   = false;
    app->cap_start_tick = 0;
    app->signal_ready   = false;
    app->stop_calls     = 0;
    app->start_calls    = 0;
}

static void rf_start(App* app) {
    app->rf_capturing = true;
    app->start_calls++;
}
static void rf_stop(App* app) {
    app->rf_capturing = false;
    app->stop_calls++;
}

// ALT: rolllab_app.c before the fix -- no timeout branch exists at all.
static void alt_capturing_step(App* app) {
    if(!app->rf_capturing) {
        rf_start(app);
        // (no cap_start_tick in ALT)
    }
    if(app->signal_ready) {
        rf_stop(app);
        app->state = ST_ANALYZE_VIEW;
    }
    // ALT: no else-if -- nothing else can ever move the state off ST_CAPTURING.
}

// NEU: rolllab_app.c/.h after the fix, transcribed verbatim from the patch.
static void neu_capturing_step(App* app) {
    if(!app->rf_capturing) {
        rf_start(app);
        app->cap_start_tick = mock_get_tick();
    }
    if(app->signal_ready) {
        rf_stop(app);
        app->state = ST_ANALYZE_VIEW;
    } else if(mock_get_tick() - app->cap_start_tick >= RLAB_CAP_TIMEOUT_MS) {
        rf_stop(app);
        app->state = ST_MENU;
    }
}

static int g_checks  = 0;
static int g_failures = 0;

#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        g_checks++;                                                     \
        if(!(cond)) {                                                   \
            g_failures++;                                                \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);      \
        }                                                                \
    } while(0)

int main(void) {
    // --- Scenario 1: ALT hangs forever when no signal ever arrives ---
    {
        App app;
        app_init(&app);
        g_mock_tick = 0;
        for(int i = 0; i < 2000; i++) { // 2000 * 20ms = 40s simulated, way past 9s
            alt_capturing_step(&app);
            g_mock_tick += LOOP_DELAY_MS;
        }
        CHECK(app.state == ST_CAPTURING, "ALT: state must still be stuck in CAPTURING after 40s with no signal (demonstrates the hang)");
        CHECK(app.stop_calls == 0, "ALT: rf was never stopped (no recovery path taken)");
    }

    // --- Scenario 2: NEU times out and returns to MENU once the budget is spent ---
    {
        App app;
        app_init(&app);
        g_mock_tick = 0;
        int iters_to_timeout = -1;
        for(int i = 0; i < 2000 && app.state == ST_CAPTURING; i++) {
            neu_capturing_step(&app);
            if(app.state != ST_CAPTURING) iters_to_timeout = i;
            g_mock_tick += LOOP_DELAY_MS;
        }
        CHECK(app.state == ST_MENU, "NEU: state must return to MENU after the timeout with no signal");
        CHECK(app.stop_calls == 1, "NEU: capture must be stopped exactly once on timeout");
        // First iteration i has mock_tick == i*20 at call time; cap_start_tick was set to 0
        // on i==0. Condition fires once tick - 0 >= 9000, i.e. i*20 >= 9000 -> i >= 450.
        CHECK(iters_to_timeout == 450, "NEU: must time out at exactly the 450th iteration (9000ms / 20ms)");
    }

    // --- Scenario 3: NEU regression -- a signal completing well before the timeout
    //     still wins (success branch is the `if`, timeout is the `else if`) ---
    {
        App app;
        app_init(&app);
        g_mock_tick = 0;
        for(int i = 0; i < 25; i++) { // 25 * 20ms = 500ms, well under the 9s budget
            if(i == 25 - 1) app.signal_ready = true; // signal arrives on the last iteration
            neu_capturing_step(&app);
            g_mock_tick += LOOP_DELAY_MS;
            if(app.state != ST_CAPTURING) break;
        }
        CHECK(app.state == ST_ANALYZE_VIEW, "NEU: a completed capture well under the budget must reach ANALYZE_VIEW, not time out");
        CHECK(app.stop_calls == 1, "NEU: exactly one stop call for the success path");
    }

    // --- Scenario 4: NEU boundary -- signal completes on the exact tick the timeout
    //     would also fire; success must still win (if/else-if, not independent checks) ---
    {
        App app;
        app_init(&app);
        g_mock_tick = 0;
        app.cap_start_tick = 0;
        // Advance the mock clock to exactly the timeout boundary before the step that
        // both delivers the signal and would independently satisfy the timeout check.
        g_mock_tick = RLAB_CAP_TIMEOUT_MS;
        app.rf_capturing  = true; // already "capturing" so the step won't reset cap_start_tick
        app.signal_ready  = true;
        neu_capturing_step(&app);
        CHECK(app.state == ST_ANALYZE_VIEW, "NEU: success must win over timeout when both conditions are true on the same iteration");
    }

    // --- Scenario 5: NEU -- restarting a capture after a timeout must restart the
    //     budget from the new start tick, not keep counting from the first attempt ---
    {
        App app;
        app_init(&app);
        g_mock_tick = 0;
        for(int i = 0; i < 2000 && app.state == ST_CAPTURING; i++) {
            neu_capturing_step(&app);
            g_mock_tick += LOOP_DELAY_MS;
        }
        CHECK(app.state == ST_MENU, "NEU setup: first attempt must have timed out before re-entry test");

        // Re-enter RLAB_CAPTURING (as the real app does when the user picks a mode
        // again from RLAB_MENU): rf_mode is no longer RLAB_RF_CAPTURING, so the first
        // `if` branch runs again and must take a fresh cap_start_tick.
        app.state        = ST_CAPTURING;
        app.rf_capturing = false;
        app.signal_ready = false;
        uint32_t tick_at_restart = g_mock_tick; // deliberately far past the old budget

        // Run the fresh attempt to completion with no signal, same cadence as
        // scenario 2 (one neu_capturing_step() per simulated 20ms loop iteration);
        // it must take another full RLAB_CAP_TIMEOUT_MS from the new start tick,
        // not fire immediately from the stale budget of the first attempt.
        int iters = 0;
        while(app.state == ST_CAPTURING && iters < 2000) {
            neu_capturing_step(&app);
            if(iters == 0) {
                CHECK(app.cap_start_tick == tick_at_restart, "NEU: re-entering CAPTURING must take a fresh cap_start_tick, not keep the stale one from the first attempt");
            }
            g_mock_tick += LOOP_DELAY_MS;
            iters++;
        }
        CHECK(app.state == ST_MENU, "NEU: second attempt must also time out on its own budget");
        // 451 calls (indices 0..450): same as scenario 2's iters_to_timeout == 450
        // (a 0-indexed call number), just counted as a 1-indexed call count here.
        CHECK(iters == 451, "NEU: second attempt must take the full 451 calls again, confirming the budget was not inherited from the first attempt");
    }

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED (%d Pruefungen, 0 Fehlschlaege)\n", g_checks);
        return 0;
    } else {
        printf("%d/%d Pruefungen fehlgeschlagen\n", g_failures, g_checks);
        return 1;
    }
}
