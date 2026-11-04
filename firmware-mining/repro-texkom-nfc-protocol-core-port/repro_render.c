// Host repro for Phase 5's applications/main/nfc/helpers/protocol_support/texkom/
// texkom_render.c (nfc_render_texkom_info()) -- the one piece of the UI layer that
// is pure string-formatting logic without any furi_hal/view/widget dependency.
//
// The Phase 1-4b protocol core (texkom_get_features/texkom_crc_byte/
// texkom_is_crc_valid/texkom_get_expected_crc/texkom_update_crc/texkom_make_blank/
// texkom_get_uid/texkom_set_uid, the CRC-8 variants, the TexkomFeatures table) is
// copied verbatim from the already-verified firmware-mining/
// repro-texkom-nfc-protocol-core-port/repro.c into this separate translation unit,
// exactly as repro_poller.c/repro_listener.c/repro_furi_hal_texkom.c already do --
// re-used here to build realistic TexkomData values, not re-verified (see repro.c
// for that). The decode_tk13/_tk15/_tk17 functions are not needed to exercise the
// renderer and are intentionally left out.
//
// nfc_render_texkom_info() itself is transcribed unchanged from Unleashed dev's
// applications/main/nfc/helpers/protocol_support/texkom/texkom_render.c. Its two
// real dependencies -- FuriString and NfcProtocolFormatType -- are replaced by:
//   - a minimal FuriString stand-in (growable buffer, realloc'd by
//     furi_string_cat_printf() same as the real one does internally) that
//     implements exactly the four calls the function makes
//     (furi_string_alloc/_cat_printf/_get_cstr/_free, called the same way by
//     every other already-ported render function in this codebase);
//   - NfcProtocolFormatType, copied from nfc_protocol_support_render_common.h
//     (two values, Short/Full, confirmed identical between this fork and
//     Unleashed dev).
//
// Scenarios:
//  1. TK13, valid CRC, full format: UID line lists exactly TEXKOM_UID_SIZE (4)
//     bytes: "CRC: xx (OK)" with no "Expected CRC" line; "Frame:" line present
//     and lists all 8 bytes.
//  2. Same data, short format: identical UID/CRC lines, but no "Frame:" line at
//     all -- confirms format_type actually gates that one block and nothing
//     else.
//  3. TK13, corrupted CRC byte (frame[7] flipped after set_uid/update_crc): "CRC:
//     xx (Invalid)" followed by an "Expected CRC: yy" line, with yy equal to the
//     value an independently computed texkom_crc_tk13() over the same UID
//     produces (not just "some value" -- the exact byte update_crc() would have
//     written).
//  4. MMBIT, valid CRC: UID line lists exactly TEXKOM_MMBIT_UID_SIZE (3) bytes --
//     confirms the loop bound comes from texkom_get_uid()'s own uid_len output,
//     not a hardcoded constant.
//  5. TexkomTypeUnknown (freshly made_blank, never given a real type): CRC byte
//     is 0x00 and texkom_is_crc_valid() is false (no known CRC handler for this
//     type) -- "CRC: 00 (Invalid)" but *no* "Expected CRC" line, since
//     texkom_get_expected_crc() also returns false for this type (same
//     crc_byte() gate as is_crc_valid()) and the real function's "if(!crc_valid
//     && texkom_get_expected_crc(...))" guard must therefore skip the second
//     line -- the one case that exercises crc_valid == false with the
//     Expected-CRC line still suppressed.
//  6. TK17, valid CRC, full format -- second coding/type combination exercised,
//     confirms the renderer itself has no type-specific branch (it only calls
//     texkom_get_uid()/texkom_is_crc_valid()/texkom_get_expected_crc(), all of
//     which already dispatch on type internally).
//
// Each scenario's expected string is built independently (snprintf against the
// known field values), then compared byte-for-byte against
// nfc_render_texkom_info()'s actual output -- not a substring/contains check,
// so a wrong field order or missing separator would fail too.

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- lib/nfc/protocols/texkom/texkom.h + texkom_i.h, transcribed (same subset
// as firmware-mining/repro-texkom-nfc-protocol-core-port/repro.c) ---
#define TEXKOM_FRAME_SIZE       (8U)
#define TEXKOM_UID_SIZE         (4U)
#define TEXKOM_MMBIT_UID_SIZE   (3U)
#define TEXKOM_UID_OFFSET       (3U)
#define TEXKOM_MMBIT_UID_OFFSET (4U)
#define TEXKOM_TYPE_BYTE_TK13   (0x63U)
#define TEXKOM_TYPE_BYTE_TK17   (0xCAU)

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
    uint8_t crc = 0;
    for(uint8_t index = 0; index < TEXKOM_UID_SIZE; ++index) {
        uint8_t input_byte = uid[index];
        for(uint8_t bit_position = 0; bit_position < 8; ++bit_position) {
            const uint8_t mix = (crc ^ input_byte) & (uint8_t)(0x01);
            crc >>= 1;
            if(mix != 0) crc ^= 0x8C;
            input_byte >>= 1;
        }
    }
    return crc;
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

// --- applications/main/nfc/helpers/protocol_support/nfc_protocol_support_render_common.h,
// transcribed (two values, confirmed identical between this fork and Unleashed dev). ---
typedef enum {
    NfcProtocolFormatTypeShort,
    NfcProtocolFormatTypeFull,
} NfcProtocolFormatType;

// --- minimal FuriString stand-in: implements exactly the four calls
// nfc_render_texkom_info() makes (alloc/cat_printf/get_cstr/free). Grows the same
// way the real furi_string_cat_printf() does -- compute the needed size with
// vsnprintf(NULL, 0, ...), then grow and format -- so the mock cannot silently
// truncate a long "Frame:" line the way a fixed-size buffer could. ---
typedef struct {
    char* buf;
    size_t len; // not counting the NUL terminator
} FuriString;

static FuriString* furi_string_alloc(void) {
    FuriString* s = malloc(sizeof(FuriString));
    assert(s);
    s->buf = malloc(1);
    assert(s->buf);
    s->buf[0] = '\0';
    s->len = 0;
    return s;
}

static void furi_string_cat_printf(FuriString* s, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int added = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    assert(added >= 0);

    char* grown = realloc(s->buf, s->len + (size_t)added + 1);
    assert(grown);
    s->buf = grown;
    int written = vsnprintf(s->buf + s->len, (size_t)added + 1, fmt, args);
    va_end(args);
    assert(written == added);
    s->len += (size_t)added;
}

static const char* furi_string_get_cstr(const FuriString* s) {
    return s->buf;
}

static void furi_string_free(FuriString* s) {
    free(s->buf);
    free(s);
}

// --- applications/main/nfc/helpers/protocol_support/texkom/texkom_render.c,
// transcribed unchanged (the function under test). ---
static void nfc_render_texkom_info(
    const TexkomData* data,
    NfcProtocolFormatType format_type,
    FuriString* str) {
    size_t uid_len = 0;
    const uint8_t* uid = texkom_get_uid(data, &uid_len);

    furi_string_cat_printf(str, "UID:");
    for(size_t i = 0; i < uid_len; i++) {
        furi_string_cat_printf(str, " %02X", uid[i]);
    }

    const bool crc_valid = texkom_is_crc_valid(data);
    furi_string_cat_printf(
        str, "\nCRC: %02X (%s)", data->frame[TEXKOM_FRAME_SIZE - 1], crc_valid ? "OK" : "Invalid");

    uint8_t expected_crc = 0;
    if(!crc_valid && texkom_get_expected_crc(data, &expected_crc)) {
        furi_string_cat_printf(str, "\nExpected CRC: %02X", expected_crc);
    }

    if(format_type == NfcProtocolFormatTypeFull) {
        furi_string_cat_printf(str, "\nFrame:");
        for(size_t i = 0; i < TEXKOM_FRAME_SIZE; i++) {
            furi_string_cat_printf(str, " %02X", data->frame[i]);
        }
    }
}

// --- test harness ---

static int g_checks = 0;

static void check_str_eq(const char* got, const char* expected, const char* label) {
    g_checks++;
    if(strcmp(got, expected) != 0) {
        fprintf(stderr, "FAIL [%s]:\n  got:      %s\n  expected: %s\n", label, got, expected);
        exit(1);
    }
}

int main(void) {
    const uint8_t uid4[TEXKOM_UID_SIZE] = {0x11, 0x22, 0x33, 0x44};
    const uint8_t uid3[TEXKOM_MMBIT_UID_SIZE] = {0xAA, 0xBB, 0xCC};

    // Scenario 1+6: TK13/TK17, valid CRC, full format.
    for(int which = 0; which < 2; which++) {
        TexkomData data;
        TexkomType type = which == 0 ? TexkomTypeTk13 : TexkomTypeTk17;
        assert(texkom_make_blank(&data, type));
        assert(texkom_set_uid(&data, uid4, TEXKOM_UID_SIZE));
        assert(texkom_is_crc_valid(&data));

        FuriString* s = furi_string_alloc();
        nfc_render_texkom_info(&data, NfcProtocolFormatTypeFull, s);

        char expected[256];
        snprintf(
            expected,
            sizeof(expected),
            "UID: %02X %02X %02X %02X\nCRC: %02X (OK)\nFrame: %02X %02X %02X %02X %02X %02X %02X "
            "%02X",
            uid4[0],
            uid4[1],
            uid4[2],
            uid4[3],
            data.frame[7],
            data.frame[0],
            data.frame[1],
            data.frame[2],
            data.frame[3],
            data.frame[4],
            data.frame[5],
            data.frame[6],
            data.frame[7]);
        check_str_eq(
            furi_string_get_cstr(s),
            expected,
            which == 0 ? "1 TK13 valid full" : "6 TK17 valid full");
        furi_string_free(s);
    }

    // Scenario 2: same TK13 data, short format -- no "Frame:" line.
    {
        TexkomData data;
        assert(texkom_make_blank(&data, TexkomTypeTk13));
        assert(texkom_set_uid(&data, uid4, TEXKOM_UID_SIZE));

        FuriString* s = furi_string_alloc();
        nfc_render_texkom_info(&data, NfcProtocolFormatTypeShort, s);

        char expected[128];
        snprintf(
            expected,
            sizeof(expected),
            "UID: %02X %02X %02X %02X\nCRC: %02X (OK)",
            uid4[0],
            uid4[1],
            uid4[2],
            uid4[3],
            data.frame[7]);
        check_str_eq(furi_string_get_cstr(s), expected, "2 TK13 valid short");
        furi_string_free(s);
    }

    // Scenario 3: TK13, CRC byte corrupted after set_uid -- "Invalid" plus the
    // exact "Expected CRC" value an independent CRC computation gives.
    {
        TexkomData data;
        assert(texkom_make_blank(&data, TexkomTypeTk13));
        assert(texkom_set_uid(&data, uid4, TEXKOM_UID_SIZE));
        data.frame[7] ^= 0x01;
        assert(!texkom_is_crc_valid(&data));
        uint8_t independent_crc = texkom_crc_tk13(uid4);

        FuriString* s = furi_string_alloc();
        nfc_render_texkom_info(&data, NfcProtocolFormatTypeFull, s);

        char expected[256];
        snprintf(
            expected,
            sizeof(expected),
            "UID: %02X %02X %02X %02X\nCRC: %02X (Invalid)\nExpected CRC: %02X\nFrame: %02X %02X "
            "%02X %02X %02X %02X %02X %02X",
            uid4[0],
            uid4[1],
            uid4[2],
            uid4[3],
            data.frame[7],
            independent_crc,
            data.frame[0],
            data.frame[1],
            data.frame[2],
            data.frame[3],
            data.frame[4],
            data.frame[5],
            data.frame[6],
            data.frame[7]);
        check_str_eq(furi_string_get_cstr(s), expected, "3 TK13 invalid CRC");
        furi_string_free(s);
    }

    // Scenario 4: MMBIT, valid CRC -- UID line lists exactly 3 bytes, not 4.
    {
        TexkomData data;
        assert(texkom_make_blank(&data, TexkomTypeMmbit));
        assert(texkom_set_uid(&data, uid3, TEXKOM_MMBIT_UID_SIZE));
        assert(texkom_is_crc_valid(&data));

        FuriString* s = furi_string_alloc();
        nfc_render_texkom_info(&data, NfcProtocolFormatTypeShort, s);

        char expected[128];
        snprintf(
            expected,
            sizeof(expected),
            "UID: %02X %02X %02X\nCRC: %02X (OK)",
            uid3[0],
            uid3[1],
            uid3[2],
            data.frame[7]);
        check_str_eq(furi_string_get_cstr(s), expected, "4 MMBIT valid short");
        furi_string_free(s);
    }

    // Scenario 5: TexkomTypeUnknown -- CRC invalid, but no "Expected CRC" line
    // (no CRC handler registered for this type -> texkom_get_expected_crc()
    // itself returns false, same gate as texkom_is_crc_valid()).
    {
        TexkomData data;
        memset(&data, 0, sizeof(data));
        data.type = TexkomTypeUnknown;
        data.frame[0] = 0xFF;
        data.frame[1] = 0xFF;
        assert(!texkom_is_crc_valid(&data));
        uint8_t dummy;
        assert(!texkom_get_expected_crc(&data, &dummy));

        FuriString* s = furi_string_alloc();
        nfc_render_texkom_info(&data, NfcProtocolFormatTypeShort, s);

        char expected[128];
        snprintf(
            expected,
            sizeof(expected),
            "UID: %02X %02X %02X %02X\nCRC: %02X (Invalid)",
            data.frame[3],
            data.frame[4],
            data.frame[5],
            data.frame[6],
            data.frame[7]);
        check_str_eq(furi_string_get_cstr(s), expected, "5 Unknown invalid, no Expected CRC");
        furi_string_free(s);
    }

    printf("OK: %d checks passed, 6 scenarios\n", g_checks);
    return 0;
}
