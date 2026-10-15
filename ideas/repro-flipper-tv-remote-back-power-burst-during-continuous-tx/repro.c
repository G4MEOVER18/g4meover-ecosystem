/*
 * Host repro: flipper_tv_remote (jmanion0139), views/tv_remote_remote.c
 *
 * tv_remote_tx_start()/tv_remote_tx_stop()/tv_remote_ir_burst() control flow
 * transcribed 1:1 from the real source (infrared_worker and notification_message
 * calls replaced by counters -- this is pure state-machine logic, no hardware
 * dependency). ALT reproduces the shipped control flow; NEU adds the one
 * guard from the patch (tv_remote_remote_c.patch).
 *
 * Bug: while a continuous-hold TX is running for button A (Settings ->
 * "Hold TX: Continuous", e.g. holding Up for its Vol_up alt-action), a single
 * tap on Back (or a double-tap for Power) is supposed to interrupt it and
 * send Back/Power instead. Because tv_remote_tx_start() just returns when
 * worker_active is already true, the burst helper's "if(app->tx_active)"
 * check still sees the OLD transmission's flag and stops THAT one -- Back/
 * Power's own signal is never sent, even though the UI already showed it as
 * "active".
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TV_BUTTON_COUNT 14
#define BTN_UP   6
#define BTN_BACK 11
#define BTN_POWER 0

typedef struct {
    bool worker_active;
    bool tx_active;
    uint8_t remote_selected;
    bool learned[TV_BUTTON_COUNT];

    /* Instrumentation (not in the real app): records every button index for
     * which a signal was actually handed to the (mocked) IR hardware. */
    uint8_t sent_log[16];
    int sent_count;

    /* How many times the "old" transmission was cut short without ever
     * delivering the newly requested button. */
    int hijacked_without_send;
} App;

static void app_init(App* app) {
    memset(app, 0, sizeof(*app));
    for(int i = 0; i < TV_BUTTON_COUNT; i++) app->learned[i] = true;
}

/* ---- infrared_worker_* / notification_message mocks ---- */
static void mock_tx_actually_send(App* app, uint8_t button_index) {
    assert(app->sent_count < (int)(sizeof(app->sent_log) / sizeof(app->sent_log[0])));
    app->sent_log[app->sent_count++] = button_index;
}

/* ---- verbatim control flow from views/tv_remote_remote.c ---- */

static void tv_remote_tx_start(App* app, uint8_t button_index) {
    if(app->worker_active) return;
    if(button_index >= TV_BUTTON_COUNT) return;
    if(!app->learned[button_index]) return;

    app->remote_selected = button_index;
    app->tx_active = true;
    app->worker_active = true;
    /* infrared_worker_tx_start() -- the actual signal is handed to the
     * hardware by tv_remote_tx_callback() on the worker thread, keyed off
     * app->remote_selected. We model that hand-off directly here. */
    mock_tx_actually_send(app, button_index);
}

static void tv_remote_tx_stop(App* app) {
    if(!app->worker_active || !app->tx_active) return;
    app->worker_active = false;
    app->tx_active = false;
}

#ifdef FIXED
static void tv_remote_ir_burst(App* app, uint8_t button_index) {
    if(app->worker_active && app->remote_selected != button_index) {
        tv_remote_tx_stop(app);
    }
    tv_remote_tx_start(app, button_index);
    if(app->tx_active) {
        /* furi_delay_ms(IR_BURST_MS); -- no-op on host */
        tv_remote_tx_stop(app);
    }
}
#else
static void tv_remote_ir_burst(App* app, uint8_t button_index) {
    uint8_t prev_selected = app->remote_selected;
    bool was_active_for_other = app->worker_active && (app->remote_selected != button_index);

    tv_remote_tx_start(app, button_index);
    if(app->tx_active) {
        /* furi_delay_ms(IR_BURST_MS); -- no-op on host */
        tv_remote_tx_stop(app);
    }

    if(was_active_for_other && app->sent_log[app->sent_count - 1] != button_index) {
        (void)prev_selected;
        app->hijacked_without_send++;
    }
}
#endif

/* ---- Scenarios ---- */

static void scenario_regression_no_conflict(void) {
    App app;
    app_init(&app);
    tv_remote_ir_burst(&app, BTN_BACK);
    assert(app.sent_count == 1 && app.sent_log[0] == BTN_BACK);
    assert(!app.worker_active && !app.tx_active);
    printf("[PASS] regression: plain Back burst with no ongoing TX sends Back, cleans up\n");
}

static void scenario_back_during_continuous_hold(void) {
    App app;
    app_init(&app);

    /* User holds Up with "Hold TX: Continuous" enabled -> continuous TX for
     * Up's hold target starts and keeps running (worker_active stays true
     * until release, unlike the burst helpers which always stop again). */
    app.remote_selected = BTN_UP;
    app.worker_active = true;
    app.tx_active = true;
    mock_tx_actually_send(&app, BTN_UP); /* the continuous TX's first signal */

    /* While still holding Up, the user taps Back once. */
    tv_remote_ir_burst(&app, BTN_BACK);

#ifdef FIXED
    assert(app.sent_count == 2 && app.sent_log[1] == BTN_BACK);
    assert(!app.worker_active); /* Back's own burst cleaned itself up */
    printf("[PASS] fixed: Back tap during continuous Up-hold TX interrupts it and actually sends Back\n");
#else
    assert(app.sent_count == 1); /* Back's signal was NEVER sent */
    assert(!app.worker_active); /* but the ongoing Up-hold TX WAS stopped */
    assert(app.hijacked_without_send == 1);
    printf("[FOUND] bug: Back tap during continuous Up-hold TX silently stops the hold "
           "and never sends Back (sent_count=%d, still shows %u)\n",
           app.sent_count, app.sent_log[app.sent_count - 1]);
#endif
}

static void scenario_power_double_tap_during_continuous_hold(void) {
    App app;
    app_init(&app);
    app.remote_selected = BTN_UP;
    app.worker_active = true;
    app.tx_active = true;
    mock_tx_actually_send(&app, BTN_UP);

    tv_remote_ir_burst(&app, BTN_POWER); /* fires from the double-tap timer */

#ifdef FIXED
    assert(app.sent_count == 2 && app.sent_log[1] == BTN_POWER);
    printf("[PASS] fixed: Power double-tap during continuous Up-hold TX actually sends Power\n");
#else
    assert(app.sent_count == 1);
    assert(app.hijacked_without_send == 1);
    printf("[FOUND] bug: Power double-tap during continuous Up-hold TX never sends Power\n");
#endif
}

static void scenario_equivalence_same_button_held_and_bursted(void) {
    /* If the burst target happens to be the SAME button already running
     * continuously (edge case, not reachable via the real key_map since
     * Back/Power are never a hold target), the guard must not re-stop it
     * needlessly -- both ALT and NEU must behave identically here. */
    App app;
    app_init(&app);
    app.remote_selected = BTN_BACK;
    app.worker_active = true;
    app.tx_active = true;
    mock_tx_actually_send(&app, BTN_BACK);

    tv_remote_ir_burst(&app, BTN_BACK);

    /* Guard is skipped (remote_selected == button_index), so both versions
     * fall straight into tx_start()'s own worker_active early-return --
     * no new signal, but the pre-existing one is still stopped afterwards. */
    assert(app.sent_count == 1);
    assert(!app.worker_active);
    printf("[PASS] equivalence: same-button burst behaves identically under ALT and NEU\n");
}

int main(void) {
    scenario_regression_no_conflict();
    scenario_back_during_continuous_hold();
    scenario_power_double_tap_during_continuous_hold();
    scenario_equivalence_same_button_held_and_bursted();
    printf("ALLE CHECKS BESTANDEN (%s)\n",
#ifdef FIXED
           "NEU"
#else
           "ALT"
#endif
    );
    return 0;
}
