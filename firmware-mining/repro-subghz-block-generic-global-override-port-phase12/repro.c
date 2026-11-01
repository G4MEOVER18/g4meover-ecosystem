/* Host repro for Phase 12 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into Security+ 2.0 (secplus_v2)
 * -- the tenth protocol of the Nachzieh-Liste after Princeton (Phase 3, button-only),
 * Security+ 1.0 (Phase 4, counter + button), Somfy Telis (Phase 5, counter + button),
 * Phoenix V2 (Phase 6, counter + button), CAME Atomo (Phase 7, counter + button,
 * bypass), Alutech AT-4N (Phase 8, counter + button, bypass), Somfy Keytis (Phase 9,
 * counter + button), KingGates Stylo4K (Phase 10, counter + button, fix/hop
 * reconstruction) and FAAC SLH (Phase 11, counter + button, pre-gated early returns).
 *
 * Of the two remaining candidates after Phase 11 (Secplus v2: 1133 diff lines, Nice
 * FloR-S: 1284 against DarkFlippers/unleashed-firmware, dev branch), Secplus v2 had
 * the smaller diff and was taken up here. My own `secplus_v2.c` is, like
 * `secplus_v1.c` in Phase 4, structurally the "old" pre-refactor OFEX logic --
 * Unleashed's current `dev` branch has since (a) replaced the `multicntr`/"reset to
 * 0, then resync to 0xE500000/0xFFFFFFF on the next call" special-casing with a
 * flatter, simplified standard-mode branch, and (b) added an entirely separate "86
 * bit (keypad)" message variant (`subghz_protocol_secplus_v2_encode_86()`,
 * `SECPLUS_V2_86_COUNT_BIT`, PIN extraction/application) that does not exist in my
 * own file at all and is a genuinely different protocol-layer feature, not an
 * override-mechanism concern. Both are deliberately NOT ported here, exactly like
 * Unleashed's separate `common.c` encoder/decoder-base refactor was excluded in
 * Phases 3/4/7/8/9/10/11 -- a blind file replace would have deleted my own,
 * functioning `multicntr` logic and silently added an unrelated 1000+-line keypad
 * feature. Instead, the same two anschlussstellen as every protocol since Phase 3:
 * one button-override call in the encoder, one counter-override leading branch
 * prepended to the existing standard-mode/OFEX if/else-if/else chain (same shape as
 * Secplus v1 in Phase 4), plus the matching `get_string()` global-push block. Unlike
 * every protocol since Phase 7 (CAME Atomo), there is no `bypass`-style flag to
 * carry over: my own `multicntr`-based standard mode has no separate counter-mode
 * selector Unleashed's dev branch doesn't also have, so the override simply pre-empts
 * the whole existing chain for one pass, exactly like KeeLoq (Phase 2) and Secplus v1
 * (Phase 4).
 *
 * This does NOT compile the full, real secplus_v2.c (it needs the Manchester
 * framing/mix-invert/mix-order helpers and the full upload-buffer generator, neither
 * of which this change touches). Instead it `#include`s the exact statements the
 * patch adds plus the pre-existing statements they are spliced into, extracted
 * byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/secplus_v2.c
 * (see the sed line ranges below), and links them against the real, unmodified
 * Phase-1 generic.c -- the actual subghz_block_generic_global_*_override_get()
 * functions this patch calls. Everything surrounding those statements in the real
 * file (the pre-existing `instance->generic.btn = ..._get_btn_code();` assignment,
 * the `fixed_1`/`fixed_2`/`rolling_digits`/`roll_1`/`roll_2` frame-encoding locals
 * that only the untouched Manchester/mix-order code downstream consumes, the
 * Manchester framing itself) is pre-existing, untouched code and is not re-verified
 * here -- reproducing it would need the full mix-invert/mix-order/encode_half helper
 * chain for no additional coverage of what this patch actually changes.
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
} MiniSecplusV2Instance;

#define TAG "SubGhzProtocoSecPlusV2"

/* --- Fragment 1: the patched button-override call plus the patched
   counter-override leading branch (with the pre-existing `multicntr`
   computation it needs), spliced ahead of the pre-existing standard-mode/OFEX
   if/else-if/else chain, from subghz_protocol_secplus_v2_encode() (sed
   400,403p for the button-override call, sed 411,458p for the `multicntr`
   computation plus the counter-override leading branch plus the two
   untouched sibling branches -- on the shipped secplus_v2.c; the 5 lines in
   between, 405-409, are the pre-existing `fixed_1`/`fixed_2`/
   `rolling_digits`/`roll_1`/`roll_2` frame-encoding locals this patch does
   not touch and are deliberately not reproduced here, since nothing
   downstream of them in the real function is exercised by this repro --
   this fragment only proves both override call sites fire and mutate
   `instance->generic.btn`/`.cnt` correctly in place, not the Manchester
   framing that consumes them afterwards). Verbatim. */
static void encode_button_and_counter_block(MiniSecplusV2Instance* instance) {
#include "button_and_counter_override_block.inc"
}

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_secplus_v2_get_string() (sed 996,1002p on the
   shipped secplus_v2.c). Verbatim. */
static void get_string_push_block(MiniSecplusV2Instance* instance) {
#include "get_string_global_push_block.inc"
}

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
}

int main(void) {
    int checks = 0;

    /* --- Scenario A (button override pending): consumed once. This
       protocol's real btn_length_bit is 8 -- the full width of the uint8_t
       `btn` field itself -- so the mask `0xFF >> (8-8)` is the identity and
       there is no narrower width to truncate against (unlike FAAC SLH's
       4-bit button in Phase 11, where the mask is observable); this
       scenario shows the override value passing straight through, and
       Scenario C below demonstrates the masking formula on `cnt`
       (28 of 32 bits), where it is observable. */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0xCD);
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x68, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xCD);
        checks++;
    }

    /* --- Scenario B (regression, no button override): btn stays whatever
       the pre-existing, unreproduced get_btn_code() assignment set it to. */
    {
        reset_global();
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0x80);
        checks++;
    }

    /* --- Scenario C (counter override pending, standard mode would
       otherwise apply): override value used verbatim, masked to cnt_length_bit
       (28) -- the entire multicntr/OFEX chain below is skipped, even though
       g_rolling_counter_mult is set to an ordinary standard-mode value. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 28;
        subghz_block_generic_global_counter_override_set(0xFFFFFFFF);
        g_rolling_counter_mult = 5;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFFFFF); // 0xFFFFFFFF & 0xFFFFFFF (28 bits)
        checks++;
    }

    /* --- Scenario D (counter override pending, OFEX sentinel active): the
       override still pre-empts the whole chain, including the OFEX branch. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 28;
        subghz_block_generic_global_counter_override_set(0x123456);
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0xFFFFFFE}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x123456);
        checks++;
    }

    /* --- Scenario E (regression, no override, standard mode, cnt!=0, no
       overflow): plain increment by multicntr (== mult here, positive). */
    {
        reset_global();
        g_rolling_counter_mult = 7;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x1007);
        checks++;
    }

    /* --- Scenario F (regression, no override, standard mode, cnt!=0,
       overflow past 0xFFFFFFF): resets to the resync sentinel 0xE500000. */
    {
        reset_global();
        g_rolling_counter_mult = 0x10;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0xFFFFFF8}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xE500000);
        checks++;
    }

    /* --- Scenario G (regression, no override, standard mode, the
       mult==0xFFFFFFF && cnt!=0 quirk): resets cnt to 0 outright, the
       dedicated "reset counter before applying new cnt value" step. */
    {
        reset_global();
        g_rolling_counter_mult = (int32_t)0xFFFFFFF;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0);
        checks++;
    }

    /* --- Scenario H (regression, no override, standard mode, cnt==0,
       three sub-cases of the resync dance): */
    {
        /* H1: mult below 0xE500000 -> jump straight to 0xE500000. */
        reset_global();
        g_rolling_counter_mult = 0x100;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xE500000);
        checks++;
    }
    {
        /* H2: mult >= 0xFFFFFFF -> jump to the top sentinel 0xFFFFFFF. */
        reset_global();
        g_rolling_counter_mult = (int32_t)0xFFFFFFF;
        /* mult==0xFFFFFFF is exactly scenario G's trigger (cnt!=0 branch),
           so use a cnt==0 instance here to reach the sibling cnt==0 arm
           instead -- same mult value, different generic.cnt precondition. */
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFFFFF);
        checks++;
    }
    {
        /* H3: mult between 0xE500000 and 0xFFFFFFF, cnt==0 -> cnt +=
           multicntr (multicntr == mult itself here, positive and already
           below the 28-bit mask) -- same "standard increment" arithmetic as
           E/F, just reached through the cnt==0 arm instead of the cnt!=0
           arm. */
        reset_global();
        g_rolling_counter_mult = 0xE500005;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xE500005);
        checks++;
    }

    /* --- Scenario I (regression, no override, OFEX mode, three sub-cases): */
    {
        /* I1: plain increment. */
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x1001);
        checks++;
    }
    {
        /* I2: overflow past 0xFFFFFFF -> resync sentinel 0xE500000. */
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0xFFFFFFF}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xE500000);
        checks++;
    }
    {
        /* I3: already in the resync window (>=0xE500000, !=0xFFFFFFE) ->
           jump straight to the top-of-window sentinel 0xFFFFFFE. */
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0xE500010}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFFFFE);
        checks++;
    }

    /* --- Scenario J (both overrides together in the same pass): the real
       code runs the button block unconditionally before the counter block,
       independent of whether a counter override is also pending. */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global.cnt_length_bit = 28;
        subghz_block_generic_global_button_override_set(0xAB);
        subghz_block_generic_global_counter_override_set(0x42);
        g_rolling_counter_mult = 9;
        MiniSecplusV2Instance inst = {.generic = {.btn = 0x80, .cnt = 0x1000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xAB);
        assert(inst.generic.cnt == 0x42);
        checks += 2;
    }

    printf("Scenarios A-J (button/counter override pre-empt their respective chains, "
           "with correct masking; unchanged regression for standard mode's "
           "mult==0xFFFFFFF&&cnt!=0 quirk, all three cnt==0 resync sub-cases and plain "
           "cnt!=0 increment/overflow, all three OFEX sub-branches; both overrides "
           "applying together in the same pass): %d checks OK\n",
           checks);

    /* --- Scenario K: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups
       (both cnt_is_available and btn_is_available always true for this
       protocol, same as every protocol ported so far except Secplus v1,
       where btn_is_available is false). cnt_length_bit is 28, btn_length_bit
       is 8 -- this protocol's own real widths. */
    int before_k = checks;
    {
        reset_global();
        MiniSecplusV2Instance inst = {.generic = {.cnt = 0xBEEEF, .btn = 0x81}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.cnt_length_bit == 28);
        assert(subghz_block_generic_global.current_cnt == 0xBEEEF);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0x81);
        assert(subghz_block_generic_global.btn_length_bit == 8);
        checks += 6;
    }
    {
        reset_global();
        MiniSecplusV2Instance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario K (get_string global-push: cnt and btn both always available, "
        "28-bit counter / 8-bit button widths): %d checks OK\n",
        checks - before_k);

    /* --- Scenario L: the length-bit-ordering contract from Phase 1 holds for
       this protocol's real bit widths -- get_string() (Fragment 2) sets both
       cnt_length_bit (28) and btn_length_bit (8) unconditionally, before any
       later override_get() call in encode() (Fragment 1) can be reached on
       any real call path (get_string() always runs first, when a signal is
       displayed, before a later send). Demonstrated here by running Fragment
       2 first and reusing its resulting lengths for a subsequent Fragment-1
       override round-trip -- no UB, correct masked result (the counter mask
       is observable at 28 of 32 bits; the button mask, as in Scenario A, is
       the identity at the full 8-bit width). */
    {
        reset_global();
        MiniSecplusV2Instance inst = {.generic = {.cnt = 0x1, .btn = 0x1}};
        get_string_push_block(&inst); // sets cnt_length_bit=28, btn_length_bit=8
        subghz_block_generic_global_counter_override_set(0xFFFFFFFF);
        subghz_block_generic_global_button_override_set(0xCD);
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xCD); // full 8-bit width, using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFFFFF); // 0xFFFFFFFF & 0xFFFFFFF
        checks += 2;
    }
    printf("Scenario L (length-bit ordering contract holds for this protocol's real "
           "28-bit counter / 8-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
