// Host repro: uart_worker() text-box truncation ordering bug in mag_scene_read.c
// (G4MEOVER18/g4meover-ecosystem, app-improvements/g4meover-magspoof)
//
// Models exactly two things read verbatim from upstream sources:
//   1) mlib's m_string_right(v, index) semantics (P-p-H-d/mlib, m-string.h):
//        - if(index >= size) { size = 0; }               // wipes the WHOLE string
//        - else { keep tail from index..size, size -= index; }
//      furi_string_right() is a 1:1 wrapper around this (Momentum-Firmware,
//      furi/core/string.c: `void furi_string_right(FuriString* v, size_t index) {
//      string_right(v->string, index); }`).
//   2) uart_worker()'s truncation block in scenes/mag_scene_read.c (ALT = as shipped,
//      NEW = this fix), operating on the same tiny "mock FuriString" model so the only
//      difference between ALT and NEW is the ordering/inputs of the real source lines.
//
// No furi/FreeRTOS/hardware dependency -- pure string-length arithmetic, exactly what
// WORKFLOW.md asks for ("reine Logik, ohne Hardware verifizierbar").

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CAP 8192

typedef struct {
    char buf[MAX_CAP + 1];
    size_t size;
} MockFuriString;

static void mock_init(MockFuriString* v, size_t size) {
    assert(size <= MAX_CAP);
    for(size_t i = 0; i < size; i++) v->buf[i] = 'A' + (char)(i % 26);
    v->buf[size] = '\0';
    v->size = size;
}

// 1:1 port of P-p-H-d/mlib m_string_right() (see file header) via furi_string_right().
static void mock_furi_string_right(MockFuriString* v, size_t index) {
    if(index >= v->size) {
        v->buf[0] = '\0';
        v->size = 0;
        return;
    }
    size_t s2 = v->size - index;
    memmove(&v->buf[0], &v->buf[index], s2 + 1);
    v->size = s2;
}

static size_t mock_furi_string_size(const MockFuriString* v) {
    return v->size;
}

static void mock_cat(MockFuriString* v, const char* chunk, size_t len) {
    assert(v->size + len <= MAX_CAP);
    memcpy(&v->buf[v->size], chunk, len);
    v->size += len;
    v->buf[v->size] = '\0';
}

#define STORE_SIZE_REAL 4096u /* UART_TERMINAL_TEXT_BOX_STORE_SIZE */
#define MAX_LEN_REAL    320u  /* UART_RX_BUF_SIZE */

// ALT: uart_worker() exactly as shipped in zacharyweiss/magspoof_flipper
// (scenes/mag_scene_read.c, WorkerEvtRxDone branch), operating on the mock store.
// tracked_strlen is the app's own `mag->uart_text_box_store_strlen` bookkeeping variable.
static void alt_uart_chunk(MockFuriString* store, size_t* tracked_strlen, const char* chunk, size_t len, size_t store_cap) {
    if(len == 0) return;

    *tracked_strlen += len; // <-- bumped BEFORE the trim decision AND before the append

    if(*tracked_strlen >= store_cap - 1) {
        mock_furi_string_right(store, *tracked_strlen / 2);
        *tracked_strlen = mock_furi_string_size(store) + len;
    }

    mock_cat(store, chunk, len);
}

// NEW (this fix): decide/trim using the size the store actually has *before* the
// append, then append; tracked_strlen re-synced the same way as ALT afterwards.
static void new_uart_chunk(MockFuriString* store, size_t* tracked_strlen, const char* chunk, size_t len, size_t store_cap) {
    if(len == 0) return;

    if(*tracked_strlen + len >= store_cap - 1) {
        mock_furi_string_right(store, *tracked_strlen / 2);
    }
    *tracked_strlen = mock_furi_string_size(store) + len;

    mock_cat(store, chunk, len);
}

static int checks = 0, failures = 0;

#define CHECK(cond, ...)                                                          \
    do {                                                                          \
        checks++;                                                                \
        if(!(cond)) {                                                            \
            failures++;                                                          \
            fprintf(stderr, "CHECK FAILED (%s:%d): ", __FILE__, __LINE__);        \
            fprintf(stderr, __VA_ARGS__);                                        \
            fprintf(stderr, "\n");                                               \
        }                                                                        \
    } while(0)

static char scratch_chunk[MAX_LEN_REAL + 1];

static void fill_chunk(size_t len) {
    for(size_t i = 0; i < len; i++) scratch_chunk[i] = '0' + (char)(i % 10);
    scratch_chunk[len] = '\0';
}

// 1) Exact-constants sweep: every (initial_size, len) pair that can actually occur
//    with THIS app's real UART_TERMINAL_TEXT_BOX_STORE_SIZE/UART_RX_BUF_SIZE, restricted
//    to the range where truncation triggers (initial_size in [store_cap-1-len, store_cap-2]).
static void sweep_real_constants(void) {
    size_t store_cap = STORE_SIZE_REAL;
    long total_alt_deficit = 0;
    int triggered = 0;

    for(size_t len = 1; len <= MAX_LEN_REAL; len++) {
        // Smallest initial_size that still trips the "+len >= cap-1" check, up to cap-2
        // (the largest actual FuriString size possible pre-append, since >=cap-1 pre-append
        // would already have been trimmed on a prior chunk).
        size_t lo = (len + 1 >= store_cap) ? 0 : (store_cap - 1 - len);
        for(size_t initial_size = lo; initial_size <= store_cap - 2; initial_size++) {
            fill_chunk(len);

            MockFuriString alt_store, new_store;
            mock_init(&alt_store, initial_size);
            mock_init(&new_store, initial_size);
            size_t alt_strlen = initial_size, new_strlen = initial_size;

            alt_uart_chunk(&alt_store, &alt_strlen, scratch_chunk, len, store_cap);
            new_uart_chunk(&new_store, &new_strlen, scratch_chunk, len, store_cap);

            // Both must have triggered the trim branch for this (initial_size, len).
            CHECK(
                initial_size + len >= store_cap - 1,
                "sweep bound wrong: initial_size=%zu len=%zu", initial_size, len);
            triggered++;

            // NEW keeps exactly floor(initial_size/2) old bytes pre-append, i.e. the
            // textbook "trim to half of what's actually there" behaviour, and matches
            // its own tracked_strlen post-append.
            size_t expected_new_kept_old = initial_size - (initial_size / 2);
            size_t new_kept_old = mock_furi_string_size(&new_store) - len;
            CHECK(
                new_kept_old == expected_new_kept_old,
                "NEW kept %zu old bytes, expected %zu (initial_size=%zu len=%zu)",
                new_kept_old, expected_new_kept_old, initial_size, len);
            CHECK(
                new_strlen == mock_furi_string_size(&new_store),
                "NEW tracked_strlen (%zu) out of sync with real size (%zu)",
                new_strlen, mock_furi_string_size(&new_store));

            // ALT keeps floor((initial_size-len)/2) old bytes pre-append instead --
            // i.e. it discards MORE of the just-captured swipe data than intended,
            // every single time the rolling truncation fires.
            size_t alt_kept_old = mock_furi_string_size(&alt_store) - len;
            long deficit = (long)expected_new_kept_old - (long)alt_kept_old;
            CHECK(
                deficit >= 0,
                "ALT kept MORE old data than NEW (initial_size=%zu len=%zu) -- model mismatch",
                initial_size, len);
            total_alt_deficit += deficit;
            CHECK(
                alt_strlen == mock_furi_string_size(&alt_store),
                "ALT tracked_strlen (%zu) out of sync with real size (%zu)",
                alt_strlen, mock_furi_string_size(&alt_store));
        }
    }

    fprintf(
        stderr,
        "[real constants] %d truncation events, ALT discarded %ld extra bytes of "
        "already-captured swipe data beyond what NEW keeps (same inputs)\n",
        triggered, total_alt_deficit);
    CHECK(triggered > 0, "sweep produced no truncation events at all");
    CHECK(total_alt_deficit > 0, "ALT never lost more data than NEW under real constants");
}

// 2) Scaled-down store (small cap) to reach the len>=initial_size regime cheaply, proving
//    the *pattern* is a full-wipe hazard in general (not reachable today only because
//    UART_RX_BUF_SIZE=320 << UART_TERMINAL_TEXT_BOX_STORE_SIZE=4096 keeps len small).
static void sweep_small_store_full_wipe_hazard(void) {
    size_t store_cap = 64;
    int wipe_cases = 0;

    for(size_t len = 1; len <= 63; len++) {
        for(size_t initial_size = 1; initial_size <= store_cap - 2; initial_size++) {
            if(initial_size + len < store_cap - 1) continue; // trim not triggered
            if(len < initial_size) continue;                 // not the full-wipe regime

            fill_chunk(len);
            MockFuriString alt_store, new_store;
            mock_init(&alt_store, initial_size);
            mock_init(&new_store, initial_size);
            size_t alt_strlen = initial_size, new_strlen = initial_size;

            alt_uart_chunk(&alt_store, &alt_strlen, scratch_chunk, len, store_cap);
            new_uart_chunk(&new_store, &new_strlen, scratch_chunk, len, store_cap);

            size_t alt_kept_old = mock_furi_string_size(&alt_store) - len;
            size_t new_kept_old = mock_furi_string_size(&new_store) - len;

            CHECK(
                alt_kept_old == 0,
                "expected ALT to wipe all old data (initial_size=%zu len=%zu), kept %zu",
                initial_size, len, alt_kept_old);
            CHECK(
                new_kept_old == initial_size - (initial_size / 2),
                "NEW should still keep half the old data even in this regime "
                "(initial_size=%zu len=%zu), kept %zu",
                initial_size, len, new_kept_old);
            wipe_cases++;
        }
    }

    fprintf(
        stderr,
        "[len>=initial_size regime, scaled-down store] %d cases: ALT wipes ALL "
        "previously-captured data every time, NEW still keeps half\n",
        wipe_cases);
    CHECK(wipe_cases > 0, "no len>=initial_size cases exercised");
}

// 3) Below-threshold traffic never trims for either version (no regression on the
//    common case of a card swipe that never gets near the 4096-byte cap).
static void sweep_below_threshold_no_trim(void) {
    size_t store_cap = STORE_SIZE_REAL;
    for(size_t initial_size = 0; initial_size <= 256; initial_size += 8) {
        for(size_t len = 1; len <= MAX_LEN_REAL; len += 17) {
            if(initial_size + len >= store_cap - 1) continue;
            fill_chunk(len);
            MockFuriString alt_store, new_store;
            mock_init(&alt_store, initial_size);
            mock_init(&new_store, initial_size);
            size_t alt_strlen = initial_size, new_strlen = initial_size;

            alt_uart_chunk(&alt_store, &alt_strlen, scratch_chunk, len, store_cap);
            new_uart_chunk(&new_store, &new_strlen, scratch_chunk, len, store_cap);

            CHECK(
                mock_furi_string_size(&alt_store) == initial_size + len &&
                    mock_furi_string_size(&new_store) == initial_size + len,
                "unexpected trim below threshold (initial_size=%zu len=%zu)",
                initial_size, len);
            CHECK(
                alt_strlen == new_strlen && alt_strlen == initial_size + len,
                "tracked_strlen mismatch below threshold (initial_size=%zu len=%zu)",
                initial_size, len);
        }
    }
}

int main(void) {
    sweep_real_constants();
    sweep_small_store_full_wipe_hazard();
    sweep_below_threshold_no_trim();

    fprintf(stderr, "\n%d checks, %d failures\n", checks, failures);
    if(failures == 0) {
        fprintf(stderr, "ALL CHECKS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
