// Host repro for the HID BLE app-entry callback-registration-order bug.
//
// Models the two lines in `hid_ble_app()` (applications/system/hid_app/hid.c) that set up
// the connection-status path before the view dispatcher runs:
//
//   ALT (mntm-012, unchanged):
//       furi_hal_bt_start_advertising();
//       bt_set_status_changed_callback(app->bt, bt_hid_connection_status_changed_callback, app);
//
//   NEU (ported fix):
//       bt_set_status_changed_callback(app->bt, bt_hid_connection_status_changed_callback, app);
//       furi_hal_bt_start_advertising();
//
// `bt_set_status_changed_callback()` only stores a function pointer + context in the Bt
// service; it does not itself change the radio state. `furi_hal_bt_start_advertising()` is the
// call that actually starts advertising and can - if a previously bonded central is already in
// range - lead to a status-changed event for that connection while still inside (or immediately
// after) the call, before the rest of `hid_ble_app()` has run any further. Whichever of the two
// calls above the app made second decides whether a callback is already registered at that
// point.
//
// This repro does not need the real BLE stack to demonstrate the bug: it only needs that
// `bt_set_status_changed_callback()` and `starting advertising` are two separate, ordered
// operations, and that the event which decides the app's "connected" status can be delivered as
// soon as advertising starts. That is modelled directly below with a minimal stand-in for the
// Bt service's callback slot, and a `sim_start_advertising()` that delivers exactly one
// "already-bonded device reconnected" event synchronously, the same way a real interrupt-driven
// status update could race with the rest of `hid_ble_app()` right after advertising begins.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum { BtStatusDisconnected, BtStatusConnected } BtStatus;

typedef void (*BtStatusChangedCallback)(BtStatus status, void* context);

// Stand-in for the Bt service's single callback slot (bt_service/bt.c keeps exactly one).
typedef struct {
    BtStatusChangedCallback callback;
    void* context;
} BtServiceSim;

static void bt_set_status_changed_callback_sim(
    BtServiceSim* bt,
    BtStatusChangedCallback callback,
    void* context) {
    bt->callback = callback;
    bt->context = context;
}

// Models `furi_hal_bt_start_advertising()`: starts advertising and, if a previously bonded
// central is in range, synchronously delivers exactly one BtStatusConnected event for it -
// exactly like the real radio/interrupt layer can, independent of whether the app has already
// registered a callback for it.
static void sim_start_advertising(BtServiceSim* bt, bool bonded_device_in_range) {
    if(bonded_device_in_range && bt->callback != NULL) {
        bt->callback(BtStatusConnected, bt->context);
    }
    // bonded_device_in_range && bt->callback == NULL: the event has nowhere to go and is lost,
    // exactly the failure this repro demonstrates.
}

// Stand-in for the app's connected-status model field, set only from the real callback
// (`bt_hid_connection_status_changed_callback()` -> `hid_mouse_jiggler_set_connected_status()`
// and its siblings for every other HID view).
typedef struct {
    bool connected;
    int callback_invocations;
} HidAppSim;

static void hid_status_callback(BtStatus status, void* context) {
    HidAppSim* app = context;
    app->connected = (status == BtStatusConnected);
    app->callback_invocations++;
}

// ALT: exact call order from the unchanged mntm-012 `hid_ble_app()`.
static void run_alt(bool bonded_device_in_range, HidAppSim* app_out) {
    BtServiceSim bt;
    memset(&bt, 0, sizeof(bt));
    HidAppSim app = {0};

    sim_start_advertising(&bt, bonded_device_in_range);
    bt_set_status_changed_callback_sim(&bt, hid_status_callback, &app);

    *app_out = app;
}

// NEU: ported fix - register the callback first, then start advertising.
static void run_neu(bool bonded_device_in_range, HidAppSim* app_out) {
    BtServiceSim bt;
    memset(&bt, 0, sizeof(bt));
    HidAppSim app = {0};

    bt_set_status_changed_callback_sim(&bt, hid_status_callback, &app);
    sim_start_advertising(&bt, bonded_device_in_range);

    *app_out = app;
}

int main(void) {
    int checks = 0;

    // Group 1: no bonded device in range at startup - both orderings behave identically
    // (nothing to miss), confirms the repro only diverges on the actual race, not in general.
    {
        HidAppSim alt, neu;
        run_alt(false, &alt);
        run_neu(false, &neu);
        assert(alt.connected == false && alt.callback_invocations == 0);
        assert(neu.connected == false && neu.callback_invocations == 0);
        checks += 4;
        printf("[group1] no bonded device in range: ALT connected=%d invocations=%d, "
               "NEU connected=%d invocations=%d (both idle, as expected)\n",
               alt.connected,
               alt.callback_invocations,
               neu.connected,
               neu.callback_invocations);
    }

    // Group 2: a previously bonded device is already in range and reconnects the instant
    // advertising starts - the actual startup race this fix closes.
    {
        HidAppSim alt, neu;
        run_alt(true, &alt);
        run_neu(true, &neu);

        // ALT: the connect event fires before the callback is registered - lost. The app's
        // model stays at its initial `connected = false` with no way to learn otherwise until
        // some later, unrelated status change happens to occur.
        assert(alt.callback_invocations == 0);
        assert(alt.connected == false);

        // NEU: the callback is already registered when advertising starts, so the same event
        // is delivered and the model is updated correctly.
        assert(neu.callback_invocations == 1);
        assert(neu.connected == true);

        checks += 4;
        printf("[group2] bonded device reconnects during startup: "
               "ALT connected=%d invocations=%d (event lost), "
               "NEU connected=%d invocations=%d (event delivered)\n",
               alt.connected,
               alt.callback_invocations,
               neu.connected,
               neu.callback_invocations);
    }

    // Group 3: regression - a later, ordinary status change (e.g. the same device eventually
    // disconnecting) still reaches the app under NEU, and is not double-delivered or lost by
    // moving the registration earlier.
    {
        BtServiceSim bt;
        memset(&bt, 0, sizeof(bt));
        HidAppSim app = {0};

        bt_set_status_changed_callback_sim(&bt, hid_status_callback, &app);
        sim_start_advertising(&bt, false); // no reconnect race this time
        assert(app.callback_invocations == 0);

        // A later, ordinary connection event (unrelated to the startup race) still works.
        bt.callback(BtStatusConnected, bt.context);
        assert(app.connected == true && app.callback_invocations == 1);
        bt.callback(BtStatusDisconnected, bt.context);
        assert(app.connected == false && app.callback_invocations == 2);

        checks += 4;
        printf("[group3] regression, ordinary post-startup connect/disconnect under NEU: "
               "final connected=%d invocations=%d (both events delivered exactly once each)\n",
               app.connected,
               app.callback_invocations);
    }

    printf("ALL CHECKS PASSED (%d checks)\n", checks);
    return 0;
}
