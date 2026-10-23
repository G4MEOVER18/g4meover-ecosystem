/* Host repro for Phase 2 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into KeeLoq -- the pilot protocol
 * chosen in firmware-mining/subghz-block-generic-global-override-port.md's Port-Skizze.
 *
 * This does NOT compile the full, real keeloq.c (it needs subghz_keystore_i.h,
 * custom_btn_i.h and m-array.h, none of which this change touches). Instead it
 * `#include`s the exact statements the patch adds, extracted byte-for-byte via `sed`
 * from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/keeloq.c
 * (see the sed line ranges in the accompanying .md), and links them against the real,
 * unmodified Phase-1 generic.c -- the actual
 * subghz_block_generic_global_counter_override_get()/_button_override_get() this patch
 * calls. Everything surrounding those statements in the real file (manufacturer string
 * compares, printf formatting, encryption) is pre-existing, untouched code and is not
 * re-verified here.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;

/* Minimal stand-ins for the real firmware types the extracted fragments reference,
   just wide enough for the two call sites touched by this patch. */
typedef int ProgMode;
#define PROG_MODE_OFF 0
#define TAG "SubGhzProtocolKeeloqRepro"

typedef struct {
    SubGhzBlockGeneric generic;
    const char* manufacture_name;
} MiniKeeloqInstance;

/* --- Fragment 1: the patched counter-increment block from
   subghz_protocol_keeloq_gen_data() (sed 186,261p on the shipped keeloq.c). Verbatim,
   references only instance->generic.cnt, keeloq_counter_mode, prog_mode,
   furi_hal_subghz_get_rolling_counter_mult() and the real override_get(). */
static void counter_up_block(
    MiniKeeloqInstance* instance,
    bool counter_up,
    ProgMode prog_mode,
    uint8_t keeloq_counter_mode) {
#include "counter_block.inc"
}

/* --- Fragment 2: the patched button-override call site in
   subghz_protocol_encoder_keeloq_get_upload() (sed 523,527p). Verbatim. */
static uint8_t button_override_wrapper(uint8_t btn) {
#include "button_override_block.inc"
    return btn;
}

/* --- Fragment 3: the three patched field-population statement groups in
   subghz_protocol_decoder_keeloq_get_string() (sed 1428,1432p / 1445,1447p / 1469p /
   1487,1489p), reassembled with the same if/else-if/else manufacturer dispatch the real
   function uses (that dispatch itself is pre-existing, untouched code, reproduced here
   only as control flow -- not re-verified). */
static void get_string_availability_sim(MiniKeeloqInstance* instance) {
#include "get_string_btn_block.inc"

    if(strcmp(instance->manufacture_name, "BFT") == 0) {
#include "get_string_bft_block.inc"
    } else if(strcmp(instance->manufacture_name, "Unknown") == 0) {
#include "get_string_unknown_block.inc"
    } else {
#include "get_string_else_block.inc"
    }
}

int32_t g_rolling_counter_mult = 1;

static void reset_global(void) {
    subghz_block_generic_global_reset(NULL);
}

int main(void) {
    int checks = 0;

    /* --- Scenario A: counter override pending -- must short-circuit every counter_mode,
       write exactly new_cnt, and clear the one-shot flag (regression for all 6 modes +
       the OFEX branch of mode 0). */
    for(uint8_t mode = 0; mode <= 6; mode++) {
        for(int ofex = 0; ofex <= 1; ofex++) {
            reset_global();
            /* Mirrors the real call order: get_string() (this patch's Fragment 3) always
               runs first and sets cnt_length_bit before any later override is applied --
               the one real precondition documented as a UB trap in Phase 1's "Risiko" for
               calling counter_override_get() before cnt_length_bit is set. */
            subghz_block_generic_global.cnt_length_bit = 16;
            g_rolling_counter_mult = ofex ? -0x7FFFFFFF : 1;
            subghz_block_generic_global_counter_override_set(0xBEEF);

            MiniKeeloqInstance inst = {0};
            inst.generic.cnt = 0x1234;
            counter_up_block(&inst, true, PROG_MODE_OFF, mode);

            assert(inst.generic.cnt == 0xBEEF);
            checks++;
            assert(subghz_block_generic_global.cnt_need_override == false); // one-shot clears
            checks++;
        }
    }
    printf("Scenario A (counter override short-circuits every mode): %d checks OK\n", checks);

    /* --- Scenario A2: the one-shot clear from A actually sticks -- a second call right
       after, without a fresh override_set, must fall through to the real mode-0
       increment instead of reapplying the stale override. */
    {
        reset_global();
        subghz_block_generic_global.cnt_length_bit = 16;
        g_rolling_counter_mult = 1;
        subghz_block_generic_global_counter_override_set(0xBEEF);
        MiniKeeloqInstance inst = {0};
        inst.generic.cnt = 0x1234;
        counter_up_block(&inst, true, PROG_MODE_OFF, 0);
        assert(inst.generic.cnt == 0xBEEF);
        checks++;
        counter_up_block(&inst, true, PROG_MODE_OFF, 0); // no new override_set
        assert(inst.generic.cnt == 0xBEF0); // plain mode-0 +1 increment, not re-overridden
        checks++;
    }
    printf("Scenario A2 (one-shot override does not reapply on the next call): 2 checks OK\n");

    /* --- Scenario B: no override pending -- every counter_mode's pre-existing increment
       logic must run completely unchanged (regression against the ALT/pre-patch
       behaviour, reproduced inline below for comparison since mode 0-6 is simple,
       well-isolated arithmetic). */
    int before_b = checks;
    struct {
        uint8_t mode;
        uint32_t in;
        uint32_t expect;
    } cases[] = {
        {0, 0x1000, 0x1001}, // mode 0, mult=1, below boundary
        {0, 0xFFFF, 0x0000}, // mode 0, mult=1, overflow wrap
        {1, 0xFFFE, 0xFFFF}, // mode 1
        {1, 0xFFFF, 0x0000}, // mode 1 overflow
        {2, 0x0001, 0x3334}, // mode 2, +0x3333
        {2, 0xFFFF, 0x0000}, // mode 2 overflow
        {3, 0x0000, 0x8006}, // mode 3, not in {8006,8007,0006} -> reset to 8006
        {3, 0x8007, 0x0006}, // mode 3 wrap
        {3, 0x0006, 0x0007}, // mode 3 increment inside the set
        {4, 0x0000, 0x807B}, // mode 4, analogous to mode 3
        {4, 0x807C, 0x007B},
        {5, 0x0000, 0xFFFF}, // mode 5, 0000<->FFFF toggle
        {5, 0xFFFF, 0x0000},
        {6, 0x4242, 0x4242}, // mode 6, frozen -- unchanged
    };
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_global();
        g_rolling_counter_mult = 1;
        MiniKeeloqInstance inst = {0};
        inst.generic.cnt = cases[i].in;
        counter_up_block(&inst, true, PROG_MODE_OFF, cases[i].mode);
        assert(inst.generic.cnt == cases[i].expect);
        checks++;
    }
    printf(
        "Scenario B (unchanged mode-0..6 arithmetic when no override is pending): %d checks OK\n",
        checks - before_b);

    /* --- Scenario C: counter_up == false or prog_mode != OFF must never touch cnt,
       override pending or not (regression -- these guards are pre-existing, unchanged by
       this patch, but the new override check sits inside them so must still respect
       them). */
    reset_global();
    subghz_block_generic_global_counter_override_set(0xBEEF);
    MiniKeeloqInstance inst_c1 = {0};
    inst_c1.generic.cnt = 0x5555;
    counter_up_block(&inst_c1, false, PROG_MODE_OFF, 0);
    assert(inst_c1.generic.cnt == 0x5555);
    assert(subghz_block_generic_global.cnt_need_override == true); // untouched, still pending
    checks += 2;

    reset_global();
    subghz_block_generic_global_counter_override_set(0xBEEF);
    MiniKeeloqInstance inst_c2 = {0};
    inst_c2.generic.cnt = 0x6666;
    counter_up_block(&inst_c2, true, /*prog_mode=*/1, 0);
    assert(inst_c2.generic.cnt == 0x6666);
    assert(subghz_block_generic_global.cnt_need_override == true);
    checks += 2;
    printf("Scenario C (counter_up/prog_mode guards still respected): 4 checks OK\n");

    /* --- Scenario D: button override, pending vs. not pending, one-shot clear. Same
       get_string()-sets-length-first precondition as the counter: real get_string()
       always sets btn_length_bit = 4 (Fragment 3) before any later override is applied. */
    reset_global();
    subghz_block_generic_global.btn_length_bit = 4;
    subghz_block_generic_global_button_override_set(0x7);
    uint8_t btn = button_override_wrapper(0xA);
    assert(btn == 0x7);
    assert(subghz_block_generic_global.btn_need_override == false);
    checks += 2;

    reset_global();
    subghz_block_generic_global.btn_length_bit = 4;
    btn = button_override_wrapper(0xA);
    assert(btn == 0xA); // nothing pending -> unchanged
    checks += 1;
    printf("Scenario D (button override one-shot get/clear): 3 checks OK\n");

    /* --- Scenario E: get_string availability population, one case per manufacturer
       branch (matches the three branches the real decoder dispatches on). */
    reset_global();
    MiniKeeloqInstance bft = {.manufacture_name = "BFT"};
    bft.generic.btn = 0x3;
    bft.generic.cnt = 0x4242;
    get_string_availability_sim(&bft);
    assert(subghz_block_generic_global.btn_is_available == true);
    assert(subghz_block_generic_global.current_btn == 0x3);
    assert(subghz_block_generic_global.btn_length_bit == 4);
    assert(subghz_block_generic_global.cnt_is_available == true);
    assert(subghz_block_generic_global.cnt_length_bit == 16);
    assert(subghz_block_generic_global.current_cnt == 0x4242);
    checks += 6;

    reset_global();
    MiniKeeloqInstance unknown = {.manufacture_name = "Unknown"};
    unknown.generic.btn = 0x1;
    get_string_availability_sim(&unknown);
    assert(subghz_block_generic_global.btn_is_available == true); // btn still editable
    assert(subghz_block_generic_global.cnt_is_available == false); // no counter, replay only
    checks += 2;

    reset_global();
    MiniKeeloqInstance other = {.manufacture_name = "Centurion"};
    other.generic.btn = 0x2;
    other.generic.cnt = 0x9999;
    get_string_availability_sim(&other);
    assert(subghz_block_generic_global.btn_is_available == true);
    assert(subghz_block_generic_global.cnt_is_available == true);
    assert(subghz_block_generic_global.cnt_length_bit == 16);
    assert(subghz_block_generic_global.current_cnt == 0x9999);
    checks += 4;
    printf("Scenario E (get_string availability fields per manufacturer branch): 12 checks OK\n");

    printf("ALL CHECKS PASSED (%d total), g_unexpected_stub_calls=%d\n", checks, g_unexpected_stub_calls);
    assert(g_unexpected_stub_calls == 0);
    return 0;
}
