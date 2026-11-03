/* Host repro for Phase 13 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into Nice FloR-S
 * (nice_flor_s) -- the eleventh and last protocol of the Nachzieh-Liste after
 * Princeton (Phase 3, button-only), Security+ 1.0 (Phase 4, counter + button),
 * Somfy Telis (Phase 5, counter + button), Phoenix V2 (Phase 6, counter +
 * button), CAME Atomo (Phase 7, counter + button, bypass), Alutech AT-4N
 * (Phase 8, counter + button, bypass), Somfy Keytis (Phase 9, counter +
 * button), KingGates Stylo4K (Phase 10, counter + button, fix/hop
 * reconstruction), FAAC SLH (Phase 11, counter + button, pre-gated early
 * returns) and Security+ 2.0 (Phase 12, counter + button, no bypass needed).
 *
 * Nice FloR-S is, like CAME Atomo/Alutech AT-4N (Phase 7/8), a protocol with
 * its own, pre-existing "CounterMode" selector (`nice_flors_counter_mode`:
 * 0 = standard/OFEX, 1 = "floxi2r" toggle, 2 = "ox2" toggle) that the plain
 * override calls alone cannot reach once mode 1 or 2 is active -- so this
 * port needs the same `bypass` escape-hatch flag as those two. Unlike CAME
 * Atomo/Alutech AT-4N, the escape is wired through an existing global flag I
 * already deliver as part of Phase 1: `subghz_block_generic_global_counter_
 * override_get()` itself only ever returns true when `subghz_block_generic_
 * global.cnt_need_override` was set (by `..._counter_override_set()`, called
 * from the signal-settings counter editor), and the connection point I add,
 * `if(subghz_block_generic_global.cnt_need_override) bypass = true;`, reuses
 * that exact same flag as the bypass trigger -- so a pending counter override
 * always forces `bypass = true` before the mode check, with no separate
 * tracking needed. Two side effects fall out of sharing one `bypass` flag
 * between the button- and counter-override call sites, both verified below:
 * (1) a pending counter override escapes modes 1/2 exactly like a CAME
 * Atomo/Alutech AT-4N multi-counter-mode override would (Scenario D); but
 * also (2) a button-only override (no counter override pending at all) sets
 * the very same `bypass = true`, which -- for this one encode pass -- forces
 * the counter logic through the mode-0 standard/OFEX branch's "no override
 * found, fall back to +mult" path even if `nice_flors_counter_mode` is
 * currently 1 or 2, silently pre-empting the active custom counter mode's
 * toggle logic for that single send (Scenario E). Both are real,
 * already-shipped behavior of the mechanism this phase connects, not
 * something this patch introduces -- Alutech AT-4N/CAME Atomo have the
 * identical interaction for the same reason (one shared `bypass` bool).
 *
 * This does NOT compile the full, real nice_flor_s.c (it needs the
 * rainbow-table decrypt/encrypt helpers and the full 16-parcel upload-buffer
 * generator, neither of which this change touches). Instead it `#include`s
 * the exact statements the patch adds plus the pre-existing statements they
 * are spliced into, extracted byte-for-byte via `sed` from the shipped,
 * patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/nice_flor_s.c
 * (see the sed line ranges below), and links them against the real,
 * unmodified Phase-1 generic.c -- the actual
 * subghz_block_generic_global_*_override_get() functions this patch calls.
 * Everything surrounding those statements in the real file (the pre-existing
 * `btn = instance->generic.btn;`/custom-button-original-save prologue, the
 * `size_upload` buffer-size check that sits textually between the two
 * override connection points but has nothing to do with either of them, the
 * rainbow-table encrypt call and the 16-parcel Manchester upload loop that
 * consume `btn`/`instance->generic.cnt` afterwards) is pre-existing, untouched
 * code and is not re-verified here -- reproducing it would need the full
 * rainbow-table/Manchester-encoding chain for no additional coverage of what
 * this patch actually changes.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;
int32_t g_rolling_counter_mult = 1;

/* Test-controlled stand-in for the real subghz_protocol_nice_flor_s_get_btn_code()
   (custom-button-cycle lookup) -- out of scope for this port, see "Phase 13:
   Nice FloR-S" in the port doc; the real function's return value is simply
   the pre-existing input the button-override call is spliced after. */
static uint8_t g_btn_code = 0;
static uint8_t subghz_protocol_nice_flor_s_get_btn_code(void) {
    return g_btn_code;
}

#define TAG "SubGhzProtocolNiceFlorS"

/* File-scope statics in the real nice_flor_s.c, exactly as Unleashed/Momentum
   declare them (see "Phase 13: Nice FloR-S"); reset explicitly between
   scenarios below since nothing else in this repro does it for us. */
static bool bypass = false;
static uint8_t nice_flors_counter_mode = 0;

typedef struct {
    SubGhzBlockGeneric generic;
} MiniNiceFlorSInstance;

/* --- Fragment 1: the patched button-override call plus the patched
   cnt_need_override-to-bypass trigger and the bypass-gated counter-mode
   chain (with the two untouched sibling mode-1/mode-2 branches), from
   subghz_protocol_encoder_nice_flor_s_get_upload() (sed 154,160p on the
   shipped nice_flor_s.c for the button-override call plus the pre-existing
   `btn = ..._get_btn_code();` assignment it is spliced after; sed 168,210p
   for the cnt_need_override-to-bypass line plus the full bypass-gated
   if/else-if/else chain -- the 7 lines in between, 161-167, are the
   pre-existing `size_upload` buffer-size check this patch does not touch and
   are deliberately not reproduced here, since nothing downstream of them in
   the real function is exercised by this repro -- this fragment only proves
   both override call sites fire and mutate the local `btn`/
   `instance->generic.cnt` correctly in place, not the rainbow-table
   encrypt/Manchester upload loop that consumes them afterwards). Verbatim. */
static uint8_t encode_button_and_counter_block(MiniNiceFlorSInstance* instance) {
    uint8_t btn = 0;
#include "button_and_counter_override_block.inc"
    return btn;
}

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_nice_flor_s_get_string() (sed 935,943p on the
   shipped nice_flor_s.c). Verbatim. */
static void get_string_push_block(MiniNiceFlorSInstance* instance) {
#include "get_string_global_push_block.inc"
}

static void reset_all(void) {
    subghz_block_generic_global_reset(NULL);
    bypass = false;
    nice_flors_counter_mode = 0;
    g_btn_code = 0;
}

int main(void) {
    int checks = 0;

    /* --- Scenario A (button override pending, mode 0, no counter override):
       the override value is returned (masked to btn_length_bit, 4 of 8 bits
       -- 0xBD & 0xF == 0xD -- observable here, unlike Secplus v2's Phase 12
       Scenario A where the full 8-bit width made the mask the identity); the
       counter still runs its normal standard-mode increment afterwards,
       since no counter override is pending and mode is already 0 -- the
       button override's `bypass = true` has no visible effect here. */
    {
        reset_all();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0xBD);
        g_rolling_counter_mult = 5;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x1000}};
        uint8_t btn = encode_button_and_counter_block(&inst);
        assert(btn == 0xD); // 0xBD & (0xFF >> (8-4))
        assert(inst.generic.cnt == 0x1005); // standard increment, unaffected
        checks += 2;
    }

    /* --- Scenario B (regression, no overrides at all, mode 0): btn is
       whatever get_btn_code() returned, cnt gets the plain standard-mode
       increment. */
    {
        reset_all();
        g_btn_code = 0x4;
        g_rolling_counter_mult = 3;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x2000}};
        uint8_t btn = encode_button_and_counter_block(&inst);
        assert(btn == 0x4);
        assert(inst.generic.cnt == 0x2003);
        checks += 2;
    }

    /* --- Scenario C (counter override pending, mode 0, standard mult would
       otherwise apply): override value used verbatim, masked to
       cnt_length_bit (16) -- the whole standard-mode +mult arithmetic is
       skipped even though g_rolling_counter_mult is set to an ordinary
       standard-mode value. */
    {
        reset_all();
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x1FFFF);
        g_rolling_counter_mult = 5;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x3000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFF); // 0x1FFFF & 0xFFFF (16 bits)
        checks++;
    }

    /* --- Scenario D (counter override pending while mode 1 "floxi2r" is
       active): subghz_block_generic_global_counter_override_get() only ever
       returns true when cnt_need_override was set by ..._set(), and the
       connection point reuses that exact same flag
       (`if(subghz_block_generic_global.cnt_need_override) bypass = true;`)
       as the bypass trigger -- so a pending counter override always forces
       `bypass = true` before the mode==1 branch is reached, escaping the
       floxi2r toggle for this one pass, exactly like a pending override
       would escape CAME Atomo/Alutech AT-4N's multi-counter-mode selection
       (Phase 7/8). Without the override pending, mode 1 would instead have
       toggled 0x5000 -> 0xFFFE (see Scenario F below). */
    {
        reset_all();
        nice_flors_counter_mode = 1;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0x4242);
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x5000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x4242);
        checks++;
    }

    /* --- Scenario E (button-only override pending, no counter override,
       mode 2 "ox2" active): the button-override call's `bypass = true` is
       the *same* flag the counter-mode gate checks -- so even though no
       counter override is pending at all, the active mode-2 toggle is
       silently pre-empted for this one pass in favor of the mode-0
       standard-mode fallback (override_get() returns false since
       cnt_need_override is false, so the plain "+mult" branch runs).
       Without a button override pending, mode 2 would instead have toggled
       0x7 -> 0 (see Scenario G below) -- this is the side effect documented
       in the file header above, real already-shipped behavior of sharing
       one `bypass` bool between both call sites. */
    {
        reset_all();
        nice_flors_counter_mode = 2;
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0x2);
        g_rolling_counter_mult = 9;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x7}};
        uint8_t btn = encode_button_and_counter_block(&inst);
        assert(btn == 0x2);
        assert(inst.generic.cnt == 0x10); // 0x7 + 9, NOT the ox2 toggle-to-0
        checks += 2;
    }

    /* --- Scenario F (regression, no overrides, mode 1 "floxi2r", both real
       sub-branches): toggles 0xFFFE<->0x0001, independent of
       g_rolling_counter_mult. */
    {
        reset_all();
        nice_flors_counter_mode = 1;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x5000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE);
        checks++;
    }
    {
        reset_all();
        nice_flors_counter_mode = 1;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0xFFFE}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x0001);
        checks++;
    }

    /* --- Scenario G (regression, no overrides, mode 2 "ox2", both real
       sub-branches): cnt>=1 resets to 0, cnt==0 increments to 1. */
    {
        reset_all();
        nice_flors_counter_mode = 2;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x7}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0);
        checks++;
    }
    {
        reset_all();
        nice_flors_counter_mode = 2;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 1);
        checks++;
    }

    /* --- Scenario H (regression, no overrides, mode 0, OFEX sentinel
       active, all three real sub-cases of the resync dance): cnt+1 > 0xFFFF
       (i.e. cnt==0xFFFF) resets to 0; cnt>=1 && cnt!=0xFFFE jumps straight
       to 0xFFFE; otherwise (cnt==0 or cnt==0xFFFE) increments by one. */
    {
        reset_all();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0xFFFF}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0);
        checks++;
    }
    {
        reset_all();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x5000}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFE);
        checks++;
    }
    {
        reset_all();
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 1);
        checks++;
    }

    /* --- Scenario I (counter override pending, OFEX sentinel active, mode
       0): the same cnt_need_override-to-bypass trigger as Scenario D makes
       the override reachable even though the OFEX sentinel is active --
       `furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF || bypass`
       is true via the `|| bypass` arm, not the mult comparison. */
    {
        reset_all();
        g_rolling_counter_mult = -0x7FFFFFFF;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0xABCD);
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0xFFFE}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xABCD);
        checks++;
    }

    /* --- Scenario J (both overrides together in the same pass, mode 0):
       the real code runs the button block unconditionally before the
       counter block, independent of whether a counter override is also
       pending. */
    {
        reset_all();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_button_override_set(0xA9);
        subghz_block_generic_global_counter_override_set(0x42);
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x1000}};
        uint8_t btn = encode_button_and_counter_block(&inst);
        assert(btn == 0x9); // 0xA9 & 0xF
        assert(inst.generic.cnt == 0x42);
        checks += 2;
    }

    printf("Scenarios A-J (button/counter override pre-empt the active counter "
           "mode including modes 1/2, with correct masking; the shared bypass "
           "flag's two documented side effects -- counter override escaping "
           "modes 1/2, button-only override silently pre-empting modes 1/2 too; "
           "unchanged regression for modes 0/1/2 and all three OFEX sub-cases; "
           "both overrides applying together): %d checks OK\n",
           checks);

    /* --- Scenario K: get_string global-push block -- cnt/btn availability
       and values pushed unconditionally, matching the real two assignment
       groups (both cnt_is_available and btn_is_available always true for
       this protocol, same as every protocol ported so far except Secplus
       v1). cnt_length_bit is 16, btn_length_bit is 4 -- this protocol's own
       real widths, identical to KeeLoq/Princeton/Somfy Telis/Phoenix
       V2/CAME Atomo/Somfy Keytis/FAAC SLH's button width (4) but NOT FAAC
       SLH's counter width (20) or Secplus v2's (28). */
    int before_k = checks;
    {
        reset_all();
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0xBEEF, .btn = 0x8}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.cnt_length_bit == 16);
        assert(subghz_block_generic_global.current_cnt == 0xBEEF);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0x8);
        assert(subghz_block_generic_global.btn_length_bit == 4);
        checks += 6;
    }
    {
        reset_all();
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario K (get_string global-push: cnt and btn both always available, "
        "16-bit counter / 4-bit button widths): %d checks OK\n",
        checks - before_k);

    /* --- Scenario L: the length-bit-ordering contract from Phase 1 holds
       for this protocol's real bit widths -- get_string() (Fragment 2) sets
       both cnt_length_bit (16) and btn_length_bit (4) unconditionally,
       before any later override_get() call in encode() (Fragment 1) can be
       reached on any real call path (get_string() always runs first, when a
       signal is displayed, before a later send). Demonstrated here by
       running Fragment 2 first and reusing its resulting lengths for a
       subsequent Fragment-1 override round-trip -- no UB, correctly masked
       result on both fields. */
    {
        reset_all();
        MiniNiceFlorSInstance inst = {.generic = {.cnt = 0x1, .btn = 0x1}};
        get_string_push_block(&inst); // sets cnt_length_bit=16, btn_length_bit=4
        subghz_block_generic_global_counter_override_set(0x1FFFF);
        subghz_block_generic_global_button_override_set(0xBD);
        uint8_t btn = encode_button_and_counter_block(&inst);
        assert(btn == 0xD); // 0xBD & 0xF, using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFF); // 0x1FFFF & 0xFFFF
        checks += 2;
    }
    printf("Scenario L (length-bit ordering contract holds for this protocol's "
           "real 16-bit counter / 4-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
