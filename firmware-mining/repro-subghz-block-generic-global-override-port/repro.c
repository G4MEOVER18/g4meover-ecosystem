/* Host repro for lib/subghz/blocks/generic.c's counter/button override API
   (SubGhzBlockGenericGlobal) -- the mechanism this port proposal brings over
   from Unleashed, merged additively into my own generic.c/.h (the copies
   compiled here are the actual post-merge files from this proposal's
   lib/subghz/blocks/, i.e. my own Lat/Lon serialize_common() stays
   byte-for-byte as-is, see the port-proposal doc's "Verifikation" section
   for the connective-patches diff against a pristine clone). Only the
   file's unrelated dependencies (FuriString, FlipperFormat, Stream) are
   mocked, and this repro never calls into the mocked paths (see the
   g_unexpected_stub_calls assertion at the end).

   What is verified against the real, documented contract:
   1. counter/button override is "consumed once": after a _set(), the next
      _get() returns true and the override value; a second _get() without
      an intervening _set() returns false and leaves the output untouched.
   2. the masking formula in _get() -- cutting the override value down to
      the protocol's declared bit length -- is checked against every
      cnt_length_bit/btn_length_bit value actually used by a real protocol
      in Unleashed's lib/subghz/protocols/ (various .c files) (8/16/20/28/32 for counters;
      1/2/3/4/8 for buttons -- grepped from the upstream tree, see the
      port-proposal doc's "Verifikation" section for the exact list).
   3. _reset() zeroes every field except endless_tx, which survives -- the
      one explicitly commented, order-sensitive exception in the function.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "lib/subghz/blocks/generic.h"

extern int g_unexpected_stub_calls;

static int checks_passed = 0;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            exit(1); \
        } \
        checks_passed++; \
    } while(0)

static void reset_global(void) {
    memset(&subghz_block_generic_global, 0, sizeof(subghz_block_generic_global));
}

/* Real cnt_length_bit / btn_length_bit values set by real protocols in
   Unleashed's lib/subghz/protocols/ (various .c files) right before they call
   subghz_block_generic_global_counter_override_get() /
   _button_override_get() (keeloq.c, faac_slh.c, secplus_v1.c, secplus_v2.c,
   alutech_at_4n.c, beninca_arc.c, jarolift.c, prastel.c, ditec_gol4.c,
   phoenix_v2.c, nice_flor_s.c, came_atomo.c, kinggates_stylo_4k.c,
   somfy_keytis.c, somfy_telis.c). */
static const uint8_t real_cnt_length_bits[] = {8, 16, 20, 28, 32};
static const uint8_t real_btn_length_bits[] = {1, 2, 3, 4, 8};

static void test_counter_override_masking(void) {
    for(size_t i = 0; i < sizeof(real_cnt_length_bits); i++) {
        uint8_t bits = real_cnt_length_bits[i];
        reset_global();
        subghz_block_generic_global.cnt_length_bit = bits;

        /* no override pending yet -> _get must return false and must not
           touch the output variable */
        uint32_t cnt = 0xAAAAAAAAu;
        bool got = subghz_block_generic_global_counter_override_get(&cnt);
        CHECK(got == false);
        CHECK(cnt == 0xAAAAAAAAu);

        uint32_t expected_mask = (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);

        subghz_block_generic_global_counter_override_set(0xFFFFFFFFu);
        CHECK(subghz_block_generic_global.cnt_need_override == true);

        cnt = 0;
        got = subghz_block_generic_global_counter_override_get(&cnt);
        CHECK(got == true);
        CHECK(cnt == expected_mask);
        /* consumed once: flag cleared, second call without a new _set()
           returns false and leaves cnt untouched */
        CHECK(subghz_block_generic_global.cnt_need_override == false);
        cnt = 0x12345678u;
        got = subghz_block_generic_global_counter_override_get(&cnt);
        CHECK(got == false);
        CHECK(cnt == 0x12345678u);

        /* a value that already fits the declared length must survive
           untouched (no spurious masking of in-range values) */
        reset_global();
        subghz_block_generic_global.cnt_length_bit = bits;
        uint32_t in_range = expected_mask; /* all-ones already in range */
        subghz_block_generic_global_counter_override_set(in_range);
        cnt = 0;
        CHECK(subghz_block_generic_global_counter_override_get(&cnt) == true);
        CHECK(cnt == in_range);
    }
}

static void test_button_override_masking(void) {
    for(size_t i = 0; i < sizeof(real_btn_length_bits); i++) {
        uint8_t bits = real_btn_length_bits[i];
        reset_global();
        subghz_block_generic_global.btn_length_bit = bits;

        uint8_t btn = 0xAA;
        bool got = subghz_block_generic_global_button_override_get(&btn);
        CHECK(got == false);
        CHECK(btn == 0xAA);

        uint8_t expected_mask = (uint8_t)((1u << bits) - 1u);

        subghz_block_generic_global_button_override_set(0xFFu);
        btn = 0;
        got = subghz_block_generic_global_button_override_get(&btn);
        CHECK(got == true);
        CHECK(btn == expected_mask);
        CHECK(subghz_block_generic_global.btn_need_override == false);

        btn = 0x5A;
        got = subghz_block_generic_global_button_override_get(&btn);
        CHECK(got == false);
        CHECK(btn == 0x5A);
    }
}

static void test_reset_preserves_endless_tx(void) {
    reset_global();
    subghz_block_generic_global.endless_tx = true;
    subghz_block_generic_global.cnt_length_bit = 16;
    subghz_block_generic_global.btn_length_bit = 4;
    subghz_block_generic_global.cnt_is_available = true;
    subghz_block_generic_global.btn_is_available = true;
    subghz_block_generic_global.current_cnt = 1234;
    subghz_block_generic_global.current_btn = 2;
    subghz_block_generic_global_counter_override_set(999);
    subghz_block_generic_global_button_override_set(3);

    subghz_block_generic_global_reset(NULL);

    CHECK(subghz_block_generic_global.endless_tx == true);
    CHECK(subghz_block_generic_global.cnt_length_bit == 0);
    CHECK(subghz_block_generic_global.btn_length_bit == 0);
    CHECK(subghz_block_generic_global.cnt_is_available == false);
    CHECK(subghz_block_generic_global.btn_is_available == false);
    CHECK(subghz_block_generic_global.current_cnt == 0);
    CHECK(subghz_block_generic_global.current_btn == 0);
    CHECK(subghz_block_generic_global.cnt_need_override == false);
    CHECK(subghz_block_generic_global.btn_need_override == false);

    /* and the other direction: starting with endless_tx == false, reset
       must leave it false (not flip it on as a side effect) */
    reset_global();
    subghz_block_generic_global.endless_tx = false;
    subghz_block_generic_global.cnt_length_bit = 16;
    subghz_block_generic_global_reset(NULL);
    CHECK(subghz_block_generic_global.endless_tx == false);
    CHECK(subghz_block_generic_global.cnt_length_bit == 0);
}

static void test_no_override_pending_after_fresh_reset(void) {
    /* straight after a reset (the real call site: base.c's
       subghz_protocol_decoder_base_get_string() hook, see the port-proposal
       doc), neither override must be reported as pending. */
    reset_global();
    subghz_block_generic_global.cnt_length_bit = 32;
    subghz_block_generic_global.btn_length_bit = 8;
    subghz_block_generic_global_reset(NULL);
    uint32_t cnt = 777;
    uint8_t btn = 77;
    CHECK(subghz_block_generic_global_counter_override_get(&cnt) == false);
    CHECK(cnt == 777);
    CHECK(subghz_block_generic_global_button_override_get(&btn) == false);
    CHECK(btn == 77);
}

int main(void) {
    test_counter_override_masking();
    test_button_override_masking();
    test_reset_preserves_endless_tx();
    test_no_override_pending_after_fresh_reset();

    CHECK(g_unexpected_stub_calls == 0);

    printf("%d Pruefungen bestanden, 0 Aufrufe in die gemockten "
           "FuriString/FlipperFormat/Stream-Pfade (nur die vier reinen "
           "Override-/Reset-Funktionen wurden ausgeuebt)\n",
           checks_passed);
    return 0;
}
