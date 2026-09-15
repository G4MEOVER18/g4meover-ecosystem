// Host repro for the MIFARE Ultralight AES Phase 2 patch (struct extension + serialization,
// analogous to the already-delivered MIFARE Plus SL3 Phase 2): compiles the actual, unmodified
// (bar the two host-only include swaps documented in mf_ultralight.h) patched
// mf_ultralight.c/.h and exercises every new code path plus every touched existing path
// (reset/copy/is_equal/load/save) for regressions.
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I stubs repro.c
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

static MfUltralightData* alloc_poisoned(uint8_t poison_byte) {
    // Simulate mf_ultralight_alloc()'s real, non-zeroing malloc(): fill the whole struct with a
    // fixed non-zero pattern first, then allocate the one real dynamic member the normal way.
    MfUltralightData* data = malloc(sizeof(MfUltralightData));
    memset(data, poison_byte, sizeof(*data));
    // Except `page`: mf_ultralight_is_equal() memcmp's the full, fixed-size COUNT_OF(data->page)
    // array regardless of pages_total, a pre-existing characteristic unrelated to this patch (it
    // means two same-type cards with different allocator history and pages_total < 510 would
    // already disagree on that array's untouched tail, independent of anything under test here).
    // Zeroed uniformly so these tests exercise the new AES fields, not that unrelated behavior.
    memset(data->page, 0, sizeof(data->page));
    data->iso14443_3a_data = iso14443_3a_alloc();
    return data;
}

static void fill_minimal_card(MfUltralightData* data, MfUltralightType type, uint8_t uid_seed) {
    uint8_t uid[7];
    for(size_t i = 0; i < sizeof(uid); i++) uid[i] = (uint8_t)(uid_seed + i);
    mf_ultralight_set_uid(data, uid, sizeof(uid));
    data->iso14443_3a_data->atqa[0] = 0x44;
    data->iso14443_3a_data->atqa[1] = 0x00;
    data->iso14443_3a_data->sak = 0x00;
    data->type = type;
    data->pages_total = mf_ultralight_get_pages_total(type);
    if(data->pages_total > MF_ULTRALIGHT_MAX_PAGE_NUM) data->pages_total = MF_ULTRALIGHT_MAX_PAGE_NUM;
    data->pages_read = data->pages_total;
    for(size_t i = 0; i < data->pages_total; i++) {
        for(size_t b = 0; b < MF_ULTRALIGHT_PAGE_SIZE; b++) {
            data->page[i].data[b] = (uint8_t)(i * 4 + b);
        }
    }
    for(size_t i = 0; i < MF_ULTRALIGHT_COUNTER_NUM; i++) data->counter[i].counter = 0;
    for(size_t i = 0; i < MF_ULTRALIGHT_TEARING_FLAG_NUM; i++) data->tearing_flag[i].data = 0xBD;
    data->auth_attempts = 0;
}

// --- Group 1: reset() clears the new fields ------------------------------------------------

static void test_reset(void) {
    MfUltralightData* data = alloc_poisoned(0xAA);
    mf_ultralight_reset(data);

    CHECK(data->aes_signature_present == false, "reset: aes_signature_present not cleared");
    uint8_t zero[MF_ULTRALIGHT_AES_SIGNATURE_SIZE] = {0};
    CHECK(
        memcmp(data->aes_signature, zero, sizeof(zero)) == 0,
        "reset: aes_signature not zeroed");

    iso14443_3a_free(data->iso14443_3a_data);
    free(data);
}

// --- Group 2: copy() propagates the new fields, independent buffers ------------------------

static void test_copy(void) {
    MfUltralightData* src = alloc_poisoned(0x11);
    mf_ultralight_reset(src);
    fill_minimal_card(src, MfUltralightTypeUltralightAES, 0x10);
    src->aes_signature_present = true;
    for(size_t i = 0; i < MF_ULTRALIGHT_AES_SIGNATURE_SIZE; i++) src->aes_signature[i] = (uint8_t)(i + 1);

    MfUltralightData* dst = alloc_poisoned(0x22);
    mf_ultralight_reset(dst);
    mf_ultralight_copy(dst, src);

    CHECK(dst->aes_signature_present == true, "copy: aes_signature_present not propagated");
    CHECK(
        memcmp(dst->aes_signature, src->aes_signature, MF_ULTRALIGHT_AES_SIGNATURE_SIZE) == 0,
        "copy: aes_signature bytes not propagated");

    // Independence: mutating the source afterwards must not affect the copy.
    src->aes_signature[0] = 0xFF;
    CHECK(dst->aes_signature[0] == 1, "copy: destination aliases source buffer");

    iso14443_3a_free(src->iso14443_3a_data);
    free(src);
    iso14443_3a_free(dst->iso14443_3a_data);
    free(dst);
}

// --- Group 3: full save/load round trip WITH an AES signature ------------------------------

static void test_round_trip_with_signature(void) {
    MfUltralightData* orig = alloc_poisoned(0x33);
    mf_ultralight_reset(orig);
    fill_minimal_card(orig, MfUltralightTypeUltralightAES, 0x20);
    orig->aes_signature_present = true;
    for(size_t i = 0; i < MF_ULTRALIGHT_AES_SIGNATURE_SIZE; i++)
        orig->aes_signature[i] = (uint8_t)(0xA0 + i);

    FlipperFormat* ff = flipper_format_alloc();
    CHECK(mf_ultralight_save(orig, ff), "round-trip(with sig): save failed");

    MfUltralightData* loaded = alloc_poisoned(0x44);
    mf_ultralight_reset(loaded);
    CHECK(
        mf_ultralight_load(loaded, ff, 2u),
        "round-trip(with sig): load failed");

    CHECK(loaded->aes_signature_present == true, "round-trip(with sig): presence lost");
    CHECK(
        memcmp(loaded->aes_signature, orig->aes_signature, MF_ULTRALIGHT_AES_SIGNATURE_SIZE) == 0,
        "round-trip(with sig): signature bytes changed");
    CHECK(mf_ultralight_is_equal(orig, loaded), "round-trip(with sig): is_equal mismatch");

    flipper_format_free(ff);
    iso14443_3a_free(orig->iso14443_3a_data);
    free(orig);
    iso14443_3a_free(loaded->iso14443_3a_data);
    free(loaded);
}

// --- Group 4: legacy/non-AES round trip is byte-for-byte unaffected ------------------------

static void test_round_trip_without_signature(void) {
    MfUltralightData* orig = alloc_poisoned(0x55);
    mf_ultralight_reset(orig);
    fill_minimal_card(orig, MfUltralightTypeMfulC, 0x30);
    // aes_signature_present stays false (as reset() left it) -- this is the common case: every
    // card that isn't UL-AES, plus every dump saved before this patch existed.

    FlipperFormat* ff = flipper_format_alloc();
    CHECK(mf_ultralight_save(orig, ff), "round-trip(no sig): save failed");
    CHECK(
        ff_find(ff, "AES signature") == NULL,
        "round-trip(no sig): AES signature key written for a card that never captured one");

    MfUltralightData* loaded = alloc_poisoned(0x66); // different poison than orig's 0x55
    mf_ultralight_reset(loaded);
    CHECK(
        mf_ultralight_load(loaded, ff, 2u),
        "round-trip(no sig): load failed");

    CHECK(
        loaded->aes_signature_present == false,
        "round-trip(no sig): presence flag incorrectly set");
    uint8_t zero[MF_ULTRALIGHT_AES_SIGNATURE_SIZE] = {0};
    CHECK(
        memcmp(loaded->aes_signature, zero, sizeof(zero)) == 0,
        "round-trip(no sig): buffer left as heap garbage instead of zero");
    CHECK(mf_ultralight_is_equal(orig, loaded), "round-trip(no sig): is_equal mismatch");

    flipper_format_free(ff);
    iso14443_3a_free(orig->iso14443_3a_data);
    free(orig);
    iso14443_3a_free(loaded->iso14443_3a_data);
    free(loaded);
}

// --- Group 5: ALT (naive 1:1 port) vs NEU (this patch) on the is_equal() instability -------
//
// Unleashed's own one-liner is `data->aes_signature_present = flipper_format_read_hex(...)`,
// with no prior memset. mf_ultralight_alloc() does not zero-initialize (plain malloc()), so on
// two independently allocated cards that both lack the optional key, the naive port leaves
// `aes_signature` holding whatever the allocator's memory previously contained -- and
// mf_ultralight_is_equal() memcmp's that array unconditionally, regardless of the presence flag.
// This reproduces exactly that scenario using the same stub flipper_format_read_hex() the real
// load() calls, without needing a second copy of the whole file.

static bool naive_read_aes_signature(FlipperFormat* ff, uint8_t* out_signature) {
    // Mirrors Unleashed's literal one-liner: no memset before the read.
    return flipper_format_read_hex(
        ff, "AES signature", out_signature, MF_ULTRALIGHT_AES_SIGNATURE_SIZE);
}

static void test_alt_vs_neu_uninitialized_buffer(void) {
    FlipperFormat* ff = flipper_format_alloc(); // no "AES signature" key -- the common case

    uint8_t card_a[MF_ULTRALIGHT_AES_SIGNATURE_SIZE];
    uint8_t card_b[MF_ULTRALIGHT_AES_SIGNATURE_SIZE];
    memset(card_a, 0x5A, sizeof(card_a)); // stand-in for card A's malloc() leftovers
    memset(card_b, 0xC3, sizeof(card_b)); // stand-in for card B's malloc() leftovers (different)

    bool present_a = naive_read_aes_signature(ff, card_a);
    bool present_b = naive_read_aes_signature(ff, card_b);

    CHECK(present_a == false && present_b == false, "ALT: presence flag itself is still correct");
    // This is the defect: two cards that both genuinely lack an AES signature disagree on the
    // buffer contents purely because of allocator history, and is_equal() would call them
    // different.
    bool alt_buffers_equal = memcmp(card_a, card_b, sizeof(card_a)) == 0;
    CHECK(!alt_buffers_equal, "ALT: expected the naive port's known instability to reproduce");

    // NEU: the actual patched mf_ultralight_load(), same two poisoned allocations, same
    // signature-less file.
    MfUltralightData* loaded_a = alloc_poisoned(0x5A);
    mf_ultralight_reset(loaded_a);
    fill_minimal_card(loaded_a, MfUltralightTypeMfulC, 0x01);
    FlipperFormat* ff_a = flipper_format_alloc();
    CHECK(mf_ultralight_save(loaded_a, ff_a), "NEU: save (card A) failed");

    MfUltralightData* loaded_b = alloc_poisoned(0xC3); // different poison than card A
    mf_ultralight_reset(loaded_b);
    CHECK(
        mf_ultralight_load(loaded_b, ff_a, 2u),
        "NEU: load (card B, from card A's dump) failed");

    CHECK(
        memcmp(loaded_a->aes_signature, loaded_b->aes_signature, MF_ULTRALIGHT_AES_SIGNATURE_SIZE) ==
            0,
        "NEU: differently-poisoned allocations still disagree after load()");
    CHECK(
        mf_ultralight_is_equal(loaded_a, loaded_b),
        "NEU: is_equal still unstable across allocator garbage");

    flipper_format_free(ff);
    flipper_format_free(ff_a);
    iso14443_3a_free(loaded_a->iso14443_3a_data);
    free(loaded_a);
    iso14443_3a_free(loaded_b->iso14443_3a_data);
    free(loaded_b);
}

// --- Group 6: mf_ultralight_aes_get_key() byte-reversal correctness ------------------------

static void set_key_pages(MfUltralightData* data, const uint8_t* stored_16_bytes) {
    for(size_t i = 0; i < MF_ULTRALIGHT_AES_KEY_SIZE / MF_ULTRALIGHT_PAGE_SIZE; i++) {
        memcpy(
            data->page[MF_ULTRALIGHT_AES_DATA_KEY_PAGE + i].data,
            stored_16_bytes + i * MF_ULTRALIGHT_PAGE_SIZE,
            MF_ULTRALIGHT_PAGE_SIZE);
    }
}

static void test_aes_get_key(void) {
    MfUltralightData* data = alloc_poisoned(0x77);
    mf_ultralight_reset(data);

    // Case 1: monotonic 0..15 stored reversed -> key must read back as 15..0.
    uint8_t stored1[MF_ULTRALIGHT_AES_KEY_SIZE];
    for(size_t i = 0; i < MF_ULTRALIGHT_AES_KEY_SIZE; i++) stored1[i] = (uint8_t)i;
    set_key_pages(data, stored1);
    uint8_t key1[MF_ULTRALIGHT_AES_KEY_SIZE];
    mf_ultralight_aes_get_key(data, key1);
    for(size_t i = 0; i < MF_ULTRALIGHT_AES_KEY_SIZE; i++) {
        CHECK(key1[i] == (uint8_t)(MF_ULTRALIGHT_AES_KEY_SIZE - 1 - i), "aes_get_key: case 1 mismatch");
    }

    // Case 2: all-zero -> reversal is a no-op, key must be all-zero.
    uint8_t stored2[MF_ULTRALIGHT_AES_KEY_SIZE] = {0};
    set_key_pages(data, stored2);
    uint8_t key2[MF_ULTRALIGHT_AES_KEY_SIZE];
    mf_ultralight_aes_get_key(data, key2);
    uint8_t zero[MF_ULTRALIGHT_AES_KEY_SIZE] = {0};
    CHECK(memcmp(key2, zero, sizeof(zero)) == 0, "aes_get_key: case 2 (all-zero) mismatch");

    // Case 3: known reference key, reversed by hand, round-tripped through the accessor.
    static const uint8_t reference_key[MF_ULTRALIGHT_AES_KEY_SIZE] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c}; // AES-128 FIPS/RFC-4493 test key
    uint8_t stored3[MF_ULTRALIGHT_AES_KEY_SIZE];
    for(size_t i = 0; i < MF_ULTRALIGHT_AES_KEY_SIZE; i++)
        stored3[i] = reference_key[MF_ULTRALIGHT_AES_KEY_SIZE - 1 - i];
    set_key_pages(data, stored3);
    uint8_t key3[MF_ULTRALIGHT_AES_KEY_SIZE];
    mf_ultralight_aes_get_key(data, key3);
    CHECK(memcmp(key3, reference_key, sizeof(reference_key)) == 0, "aes_get_key: case 3 mismatch");

    iso14443_3a_free(data->iso14443_3a_data);
    free(data);
}

// --- Group 7: new type wiring sanity --------------------------------------------------------

static void test_type_wiring(void) {
    CHECK(
        mf_ultralight_get_pages_total(MfUltralightTypeUltralightAES) == 60,
        "type wiring: total_pages != 60");
    CHECK(
        mf_ultralight_get_config_page_num(MfUltralightTypeUltralightAES) == 0,
        "type wiring: config_page != 0");

    uint32_t features = mf_ultralight_get_feature_support_set(MfUltralightTypeUltralightAES);
    CHECK(
        mf_ultralight_support_feature(features, MfUltralightFeatureSupportReadVersion),
        "type wiring: missing ReadVersion");
    CHECK(
        mf_ultralight_support_feature(features, MfUltralightFeatureSupportAuthenticate),
        "type wiring: missing Authenticate");
    CHECK(
        mf_ultralight_support_feature(features, MfUltralightFeatureSupportReadCounter),
        "type wiring: missing ReadCounter");
    CHECK(
        mf_ultralight_support_feature(features, MfUltralightFeatureSupportIncCounter),
        "type wiring: missing IncCounter");
    // Deliberately not modeled yet (see port doc): no ReadSignature (48-byte sig doesn't fit the
    // shared 32-byte struct), no FastRead, no PasswordAuth.
    CHECK(
        !mf_ultralight_support_feature(features, MfUltralightFeatureSupportReadSignature),
        "type wiring: unexpectedly has ReadSignature");
    CHECK(
        !mf_ultralight_support_feature(features, MfUltralightFeatureSupportFastRead),
        "type wiring: unexpectedly has FastRead");
    CHECK(
        !mf_ultralight_support_feature(features, MfUltralightFeatureSupportPasswordAuth),
        "type wiring: unexpectedly has PasswordAuth");

    MfUltralightData* data = alloc_poisoned(0x88);
    mf_ultralight_reset(data);
    data->type = MfUltralightTypeUltralightAES;
    const char* name = mf_ultralight_get_device_name(data, NfcDeviceNameTypeFull);
    CHECK(strcmp(name, "Mifare Ultralight AES") == 0, "type wiring: device name mismatch");

    iso14443_3a_free(data->iso14443_3a_data);
    free(data);
}

// --- Group 8: pre-existing type round trip is untouched by this patch ----------------------

static void test_regression_existing_type(void) {
    MfUltralightData* a = alloc_poisoned(0x99);
    mf_ultralight_reset(a);
    fill_minimal_card(a, MfUltralightTypeUL21, 0x40);
    for(size_t i = 0; i < MF_ULTRALIGHT_SIGNATURE_SIZE; i++) a->signature.data[i] = (uint8_t)(i * 3);

    FlipperFormat* ff = flipper_format_alloc();
    CHECK(mf_ultralight_save(a, ff), "regression: save failed");

    MfUltralightData* b = alloc_poisoned(0x66);
    mf_ultralight_reset(b);
    CHECK(
        mf_ultralight_load(b, ff, 2u), "regression: load failed");
    CHECK(mf_ultralight_is_equal(a, b), "regression: is_equal mismatch for untouched type");

    flipper_format_free(ff);
    iso14443_3a_free(a->iso14443_3a_data);
    free(a);
    iso14443_3a_free(b->iso14443_3a_data);
    free(b);
}

int main(void) {
    test_reset();
    test_copy();
    test_round_trip_with_signature();
    test_round_trip_without_signature();
    test_alt_vs_neu_uninitialized_buffer();
    test_aes_get_key();
    test_type_wiring();
    test_regression_existing_type();

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", g_failures);
        return 1;
    }
}
