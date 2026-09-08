// Host repro for the Kia V2 decoder deserialize field-restore bug.
//
// Models only the parts needed to demonstrate the bug: a stand-in for the
// stock <lib/subghz/blocks/generic.h> helper
// subghz_block_generic_deserialize_check_count_bit(), which (per the same
// contract already verified in the fiat_v0/fiat_v1 repros in this
// directory, and used unmodified across ~20 decoders in this same
// protocol set) populates ONLY generic.data and generic.data_count_bit
// from the "Key"/"Bit" fields of a saved capture file -- it never touches
// generic.serial/btn/cnt.
//
// Build:
//   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t data;
    uint32_t data_count_bit;
    uint32_t serial;
    uint8_t btn;
    uint16_t cnt;
} SubGhzBlockGeneric;

// Stand-in for a loaded .sub file: only the fields kia_v2 actually writes
// in kia_protocol_decoder_v2_serialize() ("Key" via the generic serializer,
// "Bit" likewise). Serial/Btn/Cnt/CRC/RawCnt are also written by
// serialize() but -- this is the bug -- never read back by deserialize().
typedef struct {
    uint64_t key; // "Key" field: instance->generic.data, 53 bits used
    uint32_t bit; // "Bit" field
} FakeFile;

// Faithful stand-in for the real, stock
// subghz_block_generic_deserialize_check_count_bit(): reads "Bit" into
// data_count_bit and "Key" into data, checks the bit count against
// min_count_bit_for_found, and does NOTHING else.
static int mock_generic_deserialize_check_count_bit(
    SubGhzBlockGeneric* generic,
    const FakeFile* file,
    uint32_t min_count_bit_for_found) {
    generic->data_count_bit = file->bit;
    generic->data = file->key;
    if(generic->data_count_bit != min_count_bit_for_found) {
        return 0; // SubGhzProtocolStatusError-ish
    }
    return 1; // SubGhzProtocolStatusOk
}

typedef struct {
    SubGhzBlockGeneric generic;
} DecoderKiaV2;

#define KIA_V2_MIN_COUNT_BIT 53

// ALT: kia_protocol_decoder_v2_deserialize() before the fix -- exactly the
// one-line passthrough present in protocols/kia_v2.c today.
static int deserialize_before(DecoderKiaV2* instance, const FakeFile* file) {
    memset(instance, 0, sizeof(*instance)); // matches malloc()+zero-init in _alloc()
    return mock_generic_deserialize_check_count_bit(
        &instance->generic, file, KIA_V2_MIN_COUNT_BIT);
}

// NEW: same generic-helper call, plus the added post-processing that
// restores generic.serial/btn/cnt -- copied verbatim from the bit layout
// already used on the live-decode path in
// kia_protocol_decoder_v2_feed() (protocols/kia_v2.c, decode_count_bit == 53
// branch).
static int deserialize_after(DecoderKiaV2* instance, const FakeFile* file) {
    memset(instance, 0, sizeof(*instance));
    if(!mock_generic_deserialize_check_count_bit(&instance->generic, file, KIA_V2_MIN_COUNT_BIT))
        return 0;

    instance->generic.serial = (uint32_t)((instance->generic.data >> 20) & 0xFFFFFFFFULL);
    instance->generic.btn = (uint8_t)((instance->generic.data >> 16) & 0x0F);

    uint16_t raw_count = (uint16_t)((instance->generic.data >> 4) & 0xFFF);
    instance->generic.cnt = ((raw_count >> 4) | (raw_count << 8)) & 0xFFF;
    return 1;
}

// Mirrors the fields actually printed by kia_protocol_decoder_v2_get_string():
// Key (from generic.data, unaffected by the bug), Sn/Btn/Cnt (from
// generic.serial/btn/cnt, the fields the bug leaves at 0).
static void print_get_string(const char* label, const DecoderKiaV2* instance) {
    printf(
        "%-6s Key:%013llX Sn:%08X Btn:%X Cnt:%03X\n",
        label,
        (unsigned long long)instance->generic.data,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}

// Builds a 53-bit "Key" the same way kia_protocol_decoder_v2_feed() would
// have produced it from a live decode, given target serial/btn/cnt --
// inverse of the reconstruction under test, so the round-trip is checked
// against independently-derived expected values, not against itself.
static uint64_t make_key(uint32_t serial, uint8_t btn, uint16_t cnt12) {
    uint64_t data = 0;
    data |= ((uint64_t)serial << 20) & 0xFFFFFFFFF00000ULL;
    data |= ((uint64_t)(btn & 0x0F)) << 16;
    // raw_count is the on-wire nibble-swapped form of cnt12 that
    // kia_protocol_decoder_v2_feed() unswaps via
    // ((raw_count >> 4) | (raw_count << 8)) & 0xFFF -- build raw_count as
    // its inverse so make_key()/feed()'s reconstruction round-trip exactly.
    uint16_t raw_count = ((cnt12 << 4) | (cnt12 >> 8)) & 0xFFF;
    data |= ((uint64_t)raw_count) << 4;
    return data;
}

int main(void) {
    struct {
        const char* name;
        uint32_t serial;
        uint8_t btn;
        uint16_t cnt12;
    } cases[] = {
        {"typical capture", 0x0012AB34, 0x5, 0x0A7},
        {"zero serial/btn/cnt (indistinguishable from the bug's own output)", 0, 0, 0},
        {"max fields", 0xFFFFFFFF, 0xF, 0xFFF},
    };

    int failures = 0;
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FakeFile file = {
            .key = make_key(cases[i].serial, cases[i].btn, cases[i].cnt12),
            .bit = KIA_V2_MIN_COUNT_BIT,
        };

        DecoderKiaV2 before, after;
        int ok_before = deserialize_before(&before, &file);
        int ok_after = deserialize_after(&after, &file);

        printf("-- case %zu: %s --\n", i, cases[i].name);
        printf(
            "  saved:  serial=%08X btn=%X cnt=%03X\n",
            cases[i].serial,
            cases[i].btn,
            cases[i].cnt12);
        print_get_string("ALT", &before);
        print_get_string("NEW", &after);

        if(!ok_before || !ok_after) {
            printf("  UNEXPECTED deserialize failure\n");
            failures++;
            continue;
        }

        // generic.data itself (unaffected by the bug) must already be
        // correct even in ALT -- only serial/btn/cnt are lost.
        if(before.generic.data != file.key) {
            printf("  FAIL: even generic.data wrong in ALT (unexpected)\n");
            failures++;
        }
        if(after.generic.serial != cases[i].serial || after.generic.btn != cases[i].btn ||
           after.generic.cnt != cases[i].cnt12) {
            printf("  FAIL: NEW did not round-trip serial/btn/cnt\n");
            failures++;
        }
        int alt_matches_saved = (before.generic.serial == cases[i].serial &&
                                  before.generic.btn == cases[i].btn &&
                                  before.generic.cnt == cases[i].cnt12);
        int alt_should_match = (cases[i].serial == 0 && cases[i].btn == 0 && cases[i].cnt12 == 0);
        if(alt_matches_saved != alt_should_match) {
            printf("  FAIL: bug-presence expectation violated for ALT\n");
            failures++;
        }
    }

    if(failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed: ALT loses serial/btn/cnt on every non-zero "
           "capture, NEW round-trips exactly.\n");
    return 0;
}
