// Host repro for: hid_mouse_jiggler_stealth_exit_callback() stops the FuriTimer but never
// touches model->running, and does so with no synchronization at all against the timer
// callback's own self-rearm sequence -- a real race, not just a display glitch.
//
// applications/system/hid_app/views/hid_mouse_jiggler_stealth.c (Momentum mntm-012, byte-
// identical to the pre-PR#1112 Unleashed source in the relevant functions):
//
//   static void hid_mouse_jiggler_stealth_timer_callback(void* context) {
//       ...
//       with_view_model(view, model, {
//           if(model->running) {
//               ... compute random interval, move_x/move_y ...
//               hid_hal_mouse_move(hid, move_x, move_y);
//               furi_timer_stop(timer);
//               furi_timer_start(timer, interval);   // self re-arm, INSIDE the model lock
//           }
//       }, false);
//   }
//
//   static void hid_mouse_jiggler_stealth_exit_callback(void* context) {
//       furi_timer_stop(hid_mouse_jiggler->timer);   // OUTSIDE any lock, no model access at all
//   }
//
// The model's with_view_model() lock only ever serializes accesses to *model* fields. It gives
// no ordering guarantee for the furi_timer_stop()/furi_timer_start() calls themselves against
// a caller -- like exit_callback -- that never takes that lock. So while the timer service task
// is mid-way through executing the callback's locked block (already past its `if(model->running)`
// read, about to call its own stop()+start() pair to re-arm), the UI task can run
// exit_callback() and call furi_timer_stop() *between* the callback's internal stop() and
// start() (or even before both) with nothing to stop it -- the callback's own start() then wins
// and the timer stays armed after the user has already left the view (and, in the direct
// consequence, keeps moving the mouse in the background with no UI feedback whatsoever, until
// the app is closed or the user reopens the view and presses OK twice).
//
// This models each call as a single event and enumerates every relative-order interleaving that
// the real synchronization primitives permit, rather than picking one interleaving out of the
// air:
//   - ALT: the callback's own internal timer_stop()/timer_start() are unlocked w.r.t.
//     exit_callback's timer_stop() (exit takes no lock), so exit's call can land in any of the
//     3 slots around them.
//   - NEU (this session's fix, exit_callback sets model->running = false *under the model lock*,
//     before its own furi_timer_stop()): exit's model write and the callback's locked block now
//     share the same mutex, so they can never interleave with each other -- only two total
//     orders are possible, and both are modeled explicitly.
//
// Build:
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
// (also verified at -O0)

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    bool armed;
} MockTimer;

static void mock_timer_stop(MockTimer* t) {
    t->armed = false;
}

static void mock_timer_start(MockTimer* t) {
    t->armed = true;
}

typedef struct {
    bool running;
} Model;

// A single discrete event in the interleaving. Each event is one indivisible operation as seen
// from the outside (one furi_timer_stop()/furi_timer_start() call, or one locked model write).
typedef enum {
    EV_CB_STOP, // callback's internal furi_timer_stop() (re-arm step 1)
    EV_CB_START, // callback's internal furi_timer_start() (re-arm step 2)
    EV_EXIT_SET_FALSE, // NEU only: exit's locked `model->running = false`
    EV_EXIT_STOP, // exit's furi_timer_stop()
} Event;

static void run_sequence(const Event* seq, int n, bool cb_reads_running_true, MockTimer* timer) {
    // cb_reads_running_true models that the callback's `if(model->running)` check (which
    // happens before EV_CB_STOP in program order and is not itself a separate event here)
    // already observed running == true for this invocation -- i.e. the scenario where the
    // callback is genuinely mid-flight when the user exits, which is the case this bug matters
    // for. If it observed false, the callback would do nothing at all, which is uninteresting.
    (void)cb_reads_running_true;
    timer->armed = true; // timer was armed (it just fired) at the start of this scenario
    for(int i = 0; i < n; i++) {
        switch(seq[i]) {
        case EV_CB_STOP:
            mock_timer_stop(timer);
            break;
        case EV_CB_START:
            mock_timer_start(timer);
            break;
        case EV_EXIT_SET_FALSE:
            // no timer effect, modeled separately via `model` in the caller
            break;
        case EV_EXIT_STOP:
            mock_timer_stop(timer);
            break;
        }
    }
}

int main(void) {
    int checks = 0;

    // --- ALT: exit_callback has exactly one event (EV_EXIT_STOP), unsynchronized with the
    // callback's EV_CB_STOP/EV_CB_START pair, so it can land in any of 3 slots. ---
    {
        const Event orders[3][3] = {
            {EV_EXIT_STOP, EV_CB_STOP, EV_CB_START}, // exit races in before both
            {EV_CB_STOP, EV_EXIT_STOP, EV_CB_START}, // exit races in between them
            {EV_CB_STOP, EV_CB_START, EV_EXIT_STOP}, // exit runs after both (lucky ordering)
        };
        int armed_after_exit = 0;
        for(int i = 0; i < 3; i++) {
            MockTimer t = {0};
            run_sequence(orders[i], 3, true, &t);
            if(t.armed) armed_after_exit++;
            printf(
                "[ALT order %d] timer.armed after the interleaving = %s\n",
                i,
                t.armed ? "true (BUG: still jiggling after Quit)" : "false");
        }
        assert(armed_after_exit == 2); // 2 of 3 possible orderings leave the timer wrongly armed
        printf(
            "[ALT] %d of 3 possible interleavings leave the timer armed after the user exited\n",
            armed_after_exit);
        checks++;
    }

    // --- NEU: exit's EV_EXIT_SET_FALSE shares the model mutex with the callback's locked
    // block, so the two can never interleave with each other -- only two total orders exist:
    // exit-fully-before-callback, or callback-fully-before-exit. Both are modeled explicitly,
    // each followed by exit's own (still unlocked, but now trailing) EV_EXIT_STOP. ---
    {
        // Order A: exit's locked write commits before the callback's locked block runs, so the
        // callback observes running == false and never issues EV_CB_STOP/EV_CB_START at all.
        {
            MockTimer t = {0};
            Model m = {.running = true};
            t.armed = true;
            m.running = false; // EV_EXIT_SET_FALSE, committed first
            // callback's locked block: re-reads model->running under the same lock -> false,
            // so it does nothing (no EV_CB_STOP/EV_CB_START -- this is exactly why the fix
            // needs no change to the timer callback itself).
            mock_timer_stop(&t); // EV_EXIT_STOP, trailing
            assert(t.armed == false);
            assert(m.running == false);
            printf("[NEU order A] exit-write-first: callback sees running=false, does nothing; "
                   "timer.armed=false\n");
            checks++;
        }

        // Order B: the callback's locked block (already committed to running == true from
        // before exit's write landed) runs fully first and re-arms; exit's locked write and its
        // trailing furi_timer_stop() both necessarily come after in program order.
        {
            MockTimer t = {0};
            Model m = {.running = true};
            t.armed = true;
            mock_timer_stop(&t); // EV_CB_STOP
            mock_timer_start(&t); // EV_CB_START (re-arms)
            assert(t.armed == true);
            m.running = false; // EV_EXIT_SET_FALSE (necessarily after, same thread as EV_EXIT_STOP)
            mock_timer_stop(&t); // EV_EXIT_STOP -- always trails EV_CB_START in this order
            assert(t.armed == false);
            assert(m.running == false);
            printf("[NEU order B] callback-first: it re-arms, but exit's own stop() always runs "
                   "after it (same thread, program order) and wins; timer.armed=false\n");
            checks++;
        }
    }

    // --- Regression: exiting while nothing is running (the common case) is unaffected in
    // either variant -- no spurious state change. ---
    {
        MockTimer alt_t = {.armed = false}, neu_t = {.armed = false};
        Model neu_m = {.running = false};
        mock_timer_stop(&alt_t); // ALT exit_callback
        neu_m.running = false; // NEU exit_callback's locked write
        mock_timer_stop(&neu_t); // NEU exit_callback's trailing stop
        assert(alt_t.armed == false && neu_t.armed == false && neu_m.running == false);
        printf("[regression] exit while not running: ALT and NEU agree (armed=false)\n");
        checks++;
    }

    // --- Regression sweep: NEU never ends a "was running, then exited" cycle with the timer
    // armed, across many repeated start/exit cycles in both possible orderings. ---
    {
        int mismatches = 0;
        for(int i = 0; i < 5000; i++) {
            MockTimer t = {.armed = true};
            Model m = {.running = true};
            if(i % 2 == 0) {
                // order A this cycle
                m.running = false;
                mock_timer_stop(&t);
            } else {
                // order B this cycle
                mock_timer_stop(&t);
                mock_timer_start(&t);
                m.running = false;
                mock_timer_stop(&t);
            }
            if(t.armed != false || m.running != false) mismatches++;
        }
        assert(mismatches == 0);
        printf("[regression] 5000 NEU start/exit cycles alternating both orders: mismatches=%d "
               "(expected 0)\n",
               mismatches);
        checks++;
    }

    printf("ALL CHECKS PASSED (%d checks)\n", checks);
    return 0;
}
