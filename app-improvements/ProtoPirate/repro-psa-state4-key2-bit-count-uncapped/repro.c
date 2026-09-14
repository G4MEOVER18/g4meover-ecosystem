// Host repro for the PSA State4 decode_count_bit overflow bug.
//
// Models the two independent bit-counting paths in protocols/psa.c::subghz_protocol_decoder_psa_feed()
// that both lead into the same end-of-frame check `decode_count_bit != PSA_KEY2_BITS`:
//
//   State2 (line ~1076): increments decode_count_bit only while it is still below
//     PSA_KEY2_BITS -- `if(should_process && instance->decode_count_bit < PSA_KEY2_BITS) { ... }`
//
//   State4 (line ~1209): increments decode_count_bit unconditionally on every successfully
//     decoded manchester bit, with no PSA_KEY2_BITS cap -- only the much looser
//     `decode_count_bit >= PSA_MAX_BITS` (121) top-of-case guard.
//
// Both states funnel into the identical acceptance test before calling psa_handle_decoded_frame():
//     if(instance->decode_count_bit != PSA_KEY2_BITS) { return; }
//
// This repro shows that any number of extra manchester-decodable bit transitions arriving
// after the 80th real data bit (spurious short-pulse noise/jitter before the true end-of-frame
// long gap is recognized as such -- physically plausible, and exactly what State2 already
// defends against) causes State4 to silently drop an otherwise fully decoded, valid capture,
// while State2 accepts the identical capture unaffected.
//
// gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#define PSA_KEY1_BITS 64
#define PSA_KEY2_BITS 80
#define PSA_MAX_BITS  121

// ALT: State4's actual logic -- unconditional increment, only the loose top-of-case guard.
static int feed_state4_alt(int real_bits, int extra_bits) {
    int decode_count_bit = 0;
    int total_edges = real_bits + extra_bits;
    for(int i = 0; i < total_edges; i++) {
        if(decode_count_bit >= PSA_MAX_BITS) break; // mirrors the top-of-case reset-to-idle guard
        decode_count_bit++;                         // unconditional -- the bug
    }
    // end-of-frame acceptance test, identical text in both states
    return decode_count_bit == PSA_KEY2_BITS;
}

// NEU (fix): mirror State2's existing guard inside State4's bit-decode branch.
static int feed_state4_fixed(int real_bits, int extra_bits) {
    int decode_count_bit = 0;
    int total_edges = real_bits + extra_bits;
    for(int i = 0; i < total_edges; i++) {
        if(decode_count_bit < PSA_KEY2_BITS) {
            decode_count_bit++;
        }
        // extra edges once count==PSA_KEY2_BITS are silently absorbed, exactly like State2
    }
    return decode_count_bit == PSA_KEY2_BITS;
}

// Reference: State2's actual existing logic, unchanged, used only to confirm it is unaffected
// by the same extra-edge scenario (it already has the guard).
static int feed_state2_existing(int real_bits, int extra_bits) {
    int decode_count_bit = 0;
    int total_edges = real_bits + extra_bits;
    for(int i = 0; i < total_edges; i++) {
        if(decode_count_bit < PSA_KEY2_BITS) {
            decode_count_bit++;
        }
    }
    return decode_count_bit == PSA_KEY2_BITS;
}

int main(void) {
    int alt_false_negatives = 0;
    int fixed_false_negatives = 0;
    int state2_false_negatives = 0;
    int total_cases = 0;

    // A fully valid capture always delivers exactly PSA_KEY2_BITS (80) real data-bit edges
    // before the end-of-frame gap. extra_bits models 0..5 spurious trailing bit-edges that
    // are physically plausible jitter/noise before the true long end-gap is recognized.
    for(int extra_bits = 0; extra_bits <= 5; extra_bits++) {
        total_cases++;
        int alt = feed_state4_alt(PSA_KEY2_BITS, extra_bits);
        int fixed = feed_state4_fixed(PSA_KEY2_BITS, extra_bits);
        int s2 = feed_state2_existing(PSA_KEY2_BITS, extra_bits);

        printf(
            "extra_bits=%d  State4-ALT accepted=%d  State4-FIXED accepted=%d  State2(existing) accepted=%d\n",
            extra_bits,
            alt,
            fixed,
            s2);

        if(extra_bits == 0) {
            // Baseline: no trailing noise -- all three must accept a genuinely valid capture.
            assert(alt == 1);
            assert(fixed == 1);
            assert(s2 == 1);
        } else {
            // With any trailing noise, only the unpatched State4 loses the frame.
            if(!alt) alt_false_negatives++;
            if(!fixed) fixed_false_negatives++;
            if(!s2) state2_false_negatives++;
        }
    }

    // Regression check: a genuinely short/incomplete capture (fewer than 80 real bits, no
    // extra noise) must still be correctly rejected by all three -- the fix must not turn
    // truncated captures into false accepts.
    for(int missing = 1; missing <= 5; missing++) {
        int real_bits = PSA_KEY2_BITS - missing;
        int alt = feed_state4_alt(real_bits, 0);
        int fixed = feed_state4_fixed(real_bits, 0);
        int s2 = feed_state2_existing(real_bits, 0);
        assert(alt == 0);
        assert(fixed == 0);
        assert(s2 == 0);
    }

    printf("\n--- summary over %d trailing-noise cases (extra_bits=1..5) ---\n", total_cases - 1);
    printf(
        "State4-ALT   false negatives (valid capture dropped): %d / %d\n",
        alt_false_negatives,
        total_cases - 1);
    printf(
        "State4-FIXED false negatives (valid capture dropped): %d / %d\n",
        fixed_false_negatives,
        total_cases - 1);
    printf(
        "State2 (existing, unchanged) false negatives:         %d / %d\n",
        state2_false_negatives,
        total_cases - 1);

    assert(alt_false_negatives == total_cases - 1); // ALT loses the frame on every single case
    assert(fixed_false_negatives == 0);             // fix recovers all of them
    assert(state2_false_negatives == 0);             // State2 was never affected

    printf(
        "\nOK: State4-ALT drops every trailing-noise capture that State2 and State4-FIXED both "
        "still accept correctly; truncated captures remain correctly rejected by all three.\n");
    return 0;
}
