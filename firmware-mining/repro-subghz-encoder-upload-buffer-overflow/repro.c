// Host repro for the SubGhz encoder upload-buffer overflow (see
// firmware-mining/subghz-encoder-upload-buffer-overflow.md).
//
// Compiles the real, unmodified subghz_protocol_blocks_get_upload_from_bit_array()
// from my own mntm-012 base (encoder_base.c, "ALT") and the patched version
// (encoder_fixed.c, "NEW", matching Unleashed commit cc2d85278) into the same
// binary under renamed symbols (via -D at compile time), so both real,
// compiled functions can be called and compared directly.
//
// Each case runs in a forked, ASan-instrumented child so a heap-buffer-overflow
// abort in one case does not take down the rest of the test run; the parent
// only inspects the child's exit status. furi_crash() (the firmware's own
// guard) is stubbed to _exit(42), so "the guard fired" (42), "ASan caught an
// OOB write" (ASan's own fatal-error exit, not 42 or 0) and "returned cleanly"
// (0) are all distinguishable from the parent.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "level_duration.h"

typedef enum {
    SubGhzProtocolBlockAlignBitLeft,
    SubGhzProtocolBlockAlignBitRight,
} SubGhzProtocolBlockAlignBit;

// ALT = encoder_base.c (my unmodified mntm-012 base), compiled with renamed symbols.
void alt_set_bit_array(bool bit_value, uint8_t data_array[], size_t set_index_bit, size_t max_size_array);
bool alt_get_bit_array(uint8_t data_array[], size_t read_index_bit);
size_t alt_get_upload_from_bit_array(
    uint8_t data_array[],
    size_t count_bit_data_array,
    LevelDuration* upload,
    size_t max_size_upload,
    uint32_t duration_bit,
    SubGhzProtocolBlockAlignBit align_bit);

// NEW = encoder_fixed.c (the proposed patch), compiled with renamed symbols.
void new_set_bit_array(bool bit_value, uint8_t data_array[], size_t set_index_bit, size_t max_size_array);
bool new_get_bit_array(uint8_t data_array[], size_t read_index_bit);
size_t new_get_upload_from_bit_array(
    uint8_t data_array[],
    size_t count_bit_data_array,
    LevelDuration* upload,
    size_t max_size_upload,
    uint32_t duration_bit,
    SubGhzProtocolBlockAlignBit align_bit);

#define EXIT_GUARD_CRASH 42
#define EXIT_CLEAN       0

// Builds an alternating 0/1/0/1/... bit pattern of bit_count bits: every single
// bit differs from its predecessor, so every step of the encoder loop is a
// "transition" and produces one upload entry -- the worst case for the number
// of upload entries per bit consumed.
static void fill_alternating(uint8_t* data_array, size_t byte_count, size_t bit_count) {
    memset(data_array, 0, byte_count);
    for(size_t i = 0; i < bit_count; i++) {
        bool bit_value = (i % 2) != 0;
        alt_set_bit_array(bit_value, data_array, i, byte_count);
    }
}

// A short, realistic fixed-code-style bit pattern (24 bits, a handful of runs,
// nowhere near exhausting a generously sized upload buffer) for the regression case.
static void fill_realistic(uint8_t* data_array, size_t byte_count) {
    memset(data_array, 0, byte_count);
    static const bool bits[24] = {
        1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1,
    };
    for(size_t i = 0; i < 24; i++) {
        alt_set_bit_array(bits[i], data_array, i, byte_count);
    }
}

typedef struct {
    int exit_code;
    bool timed_out;
} ChildResult;

// Runs fn() in a forked child so an ASan abort in one scenario cannot affect
// the others; returns the child's (possibly ASan-remapped) exit code.
static ChildResult run_isolated(void (*fn)(void)) {
    fflush(NULL);
    pid_t pid = fork();
    if(pid == 0) {
        fn();
        _exit(EXIT_CLEAN);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ChildResult result = {0};
    if(WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        // Killed by a signal (e.g. ASan configured to abort rather than exit).
        result.exit_code = -1;
    }
    return result;
}

// ---- Case B: the final, wholly unguarded upload[size_upload++] after the loop ----
// N = max_size_upload + 1 alternating bits: max_size_upload mid-loop writes fill
// indices 0..max_size_upload-1 exactly (no mid-loop check ever trips), then the
// unconditional post-loop flush writes index max_size_upload -- one past the
// allocated buffer, in ALT with *no* guard anywhere on this line at all.
#define CASE_B_MAX_UPLOAD 8
#define CASE_B_BITS       (CASE_B_MAX_UPLOAD + 1)

static void case_b_alt(void) {
    uint8_t data[8];
    fill_alternating(data, sizeof(data), CASE_B_BITS);
    LevelDuration* upload = malloc(CASE_B_MAX_UPLOAD * sizeof(LevelDuration));
    size_t n = alt_get_upload_from_bit_array(
        data, CASE_B_BITS, upload, CASE_B_MAX_UPLOAD, 100, SubGhzProtocolBlockAlignBitLeft);
    (void)n;
    free(upload);
}

static void case_b_new(void) {
    uint8_t data[8];
    fill_alternating(data, sizeof(data), CASE_B_BITS);
    LevelDuration* upload = malloc(CASE_B_MAX_UPLOAD * sizeof(LevelDuration));
    size_t n = new_get_upload_from_bit_array(
        data, CASE_B_BITS, upload, CASE_B_MAX_UPLOAD, 100, SubGhzProtocolBlockAlignBitLeft);
    (void)n;
    free(upload);
}

// ---- Case A: the mid-loop "size_upload > max_size_upload" off-by-one ----
// N = max_size_upload + 2 alternating bits: the first max_size_upload mid-loop
// writes fill indices 0..max_size_upload-1 safely, the (max_size_upload+1)-th
// mid-loop write is the one ALT's off-by-one check lets through at
// size_upload == max_size_upload (OOB write at that exact index).
#define CASE_A_MAX_UPLOAD 8
#define CASE_A_BITS       (CASE_A_MAX_UPLOAD + 2)

static void case_a_alt(void) {
    uint8_t data[8];
    fill_alternating(data, sizeof(data), CASE_A_BITS);
    LevelDuration* upload = malloc(CASE_A_MAX_UPLOAD * sizeof(LevelDuration));
    size_t n = alt_get_upload_from_bit_array(
        data, CASE_A_BITS, upload, CASE_A_MAX_UPLOAD, 100, SubGhzProtocolBlockAlignBitLeft);
    (void)n;
    free(upload);
}

static void case_a_new(void) {
    uint8_t data[8];
    fill_alternating(data, sizeof(data), CASE_A_BITS);
    LevelDuration* upload = malloc(CASE_A_MAX_UPLOAD * sizeof(LevelDuration));
    size_t n = new_get_upload_from_bit_array(
        data, CASE_A_BITS, upload, CASE_A_MAX_UPLOAD, 100, SubGhzProtocolBlockAlignBitLeft);
    (void)n;
    free(upload);
}

// ---- Regression: a realistic, far-from-the-limit 24-bit code must round-trip
// byte-for-byte identically between ALT and NEW, with no crash on either side.
#define REGRESSION_MAX_UPLOAD 64

static int regression_case(void) {
    uint8_t data_alt[3];
    uint8_t data_new[3];
    fill_realistic(data_alt, sizeof(data_alt));
    fill_realistic(data_new, sizeof(data_new));

    LevelDuration upload_alt[REGRESSION_MAX_UPLOAD];
    LevelDuration upload_new[REGRESSION_MAX_UPLOAD];

    size_t n_alt = alt_get_upload_from_bit_array(
        data_alt, 24, upload_alt, REGRESSION_MAX_UPLOAD, 100, SubGhzProtocolBlockAlignBitLeft);
    size_t n_new = new_get_upload_from_bit_array(
        data_new, 24, upload_new, REGRESSION_MAX_UPLOAD, 100, SubGhzProtocolBlockAlignBitLeft);

    if(n_alt != n_new) {
        printf("  FAIL: size_upload differs (ALT=%zu NEW=%zu)\n", n_alt, n_new);
        return 1;
    }
    if(memcmp(upload_alt, upload_new, n_alt * sizeof(LevelDuration)) != 0) {
        printf("  FAIL: upload[] content differs between ALT and NEW\n");
        return 1;
    }
    printf("  OK: ALT and NEW agree byte-for-byte (%zu upload entries)\n", n_alt);
    return 0;
}

static const char* describe(int exit_code) {
    switch(exit_code) {
    case EXIT_CLEAN:
        return "returned cleanly, no OOB write";
    case EXIT_GUARD_CRASH:
        return "furi_crash() guard fired before any OOB write";
    case -1:
        return "killed by signal";
    default:
        // ASan's own fatal-error exit path (its default is exit(1); it prints
        // the full heap-buffer-overflow report to stderr above this line).
        return "ASan: heap-buffer-overflow (OOB write happened, see report above)";
    }
}

int main(void) {
    int failures = 0;

    printf(
        "Case B (final unguarded upload[size_upload++] after the loop, %d bits into an "
        "%d-slot buffer):\n",
        CASE_B_BITS,
        CASE_B_MAX_UPLOAD);
    ChildResult b_alt = run_isolated(case_b_alt);
    ChildResult b_new = run_isolated(case_b_new);
    printf("  ALT: %s (exit %d)\n", describe(b_alt.exit_code), b_alt.exit_code);
    printf("  NEW: %s (exit %d)\n", describe(b_new.exit_code), b_new.exit_code);
    if(b_alt.exit_code == EXIT_CLEAN) {
        printf("  FAIL: ALT should have overflowed but returned cleanly\n");
        failures++;
    }
    if(b_new.exit_code != EXIT_GUARD_CRASH) {
        printf("  FAIL: NEW should have hit the new post-loop guard\n");
        failures++;
    }

    printf(
        "\nCase A (mid-loop 'size_upload > max_size_upload' off-by-one, %d bits into an "
        "%d-slot buffer):\n",
        CASE_A_BITS,
        CASE_A_MAX_UPLOAD);
    ChildResult a_alt = run_isolated(case_a_alt);
    ChildResult a_new = run_isolated(case_a_new);
    printf("  ALT: %s (exit %d)\n", describe(a_alt.exit_code), a_alt.exit_code);
    printf("  NEW: %s (exit %d)\n", describe(a_new.exit_code), a_new.exit_code);
    if(a_alt.exit_code == EXIT_CLEAN) {
        printf("  FAIL: ALT should have overflowed but returned cleanly\n");
        failures++;
    }
    if(a_new.exit_code != EXIT_GUARD_CRASH) {
        printf("  FAIL: NEW should have hit the tightened mid-loop guard\n");
        failures++;
    }

    printf("\nRegression (realistic 24-bit code, %d-slot buffer, nowhere near the limit):\n",
           REGRESSION_MAX_UPLOAD);
    failures += regression_case();

    if(failures == 0) {
        printf("\nALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("\n%d CHECK(S) FAILED\n", failures);
        return 1;
    }
}
