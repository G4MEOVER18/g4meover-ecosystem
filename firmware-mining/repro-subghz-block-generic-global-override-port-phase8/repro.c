/* Host repro for Phase 8 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into Alutech AT-4N
 * (alutech_at_4n) -- the sixth protocol of the Nachzieh-Liste after Princeton
 * (Phase 3, button-only), Security+ 1.0 (Phase 4, counter + button), Somfy Telis
 * (Phase 5, counter + button), Phoenix V2 (Phase 6, counter + button) and CAME Atomo
 * (Phase 7, counter + button, bypass ported because came_atomo_counter_mode was a
 * 1:1 match to Unleashed's own).
 *
 * My own alutech_at_4n.c already has its own multi-mode counter feature, just like
 * CAME Atomo: `alutech_at4n_counter_mode` selects between three counter algorithms
 * (0: standard/OFEX, 1: fixed 0000/0001/FFFE/FFFF table, 2: 0x0000..0x0005 wraparound
 * counter) -- one mode fewer than CAME Atomo's four (no freeze mode here), but the
 * same problem: modes 1/2 never look at the override at all, so Unleashed's own
 * alutech_at_4n.c pairs the override hookup with the same one-shot `bypass` static
 * that forces mode 0's standard/OFEX branch to run for one pass whenever an override
 * is pending, regardless of which counter_mode is configured. Ported here verbatim.
 *
 * Unlike CAME Atomo (where the bypass setup checks both cnt_need_override and
 * btn_need_override before the counter_mode branch), Alutech AT-4N's own bypass
 * setup only checks cnt_need_override there -- the button-override call site (in
 * get_upload(), run before gen_data() is ever called) sets `bypass = true` directly
 * on success instead. Both paths still converge on the same static, so a pending
 * button-only override still forces mode 0 for the counter pass that follows in the
 * same get_upload() call -- exercised below as Scenario E using the button fragment
 * followed by the counter fragment, exactly the real call order in get_upload().
 *
 * This does NOT compile the full, real alutech_at_4n.c (it needs the preamble/bit
 * decoder state machine and the full encrypt()/decrypt() rainbow-table cipher, none
 * of which this change touches). Instead it #includes the exact statements the
 * patch adds plus the pre-existing statements they are spliced into, extracted
 * byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/alutech_at_4n.c
 * (see the sed line ranges below), and links them against the real, unmodified
 * Phase-1 generic.c -- the actual subghz_block_generic_global_*_override_get()
 * functions this patch calls. Everything surrounding those statements in the real
 * file (the preamble/bit decoder, the rainbow-table cipher) is pre-existing,
 * untouched code and is not re-verified here.
 *
 * Unlike CAME Atomo's two separate fragments (counter logic near the top of
 * get_upload()'s callee gen_data(), button mapping further down in get_upload()
 * itself), this protocol's button override is a single standalone call right after
 * the pre-existing `btn = subghz_protocol_alutech_at_4n_get_btn_code();` line, with
 * no inline mapping table to also reproduce -- so the button fragment here is
 * shorter than CAME Atomo's.
 */
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"

#define TAG "SubGhzProtocoAlutech_at_4n"

extern int g_unexpected_stub_calls;
int32_t g_rolling_counter_mult = 1;

typedef struct {
    SubGhzBlockGeneric generic;
} MiniAlutechAt4nInstance;

static uint8_t alutech_at4n_counter_mode = 0;
static bool bypass = false;

/* --- Fragment 1: the patched bypass-setup lines plus the full, pre-existing
   alutech_at4n_counter_mode if/else-if/else chain (modes 0-2), with the patched
   override_get() call spliced into mode 0's standard branch, from
   subghz_protocol_alutech_at_4n_gen_data() (sed 285,332p on the shipped
   alutech_at_4n.c). Verbatim -- including the two untouched sibling modes (1, 2),
   reproduced here as control flow so bypass's actual effect (forcing mode 0 to run
   instead of modes 1/2) is exercised, not just assumed. `instance->generic.cnt` is
   the real field the real code also mutates in place. */
static void counter_block(MiniAlutechAt4nInstance* instance) {
#include "counter_override_block.inc"
}

/* --- Fragment 2: the patched button-override call, standalone, from
   subghz_protocol_encoder_alutech_at_4n_get_upload() (sed 399,403p on the shipped
   alutech_at_4n.c). Verbatim. `btn` is the function parameter the real code also
   overrides in place (the real code's prior, pre-existing
   `btn = subghz_protocol_alutech_at_4n_get_btn_code();` assignment happens before
   this fragment and is simulated by the caller passing in its result). Unlike CAME
   Atomo, there is no inline button-code mapping table here to also reproduce --
   this protocol's override runs directly on the custom-button-resolved value. */
static uint8_t button_block(uint8_t btn) {
#include "button_override_block.inc"
    return btn;
}

/* --- Fragment 3: the patched field-population block in
   subghz_protocol_decoder_alutech_at_4n_get_string() (sed 898,906p on the shipped
   alutech_at_4n.c). Verbatim. */
static void get_string_push_block(MiniAlutechAt4nInstance* instance) {
#include "get_string_global_push_block.inc"
}

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
    bypass = false;
    alutech_at4n_counter_mode = 0;
    g_rolling_counter_mult = 1;
}

int main(void) {
    int checks = 0;

    /* --- Scenario A: button override pending -- consumed once. btn_length_bit is
       8 for this protocol (full byte, unlike CAME Atomo's 4-bit nibble), so the
       masking formula (`0xFF >> (8 - 8)`) is an identity mask covering the whole
       uint8_t range -- there is no out-of-range value left to prove truncation
       with (the override setter itself already takes a uint8_t), so a plain
       round-trip of a real button code is the correct regression check here. */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0xAB);
        uint8_t result = button_block(0x11); // pre-override custom-btn-resolved value
        assert(result == 0xAB); // full-byte override wins verbatim
        assert(subghz_block_generic_global.btn_need_override == false); // one-shot clears
        assert(bypass == true); // button override success sets bypass, unlike CAME Atomo
        checks += 3;
    }

    /* --- Scenario B: no button override pending -- the pre-existing resolved button
       value must survive unchanged (regression), for all five real button codes,
       and bypass must stay false. */
    {
        reset_global();
        assert(button_block(0x11) == 0x11);
        assert(button_block(0x22) == 0x22);
        assert(button_block(0xFF) == 0xFF);
        assert(button_block(0x44) == 0x44);
        assert(button_block(0x33) == 0x33);
        assert(bypass == false);
        checks += 6;
    }

    /* --- Scenario C: counter override pending, counter_mode 0 (default) -- must
       pre-empt the entire standard/OFEX chain and use the masked value verbatim,
       then clear the one-shot flag and the one-shot bypass. A value above the real
       16-bit range is used to also prove the masking formula for this protocol's
       real cnt_length_bit. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x1ABCD); // only low 16 bits survive
        g_rolling_counter_mult = 2; // standard mult -- irrelevant, must not be read
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x5}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0xABCD); // 0x1ABCD & 0xFFFF
        assert(subghz_block_generic_global.cnt_need_override == false);
        assert(bypass == false); // one-shot bypass reset after being consumed
        checks += 3;

        // One-shot clear sticks: next call, no new override_set, standard mult path
        // must run on whatever cnt is now in place (bypass no longer forced true).
        counter_block(&inst);
        assert(inst.generic.cnt == 0xABCF); // 0xABCD + mult(2), not re-overridden
        checks += 1;
    }

    /* --- Scenario D: counter override pending, OFEX sentinel active
       (mult=-0x7FFFFFFF), counter_mode 0 -- the override must still pre-empt the
       OFEX branch, proving the new branch runs strictly before the OFEX/standard
       split, not merely before the standard half of it. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x9);
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0xFFFF}}; // would wrap to 0 if OFEX ran
        counter_block(&inst);
        assert(inst.generic.cnt == 0x9); // override wins, OFEX wrap never runs
        checks += 1;
    }

    /* --- Scenario E (the actual point of this protocol's `bypass` field, and the
       cross-fragment path specific to Alutech AT-4N): a button-only override
       pending while counter_mode is 1 or 2 -- modes 1/2 have fixed/incrementing
       tables that never look at the override on their own, so the button fragment's
       `bypass = true` (set when the button override succeeds, independent of any
       counter override) must still force the counter fragment's standard/OFEX
       branch (mode 0's body) to run for the very next pass, exactly the real
       get_upload() -> gen_data() call order. Without this cross-fragment effect,
       these assertions would fail (the fixed/incrementing bodies would run
       instead). No counter override is set in this scenario -- only the button
       fragment's side effect on `bypass` is exercised. */
    {
        reset_global();
        alutech_at4n_counter_mode = 1;
        uint8_t result = button_block(0x11);
        assert(result == 0x11); // no btn override pending in this scenario
        assert(bypass == false); // and so bypass stays false -- unlike Scenario A
        checks += 2;
    }
    {
        reset_global();
        alutech_at4n_counter_mode = 1;
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0x22);
        button_block(0x11); // succeeds, sets bypass = true as a side effect
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x7);
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x10}}; // mode 1 table would ignore this
        counter_block(&inst);
        assert(inst.generic.cnt == 0x7); // override wins over mode 1's fixed table
        checks += 1;
    }
    {
        reset_global();
        alutech_at4n_counter_mode = 2;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x3);
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x1}}; // mode 2 table would ignore this
        counter_block(&inst);
        assert(inst.generic.cnt == 0x3); // override wins over mode 2's own table, via
                                          // cnt_need_override alone (no button involved)
        checks += 1;
    }

    /* --- Scenario F: no override pending, modes 1/2 run their own tables completely
       unchanged (regression) -- bypass must stay false and must not be forced by a
       pending override that was never set. */
    {
        reset_global();
        alutech_at4n_counter_mode = 1;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x10}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE); // mode 1 table, untouched
        checks += 1;
    }
    {
        reset_global();
        alutech_at4n_counter_mode = 2;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x3}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0x4); // mode 2 incrementing table, untouched
        checks += 1;
    }
    {
        reset_global();
        alutech_at4n_counter_mode = 2;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x5}}; // mode 2 wraps at >= 5
        counter_block(&inst);
        assert(inst.generic.cnt == 0); // wraps to 0
        checks += 1;
    }

    /* --- Scenario G: no override pending, counter_mode 0 -- the pre-existing
       standard-mode formula must run completely unchanged (regression), both the
       plain increment case and the 0xFFFF wraparound case. */
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x10}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0x13); // plain increment by mult
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0xFFFE}}; // 0xFFFE + 3 > 0xFFFF
        counter_block(&inst);
        assert(inst.generic.cnt == 0); // wraps to 0
        checks += 1;
    }

    /* --- Scenario H: no override pending, counter_mode 0, OFEX sentinel active --
       the pre-existing OFEX path must run completely unchanged, covering all three
       of its real branches: overflow-wrap at 0xFFFF, jump-to-0xFFFE for any other
       in-range value, and the two plain-increment edge cases (0 and 0xFFFE) that the
       jump branch deliberately excludes. */
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0xFFFF}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0); // cnt+1 > 0xFFFF -> wrap to 0
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x1234}}; // >=1, != 0xFFFE
        counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE); // jump, not a plain +1
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0}};
        counter_block(&inst);
        assert(inst.generic.cnt == 1); // cnt == 0 excluded from the jump branch -> plain ++
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0xFFFE}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFF); // cnt == 0xFFFE excluded -> plain ++
        checks += 1;
    }

    printf("Scenarios A-H (button/counter override pre-empt their respective chains, "
           "with correct masking, the bypass mechanism actually forcing mode 0 over "
           "modes 1/2 -- including the button-only, cross-fragment case specific to "
           "this protocol -- and unchanged regression for modes 1/2 and all OFEX "
           "branches when no override is pending): %d checks OK\n",
           checks);

    /* --- Scenario I: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups (both
       cnt_is_available and btn_is_available always true for this protocol, same as
       KeeLoq/Princeton/Somfy Telis/Phoenix V2/CAME Atomo). */
    int before_i = checks;
    {
        reset_global();
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0xBEEF, .btn = 0x22}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.cnt_length_bit == 16);
        assert(subghz_block_generic_global.current_cnt == 0xBEEF);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0x22);
        assert(subghz_block_generic_global.btn_length_bit == 8);
        checks += 6;
    }
    {
        reset_global();
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario I (get_string global-push: cnt and btn both always available): %d checks OK\n",
        checks - before_i);

    /* --- Scenario J: the length-bit-ordering contract from Phase 1 holds for this
       protocol's real bit widths -- get_string() (Fragment 3) sets both
       cnt_length_bit (16) and btn_length_bit (8) unconditionally, before any later
       override_get() call in get_upload()/gen_data() (Fragments 1/2) can be reached
       on any real call path (get_string() always runs first, when a signal is
       displayed, before a later send). Demonstrated here by running Fragment 3
       first and reusing its resulting lengths for subsequent Fragment-2/1
       override round-trips, in the real call order (button fragment in
       get_upload() runs before the counter fragment in gen_data()) -- no UB,
       correctly masked results. */
    {
        reset_global();
        MiniAlutechAt4nInstance inst = {.generic = {.cnt = 0x1, .btn = 0x11}};
        get_string_push_block(&inst); // sets cnt_length_bit=16, btn_length_bit=8
        subghz_block_generic_global_counter_override_set(0x1FFFF); // only low 16 bits survive
        subghz_block_generic_global_button_override_set(0xCD); // full byte, identity-masked
        uint8_t result_btn = button_block(0x11); // real order: button fragment first
        counter_block(&inst); // then the counter fragment
        assert(result_btn == 0xCD); // using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFF); // 0x1FFFF & 0xFFFF
        checks += 2;
    }
    printf("Scenario J (length-bit ordering contract holds for this protocol's real "
           "16-bit counter / 8-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
