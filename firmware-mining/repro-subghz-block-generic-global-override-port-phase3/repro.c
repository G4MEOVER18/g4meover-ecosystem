/* Host repro for Phase 3 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered button override mechanism into Princeton -- the first Phase-3
 * protocol (see firmware-mining/subghz-block-generic-global-override-port.md's
 * Port-Skizze and "Phase 3: Princeton" section). Unlike the Phase 2 KeeLoq pilot,
 * Princeton is a fixed-code (SubGhzProtocolTypeStatic), not a rolling-code, protocol --
 * it only ever uses the button override, never the counter override.
 *
 * This does NOT compile the full, real princeton.c (it needs custom_btn_i.h,
 * math.h and the decoder/encoder feed state machine, none of which this change
 * touches). Instead it `#include`s the exact statements the patch adds, extracted
 * byte-for-byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/princeton.c
 * (see the sed line ranges in the accompanying .md), and links them against the real,
 * unmodified Phase-1 generic.c -- the actual subghz_block_generic_global_button_override_get()
 * this patch calls. Everything surrounding those statements in the real file (the
 * get_btn_code() custom-button state machine, the printf formatting, the TE/duration
 * encoding) is pre-existing, untouched code and is not re-verified here.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;

#define TAG "SubGhzProtocolPrincetonRepro"

typedef struct {
    SubGhzBlockGeneric generic;
} MiniPrincetonInstance;

/* --- Fragment 1: the patched button-override call site plus the pre-existing data
   reconstruction it feeds, from subghz_protocol_encoder_princeton_get_upload() (sed
   264,280p on the shipped princeton.c). Verbatim. The real function first assigns
   instance->generic.btn = subghz_protocol_princeton_get_btn_code() (pre-existing,
   untouched, a pure custom-button state machine unrelated to this patch) -- this
   wrapper instead takes that pre-override value as a parameter, mirroring exactly
   what get_btn_code() would have just returned. */
static void get_upload_block(MiniPrincetonInstance* instance, uint8_t btn_before_override) {
    instance->generic.btn = btn_before_override;
#include "button_override_and_reconstruction_block.inc"
}

/* --- Fragment 2: the three patched field-population statements in
   subghz_protocol_decoder_princeton_get_string() (sed 602,604p / 608p / 627p),
   reassembled with the same if/else branch condition the real function uses (that
   condition itself is pre-existing, untouched code, reproduced here only as control
   flow -- not re-verified). */
static void get_string_availability_sim(MiniPrincetonInstance* instance) {
#include "get_string_avail_prefix_block.inc"

    if(instance->generic.btn == 0x30 || instance->generic.btn == 0xC0 ||
       instance->generic.btn == 0xF3 || instance->generic.btn == 0xFC) {
#include "get_string_branch_8b_block.inc"
    } else {
#include "get_string_branch_4b_block.inc"
    }
}

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
}

int main(void) {
    int checks = 0;
    const uint32_t serial = 0x12345;

    /* --- Scenario A: override pending, lands in the 8-bit/no-mask reconstruction
       branch (btn == 0x30 or 0xC0) -- must short-circuit the custom-button value and
       feed the overridden btn into the existing data formula, then clear the one-shot
       flag. Real get_string() (Fragment 2) always sets btn_length_bit before any later
       override is applied -- the one real precondition carried over from Phase 1's
       documented UB trap (see "Risiko"), reproduced here for completeness even though
       Princeton's mask formula (btn_length_bit 4 or 8, never 0) cannot actually hit it. */
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0x30);
        MiniPrincetonInstance inst = {.generic = {.serial = serial}};
        get_upload_block(&inst, /*btn_before_override=*/0x9);
        assert(inst.generic.btn == 0x30);
        assert(inst.generic.data == (((uint64_t)serial << 8) | 0x30));
        assert(subghz_block_generic_global.btn_need_override == false); // one-shot clears
        checks += 3;

        // One-shot clear actually sticks: next call, no new override_set, a different
        // pre-override sentinel (as a fresh get_btn_code() call would return) must pass
        // through unchanged, landing in the 4-bit/else branch this time.
        get_upload_block(&inst, /*btn_before_override=*/0x5);
        assert(inst.generic.btn == 0x5); // not re-overridden
        assert(inst.generic.data == (((uint64_t)serial << 4) | 0x5));
        checks += 2;
    }
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0xC0);
        MiniPrincetonInstance inst = {.generic = {.serial = serial}};
        get_upload_block(&inst, /*btn_before_override=*/0x1);
        assert(inst.generic.btn == 0xC0);
        assert(inst.generic.data == (((uint64_t)serial << 8) | 0xC0));
        checks += 2;
    }
    printf("Scenario A (override into the 8bit/no-mask branch, one-shot clear): %d checks OK\n", checks);

    /* --- Scenario B: override pending, lands in the 8-bit/masked-to-4-bit
       reconstruction branch (btn == 0xF3 or 0xFC) -- the one branch where the override
       value itself gets masked with 0xF before being folded into generic.data, same as
       the pre-existing (unmodified) formula already did for a get_btn_code() result. */
    int before_b = checks;
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0xF3);
        MiniPrincetonInstance inst = {.generic = {.serial = serial}};
        get_upload_block(&inst, /*btn_before_override=*/0x2);
        assert(inst.generic.btn == 0xF3);
        assert(inst.generic.data == (((uint64_t)serial << 8) | (0xF3 & 0xF)));
        checks += 2;
    }
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 8;
        subghz_block_generic_global_button_override_set(0xFC);
        MiniPrincetonInstance inst = {.generic = {.serial = serial}};
        get_upload_block(&inst, /*btn_before_override=*/0x4);
        assert(inst.generic.btn == 0xFC);
        assert(inst.generic.data == (((uint64_t)serial << 8) | (0xFC & 0xF)));
        checks += 2;
    }
    printf(
        "Scenario B (override into the 8bit/masked-to-4bit branch): %d checks OK\n",
        checks - before_b);

    /* --- Scenario C: override pending, lands in the plain 4-bit/else branch -- any
       value that is not one of the four 8-bit button codes. */
    before_b = checks;
    {
        reset_global();
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0x7);
        // Pre-override sentinel deliberately an 8-bit code, to prove the override --
        // not the sentinel -- decides which reconstruction branch runs.
        MiniPrincetonInstance inst = {.generic = {.serial = serial}};
        get_upload_block(&inst, /*btn_before_override=*/0x30);
        assert(inst.generic.btn == 0x7);
        assert(inst.generic.data == (((uint64_t)serial << 4) | 0x7));
        checks += 2;
    }
    printf("Scenario C (override into the plain 4bit/else branch): %d checks OK\n", checks - before_b);

    /* --- Scenario D: no override pending -- the pre-existing reconstruction formula
       must run completely unchanged for every button code the real get_btn_code() can
       return (regression against the ALT/pre-patch behaviour). */
    before_b = checks;
    struct {
        uint8_t btn;
        uint64_t expect_data;
    } cases[] = {
        {0x30, ((uint64_t)serial << 8) | 0x30},
        {0xC0, ((uint64_t)serial << 8) | 0xC0},
        {0xF3, ((uint64_t)serial << 8) | (0xF3 & 0xF)},
        {0xFC, ((uint64_t)serial << 8) | (0xFC & 0xF)},
        {0x1, ((uint64_t)serial << 4) | 0x1},
        {0x2, ((uint64_t)serial << 4) | 0x2},
        {0x4, ((uint64_t)serial << 4) | 0x4},
        {0x8, ((uint64_t)serial << 4) | 0x8},
        {0xF, ((uint64_t)serial << 4) | 0xF},
    };
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_global();
        MiniPrincetonInstance inst = {.generic = {.serial = serial}};
        get_upload_block(&inst, cases[i].btn);
        assert(inst.generic.btn == cases[i].btn); // unchanged, nothing overridden it
        assert(inst.generic.data == cases[i].expect_data);
        checks += 2;
    }
    printf(
        "Scenario D (unchanged reconstruction for every btn code when no override is pending): %d checks OK\n",
        checks - before_b);

    /* --- Scenario E: get_string availability population, one case per display branch
       (matches the two branches the real decoder dispatches on). */
    before_b = checks;
    {
        reset_global();
        MiniPrincetonInstance inst = {.generic = {.btn = 0xC0}};
        get_string_availability_sim(&inst);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0xC0);
        assert(subghz_block_generic_global.btn_length_bit == 8);
        checks += 3;
    }
    {
        reset_global();
        MiniPrincetonInstance inst = {.generic = {.btn = 0x3}};
        get_string_availability_sim(&inst);
        assert(subghz_block_generic_global.btn_is_available == true);
        assert(subghz_block_generic_global.current_btn == 0x3);
        assert(subghz_block_generic_global.btn_length_bit == 4);
        checks += 3;
    }
    printf(
        "Scenario E (get_string availability fields per display branch): %d checks OK\n",
        checks - before_b);

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
