/* Host repro for Phase 10 of the SubGhzBlockGenericGlobal override port: wires the
 * Phase-1-delivered counter/button override mechanism into KingGates Stylo4K
 * (kinggates_stylo_4k) -- the eighth protocol of the Nachzieh-Liste after Princeton
 * (Phase 3, button-only), Security+ 1.0 (Phase 4, counter + button), Somfy Telis
 * (Phase 5, counter + button), Phoenix V2 (Phase 6, counter + button), CAME Atomo
 * (Phase 7, counter + button, bypass), Alutech AT-4N (Phase 8, counter + button,
 * bypass) and Somfy Keytis (Phase 9, counter + button).
 *
 * Of the four remaining candidates after Phase 9 (FAAC SLH: 712 diff lines, Secplus
 * v2: 1133, Nice FloR-S: 1284, KingGates Stylo4K: 496 against
 * DarkFlippers/unleashed-firmware), KingGates Stylo4K had the smallest diff and was
 * taken up here.
 *
 * Unlike every protocol connected so far, my own
 * subghz_protocol_kinggates_stylo_4k_gen_data() does not build its frame fields
 * (hop/fix) from instance->generic.serial/btn/cnt directly -- it decrypts the
 * PREVIOUSLY LOADED hop (instance->generic.data_2) with the KeeLoq manufacturer key
 * to recover the old counter, bumps it, and re-encrypts; btn/serial are read out of
 * the previously loaded fix (instance->generic.data) and never fed back into it. A
 * counter override therefore already "just works" by forcing instance->generic.cnt
 * before the re-encrypt step (the existing pattern from every prior phase). A button
 * override does NOT: instance->generic.btn is only ever read FROM fix, and nothing
 * downstream rebuilds fix or the hop's cleartext from a new button value -- setting
 * instance->generic.btn alone (the Somfy Keytis/Phase 9 pattern) would silently
 * change the displayed/serialized button metadata while the actually transmitted
 * bits kept encoding the old one.
 *
 * So Phase 10's button-override hook, besides the override_get() call itself, also
 * rebuilds fix (bits [20:17] carry btn, matching the decoder's own
 * `instance->btn = (fix >> 17) & 0x0F` read) and the hop's 32-bit cleartext
 * (btn<<28 | 0x0C<<24 | (serial&0xFF)<<16 | cnt, matching the decoder's own
 * validation `(decrypt>>28)==btn && (decrypt>>24&0xF)==0xC && (decrypt>>16&0xFF)
 * ==serial&0xFF`) from the (possibly new) button plus the serial/cnt already in
 * hand -- both formulas are read directly off the existing, unmodified decode side
 * of this same file (subghz_protocol_kinggates_stylo_4k_remote_controller()), not
 * invented. This repro checks exactly that rebuild for correctness (round-trip
 * fix<->serial/btn, and hop-cleartext format matching the decoder's own acceptance
 * test), in addition to the override_get()/_set() plumbing every prior phase's repro
 * already checks.
 *
 * This does NOT compile the full, real kinggates_stylo_4k.c (it needs the KeeLoq
 * keystore iteration and the full upload-buffer generator, neither of which this
 * change touches, plus furi_hal_subghz_get_rolling_counter_mult() already covered by
 * every prior phase's repro). Instead it `#include`s the exact statements the patch
 * adds plus the pre-existing statements they are spliced into, extracted byte-for-
 * byte via `sed` from the shipped, patched
 * firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/kinggates_stylo_4k.c
 * (see the sed line ranges in the accompanying .md), and links them against the
 * real, unmodified Phase-1 generic.c -- the actual
 * subghz_block_generic_global_*_override_get()/_set() functions this patch calls.
 * Everything surrounding those statements in the real file (the KeeLoq keystore
 * loop, the Manchester-style framing, the upload buffer) is pre-existing, untouched
 * code and is not re-verified here.
 *
 * subghz_protocol_blocks_reverse_key() itself (lib/subghz/blocks/math.c) is not
 * part of this patch -- it is pre-existing, unmodified base code that
 * kinggates_stylo_4k.c already calls on both sides (encode and decode) before this
 * patch exists. It is not declared by generic.h, so it is vendored in here verbatim
 * (upstream/lib/subghz/blocks/math.c, an exact copy of
 * G4MEOVER18/G4MEOVER-FW@b359f6fc32f282ce7217dd2d48a4e3478771852e's file of the same
 * name, unmodified) purely so this host repro can link against the real function
 * instead of reimplementing it -- the fix/data rebuild this phase adds depends on
 * its actual bit order, so a reimplementation would risk re-deriving the same bug
 * twice rather than catching it.
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lib/subghz/blocks/generic.h"
#include "lib/subghz/blocks/math.h"

extern int g_unexpected_stub_calls;
int32_t g_rolling_counter_mult = 1;

#define TAG "SubGhzProtocoKingGatesStylo4k"

typedef struct {
    SubGhzBlockGeneric generic;
} MiniKingGatesInstance;

static int g_checks = 0;
#define CHECK(cond) \
    do { \
        g_checks++; \
        if(!(cond)) { \
            fprintf(stderr, "CHECK FAILED at line %d: %s\n", __LINE__, #cond); \
            return 1; \
        } \
    } while(0)

/* Decodes a 53-bit `fix` the same way the real, unmodified decode side of this file
   (subghz_protocol_kinggates_stylo_4k_remote_controller()) does -- used here only to
   verify Fragment 1's button-override rebuild round-trips correctly. Not part of the
   patch. */
static void decode_fix(uint64_t fix, uint32_t* serial_out, uint8_t* btn_out) {
    *btn_out = (fix >> 17) & 0x0F;
    *serial_out = (uint32_t)(((fix >> 5) & 0xFFFF0000) | (fix & 0xFFFF));
}

/* Fragment 1 (the patched counter-override leading branch plus the patched
   button-override block, from subghz_protocol_kinggates_stylo_4k_gen_data(), sed
   160,200p on the shipped kinggates_stylo_4k.c) is #include'd directly inside each
   scenario block below, right where the real function has it: after `fix` and
   `decrypt` are in scope (both already computed by the two M_EACH/decrypt loops
   above this point in the real file, left untouched here) and operating on a real
   MiniKingGatesInstance pointer named `instance` exactly as the real code operates
   on its own `instance->generic.*`. Verbatim -- including the three untouched
   sibling lines (the two counter branches, the btn/serial unpack) it is spliced
   around, reproduced here as control flow so the new code's placement is actually
   exercised, not just assumed. */

/* --- Fragment 2: the patched field-population block in
   subghz_protocol_decoder_kinggates_stylo_4k_get_string() (sed 605,612p on the
   shipped kinggates_stylo_4k.c). Verbatim. */
static void get_string_push_block(MiniKingGatesInstance* instance) {
#include "get_string_global_push_block.inc"
}

int main(void) {
    /* ---- Scenario A: no override pending -- regression check. Counter follows the
       untouched standard-mult path (same shape every prior phase's repro checks),
       and -- the part specific to this protocol -- `data`'s upper 16 bits stay
       exactly `decrypt & 0xFFFF0000` (the pre-existing formula), `fix` stays exactly
       the input `fix` (the button-override branch must not fire at all). */
    {
        subghz_block_generic_global_reset(NULL);
        MiniKingGatesInstance storage = {0};
        MiniKingGatesInstance* instance = &storage;
        instance->generic.cnt = 0x1234;
        g_rolling_counter_mult = 1;

        uint32_t serial_in = 0xAABBCC;
        uint8_t btn_in = 0x7;
        uint64_t fix_in =
            (((uint64_t)((serial_in >> 16) & 0xFFFF)) << 21) | ((uint64_t)btn_in << 17) |
            ((uint64_t)1 << 16) | (serial_in & 0xFFFF);
        uint32_t decrypt_in = ((uint32_t)btn_in << 28) | (0x0CUL << 24) |
                               ((serial_in & 0xFF) << 16) | 0x1234;

        uint64_t fix = fix_in;
        uint32_t decrypt = decrypt_in;
#include "button_and_counter_override_block.inc"
        /* cnt: no override pending -> standard mult path, +1 from 0x1234 */
        CHECK(instance->generic.cnt == 0x1235);
        /* btn/serial unpacked from fix_in, unchanged (no override) */
        CHECK(instance->generic.btn == btn_in);
        CHECK(instance->generic.serial == serial_in);
        /* fix untouched: button-override branch must not have fired */
        CHECK(fix == fix_in);
        /* data upper 16 bits untouched (decrypt_in's upper bits), lower 16 = new cnt */
        CHECK(data == ((decrypt_in & 0xFFFF0000) | instance->generic.cnt));
    }

    /* ---- Scenario B: counter override pending only. cnt takes the override value
       verbatim (masked to cnt_length_bit, matching every prior phase), OFEX/mult
       chain skipped entirely; fix/button side untouched (no button override). */
    {
        subghz_block_generic_global_reset(NULL);
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global_counter_override_set(0xBEEF1234); /* masked to 16 bit */

        MiniKingGatesInstance storage = {0};
        MiniKingGatesInstance* instance = &storage;
        instance->generic.cnt = 0x1234; /* must be fully replaced, not incremented */
        g_rolling_counter_mult = 1;

        uint32_t serial_in = 0x00112233;
        uint8_t btn_in = 0xB;
        uint64_t fix_in =
            (((uint64_t)((serial_in >> 16) & 0xFFFF)) << 21) | ((uint64_t)btn_in << 17) |
            ((uint64_t)1 << 16) | (serial_in & 0xFFFF);
        uint32_t decrypt_in = ((uint32_t)btn_in << 28) | (0x0CUL << 24) |
                               ((serial_in & 0xFF) << 16) | 0x1234;

        uint64_t fix = fix_in;
        uint32_t decrypt = decrypt_in;
#include "button_and_counter_override_block.inc"
        CHECK(instance->generic.cnt == 0x1234); /* 0xBEEF1234 & 0xFFFF */
        CHECK(instance->generic.btn == btn_in);
        CHECK(instance->generic.serial == serial_in);
        CHECK(fix == fix_in);
        CHECK(data == ((decrypt_in & 0xFFFF0000) | instance->generic.cnt));
        /* override flag was one-shot-consumed */
        CHECK(subghz_block_generic_global.cnt_need_override == false);
    }

    /* ---- Scenario C: button override pending only -- the protocol-specific part of
       this phase. fix/data must be REBUILT from the new button (not just
       instance->generic.btn silently relabeled), and the rebuild must round-trip:
       decoding the new fix must yield the override button and the UNCHANGED serial;
       the new data's upper 16 bits must match the decoder's own acceptance format
       for that button/serial pair. */
    {
        subghz_block_generic_global_reset(NULL);
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0xD); /* masked to 4 bit: 0xD */

        MiniKingGatesInstance storage = {0};
        MiniKingGatesInstance* instance = &storage;
        instance->generic.cnt = 0x5678;
        g_rolling_counter_mult = 1;

        uint32_t serial_in = 0x00445566;
        uint8_t btn_in = 0xE; /* original button, must end up replaced by 0xD */
        uint64_t fix_in =
            (((uint64_t)((serial_in >> 16) & 0xFFFF)) << 21) | ((uint64_t)btn_in << 17) |
            ((uint64_t)1 << 16) | (serial_in & 0xFFFF);
        uint32_t decrypt_in = ((uint32_t)btn_in << 28) | (0x0CUL << 24) |
                               ((serial_in & 0xFF) << 16) | 0x5678;

        uint64_t fix = fix_in;
        uint32_t decrypt = decrypt_in;
#include "button_and_counter_override_block.inc"
        CHECK(instance->generic.btn == 0xD); /* overridden */
        CHECK(instance->generic.serial == serial_in); /* serial must survive the rebuild */
        CHECK(fix != fix_in); /* fix must actually have been rebuilt */

        uint32_t decoded_serial;
        uint8_t decoded_btn;
        decode_fix(fix, &decoded_serial, &decoded_btn);
        CHECK(decoded_btn == 0xD);
        CHECK(decoded_serial == serial_in);
        /* also check against the real, linked subghz_protocol_blocks_reverse_key(),
           not just the hand decode formula, to rule out a 53-bit truncation
           slipping past the two parallel (but structurally similar) bit layouts
           above -- reversing twice must return the original low 53 bits */
        uint64_t fix_reversed_twice =
            subghz_protocol_blocks_reverse_key(subghz_protocol_blocks_reverse_key(fix, 53), 53);
        CHECK(fix_reversed_twice == (fix & ((((uint64_t)1) << 53) - 1)));

        /* data's upper 16 bits must match the decoder's own acceptance test format:
           (decrypt>>28)==btn && (decrypt>>24&0xF)==0xC && (decrypt>>16&0xFF)==
           serial&0xFF -- i.e. exactly what remote_controller() checks on receive */
        CHECK(((data >> 28) & 0xF) == 0xD);
        CHECK(((data >> 24) & 0xF) == 0xC);
        CHECK(((data >> 16) & 0xFF) == (serial_in & 0xFF));
        /* lower 16 bits of data remain the counter, untouched by the button branch */
        CHECK((data & 0xFFFF) == instance->generic.cnt);

        CHECK(subghz_block_generic_global.btn_need_override == false);
    }

    /* ---- Scenario D: both overrides pending simultaneously. */
    {
        subghz_block_generic_global_reset(NULL);
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_counter_override_set(0x0042);
        subghz_block_generic_global_button_override_set(0x7);

        MiniKingGatesInstance storage = {0};
        MiniKingGatesInstance* instance = &storage;
        instance->generic.cnt = 0x9999;
        g_rolling_counter_mult = 1;

        uint32_t serial_in = 0x00778899;
        uint8_t btn_in = 0xF;
        uint64_t fix_in =
            (((uint64_t)((serial_in >> 16) & 0xFFFF)) << 21) | ((uint64_t)btn_in << 17) |
            ((uint64_t)1 << 16) | (serial_in & 0xFFFF);
        uint32_t decrypt_in = ((uint32_t)btn_in << 28) | (0x0CUL << 24) |
                               ((serial_in & 0xFF) << 16) | 0x9999;

        uint64_t fix = fix_in;
        uint32_t decrypt = decrypt_in;
#include "button_and_counter_override_block.inc"
        CHECK(instance->generic.cnt == 0x0042);
        CHECK(instance->generic.btn == 0x7);
        CHECK(instance->generic.serial == serial_in);

        uint32_t decoded_serial;
        uint8_t decoded_btn;
        decode_fix(fix, &decoded_serial, &decoded_btn);
        CHECK(decoded_btn == 0x7);
        CHECK(decoded_serial == serial_in);
        CHECK(((data >> 28) & 0xF) == 0x7);
        CHECK(((data >> 24) & 0xF) == 0xC);
        CHECK(((data >> 16) & 0xFF) == (serial_in & 0xFF));
        CHECK((data & 0xFFFF) == 0x0042); /* counter override value, not 0x9999 */
    }

    /* ---- Scenario E: button masking. new_btn above 4 bits must be masked down
       (0xFF >> (8-4) == 0x0F), matching button_override_get()'s own masking that
       every prior phase's repro also exercises against generic.c directly; checked
       here end-to-end through the actual rebuild, not just the bare getter. */
    {
        subghz_block_generic_global_reset(NULL);
        subghz_block_generic_global.btn_length_bit = 4;
        subghz_block_generic_global_button_override_set(0xFE); /* -> masked to 0xE */

        MiniKingGatesInstance storage = {0};
        MiniKingGatesInstance* instance = &storage;
        instance->generic.cnt = 0x0001;
        g_rolling_counter_mult = 1;

        uint32_t serial_in = 0x00112200;
        uint8_t btn_in = 0x3;
        uint64_t fix_in =
            (((uint64_t)((serial_in >> 16) & 0xFFFF)) << 21) | ((uint64_t)btn_in << 17) |
            ((uint64_t)1 << 16) | (serial_in & 0xFFFF);
        uint32_t decrypt_in = ((uint32_t)btn_in << 28) | (0x0CUL << 24) |
                               ((serial_in & 0xFF) << 16) | 0x0001;

        uint64_t fix = fix_in;
        uint32_t decrypt = decrypt_in;
#include "button_and_counter_override_block.inc"
        CHECK(instance->generic.btn == 0xE);
        /* the masked override must also have reached the rebuilt hop cleartext,
           not just the displayed field */
        CHECK(((data >> 28) & 0xF) == 0xE);
    }

    /* ---- Fragment 2: get_string push block, same shape every prior phase's repro
       checks -- the global struct must end up mirroring whatever the decoder's
       instance currently holds, with the fixed bit-lengths this protocol uses. */
    {
        subghz_block_generic_global_reset(NULL);
        MiniKingGatesInstance storage = {0};
        storage.generic.cnt = 0xCAFE;
        storage.generic.btn = 0x9;

        get_string_push_block(&storage);

        CHECK(subghz_block_generic_global.cnt_is_available == true);
        CHECK(subghz_block_generic_global.cnt_length_bit == 16);
        CHECK(subghz_block_generic_global.current_cnt == 0xCAFE);
        CHECK(subghz_block_generic_global.btn_is_available == true);
        CHECK(subghz_block_generic_global.current_btn == 0x9);
        CHECK(subghz_block_generic_global.btn_length_bit == 4);
    }

    CHECK(g_unexpected_stub_calls == 0);

    printf("ALL CHECKS PASSED (%d total)\n", g_checks);
    return 0;
}
