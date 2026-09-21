// Host repro for: HID Mouse Jiggler "Stop" does not actually stop the FuriTimer.
//
// Models exactly the two code paths from hid_mouse_jiggler_input_callback() in
// applications/system/hid_app/views/hid_mouse_jiggler.c (mntm-012), reduced to a mock timer that
// records start()/stop() calls and an "armed" flag instead of the real furi_timer_* HAL calls
// (those are opaque hardware/RTOS primitives; the bug is entirely in which branch calls them, not
// in what they do internally). The model->running flag and the OK-press branching are copied
// verbatim from the real source.
//
// Build & run:
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool armed; // true while the (mock) FuriTimer is running and would keep firing ticks
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
} JigglerModel;

// ALT: hid_mouse_jiggler.c as it stands in my mntm-012 base today (byte-identical to the
// Unleashed pre-8fdb70dd3 state) -- only the "now running" branch touches the timer.
static void alt_ok_press(JigglerModel* model, MockTimer* timer) {
    model->running = !model->running;
    if(model->running) {
        mock_timer_stop(timer);
        mock_timer_start(timer);
    }
}

// NEU: unconditional stop first, start only if now running -- ported fix.
static void neu_ok_press(JigglerModel* model, MockTimer* timer) {
    model->running = !model->running;
    mock_timer_stop(timer);
    if(model->running) {
        mock_timer_start(timer);
    }
}

// Simulates the timer daemon delivering one periodic tick while it is armed. Mirrors
// hid_mouse_jiggler_timer_callback(): it always acquires the view model lock and always runs,
// regardless of model->running (that check happens *inside* the callback, after the lock is
// already taken) -- so every tick while armed is real work done, whether or not it produces a
// mouse move.
static int ticks_serviced_while_armed(MockTimer* timer, int n_ticks) {
    int serviced = 0;
    for(int i = 0; i < n_ticks; i++) {
        if(timer->armed) serviced++;
    }
    return serviced;
}

int main(void) {
    int checks = 0;

    // Group 1: ALT -- press Start, then Stop. The timer must be stopped, but isn't.
    {
        JigglerModel model = {0};
        MockTimer timer = {0};

        alt_ok_press(&model, &timer); // Start
        assert(model.running == true);
        assert(timer.armed == true);
        checks++;

        alt_ok_press(&model, &timer); // Stop
        assert(model.running == false);
        assert(timer.stop_calls == 1); // only the earlier "start" branch's internal stop
        assert(timer.armed == true); // BUG: still armed after Stop
        checks++;

        int wasted = ticks_serviced_while_armed(&timer, 1000);
        assert(wasted == 1000); // 1000 wasted lock-acquire-and-check cycles while "stopped"
        printf(
            "[group1] ALT: after pressing Stop the timer is still armed (armed=%s), "
            "%d/1000 further ticks are serviced into a no-op callback\n",
            timer.armed ? "true" : "false",
            wasted);
        checks++;
    }

    // Group 2: NEU -- same sequence, timer is genuinely halted after Stop.
    {
        JigglerModel model = {0};
        MockTimer timer = {0};

        neu_ok_press(&model, &timer); // Start
        assert(model.running == true);
        assert(timer.armed == true);
        checks++;

        neu_ok_press(&model, &timer); // Stop
        assert(model.running == false);
        assert(timer.armed == false); // FIXED: no longer armed after Stop
        checks++;

        int wasted = ticks_serviced_while_armed(&timer, 1000);
        assert(wasted == 0); // 0 wasted cycles -- the timer is actually stopped
        printf(
            "[group2] NEU: after pressing Stop the timer is armed=%s, "
            "%d/1000 further ticks are serviced (expected 0)\n",
            timer.armed ? "true" : "false",
            wasted);
        checks++;
    }

    // Group 3: regression -- across a long random-ish press sequence, NEU's model->running and
    // timer.armed must always agree (armed iff running), and both variants must agree on the
    // resulting model->running value at every step (the toggle itself is unchanged).
    {
        JigglerModel alt_model = {0}, neu_model = {0};
        MockTimer alt_timer = {0}, neu_timer = {0};
        unsigned seed = 12345;
        int presses = 5000;
        int mismatches_running = 0;
        int neu_armed_running_mismatches = 0;

        for(int i = 0; i < presses; i++) {
            // deterministic pseudo-random OK-press pattern, not every tick is a press
            seed = seed * 1103515245u + 12345u;
            bool press = ((seed >> 16) & 1) == 1;
            if(!press) continue;

            alt_ok_press(&alt_model, &alt_timer);
            neu_ok_press(&neu_model, &neu_timer);

            if(alt_model.running != neu_model.running) mismatches_running++;
            if(neu_timer.armed != neu_model.running) neu_armed_running_mismatches++;
        }

        assert(mismatches_running == 0); // toggle semantics unchanged by the fix
        assert(neu_armed_running_mismatches == 0); // NEU: timer.armed always == model.running
        printf(
            "[group3] regression over %d presses: running-flag mismatches ALT-vs-NEU=%d, "
            "NEU armed!=running mismatches=%d\n",
            presses,
            mismatches_running,
            neu_armed_running_mismatches);
        checks += 2;
    }

    // Group 4: ALT's own invariant is broken -- after an odd number of presses ending on Stop,
    // armed can stay true indefinitely (not just once), i.e. it's not a one-tick artifact.
    {
        JigglerModel model = {0};
        MockTimer timer = {0};
        alt_ok_press(&model, &timer); // Start (armed=true)
        alt_ok_press(&model, &timer); // Stop  (armed stays true -- bug)
        bool still_armed_after_many_more_stops_attempts = timer.armed;
        // Re-affirm: nothing except a Start press ever clears armed in ALT once stuck.
        for(int i = 0; i < 10; i++) {
            ticks_serviced_while_armed(&timer, 1); // ticks alone don't clear it either
        }
        assert(still_armed_after_many_more_stops_attempts == true);
        assert(timer.armed == true);
        printf("[group4] ALT: armed stays stuck true indefinitely once Stop is pressed once\n");
        checks++;
    }

    printf("ALL CHECKS PASSED (%d checks)\n", checks);
    return 0;
}
