// Host-Repro: Indala224-LFRFID-Protokoll-Port (Unleashed dev -> mntm-012-Basis).
//
// protocol_indala224.{c,h} und lfrfid_write_fill.{c,h} sind unveraendert aus Unleashed
// transkribiert (nur furi.h/FuriString/ProtocolBase/t5577.h-Abhaengigkeiten auf das fuer
// diesen Host-Test notwendige Minimum reduziert, keine eigene Logik geaendert). bit_lib_*
// ist unveraendert aus der eigenen Basis (lib/bit_lib/bit_lib.c) transkribiert.
//
// Testvektoren (INDALA224_TEST_DATA, INDALA224_ALT_TEST_DATA) sind die von Unleashed selbst
// mitgelieferten, laut Kommentar Proxmark3-verifizierten Referenzwerte
// (applications/debug/unit_tests/tests/lfrfid/lfrfid_protocols.c).

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

// ---- Minimal-Stubs (furi.h/FuriString) ----

#define FURI_LOG_D(...) ((void)0)

typedef struct {
    char buf[256];
} FuriString;

static void furi_string_printf(FuriString* s, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(s->buf, sizeof(s->buf), fmt, args);
    va_end(args);
}

// ---- lib/toolbox/level_duration.h (unveraendert, LEVEL_DURATION_BIG-Zweig) ----

#define LEVEL_DURATION_RESET      0U
#define LEVEL_DURATION_LEVEL_LOW  1U
#define LEVEL_DURATION_LEVEL_HIGH 2U
#define LEVEL_DURATION_WAIT       3U

typedef struct {
    uint32_t duration : 30;
    uint8_t level     : 2;
} LevelDuration;

static inline LevelDuration level_duration_make(bool level, uint32_t duration) {
    LevelDuration level_duration;
    level_duration.level = level ? LEVEL_DURATION_LEVEL_HIGH : LEVEL_DURATION_LEVEL_LOW;
    level_duration.duration = duration;
    return level_duration;
}

static inline bool level_duration_get_level(LevelDuration level_duration) {
    return level_duration.level == LEVEL_DURATION_LEVEL_HIGH;
}

// ---- lib/toolbox/protocols/protocol.h (unveraendert, fuer den Struct-Literal am Dateiende) ----

typedef void* (*ProtocolAlloc)(void);
typedef void (*ProtocolFree)(void* protocol);
typedef uint8_t* (*ProtocolGetData)(void* protocol);
typedef void (*ProtocolDecoderStart)(void* protocol);
typedef bool (*ProtocolDecoderFeed)(void* protocol, bool level, uint32_t duration);
typedef bool (*ProtocolEncoderStart)(void* protocol);
typedef LevelDuration (*ProtocolEncoderYield)(void* protocol);
typedef void (*ProtocolRenderData)(void* protocol, FuriString* result);
typedef bool (*ProtocolWriteData)(void* protocol, void* data);

typedef struct {
    ProtocolDecoderStart start;
    ProtocolDecoderFeed feed;
} ProtocolDecoder;

typedef struct {
    ProtocolEncoderStart start;
    ProtocolEncoderYield yield;
} ProtocolEncoder;

typedef struct {
    const size_t data_size;
    const char* name;
    const char* manufacturer;
    const uint32_t features;
    const uint8_t validate_count;

    ProtocolAlloc alloc;
    ProtocolFree free;
    ProtocolGetData get_data;
    ProtocolDecoder decoder;
    ProtocolEncoder encoder;
    ProtocolRenderData render_uid;
    ProtocolRenderData render_data;
    ProtocolRenderData render_brief_data;
    ProtocolWriteData write_data;
} ProtocolBase;

// ---- lib/bit_lib/bit_lib.c (unveraendert, nur die von protocol_indala224.c/lfrfid_write_fill.c
// tatsaechlich benutzten Funktionen) ----

#define bit_lib_increment_index(index, length) (index = (((index) + 1) % (length)))

void bit_lib_push_bit(uint8_t* data, size_t data_size, bool bit) {
    size_t last_index = data_size - 1;
    for(size_t i = 0; i < last_index; ++i) {
        data[i] = (data[i] << 1) | ((data[i + 1] >> 7) & 1);
    }
    data[last_index] = (data[last_index] << 1) | bit;
}

bool bit_lib_get_bit(const uint8_t* data, size_t position) {
    return (data[position / 8] >> (7 - (position % 8))) & 1;
}

void bit_lib_set_bit(uint8_t* data, size_t position, bool bit) {
    if(bit) {
        data[position / 8] |= 1UL << (7 - (position % 8));
    } else {
        data[position / 8] &= ~(1UL << (7 - (position % 8)));
    }
}

uint8_t bit_lib_get_bits(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else {
        // TODO FL-3534: fix read out of bounds (eigene Basis, unveraendert -- fuer diesen
        // Port irrelevant, siehe .md "Risiko": alle Aufrufstellen hier sind byte-ausgerichtet).
        uint8_t value = (data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift);
        value = value >> (8 - length);
        return value;
    }
}

uint32_t bit_lib_get_bits_32(const uint8_t* data, size_t position, uint8_t length) {
    uint32_t value = 0;
    if(length <= 8) {
        value = bit_lib_get_bits(data, position, length);
    } else if(length <= 16) {
        value = bit_lib_get_bits(data, position, 8) << (length - 8);
        value |= bit_lib_get_bits(data, position + 8, length - 8);
    } else if(length <= 24) {
        value = bit_lib_get_bits(data, position, 8) << (length - 8);
        value |= bit_lib_get_bits(data, position + 8, 8) << (length - 16);
        value |= bit_lib_get_bits(data, position + 16, length - 16);
    } else {
        value = (uint32_t)bit_lib_get_bits(data, position, 8) << (length - 8);
        value |= (uint32_t)bit_lib_get_bits(data, position + 8, 8) << (length - 16);
        value |= (uint32_t)bit_lib_get_bits(data, position + 16, 8) << (length - 24);
        value |= bit_lib_get_bits(data, position + 24, length - 24);
    }
    return value;
}

// ---- lib/lfrfid/tools/t5577.h (unveraendert, nur die hier benoetigten Konstanten) ----

#define LFRFID_T5577_BLOCK_COUNT    8
#define LFRFID_T5577_MAXBLOCK_SHIFT 5
#define LFRFID_T5577_MODULATION_PSK2 0x00002000
#define LFRFID_T5577_BITRATE_RF_32   0x00080000

typedef struct {
    uint32_t block[LFRFID_T5577_BLOCK_COUNT];
    uint32_t blocks_to_write;
    uint8_t mask;
} LFRFIDT5577;

// ---- lib/lfrfid/protocols/lfrfid_protocols.h (unveraendert, nur der Write-Request-Teil) ----

typedef enum {
    LFRFIDWriteTypeT5577,
    LFRFIDWriteTypeMax,
} LFRFIDWriteType;

typedef struct {
    LFRFIDWriteType write_type;
    union {
        LFRFIDT5577 t5577;
    };
} LFRFIDWriteRequest;

#define LFRFIDFeaturePSK (1 << 1)

// ---- lib/lfrfid/protocols/lfrfid_write_fill.c (unveraendert, nur die T5577-Haelfte -- die
// EM4305-Haelfte wird von protocol_indala224.c nicht benutzt) ----

#define LFRFID_T5577_MAXBLOCK_FIELD_MASK (0x7)

void lfrfid_write_fill_t5577(LFRFIDWriteRequest* request, uint32_t block0, const uint8_t* bits) {
    const uint8_t blocks = (block0 >> LFRFID_T5577_MAXBLOCK_SHIFT) &
                            LFRFID_T5577_MAXBLOCK_FIELD_MASK;

    request->t5577.block[0] = block0;

    for(uint8_t block = 1; block <= blocks; block++) {
        request->t5577.block[block] = bit_lib_get_bits_32(bits, (block - 1) * 32, 32);
    }

    request->t5577.blocks_to_write = blocks + 1;
}

// ======================================================================================
// lib/lfrfid/protocols/protocol_indala224.c -- unveraendert transkribiert (Unleashed dev,
// Commit a70a69d9d47c1e0a3b1f868aa25c4d0a7a5346d5, 2026-10-20).
// ======================================================================================

#define INDALA224_PREAMBLE_BIT_SIZE  (30)
#define INDALA224_PREAMBLE_DATA_SIZE (4)

#define INDALA224_ENCODED_BIT_SIZE (224)
#define INDALA224_ENCODED_DATA_SIZE \
    (((INDALA224_ENCODED_BIT_SIZE) / 8) + INDALA224_PREAMBLE_DATA_SIZE)
#define INDALA224_ENCODED_DATA_LAST ((INDALA224_ENCODED_BIT_SIZE) / 8)

#define INDALA224_DECODED_BIT_SIZE  (224)
#define INDALA224_DECODED_DATA_SIZE (28)

#define INDALA224_US_PER_BIT             (255)
#define INDALA224_ENCODER_PULSES_PER_BIT (16)

typedef struct {
    uint8_t data_index;
    uint8_t bit_clock_index;
    bool current_polarity;
    bool pulse_phase;
} ProtocolIndala224Encoder;

typedef struct {
    uint8_t encoded_data[INDALA224_ENCODED_DATA_SIZE];
    uint8_t negative_encoded_data[INDALA224_ENCODED_DATA_SIZE];

    uint8_t data[INDALA224_DECODED_DATA_SIZE];
    ProtocolIndala224Encoder encoder;
} ProtocolIndala224;

ProtocolIndala224* protocol_indala224_alloc(void) {
    ProtocolIndala224* protocol = malloc(sizeof(ProtocolIndala224));
    return protocol;
}

void protocol_indala224_free(ProtocolIndala224* protocol) {
    free(protocol);
}

uint8_t* protocol_indala224_get_data(ProtocolIndala224* protocol) {
    return protocol->data;
}

void protocol_indala224_decoder_start(ProtocolIndala224* protocol) {
    memset(protocol->encoded_data, 0, INDALA224_ENCODED_DATA_SIZE);
    memset(protocol->negative_encoded_data, 0, INDALA224_ENCODED_DATA_SIZE);
}

static bool protocol_indala224_check_preamble(uint8_t* data, size_t bit_index) {
    // Normal preamble: 1 followed by 29 zeros
    if(bit_lib_get_bits(data, bit_index, 8) != 0b10000000) return false;
    if(bit_lib_get_bits(data, bit_index + 8, 8) != 0) return false;
    if(bit_lib_get_bits(data, bit_index + 16, 8) != 0) return false;
    if(bit_lib_get_bits(data, bit_index + 24, 6) != 0) return false;
    return true;
}

static bool protocol_indala224_check_inverted_preamble(uint8_t* data, size_t bit_index) {
    // Inverted preamble: 0 followed by 29 ones (phase-alternating PSK2 cards)
    if(bit_lib_get_bits(data, bit_index, 8) != 0b01111111) return false;
    if(bit_lib_get_bits(data, bit_index + 8, 8) != 0xFF) return false;
    if(bit_lib_get_bits(data, bit_index + 16, 8) != 0xFF) return false;
    if(bit_lib_get_bits(data, bit_index + 24, 6) != 0b111111) return false;
    return true;
}

static bool protocol_indala224_can_be_decoded(uint8_t* data) {
    if(!protocol_indala224_check_preamble(data, 0)) return false;
    // Second frame may have same or inverted preamble (PSK2 phase alternation)
    if(!protocol_indala224_check_preamble(data, INDALA224_ENCODED_BIT_SIZE) &&
       !protocol_indala224_check_inverted_preamble(data, INDALA224_ENCODED_BIT_SIZE)) {
        return false;
    }
    return true;
}

static bool protocol_indala224_decoder_feed_internal(bool polarity, uint32_t time, uint8_t* data) {
    time += (INDALA224_US_PER_BIT / 2);

    size_t bit_count = (time / INDALA224_US_PER_BIT);
    bool result = false;

    // A payload of all zeros holds the PSK2 carrier phase for a whole frame, so a run of
    // exactly INDALA224_ENCODED_BIT_SIZE bits is legitimate and must not be discarded.
    if(bit_count <= INDALA224_ENCODED_BIT_SIZE) {
        for(size_t i = 0; i < bit_count; i++) {
            bit_lib_push_bit(data, INDALA224_ENCODED_DATA_SIZE, polarity);
            if(protocol_indala224_can_be_decoded(data)) {
                result = true;
                break;
            }
        }
    }

    return result;
}

static void protocol_indala224_decoder_save(uint8_t* data_to, const uint8_t* data_from) {
    // PSK2 differential decode: the shift register contains phase values, not data.
    // T5577 PSK2 encodes data as phase transitions: data[n] = phase[n] XOR phase[n+1].
    // Bit 224 (start of second frame) provides phase[n+1] for the last data bit.
    for(size_t i = 0; i < INDALA224_DECODED_BIT_SIZE; i++) {
        bool phase_current = bit_lib_get_bit(data_from, i);
        bool phase_next = bit_lib_get_bit(data_from, i + 1);
        bit_lib_set_bit(data_to, i, phase_current ^ phase_next);
    }
}

bool protocol_indala224_decoder_feed(ProtocolIndala224* protocol, bool level, uint32_t duration) {
    bool result = false;

    if(duration > (INDALA224_US_PER_BIT / 2)) {
        if(protocol_indala224_decoder_feed_internal(level, duration, protocol->encoded_data)) {
            protocol_indala224_decoder_save(protocol->data, protocol->encoded_data);
            FURI_LOG_D("Indala224", "Positive");
            result = true;
            return result;
        }

        if(protocol_indala224_decoder_feed_internal(
               !level, duration, protocol->negative_encoded_data)) {
            protocol_indala224_decoder_save(protocol->data, protocol->negative_encoded_data);
            FURI_LOG_D("Indala224", "Negative");
            result = true;
            return result;
        }
    }

    return result;
}

bool protocol_indala224_encoder_start(ProtocolIndala224* protocol) {
    // Store raw data for PSK2 emulation - the yield function handles PSK2 modulation.
    memcpy(protocol->encoded_data, protocol->data, INDALA224_DECODED_DATA_SIZE);

    protocol->encoder.data_index = 0;
    protocol->encoder.current_polarity = true;
    protocol->encoder.pulse_phase = true;
    protocol->encoder.bit_clock_index = 0;

    return true;
}

LevelDuration protocol_indala224_encoder_yield(ProtocolIndala224* protocol) {
    LevelDuration level_duration;
    ProtocolIndala224Encoder* encoder = &protocol->encoder;

    if(encoder->pulse_phase) {
        level_duration = level_duration_make(encoder->current_polarity, 1);
        encoder->pulse_phase = false;
    } else {
        level_duration = level_duration_make(!encoder->current_polarity, 1);
        encoder->pulse_phase = true;

        encoder->bit_clock_index++;
        if(encoder->bit_clock_index >= INDALA224_ENCODER_PULSES_PER_BIT) {
            encoder->bit_clock_index = 0;

            // PSK2: carrier phase flips when data bit is 1
            bool current_bit = bit_lib_get_bit(protocol->encoded_data, encoder->data_index);
            if(current_bit) {
                encoder->current_polarity = !encoder->current_polarity;
            }

            bit_lib_increment_index(encoder->data_index, INDALA224_ENCODED_BIT_SIZE);
        }
    }

    return level_duration;
}

static void protocol_indala224_render_data_internal(
    ProtocolIndala224* protocol,
    FuriString* result,
    bool brief) {
    if(brief) {
        furi_string_printf(
            result,
            "Raw: %02X%02X%02X%02X\n"
            "     %02X%02X%02X%02X...",
            protocol->data[0],
            protocol->data[1],
            protocol->data[2],
            protocol->data[3],
            protocol->data[4],
            protocol->data[5],
            protocol->data[6],
            protocol->data[7]);
    } else {
        furi_string_printf(
            result,
            "Raw: %02X%02X%02X%02X %02X%02X%02X%02X\n"
            "%02X%02X%02X%02X %02X%02X%02X%02X\n"
            "%02X%02X%02X%02X %02X%02X%02X%02X\n"
            "%02X%02X%02X%02X",
            protocol->data[0],
            protocol->data[1],
            protocol->data[2],
            protocol->data[3],
            protocol->data[4],
            protocol->data[5],
            protocol->data[6],
            protocol->data[7],
            protocol->data[8],
            protocol->data[9],
            protocol->data[10],
            protocol->data[11],
            protocol->data[12],
            protocol->data[13],
            protocol->data[14],
            protocol->data[15],
            protocol->data[16],
            protocol->data[17],
            protocol->data[18],
            protocol->data[19],
            protocol->data[20],
            protocol->data[21],
            protocol->data[22],
            protocol->data[23],
            protocol->data[24],
            protocol->data[25],
            protocol->data[26],
            protocol->data[27]);
    }
}

void protocol_indala224_render_data(ProtocolIndala224* protocol, FuriString* result) {
    protocol_indala224_render_data_internal(protocol, result, false);
}

void protocol_indala224_render_brief_data(ProtocolIndala224* protocol, FuriString* result) {
    protocol_indala224_render_data_internal(protocol, result, true);
}

bool protocol_indala224_write_data(ProtocolIndala224* protocol, void* data) {
    LFRFIDWriteRequest* request = (LFRFIDWriteRequest*)data;
    bool result = false;

    if(request->write_type == LFRFIDWriteTypeT5577) {
        // Config: PSK2, RF/32, 7 data blocks (matches Proxmark3 T55X7_INDALA_224_CONFIG_BLOCK)
        lfrfid_write_fill_t5577(
            request,
            LFRFID_T5577_BITRATE_RF_32 | LFRFID_T5577_MODULATION_PSK2 |
                (7 << LFRFID_T5577_MAXBLOCK_SHIFT),
            protocol->data);
        result = true;
    }
    return result;
}

const ProtocolBase protocol_indala224 = {
    .name = "Indala224",
    .manufacturer = "Motorola",
    .data_size = INDALA224_DECODED_DATA_SIZE,
    .features = LFRFIDFeaturePSK,
    .validate_count = 6,
    .alloc = (ProtocolAlloc)protocol_indala224_alloc,
    .free = (ProtocolFree)protocol_indala224_free,
    .get_data = (ProtocolGetData)protocol_indala224_get_data,
    .decoder =
        {
            .start = (ProtocolDecoderStart)protocol_indala224_decoder_start,
            .feed = (ProtocolDecoderFeed)protocol_indala224_decoder_feed,
        },
    .encoder =
        {
            .start = (ProtocolEncoderStart)protocol_indala224_encoder_start,
            .yield = (ProtocolEncoderYield)protocol_indala224_encoder_yield,
        },
    .render_data = (ProtocolRenderData)protocol_indala224_render_data,
    .render_brief_data = (ProtocolRenderData)protocol_indala224_render_brief_data,
    .write_data = (ProtocolWriteData)protocol_indala224_write_data,
};

// ======================================================================================
// Test-Geschirr (nicht Teil des Ports) -- treibt die obigen Funktionen direkt an, ohne
// protocol_dict/pulse_glue. Die Encode->Bit-Pegel-Technik (Pegel am Anfang jeder 32-Yield-
// Bitperiode abtasten, dann als Lauflaengen *US_PER_BIT an den Decoder) ist dieselbe, die
// Unleashed selbst in seinem eigenen Unit-Test verwendet (PSK-Encoder-Ausgabe ist mit
// Dauer=1 pro Halbzyklus feiner als die vom Decoder erwartete Abtastgranularitaet).
// ======================================================================================

#define INDALA224_TEST_DATA_SIZE 28
static const uint8_t INDALA224_TEST_DATA[INDALA224_TEST_DATA_SIZE] = {
    0x80, 0x00, 0x00, 0x01, 0xB2, 0x35, 0x23, 0xA6, 0xC2, 0xE3, 0x1E, 0xBA,
    0x3C, 0xBE, 0xE4, 0xAF, 0xB3, 0xC6, 0xAD, 0x1F, 0xCF, 0x64, 0x93, 0x93,
    0x92, 0x8C, 0x14, 0xE5};

static const uint8_t INDALA224_ALT_TEST_DATA[INDALA224_TEST_DATA_SIZE] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

#define INDALA224_FRAMES_TO_ENCODE 3
static int g_checks = 0;

#define CHECK(cond)                                                               \
    do {                                                                          \
        g_checks++;                                                               \
        if(!(cond)) {                                                            \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                             \
        }                                                                         \
    } while(0)

// Encode `data` fuer `frames` 224-Bit-Rahmen und liefere die Pegel-pro-Bit-Folge
// (ein bool je Bitperiode, exakt die vom Unleashed-Unit-Test verwendete Abtasttechnik).
static bool* encode_to_bit_levels(const uint8_t* data, size_t frames) {
    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    memcpy(p->data, data, INDALA224_TEST_DATA_SIZE);
    CHECK(protocol_indala224_encoder_start(p));

    size_t total_bits = 224 * frames;
    bool* levels = malloc(total_bits * sizeof(bool));
    CHECK(levels != NULL);

    for(size_t i = 0; i < total_bits; i++) {
        LevelDuration ld = protocol_indala224_encoder_yield(p);
        levels[i] = level_duration_get_level(ld);
        for(size_t skip = 0; skip < 31; skip++) {
            protocol_indala224_encoder_yield(p);
        }
    }

    protocol_indala224_free(p);
    return levels;
}

// Lauflaengen aus den Bitpegeln bilden (*US_PER_BIT) und direkt an decoder_feed() reichen,
// bis sie dekodiert oder der Vorrat erschoepft ist.
static bool decode_bit_levels(ProtocolIndala224* p, const bool* levels, size_t total_bits) {
    protocol_indala224_decoder_start(p);

    size_t i = 0;
    while(i < total_bits) {
        bool cur = levels[i];
        size_t run = 1;
        while(i + run < total_bits && levels[i + run] == cur) run++;
        uint32_t duration = (uint32_t)(run * INDALA224_US_PER_BIT);

        if(protocol_indala224_decoder_feed(p, cur, duration)) return true;
        i += run;
    }
    return false;
}

static void test_roundtrip(const uint8_t* data, const char* label) {
    size_t total_bits = 224 * INDALA224_FRAMES_TO_ENCODE;
    bool* levels = encode_to_bit_levels(data, INDALA224_FRAMES_TO_ENCODE);

    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    bool decoded = decode_bit_levels(p, levels, total_bits);
    CHECK(decoded);
    CHECK(memcmp(p->data, data, INDALA224_TEST_DATA_SIZE) == 0);

    // Zweiter, unabhaengiger Rundlauf auf einer frischen Instanz direkt danach --
    // kein Zustand haengt zwischen zwei Decode-Lebenszyklen (ASan haette ein Leck/
    // Doppel-Free bei Alloc/Free-Unsymmetrie ebenfalls gemeldet).
    protocol_indala224_decoder_start(p);
    bool decoded_again = decode_bit_levels(p, levels, total_bits);
    CHECK(decoded_again);
    CHECK(memcmp(p->data, data, INDALA224_TEST_DATA_SIZE) == 0);

    protocol_indala224_free(p);
    free(levels);
    fprintf(stderr, "[ok] roundtrip: %s\n", label);
}

static void test_alternating_phase_flag(void) {
    // ALT_TEST_DATA hat eine ungerade Zahl 1-Bits (genau ein 1-Bit, das Praeambel-Leitbit
    // selbst) -- die PSK2-Phase muss daher zwischen Rahmen 1 und Rahmen 2 invertiert sein.
    bool* levels = encode_to_bit_levels(INDALA224_ALT_TEST_DATA, INDALA224_FRAMES_TO_ENCODE);
    CHECK(levels[0] != levels[224]);
    free(levels);
    fprintf(stderr, "[ok] alternating-phase flag set as expected\n");
}

static void test_negative_garbage_then_valid(void) {
    // "Muellzug" (zufaellige kurze Pegelwechsel, decodiert niemals) direkt gefolgt vom
    // echten Rahmen im selben, durchgehenden Bitstrom -- das Schieberegister darf den
    // vorherigen Muell nicht an einem erfolgreichen Decode hindern (Texkom-Phase-4b-Muster).
    bool* valid_levels = encode_to_bit_levels(INDALA224_TEST_DATA, INDALA224_FRAMES_TO_ENCODE);
    size_t valid_bits = 224 * INDALA224_FRAMES_TO_ENCODE;

    size_t garbage_bits = 50;
    size_t total = garbage_bits + valid_bits;
    bool* levels = malloc(total * sizeof(bool));
    CHECK(levels != NULL);
    for(size_t i = 0; i < garbage_bits; i++) levels[i] = (i % 3) == 0;
    memcpy(levels + garbage_bits, valid_levels, valid_bits * sizeof(bool));

    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    bool decoded = decode_bit_levels(p, levels, total);
    CHECK(decoded);
    CHECK(memcmp(p->data, INDALA224_TEST_DATA, INDALA224_TEST_DATA_SIZE) == 0);

    protocol_indala224_free(p);
    free(levels);
    free(valid_levels);
    fprintf(stderr, "[ok] garbage-then-valid in one continuous stream decodes\n");
}

// Das 256-Bit-Schieberegister wird beim ersten Rahmen von links (Bit-Index 0) mit
// Initialnullen aus decoder_start() gefuellt; ein echtes, gueltiges Bit sitzt an Index 0 des
// Fensters erst, sobald exakt 256 reale Bit gepusht wurden (vorher ist Index 0 noch
// Fuellnull, danach ist das erste echte Bit bereits wieder herausgeschoben) -- die einzige
// Gelegenheit, zu der Fensteranfang (Index 0) und Fensterende (Index 224, 224 Bit spaeter
// dieselbe Rahmengrenze) gleichzeitig auf echten Daten liegen, ist deshalb genau N=256.
#define INDALA224_MIN_WINDOW_BITS (256)

static void test_minimal_window_decodes(void) {
    // Kontrollprobe fuer die beiden folgenden Negativtests: exakt an diesem einzigen Punkt
    // (256 Bit, keinerlei Spielraum) muss ein unveraenderter Strom bereits erfolgreich
    // decodieren -- sonst waeren die Negativtests unten nur deshalb erfolgreich, weil die
    // Fensterlaenge selbst schon zu knapp waere.
    bool* levels = encode_to_bit_levels(INDALA224_TEST_DATA, INDALA224_FRAMES_TO_ENCODE);
    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    bool decoded = decode_bit_levels(p, levels, INDALA224_MIN_WINDOW_BITS);
    CHECK(decoded);
    CHECK(memcmp(p->data, INDALA224_TEST_DATA, INDALA224_TEST_DATA_SIZE) == 0);
    protocol_indala224_free(p);
    free(levels);
    fprintf(stderr, "[ok] minimal 256-bit window already decodes (control for negatives below)\n");
}

static void test_negative_insufficient_length(void) {
    // Ein Bit weniger als das rechnerische Minimum: die zweite Praeambel-Pruefung (Bit-Index
    // 224) kann nie erreicht werden, weil ihr letztes benoetigtes Bit fehlt -- muss in jedem
    // Fall ablehnen, unabhaengig vom Dateninhalt.
    bool* levels = encode_to_bit_levels(INDALA224_TEST_DATA, INDALA224_FRAMES_TO_ENCODE);
    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    bool decoded = decode_bit_levels(p, levels, INDALA224_MIN_WINDOW_BITS - 1);
    CHECK(!decoded);
    protocol_indala224_free(p);
    free(levels);
    fprintf(stderr, "[ok] one bit short of the minimal window correctly never decodes\n");
}

static void test_negative_corrupted_preamble(void) {
    // Bei exakt der minimalen Fensterlaenge (siehe oben) ist noch kein einziges Bit aus dem
    // Schieberegister herausgeschoben -- ein gekipptes Praeambel-Leitbit (Bit 0 des Stroms)
    // bleibt deshalb bis zum letzten Fuetterungsversuch im Fenster sichtbar und es gibt
    // keine weitere Wiederholung, die den Fehler "heilen" koennte.
    bool* levels = encode_to_bit_levels(INDALA224_TEST_DATA, INDALA224_FRAMES_TO_ENCODE);
    levels[0] = !levels[0];

    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    bool decoded = decode_bit_levels(p, levels, INDALA224_MIN_WINDOW_BITS);
    CHECK(!decoded);

    protocol_indala224_free(p);
    free(levels);
    fprintf(stderr, "[ok] corrupted leading preamble bit correctly rejected (no later repeat to heal it)\n");
}

static void test_write_data_t5577(void) {
    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    memcpy(p->data, INDALA224_TEST_DATA, INDALA224_TEST_DATA_SIZE);

    LFRFIDWriteRequest request;
    memset(&request, 0, sizeof(request));
    request.write_type = LFRFIDWriteTypeT5577;

    CHECK(protocol_indala224_write_data(p, &request));
    CHECK(request.t5577.blocks_to_write == 8);

    uint32_t expected_block0 = LFRFID_T5577_BITRATE_RF_32 | LFRFID_T5577_MODULATION_PSK2 |
                                (7 << LFRFID_T5577_MAXBLOCK_SHIFT);
    CHECK(request.t5577.block[0] == expected_block0);

    for(int block = 1; block <= 7; block++) {
        uint32_t expected = 0;
        for(int byte = 0; byte < 4; byte++) {
            expected = (expected << 8) | INDALA224_TEST_DATA[(block - 1) * 4 + byte];
        }
        CHECK(request.t5577.block[block] == expected);
    }

    protocol_indala224_free(p);
    fprintf(stderr, "[ok] write_data() fills exactly 8 T5577 blocks matching source bytes\n");
}

static void test_render(void) {
    ProtocolIndala224* p = protocol_indala224_alloc();
    CHECK(p != NULL);
    memcpy(p->data, INDALA224_TEST_DATA, INDALA224_TEST_DATA_SIZE);

    FuriString result;
    protocol_indala224_render_data(p, &result);
    CHECK(strcmp(
              result.buf,
              "Raw: 80000001 B23523A6\n"
              "C2E31EBA 3CBEE4AF\n"
              "B3C6AD1F CF649393\n"
              "928C14E5") == 0);

    protocol_indala224_render_brief_data(p, &result);
    CHECK(strcmp(
              result.buf,
              "Raw: 80000001\n"
              "     B2352 3A6...") != 0); // Gegenprobe: Format darf sich nicht veraendert haben
    CHECK(strcmp(
              result.buf,
              "Raw: 80000001\n"
              "     B23523A6...") == 0);

    protocol_indala224_free(p);
    fprintf(stderr, "[ok] render_data()/render_brief_data() produce the expected hex layout\n");
}

int main(void) {
    test_roundtrip(INDALA224_TEST_DATA, "INDALA224_TEST_DATA (Proxmark3-Referenz)");
    test_roundtrip(INDALA224_ALT_TEST_DATA, "INDALA224_ALT_TEST_DATA (Phasenwechsel)");
    test_alternating_phase_flag();
    test_negative_garbage_then_valid();
    test_minimal_window_decodes();
    test_negative_insufficient_length();
    test_negative_corrupted_preamble();
    test_write_data_t5577();
    test_render();

    fprintf(stderr, "\n%d Pruefungen, alle bestanden.\n", g_checks);
    return 0;
}
