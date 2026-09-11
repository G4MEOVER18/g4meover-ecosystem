// Host repro for the unreachable "restart preamble" branch inside
// FordV3DecoderStepData in protocols/ford_v3.c (subghz_protocol_decoder_ford_v3_feed()).
//
// The branch under test, reproduced verbatim from the original file:
//
//   case FordV3DecoderStepData: {
//       if (!pp_is_short(duration, &subghz_protocol_ford_v3_const) &&
//           !pp_is_long(duration, &subghz_protocol_ford_v3_const)) {
//           ford_v3_emit_if_ready(instance);
//           instance->decoder.parser_step = FordV3DecoderStepReset;
//
//           if (pp_is_short(duration, &subghz_protocol_ford_v3_const)) {   // <-- always false here
//               ford_v3_reset_data(instance);
//               instance->preamble_count = 1U;
//               instance->decoder.parser_step = FordV3DecoderStepPreamble;
//           }
//           break;
//       }
//       ...
//
// pp_is_short()/pp_is_long() are pure functions of (duration, protocol const) with no side
// effects (protocols/protocols_common.h:92-98). The inner check re-evaluates pp_is_short() on
// the SAME `duration` that the outer `if` has just proven to fail pp_is_short() (that's the
// left-hand side of the outer `&&`) -- so the inner check can never be true. It is a leftover
// copy-paste of the three lines used in FordV3DecoderStepReset (lines 133-138 of the original),
// evidently meant to let an invalid pulse double as the first short pulse of a new preamble in
// the same feed() call, but wired to check the pulse that was just rejected instead of the next
// one.
//
// This program proves two things on the host:
//   1. Exhaustively, for every duration in a wide range, that whenever the outer condition
//      (neither short nor long) holds, the inner pp_is_short() check is false -- i.e. the branch
//      is unreachable by construction, not just "didn't happen to trigger in my test data".
//   2. By randomized simulation of the OLD vs. a NEW decoder with the dead branch deleted, that
//      parser_step / preamble_count / bit_count after every fed pulse are identical for OLD and
//      NEW across thousands of random (level, duration) pulse streams -- i.e. deleting the dead
//      branch is behavior-preserving.
//
// Build & run:
//   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Actual output when run:
//   [1/2] exhaustive duration sweep (0..20000): inner pp_is_short() fired 0 / 20001 times
//   [2/2] randomized OLD-vs-NEW equivalence: 200000 pulses fed, 0 mismatches, dead branch entered 0 times

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FORD_V3_TE_SHORT  240U
#define FORD_V3_TE_LONG   480U
#define FORD_V3_TE_DELTA  60U
#define FORD_V3_DATA_BITS 104U
#define FORD_V3_PREAMBLE_MIN 30U

// DURATION_DIFF as defined by the Flipper Zero firmware SDK (lib/subghz/blocks/const.h):
// absolute difference between two durations.
static uint32_t DURATION_DIFF(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}

static bool pp_is_short(uint32_t duration) {
    return DURATION_DIFF(duration, FORD_V3_TE_SHORT) < FORD_V3_TE_DELTA;
}

static bool pp_is_long(uint32_t duration) {
    return DURATION_DIFF(duration, FORD_V3_TE_LONG) < FORD_V3_TE_DELTA;
}

// ---------------------------------------------------------------------------
// Part 1: exhaustive proof that the inner check is unreachable.
// ---------------------------------------------------------------------------
static void exhaustive_sweep(void) {
    unsigned long fired = 0;
    unsigned long total = 0;
    for(uint32_t duration = 0; duration <= 20000U; duration++) {
        total++;
        if(!pp_is_short(duration) && !pp_is_long(duration)) {
            // This is the OLD code's inner condition, evaluated exactly as written.
            if(pp_is_short(duration)) {
                fired++;
            }
        }
    }
    printf(
        "[1/2] exhaustive duration sweep (0..20000): inner pp_is_short() fired %lu / %lu times\n",
        fired,
        total);
    assert(fired == 0);
}

// ---------------------------------------------------------------------------
// Part 2: OLD vs NEW decoder-step equivalence under randomized pulse streams.
// Only the parser_step / preamble_count / bit_count bookkeeping that the dead
// branch could affect is modeled; the Manchester bit extraction itself is
// untouched by the fix and is represented by a shared stand-in so it is
// identical in both versions by construction.
// ---------------------------------------------------------------------------

typedef enum { StepReset = 0, StepPreamble = 1, StepData = 2 } Step;

typedef struct {
    Step step;
    uint16_t preamble_count;
    uint16_t bit_count; // stand-in for manchester-derived bit progress
} DecState;

static void reset_data(DecState* s) {
    s->bit_count = 0;
    s->preamble_count = 0;
}

// Shared "manchester consumed a bit" stand-in: deterministic function of
// (duration, level) so OLD and NEW see the same bit progress.
static bool manchester_consumed_bit(uint32_t duration, bool level) {
    return ((duration + (level ? 1U : 0U)) % 3U) != 0U;
}

static void feed_data_step_OLD(DecState* s, bool level, uint32_t duration, unsigned long* dead_branch_hits) {
    if(!pp_is_short(duration) && !pp_is_long(duration)) {
        // ford_v3_emit_if_ready() has no observable effect on step/preamble_count/bit_count
        // beyond what happens below, so it is omitted here (it only invokes the base callback).
        s->step = StepReset;

        if(pp_is_short(duration)) { // dead branch under test
            (*dead_branch_hits)++;
            reset_data(s);
            s->preamble_count = 1U;
            s->step = StepPreamble;
        }
        return;
    }

    if(manchester_consumed_bit(duration, level)) {
        if(s->bit_count < FORD_V3_DATA_BITS) {
            s->bit_count++;
        }
        if(s->bit_count >= FORD_V3_DATA_BITS) {
            s->step = StepReset;
        }
    }
}

static void feed_data_step_NEW(DecState* s, bool level, uint32_t duration) {
    if(!pp_is_short(duration) && !pp_is_long(duration)) {
        // Dead branch removed -- behavior is identical to OLD because OLD's inner
        // condition could never fire (see Part 1).
        s->step = StepReset;
        return;
    }

    if(manchester_consumed_bit(duration, level)) {
        if(s->bit_count < FORD_V3_DATA_BITS) {
            s->bit_count++;
        }
        if(s->bit_count >= FORD_V3_DATA_BITS) {
            s->step = StepReset;
        }
    }
}

static void randomized_equivalence(void) {
    srand(12345);
    unsigned long mismatches = 0;
    unsigned long dead_branch_hits = 0;
    unsigned long pulses = 0;

    for(int stream = 0; stream < 2000; stream++) {
        DecState old_s = {.step = StepData, .preamble_count = FORD_V3_PREAMBLE_MIN, .bit_count = 0};
        DecState new_s = old_s;

        for(int i = 0; i < 100; i++) {
            bool level = (rand() % 2) != 0;
            // Bias durations to land both inside and outside the short/long windows.
            uint32_t duration = (uint32_t)(rand() % 1000);
            pulses++;

            feed_data_step_OLD(&old_s, level, duration, &dead_branch_hits);
            feed_data_step_NEW(&new_s, level, duration);

            if(old_s.step != new_s.step || old_s.preamble_count != new_s.preamble_count ||
               old_s.bit_count != new_s.bit_count) {
                mismatches++;
            }

            if(old_s.step == StepReset) {
                // Both would transition onward via the outer switch's Reset case on the
                // next feed() call in the real decoder; re-arm both sides identically here
                // to keep simulating a continuous stream.
                old_s.step = StepData;
                new_s.step = StepData;
            }
        }
    }

    printf(
        "[2/2] randomized OLD-vs-NEW equivalence: %lu pulses fed, %lu mismatches, dead branch entered %lu times\n",
        pulses,
        mismatches,
        dead_branch_hits);
    assert(mismatches == 0);
    assert(dead_branch_hits == 0);
}

int main(void) {
    exhaustive_sweep();
    randomized_equivalence();
    printf("OK: dead branch is unreachable and its removal is behavior-preserving.\n");
    return 0;
}
