// Host repro for the MIFARE Ultralight/NTAG type-detection rework: compiles the actual,
// unmodified (bar the host-only include swap already established by the earlier UL ports)
// patched mf_ultralight.c/.h and exercises the real mf_ultralight_get_type_by_version() plus
// the three new feature-table entries (NTAG210, NTAG212, Unknown) it can now return.
//
// old_get_type_by_version() below is a literal, unmodified copy of my pre-patch base logic
// (mf_ultralight.c.orig / my main branch), kept purely for the ALT-vs-NEU comparison in group 4;
// it is not part of the patch and duplicates none of the real function's now-current behavior.
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I . -I stubs repro.c
//        mf_ultralight.c -o repro && ./repro
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mf_ultralight.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if(!(cond)) {                                                        \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            g_failures++;                                                    \
        }                                                                    \
    } while(0)

static MfUltralightVersion make_version(
    uint8_t prod_type,
    uint8_t prod_subtype,
    uint8_t prod_ver_major,
    uint8_t prod_ver_minor,
    uint8_t storage_size) {
    // header/vendor_id/protocol_type play no role in the function under test; fixed real-world
    // values (0x00, NXP 0x04, 0x03) so a stray read of them would still look plausible instead
    // of silently passing on an obviously-poisoned host artifact.
    MfUltralightVersion version = {
        .header = 0x00,
        .vendor_id = 0x04,
        .prod_type = prod_type,
        .prod_subtype = prod_subtype,
        .prod_ver_major = prod_ver_major,
        .prod_ver_minor = prod_ver_minor,
        .storage_size = storage_size,
        .protocol_type = 0x03,
    };
    return version;
}

// Literal copy of my base main-branch function, kept only as the OLD side of the group 4
// ALT-vs-NEU comparison below.
static MfUltralightType old_get_type_by_version(MfUltralightVersion* version) {
    MfUltralightType type = MfUltralightTypeOrigin;

    if(version->storage_size == 0x0B || version->storage_size == 0x00) {
        type = MfUltralightTypeUL11;
    } else if(version->storage_size == 0x0E) {
        type = MfUltralightTypeUL21;
    } else if(version->storage_size == 0x0F) {
        type = MfUltralightTypeNTAG213;
    } else if(version->storage_size == 0x11) {
        type = MfUltralightTypeNTAG215;
    } else if(version->prod_subtype == 5 && version->prod_ver_major == 2) {
        if(version->prod_ver_minor == 1) {
            if(version->storage_size == 0x13) {
                type = MfUltralightTypeNTAGI2C1K;
            } else if(version->storage_size == 0x15) {
                type = MfUltralightTypeNTAGI2C2K;
            }
        } else if(version->prod_ver_minor == 2) {
            if(version->storage_size == 0x13) {
                type = MfUltralightTypeNTAGI2CPlus1K;
            } else if(version->storage_size == 0x15) {
                type = MfUltralightTypeNTAGI2CPlus2K;
            }
        }
    } else if(version->storage_size == 0x13) {
        type = MfUltralightTypeNTAG216;
    }

    return type;
}

// --- Group 1: pre-existing, non-colliding storage_size values -- unchanged behavior ---

static void test_unchanged_sizes(void) {
    // Real Ultralight/NTAG chips (prod_type family nibble 0x03) at storage_size values that
    // never collided with an NTAG-family counterpart: must still map exactly as before.
    MfUltralightVersion v;

    v = make_version(0x03, 0x01, 0x01, 0x01, 0x0F);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAG213,
        "0x0F/family 0x03 still types as NTAG213");

    v = make_version(0x03, 0x01, 0x01, 0x01, 0x11);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAG215,
        "0x11 still types as NTAG215 regardless of family");

    v = make_version(0x03, 0x01, 0x01, 0x01, 0x13);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAG216,
        "0x13 still types as NTAG216 regardless of family");
}

// --- Group 2: the two storage_size collisions -- new family-nibble disambiguation ---

static void test_ul_vs_ntag_collision(void) {
    // storage_size 0x0B (and its 0x00 alias) is shared by MF0UL11 and NTAG210; 0x0E is shared
    // by MF0UL21 and NTAG212. Real silicon disambiguates via the prod_type family nibble
    // (0x03 = Ultralight, 0x04 = NTAG); this is the actual new behavior this patch adds.
    MfUltralightVersion v;

    v = make_version(0x03, 0x01, 0x01, 0x01, 0x0B);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUL11,
        "0x0B/family 0x03 (Ultralight) still types as UL11");

    v = make_version(0x03, 0x01, 0x01, 0x01, 0x00);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUL11,
        "0x00/family 0x03 still types as UL11 (0x00 is the 0x0B alias)");

    v = make_version(0x04, 0x01, 0x01, 0x01, 0x0B);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAG210,
        "0x0B/family 0x04 (NTAG) now types as NTAG210, not UL11");

    v = make_version(0x04, 0x01, 0x01, 0x01, 0x00);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAG210,
        "0x00/family 0x04 also types as NTAG210 (0x00 is the 0x0B alias)");

    v = make_version(0x03, 0x01, 0x01, 0x01, 0x0E);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUL21,
        "0x0E/family 0x03 (Ultralight) still types as UL21");

    v = make_version(0x04, 0x01, 0x01, 0x01, 0x0E);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAG212,
        "0x0E/family 0x04 (NTAG) now types as NTAG212, not UL21");
}

static void test_non_standard_prod_type_stays_lenient(void) {
    // A clone reporting neither family nibble (e.g. 0x00) must still type by storage_size alone,
    // exactly as the pre-existing "lenient" logic did -- the family check only disambiguates the
    // two colliding sizes, it never becomes a hard requirement for every other card.
    MfUltralightVersion v = make_version(0x00, 0x01, 0x01, 0x01, 0x0B);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUL11,
        "non-standard prod_type at 0x0B still falls back to UL11, not NTAG210");

    v = make_version(0x00, 0x01, 0x01, 0x01, 0x0E);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUL21,
        "non-standard prod_type at 0x0E still falls back to UL21, not NTAG212");
}

// --- Group 3: NTAG I2C branch -- unchanged matches, new Unknown fallback ---

static void test_ntag_i2c_unchanged_and_unknown_fallback(void) {
    MfUltralightVersion v;

    v = make_version(0x04, 5, 2, 1, 0x13);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAGI2C1K,
        "subtype 5/major 2/minor 1/0x13 still types as NTAG I2C 1K");

    v = make_version(0x04, 5, 2, 1, 0x15);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAGI2C2K,
        "subtype 5/major 2/minor 1/0x15 still types as NTAG I2C 2K");

    v = make_version(0x04, 5, 2, 2, 0x13);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAGI2CPlus1K,
        "subtype 5/major 2/minor 2/0x13 still types as NTAG I2C Plus 1K");

    v = make_version(0x04, 5, 2, 2, 0x15);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeNTAGI2CPlus2K,
        "subtype 5/major 2/minor 2/0x15 still types as NTAG I2C Plus 2K");

    // Inside the NTAG I2C branch (subtype 5, major 2) but a storage_size neither minor value
    // recognizes: before this patch this fell all the way through to the implicit
    // MfUltralightTypeOrigin default: now it is the dedicated Unknown type instead.
    v = make_version(0x04, 5, 2, 1, 0x99);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUnknown,
        "unmatched size inside the NTAG I2C branch types as Unknown, not Origin");

    v = make_version(0x04, 5, 2, 3, 0x13);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUnknown,
        "subtype 5/major 2 with an unrecognized minor types as Unknown, not Origin");
}

static void test_default_unknown_fallback(void) {
    // A storage_size this logic has never heard of, outside the NTAG I2C branch: before this
    // patch, GetVersion succeeding with no matching branch left `type` at its initial
    // MfUltralightTypeOrigin value -- a real IC silently mislabeled as "no GetVersion reply".
    MfUltralightVersion v = make_version(0x03, 0x01, 0x01, 0x01, 0xFF);
    CHECK(
        mf_ultralight_get_type_by_version(&v) == MfUltralightTypeUnknown,
        "wholly unrecognized storage_size types as Unknown, not Origin");
}

// --- Group 4: ALT-vs-NEU -- demonstrates this is a real mislabeling fix, not cosmetic ---

static void test_alt_vs_neu(void) {
    // A real NTAG210 (family 0x04, storage_size 0x0B): the pre-patch base logic had no
    // family-nibble check at all, so this card was silently typed as MfUltralightTypeUL11 (20
    // pages, EV1-counter feature bits it doesn't actually have) instead of being recognized as
    // its own, correctly-featured NTAG210 type.
    MfUltralightVersion ntag210 = make_version(0x04, 0x01, 0x01, 0x01, 0x0B);
    CHECK(
        old_get_type_by_version(&ntag210) == MfUltralightTypeUL11,
        "ALT: base logic mistyped a real NTAG210 as UL11");
    CHECK(
        mf_ultralight_get_type_by_version(&ntag210) == MfUltralightTypeNTAG210,
        "NEU: patched logic correctly types it as NTAG210");

    // A GetVersion reply this firmware has never modeled: the pre-patch base logic left it as
    // MfUltralightTypeOrigin, indistinguishable from a card that never replied to GetVersion at
    // all (16-page assumption, wrong pages_total for a card that plainly supports GetVersion).
    MfUltralightVersion unknown_ic = make_version(0x03, 0x01, 0x01, 0x01, 0xFF);
    CHECK(
        old_get_type_by_version(&unknown_ic) == MfUltralightTypeOrigin,
        "ALT: base logic mistyped an unmodeled IC as Origin (no-GetVersion original Ultralight)");
    CHECK(
        mf_ultralight_get_type_by_version(&unknown_ic) == MfUltralightTypeUnknown,
        "NEU: patched logic correctly types it as Unknown instead");
}

// --- Group 5: new feature-table entries ---

static void test_feature_table_entries(void) {
    CHECK(
        mf_ultralight_get_pages_total(MfUltralightTypeNTAG210) == 20,
        "NTAG210: 20 total pages, same layout as UL11");
    CHECK(
        mf_ultralight_get_config_page_num(MfUltralightTypeNTAG210) == 16,
        "NTAG210: config page 16, same as UL11");
    CHECK(
        mf_ultralight_get_feature_support_set(MfUltralightTypeNTAG210) &
            MfUltralightFeatureSupportPasswordAuth,
        "NTAG210: supports password auth");
    CHECK(
        !(mf_ultralight_get_feature_support_set(MfUltralightTypeNTAG210) &
          MfUltralightFeatureSupportReadCounter),
        "NTAG210: no EV1 counter support, unlike UL11");

    CHECK(
        mf_ultralight_get_pages_total(MfUltralightTypeNTAG212) == 41,
        "NTAG212: 41 total pages, same layout as UL21");
    CHECK(
        mf_ultralight_get_config_page_num(MfUltralightTypeNTAG212) == 37,
        "NTAG212: config page 37, same as UL21");
    CHECK(
        mf_ultralight_get_feature_support_set(MfUltralightTypeNTAG212) &
            MfUltralightFeatureSupportDynamicLock,
        "NTAG212: supports dynamic lock, same as UL21");
    CHECK(
        !(mf_ultralight_get_feature_support_set(MfUltralightTypeNTAG212) &
          MfUltralightFeatureSupportReadCounter),
        "NTAG212: no EV1 counter support, unlike UL21");

    CHECK(
        mf_ultralight_get_pages_total(MfUltralightTypeUnknown) == 16,
        "Unknown: conservative 16-page read, same as Origin");
    CHECK(
        mf_ultralight_get_config_page_num(MfUltralightTypeUnknown) == 0,
        "Unknown: no config page assumed");
    CHECK(
        mf_ultralight_get_feature_support_set(MfUltralightTypeUnknown) ==
            MfUltralightFeatureSupportCompatibleWrite,
        "Unknown: exactly the same feature set the old implicit Origin fallback carried");
}

int main(void) {
    test_unchanged_sizes();
    test_ul_vs_ntag_collision();
    test_non_standard_prod_type_stays_lenient();
    test_ntag_i2c_unchanged_and_unknown_fallback();
    test_default_unknown_fallback();
    test_alt_vs_neu();
    test_feature_table_entries();

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", g_failures);
        return 1;
    }
}
