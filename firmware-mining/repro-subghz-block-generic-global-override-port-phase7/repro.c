/* Host repro for Phase 7 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into CAME Atomo (came_atomo)
 * -- the fifth protocol of the Nachzieh-Liste after Princeton (Phase 3, button-only),
 * Security+ 1.0 (Phase 4, counter + button), Somfy Telis (Phase 5, counter + button)
 * and Phoenix V2 (Phase 6, counter + button).
 *
 * Unlike Princeton/Secplus v1/Somfy Telis/Phoenix V2, my own came_atomo.c already
 * has its own multi-mode counter feature: `came_atomo_counter_mode` selects between
 * four counter algorithms (0: standard/OFEX, 1: fixed 0000/0001/FFFE/FFFF table,
 * 2: fixed 0x807B/0x807C/0x007B/0x007C table, 3: freeze), read from the .sub file's
 * "CounterMode" field -- it is NOT an Unleashed-only addition this time (contrast
 * Phoenix V2's CounterMode in Phase 6, which my own phoenix_v2.c never had and was
 * deliberately left unported). Because modes 1-3 never look at the override at all,
 * Unleashed's own came_atomo.c pairs the override hookup with a `bypass` static that
 * forces mode 0's standard/OFEX branch to run for one pass whenever an override is
 * pending, regardless of which counter_mode is configured -- otherwise a counter or
 * button edit would be silently swallowed by modes 1-3's fixed tables. That `bypass`
 * mechanism is ported here verbatim (same two fields it gates on: cnt_need_override,
 * btn_need_override; same one-shot `bypass = false` reset once consumed).
 *
 * This does NOT compile the full, real came_atomo.c (it needs the Manchester
 * decoder state machine, the atomo_encrypt()/atomo_decrypt() byte cipher and the
 * full 8-frame upload-buffer generator, none of which this change touches). Instead
 * it #includes the exact statements the patch adds plus the pre-existing statements
 * they are spliced into, extracted byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/came_atomo.c
 * (see the sed line ranges below), and links them against the real, unmodified
 * Phase-1 generic.c -- the actual subghz_block_generic_global_*_override_get()
 * functions this patch calls. Everything surrounding those statements in the real
 * file (the Manchester framing, the byte cipher, the 8-frame upload buffer) is
 * pre-existing, untouched code and is not re-verified here.
 *
 * Unlike Phases 3/4/5/6, the button-override call site here is NOT adjacent to the
 * counter-override call site in the real file (the counter logic sits near the top
 * of subghz_protocol_encoder_came_atomo_get_upload(), the button mapping/override
 * near "//Send header" well below it) -- so this repro extracts them as two separate
 * fragments instead of one combined one.
 */
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;
int32_t g_rolling_counter_mult = 1;

typedef struct {
    SubGhzBlockGeneric generic;
} MiniCameAtomoInstance;

static uint8_t came_atomo_counter_mode = 0;
static bool bypass = false;

/* --- Fragment 1: the patched bypass-setup lines plus the full, pre-existing
   came_atomo_counter_mode if/else-if/else-if/else chain (modes 0-3), with the
   patched override_get() call spliced into mode 0's standard branch, from
   subghz_protocol_encoder_came_atomo_get_upload() (sed 197,249p on the shipped
   came_atomo.c). Verbatim -- including the three untouched sibling modes (1, 2,
   3/freeze), reproduced here as control flow so bypass's actual effect (forcing
   mode 0 to run instead of modes 1-3) is exercised, not just assumed.
   `instance->generic.cnt` is the real field the real code also mutates in
   place. */
static void counter_block(MiniCameAtomoInstance* instance) {
#include "counter_override_block.inc"
}

/* --- Fragment 2: the pre-existing button-code mapping chain plus the patched
   override call spliced directly after it, from the same function (sed 258,271p
   on the shipped came_atomo.c). Verbatim. `btn` is the function parameter the
   real code also overrides in place (the real code's prior, pre-existing
   `btn = subghz_protocol_came_atomo_get_btn_code();` assignment happens before
   this fragment and is simulated by the caller passing in its result). Note
   this protocol's override operates on the already wire-nibble-mapped value
   (0x0/0x2/0x4/0x6), not the friendly button id get_btn_code() first returns --
   exactly as Unleashed's own diff does it, see "Risiko" in the port writeup. */
static uint8_t button_block(uint8_t btn) {
#include "button_override_block.inc"
    return btn;
}

/* --- Fragment 3: the patched field-population block in
   subghz_protocol_decoder_came_atomo_get_string() (sed 847,855p on the shipped
   came_atomo.c). Verbatim. */
static void get_string_push_block(MiniCameAtomoInstance* instance) {
#include "get_string_global_push_block.inc"
}

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
    bypass = false;
    came_atomo_counter_mode = 0;
    g_rolling_counter_mult = 1;
}

int main(void) {
    int checks = 0;

    /* --- Scenario A: button override pending -- consumed once, masked to the real
       btn_length_bit (4, set here exactly as the real get_string() does before any
       later override_get() can be reached). A value outside the 4-bit range is
       deliberately used to also prove the masking formula for this protocol's real
       width. The override replaces the already wire-mapped btn value verbatim. */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0x1F); // only low 4 bits must survive
        uint8_t result = button_block(0x4); // pre-override mapped value, must be replaced
        assert(result == 0xF); // 0x1F & 0xF
        assert(subghz_block_generic_global.btn_need_override == false); // one-shot clears
        checks += 2;
    }

    /* --- Scenario B: no button override pending -- the pre-existing mapped button
       value must survive unchanged (regression), for all four real button codes. */
    {
        reset_global();
        assert(button_block(0x1) == 0x0);
        assert(button_block(0x2) == 0x2);
        assert(button_block(0x3) == 0x4);
        assert(button_block(0x4) == 0x6);
        checks += 4;
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
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x5}};
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
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0xFFFF}}; // would wrap to 0 if OFEX ran
        counter_block(&inst);
        assert(inst.generic.cnt == 0x9); // override wins, OFEX wrap never runs
        checks += 1;
    }

    /* --- Scenario E (the actual point of this protocol's `bypass` field): counter
       override pending while counter_mode is 1, 2 or 3 -- modes 1-3 have fixed
       tables that never look at the override on their own, so `bypass` must force
       the standard/OFEX branch (mode 0's body) to run instead for this one pass,
       applying the override. Without `bypass`, these three assertions would fail
       (the fixed-table bodies would run and ignore the override entirely). */
    {
        reset_global();
        came_atomo_counter_mode = 1;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x42);
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x10}}; // mode 1 table would ignore this
        counter_block(&inst);
        assert(inst.generic.cnt == 0x42); // override wins over mode 1's fixed table
        checks += 1;
    }
    {
        reset_global();
        came_atomo_counter_mode = 2;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x43);
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x807B}}; // mode 2 table would ignore this
        counter_block(&inst);
        assert(inst.generic.cnt == 0x43); // override wins over mode 2's fixed table
        checks += 1;
    }
    {
        reset_global();
        came_atomo_counter_mode = 3;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x44);
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x99}}; // mode 3 freezes cnt otherwise
        counter_block(&inst);
        assert(inst.generic.cnt == 0x44); // override wins over mode 3's freeze
        checks += 1;
    }

    /* --- Scenario F: no override pending, modes 1/2/3 run their own fixed tables
       completely unchanged (regression) -- bypass must stay false and must not be
       forced by a pending override that was never set. */
    {
        reset_global();
        came_atomo_counter_mode = 1;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x10}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE); // mode 1 table, untouched
        checks += 1;
    }
    {
        reset_global();
        came_atomo_counter_mode = 2;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x10}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0x807B); // mode 2 table, untouched
        checks += 1;
    }
    {
        reset_global();
        came_atomo_counter_mode = 3;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x99}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0x99); // mode 3 freeze, untouched
        checks += 1;
    }

    /* --- Scenario G: no override pending, counter_mode 0 -- the pre-existing
       standard-mode formula must run completely unchanged (regression), both the
       plain increment case and the 0xFFFF wraparound case. */
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x10}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0x13); // plain increment by mult
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = 3;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0xFFFE}}; // 0xFFFE + 3 > 0xFFFF
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
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0xFFFF}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0); // cnt+1 > 0xFFFF -> wrap to 0
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x1234}}; // >=1, != 0xFFFE
        counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE); // jump, not a plain +1
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0}};
        counter_block(&inst);
        assert(inst.generic.cnt == 1); // cnt == 0 excluded from the jump branch -> plain ++
        checks += 1;
    }
    {
        reset_global();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0xFFFE}};
        counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFF); // cnt == 0xFFFE excluded -> plain ++
        checks += 1;
    }

    printf("Scenarios A-H (button/counter override pre-empt their respective chains, "
           "with correct masking, the bypass mechanism actually forcing mode 0 over "
           "modes 1/2/3, and unchanged regression for modes 1/2/3 and all OFEX branches "
           "when no override is pending): %d checks OK\n",
           checks);

    /* --- Scenario I: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups
       (both cnt_is_available and btn_is_available always true for this protocol,
       same as KeeLoq/Princeton/Somfy Telis/Phoenix V2). */
    int before_i = checks;
    {
        reset_global();
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0xBEEF, .btn = 0x2}};
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
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0, .btn = 0}};
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
       cnt_length_bit (16) and btn_length_bit (4) unconditionally, before any later
       override_get() call in get_upload() (Fragments 1/2) can be reached on any real
       call path (get_string() always runs first, when a signal is displayed, before
       a later send). Demonstrated here by running Fragment 3 first and reusing its
       resulting lengths for subsequent Fragment-1/2 override round-trips -- no UB,
       correctly masked results. */
    {
        reset_global();
        MiniCameAtomoInstance inst = {.generic = {.cnt = 0x1, .btn = 0x1}};
        get_string_push_block(&inst); // sets cnt_length_bit=16, btn_length_bit=4
        subghz_block_generic_global_counter_override_set(0x1FFFF);
        subghz_block_generic_global_button_override_set(0xFF);
        counter_block(&inst); // real control flow: counter block runs before button block
        uint8_t result_btn = button_block(0x1);
        assert(result_btn == 0xF); // 0xFF & 0xF, using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFF); // 0x1FFFF & 0xFFFF
        checks += 2;
    }
    printf("Scenario J (length-bit ordering contract holds for this protocol's real "
           "16-bit counter / 4-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
