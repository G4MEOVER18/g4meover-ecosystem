// Host repro for the LF-RFID Keri decoder fix (lib/lfrfid/protocols/protocol_keri.c).
//
// Transcribes the ALT (my mntm-012 base) and NEU (Unleashed fix, commit 946a52a64,
// "merge some rfid related PRs", protocol_keri.c hunk of OFW PR 4449) decoder logic
// verbatim, plus the real bit_lib primitives they call, and drives both with a
// synthetic level/duration stream that models the documented physical defect: the
// PSK slicer reports one level's duration systematically long and the other
// systematically short by a fixed offset, because a Keri frame (29 zeros in its
// 33-bit preamble) is never DC balanced.
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
// Run:   ./repro

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

// ---------------------------------------------------------------------------
// bit_lib subset (lib/bit_lib/bit_lib.c), transcribed verbatim.
// ---------------------------------------------------------------------------

static void bit_lib_push_bit(uint8_t* data, size_t data_size, bool bit) {
    size_t last_index = data_size - 1;
    for(size_t i = 0; i < last_index; ++i) {
        data[i] = (uint8_t)((data[i] << 1) | ((data[i + 1] >> 7) & 1));
    }
    data[last_index] = (uint8_t)((data[last_index] << 1) | bit);
}

static bool bit_lib_get_bit(const uint8_t* data, size_t position) {
    return (data[position / 8] >> (7 - (position % 8))) & 1;
}

static void bit_lib_set_bit(uint8_t* data, size_t position, bool bit) {
    if(bit) {
        data[position / 8] |= 1UL << (7 - (position % 8));
    } else {
        data[position / 8] &= ~(1UL << (7 - (position % 8)));
    }
}

static uint8_t bit_lib_get_bits(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else {
        uint16_t value = (uint16_t)(data[position / 8] << 8 | data[position / 8 + 1]);
        value >>= (16 - shift - length);
        return (uint8_t)value;
    }
}

static uint32_t bit_lib_get_bits_32(const uint8_t* data, size_t position, uint8_t length) {
    uint32_t value = 0;
    if(length <= 8) {
        value = bit_lib_get_bits(data, position, length);
    } else if(length <= 16) {
        value = (uint32_t)bit_lib_get_bits(data, position, 8) << (length - 8);
        value |= bit_lib_get_bits(data, position + 8, (uint8_t)(length - 8));
    } else if(length <= 24) {
        value = (uint32_t)bit_lib_get_bits(data, position, 8) << (length - 8);
        value |= (uint32_t)bit_lib_get_bits(data, position + 8, 8) << (length - 16);
        value |= bit_lib_get_bits(data, position + 16, (uint8_t)(length - 16));
    } else {
        value = (uint32_t)bit_lib_get_bits(data, position, 8) << (length - 8);
        value |= (uint32_t)bit_lib_get_bits(data, position + 8, 8) << (length - 16);
        value |= (uint32_t)bit_lib_get_bits(data, position + 16, 8) << (length - 24);
        value |= bit_lib_get_bits(data, position + 24, (uint8_t)(length - 24));
    }
    return value;
}

// ---------------------------------------------------------------------------
// ALT: protocol_keri.c as it stands in my mntm-012 base (byte-identical to the
// Unleashed pre-946a52a64 state for this file).
// ---------------------------------------------------------------------------

#define KERI_PREAMBLE_BIT_SIZE_ALT (33)
#define KERI_PREAMBLE_DATA_SIZE_ALT (5)
#define KERI_ENCODED_BIT_SIZE_ALT (64)
#define KERI_ENCODED_DATA_SIZE_ALT (((KERI_ENCODED_BIT_SIZE_ALT) / 8) + KERI_PREAMBLE_DATA_SIZE_ALT)
#define KERI_US_PER_BIT (255)

typedef struct {
    uint8_t encoded_data[KERI_ENCODED_DATA_SIZE_ALT];
    uint8_t negative_encoded_data[KERI_ENCODED_DATA_SIZE_ALT];
    uint8_t corrupted_encoded_data[KERI_ENCODED_DATA_SIZE_ALT];
    uint8_t corrupted_negative_encoded_data[KERI_ENCODED_DATA_SIZE_ALT];
    uint8_t data[4];
} ProtocolKeriAlt;

static bool protocol_keri_check_preamble_alt(uint8_t* data, size_t bit_index) {
    if(*(uint32_t*)&data[bit_index / 8] != 0b00000000000000000000000011100000) return false;
    if(bit_lib_get_bit(data, bit_index + 32) != 1) return false;
    return true;
}

static bool protocol_keri_can_be_decoded_alt(uint8_t* data) {
    if(!protocol_keri_check_preamble_alt(data, 0)) return false;
    if(!protocol_keri_check_preamble_alt(data, 64)) return false;
    return true;
}

static bool
    protocol_keri_decoder_feed_internal_alt(bool polarity, uint32_t time, uint8_t* data) {
    time += (KERI_US_PER_BIT / 2);
    size_t bit_count = (time / KERI_US_PER_BIT);
    bool result = false;
    if(bit_count < KERI_ENCODED_BIT_SIZE_ALT) {
        for(size_t i = 0; i < bit_count; i++) {
            bit_lib_push_bit(data, KERI_ENCODED_DATA_SIZE_ALT, polarity);
            if(protocol_keri_can_be_decoded_alt(data)) {
                result = true;
                break;
            }
        }
    }
    return result;
}

static void protocol_keri_decoder_save_alt(uint8_t* data_to, const uint8_t* data_from) {
    uint32_t id = bit_lib_get_bits_32(data_from, 32, 32);
    data_to[3] = (uint8_t)id;
    data_to[2] = (uint8_t)(id >>= 8);
    data_to[1] = (uint8_t)(id >>= 8);
    data_to[0] = (uint8_t)(id >>= 8);
}

// Returns true on decode; *out_id receives the 32-bit id field of the buffer that matched.
static bool protocol_keri_decoder_feed_alt(ProtocolKeriAlt* p, bool level, uint32_t duration) {
    if(duration > (KERI_US_PER_BIT / 2)) {
        if(protocol_keri_decoder_feed_internal_alt(level, duration, p->encoded_data)) {
            protocol_keri_decoder_save_alt(p->data, p->encoded_data);
            return true;
        }
        if(protocol_keri_decoder_feed_internal_alt(!level, duration, p->negative_encoded_data)) {
            protocol_keri_decoder_save_alt(p->data, p->negative_encoded_data);
            return true;
        }
    }
    if(duration > (KERI_US_PER_BIT / 4)) {
        if(level) {
            duration += 120;
        } else {
            if(duration > 120) duration -= 120;
        }
        if(protocol_keri_decoder_feed_internal_alt(level, duration, p->corrupted_encoded_data)) {
            protocol_keri_decoder_save_alt(p->data, p->corrupted_encoded_data);
            return true;
        }
        if(protocol_keri_decoder_feed_internal_alt(
               !level, duration, p->corrupted_negative_encoded_data)) {
            protocol_keri_decoder_save_alt(p->data, p->corrupted_negative_encoded_data);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// NEU: protocol_keri.c after commit 946a52a64 (OFW PR 4449).
// ---------------------------------------------------------------------------

#define KERI_PREAMBLE_BIT_SIZE_NEU (33)
#define KERI_ENCODED_BIT_SIZE_NEU (64)
#define KERI_ENCODED_DATA_SIZE_NEU (((KERI_ENCODED_BIT_SIZE_NEU) * 2) / 8)
#define KERI_EDGE_SKEW_US (120)

typedef struct {
    uint8_t encoded_data[KERI_ENCODED_DATA_SIZE_NEU];
    uint8_t negative_encoded_data[KERI_ENCODED_DATA_SIZE_NEU];
    uint8_t high_long_encoded_data[KERI_ENCODED_DATA_SIZE_NEU];
    uint8_t high_long_negative_encoded_data[KERI_ENCODED_DATA_SIZE_NEU];
    uint8_t high_short_encoded_data[KERI_ENCODED_DATA_SIZE_NEU];
    uint8_t high_short_negative_encoded_data[KERI_ENCODED_DATA_SIZE_NEU];
    uint8_t data[4];
} ProtocolKeriNeu;

static bool protocol_keri_check_preamble_neu(uint8_t* data, size_t bit_index) {
    if(*(uint32_t*)&data[bit_index / 8] != 0b00000000000000000000000011100000) return false;
    if(bit_lib_get_bit(data, bit_index + 32) != 1) return false;
    return true;
}

static bool protocol_keri_can_be_decoded_neu(uint8_t* data) {
    if(!protocol_keri_check_preamble_neu(data, 0)) return false;
    if(!protocol_keri_check_preamble_neu(data, KERI_ENCODED_BIT_SIZE_NEU)) return false;
    if(bit_lib_get_bits_32(data, 32, 32) !=
       bit_lib_get_bits_32(data, KERI_ENCODED_BIT_SIZE_NEU + 32, 32))
        return false;
    return true;
}

static bool
    protocol_keri_decoder_feed_internal_neu(bool polarity, uint32_t time, uint8_t* data) {
    time += (KERI_US_PER_BIT / 2);
    size_t bit_count = (time / KERI_US_PER_BIT);
    bool result = false;
    if(bit_count < KERI_ENCODED_BIT_SIZE_NEU) {
        for(size_t i = 0; i < bit_count; i++) {
            bit_lib_push_bit(data, KERI_ENCODED_DATA_SIZE_NEU, polarity);
            if(protocol_keri_can_be_decoded_neu(data)) {
                result = true;
                break;
            }
        }
    }
    return result;
}

static void protocol_keri_decoder_save_neu(uint8_t* data_to, const uint8_t* data_from) {
    uint32_t id = bit_lib_get_bits_32(data_from, 32, 32);
    data_to[3] = (uint8_t)id;
    data_to[2] = (uint8_t)(id >>= 8);
    data_to[1] = (uint8_t)(id >>= 8);
    data_to[0] = (uint8_t)(id >>= 8);
}

static bool protocol_keri_decoder_feed_pair_neu(
    ProtocolKeriNeu* p,
    bool level,
    uint32_t duration,
    uint8_t* positive,
    uint8_t* negative) {
    if(protocol_keri_decoder_feed_internal_neu(level, duration, positive)) {
        protocol_keri_decoder_save_neu(p->data, positive);
        return true;
    }
    if(protocol_keri_decoder_feed_internal_neu(!level, duration, negative)) {
        protocol_keri_decoder_save_neu(p->data, negative);
        return true;
    }
    return false;
}

static bool protocol_keri_decoder_feed_neu(ProtocolKeriNeu* p, bool level, uint32_t duration) {
    if(duration > (KERI_US_PER_BIT / 2)) {
        if(protocol_keri_decoder_feed_pair_neu(
               p, level, duration, p->encoded_data, p->negative_encoded_data)) {
            return true;
        }
    }
    if(duration > (KERI_US_PER_BIT / 4)) {
        const uint32_t shortened =
            (duration > KERI_EDGE_SKEW_US) ? (duration - KERI_EDGE_SKEW_US) : duration;
        const uint32_t lengthened = duration + KERI_EDGE_SKEW_US;

        if(protocol_keri_decoder_feed_pair_neu(
               p,
               level,
               level ? shortened : lengthened,
               p->high_long_encoded_data,
               p->high_long_negative_encoded_data)) {
            return true;
        }
        if(protocol_keri_decoder_feed_pair_neu(
               p,
               level,
               level ? lengthened : shortened,
               p->high_short_encoded_data,
               p->high_short_negative_encoded_data)) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Test scaffolding: build a synthetic two-frame Keri transmission and RLE-encode
// it into (level, duration) pairs, optionally applying a level-dependent skew
// that models "one level reads long, the other short" per OFW PR 4449.
// ---------------------------------------------------------------------------

#define FRAME_BITS (64)
#define PREAMBLE_BYTE (0xE0) // 11100000, bits 0..7 of a frame

// Fills 'bits' (one byte per bit, 0/1) with 'frame_count' repeated Keri frames,
// each carrying the given 31-bit internal_id (bit 32 is always the fixed start bit).
static void build_frames(uint8_t* bits, size_t frame_count, uint32_t internal_id) {
    uint32_t id_field = 0x80000000u | (internal_id & 0x7FFFFFFFu);
    for(size_t f = 0; f < frame_count; f++) {
        uint8_t* fb = bits + f * FRAME_BITS;
        for(int i = 0; i < 8; i++) fb[i] = (PREAMBLE_BYTE >> (7 - i)) & 1;
        for(int i = 8; i < 32; i++) fb[i] = 0;
        for(int i = 0; i < 32; i++) fb[32 + i] = (id_field >> (31 - i)) & 1;
    }
}

typedef struct {
    bool level;
    uint32_t duration;
} Run;

// RLE-encodes 'bits' (n_bits entries of 0/1) into runs, applying a level-dependent
// bias (added to level==true runs, subtracted from level==false runs) to the
// nominal run_length * KERI_US_PER_BIT duration. Returns the run count.
static size_t rle_encode_biased(const uint8_t* bits, size_t n_bits, int32_t bias_true_us, Run* out) {
    size_t run_count = 0;
    size_t i = 0;
    while(i < n_bits) {
        bool level = bits[i];
        size_t run_len = 0;
        while(i < n_bits && bits[i] == level) {
            run_len++;
            i++;
        }
        int64_t nominal = (int64_t)run_len * KERI_US_PER_BIT;
        int64_t biased = level ? (nominal + bias_true_us) : (nominal - bias_true_us);
        if(biased < 1) biased = 1;
        out[run_count].level = level;
        out[run_count].duration = (uint32_t)biased;
        run_count++;
    }
    return run_count;
}

static uint32_t decode_result_to_id(const uint8_t data[4]) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) |
           data[3];
}

// Drives the ALT decoder across all runs; returns true iff it ever produces the
// correct id_field at any point in the stream. *any_success set if it returns
// true at all (even with a wrong id), *wrong_id_seen set if a wrong id ever came out.
static bool drive_alt(const Run* runs, size_t n, uint32_t expected_id_field, bool* any_success, bool* wrong_id_seen) {
    ProtocolKeriAlt p;
    memset(&p, 0, sizeof(p));
    bool ever_correct = false;
    *any_success = false;
    *wrong_id_seen = false;
    for(size_t i = 0; i < n; i++) {
        if(protocol_keri_decoder_feed_alt(&p, runs[i].level, runs[i].duration)) {
            *any_success = true;
            uint32_t got = decode_result_to_id(p.data);
            if(got == expected_id_field) {
                ever_correct = true;
            } else {
                *wrong_id_seen = true;
            }
        }
    }
    return ever_correct;
}

static bool drive_neu(const Run* runs, size_t n, uint32_t expected_id_field, bool* any_success, bool* wrong_id_seen) {
    ProtocolKeriNeu p;
    memset(&p, 0, sizeof(p));
    bool ever_correct = false;
    *any_success = false;
    *wrong_id_seen = false;
    for(size_t i = 0; i < n; i++) {
        if(protocol_keri_decoder_feed_neu(&p, runs[i].level, runs[i].duration)) {
            *any_success = true;
            uint32_t got = decode_result_to_id(p.data);
            if(got == expected_id_field) {
                ever_correct = true;
            } else {
                *wrong_id_seen = true;
            }
        }
    }
    return ever_correct;
}

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond, msg)                                               \
    do {                                                               \
        g_checks++;                                                    \
        if(!(cond)) {                                                  \
            g_failed++;                                                \
            fprintf(stderr, "CHECK FAILED (%s:%d): %s\n", __FILE__, __LINE__, msg); \
        }                                                               \
    } while(0)

int main(void) {
    const uint32_t internal_id = 0x3A5F1C2u; // arbitrary 31-bit test id
    const uint32_t id_field = 0x80000000u | (internal_id & 0x7FFFFFFFu);
    const size_t frame_count = 4; // repeated transmission, enough for any start alignment
    const size_t n_bits = frame_count * FRAME_BITS;

    uint8_t* bits = malloc(n_bits);
    assert(bits);
    build_frames(bits, frame_count, internal_id);

    Run* runs = malloc(sizeof(Run) * n_bits);
    assert(runs);

    // --- Group 1: zero bias (regression sanity, both should decode correctly) ---
    {
        size_t n = rle_encode_biased(bits, n_bits, 0, runs);
        bool alt_any, alt_wrong, neu_any, neu_wrong;
        bool alt_ok = drive_alt(runs, n, id_field, &alt_any, &alt_wrong);
        bool neu_ok = drive_neu(runs, n, id_field, &neu_any, &neu_wrong);
        CHECK(alt_ok, "zero bias: ALT should decode the correct id");
        CHECK(neu_ok, "zero bias: NEU should decode the correct id");
        CHECK(!alt_wrong, "zero bias: ALT should not report a wrong id");
        CHECK(!neu_wrong, "zero bias: NEU should not report a wrong id");
        printf(
            "[zero bias]      ALT correct=%d any=%d wrong=%d | NEU correct=%d any=%d wrong=%d\n",
            alt_ok, alt_any, alt_wrong, neu_ok, neu_any, neu_wrong);
    }

    // --- Group 2: level=true runs read long, level=false runs read short (as
    //     described in the OFW PR 4449 changelog note, ~135us) ---
    {
        size_t n = rle_encode_biased(bits, n_bits, 135, runs);
        bool alt_any, alt_wrong, neu_any, neu_wrong;
        bool alt_ok = drive_alt(runs, n, id_field, &alt_any, &alt_wrong);
        bool neu_ok = drive_neu(runs, n, id_field, &neu_any, &neu_wrong);
        CHECK(!alt_ok, "biased (true-long): ALT must NOT recover the correct id (documented defect)");
        CHECK(neu_ok, "biased (true-long): NEU must recover the correct id");
        printf(
            "[true-long bias]  ALT correct=%d any=%d wrong=%d | NEU correct=%d any=%d wrong=%d\n",
            alt_ok, alt_any, alt_wrong, neu_ok, neu_any, neu_wrong);
    }

    // --- Group 3: the opposite physical direction (level=false long, level=true
    //     short) -- NEU must still recover it since it tries both corrections. ---
    {
        size_t n = rle_encode_biased(bits, n_bits, -135, runs);
        bool alt_any, alt_wrong, neu_any, neu_wrong;
        bool alt_ok = drive_alt(runs, n, id_field, &alt_any, &alt_wrong);
        bool neu_ok = drive_neu(runs, n, id_field, &neu_any, &neu_wrong);
        CHECK(neu_ok, "biased (false-long): NEU must recover the correct id regardless of skew direction");
        printf(
            "[false-long bias] ALT correct=%d any=%d wrong=%d | NEU correct=%d any=%d wrong=%d\n",
            alt_ok, alt_any, alt_wrong, neu_ok, neu_any, neu_wrong);
    }

    free(bits);
    free(runs);

    // --- Group 4: direct unit check of the new cross-frame ID guard, bypassing
    //     the run/bias feed path. Two aligned frames whose id fields differ by
    //     a single bit (a marginal second-frame capture, no parity/checksum to
    //     catch it otherwise): NEU must reject, ALT structurally cannot even
    //     see the second frame's id field (its buffer is 104 bits, too small
    //     to reach bits[96:128)).
    {
        uint8_t buf_neu[KERI_ENCODED_DATA_SIZE_NEU];
        memset(buf_neu, 0, sizeof(buf_neu));
        for(int i = 0; i < 8; i++) bit_lib_set_bit(buf_neu, i, (PREAMBLE_BYTE >> (7 - i)) & 1);
        for(int i = 0; i < 32; i++) bit_lib_set_bit(buf_neu, 32 + i, (id_field >> (31 - i)) & 1);
        for(int i = 0; i < 8; i++)
            bit_lib_set_bit(buf_neu, FRAME_BITS + i, (PREAMBLE_BYTE >> (7 - i)) & 1);
        uint32_t mismatched_id_field = id_field ^ 0x00000001u; // single-bit corruption
        for(int i = 0; i < 32; i++)
            bit_lib_set_bit(buf_neu, FRAME_BITS + 32 + i, (mismatched_id_field >> (31 - i)) & 1);

        bool neu_rejects_mismatch = !protocol_keri_can_be_decoded_neu(buf_neu);
        CHECK(neu_rejects_mismatch, "NEU must reject two structurally valid frames with differing ids");

        // Now with matching ids, NEU must accept.
        for(int i = 0; i < 32; i++)
            bit_lib_set_bit(buf_neu, FRAME_BITS + 32 + i, (id_field >> (31 - i)) & 1);
        bool neu_accepts_match = protocol_keri_can_be_decoded_neu(buf_neu);
        CHECK(neu_accepts_match, "NEU must accept two structurally valid frames with matching ids");

        printf(
            "[cross-frame check] NEU rejects mismatch=%d, accepts match=%d "
            "(ALT has no buffer room to even see frame 2's id field: %d < %d bits)\n",
            neu_rejects_mismatch,
            neu_accepts_match,
            KERI_ENCODED_DATA_SIZE_ALT * 8,
            FRAME_BITS + 32 + 32);
    }

    printf("\n%d/%d checks passed\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
