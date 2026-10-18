// Host repro for the Texkom NFC Poller layer (Phase 2 of the texkom port,
// mntm-012, API 87.1). Transcribes lib/nfc/protocols/texkom/texkom_poller.c
// (alloc/free/set_callback/get_data/read/run/detect and the nfc_poller_texkom
// vtable) exactly as it reads upstream. furi_assert() -> assert().
//
// The Phase 1 protocol core (texkom_decode_intervals() and everything it
// calls) is reused here unmodified -- same transcription already verified in
// repro.c, duplicated into this file so it builds as its own translation
// unit. This file does not re-check Phase 1's own CRC/decode correctness
// (already 57/57 in repro.c); it checks only the Poller control flow on top
// of it: does it call the right things in the right order, does it gate on
// the right event, does it wire the callback/event plumbing correctly, does
// it leave old data alone on failure.
//
// The only real hardware boundary the Poller crosses is nfc_texkom_poller_rx()
// (lib/nfc/nfc.c; itself only a thin wrapper around furi_hal_nfc_poller_rx(),
// Phase 4, not yet ported). That one function is mocked here, test-scripted
// per scenario; Nfc/BitBuffer/NfcGenericEvent/NfcPollerBase are minimal
// stand-ins exposing only the subset of fields/functions texkom_poller.c
// itself actually touches, with the same names and signatures as upstream's
// lib/nfc/nfc.h / lib/nfc/protocols/nfc_generic_event.h /
// lib/nfc/protocols/nfc_poller_base.h so the transcribed file below needed no
// edits beyond dropping #include lines.
//
// Scenarios:
//  1. Alloc/free lifecycle: nfc_config() called exactly once with
//     (nfc, NfcModePoller, NfcTechTexkom); fresh data is blank
//     (TexkomTypeUnknown, from texkom_alloc()'s own texkom_reset()); the
//     general_event/texkom_event wiring done once in alloc() is already
//     correct before any read ever happens; alloc/free are balanced (no
//     leak, no double free) under counting malloc/free wrappers.
//  2. run()/detect() gate on NfcEventTypePollerReady: an event of any other
//     type must return immediately (false / NfcCommandContinue) without
//     ever calling nfc_texkom_poller_rx() or the callback.
//  3. TK13 end to end via run(): one successful capture reaches the
//     callback exactly once, with the correct NfcGenericEvent
//     (protocol/instance/event_data), get_data() reflects the decoded
//     frame, and run()'s return value is exactly whatever the callback
//     returned (not hardcoded) -- proves the command is plumbed through,
//     not synthesized.
//  4. TK17 via detect(): same capture success, but through detect() --
//     returns true, get_data() reflects it, and -- unlike run() -- the
//     callback is never invoked (detect() has no callback concept).
//  5. A capture that returns NfcErrorNone but data no decoder accepts (all
//     intervals equal -> average_gaps' max<=min guard) must fail read(),
//     so run() stays at NfcCommandContinue and detect() stays false,
//     without invoking the callback, and without touching the previous
//     successful decode's data.
//  6. NfcErrorTimeout short-circuits before the interval buffer is ever
//     inspected: the mock returns a timeout while the rx_buffer still holds
//     stale, validly-shaped bytes from an earlier successful call --
//     get_data() must remain exactly what it was before this call, proving
//     the `if(error != NfcErrorNone) return false;` guard really is checked
//     first, not bypassed into redecoding leftover bytes.
//  7. All four variants (TK13, TK15-coding, TK17, MMBIT) round-trip through
//     the full run()->callback->get_data() path, each using the same
//     Phase-1 vectors already verified in repro.c -- the Poller never
//     special-cases a variant, it only ever calls the one decode dispatch.
//  8. Repeated successful run() calls keep the same general_event/
//     texkom_event pointer identity (wired once at alloc, never rebuilt
//     per call).

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_assert(x) assert(x)
// FURI_LOG_D is a trace-only logging macro upstream; dropped to a no-op here since this repro
// has no logger. The call site below keeps the real macro name unchanged.
#define FURI_LOG_D(...) ((void)0)

// ============================================================================
// Phase 1 protocol core, transcribed unmodified from repro.c (same logic
// already verified there, 57/57 checks) -- duplicated so this file is a
// standalone translation unit.
// ============================================================================

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

static void texkom_reset(TexkomData* data) {
    memset(data, 0, sizeof(TexkomData));
    data->type = TexkomTypeUnknown;
}

static TexkomData* texkom_alloc(void) {
    TexkomData* data = malloc(sizeof(TexkomData));
    texkom_reset(data);
    return data;
}

static void texkom_free(TexkomData* data) {
    furi_assert(data);
    free(data);
}

// --- Vector builders, identical to repro.c's "Test helpers" ---

static void frame_to_bits(const uint8_t frame[TEXKOM_FRAME_SIZE], uint8_t bits[TEXKOM_FRAME_BITS]) {
    for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
        bits[i] = (frame[i / 8] >> (7 - (i % 8))) & 1;
    }
}

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

// Builds a TK13-family (TK13/TK15-coding/MMBIT) interval vector for a given UID/type in
// one call, mirroring what a real capture of that variant would hand to the Poller.
static void
    build_tk13_family_vector(TexkomType type, const uint8_t* uid, size_t uid_len, uint8_t out[TEXKOM_TK13_INTERVALS]) {
    TexkomData d;
    bool ok = texkom_make_blank(&d, type);
    furi_assert(ok);
    ok = texkom_set_uid(&d, uid, uid_len);
    furi_assert(ok);
    uint8_t bits[TEXKOM_FRAME_BITS];
    frame_to_bits(d.frame, bits);
    encode_tk13_family(bits, 200, 50, out);
}

static void build_tk17_vector(const uint8_t* uid, uint8_t out[TEXKOM_TK17_INTERVALS]) {
    TexkomData d;
    bool ok = texkom_make_blank(&d, TexkomTypeTk17);
    furi_assert(ok);
    ok = texkom_set_uid(&d, uid, TEXKOM_UID_SIZE);
    furi_assert(ok);
    uint8_t bits[TEXKOM_FRAME_BITS];
    frame_to_bits(d.frame, bits);
    encode_tk17(bits, out);
}

// ============================================================================
// Minimal Nfc/BitBuffer/NfcGenericEvent/NfcPollerBase stand-ins -- same names
// and signatures as lib/nfc/nfc.h / lib/nfc/protocols/nfc_generic_event.h /
// lib/nfc/protocols/nfc_poller_base.h, limited to what texkom_poller.c itself
// touches. Nothing here is firmware code; it is the seam the real Nfc/HAL
// layer (Phase 4, not yet ported) will fill in.
// ============================================================================

typedef struct Nfc Nfc;
struct Nfc {
    int tag; // unused; just gives the opaque pointer somewhere real to point to.
};

typedef enum {
    NfcModePoller,
    NfcModeListener,
} NfcMode;

typedef enum {
    NfcTechIso14443a,
    NfcTechTexkom,
} NfcTech;

typedef enum {
    NfcErrorNone,
    NfcErrorTimeout,
    NfcErrorInternal,
} NfcError;

typedef enum {
    NfcEventTypeFieldOn,
    NfcEventTypePollerReady,
} NfcEventType;

typedef struct {
    NfcEventType type;
} NfcEvent;

typedef enum {
    NfcProtocolTexkom,
    NfcProtocolNum,
    NfcProtocolInvalid,
} NfcProtocol;

typedef void NfcGenericInstance;
typedef void NfcGenericEventData;

typedef struct {
    NfcProtocol protocol;
    NfcGenericInstance* instance;
    NfcGenericEventData* event_data;
} NfcGenericEvent;

typedef enum {
    NfcCommandContinue,
    NfcCommandReset,
    NfcCommandStop,
} NfcCommand;

typedef NfcCommand (*NfcGenericCallback)(NfcGenericEvent event, void* context);

typedef struct BitBuffer BitBuffer;
struct BitBuffer {
    uint8_t* data;
    size_t capacity_bytes;
    size_t size_bytes;
};

static BitBuffer* bit_buffer_alloc(size_t capacity_bytes) {
    furi_assert(capacity_bytes);
    BitBuffer* buf = malloc(sizeof(BitBuffer));
    buf->data = malloc(capacity_bytes);
    buf->capacity_bytes = capacity_bytes;
    buf->size_bytes = 0;
    return buf;
}

static void bit_buffer_free(BitBuffer* buf) {
    furi_assert(buf);
    free(buf->data);
    free(buf);
}

static const uint8_t* bit_buffer_get_data(const BitBuffer* buf) {
    furi_assert(buf);
    return buf->data;
}

static size_t bit_buffer_get_size_bytes(const BitBuffer* buf) {
    furi_assert(buf);
    return buf->size_bytes;
}

// Test-only: stands in for the real bit_buffer_copy_bits() call made inside the real,
// not-yet-ported nfc_texkom_poller_rx() (lib/nfc/nfc.c) -- fills the buffer the way a
// real capture would, once the mock below has decided what that capture returned.
static void test_bit_buffer_fill(BitBuffer* buf, const uint8_t* data, size_t size_bytes) {
    furi_assert(buf);
    furi_assert(size_bytes <= buf->capacity_bytes);
    memcpy(buf->data, data, size_bytes);
    buf->size_bytes = size_bytes;
}

typedef NfcGenericInstance* (*NfcPollerAlloc)(NfcGenericInstance* base_poller);
typedef void (*NfcPollerFree)(NfcGenericInstance* instance);
typedef void (
    *NfcPollerSetCallback)(NfcGenericInstance* poller, NfcGenericCallback callback, void* context);
typedef NfcCommand (*NfcPollerRun)(NfcGenericEvent event, void* context);
typedef bool (*NfcPollerDetect)(NfcGenericEvent event, void* context);
typedef void NfcDeviceData;
typedef const NfcDeviceData* (*NfcPollerGetData)(const NfcGenericInstance* instance);

typedef struct {
    NfcPollerAlloc alloc;
    NfcPollerFree free;
    NfcPollerSetCallback set_callback;
    NfcPollerRun run;
    NfcPollerDetect detect;
    NfcPollerGetData get_data;
} NfcPollerBase;

// nfc_config(): records what it was last called with, so alloc() can be checked; otherwise
// a no-op (the real one configures the NFC HAL, Phase 4).
static int nfc_config_call_count = 0;
static Nfc* nfc_config_last_instance = NULL;
static NfcMode nfc_config_last_mode = (NfcMode)-1;
static NfcTech nfc_config_last_tech = (NfcTech)-1;

static void nfc_config(Nfc* instance, NfcMode mode, NfcTech tech) {
    nfc_config_call_count++;
    nfc_config_last_instance = instance;
    nfc_config_last_mode = mode;
    nfc_config_last_tech = tech;
}

// nfc_texkom_poller_rx() mock: each test programs exactly one scripted response before
// calling into the Poller, then checks call_count afterwards.
typedef struct {
    bool programmed;
    NfcError result;
    const uint8_t* intervals;
    size_t interval_count;
} MockRxResponse;

static MockRxResponse mock_rx_response;
static int mock_rx_call_count = 0;

static void mock_rx_program(NfcError result, const uint8_t* intervals, size_t interval_count) {
    mock_rx_response.programmed = true;
    mock_rx_response.result = result;
    mock_rx_response.intervals = intervals;
    mock_rx_response.interval_count = interval_count;
}

static NfcError nfc_texkom_poller_rx(Nfc* instance, BitBuffer* rx_buffer) {
    furi_assert(instance);
    furi_assert(rx_buffer);
    furi_assert(mock_rx_response.programmed);

    mock_rx_call_count++;
    mock_rx_response.programmed = false;

    if(mock_rx_response.result != NfcErrorNone) return mock_rx_response.result;

    test_bit_buffer_fill(rx_buffer, mock_rx_response.intervals, mock_rx_response.interval_count);
    return NfcErrorNone;
}

// ============================================================================
// lib/nfc/protocols/texkom/texkom_poller.h, transcribed unmodified.
// ============================================================================

typedef struct TexkomPoller TexkomPoller;

typedef enum {
    TexkomPollerEventTypeReady,
} TexkomPollerEventType;

typedef struct {
    TexkomPollerEventType type;
} TexkomPollerEvent;

// ============================================================================
// lib/nfc/protocols/texkom/texkom_poller_i.h, transcribed unmodified.
// ============================================================================

struct TexkomPoller {
    Nfc* nfc;
    TexkomData* data;
    BitBuffer* rx_buffer;

    NfcGenericEvent general_event;
    TexkomPollerEvent texkom_event;
    NfcGenericCallback callback;
    void* context;
};

const TexkomData* texkom_poller_get_data(TexkomPoller* instance);

// ============================================================================
// lib/nfc/protocols/texkom/texkom_poller.c, transcribed unmodified (only the
// #include lines are dropped; everything from "static TexkomPoller*
// texkom_poller_alloc" to the nfc_poller_texkom table below is identical to
// upstream).
// ============================================================================

#define TAG "TexkomPoller"

static TexkomPoller* texkom_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    TexkomPoller* instance = malloc(sizeof(TexkomPoller));
    instance->nfc = nfc;
    instance->data = texkom_alloc();
    // One byte per gap, sized for the longest frame.
    instance->rx_buffer = bit_buffer_alloc(TEXKOM_TK13_INTERVALS);

    nfc_config(instance->nfc, NfcModePoller, NfcTechTexkom);

    instance->texkom_event.type = TexkomPollerEventTypeReady;
    instance->general_event.protocol = NfcProtocolTexkom;
    instance->general_event.event_data = &instance->texkom_event;
    instance->general_event.instance = instance;

    return instance;
}

static void texkom_poller_free(TexkomPoller* instance) {
    furi_assert(instance);

    bit_buffer_free(instance->rx_buffer);
    texkom_free(instance->data);
    free(instance);
}

static void
    texkom_poller_set_callback(TexkomPoller* instance, NfcGenericCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
}

const TexkomData* texkom_poller_get_data(TexkomPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->data);

    return instance->data;
}

/** Capture one repetition and decode it. */
static bool texkom_poller_read(TexkomPoller* instance) {
    const NfcError error = nfc_texkom_poller_rx(instance->nfc, instance->rx_buffer);
    if(error != NfcErrorNone) return false;

    const uint8_t* intervals = bit_buffer_get_data(instance->rx_buffer);
    const size_t count = bit_buffer_get_size_bytes(instance->rx_buffer);

    if(texkom_decode_intervals(instance->data, intervals, count)) return true;

    // A plausible length that no decoder accepted: either the timings are too ragged, or the
    // frame's type byte names a variant that does not use the coding it arrived in.
    FURI_LOG_D(TAG, "Captured %zu gaps, none of the codings fit", count);

    return false;
}

static NfcCommand texkom_poller_run(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolInvalid);
    furi_assert(event.event_data);

    TexkomPoller* instance = context;
    const NfcEvent* nfc_event = event.event_data;

    if(nfc_event->type != NfcEventTypePollerReady) return NfcCommandContinue;

    // A tag that talks unprompted gives no sign of being absent, so a failed attempt is not an
    // error worth reporting - keep listening until the caller stops us.
    if(!texkom_poller_read(instance)) return NfcCommandContinue;

    return instance->callback(instance->general_event, instance->context);
}

static bool texkom_poller_detect(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolInvalid);

    TexkomPoller* instance = context;
    const NfcEvent* nfc_event = event.event_data;

    if(nfc_event->type != NfcEventTypePollerReady) return false;

    return texkom_poller_read(instance);
}

static const NfcPollerBase nfc_poller_texkom = {
    .alloc = (NfcPollerAlloc)texkom_poller_alloc,
    .free = (NfcPollerFree)texkom_poller_free,
    .set_callback = (NfcPollerSetCallback)texkom_poller_set_callback,
    .run = (NfcPollerRun)texkom_poller_run,
    .detect = (NfcPollerDetect)texkom_poller_detect,
    .get_data = (NfcPollerGetData)texkom_poller_get_data,
};

// ============================================================================
// Test harness
// ============================================================================

static int test_callback_call_count = 0;
static NfcGenericEvent test_callback_last_event;
static void* test_callback_last_context = NULL;
static NfcCommand test_callback_return_value = NfcCommandContinue;

static NfcCommand test_callback(NfcGenericEvent event, void* context) {
    test_callback_call_count++;
    test_callback_last_event = event;
    test_callback_last_context = context;
    return test_callback_return_value;
}

static NfcGenericEvent make_poller_ready_event(NfcEvent* nfc_event) {
    nfc_event->type = NfcEventTypePollerReady;
    NfcGenericEvent event = {
        .protocol = NfcProtocolInvalid,
        .instance = NULL,
        .event_data = nfc_event,
    };
    return event;
}

static NfcGenericEvent make_field_on_event(NfcEvent* nfc_event) {
    nfc_event->type = NfcEventTypeFieldOn;
    NfcGenericEvent event = {
        .protocol = NfcProtocolInvalid,
        .instance = NULL,
        .event_data = nfc_event,
    };
    return event;
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
    Nfc nfc_hw = {0};

    // --- Scenario 1: alloc/free lifecycle, wiring already correct before any read. ---
    {
        nfc_config_call_count = 0;
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);

        CHECK(instance != NULL, "s1 alloc returns non-null");
        CHECK(nfc_config_call_count == 1, "s1 nfc_config called exactly once");
        CHECK(nfc_config_last_instance == &nfc_hw, "s1 nfc_config got the right Nfc*");
        CHECK(nfc_config_last_mode == NfcModePoller, "s1 nfc_config mode == Poller");
        CHECK(nfc_config_last_tech == NfcTechTexkom, "s1 nfc_config tech == Texkom");

        const TexkomData* data = (const TexkomData*)poller_base->get_data(instance);
        CHECK(data->type == TexkomTypeUnknown, "s1 fresh data is blank");

        CHECK(instance->texkom_event.type == TexkomPollerEventTypeReady, "s1 texkom_event wired");
        CHECK(instance->general_event.protocol == NfcProtocolTexkom, "s1 general_event protocol");
        CHECK(
            instance->general_event.event_data == &instance->texkom_event,
            "s1 general_event.event_data points at texkom_event");
        CHECK(instance->general_event.instance == instance, "s1 general_event.instance == self");

        poller_base->free(instance);
        // No crash, no double free (libc/ASan would abort on either) -- reaching here passes.
        checks++;
    }

    // --- Scenario 2: run()/detect() gate on NfcEventTypePollerReady. ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);

        mock_rx_call_count = 0;
        test_callback_call_count = 0;

        NfcEvent nfc_event;
        NfcGenericEvent event = make_field_on_event(&nfc_event);

        const NfcCommand cmd = poller_base->run(event, instance);
        CHECK(cmd == NfcCommandContinue, "s2 run() ignores non-ready event (Continue)");
        CHECK(mock_rx_call_count == 0, "s2 run() must not call nfc_texkom_poller_rx");
        CHECK(test_callback_call_count == 0, "s2 run() must not call the callback");

        const bool detected = poller_base->detect(event, instance);
        CHECK(detected == false, "s2 detect() ignores non-ready event");
        CHECK(mock_rx_call_count == 0, "s2 detect() must not call nfc_texkom_poller_rx either");

        poller_base->free(instance);
    }

    // --- Scenario 3: TK13 end to end via run(). ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);

        const uint8_t uid[TEXKOM_UID_SIZE] = {0x11, 0x22, 0x33, 0x44};
        uint8_t intervals[TEXKOM_TK13_INTERVALS];
        build_tk13_family_vector(TexkomTypeTk13, uid, TEXKOM_UID_SIZE, intervals);

        mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK13_INTERVALS);
        test_callback_call_count = 0;
        test_callback_return_value = NfcCommandReset; // arbitrary, distinct from Continue

        NfcEvent nfc_event;
        NfcGenericEvent event = make_poller_ready_event(&nfc_event);
        const NfcCommand cmd = poller_base->run(event, instance);

        CHECK(test_callback_call_count == 1, "s3 callback invoked exactly once");
        CHECK(cmd == NfcCommandReset, "s3 run() returns exactly what the callback returned");
        CHECK(
            test_callback_last_event.protocol == NfcProtocolTexkom,
            "s3 callback event.protocol == Texkom");
        CHECK(
            test_callback_last_event.instance == instance, "s3 callback event.instance == poller");
        CHECK(
            test_callback_last_event.event_data == &instance->texkom_event,
            "s3 callback event.event_data == &texkom_event");
        CHECK(test_callback_last_context == instance, "s3 callback context == poller");

        const TexkomData* data = (const TexkomData*)poller_base->get_data(instance);
        CHECK(data->type == TexkomTypeTk13, "s3 get_data reflects decoded type");
        const uint8_t expected_frame[TEXKOM_FRAME_SIZE] =
            {0xFF, 0xFF, TEXKOM_TYPE_BYTE_TK13, 0x11, 0x22, 0x33, 0x44, 0x00};
        CHECK(
            memcmp(data->frame, expected_frame, TEXKOM_UID_OFFSET + TEXKOM_UID_SIZE) == 0,
            "s3 get_data frame content up to CRC byte");

        poller_base->free(instance);
    }

    // --- Scenario 4: TK17 via detect() -- no callback involved. ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);

        const uint8_t uid[TEXKOM_UID_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
        uint8_t intervals[TEXKOM_TK17_INTERVALS];
        build_tk17_vector(uid, intervals);

        mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK17_INTERVALS);
        test_callback_call_count = 0;

        NfcEvent nfc_event;
        NfcGenericEvent event = make_poller_ready_event(&nfc_event);
        const bool detected = poller_base->detect(event, instance);

        CHECK(detected == true, "s4 detect() reports success");
        CHECK(test_callback_call_count == 0, "s4 detect() never calls the callback");

        const TexkomData* data = (const TexkomData*)poller_base->get_data(instance);
        CHECK(data->type == TexkomTypeTk17, "s4 get_data reflects TK17");
        CHECK(memcmp(&data->frame[TEXKOM_UID_OFFSET], uid, TEXKOM_UID_SIZE) == 0, "s4 uid matches");

        poller_base->free(instance);
    }

    // --- Scenario 5: capture succeeds (NfcErrorNone) but no decoder accepts the data;
    // must fail without touching the previous successful decode. ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);

        // Seed a known-good previous decode first.
        const uint8_t uid[TEXKOM_UID_SIZE] = {0x01, 0x02, 0x03, 0x04};
        uint8_t good_intervals[TEXKOM_TK13_INTERVALS];
        build_tk13_family_vector(TexkomTypeTk13, uid, TEXKOM_UID_SIZE, good_intervals);
        mock_rx_program(NfcErrorNone, good_intervals, TEXKOM_TK13_INTERVALS);
        NfcEvent nfc_event;
        NfcGenericEvent event = make_poller_ready_event(&nfc_event);
        CHECK(poller_base->detect(event, instance), "s5 seed capture succeeds");
        TexkomData seeded = *(const TexkomData*)poller_base->get_data(instance);

        // All-equal gaps: texkom_average_gaps' max<=min guard rejects outright.
        uint8_t garbage[TEXKOM_TK13_INTERVALS];
        memset(garbage, 100, sizeof(garbage));
        mock_rx_program(NfcErrorNone, garbage, TEXKOM_TK13_INTERVALS);
        test_callback_call_count = 0;
        const NfcCommand cmd = poller_base->run(event, instance);

        CHECK(cmd == NfcCommandContinue, "s5 unparseable capture -> Continue");
        CHECK(test_callback_call_count == 0, "s5 callback not invoked on decode failure");
        const TexkomData* data = (const TexkomData*)poller_base->get_data(instance);
        CHECK(memcmp(data, &seeded, sizeof(TexkomData)) == 0, "s5 previous decode left untouched");

        poller_base->free(instance);
    }

    // --- Scenario 6: NfcErrorTimeout short-circuits before the buffer is inspected,
    // even if the buffer still holds stale, validly-shaped bytes from an earlier call. ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);

        const uint8_t uid[TEXKOM_UID_SIZE] = {0xAA, 0xBB, 0xCC, 0xDD};
        uint8_t intervals[TEXKOM_TK13_INTERVALS];
        build_tk13_family_vector(TexkomTypeTk13, uid, TEXKOM_UID_SIZE, intervals);
        mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK13_INTERVALS);
        NfcEvent nfc_event;
        NfcGenericEvent event = make_poller_ready_event(&nfc_event);
        CHECK(poller_base->detect(event, instance), "s6 seed capture succeeds");
        TexkomData seeded = *(const TexkomData*)poller_base->get_data(instance);

        // The mock's own rx_buffer still holds `intervals` from the call just above (the real
        // nfc_texkom_poller_rx() would too, nothing clears it between calls) -- a validly TK13-
        // shaped, still-decodable byte sequence sitting right there. A timeout now must still
        // short-circuit on the error code alone, not fall through into decoding it again.
        mock_rx_program(NfcErrorTimeout, NULL, 0);
        test_callback_call_count = 0;
        const NfcCommand cmd = poller_base->run(event, instance);

        CHECK(cmd == NfcCommandContinue, "s6 timeout -> Continue");
        CHECK(test_callback_call_count == 0, "s6 callback not invoked on timeout");
        const TexkomData* data = (const TexkomData*)poller_base->get_data(instance);
        CHECK(memcmp(data, &seeded, sizeof(TexkomData)) == 0, "s6 stale buffer was not redecoded");

        poller_base->free(instance);
    }

    // --- Scenario 7: all four variants round-trip through the full run()->callback path. ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);
        NfcEvent nfc_event;
        NfcGenericEvent event = make_poller_ready_event(&nfc_event);

        // TK13 (clean separation).
        {
            const uint8_t uid[TEXKOM_UID_SIZE] = {0x10, 0x20, 0x30, 0x40};
            uint8_t intervals[TEXKOM_TK13_INTERVALS];
            build_tk13_family_vector(TexkomTypeTk13, uid, TEXKOM_UID_SIZE, intervals);
            mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK13_INTERVALS);
            test_callback_call_count = 0;
            test_callback_return_value = NfcCommandContinue;
            poller_base->run(event, instance);
            CHECK(test_callback_call_count == 1, "s7 TK13 reaches the callback");
            const TexkomData* d = (const TexkomData*)poller_base->get_data(instance);
            CHECK(d->type == TexkomTypeTk13, "s7 TK13 type");
        }
        // TK17.
        {
            const uint8_t uid[TEXKOM_UID_SIZE] = {0x50, 0x60, 0x70, 0x80};
            uint8_t intervals[TEXKOM_TK17_INTERVALS];
            build_tk17_vector(uid, intervals);
            mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK17_INTERVALS);
            test_callback_call_count = 0;
            poller_base->run(event, instance);
            CHECK(test_callback_call_count == 1, "s7 TK17 reaches the callback");
            const TexkomData* d = (const TexkomData*)poller_base->get_data(instance);
            CHECK(d->type == TexkomTypeTk17, "s7 TK17 type");
        }
        // MMBIT.
        {
            const uint8_t uid[TEXKOM_MMBIT_UID_SIZE] = {0x12, 0x34, 0x56};
            uint8_t intervals[TEXKOM_TK13_INTERVALS];
            build_tk13_family_vector(TexkomTypeMmbit, uid, TEXKOM_MMBIT_UID_SIZE, intervals);
            mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK13_INTERVALS);
            test_callback_call_count = 0;
            poller_base->run(event, instance);
            CHECK(test_callback_call_count == 1, "s7 MMBIT reaches the callback");
            const TexkomData* d = (const TexkomData*)poller_base->get_data(instance);
            CHECK(d->type == TexkomTypeMmbit, "s7 MMBIT type");
        }
        // TK15 coding -- same TK13-family wire encoding (the Poller always hands over 127
        // intervals for this family), but decode_tk13's own threshold fails on narrow+mixed
        // gaps and decode_intervals falls back to decode_tk15 (see repro.c scenario 2 for the
        // bit-level reasoning); here it is only checked that the Poller carries that
        // reassigned type through untouched.
        {
            TexkomData d;
            bool ok = texkom_make_blank(&d, TexkomTypeTk13);
            furi_assert(ok);
            const uint8_t uid[TEXKOM_UID_SIZE] = {0x99, 0x88, 0x77, 0x66};
            ok = texkom_set_uid(&d, uid, TEXKOM_UID_SIZE);
            furi_assert(ok);
            uint8_t bits[TEXKOM_FRAME_BITS];
            frame_to_bits(d.frame, bits);
            uint8_t raw[TEXKOM_TK13_RAW_BITS];
            for(size_t i = 0; i < TEXKOM_FRAME_BITS; i++) {
                raw[i * 2] = bits[i] ? 1 : 0;
                raw[i * 2 + 1] = bits[i] ? 0 : 1;
            }
            uint8_t intervals[TEXKOM_TK13_INTERVALS];
            for(size_t i = 0; i < TEXKOM_TK13_INTERVALS; i++) {
                const bool second_half = (i / 2) >= 32;
                const uint8_t hi = second_half ? 70 : 200;
                const uint8_t lo = second_half ? 55 : 50;
                intervals[i] = raw[i] ? hi : lo;
            }
            mock_rx_program(NfcErrorNone, intervals, TEXKOM_TK13_INTERVALS);
            test_callback_call_count = 0;
            poller_base->run(event, instance);
            CHECK(test_callback_call_count == 1, "s7 TK15-coding reaches the callback");
            const TexkomData* decoded = (const TexkomData*)poller_base->get_data(instance);
            CHECK(decoded->type == TexkomTypeTk15, "s7 TK15 reassignment carried through");
        }

        poller_base->free(instance);
    }

    // --- Scenario 8: general_event/texkom_event identity is stable across repeated
    // successful run() calls (wired once at alloc, never rebuilt per call). ---
    {
        const NfcPollerBase* poller_base = &nfc_poller_texkom;
        TexkomPoller* instance = (TexkomPoller*)poller_base->alloc(&nfc_hw);
        poller_base->set_callback(instance, test_callback, instance);
        NfcEvent nfc_event;
        NfcGenericEvent event = make_poller_ready_event(&nfc_event);

        const uint8_t uid_a[TEXKOM_UID_SIZE] = {0x01, 0x01, 0x01, 0x01};
        uint8_t intervals_a[TEXKOM_TK13_INTERVALS];
        build_tk13_family_vector(TexkomTypeTk13, uid_a, TEXKOM_UID_SIZE, intervals_a);
        mock_rx_program(NfcErrorNone, intervals_a, TEXKOM_TK13_INTERVALS);
        test_callback_call_count = 0;
        poller_base->run(event, instance);
        const NfcGenericEventData* event_data_first = test_callback_last_event.event_data;

        const uint8_t uid_b[TEXKOM_UID_SIZE] = {0x02, 0x02, 0x02, 0x02};
        uint8_t intervals_b[TEXKOM_TK13_INTERVALS];
        build_tk13_family_vector(TexkomTypeTk13, uid_b, TEXKOM_UID_SIZE, intervals_b);
        mock_rx_program(NfcErrorNone, intervals_b, TEXKOM_TK13_INTERVALS);
        poller_base->run(event, instance);
        const NfcGenericEventData* event_data_second = test_callback_last_event.event_data;

        CHECK(
            event_data_first == event_data_second,
            "s8 event_data pointer identity stable across repeated successful run() calls");
        CHECK(event_data_first == &instance->texkom_event, "s8 still points at texkom_event");

        poller_base->free(instance);
    }

    printf("%d checks passed -- ALL CHECKS PASSED\n", checks);
    return 0;
}
