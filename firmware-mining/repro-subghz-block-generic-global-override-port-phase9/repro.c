/* Host repro for Phase 9 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into Somfy Keytis
 * (somfy_keytis) -- the seventh protocol of the Nachzieh-Liste after Princeton
 * (Phase 3, button-only), Security+ 1.0 (Phase 4, counter + button), Somfy Telis
 * (Phase 5, counter + button), Phoenix V2 (Phase 6, counter + button), CAME Atomo
 * (Phase 7, counter + button, bypass) and Alutech AT-4N (Phase 8, counter +
 * button, bypass).
 *
 * Of the five remaining candidates after Phase 8 (FAAC SLH: 712 diff lines,
 * Secplus v2: 1133, Nice FloR-S: 1284, KingGates Stylo4K: 496, Somfy Keytis:
 * 405 against DarkFlippers/unleashed-firmware), Somfy Keytis had the smallest
 * diff and was taken up here. Unlike Somfy Telis (Phase 5), which already
 * threads a `btn` function parameter through its own pre-existing
 * `get_btn_code()` custom-button resolution before building the frame, my own
 * somfy_keytis.c's `subghz_protocol_somfy_keytis_gen_data()` never uses its
 * `btn` parameter at all (`UNUSED(btn)`) -- `instance->generic.btn` is instead
 * re-derived every call from `instance->generic.data` via the same
 * `data ^ (data >> 8)` unpack that also yields `cnt`/`serial`, then read
 * straight into `frame[0]` a few lines later. So this protocol's button
 * override call is spliced to act directly on `instance->generic.btn`
 * in-place (right after that unpack, before the frame is built) rather than
 * on a local parameter -- no `get_btn_code()`/custom-button call site exists
 * here to attach to, unlike Telis/Phoenix V2/CAME Atomo/Alutech AT-4N. There
 * is also no multi-mode counter feature of my own to bypass here (unlike CAME
 * Atomo/Alutech AT-4N) -- the counter chain is the plain standard/OFEX
 * if/else-if/else pair, byte-for-byte identical to Somfy Telis's own (both
 * 16-bit `cnt`, same 0xFFFF wrap / 0xFFFE jump formulas), confirmed by diffing
 * the two files' chains directly before writing this fragment.
 *
 * This does NOT compile the full, real somfy_keytis.c (it needs the Manchester
 * decoder state machine and the full upload-buffer generator, neither of which
 * this change touches). Instead it `#include`s the exact statements the patch
 * adds plus the pre-existing statements they are spliced into, extracted
 * byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/somfy_keytis.c
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
} MiniSomfyKeytisInstance;

#define TAG "SubGhzProtocolSomfyKeytis"

/* --- Fragment 1: the patched button-override call plus the patched
   counter-override leading branch, spliced ahead of the pre-existing
   standard-mode/OFEX if/else-if/else chain, from
   subghz_protocol_somfy_keytis_gen_data() (sed 135,158p on the shipped
   somfy_keytis.c). Verbatim -- including the two untouched sibling counter
   branches (standard mult-mode, OFEX mode), reproduced here as control flow
   so the new branches' placement ahead of them is actually exercised, not
   just assumed. Unlike Somfy Telis's Fragment 1, the button override here
   acts on `instance->generic.btn` directly (there is no local `btn`
   parameter in play in the real code at this call site) -- `instance` is
   the real field the real code also mutates in place for both button and
   counter. */
static void encode_button_and_counter_block(MiniSomfyKeytisInstance* instance) {
#include "button_and_counter_override_block.inc"
}

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_somfy_keytis_get_string() (sed 805,813p on the
   shipped somfy_keytis.c). Verbatim. */
static void get_string_push_block(MiniSomfyKeytisInstance* instance) {
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
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x10, .btn = 0x3}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xF); // 0x1F & 0xF
        assert(subghz_block_generic_global.btn_need_override == false); // one-shot clears
        checks += 2;
    }

    /* --- Scenario B: no button override pending -- the pre-existing
       data-unpack-derived button value must survive unchanged (regression). */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x10, .btn = 0x7}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0x7);
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
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x5, .btn = 0x1}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xABCD); // 0x1ABCD & 0xFFFF
        assert(subghz_block_generic_global.cnt_need_override == false);
        checks += 2;

        // One-shot clear sticks: next call, no new override_set, standard mult
        // path must run on whatever cnt is now in place.
        encode_button_and_counter_block(&inst);
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
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0xFFFF, .btn = 0x1}}; // would wrap to 0 if OFEX ran
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x9); // override wins, OFEX wrap never runs
        checks += 1;
    }

    /* --- Scenario E: no override pending -- the pre-existing standard-mode
       formula must run completely unchanged (regression), both the plain
       increment case and the 0xFFFF wraparound case. */
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x10, .btn = 0x1}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x13); // plain increment by mult
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0xFFFE, .btn = 0x1}}; // 0xFFFE + 3 > 0xFFFF
        encode_button_and_counter_block(&inst);
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
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0xFFFF, .btn = 0x1}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0); // cnt+1 > 0xFFFF -> wrap to 0
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x1234, .btn = 0x1}}; // >=1, != 0xFFFE
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE); // jump, not a plain +1
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0, .btn = 0x1}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 1); // cnt == 0 excluded from the jump branch -> plain ++
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0xFFFE, .btn = 0x1}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFF); // cnt == 0xFFFE excluded -> plain ++
        checks += 1;
    }

    /* --- Scenario G: button override and counter override pending in the
       same call -- both must apply independently in the one pass (the real
       code runs the button block unconditionally before the counter chain,
       regardless of whether a counter override is also pending). */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_button_override_set(0xA);
        subghz_block_generic_global_counter_override_set(0x42);
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x5, .btn = 0x1}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xA);
        assert(inst.generic.cnt == 0x42);
        checks += 2;
    }

    printf("Scenarios A-G (button/counter override pre-empt their respective chains, "
           "with correct masking; unchanged regression for standard mode and all three "
           "OFEX branches when no override is pending; both overrides applying together "
           "in the same pass): %d checks OK\n",
           checks);

    /* --- Scenario H: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups
       (both cnt_is_available and btn_is_available always true for this
       protocol, same as KeeLoq/Princeton/Somfy Telis/Phoenix V2/CAME Atomo/
       Alutech AT-4N). */
    int before_h = checks;
    {
        reset_global();
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0xBEEF, .btn = 0x2}};
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
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario H (get_string global-push: cnt and btn both always available): %d checks OK\n",
        checks - before_h);

    /* --- Scenario I: the length-bit-ordering contract from Phase 1 holds for
       this protocol's real bit widths -- get_string() (Fragment 2) sets both
       cnt_length_bit (16) and btn_length_bit (4) unconditionally, before any
       later override_get() call in gen_data() (Fragment 1) can be reached on
       any real call path (get_string() always runs first, when a signal is
       displayed, before a later send). Demonstrated here by running Fragment
       2 first and reusing its resulting lengths for a subsequent Fragment-1
       override round-trip -- no UB, correct masked result. */
    {
        reset_global();
        MiniSomfyKeytisInstance inst = {.generic = {.cnt = 0x1, .btn = 0x1}};
        get_string_push_block(&inst); // sets cnt_length_bit=16, btn_length_bit=4
        subghz_block_generic_global_counter_override_set(0x1FFFF);
        subghz_block_generic_global_button_override_set(0xFF);
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xF); // 0xFF & 0xF, using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFF); // 0x1FFFF & 0xFFFF
        checks += 2;
    }
    printf("Scenario I (length-bit ordering contract holds for this protocol's real "
           "16-bit counter / 4-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
