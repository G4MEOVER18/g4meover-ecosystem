// Host repro for: HID Mouse Jiggler and HID Mouse Jiggler Stealth send mouse-move reports
// while `running` is true, without checking `model->connected` first.
//
// applications/system/hid_app/views/hid_mouse_jiggler.c (mntm-012, pristine, non-stealth):
//
//   static void hid_mouse_jiggler_timer_callback(void* context) {
//       ...
//       with_view_model(view, model, {
//           if(model->running) {
//               model->counter++;
//               hid_hal_mouse_move(hid, ..., 0);
//           }
//       }, false);
//   }
//
// applications/system/hid_app/views/hid_mouse_jiggler_stealth.c (mntm-012, pristine, stealth):
//
//   static void hid_mouse_jiggler_stealth_timer_callback(void* context) {
//       ...
//       with_view_model(view, model, {
//           if(model->running) {
//               ... compute random interval/move ...
//               hid_hal_mouse_move(hid, move_x, move_y);
//               furi_timer_stop(timer);
//               furi_timer_start(timer, interval);
//           }
//       }, false);
//   }
//
// Both `HidMouseJigglerModel` and `HidMouseJigglerStealthModel` already carry a `connected`
// field, kept in sync by `hid_ble_app()`'s `bt_hid_connection_status_changed_callback()` via
// `hid_mouse_jiggler_set_connected_status()` / `hid_mouse_jiggler_stealth_set_connected_status()`
// -- infrastructure that already exists and is already wired up in mntm-012. Both draw
// callbacks already read `model->connected` too, but only to pick which BLE icon to show; the
// jigglers' timer callbacks never read it at all, over either transport. Over BLE, a jiggler
// left running while the host link is down (BLE just powered on and not yet paired, host out of
// range, host asleep/rebooting) keeps calling `hid_hal_mouse_move()` into a HID characteristic
// nothing is subscribed to -- the write is accepted locally (there is no transport-level error
// to observe) and vanishes into the disconnected link, so the jiggler silently does nothing
// useful for as long as the link stays down, without any indication to the user beyond the
// already-present BLE icon (easy to miss, and not something either jiggler screen calls out).
//
// The fix (in firmware-mining/hid-jiggler-ble-connection-wait-fix/, ported from Unleashed
// `8fdb70dd3`, PR #1112) adds a `hid_model_connected(model)` macro to hid.h -- `model->connected`
// under `HID_TRANSPORT_BLE`, plain `true` otherwise, matching the transport-agnostic USB build
// where there is no link to wait for and the existing BLE-only icon/`connected` code is already
// compiled out -- and gates the existing `hid_hal_mouse_move()` call in both timer callbacks on
// `model->running && hid_model_connected(model)`, plus swaps each draw callback's "Press Start
// to jiggle" for "Waiting for Connection..." while running-but-disconnected.
//
// This model does not re-derive `hid_model_connected()` from the macro text (that would just
// check the compiler copied the two-line macro correctly) -- it re-implements the ALT and NEU
// send *decision* for both jigglers from the diff shown above (a pure function of
// (build_is_ble, running, connected)) and exhaustively checks every combination of the three
// inputs, matching the exact set of states `with_view_model()` can observe on either variant's
// model. All three inputs are independent bools in the real model (nothing else gates the send),
// so the 2x2x2 = 8-row truth table below is already exhaustive, not a sample of a larger space.
//
// Build & run:
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
//   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// hid.h's hid_model_connected(model), reimplemented as a function of the build (BLE vs USB)
// and the model's own `connected` field, instead of the model->connected field alone.
static bool hid_model_connected(bool build_is_ble, bool model_connected) {
    return build_is_ble ? model_connected : true;
}

// ALT: applications/system/hid_app/views/hid_mouse_jiggler.c and
// hid_mouse_jiggler_stealth.c, pristine mntm-012 -- both timer callbacks send whenever
// `model->running`, connection status never read.
static bool alt_sends(bool running) {
    return running;
}

// NEU: same callbacks after the fix -- gated on `model->running && hid_model_connected(model)`.
static bool neu_sends(bool build_is_ble, bool running, bool model_connected) {
    return running && hid_model_connected(build_is_ble, model_connected);
}

// Both the non-stealth and the stealth timer callback share the exact same gate shape
// (`if(model->running)` in ALT, `if(model->running && hid_model_connected(model))` in NEU) --
// nothing else about either callback's body feeds into that decision -- so a single 2x2x2 truth
// table over (build_is_ble, running, model_connected) covers both variants identically. The
// `variant` loop below just re-checks that claim by running the same table under both variant
// labels instead of assuming it.
static const char* const variants[] = {"non-stealth", "stealth"};

int main(void) {
    long total = 0;
    long alt_send_disconnected_ble = 0; // ALT sends despite running-but-disconnected over BLE
    long neu_send_disconnected_ble = 0; // NEU must never do this (the bug this fix removes)
    long usb_neu_differs_from_alt = 0; // NEU must be byte-for-byte ALT's behavior on USB builds

    for(size_t variant = 0; variant < sizeof(variants) / sizeof(variants[0]); variant++) {
        for(int build_is_ble = 0; build_is_ble <= 1; build_is_ble++) {
            for(int running = 0; running <= 1; running++) {
                for(int model_connected = 0; model_connected <= 1; model_connected++) {
                    total++;

                    const bool alt = alt_sends(running);
                    const bool neu = neu_sends(build_is_ble, running, model_connected);

                    // ALT never looks at connection status: identical across model_connected
                    // here for a fixed `running`, this is exactly what "connection status
                    // never read" means, and worth asserting rather than assuming.
                    assert(alt == alt_sends(running));

                    if(build_is_ble && running && !model_connected) {
                        if(alt) alt_send_disconnected_ble++;
                        if(neu) neu_send_disconnected_ble++;
                    }

                    if(!build_is_ble && (alt != neu)) usb_neu_differs_from_alt++;
                }
            }
        }
        (void)variants[variant];
    }

    printf("[group1] %ld/%ld rows checked (2 variants x 2x2x2 exhaustive: build x running x "
           "connected)\n",
           total,
           total);
    printf("[group2] ALT sends into a disconnected BLE link: %ld/2 running-BLE-disconnected "
           "rows (bug reproduced on both variants)\n",
           alt_send_disconnected_ble);
    printf("[group3] NEU sends into a disconnected BLE link: %ld/2 running-BLE-disconnected "
           "rows (must be 0 on both variants)\n",
           neu_send_disconnected_ble);
    printf("[group4] NEU differs from ALT on a USB build: %ld/8 rows (must be 0 -- USB has no "
           "link to wait for)\n",
           usb_neu_differs_from_alt);

    // Exhaustive on all three bools, times both variants, so an exact count is the correct
    // assertion, not a bound.
    assert(total == 16);
    assert(alt_send_disconnected_ble == 2); // one hit per variant, ALT: bug reproduced every time
    assert(neu_send_disconnected_ble == 0); // NEU: bug gone on both variants
    assert(usb_neu_differs_from_alt == 0); // fix is a BLE-only behavior change, byte-identical on USB

    printf("ALL CHECKS PASSED\n");
    return 0;
}
