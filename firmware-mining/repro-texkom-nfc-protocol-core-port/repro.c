// Host repro for the Texkom NFC protocol-core port (mntm-012, API 87.1).
// Transcribes the exact logic of the ported lib/nfc/protocols/texkom/texkom.c
// (CRC-8 variants, frame pack/preamble check, the three interval decoders
// TK13/TK15/TK17, the type/coding consistency check, make_blank, get/set_uid)
// without any furi/FlipperFormat headers. furi_check() -> assert(); the one
// external helper the file uses (maxim_crc8(), from lib/one_wire/maxim_crc.c)
// is copied verbatim below -- confirmed byte-identical between this fork and
// Unleashed dev beforehand, so copying it here changes nothing about what is
// being tested.
//
// texkom_load()/texkom_save() (plain pass-through to flipper_format_read_*/
// write_* -- the same helpers every other already-ported NFC protocol in this
// codebase uses the same way) are not re-tested here: they carry no decoding
// logic of their own, only the numeric/bit logic below does.
//
// Scenarios:
//  1. Round trip TK13 via the public dispatch (texkom_decode_intervals) with
//     widely separated gap lengths: make_blank+set_uid builds a frame, the
//     frame's bits are re-encoded into 127 synthetic TK13-coding intervals,
//     decoded back, and compared byte-for-byte against the original. Exercises
//     the threshold-based decode_tk13 path end to end, including its orphan
//     last-bit reconstruction (127 intervals is always odd).
//  2. Same frame, but with narrow, overlapping gap lengths on half the bit
//     positions so the single global hi/low threshold in decode_tk13
//     misclassifies into an equal raw pair and decode_tk13 must return false --
//     decode_intervals must then fall back to decode_tk15 (comparison-based)
//     and still recover the exact same frame, reassigned to TexkomTypeTk15.
//     This is the one place the file silently changes its own classification
//     of the same bits depending on how sloppy the timings are, so it is
//     checked as its own scenario rather than folded into scenario 1.
//  3. Round trip TK17 via the public dispatch: the 2-bit-per-pair share
//     encoding (and the pair reversal within each byte) is inverted by hand
//     and fed through 64 synthetic intervals.
//  4. Round trip MMBIT: 3-byte UID field, high-nibble-only CRC mask -- checks
//     that a payload-nibble (low nibble) change leaves is_crc_valid() true,
//     since MMBIT's mask excludes it, while a high-nibble change makes it
//     false.
//  5. Negative: corrupted preamble (first bit cleared) -> decode_intervals
//     rejects, both via decode_tk13's own preamble check and a second run
//     confirming the clean frame still decodes (not a one-shot fluke).
//  6. Negative: a frame whose type byte names TK17 but is sent with TK13-style
//     127-interval coding -- decode_tk13 recovers the bits bit-exactly, but
//     texkom_type_from_frame() must still reject it (coding mismatch), so
//     decode_intervals as a whole returns false.
//  7. Negative (TK17 decoder, standalone): a share outside [10,90] and a
//     total == 0 pair must both be rejected without UB (division guarded).
//  8. texkom_is_crc_valid()/get_expected_crc()/update_crc() consistency for
//     all three known CRC variants (TK13, TK17, MMBIT) plus TexkomTypeUnknown
//     (no known CRC -> both query functions must say so).

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- lib/one_wire/maxim_crc.c, verbatim (furi_check(data) dropped: data is
// never NULL in any call below). Confirmed byte-identical to Unleashed dev
// before this file was written. ---
static uint8_t maxim_crc8(const uint8_t* data, const uint8_t data_size, const uint8_t crc_init) {
    uint8_t crc = crc_init;
    for(uint8_t index = 0; index < data_size; ++index) {
        uint8_t input_byte = data[index];
        for(uint8_t bit_position = 0; bit_position < 8; ++bit_position) {
            const uint8_t mix = (crc ^ input_byte) & (uint8_t)(0x01);
            crc >>= 1;
            if(mix != 0) crc ^= 0x8C;
            input_byte >>= 1;
        }
    }
    return crc;
}
#define MAXIM_CRC8_INIT 0

// --- lib/nfc/protocols/texkom/texkom.h + texkom_i.h, transcribed ---
#define TEXKOM_FRAME_SIZE       (8U)
#define TEXKOM_UID_SIZE         (4U)
#define TEXKOM_MMBIT_UID_SIZE   (3U)
#define TEXKOM_UID_OFFSET       (3U)
#define TEXKOM_MMBIT_UID_OFFSET (4U)
#define TEXKOM_TYPE_BYTE_TK13   (0x63U)
#define TEXKOM_TYPE_BYTE_TK17   (0xCAU)
#define TEXKOM_TK13_INTERVALS   (127U)
#define TEXKOM_TK17_INTERVALS   (64U)
#define TEXKOM_FRAME_BITS       (TEXKOM_FRAME_SIZE * 8)
#define TEXKOM_TK13_RAW_BITS    (TEXKOM_FRAME_BITS * 2)
#define TEXKOM_PREAMBLE_BITS    (16U)

typedef enum {
    TexkomTypeUnknown,
    TexkomTypeTk13,
    TexkomTypeTk15,
    TexkomTypeTk17,
    TexkomTypeMmbit,
    TexkomTypeNum,
} TexkomType;

typedef enum {
    TexkomCodingTk13,
    TexkomCodingTk17,
} TexkomCoding;

typedef struct {
    uint8_t frame[TEXKOM_FRAME_SIZE];
    TexkomType type;
} TexkomData;

typedef uint8_t (*TexkomCrcHandler)(const uint8_t* uid);

typedef struct {
    const char* full_name;
    const char* type_name;
    uint8_t uid_offset;
    uint8_t uid_size;
    TexkomCoding coding;
    uint8_t type_byte;
    TexkomCrcHandler crc;
    uint8_t crc_mask;
} TexkomFeatures;

static uint8_t texkom_crc_tk13(const uint8_t* uid);
static uint8_t texkom_crc_tk17(const uint8_t* uid);
static uint8_t texkom_crc_mmbit(const uint8_t* uid);

static const TexkomFeatures texkom_features[TexkomTypeNum] = {
    [TexkomTypeUnknown] =
        {.type_byte = 0x00,
         .full_name = "Texkom (unknown)",
         .type_name = "Unknown",
         .uid_offset = TEXKOM_UID_OFFSET,
         .uid_size = TEXKOM_UID_SIZE,
         .coding = TexkomCodingTk13,
         .crc = NULL,
         .crc_mask = 0x00},
    [TexkomTypeTk13] =
        {.type_byte = TEXKOM_TYPE_BYTE_TK13,
         .full_name = "Texkom TK13",
         .type_name = "TK13",
         .uid_offset = TEXKOM_UID_OFFSET,
         .uid_size = TEXKOM_UID_SIZE,
         .coding = TexkomCodingTk13,
         .crc = texkom_crc_tk13,
         .crc_mask = 0xFF},
    [TexkomTypeTk15] =
        {.type_byte = TEXKOM_TYPE_BYTE_TK13,
         .full_name = "Texkom TK15",
         .type_name = "TK15",
         .uid_offset = TEXKOM_UID_OFFSET,
         .uid_size = TEXKOM_UID_SIZE,
         .coding = TexkomCodingTk13,
         .crc = texkom_crc_tk13,
         .crc_mask = 0xFF},
    [TexkomTypeTk17] =
        {.type_byte = TEXKOM_TYPE_BYTE_TK17,
         .full_name = "Texkom TK17",
         .type_name = "TK17",
         .uid_offset = TEXKOM_UID_OFFSET,
         .uid_size = TEXKOM_UID_SIZE,
         .coding = TexkomCodingTk17,
         .crc = texkom_crc_tk17,
         .crc_mask = 0xFF},
    [TexkomTypeMmbit] =
        {.type_byte = 0xFF,
         .full_name = "Texkom MMBIT",
         .type_name = "MMBIT",
         .uid_offset = TEXKOM_MMBIT_UID_OFFSET,
         .uid_size = TEXKOM_MMBIT_UID_SIZE,
         .coding = TexkomCodingTk13,
         .crc = texkom_crc_mmbit,
         .crc_mask = 0xF0},
};

static const TexkomFeatures* texkom_get_features(const TexkomData* data) {
    assert(data->type < TexkomTypeNum);
    return &texkom_features[data->type];
}

static uint8_t texkom_crc_tk13(const uint8_t* uid) {
    uint8_t crc = 0;
    for(size_t i = 0; i < TEXKOM_UID_SIZE; i++) {
        crc ^= uid[i];
        for(size_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (0x31 ^ (crc << 1)) : (crc << 1);
        }
    }
    return crc;
}

static uint8_t texkom_crc_tk17(const uint8_t* uid) {
    return maxim_crc8(uid, TEXKOM_UID_SIZE, MAXIM_CRC8_INIT);
}

static uint8_t texkom_crc_mmbit(const uint8_t* uid) {
    uint8_t crc = 0x0F;
    for(size_t i = 0; i < TEXKOM_MMBIT_UID_SIZE; i++) {
        crc ^= (uid[i] & 0x0F) ^ (uid[i] >> 4);
    }
    return (crc & 0x0F) << 4;
}

static bool texkom_crc_byte(const TexkomData* data, uint8_t* crc) {
    const TexkomFeatures* features = texkom_get_features(data);
    if(!features->crc) return false;
    const uint8_t stored = data->frame[TEXKOM_FRAME_SIZE - 1];
    *crc = (stored & ~features->crc_mask) | features->crc(&data->frame[features->uid_offset]);
    return true;
}

static bool texkom_is_crc_valid(const TexkomData* data) {
    uint8_t expected;
    return texkom_crc_byte(data, &expected) && expected == data->frame[TEXKOM_FRAME_SIZE - 1];
}

static bool texkom_get_expected_crc(const TexkomData* data, uint8_t* crc) {
    return texkom_crc_byte(data, crc);
}

static void texkom_update_crc(TexkomData* data) {
    uint8_t crc;
    if(texkom_crc_byte(data, &crc)) {
        data->frame[TEXKOM_FRAME_SIZE - 1] = crc;
    }
}

static bool texkom_make_blank(TexkomData* data, TexkomType type) {
    if(type == TexkomTypeUnknown || type >= TexkomTypeNum) return false;
    data->type = type;
    memset(data->frame, 0, TEXKOM_FRAME_SIZE);
    data->frame[0] = 0xFF;
    data->frame[1] = 0xFF;
    data->frame[2] = texkom_features[type].type_byte;
    if(type == TexkomTypeMmbit) {
        data->frame[3] = 0xFF;
    }
    texkom_update_crc(data);
    return true;
}

static const uint8_t* texkom_get_uid(const TexkomData* data, size_t* uid_len) {
    const TexkomFeatures* features = texkom_get_features(data);
    if(uid_len) *uid_len = features->uid_size;
    return &data->frame[features->uid_offset];
}

static bool texkom_set_uid(TexkomData* data, const uint8_t* uid, size_t uid_len) {
    if(data->type == TexkomTypeUnknown) {
        return uid_len == TEXKOM_UID_SIZE || uid_len == TEXKOM_MMBIT_UID_SIZE;
    }
    const TexkomFeatures* features = texkom_get_features(data);
    if(uid_len != features->uid_size) return false;
    memcpy(&data->frame[features->uid_offset], uid, uid_len);
    texkom_update_crc(data);
    return true;
}

static bool
    texkom_average_gaps(const uint8_t* intervals, size_t count, uint32_t* hi, uint32_t* low) {
    uint8_t max = 0;
    uint8_t min = UINT8_MAX;
    for(size_t i = 0; i < count; i++) {
        if(intervals[i] > max) max = intervals[i];
        if(intervals[i] < min) min = intervals[i];
    }
    if(max <= min) return false;

    const uint32_t middle = ((uint32_t)max + min) / 2;
    uint32_t hi_sum = 0, hi_count = 0, low_sum = 0, low_count = 0;
    for(size_t i = 0; i < count; i++) {
        if(intervals[i] > middle) {
            hi_sum += intervals[i];
            hi_count++;
        } else {
            low_sum += intervals[i];
            low_count++;
        }
    }
    if(hi_count == 0 || low_count == 0) return false;

    *hi = hi_sum / hi_count;
    *low = low_sum / low_count;
    return *hi > *low;
}

static inline bool texkom_gap_matches(uint8_t interval, uint32_t length, uint32_t tolerance) {
    return (interval < length + tolerance) && (interval + tolerance > length);
}

static void texkom_pack_frame(const uint8_t* bits, uint8_t* frame) {
    memset(frame, 0, TEXKOM_FRAME_SIZE);
    for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
        frame[i / 8] = (frame[i / 8] << 1) | (bits[i] & 1);
    }
}

static bool texkom_preamble_valid(const uint8_t* bits) {
    for(size_t i = 0; i < TEXKOM_PREAMBLE_BITS; i++) {
        if(!bits[i]) return false;
    }
    return true;
}

static bool texkom_decode_tk13(const uint8_t* intervals, size_t count, uint8_t* frame) {
    assert(count == TEXKOM_TK13_INTERVALS);
    uint32_t hi = 0, low = 0;
    if(!texkom_average_gaps(intervals, count, &hi, &low)) return false;

    const uint32_t tolerance = (hi - low) / 3 + 1;

    uint8_t raw[TEXKOM_TK13_RAW_BITS];
    size_t raw_count = 0;

    for(size_t i = 0; i < count && raw_count < TEXKOM_TK13_RAW_BITS; i++) {
        if(texkom_gap_matches(intervals[i], hi, tolerance)) {
            raw[raw_count++] = 1;
        } else if(texkom_gap_matches(intervals[i], low, tolerance)) {
            raw[raw_count++] = 0;
        } else {
            return false;
        }
    }

    if(raw_count % 2 != 0 && raw_count < TEXKOM_TK13_RAW_BITS) {
        raw[raw_count] = !raw[raw_count - 1];
        raw_count++;
    }
    if(raw_count != TEXKOM_TK13_RAW_BITS) return false;

    uint8_t bits[TEXKOM_FRAME_BITS];
    for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
        if(raw[i * 2] == raw[i * 2 + 1]) return false;
        bits[i] = raw[i * 2];
    }

    if(!texkom_preamble_valid(bits)) return false;
    texkom_pack_frame(bits, frame);
    return true;
}

static bool texkom_decode_tk15(const uint8_t* intervals, size_t count, uint8_t* frame) {
    assert(count == TEXKOM_TK13_INTERVALS);
    uint8_t bits[TEXKOM_FRAME_BITS];
    size_t bit_count = 0;

    for(size_t i = 0; i + 1 < count && bit_count < TEXKOM_FRAME_BITS; i += 2) {
        if(intervals[i] == intervals[i + 1]) return false;
        bits[bit_count++] = intervals[i] > intervals[i + 1];
    }

    if(count > 2 && count % 2 != 0 && bit_count < TEXKOM_FRAME_BITS) {
        const int last = intervals[count - 1];
        const bool previous_bit = intervals[count - 3] > intervals[count - 2];
        const bool same_as_previous = abs(last - (int)intervals[count - 3]) <
                                      abs(last - (int)intervals[count - 2]);
        bits[bit_count++] = previous_bit ^ (!same_as_previous);
    }

    if(bit_count != TEXKOM_FRAME_BITS) return false;
    if(!texkom_preamble_valid(bits)) return false;
    texkom_pack_frame(bits, frame);
    return true;
}

static bool texkom_decode_tk17(const uint8_t* intervals, size_t count, uint8_t* frame) {
    assert(count == TEXKOM_TK17_INTERVALS);
    uint8_t raw[TEXKOM_FRAME_BITS];
    size_t raw_count = 0;

    for(size_t i = 0; i + 1 < count && raw_count + 1 < TEXKOM_FRAME_BITS; i += 2) {
        const uint32_t total = (uint32_t)intervals[i] + intervals[i + 1];
        if(total == 0) return false;

        const uint32_t share = ((uint32_t)intervals[i + 1] * 100) / total;
        if(share < 10 || share > 90) return false;

        if(share < 30) {
            raw[raw_count++] = 0;
            raw[raw_count++] = 0;
        } else if(share < 50) {
            raw[raw_count++] = 1;
            raw[raw_count++] = 0;
        } else if(share < 70) {
            raw[raw_count++] = 0;
            raw[raw_count++] = 1;
        } else {
            raw[raw_count++] = 1;
            raw[raw_count++] = 1;
        }
    }

    if(raw_count != TEXKOM_FRAME_BITS) return false;

    uint8_t bits[TEXKOM_FRAME_BITS];
    for(size_t byte = 0; byte < TEXKOM_FRAME_SIZE; byte++) {
        for(size_t pair = 0; pair < 4; pair++) {
            memcpy(&bits[byte * 8 + pair * 2], &raw[byte * 8 + (3 - pair) * 2], 2);
        }
    }

    if(!texkom_preamble_valid(bits)) return false;
    texkom_pack_frame(bits, frame);
    return true;
}

static TexkomType texkom_type_from_frame(const uint8_t* frame, TexkomCoding coding) {
    TexkomType type;
    if(frame[2] == TEXKOM_TYPE_BYTE_TK17) {
        type = TexkomTypeTk17;
    } else if(frame[2] == TEXKOM_TYPE_BYTE_TK13) {
        type = TexkomTypeTk13;
    } else if(frame[2] == 0xFF && frame[3] == 0xFF) {
        type = TexkomTypeMmbit;
    } else {
        return TexkomTypeUnknown;
    }
    return (texkom_features[type].coding == coding) ? type : TexkomTypeUnknown;
}

static bool
    texkom_decode_intervals(TexkomData* data, const uint8_t* intervals, size_t interval_count) {
    uint8_t frame[TEXKOM_FRAME_SIZE];
    TexkomType type = TexkomTypeUnknown;

    if(interval_count == TEXKOM_TK17_INTERVALS) {
        if(texkom_decode_tk17(intervals, interval_count, frame)) {
            type = texkom_type_from_frame(frame, TexkomCodingTk17);
        }
    } else if(interval_count == TEXKOM_TK13_INTERVALS) {
        if(texkom_decode_tk13(intervals, interval_count, frame)) {
            type = texkom_type_from_frame(frame, TexkomCodingTk13);
        } else if(texkom_decode_tk15(intervals, interval_count, frame)) {
            type = texkom_type_from_frame(frame, TexkomCodingTk13);
            if(type == TexkomTypeTk13) type = TexkomTypeTk15;
        }
    }

    if(type == TexkomTypeUnknown) return false;

    memcpy(data->frame, frame, TEXKOM_FRAME_SIZE);
    data->type = type;
    return true;
}

// --- Test helpers (not part of the ported file) ---

static void frame_to_bits(const uint8_t frame[TEXKOM_FRAME_SIZE], uint8_t bits[TEXKOM_FRAME_BITS]) {
    for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
        bits[i] = (frame[i / 8] >> (7 - (i % 8))) & 1;
    }
}

// TK13-family encode: bit 1 -> raw pair (1,0), bit 0 -> raw pair (0,1); raw 1 -> gap `hi`,
// raw 0 -> gap `lo`. Drops the very last raw bit (127 of 128), same as a real capture.
static void encode_tk13_family(
    const uint8_t bits[TEXKOM_FRAME_BITS],
    uint8_t hi,
    uint8_t lo,
    uint8_t out[TEXKOM_TK13_INTERVALS]) {
    uint8_t raw[TEXKOM_TK13_RAW_BITS];
    for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
        raw[i * 2] = bits[i] ? 1 : 0;
        raw[i * 2 + 1] = bits[i] ? 0 : 1;
    }
    for(size_t i = 0; i < TEXKOM_TK13_INTERVALS; i++) {
        out[i] = raw[i] ? hi : lo;
    }
}

// Mirrors encode_tk13_family, but bit position i in [32,64) uses a second, narrower
// (hi2,lo2) gap pair instead of (hi,lo) -- used to force decode_tk13's single global
// threshold to misclassify while decode_tk15's pairwise comparison still works everywhere.
static void encode_tk13_family_mixed(
    const uint8_t bits[TEXKOM_FRAME_BITS],
    uint8_t hi,
    uint8_t lo,
    uint8_t hi2,
    uint8_t lo2,
    uint8_t out[TEXKOM_TK13_INTERVALS]) {
    uint8_t raw[TEXKOM_TK13_RAW_BITS];
    for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
        raw[i * 2] = bits[i] ? 1 : 0;
        raw[i * 2 + 1] = bits[i] ? 0 : 1;
    }
    for(size_t i = 0; i < TEXKOM_TK13_INTERVALS; i++) {
        const bool second_half = (i / 2) >= 32;
        const uint8_t h = second_half ? hi2 : hi;
        const uint8_t l = second_half ? lo2 : lo;
        out[i] = raw[i] ? h : l;
    }
}

// TK17 encode: inverts decode_tk17's byte-local pair reversal, then maps each 2-bit raw
// value to a (i0,i1) gap pair whose share = i1*100/(i0+i1) falls in that raw value's band.
static void encode_tk17(const uint8_t bits[TEXKOM_FRAME_BITS], uint8_t out[TEXKOM_TK17_INTERVALS]) {
    uint8_t raw[TEXKOM_FRAME_BITS];
    for(size_t byte = 0; byte < TEXKOM_FRAME_SIZE; byte++) {
        for(size_t pair = 0; pair < 4; pair++) {
            memcpy(&raw[byte * 8 + (3 - pair) * 2], &bits[byte * 8 + pair * 2], 2);
        }
    }
    for(size_t i = 0; i < TEXKOM_FRAME_BITS / 2; i++) {
        const uint8_t b0 = raw[i * 2];
        const uint8_t b1 = raw[i * 2 + 1];
        uint8_t share;
        if(b0 == 0 && b1 == 0) {
            share = 20;
        } else if(b0 == 1 && b1 == 0) {
            share = 40;
        } else if(b0 == 0 && b1 == 1) {
            share = 60;
        } else {
            share = 80;
        }
        out[i * 2] = (uint8_t)(100 - share);
        out[i * 2 + 1] = share;
    }
}

static int checks = 0;

#define CHECK(cond, msg)                               \
    do {                                                \
        if(!(cond)) {                                    \
            fprintf(stderr, "FAIL: %s\n", msg);           \
            return 1;                                      \
        }                                                    \
        checks++;                                              \
    } while(0)

int main(void) {
    // --- Scenario 1: TK13 round trip via the public dispatch, clean separation. ---
    {
        TexkomData d;
        CHECK(texkom_make_blank(&d, TexkomTypeTk13), "s1 make_blank");
        const uint8_t uid[TEXKOM_UID_SIZE] = {0x11, 0x22, 0x33, 0x44};
        CHECK(texkom_set_uid(&d, uid, TEXKOM_UID_SIZE), "s1 set_uid");
        CHECK(texkom_is_crc_valid(&d), "s1 crc valid after set_uid");

        uint8_t bits[TEXKOM_FRAME_BITS];
        frame_to_bits(d.frame, bits);
        uint8_t intervals[TEXKOM_TK13_INTERVALS];
        encode_tk13_family(bits, 200, 50, intervals);

        TexkomData decoded = {0};
        CHECK(
            texkom_decode_intervals(&decoded, intervals, TEXKOM_TK13_INTERVALS),
            "s1 decode_intervals");
        CHECK(decoded.type == TexkomTypeTk13, "s1 type == Tk13");
        CHECK(memcmp(decoded.frame, d.frame, TEXKOM_FRAME_SIZE) == 0, "s1 frame round trip");
    }

    // --- Scenario 2: same frame, narrow+overlapping second half -> decode_tk13 must
    // fail (equal raw pair under the single global threshold), decode_intervals must
    // fall back to decode_tk15 and still land on the exact same bits, as TexkomTypeTk15. ---
    {
        TexkomData d;
        CHECK(texkom_make_blank(&d, TexkomTypeTk13), "s2 make_blank");
        const uint8_t uid[TEXKOM_UID_SIZE] = {0xAA, 0x55, 0x01, 0xFE};
        CHECK(texkom_set_uid(&d, uid, TEXKOM_UID_SIZE), "s2 set_uid");

        uint8_t bits[TEXKOM_FRAME_BITS];
        frame_to_bits(d.frame, bits);
        uint8_t intervals[TEXKOM_TK13_INTERVALS];
        encode_tk13_family_mixed(bits, 200, 50, 70, 55, intervals);

        // Confirm decode_tk13 alone really does fail against this input (not decoding
        // it, correctly or otherwise) before checking the fallback behaves.
        uint8_t throwaway[TEXKOM_FRAME_SIZE];
        CHECK(
            !texkom_decode_tk13(intervals, TEXKOM_TK13_INTERVALS, throwaway),
            "s2 decode_tk13 must fail on mixed-magnitude timing");

        TexkomData decoded = {0};
        CHECK(
            texkom_decode_intervals(&decoded, intervals, TEXKOM_TK13_INTERVALS),
            "s2 decode_intervals (tk15 fallback)");
        CHECK(decoded.type == TexkomTypeTk15, "s2 type reassigned to Tk15");
        CHECK(memcmp(decoded.frame, d.frame, TEXKOM_FRAME_SIZE) == 0, "s2 frame round trip");
    }

    // --- Scenario 3: TK17 round trip via the public dispatch. ---
    {
        TexkomData d;
        CHECK(texkom_make_blank(&d, TexkomTypeTk17), "s3 make_blank");
        const uint8_t uid[TEXKOM_UID_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
        CHECK(texkom_set_uid(&d, uid, TEXKOM_UID_SIZE), "s3 set_uid");
        CHECK(texkom_is_crc_valid(&d), "s3 crc valid after set_uid");

        uint8_t bits[TEXKOM_FRAME_BITS];
        frame_to_bits(d.frame, bits);
        uint8_t intervals[TEXKOM_TK17_INTERVALS];
        encode_tk17(bits, intervals);

        TexkomData decoded = {0};
        CHECK(
            texkom_decode_intervals(&decoded, intervals, TEXKOM_TK17_INTERVALS),
            "s3 decode_intervals");
        CHECK(decoded.type == TexkomTypeTk17, "s3 type == Tk17");
        CHECK(memcmp(decoded.frame, d.frame, TEXKOM_FRAME_SIZE) == 0, "s3 frame round trip");
    }

    // --- Scenario 4: MMBIT -- 3-byte UID, high-nibble-only CRC mask. ---
    {
        TexkomData d;
        CHECK(texkom_make_blank(&d, TexkomTypeMmbit), "s4 make_blank");
        CHECK(d.frame[3] == 0xFF, "s4 second FF marker");
        const uint8_t uid[TEXKOM_MMBIT_UID_SIZE] = {0x12, 0x34, 0x56};
        CHECK(texkom_set_uid(&d, uid, TEXKOM_MMBIT_UID_SIZE), "s4 set_uid");
        CHECK(texkom_is_crc_valid(&d), "s4 crc valid after set_uid");
        CHECK(
            texkom_set_uid(&d, uid, TEXKOM_UID_SIZE) == false,
            "s4 set_uid rejects wrong length for the variant");

        // Low nibble is payload for MMBIT (crc_mask 0xF0): changing it must not
        // disturb CRC validity. High nibble is the checksum: changing it must.
        TexkomData low_changed = d;
        low_changed.frame[TEXKOM_FRAME_SIZE - 1] ^= 0x0F;
        CHECK(texkom_is_crc_valid(&low_changed), "s4 low-nibble payload change keeps crc valid");

        TexkomData high_changed = d;
        high_changed.frame[TEXKOM_FRAME_SIZE - 1] ^= 0xF0;
        CHECK(!texkom_is_crc_valid(&high_changed), "s4 high-nibble crc change is caught");

        size_t uid_len = 0;
        const uint8_t* got_uid = texkom_get_uid(&d, &uid_len);
        CHECK(uid_len == TEXKOM_MMBIT_UID_SIZE, "s4 get_uid length");
        CHECK(memcmp(got_uid, uid, TEXKOM_MMBIT_UID_SIZE) == 0, "s4 get_uid content");
    }

    // --- Scenario 5: corrupted preamble is rejected; the clean frame still decodes
    // right after, so the rejection is not a one-shot fluke of shared state. ---
    {
        TexkomData d;
        CHECK(texkom_make_blank(&d, TexkomTypeTk13), "s5 make_blank");
        const uint8_t uid[TEXKOM_UID_SIZE] = {0x01, 0x02, 0x03, 0x04};
        CHECK(texkom_set_uid(&d, uid, TEXKOM_UID_SIZE), "s5 set_uid");

        uint8_t bits[TEXKOM_FRAME_BITS];
        frame_to_bits(d.frame, bits);
        uint8_t intervals[TEXKOM_TK13_INTERVALS];
        encode_tk13_family(bits, 200, 50, intervals);

        uint8_t corrupted_bits[TEXKOM_FRAME_BITS];
        memcpy(corrupted_bits, bits, sizeof(bits));
        corrupted_bits[0] = 0; // break the FF FF preamble
        uint8_t corrupted_intervals[TEXKOM_TK13_INTERVALS];
        encode_tk13_family(corrupted_bits, 200, 50, corrupted_intervals);

        TexkomData decoded = {0};
        CHECK(
            !texkom_decode_intervals(&decoded, corrupted_intervals, TEXKOM_TK13_INTERVALS),
            "s5 corrupted preamble rejected");
        CHECK(
            texkom_decode_intervals(&decoded, intervals, TEXKOM_TK13_INTERVALS),
            "s5 clean frame still decodes after the corrupted one");
    }

    // --- Scenario 6: TK17 type byte sent with TK13-style (127-interval) coding. The
    // bits decode bit-exactly (the coding itself doesn't know about type bytes), but
    // texkom_type_from_frame() must reject the type/coding mismatch either way. ---
    {
        uint8_t frame[TEXKOM_FRAME_SIZE] = {0xFF, 0xFF, TEXKOM_TYPE_BYTE_TK17, 0, 0, 0, 0, 0};
        uint8_t bits[TEXKOM_FRAME_BITS];
        frame_to_bits(frame, bits);
        uint8_t intervals[TEXKOM_TK13_INTERVALS];
        encode_tk13_family(bits, 200, 50, intervals);

        uint8_t decoded_frame[TEXKOM_FRAME_SIZE];
        CHECK(
            texkom_decode_tk13(intervals, TEXKOM_TK13_INTERVALS, decoded_frame),
            "s6 decode_tk13 recovers the bits");
        CHECK(memcmp(decoded_frame, frame, TEXKOM_FRAME_SIZE) == 0, "s6 bits are bit-exact");
        CHECK(
            texkom_type_from_frame(decoded_frame, TexkomCodingTk13) == TexkomTypeUnknown,
            "s6 coding mismatch rejected by type_from_frame");

        TexkomData decoded = {0};
        CHECK(
            !texkom_decode_intervals(&decoded, intervals, TEXKOM_TK13_INTERVALS),
            "s6 decode_intervals rejects the mismatch end to end");
    }

    // --- Scenario 7: TK17 decoder standalone -- out-of-band share and total==0 both
    // rejected without UB. ---
    {
        uint8_t frame[TEXKOM_FRAME_SIZE];
        uint8_t intervals[TEXKOM_TK17_INTERVALS] = {0};
        // First pair share = 5*100/100 = 5 < 10 -> out of band.
        intervals[0] = 95;
        intervals[1] = 5;
        CHECK(
            !texkom_decode_tk17(intervals, TEXKOM_TK17_INTERVALS, frame),
            "s7 out-of-band share rejected");

        uint8_t intervals_zero[TEXKOM_TK17_INTERVALS] = {0};
        CHECK(
            !texkom_decode_tk17(intervals_zero, TEXKOM_TK17_INTERVALS, frame),
            "s7 zero-total pair rejected without division by zero");
    }

    // --- Scenario 8: CRC query/update consistency across all variants, including the
    // "no known CRC" case. ---
    {
        TexkomData unknown = {0};
        unknown.type = TexkomTypeUnknown;
        uint8_t crc;
        CHECK(!texkom_get_expected_crc(&unknown, &crc), "s8 Unknown has no known crc");
        CHECK(!texkom_is_crc_valid(&unknown), "s8 Unknown is never crc-valid");

        const TexkomType known[] = {TexkomTypeTk13, TexkomTypeTk17, TexkomTypeMmbit};
        for(size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
            TexkomData d;
            CHECK(texkom_make_blank(&d, known[i]), "s8 make_blank");
            uint8_t expected;
            CHECK(texkom_get_expected_crc(&d, &expected), "s8 get_expected_crc succeeds");
            CHECK(expected == d.frame[TEXKOM_FRAME_SIZE - 1], "s8 make_blank already set it");
            CHECK(texkom_is_crc_valid(&d), "s8 is_crc_valid agrees");

            d.frame[TEXKOM_FRAME_SIZE - 1] ^= 0xFF;
            CHECK(!texkom_is_crc_valid(&d), "s8 flipped crc byte is caught");
            texkom_update_crc(&d);
            CHECK(texkom_is_crc_valid(&d), "s8 update_crc repairs it");
        }
    }

    printf("%d checks passed -- ALL CHECKS PASSED\n", checks);
    return 0;
}
