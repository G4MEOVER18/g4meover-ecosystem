// Host repro for: hid_mouse_jiggler_exit_callback() stops the FuriTimer but never resets
// model->running, so re-entering the Mouse Jiggler view shows "Stop" highlighted even though
// jiggling was actually stopped on exit.
//
// This reproduces the exact control flow of hid_mouse_jiggler_input_callback() (the OK-press
// toggle) and hid_mouse_jiggler_exit_callback() from
// applications/system/hid_app/views/hid_mouse_jiggler.c, with furi_timer_start/stop and
// with_view_model replaced by a minimal mock (a struct plus a few tracking counters) since the
// bug is entirely about which flag gets touched by which callback, not about what the real
// Furi HAL timer or view-model lock do internally.
//
// Build:
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
// (also verified at -O0)

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    bool armed;
    int start_calls;
    int stop_calls;
} MockTimer;

static void mock_timer_start(MockTimer* t) {
    t->armed = true;
    t->start_calls++;
}

static void mock_timer_stop(MockTimer* t) {
    t->armed = false;
    t->stop_calls++;
}

typedef struct {
    bool running;
} Model;

typedef struct {
    Model model;
    MockTimer timer;
} HidMouseJiggler;

// --- ALT: exit callback only stops the timer, never touches model->running (the bug) ---
static void alt_exit_callback(HidMouseJiggler* j) {
    mock_timer_stop(&j->timer);
}

// --- NEU: exit callback also resets model->running (this session's fix) ---
static void neu_exit_callback(HidMouseJiggler* j) {
    mock_timer_stop(&j->timer);
    j->model.running = false;
}

// Shared: OK-press toggle, 1:1 the same in both variants (this function is untouched by this
// session's fix; the prior session's separate fix already made Stop synchronously disarm the
// timer here, which this repro also models so the two fixes don't appear to conflict).
static void ok_press(HidMouseJiggler* j, const int intervals[], int interval_idx) {
    j->model.running = !j->model.running;
    mock_timer_stop(&j->timer);
    if(j->model.running) {
        mock_timer_start(&j->timer);
        (void)intervals;
        (void)interval_idx;
    }
}

int main(void) {
    const int intervals[1] = {500};
    int checks = 0;

    // --- group 1: ALT leaves model->running=true after Quit while jiggling was on ---
    {
        HidMouseJiggler j = {0};
        ok_press(&j, intervals, 0); // Start
        assert(j.model.running == true);
        alt_exit_callback(&j); // Quit/Back leaves the view
        assert(j.timer.armed == false); // timer correctly stopped
        assert(j.model.running == true); // BUG: UI will redraw this view with "Stop" highlighted
        printf(
            "[group1] ALT: after Quit while running, timer.armed=%s but model.running=%s "
            "(re-entering the view misleadingly shows \"Stop\")\n",
            j.timer.armed ? "true" : "false",
            j.model.running ? "true" : "false");
        checks++;
    }

    // --- group 2: NEU resets model->running on exit, matching the now-stopped timer ---
    {
        HidMouseJiggler j = {0};
        ok_press(&j, intervals, 0); // Start
        assert(j.model.running == true);
        neu_exit_callback(&j);
        assert(j.timer.armed == false);
        assert(j.model.running == false);
        printf(
            "[group2] NEU: after Quit while running, timer.armed=%s and model.running=%s "
            "(re-entering the view correctly shows \"Start\")\n",
            j.timer.armed ? "true" : "false",
            j.model.running ? "true" : "false");
        checks++;
    }

    // --- group 3: re-entry regression. ALT needs two OK presses to actually resume jiggling
    // after a Quit-while-running (first press only flips the stale flag back to false), NEU
    // needs exactly one. ---
    {
        HidMouseJiggler alt = {0}, neu = {0};
        ok_press(&alt, intervals, 0);
        alt_exit_callback(&alt);
        ok_press(&neu, intervals, 0);
        neu_exit_callback(&neu);

        // Re-enter and press OK once, intending to (re)start jiggling.
        ok_press(&alt, intervals, 0);
        ok_press(&neu, intervals, 0);

        assert(alt.model.running == false && alt.timer.armed == false); // ALT: still not running
        assert(neu.model.running == true && neu.timer.armed == true); // NEU: running as intended

        printf(
            "[group3] one OK press after re-entry: ALT running=%s armed=%s (needs a second "
            "press), NEU running=%s armed=%s (jiggling as the user intended)\n",
            alt.model.running ? "true" : "false",
            alt.timer.armed ? "true" : "false",
            neu.model.running ? "true" : "false",
            neu.timer.armed ? "true" : "false");
        checks++;
    }

    // --- group 4: exiting while NOT running is unaffected (no spurious state change, timer
    // stop stays idempotent/harmless either way). ---
    {
        HidMouseJiggler alt = {0}, neu = {0};
        alt_exit_callback(&alt);
        neu_exit_callback(&neu);
        assert(alt.model.running == false && alt.timer.armed == false);
        assert(neu.model.running == false && neu.timer.armed == false);
        assert(alt.timer.stop_calls == 1 && neu.timer.stop_calls == 1);
        printf("[group4] exit while not running: ALT and NEU agree (running=false, armed=false)\n");
        checks++;
    }

    // --- group 5: regression over many random-ish start/quit/re-enter cycles, NEU keeps
    // model.running in sync with timer.armed at every exit; ALT drifts as soon as any cycle
    // exits while running. ---
    {
        HidMouseJiggler neu = {0};
        int mismatches = 0;
        for(int i = 0; i < 5000; i++) {
            bool want_run = (i % 3) != 0;
            if(want_run && !neu.model.running) ok_press(&neu, intervals, 0);
            if(!want_run && neu.model.running) ok_press(&neu, intervals, 0);
            neu_exit_callback(&neu);
            if(neu.model.running != neu.timer.armed) mismatches++;
            if(neu.model.running != false) mismatches++; // must always be false right after exit
        }
        assert(mismatches == 0);
        printf(
            "[group5] regression over 5000 start/quit cycles: NEU mismatches=%d (expected 0)\n",
            mismatches);
        checks++;
    }

    printf("ALL CHECKS PASSED (%d checks)\n", checks);
    return 0;
}
