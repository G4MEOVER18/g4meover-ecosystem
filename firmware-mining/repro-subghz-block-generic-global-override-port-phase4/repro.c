/* Host repro for Phase 4 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter override mechanism into Security+ 1.0 (secplus_v1) --
 * the second protocol of the Nachzieh-Liste after Princeton (Phase 3), and the
 * first one that actually exercises the *counter* half of the mechanism (Princeton
 * is fixed-code and only used the button half; KeeLoq in Phase 2 already proved the
 * counter half, but secplus_v1's own increment logic is structurally different from
 * KeeLoq's mode table -- see the accompanying .md's "Phase 4: Security+ 1.0").
 *
 * This does NOT compile the full, real secplus_v1.c (it needs the TE/duration
 * encoder state machine and the base-3 digit-array reconstruction, neither of
 * which this change touches). Instead it `#include`s the exact statements the
 * patch adds plus the pre-existing statements they are spliced into, extracted
 * byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/secplus_v1.c
 * (see the sed line ranges below), and links them against the real, unmodified
 * Phase-1 generic.c -- the actual subghz_block_generic_global_counter_override_get()
 * this patch calls. Everything surrounding those statements in the real file (the
 * base-3 digit encoding, the fixed-code bound check, the printf formatting) is
 * pre-existing, untouched code and is not re-verified here.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;
int32_t g_rolling_counter_mult = 1;

typedef struct {
    SubGhzBlockGeneric generic;
} MiniSecPlusV1Instance;

/* --- Fragment 1: the patched counter-override branch plus the pre-existing
   multicntr setup and OFEX else-branch it is spliced into, from
   subghz_protocol_secplus_v1_encode() (sed 228,274p on the shipped secplus_v1.c).
   Verbatim -- including the two untouched sibling branches (the standard
   increment path and the OFEX path), reproduced here as control flow so the new
   override branch's placement ahead of them is actually exercised, not just
   assumed. */
static uint32_t encode_counter_block(uint32_t rolling) {
#include "counter_override_and_multicntr_block.inc"
    return rolling;
}

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_secplus_v1_get_string() (sed 613,621p on the shipped
   secplus_v1.c). Verbatim. */
static void get_string_push_block(MiniSecPlusV1Instance* instance) {
#include "get_string_global_push_block.inc"
}

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
}

int main(void) {
    int checks = 0;

    /* --- Scenario A: override pending -- must short-circuit both the standard
       increment path and the OFEX path entirely, using the overridden value
       verbatim, then clear the one-shot flag. Real get_string() (Fragment 2)
       always sets cnt_length_bit before any later override is applied -- the one
       real precondition carried over from Phase 1's documented UB trap (see
       "Risiko"), satisfied here exactly as it is for every real caller. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 32;
        subghz_block_generic_global_counter_override_set(0x12345678);
        g_rolling_counter_mult = 2; // standard mult -- irrelevant, must not be read
        uint32_t result = encode_counter_block(/*rolling_before=*/0xAAAAAAAA);
        assert(result == 0x12345678);
        assert(subghz_block_generic_global.cnt_need_override == false); // one-shot clears
        checks += 2;

        // One-shot clear actually sticks: next call, no new override_set, the standard
        // mult path must run on whatever rolling value is now passed in.
        result = encode_counter_block(/*rolling_before=*/0x100);
        assert(result == 0x102); // 0x100 + mult(2), not re-overridden
        checks += 1;
    }

    /* --- Scenario B: override pending, g_rolling_counter_mult deliberately set to
       the OFEX sentinel (-0x7FFFFFFF) -- the override must still pre-empt the OFEX
       branch, proving the new branch runs strictly before the if/else-if chain,
       not merely before the standard-mode half of it. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 32;
        subghz_block_generic_global_counter_override_set(0xE7000001);
        g_rolling_counter_mult = -0x7FFFFFFF;
        uint32_t result = encode_counter_block(/*rolling_before=*/0xE6000000);
        assert(result == 0xE7000001); // override wins, OFEX increment-by-1 never runs
        checks += 1;
    }

    /* --- Scenario C: no override pending -- the pre-existing standard-mode
       increment formula must run completely unchanged (regression against the
       ALT/pre-patch behaviour), for both the "rolling == 0" special case and the
       plain-increment case. */
    {
        reset_global();
        g_rolling_counter_mult = 4;
        uint32_t result = encode_counter_block(/*rolling_before=*/0);
        // rolling == 0, mult(4) >= 0x6000000 is false -> rolling = 0xE6000000 (small-mult branch)
        assert(result == 0xE6000000);
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = 4;
        uint32_t result = encode_counter_block(/*rolling_before=*/0x1000);
        assert(result == 0x1004); // plain increment by mult
        checks += 1;
    }

    /* --- Scenario D: no override pending, OFEX sentinel active -- the pre-existing
       OFEX path must run completely unchanged, both for a plain below-threshold
       increment and for the above-threshold jump-to-0xFFFFFFFE case. */
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        uint32_t result = encode_counter_block(/*rolling_before=*/0x5);
        assert(result == 0x6); // plain OFEX increment, below the 0xE6000000 threshold
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        uint32_t result = encode_counter_block(/*rolling_before=*/0xE6000005);
        assert(result == 0xFFFFFFFE); // at/above threshold: jump, not a plain +1
        checks += 1;
    }

    printf("Scenarios A-D (counter override pre-empts both the standard and the OFEX "
           "branch; unchanged regression for both when no override is pending): "
           "%d checks OK\n",
           checks);

    /* --- Scenario E: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups
       (cnt_is_available always true, btn_is_available always false for this
       protocol -- secplus_v1's button value is derived from the fixed code, not
       independently editable, same as Unleashed's own upstream addition). */
    int before_e = checks;
    {
        reset_global();
        MiniSecPlusV1Instance inst = {.generic = {.cnt = 0xDEADBEEF, .btn = 1}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.cnt_length_bit == 32);
        assert(subghz_block_generic_global.current_cnt == 0xDEADBEEF);
        assert(subghz_block_generic_global.btn_is_available == false);
        assert(subghz_block_generic_global.current_btn == 1);
        assert(subghz_block_generic_global.btn_length_bit == 2);
        checks += 6;
    }
    {
        reset_global();
        MiniSecPlusV1Instance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == false);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario E (get_string global-push: cnt always available, btn never): %d checks OK\n",
        checks - before_e);

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
