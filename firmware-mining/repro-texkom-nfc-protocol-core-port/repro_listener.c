// Host repro for the Texkom NFC protocol-core port (mntm-012, API 87.1),
// Phase 3: texkom_listener.c/.h/_defs.h/_i.h (emulation state machine) and
// texkom_signal.c/.h (DigitalSequence preset that turns a frame into the
// on-air impulse/gap timeline). Both are transcribed unchanged below, split
// into two independently-mocked test groups because that is how the two
// files actually relate in this snapshot: texkom_listener_run() hands the
// generic nfc_listener_tx() a plain 8-byte buffer and never calls
// texkom_signal_tx() or passes a TexkomCoding anywhere -- the two meet only
// inside furi_hal_nfc_texkom.c (Phase 4, not yet ported). See the "Offen"
// note this produced in texkom-nfc-protocol-core-port.md.
//
// Group A -- texkom_signal_tx() differentially against the already
// host-verified Phase-1 decoder (texkom_decode_intervals(), transcribed
// again below unchanged, same technique repro_poller.c used for Phase 2):
// encode a frame, replay the exact gap timeline a real reader would
// measure, decode it back, and require a byte-exact match. The mock
// DigitalSignal/DigitalSequence pair records (level, raw unit count) per
// period instead of real GPIO timing -- texkom_signal.c always passes
// TEXKOM_SIGNAL_FC_64 * <small integer>, and the mock divides that scale
// back out, so the periods it actually adds are unchanged from the running
// firmware's. Measured gaps are simply the first TEXKOM_TK13_INTERVALS (or
// _TK17_) low-level periods the encoder produced, in order -- the same rule
// a real reader samples by (stop after the frame's known length), and the
// one explained in the "Fund" section below.
//
// Group B -- texkom_listener_run()/_alloc()/_free()/_set_callback()/
// _get_data() against a mocked Nfc/BitBuffer/nfc_listener_tx(), transcribed
// unchanged from texkom_listener.c/_i.h.
//
// Scenarios:
//  1. TK13 round trip, 8 UIDs (0x00..0x00, 0xFF..0xFF, and 6 mixed
//     patterns): make_blank+set_uid -> texkom_signal_tx(Tk13) -> first 127
//     low periods -> texkom_decode_intervals() -> frame and type
//     (TexkomTypeTk13, not Tk15 -- clean timings stay on the
//     absolute-threshold path) match the original exactly.
//  2. TK17 round trip, same 8 UIDs: texkom_signal_tx(Tk17) -> first 64 low
//     periods -> decode -> frame and type (TexkomTypeTk17) match.
//  3. MMBIT round trip, 8 three-byte UIDs: MMBIT rides the TK13 coding
//     (texkom_features[Mmbit].coding) -> texkom_signal_tx(Tk13) -> same
//     127-gap extraction -> decode -> frame and type (TexkomTypeMmbit)
//     match; set_uid()'s CRC update is included, so a trailing CRC byte
//     mismatch would also be caught.
//  4. Negative (validates the harness itself, not just the port): flip one
//     recorded gap's level in a TK13 encoding before decoding -- must make
//     texkom_decode_intervals() return false, and a second, unmodified
//     encode/decode right after must still succeed (no state leaked
//     between calls).
//  5. texkom_signal_alloc/_free lifecycle: two independent instances,
//     interleaved tx() calls on each, freed in reverse order -- ASan would
//     catch a leak, a double-free or any registered-signal use-after-free.
//  6. Listener alloc/free: nfc_config() is called exactly once, with
//     (NfcModeListener, NfcTechTexkom); get_data() returns the exact data
//     pointer passed to alloc(), unchanged.
//  7. Listener run() on NfcEventTypeListenerTick calls nfc_listener_tx()
//     exactly once per run() call with the 8 frame bytes byte-exact
//     (independent of variant/coding -- this layer never looks at either);
//     a non-tick event type calls it zero times; the return value is
//     NfcCommandContinue in both cases.
//  8. Listener run() still returns NfcCommandContinue and does not crash
//     when the mocked nfc_listener_tx() reports a transmit error -- matches
//     the source's FURI_LOG_E-and-continue handling, no retry/abort path to
//     miss.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#define UNUSED(x)   ((void)(x))

// --- lib/one_wire/maxim_crc.c, verbatim; confirmed byte-identical to
// Unleashed dev in the Phase-1 session, unchanged since. ---
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

// --- lib/nfc/protocols/texkom/texkom.h + texkom_i.h, transcribed (Phase 1,
// already delivered and host-verified; copied again as its own translation
// unit, not re-verified here -- same approach repro_poller.c used). ---
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
        }
    }

    if(type == TexkomTypeUnknown) return false;

    memcpy(data->frame, frame, TEXKOM_FRAME_SIZE);
    data->type = type;
    return true;
}

// --- lib/digital_signal/presets/nfc/texkom_signal.c + .h, transcribed
// unchanged (Phase 3, this session). GpioPin/DigitalSignal/DigitalSequence
// below are host mocks replacing furi_hal_resources.h /
// digital_signal/digital_sequence.h -- everything from the _Static_assert
// down to texkom_signal_tx() is the real file's logic untouched. ---

typedef struct GpioPin {
    int unused;
} GpioPin;

#define TEXKOM_SIGNAL_MAX_PERIODS (8U)

typedef struct {
    bool level;
    uint32_t units; // raw pre-FC_64-scaling unit count, recovered by the mock add-period below
} MockPeriod;

typedef struct {
    MockPeriod periods[TEXKOM_SIGNAL_MAX_PERIODS];
    size_t count;
} DigitalSignal;

#define DIGITAL_SEQUENCE_MAX_SIGNALS (8U)
#define DIGITAL_SEQUENCE_MAX_PROGRAM (256U)

typedef struct {
    DigitalSignal* registered[DIGITAL_SEQUENCE_MAX_SIGNALS];
    size_t program[DIGITAL_SEQUENCE_MAX_PROGRAM];
    size_t program_len;
    const GpioPin* pin;
} DigitalSequence;

static DigitalSignal* digital_signal_alloc(uint32_t max_periods) {
    UNUSED(max_periods);
    DigitalSignal* signal = calloc(1, sizeof(DigitalSignal));
    assert(signal);
    return signal;
}

static void digital_signal_free(DigitalSignal* signal) {
    free(signal);
}

#define TEXKOM_SIGNAL_FC    (13.56e6)
#define TEXKOM_SIGNAL_FC_64 (64.0e11 / TEXKOM_SIGNAL_FC)

static void
    digital_signal_add_period_with_level(DigitalSignal* signal, float duration, bool level) {
    assert(signal->count < COUNT_OF(signal->periods));
    // Recover the raw unit count texkom_signal.c multiplied by TEXKOM_SIGNAL_FC_64 -- the mock
    // cares about the ratios between gaps (what the decoder judges), not the real GPIO timing.
    const double raw = (double)duration / TEXKOM_SIGNAL_FC_64;
    const uint32_t units = (uint32_t)(raw + 0.5);
    assert(units > 0 || duration == 0.0f);
    signal->periods[signal->count].level = level;
    signal->periods[signal->count].units = units;
    signal->count++;
}

static DigitalSequence* digital_sequence_alloc(uint32_t max_periods, const GpioPin* pin) {
    UNUSED(max_periods);
    assert(pin);
    DigitalSequence* sequence = calloc(1, sizeof(DigitalSequence));
    assert(sequence);
    sequence->pin = pin;
    return sequence;
}

static void digital_sequence_free(DigitalSequence* sequence) {
    free(sequence);
}

static void
    digital_sequence_register_signal(DigitalSequence* sequence, size_t index, DigitalSignal* signal) {
    assert(index < COUNT_OF(sequence->registered));
    sequence->registered[index] = signal;
}

static void digital_sequence_clear(DigitalSequence* sequence) {
    sequence->program_len = 0;
}

static void digital_sequence_add_signal(DigitalSequence* sequence, size_t index) {
    assert(sequence->program_len < COUNT_OF(sequence->program));
    assert(sequence->registered[index]);
    sequence->program[sequence->program_len++] = index;
}

static void digital_sequence_transmit(DigitalSequence* sequence) {
    // Real transmission is the one thing this mock cannot and does not need to do -- the test
    // harness below walks sequence->program/registered itself once this returns.
    UNUSED(sequence);
}

#define TEXKOM_SIGNAL_FRAME_SIZE (8U)

typedef enum {
    TexkomSignalCodingTk13,
    TexkomSignalCodingTk17,
    TexkomSignalCodingNum,
} TexkomSignalCoding;

typedef struct TexkomSignal TexkomSignal;

_Static_assert(
    TEXKOM_SIGNAL_FRAME_SIZE == TEXKOM_FRAME_SIZE,
    "Texkom frame size disagrees with the protocol layer");
_Static_assert(
    (int)TexkomSignalCodingTk13 == (int)TexkomCodingTk13 &&
        (int)TexkomSignalCodingTk17 == (int)TexkomCodingTk17,
    "Texkom coding values disagree with the protocol layer");

#define BITS_IN_BYTE (8U)

#define TEXKOM_SIGNAL_IMPULSE (1U)

#define TEXKOM_SIGNAL_TK13_LONG  (17U)
#define TEXKOM_SIGNAL_TK13_SHORT (7U)

static const uint8_t texkom_signal_tk17_lows[4][2] = {
    {25, 5},
    {12, 18},
    {17, 13},
    {7, 23},
};

#define TEXKOM_SIGNAL_TAIL (10U)

#define TEXKOM_SIGNAL_SYMBOL_PERIODS (4U)
#define TEXKOM_SIGNAL_BUFFER_SIZE    (TEXKOM_SIGNAL_FRAME_SIZE * BITS_IN_BYTE + 1)

typedef enum {
    TexkomSignalIndexTk13Zero,
    TexkomSignalIndexTk13One,
    TexkomSignalIndexTk13Tail,
    TexkomSignalIndexTk17Sym00,
    TexkomSignalIndexTk17Sym01,
    TexkomSignalIndexTk17Sym10,
    TexkomSignalIndexTk17Sym11,
    TexkomSignalIndexTk17Tail,
    TexkomSignalIndexNum,
} TexkomSignalIndex;

struct TexkomSignal {
    DigitalSequence* tx_sequence;
    DigitalSignal* signals[TexkomSignalIndexNum];
};

static void
    texkom_signal_add_symbol(DigitalSignal* signal, uint32_t first_low, uint32_t second_low) {
    digital_signal_add_period_with_level(
        signal, TEXKOM_SIGNAL_FC_64 * TEXKOM_SIGNAL_IMPULSE, true);
    digital_signal_add_period_with_level(signal, TEXKOM_SIGNAL_FC_64 * first_low, false);
    digital_signal_add_period_with_level(
        signal, TEXKOM_SIGNAL_FC_64 * TEXKOM_SIGNAL_IMPULSE, true);
    digital_signal_add_period_with_level(signal, TEXKOM_SIGNAL_FC_64 * second_low, false);
}

static void texkom_signal_add_park(DigitalSignal* signal) {
    digital_signal_add_period_with_level(signal, TEXKOM_SIGNAL_FC_64 * TEXKOM_SIGNAL_TAIL, false);
}

static void texkom_signal_fill(TexkomSignal* instance) {
    DigitalSignal** signals = instance->signals;

    for(size_t i = 0; i < TexkomSignalIndexNum; i++) {
        signals[i] = digital_signal_alloc(TEXKOM_SIGNAL_SYMBOL_PERIODS);
        digital_sequence_register_signal(instance->tx_sequence, i, signals[i]);
    }

    texkom_signal_add_symbol(
        signals[TexkomSignalIndexTk13Zero], TEXKOM_SIGNAL_TK13_SHORT, TEXKOM_SIGNAL_TK13_LONG);
    texkom_signal_add_symbol(
        signals[TexkomSignalIndexTk13One], TEXKOM_SIGNAL_TK13_LONG, TEXKOM_SIGNAL_TK13_SHORT);

    for(size_t value = 0; value < COUNT_OF(texkom_signal_tk17_lows); value++) {
        texkom_signal_add_symbol(
            signals[TexkomSignalIndexTk17Sym00 + value],
            texkom_signal_tk17_lows[value][0],
            texkom_signal_tk17_lows[value][1]);
    }

    texkom_signal_add_park(signals[TexkomSignalIndexTk13Tail]);
    digital_signal_add_period_with_level(
        signals[TexkomSignalIndexTk17Tail], TEXKOM_SIGNAL_FC_64 * TEXKOM_SIGNAL_IMPULSE, true);
    texkom_signal_add_park(signals[TexkomSignalIndexTk17Tail]);
}

static TexkomSignal* texkom_signal_alloc(const GpioPin* pin) {
    assert(pin);
    TexkomSignal* instance = calloc(1, sizeof(TexkomSignal));
    assert(instance);
    instance->tx_sequence = digital_sequence_alloc(TEXKOM_SIGNAL_BUFFER_SIZE, pin);
    texkom_signal_fill(instance);
    return instance;
}

static void texkom_signal_free(TexkomSignal* instance) {
    assert(instance);
    digital_sequence_free(instance->tx_sequence);
    for(size_t i = 0; i < TexkomSignalIndexNum; i++) {
        digital_signal_free(instance->signals[i]);
    }
    free(instance);
}

static void texkom_signal_encode_tk13(TexkomSignal* instance, const uint8_t* frame) {
    for(size_t i = 0; i < TEXKOM_SIGNAL_FRAME_SIZE; i++) {
        for(size_t bit = 0; bit < BITS_IN_BYTE; bit++) {
            const bool one = (frame[i] << bit) & 0x80;
            digital_sequence_add_signal(
                instance->tx_sequence, one ? TexkomSignalIndexTk13One : TexkomSignalIndexTk13Zero);
        }
    }
    digital_sequence_add_signal(instance->tx_sequence, TexkomSignalIndexTk13Tail);
}

static void texkom_signal_encode_tk17(TexkomSignal* instance, const uint8_t* frame) {
    for(size_t i = 0; i < TEXKOM_SIGNAL_FRAME_SIZE; i++) {
        for(size_t bit = 0; bit < BITS_IN_BYTE; bit += 2) {
            digital_sequence_add_signal(
                instance->tx_sequence, TexkomSignalIndexTk17Sym00 + ((frame[i] >> bit) & 0x03));
        }
    }
    digital_sequence_add_signal(instance->tx_sequence, TexkomSignalIndexTk17Tail);
}

static void texkom_signal_tx(TexkomSignal* instance, TexkomSignalCoding coding, const uint8_t* frame) {
    assert(instance);
    assert(coding < TexkomSignalCodingNum);
    assert(frame);

    digital_sequence_clear(instance->tx_sequence);

    if(coding == TexkomSignalCodingTk17) {
        texkom_signal_encode_tk17(instance, frame);
    } else {
        texkom_signal_encode_tk13(instance, frame);
    }

    digital_sequence_transmit(instance->tx_sequence);
}

// --- Test-harness-only helper: walk the sequence just transmitted and pull
// out the ordered list of low-level period lengths -- what a real reader's
// capture would hand to texkom_decode_intervals(). Not part of any ported
// file. ---
static size_t texkom_signal_extract_gaps(const TexkomSignal* instance, uint8_t* out, size_t max) {
    size_t n = 0;
    const DigitalSequence* seq = instance->tx_sequence;
    for(size_t p = 0; p < seq->program_len; p++) {
        const DigitalSignal* sig = seq->registered[seq->program[p]];
        for(size_t i = 0; i < sig->count; i++) {
            if(!sig->periods[i].level) {
                assert(n < max);
                assert(sig->periods[i].units <= UINT8_MAX);
                out[n++] = (uint8_t)sig->periods[i].units;
            }
        }
    }
    return n;
}

// --- lib/nfc/protocols/texkom/texkom_listener.c/.h/_defs.h/_i.h,
// transcribed unchanged (Phase 3, this session). Mocked Nfc/BitBuffer/
// NfcEvent/nfc_listener_tx()/nfc_config() below stand in for nfc/nfc.h,
// nfc/nfc_listener.h, nfc/protocols/nfc_listener_base.h. ---

typedef enum { NfcErrorNone, NfcErrorTimeout } NfcError;
typedef enum { NfcModeListener } NfcMode;
typedef enum { NfcTechTexkom } NfcTech;
typedef enum { NfcEventTypeListenerTick, NfcEventTypeOther } NfcEventType;
typedef enum { NfcCommandContinue } NfcCommand;

typedef struct {
    NfcEventType type;
} NfcEvent;

typedef struct {
    void* protocol;
    void* instance;
    void* event_data;
} NfcGenericEvent;

typedef NfcCommand (*NfcGenericCallback)(NfcGenericEvent event, void* context);

typedef struct {
    uint8_t bytes[TEXKOM_FRAME_SIZE];
    size_t len;
} BitBuffer;

static BitBuffer* bit_buffer_alloc(size_t capacity) {
    UNUSED(capacity);
    BitBuffer* buffer = calloc(1, sizeof(BitBuffer));
    assert(buffer);
    return buffer;
}

static void bit_buffer_free(BitBuffer* buffer) {
    free(buffer);
}

static void bit_buffer_copy_bytes(BitBuffer* buffer, const uint8_t* data, size_t len) {
    assert(len <= sizeof(buffer->bytes));
    memcpy(buffer->bytes, data, len);
    buffer->len = len;
}

// Mock Nfc object: records what nfc_config()/nfc_listener_tx() were called with, and what the
// latter should report back, for the test to inspect/control.
typedef struct {
    NfcMode configured_mode;
    NfcTech configured_tech;
    size_t config_calls;

    NfcError tx_result;
    size_t tx_calls;
    uint8_t last_tx_bytes[TEXKOM_FRAME_SIZE];
    size_t last_tx_len;
} Nfc;

static void nfc_config(Nfc* nfc, NfcMode mode, NfcTech tech) {
    nfc->configured_mode = mode;
    nfc->configured_tech = tech;
    nfc->config_calls++;
}

static NfcError nfc_listener_tx(Nfc* nfc, const BitBuffer* buffer) {
    nfc->tx_calls++;
    memcpy(nfc->last_tx_bytes, buffer->bytes, buffer->len);
    nfc->last_tx_len = buffer->len;
    return nfc->tx_result;
}

#define FURI_LOG_E(tag, fmt, ...) ((void)0)
#define TAG                       "TexkomListener"

typedef struct TexkomListener {
    Nfc* nfc;
    TexkomData* data;
    BitBuffer* tx_buffer;

    NfcGenericCallback callback;
    void* context;
} TexkomListener;

static TexkomListener* texkom_listener_alloc(Nfc* nfc, TexkomData* data) {
    assert(nfc);
    assert(data);

    TexkomListener* instance = malloc(sizeof(TexkomListener));
    instance->nfc = nfc;
    instance->data = data;
    instance->tx_buffer = bit_buffer_alloc(TEXKOM_FRAME_SIZE);

    nfc_config(instance->nfc, NfcModeListener, NfcTechTexkom);

    return instance;
}

static void texkom_listener_free(TexkomListener* instance) {
    assert(instance);
    bit_buffer_free(instance->tx_buffer);
    free(instance);
}

static void texkom_listener_set_callback(
    TexkomListener* instance,
    NfcGenericCallback callback,
    void* context) {
    assert(instance);
    instance->callback = callback;
    instance->context = context;
}

static const TexkomData* texkom_listener_get_data(const TexkomListener* instance) {
    assert(instance);
    assert(instance->data);
    return instance->data;
}

static NfcCommand texkom_listener_run(NfcGenericEvent event, void* context) {
    assert(context);
    assert(event.event_data);

    TexkomListener* instance = context;
    const NfcEvent* nfc_event = event.event_data;

    if(nfc_event->type == NfcEventTypeListenerTick) {
        bit_buffer_copy_bytes(instance->tx_buffer, instance->data->frame, TEXKOM_FRAME_SIZE);

        const NfcError error = nfc_listener_tx(instance->nfc, instance->tx_buffer);
        if(error != NfcErrorNone) {
            FURI_LOG_E(TAG, "Failed to transmit the frame: %d", error);
        }
    }

    return NfcCommandContinue;
}

// =========================== test harness ===========================

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        checks_run++;                                                          \
        if(!(cond)) {                                                          \
            checks_failed++;                                                   \
            fprintf(stderr, "CHECK FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        }                                                                       \
    } while(0)

static void sweep_uids(TexkomType type, TexkomSignalCoding coding, size_t uid_len) {
    // 8 UIDs: all-zero, all-0xFF, and 6 mixed patterns -- chosen to cover a UID with and
    // without the preamble-adjacent bit pattern repeated, not to be an exhaustive sweep.
    static const uint8_t pool[8][4] = {
        {0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF},
        {0x01, 0x23, 0x45, 0x67},
        {0xDE, 0xAD, 0xBE, 0xEF},
        {0xA5, 0x5A, 0xA5, 0x5A},
        {0x80, 0x40, 0x20, 0x10},
        {0x7F, 0x3F, 0x1F, 0x0F},
        {0x13, 0x37, 0xC0, 0xDE},
    };

    for(size_t u = 0; u < COUNT_OF(pool); u++) {
        TexkomData original = {0};
        CHECK(texkom_make_blank(&original, type));
        CHECK(texkom_set_uid(&original, pool[u], uid_len));

        TexkomSignal* signal = texkom_signal_alloc((const GpioPin*)1);
        texkom_signal_tx(signal, coding, original.frame);

        const size_t interval_count =
            (coding == TexkomSignalCodingTk17) ? TEXKOM_TK17_INTERVALS : TEXKOM_TK13_INTERVALS;
        uint8_t gaps[260];
        const size_t extracted = texkom_signal_extract_gaps(signal, gaps, COUNT_OF(gaps));
        CHECK(extracted >= interval_count);

        TexkomData decoded = {0};
        CHECK(texkom_decode_intervals(&decoded, gaps, interval_count));
        CHECK(decoded.type == type);
        CHECK(memcmp(decoded.frame, original.frame, TEXKOM_FRAME_SIZE) == 0);

        texkom_signal_free(signal);
    }
}

int main(void) {
    // Scenario 1: TK13 round trip.
    sweep_uids(TexkomTypeTk13, TexkomSignalCodingTk13, TEXKOM_UID_SIZE);

    // Scenario 2: TK17 round trip.
    sweep_uids(TexkomTypeTk17, TexkomSignalCodingTk17, TEXKOM_UID_SIZE);

    // Scenario 3: MMBIT round trip (TK13 coding, 3-byte UID).
    sweep_uids(TexkomTypeMmbit, TexkomSignalCodingTk13, TEXKOM_MMBIT_UID_SIZE);

    // Scenario 4: negative -- flip one gap's level before decoding, then prove the next,
    // unmodified round trip is unaffected.
    {
        TexkomData original = {0};
        CHECK(texkom_make_blank(&original, TexkomTypeTk13));
        const uint8_t uid[4] = {0x11, 0x22, 0x33, 0x44};
        CHECK(texkom_set_uid(&original, uid, sizeof(uid)));

        TexkomSignal* signal = texkom_signal_alloc((const GpioPin*)1);
        texkom_signal_tx(signal, TexkomSignalCodingTk13, original.frame);

        uint8_t gaps[260];
        size_t extracted = texkom_signal_extract_gaps(signal, gaps, COUNT_OF(gaps));
        CHECK(extracted >= TEXKOM_TK13_INTERVALS);

        uint8_t corrupted[TEXKOM_TK13_INTERVALS];
        memcpy(corrupted, gaps, TEXKOM_TK13_INTERVALS);
        corrupted[0] = (uint8_t)(TEXKOM_SIGNAL_TK13_SHORT + TEXKOM_SIGNAL_TK13_LONG) / 2; // ambiguous value, neither threshold

        TexkomData decoded_bad = {0};
        CHECK(!texkom_decode_intervals(&decoded_bad, corrupted, TEXKOM_TK13_INTERVALS));

        TexkomData decoded_good = {0};
        CHECK(texkom_decode_intervals(&decoded_good, gaps, TEXKOM_TK13_INTERVALS));
        CHECK(decoded_good.type == TexkomTypeTk13);
        CHECK(memcmp(decoded_good.frame, original.frame, TEXKOM_FRAME_SIZE) == 0);

        texkom_signal_free(signal);
    }

    // Scenario 5: alloc/free lifecycle, two interleaved instances.
    {
        TexkomData a = {0}, b = {0};
        CHECK(texkom_make_blank(&a, TexkomTypeTk13));
        CHECK(texkom_make_blank(&b, TexkomTypeTk17));
        const uint8_t uid_a[4] = {1, 2, 3, 4};
        const uint8_t uid_b[4] = {5, 6, 7, 8};
        CHECK(texkom_set_uid(&a, uid_a, sizeof(uid_a)));
        CHECK(texkom_set_uid(&b, uid_b, sizeof(uid_b)));

        TexkomSignal* sig_a = texkom_signal_alloc((const GpioPin*)1);
        TexkomSignal* sig_b = texkom_signal_alloc((const GpioPin*)2);

        texkom_signal_tx(sig_a, TexkomSignalCodingTk13, a.frame);
        texkom_signal_tx(sig_b, TexkomSignalCodingTk17, b.frame);
        texkom_signal_tx(sig_a, TexkomSignalCodingTk13, a.frame); // re-tx on the same instance

        uint8_t gaps_a[260];
        size_t extracted_a = texkom_signal_extract_gaps(sig_a, gaps_a, COUNT_OF(gaps_a));
        CHECK(extracted_a >= TEXKOM_TK13_INTERVALS);
        TexkomData decoded_a = {0};
        CHECK(texkom_decode_intervals(&decoded_a, gaps_a, TEXKOM_TK13_INTERVALS));
        CHECK(memcmp(decoded_a.frame, a.frame, TEXKOM_FRAME_SIZE) == 0);

        texkom_signal_free(sig_b);
        texkom_signal_free(sig_a);
    }

    // Scenario 6: listener alloc/free, nfc_config() call, get_data() identity.
    {
        TexkomData data = {0};
        CHECK(texkom_make_blank(&data, TexkomTypeTk13));
        Nfc nfc = {0};

        TexkomListener* listener = texkom_listener_alloc(&nfc, &data);
        CHECK(nfc.config_calls == 1);
        CHECK(nfc.configured_mode == NfcModeListener);
        CHECK(nfc.configured_tech == NfcTechTexkom);
        CHECK(texkom_listener_get_data(listener) == &data);

        int dummy_context = 42;
        texkom_listener_set_callback(listener, texkom_listener_run, &dummy_context);
        CHECK(listener->callback == texkom_listener_run);
        CHECK(listener->context == &dummy_context);

        texkom_listener_free(listener);
    }

    // Scenario 7: run() on a tick transmits the exact frame bytes exactly once; a non-tick
    // event transmits zero times.
    {
        TexkomData data = {0};
        CHECK(texkom_make_blank(&data, TexkomTypeMmbit));
        const uint8_t uid[3] = {0xAA, 0xBB, 0xCC};
        CHECK(texkom_set_uid(&data, uid, sizeof(uid)));
        Nfc nfc = {0};
        nfc.tx_result = NfcErrorNone;

        TexkomListener* listener = texkom_listener_alloc(&nfc, &data);

        NfcEvent tick_event = {.type = NfcEventTypeListenerTick};
        NfcGenericEvent generic_tick = {.protocol = NULL, .instance = NULL, .event_data = &tick_event};
        NfcCommand cmd = texkom_listener_run(generic_tick, listener);
        CHECK(cmd == NfcCommandContinue);
        CHECK(nfc.tx_calls == 1);
        CHECK(nfc.last_tx_len == TEXKOM_FRAME_SIZE);
        CHECK(memcmp(nfc.last_tx_bytes, data.frame, TEXKOM_FRAME_SIZE) == 0);

        NfcEvent other_event = {.type = NfcEventTypeOther};
        NfcGenericEvent generic_other = {.protocol = NULL, .instance = NULL, .event_data = &other_event};
        cmd = texkom_listener_run(generic_other, listener);
        CHECK(cmd == NfcCommandContinue);
        CHECK(nfc.tx_calls == 1); // unchanged

        texkom_listener_free(listener);
    }

    // Scenario 8: run() survives a reported transmit error without changing its return value.
    {
        TexkomData data = {0};
        CHECK(texkom_make_blank(&data, TexkomTypeTk17));
        const uint8_t uid[4] = {9, 9, 9, 9};
        CHECK(texkom_set_uid(&data, uid, sizeof(uid)));
        Nfc nfc = {0};
        nfc.tx_result = NfcErrorTimeout;

        TexkomListener* listener = texkom_listener_alloc(&nfc, &data);

        NfcEvent tick_event = {.type = NfcEventTypeListenerTick};
        NfcGenericEvent generic_tick = {.protocol = NULL, .instance = NULL, .event_data = &tick_event};
        NfcCommand cmd = texkom_listener_run(generic_tick, listener);
        CHECK(cmd == NfcCommandContinue);
        CHECK(nfc.tx_calls == 1);
        CHECK(memcmp(nfc.last_tx_bytes, data.frame, TEXKOM_FRAME_SIZE) == 0);

        texkom_listener_free(listener);
    }

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
