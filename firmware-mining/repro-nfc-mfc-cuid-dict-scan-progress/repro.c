// Host repro for the NFC MFC CUID-dictionary loading-popup wiring
// (nfc_scene_mf_classic_dict_attack.c + nfc_app.c/.h, this session's patch).
//
// Two independent pieces, each transcribed 1:1 from the real patched code:
//
//   1. percent_for(): the exact progress-fraction expression used inside the scan loop
//      (dict->total_keys * dict->key_size_symbols * 100 / dict_size, MIN-clamped to 100,
//      then /100.0f) -- checks it never exceeds 1.0f, is monotonic non-decreasing as more
//      keys are consumed, reaches (at least) very close to 1.0f by the last key of a
//      well-formed dictionary, and that the dict_size == 0 guard is never bypassed.
//
//   2. run_prepare_view_cuid_branch(): the real CUID `do { ... } while(false)` control-flow
//      skeleton from nfc_scene_mf_classic_dict_attack_prepare_view(), copied verbatim
//      (comments trimmed) with furi/storage/stream calls replaced by instrumented stand-ins
//      that record every nfc_show_loading_dict_popup()/nfc_set_loading_dict_progress() call --
//      checks the popup is shown exactly once before the scan and hidden exactly once after
//      it, on both the success and the "dictionary has zero keys" paths, and never shown at
//      all when the file fails to open (matching the three original `break` points).
//
// Not re-verified here (out of scope for this patch): the bitmap population loop itself
// and MfClassicKey/key-index semantics (pre-existing, unchanged by this patch) and
// loading_set_progress()/loading_set_text()/loading_reset_progress() themselves, already
// exhaustively host-tested against their real bodies in
// firmware-mining/repro-loading-widget-label-and-progress-bar/repro.c.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// ---------------------------------------------------------------------------
// Part 1: progress-fraction formula, extracted verbatim from the patched loop body.
// ---------------------------------------------------------------------------

static float percent_for(uint64_t total_keys_before, size_t key_size_symbols, size_t dict_size) {
    if(!dict_size) {
        // Guard branch: caller must not evaluate the division at all.
        assert(0 && "percent_for() must not be called when dict_size == 0");
    }
    const uint32_t percent = (uint32_t)(total_keys_before * key_size_symbols * 100 / dict_size);
    return MIN(percent, 100U) / 100.0f;
}

static void test_percent_formula(void) {
    // key_size_symbols for a CUID line: (sizeof(MfClassicKey)+1)*2+1 = 7*2+1 = 15, as in the
    // real code (dict->key_size = sizeof(MfClassicKey)+1 = 7, key_size_symbols = key_size*2+1).
    const size_t key_size_symbols = 15;
    long checks = 0;

    // Well-formed file: N lines of exactly key_size_symbols bytes each, no trailing junk.
    for(uint64_t n = 1; n <= 5000; n++) {
        const size_t dict_size = (size_t)n * key_size_symbols;
        float prev = -1.0f;
        for(uint64_t consumed = 0; consumed < n; consumed++) {
            float p = percent_for(consumed, key_size_symbols, dict_size);
            assert(p >= 0.0f && p <= 1.0f);
            assert(p >= prev); // monotonic non-decreasing as more keys are read
            prev = p;
            checks++;
        }
        // One key short of the full file (consumed == n-1, the last iteration above) must
        // already read essentially the whole file back as percent -- within the +-1 whole
        // percentage point that truncating integer division can lose.
        float last = percent_for(n - 1, key_size_symbols, dict_size);
        assert(last >= (float)(n - 1) / (float)n - 0.0101f);
    }

    // Trailing content past the last full line (e.g. no final newline byte counted, or a
    // few stray bytes) makes the raw ratio exceed 100 before clamping -- must clamp to 1.0f,
    // never assert/overflow/return >1.0f.
    for(uint64_t n = 1; n <= 200; n++) {
        for(size_t short_by = 1; short_by <= key_size_symbols; short_by++) {
            const size_t dict_size = (size_t)n * key_size_symbols - short_by;
            if(!dict_size) continue;
            float p = percent_for(n, key_size_symbols, dict_size); // n keys already counted
            assert(p <= 1.0f);
            assert(p == 1.0f); // n*key_size_symbols/dict_size is always > 1 here
            checks++;
        }
    }

    // Single-key dictionaries and the very first iteration (0 keys counted yet) must not
    // divide by zero and must read as 0%.
    assert(percent_for(0, key_size_symbols, key_size_symbols) == 0.0f);
    assert(percent_for(0, key_size_symbols, 1000000) == 0.0f);
    checks += 2;

    printf("[percent_for] %ld checks passed\n", checks);
}

// ---------------------------------------------------------------------------
// Part 2: control-flow skeleton of the CUID branch, verbatim structure, instrumented I/O.
// ---------------------------------------------------------------------------

typedef struct {
    size_t key_size;
    size_t key_size_symbols;
    uint64_t total_keys;
    // test fixture: how many keys this fake dictionary yields, and the fake byte size
    // stream_size() reports for it (0 disables progress, matching a failed/empty size query).
    int keys_to_yield;
    int keys_yielded;
    size_t fake_dict_size;
} KeysDict;

typedef struct {
    bool open_should_fail;
    bool file_exists;
    int total_keys_in_file; // 0 means "present but empty", matching the real total_keys==0 path
    size_t fake_dict_size;
    int keys_yielded;

    // instrumentation
    int show_calls;
    int hide_calls;
    int progress_calls;
    bool shown_now;
} Fixture;

static bool fx_keys_dict_check_presence(Fixture* fx) {
    return fx->file_exists;
}

static bool fx_buffered_file_stream_open(Fixture* fx) {
    return !fx->open_should_fail;
}

static size_t fx_stream_size(Fixture* fx) {
    return fx->fake_dict_size;
}

static bool fx_keys_dict_get_next_key(Fixture* fx, KeysDict* dict) {
    (void)dict;
    if(fx->keys_yielded >= fx->total_keys_in_file) return false;
    fx->keys_yielded++;
    return true;
}

static void fx_show_popup(Fixture* fx) {
    fx->show_calls++;
    assert(!fx->shown_now && "popup shown twice without an intervening hide");
    fx->shown_now = true;
}

static void fx_hide_popup(Fixture* fx) {
    fx->hide_calls++;
    assert(fx->shown_now && "popup hidden without having been shown");
    fx->shown_now = false;
}

static void fx_set_progress(Fixture* fx) {
    fx->progress_calls++;
    assert(fx->shown_now && "progress set while popup not shown");
}

// Verbatim transcription of the patched CUID `do { ... } while(false)` skeleton, with the
// bitmap-population and dict-path bookkeeping stripped (unchanged by this patch, already
// covered by earlier sessions) but every branch, break, and popup call kept in the same
// place and order as the real file.
typedef enum { OutcomeNoCuidFile, OutcomeOpenFailed, OutcomeEmptyDict, OutcomeSuccess } Outcome;

// Note: the real file uses `do { ... break; ... } while(false)` so the early-exit paths can
// fall through into the (unrelated, not part of this patch) non-CUID dictionary fallback
// code that follows the block. That fallback isn't reproduced here, so each early exit is a
// plain `return` of the outcome it would have `break`-ed to instead -- the show/hide call
// sites, their order, and every branch condition are otherwise identical to the real file.
static Outcome run_prepare_view_cuid_branch(Fixture* fx) {
    if(!fx_keys_dict_check_presence(fx)) {
        return OutcomeNoCuidFile;
    }

    KeysDict dict_storage = {0};
    KeysDict* dict = &dict_storage;
    dict->key_size = 7;
    dict->key_size_symbols = dict->key_size * 2 + 1;
    dict->total_keys = 0;
    dict->keys_to_yield = fx->total_keys_in_file;
    dict->fake_dict_size = fx->fake_dict_size;

    if(!fx_buffered_file_stream_open(fx)) {
        return OutcomeOpenFailed;
    }

    // -- this session's addition --
    fx_show_popup(fx);
    const size_t dict_size = fx_stream_size(fx);
    while(fx_keys_dict_get_next_key(fx, dict)) {
        if(dict_size) {
            fx_set_progress(fx);
        }
        dict->total_keys++;
    }
    fx_hide_popup(fx);
    // -- end this session's addition --

    if(dict->total_keys == 0) {
        return OutcomeEmptyDict;
    }

    return OutcomeSuccess;
}

static void test_control_flow(void) {
    long checks = 0;

    // No CUID dictionary file at all for this UID: never touches the popup.
    {
        Fixture fx = {.file_exists = false};
        Outcome o = run_prepare_view_cuid_branch(&fx);
        assert(o == OutcomeNoCuidFile);
        assert(fx.show_calls == 0 && fx.hide_calls == 0 && fx.progress_calls == 0);
        checks++;
    }

    // File exists but fails to open: never touches the popup either (break happens before
    // the show call in the real code).
    {
        Fixture fx = {.file_exists = true, .open_should_fail = true};
        Outcome o = run_prepare_view_cuid_branch(&fx);
        assert(o == OutcomeOpenFailed);
        assert(fx.show_calls == 0 && fx.hide_calls == 0 && fx.progress_calls == 0);
        checks++;
    }

    // File opens but is empty (0 keys): popup shown once, hidden once, no progress updates
    // (the while loop body never runs), and hide happens before the empty check runs.
    {
        Fixture fx = {.file_exists = true, .total_keys_in_file = 0, .fake_dict_size = 15};
        Outcome o = run_prepare_view_cuid_branch(&fx);
        assert(o == OutcomeEmptyDict);
        assert(fx.show_calls == 1 && fx.hide_calls == 1);
        assert(fx.progress_calls == 0);
        assert(!fx.shown_now);
        checks++;
    }

    // Normal dictionary: shown once, hidden once, exactly one progress update per key
    // (dict_size nonzero), popup is down again before the caller can inspect total_keys.
    for(int n = 1; n <= 2000; n += 37) {
        Fixture fx = {
            .file_exists = true, .total_keys_in_file = n, .fake_dict_size = (size_t)n * 15};
        Outcome o = run_prepare_view_cuid_branch(&fx);
        assert(o == OutcomeSuccess);
        assert(fx.show_calls == 1 && fx.hide_calls == 1);
        assert(fx.progress_calls == n);
        assert(!fx.shown_now);
        checks++;
    }

    // dict_size == 0 (a failed/degenerate size query): popup still shown/hidden around the
    // scan (user still gets the spinner + label), but never told to update the bar.
    {
        Fixture fx = {.file_exists = true, .total_keys_in_file = 50, .fake_dict_size = 0};
        Outcome o = run_prepare_view_cuid_branch(&fx);
        assert(o == OutcomeSuccess);
        assert(fx.show_calls == 1 && fx.hide_calls == 1);
        assert(fx.progress_calls == 0);
        checks++;
    }

    printf("[control_flow] %ld checks passed\n", checks);
}

int main(void) {
    test_percent_formula();
    test_control_flow();
    printf("ALL CHECKS PASSED\n");
    return 0;
}
