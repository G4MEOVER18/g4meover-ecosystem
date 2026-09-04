// Host repro: Mifare Fuzzer "Load UIDs from file" line-selection loop.
// Mirrors the exact control flow of
// mifare_fuzzer_scene_emulator.c :: mifare_fuzzer_scene_emulator_on_event()
// case MifareFuzzerEventStartAttack, attack == MifareFuzzerAttackLoadUidsFromFile,
// with a plain in-memory line list standing in for the Flipper Stream/FuriString
// API (stream_read_line() / stream_rewind()). The bug does not depend on any
// Flipper-specific byte semantics of stream_read_line() -- only on the fact
// that "no line in the file ever satisfies the filter" makes the ALT loop
// retry forever.
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    const char** lines;
    size_t count;
    size_t pos;
} LineStream;

// Mimics stream_read_line(): returns false at EOF, true with *out set
// otherwise. Does not include a trailing newline (irrelevant to the defect).
static bool stream_read_line_stub(LineStream* s, const char** out) {
    if(s->pos >= s->count) return false;
    *out = s->lines[s->pos++];
    return true;
}
static void stream_rewind_stub(LineStream* s) {
    s->pos = 0;
}

#define MAX_ITER 1000000u // generous cap so a hang is detected instead of the test itself hanging

// --- ALT: verbatim translation of the shipped loop ---
// Returns false if it "hung" (iteration cap hit), true if it terminated normally.
static bool alt_select_line(LineStream* s, const char** chosen, unsigned* iterations) {
    *iterations = 0;
    while(true) {
        if(++(*iterations) > MAX_ITER) return false; // stands in for "never returns"
        const char* line;
        if(!stream_read_line_stub(s, &line)) {
            stream_rewind_stub(s);
            continue;
        }
        if(line[0] == '#') continue;
        size_t len = strlen(line);
        if(len != 8 && len != 14) continue; // 4-byte / 7-byte UID as hex, sans newline
        *chosen = line;
        return true;
    }
}

// --- NEU: same loop, but gives up after one full pass finds nothing usable ---
static bool neu_select_line(LineStream* s, const char** chosen, unsigned* iterations) {
    *iterations = 0;
    bool found_any_this_pass = false;
    while(true) {
        if(++(*iterations) > MAX_ITER) return false;
        const char* line;
        if(!stream_read_line_stub(s, &line)) {
            if(!found_any_this_pass) {
                return false; // whole file scanned once, nothing usable -> stop, don't spin
            }
            stream_rewind_stub(s);
            found_any_this_pass = false;
            continue;
        }
        if(line[0] == '#') continue;
        size_t len = strlen(line);
        if(len != 8 && len != 14) continue;
        found_any_this_pass = true;
        *chosen = line;
        return true;
    }
}

static void run_case(const char* name, const char** lines, size_t count) {
    LineStream s_alt = {lines, count, 0};
    LineStream s_neu = {lines, count, 0};
    const char* chosen = NULL;
    unsigned iters = 0;

    bool alt_ok = alt_select_line(&s_alt, &chosen, &iters);
    printf(
        "[%s] ALT: %s after %u iterations%s\n",
        name,
        alt_ok ? "terminated" : "DID NOT TERMINATE (hang)",
        iters,
        alt_ok ? "" : " (hit iteration cap)");

    chosen = NULL;
    iters = 0;
    bool neu_ok = neu_select_line(&s_neu, &chosen, &iters);
    printf(
        "[%s] NEU: %s after %u iterations%s\n",
        name,
        neu_ok ? "terminated with a UID" : "terminated with 'no usable line' (attack stopped, no hang)",
        iters,
        "");
}

int main(void) {
    // Case 1: a normal file with a mix of comments, blanks-would-be-skipped-by-length,
    // and two valid 4-byte-UID lines -- both ALT and NEU must terminate quickly and
    // keep cycling correctly (regression check: NEU must not break normal use).
    const char* good_lines[] = {"# header", "11223344", "AABBCCDD"};
    run_case("valid-file", good_lines, sizeof(good_lines) / sizeof(good_lines[0]));

    // Case 2: comment-only file (e.g. a template the user forgot to fill in).
    const char* comment_only[] = {"# put your UIDs below", "# 11223344"};
    run_case("comment-only-file", comment_only, sizeof(comment_only) / sizeof(comment_only[0]));

    // Case 3: completely empty file.
    const char* empty_lines[] = {0};
    run_case("empty-file", empty_lines, 0);

    // Case 4: wrong-length lines only (e.g. a 10-byte UID, unsupported by this app).
    const char* wrong_len[] = {"0102030405"};
    run_case("wrong-length-only-file", wrong_len, sizeof(wrong_len) / sizeof(wrong_len[0]));

    return 0;
}
