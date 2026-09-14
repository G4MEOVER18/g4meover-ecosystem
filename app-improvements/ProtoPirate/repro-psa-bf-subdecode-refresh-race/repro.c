// Host repro for the PSA-Bruteforce-Kernel orchestration race in
// scenes/plugins/protopirate_psa_bf_plugin.c (ProtoPirate, ENABLE_SUB_DECODE_SCENE path).
//
// Reproduces the exact control flow of plugin_on_scene_event()/bf_finish_and_show_result()
// (variable names, branch structure and call sites kept 1:1) as a self-contained state
// model, without any Flipper/GUI headers. Verifies:
//   1. ALT (current code): when the periodic Tick handler notices completion before the
//      worker thread's ProtoPirateCustomEventPsaBruteforceComplete event is processed
//      ("Tick wins the race"), protopirate_subdecode_psa_bf_complete_refresh() is skipped
//      for NOT_FOUND/CANCELLED outcomes in the SubDecode context.
//   2. NEW (fix: move the refresh call into bf_finish_and_show_result() itself instead of
//      only the Custom-event handler) always fires it, regardless of which event wins.
//   3. Regression: ReceiverInfo context never calls the refresh hook, in ALT or NEW.
//   4. No double-free / no crash in any of the 2 (order) x 2 (context) x 3 (outcome) cases.
//
// Build: gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PSA_BF_STATUS_IDLE 0
#define PSA_BF_STATUS_RUNNING 1
#define PSA_BF_STATUS_FOUND 2
#define PSA_BF_STATUS_NOT_FOUND 3
#define PSA_BF_STATUS_CANCELLED 4

typedef enum {
    CtxReceiverInfo,
    CtxSubDecode,
} Ctx;

typedef struct {
    uint8_t status;
    bool freed;
} PsaBfState;

// --- instrumented stand-ins for the real host-API callbacks --------------------------------
static int g_show_result_calls;
static int g_apply_success_calls;
static int g_refresh_calls;
static int g_double_free_detected;

static void host_show_bf_result(uint8_t status) {
    (void)status;
    g_show_result_calls++;
}

static void host_apply_success_to_history(void) {
    g_apply_success_calls++;
}

static void host_subdecode_signal_info_refresh(void) {
    g_refresh_calls++;
}

// --- global state mirroring g_bf_state / g_bf_thread / g_active_ctx ------------------------
static PsaBfState* g_bf_state;
static bool g_bf_thread_active; // proxy for "g_bf_thread != NULL"
static Ctx g_active_ctx;

static void reset_globals(Ctx ctx, uint8_t final_status) {
    g_show_result_calls = 0;
    g_apply_success_calls = 0;
    g_refresh_calls = 0;
    g_double_free_detected = 0;

    g_bf_state = malloc(sizeof(PsaBfState));
    assert(g_bf_state);
    // final_status models the worker having already run psa_brute_force_run() and set
    // state->status to a terminal value *before* calling on_done()/sending the Complete
    // event -- exactly as psa_crypto_bf.c's psa_brute_force_thread_entry() does.
    g_bf_state->status = final_status;
    g_bf_state->freed = false;
    g_bf_thread_active = true;
    g_active_ctx = ctx;
}

static void free_state_once(void) {
    if(g_bf_state->freed) {
        g_double_free_detected++;
        return;
    }
    g_bf_state->freed = true;
    free(g_bf_state);
    g_bf_state = NULL;
}

// --- ALT: current code (refresh call lives only in the Custom-event handler) ---------------
static void bf_finish_and_show_result_ALT(bool with_subdecode_refresh_here) {
    if(!g_bf_state) return;

    uint8_t status = g_bf_state->status;

    if(g_bf_thread_active) {
        g_bf_thread_active = false; // furi_thread_join()+furi_thread_free()
    }

    if(status == PSA_BF_STATUS_FOUND) {
        host_apply_success_to_history();
        host_show_bf_result(status);
        // g_bf_state deliberately kept alive until the user dismisses the result screen.
    } else {
        free_state_once();
        host_show_bf_result(status);
    }

    // Bug: in ALT this parameter is only ever true from the Custom-event call site.
    if(with_subdecode_refresh_here && g_active_ctx == CtxSubDecode) {
        host_subdecode_signal_info_refresh();
    }
}

static bool tick_handler_ALT(void) {
    if(g_bf_thread_active && g_bf_state) {
        uint8_t bfst = g_bf_state->status;
        if(bfst == PSA_BF_STATUS_IDLE || bfst == PSA_BF_STATUS_RUNNING) {
            // show_bf_progress(); not relevant here
        } else {
            bf_finish_and_show_result_ALT(/*with_subdecode_refresh_here=*/false);
        }
        return true;
    }
    return false;
}

static bool custom_complete_handler_ALT(void) {
    if(g_bf_state) {
        bf_finish_and_show_result_ALT(/*with_subdecode_refresh_here=*/false);
        if(g_active_ctx == CtxSubDecode) {
            host_subdecode_signal_info_refresh();
        }
    }
    return true;
}

// --- NEW: fix (refresh call moved into bf_finish_and_show_result itself) -------------------
static void bf_finish_and_show_result_NEW(void) {
    if(!g_bf_state) return;

    uint8_t status = g_bf_state->status;

    if(g_bf_thread_active) {
        g_bf_thread_active = false;
    }

    if(status == PSA_BF_STATUS_FOUND) {
        host_apply_success_to_history();
        host_show_bf_result(status);
    } else {
        free_state_once();
        host_show_bf_result(status);
    }

    if(g_active_ctx == CtxSubDecode) {
        host_subdecode_signal_info_refresh();
    }
}

static bool tick_handler_NEW(void) {
    if(g_bf_thread_active && g_bf_state) {
        uint8_t bfst = g_bf_state->status;
        if(bfst == PSA_BF_STATUS_IDLE || bfst == PSA_BF_STATUS_RUNNING) {
            // show_bf_progress();
        } else {
            bf_finish_and_show_result_NEW();
        }
        return true;
    }
    return false;
}

static bool custom_complete_handler_NEW(void) {
    if(g_bf_state) {
        bf_finish_and_show_result_NEW();
    }
    return true;
}

// --- driver ----------------------------------------------------------------------------------
typedef enum { OrderTickFirst, OrderCustomFirst } Order;

static const char* status_name(uint8_t s) {
    switch(s) {
    case PSA_BF_STATUS_FOUND: return "FOUND";
    case PSA_BF_STATUS_NOT_FOUND: return "NOT_FOUND";
    case PSA_BF_STATUS_CANCELLED: return "CANCELLED";
    default: return "?";
    }
}

int main(void) {
    const uint8_t outcomes[] = {PSA_BF_STATUS_FOUND, PSA_BF_STATUS_NOT_FOUND, PSA_BF_STATUS_CANCELLED};
    const Ctx ctxs[] = {CtxReceiverInfo, CtxSubDecode};
    const Order orders[] = {OrderTickFirst, OrderCustomFirst};

    int alt_bug_cases = 0;
    int alt_regression_violations = 0;
    int new_missing_refresh = 0;
    int new_regression_violations = 0;

    for(size_t oi = 0; oi < sizeof(outcomes) / sizeof(outcomes[0]); oi++) {
        for(size_t ci = 0; ci < sizeof(ctxs) / sizeof(ctxs[0]); ci++) {
            for(size_t ri = 0; ri < sizeof(orders) / sizeof(orders[0]); ri++) {
                uint8_t status = outcomes[oi];
                Ctx ctx = ctxs[ci];
                Order order = orders[ri];

                // --- ALT run ---
                reset_globals(ctx, status);
                if(order == OrderTickFirst) {
                    tick_handler_ALT();
                    custom_complete_handler_ALT();
                    tick_handler_ALT(); // no-op: g_bf_thread_active already false
                } else {
                    custom_complete_handler_ALT();
                    tick_handler_ALT(); // no-op: g_bf_thread_active already false
                }
                bool alt_refreshed = g_refresh_calls > 0;
                bool alt_should_refresh = (ctx == CtxSubDecode);
                if(alt_should_refresh && !alt_refreshed) {
                    alt_bug_cases++;
                    printf(
                        "[ALT] BUG confirmed: ctx=SubDecode status=%-9s order=%-11s -> "
                        "refresh_calls=%d (expected >=1)\n",
                        status_name(status),
                        order == OrderTickFirst ? "TickFirst" : "CustomFirst",
                        g_refresh_calls);
                }
                if(ctx == CtxReceiverInfo && g_refresh_calls != 0) {
                    alt_regression_violations++;
                }
                if(g_double_free_detected) {
                    fprintf(stderr, "[ALT] double free detected, aborting\n");
                    return 1;
                }
                if(status != PSA_BF_STATUS_FOUND && g_bf_state != NULL) {
                    fprintf(stderr, "[ALT] leak: state not freed for %s\n", status_name(status));
                    return 1;
                }
                if(status == PSA_BF_STATUS_FOUND) {
                    // still owned by the "result screen", free it now to end the scenario cleanly
                    if(g_bf_state) free(g_bf_state);
                }

                // --- NEW run ---
                reset_globals(ctx, status);
                if(order == OrderTickFirst) {
                    tick_handler_NEW();
                    custom_complete_handler_NEW();
                    tick_handler_NEW();
                } else {
                    custom_complete_handler_NEW();
                    tick_handler_NEW();
                }
                bool new_refreshed = g_refresh_calls > 0;
                bool new_should_refresh = (ctx == CtxSubDecode);
                if(new_should_refresh && !new_refreshed) {
                    new_missing_refresh++;
                    printf(
                        "[NEW] REGRESSION: ctx=SubDecode status=%-9s order=%-11s -> "
                        "refresh_calls=%d (expected >=1)\n",
                        status_name(status),
                        order == OrderTickFirst ? "TickFirst" : "CustomFirst",
                        g_refresh_calls);
                }
                if(ctx == CtxReceiverInfo && g_refresh_calls != 0) {
                    new_regression_violations++;
                }
                if(g_double_free_detected) {
                    fprintf(stderr, "[NEW] double free detected, aborting\n");
                    return 1;
                }
                if(status != PSA_BF_STATUS_FOUND && g_bf_state != NULL) {
                    fprintf(stderr, "[NEW] leak: state not freed for %s\n", status_name(status));
                    return 1;
                }
                if(status == PSA_BF_STATUS_FOUND) {
                    if(g_bf_state) free(g_bf_state);
                }
            }
        }
    }

    printf("\n--- summary over %zu (outcome x context x order) cases ---\n",
           sizeof(outcomes) / sizeof(outcomes[0]) * sizeof(ctxs) / sizeof(ctxs[0]) *
               sizeof(orders) / sizeof(orders[0]));
    printf("ALT: SubDecode cases missing the refresh call: %d (expected: 2 -- NOT_FOUND and "
           "CANCELLED, both only when Tick wins)\n",
           alt_bug_cases);
    printf("ALT: ReceiverInfo regression violations (refresh leaked into wrong context): %d "
           "(expected: 0)\n",
           alt_regression_violations);
    printf("NEW: SubDecode cases still missing the refresh call: %d (expected: 0)\n",
           new_missing_refresh);
    printf("NEW: ReceiverInfo regression violations: %d (expected: 0)\n", new_regression_violations);

    if(alt_bug_cases != 2 || alt_regression_violations != 0 || new_missing_refresh != 0 ||
       new_regression_violations != 0) {
        printf("\nFAIL: results did not match expectations\n");
        return 1;
    }

    printf("\nOK: ALT reproduces the skipped-refresh bug in exactly the predicted 2 cases, NEW "
           "fixes all of them without leaking the SubDecode-only refresh into ReceiverInfo, no "
           "double-free/leak in any of the 12 scenarios.\n");
    return 0;
}
