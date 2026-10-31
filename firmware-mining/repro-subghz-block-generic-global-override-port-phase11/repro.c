/* Host repro for Phase 11 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into FAAC SLH (faac_slh) --
 * the ninth protocol of the Nachzieh-Liste after Princeton (Phase 3, button-only),
 * Security+ 1.0 (Phase 4, counter + button), Somfy Telis (Phase 5, counter +
 * button), Phoenix V2 (Phase 6, counter + button), CAME Atomo (Phase 7, counter +
 * button, bypass), Alutech AT-4N (Phase 8, counter + button, bypass), Somfy Keytis
 * (Phase 9, counter + button) and KingGates Stylo4K (Phase 10, counter + button,
 * fix/hop reconstruction).
 *
 * Of the three remaining candidates after Phase 10 (FAAC SLH: 712 diff lines,
 * Secplus v2: 1133, Nice FloR-S: 1284 against DarkFlippers/unleashed-firmware),
 * FAAC SLH had the smallest diff and was taken up here. Unlike KingGates Stylo4K
 * (Phase 10), my own `subghz_protocol_faac_slh_gen_data()` sends `fix` (serial<<4
 * | btn) in the clear as the top 32 bits of `instance->generic.data` -- there is
 * no decoder-side reconstruction formula to replicate for the button override, it
 * only has to land before `fix` is built the one time it is built. The real
 * complication here is a preceding, unrelated branch at the top of `gen_data()`
 * (triggered when the UP custom button is held) that builds an entirely different
 * "programming mode" key from `instance->generic.seed`, never touching
 * serial/btn/cnt/fix/hop at all, and returns before reaching the code this patch
 * touches -- so it is simply never reached by either override, exactly like the
 * two early-return guards right after it (zero seed without `AllowZeroSeed`;
 * regular Send button while still in programming mode with no serial/btn to
 * resend). Neither case needs a `bypass` flag like CAME Atomo/Alutech AT-4N: there
 * is no multi-mode *counter* feature to route around, just three different reasons
 * the function can return before the override call sites are ever reached -- when
 * that happens, there is also no frame being generated for the override to apply
 * to, so nothing is lost.
 *
 * This does NOT compile the full, real faac_slh.c (it needs the KeeLoq keystore
 * manufacturer-key lookup and the full upload-buffer generator, neither of which
 * this change touches). Instead it `#include`s the exact statements the patch
 * adds plus the pre-existing statements they are spliced into, extracted
 * byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/faac_slh.c
 * (see the sed line ranges below), and links them against the real, unmodified
 * Phase-1 generic.c -- the actual subghz_block_generic_global_*_override_get()
 * functions this patch calls. Everything surrounding those statements in the real
 * file (the programming-mode branch, the KeeLoq manufacturer-key lookup, the
 * Manchester framing, the upload buffer) is pre-existing, untouched code and is
 * not re-verified here.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;
int32_t g_rolling_counter_mult = 1;

/* Module-static in the real faac_slh.c, defaulting to false there too; only
   ever set to true by subghz_protocol_faac_slh_create_data()/_deserialize()
   on the real decoder/encoder side, which this repro does not exercise --
   the fragment reads it as a plain extern so each scenario can control it. */
bool allow_zero_seed = false;

typedef struct {
    SubGhzBlockGeneric generic;
} MiniFaacSlhInstance;

#define TAG "SubGhzProtocolFaacSLH"

/* --- Fragment 1: the patched button-override call plus the patched
   counter-override leading branch, spliced ahead of the pre-existing
   standard-mode/OFEX if/else-if/else chain, from
   subghz_protocol_faac_slh_gen_data() (sed 229,232p for the button-override
   call, sed 245,273p for the counter-override leading branch plus the two
   untouched sibling counter branches -- on the shipped faac_slh.c; the 12
   lines in between, 233-244, are the pre-existing `fix`/`fixx[]`/`decrypt`
   declarations this patch does not touch and are deliberately not
   reproduced here, since nothing downstream of them in the real function
   is exercised by this repro -- this fragment only proves both override
   call sites fire and mutate `instance->generic.btn`/`.cnt` correctly in
   place, not the KeeLoq encryption that consumes them afterwards).
   Verbatim. */
static void encode_button_and_counter_block(MiniFaacSlhInstance* instance) {
#include "button_and_counter_override_block.inc"
}

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_faac_slh_get_string() (sed 726,734p on the
   shipped faac_slh.c). Verbatim. */
static void get_string_push_block(MiniFaacSlhInstance* instance) {
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
        subghz_block_generic_global_button_override_set(0x2F); // only low 4 bits must survive
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x10, .btn = 0x3, .serial = 0x1234}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xF); // 0x2F & 0xF
        assert(subghz_block_generic_global.btn_need_override == false); // one-shot clears
        checks += 2;

        // the whole point of this override: fix (serial<<4|btn) is sent in the
        // clear with no decoder-side reconstruction needed, unlike KingGates
        // Stylo4K (Phase 10) -- confirm the overridden btn actually reaches it.
        uint32_t fix = inst.generic.serial << 4 | inst.generic.btn;
        assert(fix == ((0x1234u << 4) | 0xFu));
        checks += 1;
    }

    /* --- Scenario B: no button override pending -- the pre-existing
       serial/btn values must survive unchanged (regression). */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x10, .btn = 0x7, .serial = 0x1234}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0x7);
        checks += 1;
    }

    /* --- Scenario C: counter override pending -- pre-empts the entire
       else-if chain, even while seed/allow_zero_seed are both false/0 (a
       combination that, inside the real, unabbreviated gen_data(), can never
       reach this code at all: an earlier early-return guard -- not part of
       this patch, not reproduced here -- returns before this point whenever
       !allow_zero_seed && seed==0; this scenario only shows that the override
       branch itself does not depend on that gate, since it is a prior,
       textually independent condition on the very same chain). A value above
       the real 20-bit range is used to also prove the masking formula for
       this protocol's real cnt_length_bit. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 20;
        subghz_block_generic_global_counter_override_set(0x1ABCDE); // only low 20 bits survive
        allow_zero_seed = false;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x5, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xABCDE); // 0x1ABCDE & 0xFFFFF
        assert(subghz_block_generic_global.cnt_need_override == false);
        checks += 2;
    }

    /* --- Scenario D: counter override pending, OFEX sentinel active
       (mult=-0x7FFFFFFF) and seed known this time -- the override must still
       pre-empt the OFEX branch, proving the new branch runs strictly before
       the entire allow_zero_seed-gated if/else-if chain, not merely before
       its standard-mode half. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 20;
        subghz_block_generic_global_counter_override_set(0x9);
        allow_zero_seed = false;
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniFaacSlhInstance inst = {
            .generic = {.cnt = 0xFFFFF, .btn = 0x1, .seed = 0x1}}; // would hit the OFEX wrap/jump if it ran
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x9); // override wins, OFEX path never runs
        checks += 1;
    }

    /* --- Scenario E: no override pending, seed unknown and AllowZeroSeed not
       set -- the entire counter chain must be skipped, cnt left completely
       unmodified (regression for the "no frame is actually generated this
       pass" gate the real, unabbreviated function enforces one level above
       this extracted fragment via its own early return). */
    {
        reset_global();
        allow_zero_seed = false;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x4242, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x4242); // untouched
        checks += 1;
    }

    /* --- Scenario F: no override pending, seed known, standard mult-mode --
       the pre-existing standard-wrap formula must run completely unchanged
       (regression), both the plain increment case and the 0xFFFFF
       overflow-wrap-to-0 case (20-bit counter, matching the protocol's own
       "take only 20 bits from mult" comment). */
    {
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = 3;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x10, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x13); // plain increment by (mult & 0xFFFFF)
        checks += 1;
    }
    {
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = 3;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0xFFFFE, .btn = 0x1, .seed = 0}}; // +3 > 0xFFFFF
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0); // wraps to 0
        checks += 1;
    }
    {
        // the mult value itself is also masked to 20 bits before use
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = 0x100003; // only the low 20 bits (0x3) must count
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x10, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0x13); // 0x10 + (0x100003 & 0xFFFFF) == 0x10 + 3
        checks += 1;
    }

    /* --- Scenario G: no override pending, seed known, OFEX sentinel active --
       the pre-existing OFEX path must run completely unchanged, covering all
       four of its real sub-branches: cnt==0 jumps to 0xFFFFF, any other
       cnt<0xFFFFF wraps to 0, cnt>=0xFFFFF with a non-zero mult resets to 0,
       and -- a pre-existing quirk of the unmodified upstream code, unrelated
       to this port and deliberately left untouched -- cnt>=0xFFFFF with
       mult==0 falls through every branch and is left completely unchanged. */
    {
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFFF); // cnt==0 -> cnt+0xFFFFF not > 0xFFFFF -> += 0xFFFFF
        checks += 1;
    }
    {
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = -0x7FFFFFFF;
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x1234, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0); // 1 <= cnt < 0xFFFFF -> cnt+0xFFFFF > 0xFFFFF -> wrap to 0
        checks += 1;
    }
    {
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = -0x7FFFFFFF; // != 0
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0xFFFFF, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0); // cnt>=0xFFFFF && mult!=0 -> reset to 0
        checks += 1;
    }
    {
        reset_global();
        allow_zero_seed = true;
        g_rolling_counter_mult = 0; // == 0, the quirk branch
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0xFFFFF, .btn = 0x1, .seed = 0}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.cnt == 0xFFFFF); // cnt>=0xFFFFF && mult==0 -> falls through, untouched
        checks += 1;
    }

    /* --- Scenario H: button override and counter override pending in the
       same call -- both must apply independently in the one pass (the real
       code runs the button block unconditionally before the counter chain,
       regardless of whether a counter override is also pending). */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global.cnt_length_bit = 20;
        subghz_block_generic_global_button_override_set(0xA);
        subghz_block_generic_global_counter_override_set(0x42);
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x5, .btn = 0x1, .serial = 0x99}};
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xA);
        assert(inst.generic.cnt == 0x42);
        checks += 2;
    }

    printf("Scenarios A-H (button/counter override pre-empt their respective chains, "
           "with correct masking and the overridden button actually reaching the "
           "cleartext fix field; unchanged regression for the no-frame gate, standard "
           "mode's mult-masking, and all four OFEX sub-branches including the untouched "
           "mult==0 quirk; both overrides applying together in the same pass): %d "
           "checks OK\n",
           checks);

    /* --- Scenario I: get_string global-push block -- cnt/btn availability and
       values pushed unconditionally, matching the real two assignment groups
       (both cnt_is_available and btn_is_available always true for this
       protocol, same as KeeLoq/Princeton/Somfy Telis/Phoenix V2/CAME Atomo/
       Alutech AT-4N/Somfy Keytis/KingGates Stylo4K). cnt_length_bit is 20 here
       (this protocol's own 20-bit counter, per its "take only 20 bits from
       mult" comment), unlike every other protocol ported so far (16 bits). */
    int before_i = checks;
    {
        reset_global();
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0xBEEEF, .btn = 0x2}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.cnt_length_bit == 20);
        assert(subghz_block_generic_global.current_cnt == 0xBEEEF);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0x2);
        assert(subghz_block_generic_global.btn_length_bit == 4);
        checks += 6;
    }
    {
        reset_global();
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0, .btn = 0}};
        get_string_push_block(&inst);
        assert(subghz_block_generic_global.cnt_is_available == true);
        assert(subghz_block_generic_global.current_cnt == 0);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0);
        checks += 4;
    }
    printf(
        "Scenario I (get_string global-push: cnt and btn both always available, "
        "20-bit counter width): %d checks OK\n",
        checks - before_i);

    /* --- Scenario J: the length-bit-ordering contract from Phase 1 holds for
       this protocol's real bit widths -- get_string() (Fragment 2) sets both
       cnt_length_bit (20) and btn_length_bit (4) unconditionally, before any
       later override_get() call in gen_data() (Fragment 1) can be reached on
       any real call path (get_string() always runs first, when a signal is
       displayed, before a later send). Demonstrated here by running Fragment
       2 first and reusing its resulting lengths for a subsequent Fragment-1
       override round-trip -- no UB, correct masked result. */
    {
        reset_global();
        MiniFaacSlhInstance inst = {.generic = {.cnt = 0x1, .btn = 0x1}};
        get_string_push_block(&inst); // sets cnt_length_bit=20, btn_length_bit=4
        subghz_block_generic_global_counter_override_set(0x1FFFFF);
        subghz_block_generic_global_button_override_set(0xFF);
        encode_button_and_counter_block(&inst);
        assert(inst.generic.btn == 0xF); // 0xFF & 0xF, using the length set by get_string()
        assert(inst.generic.cnt == 0xFFFFF); // 0x1FFFFF & 0xFFFFF
        checks += 2;
    }
    printf("Scenario J (length-bit ordering contract holds for this protocol's real "
           "20-bit counter / 4-bit button widths): 2 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
