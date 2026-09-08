// Host repro for the Fiat V0 decoder deserialize field-restore bug.
//
// Models only the parts needed to demonstrate the bug: a stand-in for the
// stock <lib/subghz/blocks/generic.h> helper
// subghz_block_generic_deserialize_check_count_bit(), which is well known
// (and used unmodified across ~20 other decoders in this same protocol
// set) to populate ONLY generic.data and generic.data_count_bit from the
// "Key"/"Bit" fields of a saved capture file -- it never touches
// protocol-specific struct fields such as fiat_v0's own hop/fix/endbyte.
//
// Build:
//   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Everything below is a faithful, standalone reimplementation of the
// relevant control flow from protocols/fiat_v0.c; no Flipper headers
// required.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t data;
    uint32_t data_count_bit;
    uint32_t serial;
    uint32_t btn;
    uint32_t cnt;
} SubGhzBlockGeneric;

// Stand-in for a loaded .sub file: only the fields fiat_v0 actually writes
// in subghz_protocol_decoder_fiat_v0_serialize().
typedef struct {
    uint64_t key; // "Key" field: hop(32) << 32 | fix(32)
    uint32_t bit; // "Bit" field
    uint32_t end_byte; // "EndByte" field, as written by serialize()
    int has_end_byte; // serialize() always writes it, but model both cases
} FakeFile;

// Faithful stand-in for the real, stock
// subghz_block_generic_deserialize_check_count_bit(): reads "Bit" into
// data_count_bit and "Key" into data, checks the bit count, and does
// NOTHING else -- no Serial/Btn/Cnt/EndByte handling, since those are not
// part of the base SubGhzBlockGeneric contract.
static int mock_generic_deserialize_check_count_bit(
    SubGhzBlockGeneric* generic,
    const FakeFile* file,
    uint32_t min_count_bit_for_found) {
    generic->data_count_bit = file->bit;
    generic->data = file->key;
    if(generic->data_count_bit != min_count_bit_for_found &&
       generic->data_count_bit != 71) {
        return 0; // SubGhzProtocolStatusError-ish
    }
    return 1; // SubGhzProtocolStatusOk
}

typedef struct {
    SubGhzBlockGeneric generic;
    uint32_t hop;
    uint32_t fix;
    uint8_t endbyte;
} DecoderFiatV0;

// ALT: subghz_protocol_decoder_fiat_v0_deserialize() before the fix --
// exactly the one-line passthrough present in protocols/fiat_v0.c today.
static int deserialize_before(DecoderFiatV0* instance, const FakeFile* file) {
    memset(instance, 0, sizeof(*instance)); // matches calloc() in _alloc()
    return mock_generic_deserialize_check_count_bit(&instance->generic, file, 64);
}

// NEW: same generic-helper call, plus the added post-processing that
// restores hop/fix/endbyte (and keeps generic.serial/btn/cnt in sync,
// mirroring what fiat_v0_finish_packet() already does on a live decode).
static int deserialize_after(DecoderFiatV0* instance, const FakeFile* file) {
    memset(instance, 0, sizeof(*instance));
    if(!mock_generic_deserialize_check_count_bit(&instance->generic, file, 64)) return 0;

    instance->hop = (uint32_t)(instance->generic.data >> 32);
    instance->fix = (uint32_t)(instance->generic.data & 0xFFFFFFFFU);

    uint32_t endbyte_val = 0;
    int have_endbyte = file->has_end_byte;
    if(have_endbyte) {
        endbyte_val = file->end_byte;
    }
    // (fallback-to-"Btn" path omitted here: no test vector below exercises
    // a file missing "EndByte", since fiat_v0's own serialize() always
    // writes it -- see the .md writeup for the fallback rationale.)
    instance->endbyte = (uint8_t)(endbyte_val & 0x7FU);

    instance->generic.serial = instance->fix;
    instance->generic.btn = instance->endbyte;
    instance->generic.cnt = instance->hop;
    return 1;
}

// Mirrors the fields actually printed by
// subghz_protocol_decoder_fiat_v0_get_string(): Key/Hop/Sn/EndByte, all
// taken from instance->hop/fix/endbyte, never from instance->generic.*.
static void print_get_string(const char* label, const DecoderFiatV0* instance) {
    printf(
        "%-6s Key:%08X%08X Hop:%08X Sn:%08X EndByte:%02X\n",
        label,
        instance->hop,
        instance->fix,
        instance->hop,
        instance->fix,
        instance->endbyte & 0x3F);
}

int main(void) {
    struct {
        const char* name;
        uint32_t hop;
        uint32_t fix;
        uint8_t end_byte;
    } cases[] = {
        {"typical capture", 0x0001A2B3, 0xDEADBEEF, 0x2A},
        {"zero hop/fix (would be indistinguishable from the bug's own output)",
         0x00000000,
         0x00000000,
         0x00},
        {"max fields", 0xFFFFFFFF, 0xFFFFFFFF, 0x7F},
    };

    int failures = 0;
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FakeFile file = {
            .key = ((uint64_t)cases[i].hop << 32) | cases[i].fix,
            .bit = 71,
            .end_byte = cases[i].end_byte,
            .has_end_byte = 1,
        };

        DecoderFiatV0 before, after;
        int ok_before = deserialize_before(&before, &file);
        int ok_after = deserialize_after(&after, &file);

        printf("-- case %zu: %s --\n", i, cases[i].name);
        printf(
            "  saved:  hop=%08X fix=%08X endbyte=%02X\n",
            cases[i].hop,
            cases[i].fix,
            cases[i].end_byte);
        print_get_string("ALT", &before);
        print_get_string("NEW", &after);

        if(!ok_before || !ok_after) {
            printf("  UNEXPECTED deserialize failure\n");
            failures++;
            continue;
        }

        // ALT is expected to be WRONG (all-zero) whenever the saved values
        // are non-zero -- that's the bug. Only assert the correctness we
        // actually want: NEW must always round-trip exactly, and
        // generic.data itself (unaffected by the bug) must already be
        // correct even in ALT.
        if(before.generic.data != file.key) {
            printf("  FAIL: even generic.data wrong in ALT (unexpected)\n");
            failures++;
        }
        if(after.hop != cases[i].hop || after.fix != cases[i].fix ||
           after.endbyte != (cases[i].end_byte & 0x7F)) {
            printf("  FAIL: NEW did not round-trip hop/fix/endbyte\n");
            failures++;
        }
        int alt_matches_saved = (before.hop == cases[i].hop && before.fix == cases[i].fix &&
                                  before.endbyte == (cases[i].end_byte & 0x7F));
        int alt_should_match = (cases[i].hop == 0 && cases[i].fix == 0 && cases[i].end_byte == 0);
        if(alt_matches_saved != alt_should_match) {
            printf("  FAIL: bug-presence expectation violated for ALT\n");
            failures++;
        }
    }

    if(failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed: ALT loses hop/fix/endbyte on every non-zero "
           "capture, NEW round-trips exactly.\n");
    return 0;
}
