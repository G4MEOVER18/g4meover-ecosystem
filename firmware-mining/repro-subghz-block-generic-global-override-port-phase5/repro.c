/* Host repro for Phase 5 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into Somfy Telis
 * (somfy_telis) -- the third protocol of the Nachzieh-Liste after Princeton
 * (Phase 3, button-only) and Security+ 1.0 (Phase 4, counter + button). Like
 * Secplus v1, Somfy Telis uses the same OFEX (overflow experimental) sentinel
 * pattern (furi_hal_subghz_get_rolling_counter_mult() == -0x7FFFFFFF) for its
 * rolling counter, but with its own, smaller 16-bit counter range and its own
 * standard-mode wrap/OFEX-jump formulas -- both independently exercised below.
 *
 * This does NOT compile the full, real somfy_telis.c (it needs the Manchester
 * encoder/decoder state machine and the full upload-buffer generator, neither
 * of which this change touches). Instead it `#include`s the exact statements
 * the patch adds plus the pre-existing statements they are spliced into,
 * extracted byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/somfy_telis.c
 * (see the sed line ranges below), and links them against the real, unmodified
 * Phase-1 generic.c -- the actual subghz_block_generic_global_*_override_get()
 * functions this patch calls. Everything surrounding those statements in the
 * real file (the Manchester framing, the checksum, the upload buffer) is
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
} MiniSomfyTelisInstance;

/* --- Fragment 1: the patched button-override call plus the patched
   counter-override leading branch, spliced ahead of the pre-existing
   standard-mode/OFEX if/else-if/else chain, from
   subghz_protocol_somfy_telis_gen_data() (sed 129,152p on the shipped
   somfy_telis.c). Verbatim -- including the two untouched sibling counter
   branches (standard mult-mode, OFEX mode), reproduced here as control flow
   so the new branches' placement ahead of them is actually exercised, not
   just assumed. `btn` is the function parameter the real code also overrides
   in place; `instance->generic.cnt` is the real field the real code also
   mutates in place. */
static uint8_t encode_button_and_counter_block(MiniSomfyTelisInstance* instance, uint8_t btn) {
#include "button_and_counter_override_block.inc"
    return btn;
}

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_somfy_telis_get_string() (sed 762,770p on the
   shipped somfy_telis.c). Verbatim. */
static void get_string_push_block(MiniSomfyTelisInstance* instance) {
#include "get_string_global_push_block.inc"
}

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
}

int main(void) {
    int checks = 0;

    /* --- Scenario A: button override pending -- consumed once, masked to the
       real btn_length_bit (4, set here exactly as the real get_string() does
       before any later override_get() can be reached -- the ordering contract
       from Phase 1's "Risiko"). A value outside the 4-bit range is deliberately
       used to also prove the masking formula for this protocol's real width. */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0x1F); // only low 4 bits must survive
        uint8_t btn = 0x3; // pre-override get_btn_code() value, must be replaced
        uint8_t result = encode_button_and_counter_block(
            &(MiniSomfyTelisInstance){.generic = {.cnt = 0x10}}, btn);
        assert(result == 0xF); // 0x1F & 0xF
        assert(subghz_block_generic_global.btn_need_override == false); // one-shot clears
        checks += 2;
    }

    /* --- Scenario B: no button override pending -- the pre-existing
       get_btn_code() value passed in must survive unchanged (regression). */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        uint8_t btn = 0x7;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0x10}};
        uint8_t result = encode_button_and_counter_block(&inst, btn);
        assert(result == 0x7);
        checks += 1;
    }

    /* --- Scenario C: counter override pending, standard mult-mode active
       (mult=2, irrelevant) -- override must pre-empt the entire chain and use
       the masked value verbatim, then clear the one-shot flag. A value above
       the real 16-bit range is used to also prove the masking formula for
       this protocol's real cnt_length_bit. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x1ABCD); // only low 16 bits survive
        g_rolling_counter_mult = 2; // standard mult -- irrelevant, must not be read
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0x5}};
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0xABCD); // 0x1ABCD & 0xFFFF
        assert(subghz_block_generic_global.cnt_need_override == false);
        checks += 2;

        // One-shot clear sticks: next call, no new override_set, standard mult
        // path must run on whatever cnt is now in place.
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0xABCF); // 0xABCD + mult(2), not re-overridden
        checks += 1;
    }

    /* --- Scenario D: counter override pending, OFEX sentinel active
       (mult=-0x7FFFFFFF) -- the override must still pre-empt the OFEX branch,
       proving the new branch runs strictly before the if/else-if chain, not
       merely before the standard-mode half of it. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x9);
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0xFFFF}}; // would wrap to 0 if OFEX ran
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0x9); // override wins, OFEX wrap never runs
        checks += 1;
    }

    /* --- Scenario E: no override pending -- the pre-existing standard-mode
       formula must run completely unchanged (regression), both the plain
       increment case and the 0xFFFF wraparound case. */
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0x10}};
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0x13); // plain increment by mult
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0xFFFE}}; // 0xFFFE + 3 > 0xFFFF
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0); // wraps to 0
        checks += 1;
    }

    /* --- Scenario F: no override pending, OFEX sentinel active -- the
       pre-existing OFEX path must run completely unchanged, covering all
       three of its real branches: overflow-wrap at 0xFFFF, jump-to-0xFFFE
       for any other in-range value, and the two plain-increment edge cases
       (0 and 0xFFFE) that the jump branch deliberately excludes. */
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0xFFFF}};
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0); // cnt+1 > 0xFFFF -> wrap to 0
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0x1234}}; // >=1, != 0xFFFE
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0xFFFE); // jump, not a plain +1
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0}};
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 1); // cnt == 0 excluded from the jump branch -> plain ++
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0xFFFE}};
        encode_button_and_counter_block(&inst, 0x1);
        assert(inst.generic.cnt == 0xFFFF); // cnt == 0xFFFE excluded -> plain ++
        checks += 1;
    }

    printf("Scenarios A-F (button/counter override pre-empt their respective chains, "
           "with correct masking; unchanged regression for standard mode and all three "
           "OFEX branches when no override is pending): %d checks OK\n",
           checks);

    /* --- Scenario G: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups
       (both cnt_is_available and btn_is_available always true for this
       protocol -- Somfy Telis's button value is independently editable, same
       as KeeLoq/Princeton, unlike Secplus v1's derived button in Phase 4). */
    int before_g = checks;
    {
        reset_global();
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0xBEEF, .btn = 0x2}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.cnt_length_bit == 16);
        assert(subghz_block_generic_global.current_cnt == 0xBEEF);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0x2);
        assert(subghz_block_generic_global.btn_length_bit == 4);
        checks += 6;
    }
    {
        reset_global();
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario G (get_string global-push: cnt and btn both always available): %d checks OK\n",
        checks - before_g);

    /* --- Scenario H: the length-bit-ordering contract from Phase 1 holds for
       this protocol's real bit widths -- get_string() (Fragment 2) sets both
       cnt_length_bit (16) and btn_length_bit (4) unconditionally, before any
       later override_get() call in gen_data() (Fragment 1) can be reached on
       any real call path (get_string() always runs first, when a signal is
       displayed, before a later send). Demonstrated here by running Fragment
       2 first and reusing its resulting lengths for a subsequent Fragment-1
       override round-trip -- no UB, correct masked result. */
    {
        reset_global();
        MiniSomfyTelisInstance inst = {.generic = {.cnt = 0x1, .btn = 0x1}};
        get_string_push_block(&inst); // sets cnt_length_bit=16, btn_length_bit=4
        subghz_block_generic_global_counter_override_set(0x1FFFF);
        subghz_block_generic_global_button_override_set(0xFF);
        uint8_t result_btn = encode_button_and_counter_block(&inst, 0x1);
        assert(result_btn == 0xF); // 0xFF & 0xF, using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFF); // 0x1FFFF & 0xFFFF
        checks += 2;
    }
    printf("Scenario H (length-bit ordering contract holds for this protocol's real "
           "16-bit counter / 4-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
