/*
 * Host repro: mf_plus_get_type_from_version()/mf_plus_get_type_from_iso4() classification-only
 * port (lib/nfc/protocols/mf_plus/mf_plus_i.c).
 *
 * Scope note (see firmware-mining/mfplus-sl0-sl3-detection-port.md "Nicht Teil dieses Patches"):
 * Unleashed's real SL0-vs-SL3 discrimination needs a new active poller command
 * (mf_plus_poller_probe_security_level(), lib/nfc/protocols/mf_plus/mf_plus_poller.c) that sends
 * an extra APDU to a SAK-0x20 card -- that is Phase-3 poller-state-machine territory and stays
 * out of scope (hardware-blocked, same as the rest of Phase 3-6). What this repro verifies is the
 * part of the same Unleashed change that touches only already-polled SAK/ATQA/ATS bytes, with no
 * new card I/O:
 *   1. ATS product matching switches from a 4-entry exact-byte-string table to nibble/field
 *      matching (mf_plus_type_from_ats()) -- recognizes MIFARE Plus SE config/generation variants
 *      the old exact table rejects, while still gating out non-Plus (e.g. DESFire) ATS blocks.
 *   2. A new ATQA-based storage-size fallback (mf_plus_size_from_atqa()) for the GetVersion path
 *      when the storage byte itself is unrecognized.
 *   3. mf_plus_get_type_from_version()'s SAK -> SecurityLevel mapping changes from a two-way
 *      if/else (sak==0x20 ? SL3 : SL1) to a real switch: SAK 0x10/0x11 (SL2, AN10833) no longer
 *      collapses into SL1, and a SAK the switch does not recognize now yields
 *      MfPlusSecurityLevelUnknown instead of guessing SL1.
 *   4. mf_plus_get_type_from_iso4()'s SAK 0x10/0x11 cases report the generic MfPlusTypePlus
 *      instead of hardcoding MfPlusTypeX -- the bare SAK alone does not justify the X product.
 * SAK 0x20 still defaults to MfPlusSecurityLevel3 unconditionally in both ALT and NEU below (no
 * SL0 claim is ever made) -- Testgroup 6 below exists specifically to prove that non-regression.
 *
 * ATS historical-byte values are the four MIFARE Plus reference blocks already present in my own
 * mf_plus_i.c (S/X/SE, from NXP AN10833/TagInfo) plus two further SE fingerprints (TagInfo v6.2.0,
 * cited in Unleashed's own source comment for this change) that the old exact table does not cover.
 * All are public NXP/TagInfo protocol constants, not captured card data.
 *
 * Build/run:
 *   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define furi_assert(x) assert(x)
#define FURI_LOG_D(tag, ...) \
    do {                     \
    } while(0)
#define TAG "MfPlus"

/* ---- minimal stand-ins for the real firmware types (field-for-field match) ---- */

typedef struct {
    uint8_t atqa[2];
    uint8_t sak;
} Iso14443_3aData;

typedef struct {
    const uint8_t* data;
    uint32_t count;
} SimpleArray;

static uint32_t simple_array_get_count(const SimpleArray* a) {
    return a->count;
}
static const void* simple_array_cget_data(const SimpleArray* a) {
    return a->data;
}

typedef struct {
    SimpleArray* t1_tk;
} Iso14443_4aAtsData;

typedef struct {
    Iso14443_3aData* iso14443_3a_data;
    Iso14443_4aAtsData ats_data;
} Iso14443_4aData;

typedef enum {
    MfPlusErrorNone,
    MfPlusErrorProtocol,
} MfPlusError;

typedef enum {
    MfPlusTypePlus,
    MfPlusTypeEV1,
    MfPlusTypeEV2,
    MfPlusTypeS,
    MfPlusTypeSE,
    MfPlusTypeX,
    MfPlusTypeUnknown,
} MfPlusType;

typedef enum {
    MfPlusSize1K,
    MfPlusSize2K,
    MfPlusSize4K,
    MfPlusSizeUnknown,
} MfPlusSize;

typedef enum {
    MfPlusSecurityLevel0,
    MfPlusSecurityLevel1,
    MfPlusSecurityLevel2,
    MfPlusSecurityLevel3,
    MfPlusSecurityLevelUnknown,
} MfPlusSecurityLevel;

typedef struct {
    uint8_t hw_type;
    uint8_t hw_major;
    uint8_t hw_storage;
} MfPlusVersion;

typedef struct {
    MfPlusVersion version;
    MfPlusType type;
    MfPlusSize size;
    MfPlusSecurityLevel security_level;
} MfPlusData;

#define MF_PLUS_T1_TK_VALUE_LEN 7

/* ================================================================================= */
/* ALT -- byte-identical to my current mntm-012 mf_plus_i.c                          */
/* ================================================================================= */

static const uint8_t alt_ats_t1_tk_values[][MF_PLUS_T1_TK_VALUE_LEN] = {
    {0xC1, 0x05, 0x2F, 0x2F, 0x00, 0x35, 0xC7}, // Mifare Plus S
    {0xC1, 0x05, 0x2F, 0x2F, 0x01, 0xBC, 0xD6}, // Mifare Plus X
    {0xC1, 0x05, 0x21, 0x30, 0x00, 0xF6, 0xD1}, // Mifare Plus SE
    {0xC1, 0x05, 0x21, 0x30, 0x10, 0xF6, 0xD1}, // Mifare Plus SE
};

static MfPlusError
    alt_get_type_from_version(const Iso14443_4aData* iso14443_4a_data, MfPlusData* mf_plus_data) {
    furi_assert(iso14443_4a_data);
    furi_assert(mf_plus_data);

    MfPlusError error = MfPlusErrorProtocol;

    if((mf_plus_data->version.hw_type & 0x0F) == 0x02) {
        error = MfPlusErrorNone;

        switch(mf_plus_data->version.hw_major) {
        case 0x11:
            mf_plus_data->type = MfPlusTypeEV1;
            break;
        case 0x22:
            mf_plus_data->type = MfPlusTypeEV2;
            break;
        default:
            mf_plus_data->type = MfPlusTypeUnknown;
            break;
        }

        switch(mf_plus_data->version.hw_storage) {
        case 0x16:
            mf_plus_data->size = MfPlusSize2K;
            break;
        case 0x18:
            mf_plus_data->size = MfPlusSize4K;
            break;
        default:
            mf_plus_data->size = MfPlusSizeUnknown;
            break;
        }

        if(iso14443_4a_data->iso14443_3a_data->sak == 0x20) {
            mf_plus_data->security_level = MfPlusSecurityLevel3;
        } else {
            mf_plus_data->security_level = MfPlusSecurityLevel1;
        }
    }

    return error;
}

static MfPlusError
    alt_get_type_from_iso4(const Iso14443_4aData* iso4_data, MfPlusData* mf_plus_data) {
    furi_assert(iso4_data);
    furi_assert(mf_plus_data);

    MfPlusError error = MfPlusErrorProtocol;

    const size_t historical_bytes_len = simple_array_get_count(iso4_data->ats_data.t1_tk);
    if(historical_bytes_len != MF_PLUS_T1_TK_VALUE_LEN) {
        return MfPlusErrorProtocol;
    }
    const uint8_t* historical_bytes = simple_array_cget_data(iso4_data->ats_data.t1_tk);

    switch(iso4_data->iso14443_3a_data->sak) {
    case 0x08:
        if(memcmp(historical_bytes, alt_ats_t1_tk_values[0], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeS;
            mf_plus_data->size = MfPlusSize2K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        } else if(memcmp(historical_bytes, alt_ats_t1_tk_values[1], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeX;
            mf_plus_data->size = MfPlusSize2K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        } else if(
            memcmp(historical_bytes, alt_ats_t1_tk_values[2], historical_bytes_len) == 0 ||
            memcmp(historical_bytes, alt_ats_t1_tk_values[3], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeSE;
            mf_plus_data->size = MfPlusSize1K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        }
        break;
    case 0x10:
        mf_plus_data->type = MfPlusTypeX;
        mf_plus_data->size = MfPlusSize2K;
        mf_plus_data->security_level = MfPlusSecurityLevel2;
        error = MfPlusErrorNone;
        break;
    case 0x11:
        mf_plus_data->type = MfPlusTypeX;
        mf_plus_data->size = MfPlusSize4K;
        mf_plus_data->security_level = MfPlusSecurityLevel2;
        error = MfPlusErrorNone;
        break;
    case 0x18:
        if(memcmp(historical_bytes, alt_ats_t1_tk_values[0], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeS;
            mf_plus_data->size = MfPlusSize4K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        } else if(memcmp(historical_bytes, alt_ats_t1_tk_values[1], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeX;
            mf_plus_data->size = MfPlusSize4K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        }
        break;
    case 0x20:
        if(memcmp(historical_bytes, alt_ats_t1_tk_values[0], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeS;
            mf_plus_data->security_level = MfPlusSecurityLevel3;
            if((iso4_data->iso14443_3a_data->atqa[0] & 0x0F) == 0x04) {
                mf_plus_data->size = MfPlusSize2K;
                error = MfPlusErrorNone;
            } else if((iso4_data->iso14443_3a_data->atqa[0] & 0x0F) == 0x02) {
                mf_plus_data->size = MfPlusSize4K;
                error = MfPlusErrorNone;
            }
        } else if(memcmp(historical_bytes, alt_ats_t1_tk_values[1], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeX;
            mf_plus_data->security_level = MfPlusSecurityLevel3;
            if((iso4_data->iso14443_3a_data->atqa[0] & 0x0F) == 0x04) {
                mf_plus_data->size = MfPlusSize2K;
                error = MfPlusErrorNone;
            } else if((iso4_data->iso14443_3a_data->atqa[0] & 0x0F) == 0x02) {
                mf_plus_data->size = MfPlusSize4K;
                error = MfPlusErrorNone;
            }
        } else if(
            memcmp(historical_bytes, alt_ats_t1_tk_values[2], historical_bytes_len) == 0 ||
            memcmp(historical_bytes, alt_ats_t1_tk_values[3], historical_bytes_len) == 0) {
            mf_plus_data->type = MfPlusTypeSE;
            mf_plus_data->size = MfPlusSize1K;
            mf_plus_data->security_level = MfPlusSecurityLevel3;
            error = MfPlusErrorNone;
        }
        break;
    }

    return error;
}

/* ================================================================================= */
/* NEU -- classification-only slice of Unleashed's mf_plus_i.c (probe param removed) */
/* ================================================================================= */

static MfPlusType new_type_from_ats(const uint8_t* historical_bytes, size_t len) {
    if(len != MF_PLUS_T1_TK_VALUE_LEN) return MfPlusTypeUnknown;
    if(historical_bytes[0] != 0xC1 || historical_bytes[1] != 0x05) return MfPlusTypeUnknown;
    if((historical_bytes[2] & 0xF0) != 0x20) return MfPlusTypeUnknown;

    if(historical_bytes[3] == 0x30 && (historical_bytes[2] == 0x21 || historical_bytes[2] == 0x20))
        return MfPlusTypeSE;
    if(historical_bytes[2] == 0x2F && historical_bytes[3] == 0x2F) {
        return (historical_bytes[4] & 0x01) ? MfPlusTypeX : MfPlusTypeS;
    }
    return MfPlusTypeUnknown;
}

static MfPlusSize new_size_from_atqa(const uint8_t atqa[2]) {
    const uint16_t atqa_value = atqa[0] | ((uint16_t)atqa[1] << 8);
    if(atqa_value & 0x0004) return MfPlusSize2K;
    if(atqa_value & 0x0002) return MfPlusSize4K;
    return MfPlusSizeUnknown;
}

static MfPlusError
    new_get_type_from_version(const Iso14443_4aData* iso14443_4a_data, MfPlusData* mf_plus_data) {
    furi_assert(iso14443_4a_data);
    furi_assert(mf_plus_data);

    MfPlusError error = MfPlusErrorProtocol;

    if((mf_plus_data->version.hw_type & 0x0F) == 0x02) {
        error = MfPlusErrorNone;

        switch(mf_plus_data->version.hw_major) {
        case 0x11:
            mf_plus_data->type = MfPlusTypeEV1;
            break;
        case 0x22:
            mf_plus_data->type = MfPlusTypeEV2;
            break;
        default:
            mf_plus_data->type = MfPlusTypeUnknown;
            break;
        }

        switch(mf_plus_data->version.hw_storage) {
        case 0x16:
            mf_plus_data->size = MfPlusSize2K;
            break;
        case 0x18:
            mf_plus_data->size = MfPlusSize4K;
            break;
        default:
            mf_plus_data->size = new_size_from_atqa(iso14443_4a_data->iso14443_3a_data->atqa);
            break;
        }

        const uint8_t sak = iso14443_4a_data->iso14443_3a_data->sak;
        switch(sak) {
        case 0x20:
            mf_plus_data->security_level = MfPlusSecurityLevel3;
            break;
        case 0x10:
        case 0x11:
            mf_plus_data->security_level = MfPlusSecurityLevel2;
            break;
        case 0x08:
        case 0x18:
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            break;
        default:
            mf_plus_data->security_level = MfPlusSecurityLevelUnknown;
            break;
        }
    }

    return error;
}

static MfPlusError
    new_get_type_from_iso4(const Iso14443_4aData* iso4_data, MfPlusData* mf_plus_data) {
    furi_assert(iso4_data);
    furi_assert(mf_plus_data);

    MfPlusError error = MfPlusErrorProtocol;

    const size_t historical_bytes_len = simple_array_get_count(iso4_data->ats_data.t1_tk);
    if(historical_bytes_len != MF_PLUS_T1_TK_VALUE_LEN) {
        return MfPlusErrorProtocol;
    }
    const uint8_t* historical_bytes = simple_array_cget_data(iso4_data->ats_data.t1_tk);

    switch(iso4_data->iso14443_3a_data->sak) {
    case 0x08: {
        const MfPlusType type = new_type_from_ats(historical_bytes, historical_bytes_len);
        if(type != MfPlusTypeUnknown) {
            mf_plus_data->type = type;
            mf_plus_data->size = (type == MfPlusTypeSE) ? MfPlusSize1K : MfPlusSize2K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        }
        break;
    }
    case 0x10:
        mf_plus_data->type = MfPlusTypePlus;
        mf_plus_data->size = MfPlusSize2K;
        mf_plus_data->security_level = MfPlusSecurityLevel2;
        error = MfPlusErrorNone;
        break;
    case 0x11:
        mf_plus_data->type = MfPlusTypePlus;
        mf_plus_data->size = MfPlusSize4K;
        mf_plus_data->security_level = MfPlusSecurityLevel2;
        error = MfPlusErrorNone;
        break;
    case 0x18: {
        const MfPlusType type = new_type_from_ats(historical_bytes, historical_bytes_len);
        if(type == MfPlusTypeS || type == MfPlusTypeX) {
            mf_plus_data->type = type;
            mf_plus_data->size = MfPlusSize4K;
            mf_plus_data->security_level = MfPlusSecurityLevel1;
            error = MfPlusErrorNone;
        }
        break;
    }
    case 0x20: {
        // SAK 0x20 is shared with SL0 and DESFire; SL0-vs-SL3 needs the poller-side probe that is
        // out of scope here -- keep the existing SL3 default (see file header/port doc).
        const MfPlusType type = new_type_from_ats(historical_bytes, historical_bytes_len);
        if(type != MfPlusTypeUnknown) {
            mf_plus_data->type = type;
            mf_plus_data->security_level = MfPlusSecurityLevel3;
            if(type == MfPlusTypeSE) {
                mf_plus_data->size = MfPlusSize1K;
                error = MfPlusErrorNone;
            } else {
                mf_plus_data->size = new_size_from_atqa(iso4_data->iso14443_3a_data->atqa);
                if(mf_plus_data->size != MfPlusSizeUnknown) {
                    error = MfPlusErrorNone;
                }
            }
        }
        break;
    }
    }

    return error;
}

/* ================================================================================= */
/* Test harness                                                                       */
/* ================================================================================= */

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond, desc)                                     \
    do {                                                      \
        g_checks++;                                           \
        if(!(cond)) {                                         \
            g_failed++;                                       \
            printf("FAIL: %s (%s:%d)\n", desc, __FILE__, __LINE__); \
        }                                                      \
    } while(0)

static Iso14443_4aData
    make_iso4(uint8_t sak, const uint8_t atqa[2], const uint8_t* t1tk, uint32_t t1tk_len) {
    static Iso14443_3aData a3a;
    static SimpleArray arr;
    a3a.sak = sak;
    a3a.atqa[0] = atqa ? atqa[0] : 0;
    a3a.atqa[1] = atqa ? atqa[1] : 0;
    arr.data = t1tk;
    arr.count = t1tk_len;

    Iso14443_4aData d = {0};
    d.iso14443_3a_data = &a3a;
    d.ats_data.t1_tk = &arr;
    return d;
}

/* NXP AN10833/TagInfo reference ATS T1TK blocks (protocol constants, not captured card data) */
static const uint8_t ATS_S[7] = {0xC1, 0x05, 0x2F, 0x2F, 0x00, 0x35, 0xC7};
static const uint8_t ATS_X[7] = {0xC1, 0x05, 0x2F, 0x2F, 0x01, 0xBC, 0xD6};
static const uint8_t ATS_SE_A[7] = {0xC1, 0x05, 0x21, 0x30, 0x00, 0xF6, 0xD1}; // in ALT table
static const uint8_t ATS_SE_B[7] = {0xC1, 0x05, 0x21, 0x30, 0x10, 0xF6, 0xD1}; // in ALT table
// TagInfo v6.2.0 SE fingerprints cited in Unleashed's source comment, NOT in ALT's 4-entry table:
static const uint8_t ATS_SE_C[7] = {0xC1, 0x05, 0x21, 0x30, 0x00, 0x77, 0xC1};
static const uint8_t ATS_SE_D[7] = {0xC1, 0x05, 0x20, 0x30, 0x00, 0xAB, 0x9B};
// Synthetic 7-byte block that is NOT a MIFARE Plus C1-05 block (exercises the type gate only;
// not claimed to be a real DESFire fingerprint -- real DESFire T1TK is 5 bytes, see below).
static const uint8_t ATS_NON_PLUS_7[7] = {0x75, 0x77, 0x81, 0x02, 0x80, 0x00, 0x00};
// Real DESFire EV1 ATS historical bytes (5 bytes) -- length mismatch alone must reject it.
static const uint8_t ATS_DESFIRE_5[5] = {0x75, 0x77, 0x81, 0x02, 0x80};

static const uint8_t ATQA_2K[2] = {0x04, 0x00};
static const uint8_t ATQA_4K[2] = {0x02, 0x00};
static const uint8_t ATQA_NONE[2] = {0x00, 0x00};

int main(void) {
    /* ---- Testgroup 1: ATS nibble matching finds SE config variants the old exact table misses,
       both direct (SAK 0x08) and behind the SAK 0x20 gate ---- */
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x08, NULL, ATS_SE_C, 7);
        MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
        MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
        CHECK(ealt == MfPlusErrorProtocol, "T1a: ALT rejects TagInfo SE variant C (SAK 0x08)");
        CHECK(eneu == MfPlusErrorNone, "T1a: NEU recognizes TagInfo SE variant C (SAK 0x08)");
        CHECK(neu.type == MfPlusTypeSE, "T1a: NEU classifies variant C as SE");
        CHECK(neu.size == MfPlusSize1K, "T1a: NEU classifies variant C as 1K");
        CHECK(neu.security_level == MfPlusSecurityLevel1, "T1a: NEU classifies variant C as SL1");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x08, NULL, ATS_SE_D, 7);
        MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
        MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
        CHECK(ealt == MfPlusErrorProtocol, "T1b: ALT rejects TagInfo SE variant D (SAK 0x08)");
        CHECK(eneu == MfPlusErrorNone, "T1b: NEU recognizes TagInfo SE variant D (SAK 0x08)");
        CHECK(neu.type == MfPlusTypeSE, "T1b: NEU classifies variant D as SE");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x20, ATQA_NONE, ATS_SE_C, 7);
        MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
        MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
        CHECK(ealt == MfPlusErrorProtocol, "T1c: ALT rejects TagInfo SE variant C behind SAK 0x20");
        CHECK(eneu == MfPlusErrorNone, "T1c: NEU recognizes TagInfo SE variant C behind SAK 0x20");
        CHECK(neu.type == MfPlusTypeSE, "T1c: NEU classifies as SE");
        CHECK(
            neu.security_level == MfPlusSecurityLevel3, "T1c: NEU still defaults SAK 0x20 to SL3");
    }

    /* ---- Testgroup 2: nibble matcher still rejects non-Plus / gate boundary ---- */
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x20, ATQA_NONE, ATS_NON_PLUS_7, 7);
        MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
        MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
        CHECK(ealt == MfPlusErrorProtocol, "T2a: ALT rejects non-Plus 7-byte ATS at SAK 0x20");
        CHECK(eneu == MfPlusErrorProtocol, "T2a: NEU rejects non-Plus 7-byte ATS at SAK 0x20 too");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x20, ATQA_NONE, ATS_DESFIRE_5, 5);
        MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
        MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
        CHECK(ealt == MfPlusErrorProtocol, "T2b: ALT rejects real 5-byte DESFire ATS (length)");
        CHECK(eneu == MfPlusErrorProtocol, "T2b: NEU rejects real 5-byte DESFire ATS (length)");
    }

    /* ---- Testgroup 3: ATQA-based size fallback (GetVersion path, unrecognized storage byte) ---- */
    {
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x11;
        alt.version.hw_storage = 0x00; // unrecognized
        neu = alt;
        Iso14443_4aData d = make_iso4(0x08, ATQA_2K, NULL, 0);
        alt_get_type_from_version(&d, &alt);
        new_get_type_from_version(&d, &neu);
        CHECK(alt.size == MfPlusSizeUnknown, "T3a: ALT leaves size Unknown on bad storage byte");
        CHECK(neu.size == MfPlusSize2K, "T3a: NEU falls back to ATQA-derived 2K");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x11;
        alt.version.hw_storage = 0x00;
        neu = alt;
        Iso14443_4aData d = make_iso4(0x08, ATQA_4K, NULL, 0);
        alt_get_type_from_version(&d, &alt);
        new_get_type_from_version(&d, &neu);
        CHECK(neu.size == MfPlusSize4K, "T3b: NEU falls back to ATQA-derived 4K");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x11;
        alt.version.hw_storage = 0x00;
        neu = alt;
        Iso14443_4aData d = make_iso4(0x08, ATQA_NONE, NULL, 0);
        alt_get_type_from_version(&d, &alt);
        new_get_type_from_version(&d, &neu);
        CHECK(
            neu.size == MfPlusSizeUnknown,
            "T3c: NEU still Unknown when neither ATQA size bit is set (no regression)");
    }
    {
        /* recognized storage byte: NEU must behave identically to ALT (no ATQA override) */
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x11;
        alt.version.hw_storage = 0x16; // recognized: 2K
        neu = alt;
        Iso14443_4aData d = make_iso4(0x08, ATQA_4K, NULL, 0); // deliberately contradicting ATQA
        alt_get_type_from_version(&d, &alt);
        new_get_type_from_version(&d, &neu);
        CHECK(
            alt.size == MfPlusSize2K && neu.size == MfPlusSize2K,
            "T3d: recognized storage byte wins over ATQA in both ALT and NEU");
    }

    /* ---- Testgroup 4: SAK->SecurityLevel switch fix in get_type_from_version ---- */
    {
        /* the actual bug: SAK 0x10/0x11 (SL2) was silently collapsed into SL1 by ALT */
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x22;
        alt.version.hw_storage = 0x16;
        neu = alt;
        Iso14443_4aData d10 = make_iso4(0x10, NULL, NULL, 0);
        alt_get_type_from_version(&d10, &alt);
        new_get_type_from_version(&d10, &neu);
        CHECK(alt.security_level == MfPlusSecurityLevel1, "T4a: ALT mislabels SAK 0x10 as SL1");
        CHECK(neu.security_level == MfPlusSecurityLevel2, "T4a: NEU correctly labels SAK 0x10 SL2");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x22;
        alt.version.hw_storage = 0x18;
        neu = alt;
        Iso14443_4aData d11 = make_iso4(0x11, NULL, NULL, 0);
        alt_get_type_from_version(&d11, &alt);
        new_get_type_from_version(&d11, &neu);
        CHECK(alt.security_level == MfPlusSecurityLevel1, "T4b: ALT mislabels SAK 0x11 as SL1");
        CHECK(neu.security_level == MfPlusSecurityLevel2, "T4b: NEU correctly labels SAK 0x11 SL2");
    }
    {
        /* unrecognized SAK: ALT still guesses SL1, NEU now says Unknown instead of guessing */
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x02;
        alt.version.hw_major = 0x11;
        alt.version.hw_storage = 0x16;
        neu = alt;
        Iso14443_4aData d = make_iso4(0x04, NULL, NULL, 0);
        alt_get_type_from_version(&d, &alt);
        new_get_type_from_version(&d, &neu);
        CHECK(alt.security_level == MfPlusSecurityLevel1, "T4c: ALT guesses SL1 for unknown SAK");
        CHECK(
            neu.security_level == MfPlusSecurityLevelUnknown,
            "T4c: NEU reports Unknown instead of guessing for unrecognized SAK");
    }
    {
        /* regression: SAK 0x08/0x18/0x20 unchanged between ALT and NEU */
        const uint8_t saks[] = {0x08, 0x18, 0x20};
        const MfPlusSecurityLevel expect[] = {
            MfPlusSecurityLevel1, MfPlusSecurityLevel1, MfPlusSecurityLevel3};
        for(int i = 0; i < 3; i++) {
            MfPlusData alt = {0}, neu = {0};
            alt.version.hw_type = 0x02;
            alt.version.hw_major = 0x11;
            alt.version.hw_storage = 0x16;
            neu = alt;
            Iso14443_4aData d = make_iso4(saks[i], NULL, NULL, 0);
            alt_get_type_from_version(&d, &alt);
            new_get_type_from_version(&d, &neu);
            CHECK(
                alt.security_level == expect[i] && neu.security_level == expect[i],
                "T4d: regression, SAK 0x08/0x18/0x20 SL unchanged");
        }
    }
    {
        /* regression: hw_type not EV1/2 -> both ALT and NEU still return Protocol error */
        MfPlusData alt = {0}, neu = {0};
        alt.version.hw_type = 0x01; // not EV1/2
        alt.version.hw_major = 0x11;
        alt.version.hw_storage = 0x16;
        neu = alt;
        Iso14443_4aData d = make_iso4(0x08, NULL, NULL, 0);
        MfPlusError ealt = alt_get_type_from_version(&d, &alt);
        MfPlusError eneu = new_get_type_from_version(&d, &neu);
        CHECK(
            ealt == MfPlusErrorProtocol && eneu == MfPlusErrorProtocol,
            "T4e: regression, non-EV1/2 hw_type still Protocol error in both");
    }

    /* ---- Testgroup 5: SAK 0x10/0x11 generic MfPlusTypePlus instead of hardcoded X (iso4 path) ---
       (SAK 0x10/0x11 branches never read the ATS content, but both ALT and NEU still require a
       7-byte T1TK to pass the shared early length gate at the top of the function, so a dummy
       7-byte block is used here -- its content is never inspected on this path, verified by T5c.) */
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x10, NULL, ATS_S, 7);
        alt_get_type_from_iso4(&d, &alt);
        new_get_type_from_iso4(&d, &neu);
        CHECK(alt.type == MfPlusTypeX, "T5a: ALT over-claims SAK 0x10 as type X");
        CHECK(neu.type == MfPlusTypePlus, "T5a: NEU reports generic Plus for SAK 0x10");
        CHECK(
            alt.security_level == MfPlusSecurityLevel2 && neu.security_level == MfPlusSecurityLevel2,
            "T5a: SL2 unchanged");
        CHECK(alt.size == MfPlusSize2K && neu.size == MfPlusSize2K, "T5a: 2K unchanged");
    }
    {
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x11, NULL, ATS_X, 7);
        alt_get_type_from_iso4(&d, &alt);
        new_get_type_from_iso4(&d, &neu);
        CHECK(alt.type == MfPlusTypeX, "T5b: ALT over-claims SAK 0x11 as type X");
        CHECK(neu.type == MfPlusTypePlus, "T5b: NEU reports generic Plus for SAK 0x11");
    }
    {
        /* T5c: SAK 0x10/0x11 result is identical regardless of which (valid-length) ATS content is
           supplied -- confirms the branch truly ignores the ATS bytes in both ALT and NEU. */
        MfPlusData neu_s = {0}, neu_se = {0};
        Iso14443_4aData d_s = make_iso4(0x10, NULL, ATS_S, 7);
        Iso14443_4aData d_se = make_iso4(0x10, NULL, ATS_SE_C, 7);
        new_get_type_from_iso4(&d_s, &neu_s);
        new_get_type_from_iso4(&d_se, &neu_se);
        CHECK(
            neu_s.type == neu_se.type && neu_s.size == neu_se.size &&
                neu_s.security_level == neu_se.security_level,
            "T5c: SAK 0x10 result independent of ATS content in NEU");
    }
    {
        /* only a length mismatch (not SAK 0x10/0x11 content) can still reject at the shared gate */
        MfPlusData alt = {0}, neu = {0};
        Iso14443_4aData d = make_iso4(0x10, NULL, NULL, 0);
        MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
        MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
        CHECK(
            ealt == MfPlusErrorProtocol && eneu == MfPlusErrorProtocol,
            "T5d: SAK 0x10 with no/short ATS still hits the shared length gate in both");
    }

    /* ---- Testgroup 6: SAK 0x20 SL0/SL3 ambiguity -- non-regression, no SL0 claim in either ---- */
    {
        const uint8_t* ats_list[] = {ATS_S, ATS_X};
        for(int i = 0; i < 2; i++) {
            MfPlusData alt = {0}, neu = {0};
            Iso14443_4aData d = make_iso4(0x20, ATQA_2K, ats_list[i], 7);
            alt_get_type_from_iso4(&d, &alt);
            new_get_type_from_iso4(&d, &neu);
            CHECK(
                alt.security_level == MfPlusSecurityLevel3 &&
                    neu.security_level == MfPlusSecurityLevel3,
                "T6: SAK 0x20 defaults to SL3 in both ALT and NEU (never SL0 without the poller "
                "probe)");
        }
    }

    /* ---- Testgroup 7: full regression matrix over the original ALT reference table (all four
       entries, both SAK 0x08/0x18/0x20 paths, both size branches for 0x20) must classify
       identically under ALT and NEU ---- */
    {
        struct {
            uint8_t sak;
            const uint8_t* ats;
            const uint8_t* atqa;
        } cases[] = {
            {0x08, ATS_S, NULL},
            {0x08, ATS_X, NULL},
            {0x08, ATS_SE_A, NULL},
            {0x08, ATS_SE_B, NULL},
            {0x18, ATS_S, NULL},
            {0x18, ATS_X, NULL},
            {0x20, ATS_S, ATQA_2K},
            {0x20, ATS_S, ATQA_4K},
            {0x20, ATS_X, ATQA_2K},
            {0x20, ATS_X, ATQA_4K},
            {0x20, ATS_SE_A, ATQA_NONE},
            {0x20, ATS_SE_B, ATQA_NONE},
        };
        for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            MfPlusData alt = {0}, neu = {0};
            Iso14443_4aData d = make_iso4(cases[i].sak, cases[i].atqa, cases[i].ats, 7);
            MfPlusError ealt = alt_get_type_from_iso4(&d, &alt);
            MfPlusError eneu = new_get_type_from_iso4(&d, &neu);
            CHECK(ealt == MfPlusErrorNone, "T7: ALT reference case still succeeds");
            CHECK(eneu == MfPlusErrorNone, "T7: NEU reference case still succeeds");
            CHECK(alt.type == neu.type, "T7: type identical ALT vs NEU on reference table");
            CHECK(alt.size == neu.size, "T7: size identical ALT vs NEU on reference table");
            CHECK(
                alt.security_level == neu.security_level,
                "T7: security_level identical ALT vs NEU on reference table");
        }
    }

    printf("%d checks, %d failed\n", g_checks, g_failed);
    if(g_failed == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    }
    return 1;
}
