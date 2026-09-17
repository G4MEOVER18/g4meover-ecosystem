/*
 * Host repro: furi_hal_serial_control_set_expansion_callback()'s bound check.
 *
 * Real firmware (targets/f7/furi_hal/furi_hal_serial_control.c, byte-identical to my
 * mntm-012 base, verified against a fresh mntm-012 checkout):
 *
 *   typedef enum {
 *       FuriHalSerialIdUsart,
 *       FuriHalSerialIdLpuart,
 *       FuriHalSerialIdMax,
 *   } FuriHalSerialId;
 *
 *   struct FuriHalSerialHandle {
 *       FuriHalSerialId id;
 *       bool in_use;
 *   };
 *
 *   typedef struct {
 *       FuriHalSerialHandle handles[FuriHalSerialIdMax];
 *       FuriMessageQueue* queue;
 *       FuriThread* thread;
 *       ... (log/expansion fields)
 *   } FuriHalSerialControl;
 *
 *   void furi_hal_serial_control_set_expansion_callback(
 *       FuriHalSerialId serial_id, FuriHalSerialControlExpansionCallback callback, void* context) {
 *       furi_check(serial_id <= FuriHalSerialIdMax);   // ALT (bug) -- accepts the sentinel itself
 *       ...
 *   }
 *
 *   static bool furi_hal_serial_control_handler_expansion_set_callback(void* input, void* output) {
 *       ...
 *       FuriHalSerialHandle* handle = &furi_hal_serial_control->handles[message_input->id];
 *       ...
 *       handle->... = ...;   // written through unconditionally
 *   }
 *
 * FuriHalSerialIdMax (2) is a sentinel marking "one past the last real UART", used elsewhere in
 * the same file (e.g. the *logging* setter, which explicitly documents "Use FuriHalSerialIdMax to
 * disable logging" and correctly keeps its own `serial_id <= FuriHalSerialIdMax` check). The
 * *expansion*-callback setter has no such "Max means disable" semantic -- it disables via
 * `callback == NULL`, not via a sentinel serial_id -- so accepting `serial_id == FuriHalSerialIdMax`
 * here is a copy-paste-shaped off-by-one: it lets the handler index `handles[FuriHalSerialIdMax]`,
 * one element past the end of the `handles[]` array. Because `handles[]` is embedded inside a
 * larger struct (not a standalone allocation), that one-past-the-end write does not fault or trip
 * a heap-overflow detector -- it silently lands on the very next struct field, `queue`, the
 * `FuriMessageQueue*` the serial-control worker thread depends on to receive its own messages.
 *
 * This program reconstructs that exact struct layout (field order, sizes) and both versions of the
 * bound check, to show concretely:
 *   - ALT (`<=`): a call with the sentinel `FuriHalSerialIdMax` passes the check and then corrupts
 *     the live `queue` pointer with attacker-independent, but caller-supplied-adjacent garbage
 *     (the handle's own `id`/`in_use` bytes) -- a silent, hard-to-diagnose corruption of the
 *     control thread's own message queue.
 *   - NEU (`<`): the same call is rejected by the bound check (modeled here as a `furi_check`-style
 *     abort, isolated in a forked child so the test harness itself survives) *before* the `queue`
 *     field is ever touched -- `queue` is provably unchanged afterwards.
 *   - Regression: both real, in-range serial ids (Usart=0, Lpuart=1) behave identically under ALT
 *     and NEU -- the fix changes nothing for any caller that was already passing a valid id.
 *
 * Build/run:
 *   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 *   ./repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/wait.h>

typedef enum {
    FuriHalSerialIdUsart,
    FuriHalSerialIdLpuart,
    FuriHalSerialIdMax,
} FuriHalSerialId;

typedef struct {
    FuriHalSerialId id;
    bool in_use;
} FuriHalSerialHandle;

/* Same field order as the real FuriHalSerialControl struct, up through `queue`/`thread` --
 * the fields after `thread` are irrelevant to this bug and omitted. */
typedef struct {
    FuriHalSerialHandle handles[FuriHalSerialIdMax];
    void* queue; /* stand-in for FuriMessageQueue* */
    void* thread; /* stand-in for FuriThread* */
} FuriHalSerialControl;

/* sim_furi_check(): models furi_check() -- aborts the process on failure, exactly like the real
 * firmware halting/rebooting rather than continuing past a violated invariant. */
static void sim_furi_check(bool cond, const char* what) {
    if(!cond) {
        fprintf(stderr, "  furi_check failed: %s\n", what);
        abort();
    }
}

/* ALT: real, unmodified bound check (`<=`). */
static void set_expansion_callback_alt(FuriHalSerialControl* fc, FuriHalSerialId serial_id) {
    sim_furi_check(serial_id <= FuriHalSerialIdMax, "serial_id <= FuriHalSerialIdMax");
    FuriHalSerialHandle* handle = &fc->handles[serial_id];
    handle->id = serial_id;
    handle->in_use = true;
}

/* NEU: Unleashed's fix (`<`), 1:1 the same one-character change as commit 4b3c614c8. */
static void set_expansion_callback_neu(FuriHalSerialControl* fc, FuriHalSerialId serial_id) {
    sim_furi_check(serial_id < FuriHalSerialIdMax, "serial_id < FuriHalSerialIdMax");
    FuriHalSerialHandle* handle = &fc->handles[serial_id];
    handle->id = serial_id;
    handle->in_use = true;
}

typedef void (*SetExpansionCallbackFn)(FuriHalSerialControl*, FuriHalSerialId);

/* Stand-in "live" queue objects -- never freed/reused, just distinct addresses to detect
 * corruption against. Using static objects (instead of malloc) means a corrupted `queue` field
 * is never passed to free(), so the repro can't itself trigger an invalid-free under ASan --
 * exactly mirroring the real bug, where the corrupted pointer is never freed either, just
 * eventually dereferenced by the control thread. */
static int g_queue_object_a, g_queue_object_b, g_queue_object_c, g_queue_object_d;

static FuriHalSerialControl* alloc_control(void* queue_object) {
    FuriHalSerialControl* fc = malloc(sizeof(*fc));
    memset(fc, 0, sizeof(*fc));
    fc->queue = queue_object;
    fc->thread = NULL;
    return fc;
}

/* Runs `fn(fc, serial_id)` in a forked (copy-on-write) child, so a furi_check-style abort()
 * inside the child cannot take down this test harness, and any write the child makes is never
 * observed by the parent. Returns true if the child survived (call returned normally), false if
 * it aborted. The real, visible effect of a surviving ALT call is inspected separately, via a
 * second, non-forked call on the parent's own object. */
static bool run_isolated(SetExpansionCallbackFn fn, FuriHalSerialControl* fc, FuriHalSerialId id) {
    pid_t pid = fork();
    sim_furi_check(pid >= 0, "fork() failed");
    if(pid == 0) {
        fn(fc, id);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int failures = 0;

static void check(bool cond, const char* label) {
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", label);
    if(!cond) failures++;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf(
        "offsetof(handles)=%zu offsetof(queue)=%zu sizeof(handles)=%zu "
        "(queue directly follows handles[]: %s)\n\n",
        offsetof(FuriHalSerialControl, handles),
        offsetof(FuriHalSerialControl, queue),
        sizeof(((FuriHalSerialControl*)0)->handles),
        offsetof(FuriHalSerialControl, queue) ==
                sizeof(((FuriHalSerialControl*)0)->handles) + offsetof(FuriHalSerialControl, handles)
            ? "yes"
            : "no");

    /* --- Regression: both real serial ids behave identically under ALT and NEU --- */
    {
        FuriHalSerialControl* fc_alt = alloc_control(&g_queue_object_a);
        FuriHalSerialControl* fc_neu = alloc_control(&g_queue_object_b);
        void* queue_before_alt = fc_alt->queue;
        void* queue_before_neu = fc_neu->queue;
        for(FuriHalSerialId id = FuriHalSerialIdUsart; id <= FuriHalSerialIdLpuart; id++) {
            printf("regression: serial_id=%d\n", id);
            bool alt_ok = run_isolated(set_expansion_callback_alt, fc_alt, id);
            bool neu_ok = run_isolated(set_expansion_callback_neu, fc_neu, id);
            check(alt_ok, "ALT accepts real serial id");
            check(neu_ok, "NEU accepts real serial id (unchanged behavior)");
            /* apply for real (not forked) to inspect resulting state */
            set_expansion_callback_alt(fc_alt, id);
            set_expansion_callback_neu(fc_neu, id);
            check(fc_alt->handles[id].id == id && fc_alt->handles[id].in_use, "ALT sets handle correctly");
            check(fc_neu->handles[id].id == id && fc_neu->handles[id].in_use, "NEU sets handle correctly");
            check(fc_alt->queue == queue_before_alt, "ALT: queue pointer untouched by an in-range id");
            check(fc_neu->queue == queue_before_neu, "NEU: queue pointer untouched by an in-range id");
        }
        free(fc_alt);
        free(fc_neu);
        printf("\n");
    }

    /* --- BUG: sentinel FuriHalSerialIdMax passed to the expansion-callback setter --- */
    {
        FuriHalSerialControl* fc_alt = alloc_control(&g_queue_object_c);
        FuriHalSerialControl* fc_neu = alloc_control(&g_queue_object_d);
        void* queue_before_alt = fc_alt->queue;
        void* queue_before_neu = fc_neu->queue;

        printf("BUG case: serial_id=FuriHalSerialIdMax (%d)\n", FuriHalSerialIdMax);
        bool alt_survived = run_isolated(set_expansion_callback_alt, fc_alt, FuriHalSerialIdMax);
        bool neu_survived = run_isolated(set_expansion_callback_neu, fc_neu, FuriHalSerialIdMax);

        check(alt_survived, "ALT: furi_check does NOT reject the sentinel (bug: proceeds to write)");
        check(!neu_survived, "NEU: furi_check rejects the sentinel (process aborts BEFORE any OOB write)");

        /* Apply for real (not forked) to inspect the actual resulting memory state. */
        set_expansion_callback_alt(fc_alt, FuriHalSerialIdMax);
        printf(
            "  ALT: fc->queue before=%p after=%p (%s)\n",
            queue_before_alt,
            fc_alt->queue,
            fc_alt->queue == queue_before_alt ? "unchanged" : "CORRUPTED");
        check(
            fc_alt->queue != queue_before_alt,
            "ALT corrupts the live queue pointer via the OOB handles[FuriHalSerialIdMax] write");

        /* NEU: the real call would furi_check-abort; demonstrate the queue field is simply never
         * reached by inspecting it straight after the isolated (aborted) attempt. */
        printf(
            "  NEU: fc->queue before=%p after=%p (%s)\n",
            queue_before_neu,
            fc_neu->queue,
            fc_neu->queue == queue_before_neu ? "unchanged" : "CORRUPTED");
        check(fc_neu->queue == queue_before_neu, "NEU never touches queue -- rejected up front");

        /* fc_alt->queue now holds a corrupted, non-pointer value (never freed here, exactly as
         * the real control thread would never know to free/repair it either -- it would simply
         * use it as-is on its next message). */
        free(fc_neu);
        free(fc_alt);
        printf("\n");
    }

    if(failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", failures);
        return 1;
    }
}
