// Host verification for the MIFARE Plus SL3 struct + serialization port (Phase 2 of
// firmware-mining/mfplus-mfultralight-aes-auth-port.md). Exercises the ported
// mf_plus.c/mf_plus_i.c code UNMODIFIED against a minimal FlipperFormat/FuriString/
// iso14443_4a stand-in: full mf_plus_alloc -> mf_plus_save -> mf_plus_load round trip through
// a real FlipperFormat*-shaped API, plus mf_plus_copy/mf_plus_is_equal/mf_plus_merge_update and
// every new accessor.
//
// Build & run:
//   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
//       -I. -Istubs -Istubs/bit_lib \
//       mf_plus.c mf_plus_i.c repro.c -o /tmp/repro && /tmp/repro
#include "mf_plus_i.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(cond, msg) \
    do { \
        if(!(cond)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++; \
        } \
    } while(0)

static void fill_block(MfPlusBlock* b, uint8_t seed) {
    for(int i = 0; i < MF_PLUS_BLOCK_SIZE; i++) b->data[i] = (uint8_t)(seed + i);
}

static void fill_key(MfPlusKey* k, uint8_t seed) {
    for(int i = 0; i < MF_PLUS_KEY_SIZE; i++) k->data[i] = (uint8_t)(seed * 3 + i);
}

// Populate a 1K SL3 card: every block, every sector key A (not B), two of four admin keys,
// three of four config blocks, and an originality signature -- a deliberately partial,
// mixed-mask dump (the realistic case: a dictionary attack rarely recovers everything).
static void populate_partial_sl3_1k(MfPlusData* data) {
    data->security_level = MfPlusSecurityLevel3;
    data->size = MfPlusSize1K;
    data->type = MfPlusTypeS;

    const uint16_t blocks = mf_plus_get_block_count(data->size);
    for(uint16_t i = 0; i < blocks; i++) {
        MfPlusBlock b;
        fill_block(&b, (uint8_t)i);
        mf_plus_set_block_read(data, i, &b);
    }

    const uint8_t sectors = mf_plus_get_sector_count(data->size);
    for(uint8_t s = 0; s < sectors; s++) {
        MfPlusKey k;
        fill_key(&k, s);
        mf_plus_set_key_found(data, s, MfPlusKeyTypeA, &k);
        // Key B intentionally left unknown for every sector.
    }

    MfPlusKey admin;
    fill_key(&admin, 0xA0);
    mf_plus_set_admin_key_found(data, MfPlusAdminKeyCardMaster, &admin);
    fill_key(&admin, 0xA1);
    mf_plus_set_admin_key_found(data, MfPlusAdminKeyL3Switch, &admin);
    // CardConfig / SL1CardAuth intentionally left unknown.

    MfPlusBlock cfg;
    for(uint8_t c = 0; c < MF_PLUS_CONFIG_BLOCK_NUM - 1; c++) {
        fill_block(&cfg, (uint8_t)(0xC0 + c));
        mf_plus_set_config_block_read(data, c, &cfg);
    }
    // Config block MF_PLUS_CONFIG_BLOCK_NUM-1 intentionally left unread.

    data->signature_present = true;
    for(int i = 0; i < MF_PLUS_SIGNATURE_SIZE; i++) data->signature[i] = (uint8_t)(0xE0 + i);
}

static void test_accessors_and_geometry(void) {
    CHECK(mf_plus_get_block_count(MfPlusSize1K) == 64, "1K block count");
    CHECK(mf_plus_get_block_count(MfPlusSize2K) == 128, "2K block count");
    CHECK(mf_plus_get_block_count(MfPlusSize4K) == 256, "4K block count");
    CHECK(mf_plus_get_block_count(MfPlusSizeUnknown) == 0, "unknown size -> 0 blocks");

    CHECK(mf_plus_get_sector_count(MfPlusSize1K) == 16, "1K sector count");
    CHECK(mf_plus_get_sector_count(MfPlusSize2K) == 32, "2K sector count");
    CHECK(mf_plus_get_sector_count(MfPlusSize4K) == 40, "4K sector count");

    // 4K layout: 32 small (4-block) sectors then 8 large (16-block) sectors, exactly 256 blocks.
    uint16_t total = 0;
    for(uint8_t s = 0; s < 40; s++) {
        CHECK(mf_plus_sector_get_first_block(s) == total, "sector first-block matches running total");
        total = (uint16_t)(total + mf_plus_sector_get_block_count(s));
    }
    CHECK(total == 256, "4K sectors sum to 256 blocks");
    CHECK(mf_plus_sector_get_block_count(0) == 4, "sector 0 has 4 blocks");
    CHECK(mf_plus_sector_get_block_count(31) == 4, "sector 31 has 4 blocks");
    CHECK(mf_plus_sector_get_block_count(32) == 16, "sector 32 has 16 blocks");
    CHECK(mf_plus_sector_get_block_count(39) == 16, "sector 39 has 16 blocks");

    for(MfPlusAdminKeyType t = 0; t < MfPlusAdminKeyNum; t++) {
        uint16_t addr = mf_plus_get_admin_key_address(t);
        MfPlusAdminKeyType back;
        CHECK(mf_plus_admin_key_type_from_address(addr, &back), "admin address round-trips");
        CHECK(back == t, "admin address maps back to the same type");
        CHECK(mf_plus_get_admin_key_name(t) != NULL, "admin key has a name");
    }
    MfPlusAdminKeyType unused;
    CHECK(!mf_plus_admin_key_type_from_address(0x9002, &unused), "0x9002 (SL2->SL3 key) is not an admin key");
    CHECK(!mf_plus_admin_key_type_from_address(0x1234, &unused), "unrelated address is not an admin key");
}

static void test_masks_and_is_card_read(void) {
    MfPlusData* data = mf_plus_alloc();
    data->security_level = MfPlusSecurityLevel3;
    data->size = MfPlusSize1K;

    CHECK(!mf_plus_is_card_read(data), "freshly reset card is not fully read");
    for(uint16_t i = 0; i < 64; i++) CHECK(!mf_plus_is_block_read(data, i), "block starts unread");

    MfPlusBlock b;
    fill_block(&b, 1);
    mf_plus_set_block_read(data, 5, &b);
    CHECK(mf_plus_is_block_read(data, 5), "set block is read");
    CHECK(!mf_plus_is_block_read(data, 6), "neighbouring block stays unread");
    CHECK(memcmp(data->block[5].data, b.data, MF_PLUS_BLOCK_SIZE) == 0, "block payload stored");

    for(uint16_t i = 0; i < 64; i++) {
        MfPlusBlock bb;
        fill_block(&bb, (uint8_t)i);
        mf_plus_set_block_read(data, i, &bb);
    }
    CHECK(mf_plus_is_card_read(data), "card with every block set reads as fully read");

    MfPlusKey k;
    fill_key(&k, 7);
    CHECK(!mf_plus_is_key_found(data, 3, MfPlusKeyTypeA), "key A starts unknown");
    mf_plus_set_key_found(data, 3, MfPlusKeyTypeA, &k);
    CHECK(mf_plus_is_key_found(data, 3, MfPlusKeyTypeA), "key A now known");
    CHECK(!mf_plus_is_key_found(data, 3, MfPlusKeyTypeB), "key B unaffected by key A set");
    CHECK(!mf_plus_is_key_found(data, 4, MfPlusKeyTypeA), "neighbouring sector's key A unaffected");

    uint8_t sectors_read, keys_found;
    mf_plus_get_read_sectors_and_keys(data, &sectors_read, &keys_found);
    CHECK(sectors_read == 16, "all 16 sectors read once every block is set");
    CHECK(keys_found == 1, "exactly one key (A, sector 3) recovered so far");

    CHECK(!mf_plus_is_admin_key_found(data, MfPlusAdminKeyCardMaster), "admin key starts unknown");
    mf_plus_set_admin_key_found(data, MfPlusAdminKeyCardMaster, &k);
    CHECK(mf_plus_is_admin_key_found(data, MfPlusAdminKeyCardMaster), "admin key now known");
    CHECK(!mf_plus_is_admin_key_found(data, MfPlusAdminKeyCardConfig), "other admin key unaffected");

    CHECK(!mf_plus_is_config_block_read(data, 2), "config block starts unread");
    mf_plus_set_config_block_read(data, 2, &b);
    CHECK(mf_plus_is_config_block_read(data, 2), "config block now read");
    CHECK(!mf_plus_is_config_block_read(data, 0), "other config block unaffected");

    mf_plus_free(data);
}

static void test_reset_zeroes_everything(void) {
    MfPlusData* data = mf_plus_alloc();
    populate_partial_sl3_1k(data);

    uint8_t sectors_read, keys_found;
    mf_plus_get_read_sectors_and_keys(data, &sectors_read, &keys_found);
    CHECK(keys_found > 0, "sanity: populated card has recovered keys before reset");

    mf_plus_reset(data);
    CHECK(data->security_level == MfPlusSecurityLevelUnknown, "reset clears security level");
    CHECK(data->key_a_mask == 0 && data->key_b_mask == 0, "reset clears key masks");
    CHECK(data->admin_key_mask == 0, "reset clears admin key mask");
    CHECK(data->config_read_mask == 0, "reset clears config mask");
    CHECK(!data->signature_present, "reset clears signature_present");
    uint8_t zero_block[MF_PLUS_BLOCK_SIZE] = {0};
    CHECK(memcmp(data->block[0].data, zero_block, MF_PLUS_BLOCK_SIZE) == 0, "reset zeroes block data");
    for(size_t i = 0; i < MF_PLUS_BLOCK_READ_MASK_SIZE; i++) {
        CHECK(data->block_read_mask[i] == 0, "reset zeroes block_read_mask word");
    }

    mf_plus_free(data);
}

static void test_copy_and_is_equal(void) {
    MfPlusData* a = mf_plus_alloc();
    MfPlusData* b = mf_plus_alloc();
    populate_partial_sl3_1k(a);

    CHECK(!mf_plus_is_equal(a, b), "populated vs fresh card are not equal");
    mf_plus_copy(b, a);
    CHECK(mf_plus_is_equal(a, b), "copy makes cards equal");

    // Flip one bit deep in an otherwise-equal copy of the SL3 payload; is_equal must catch each.
    b->block[10].data[0] ^= 0xFF;
    CHECK(!mf_plus_is_equal(a, b), "differing block data is detected");
    mf_plus_copy(b, a);

    b->key_a[2].data[0] ^= 0xFF;
    CHECK(!mf_plus_is_equal(a, b), "differing key A data is detected");
    mf_plus_copy(b, a);

    b->key_a_mask ^= 1;
    CHECK(!mf_plus_is_equal(a, b), "differing key mask is detected");
    mf_plus_copy(b, a);

    b->admin_key_mask ^= 1;
    CHECK(!mf_plus_is_equal(a, b), "differing admin key mask is detected");
    mf_plus_copy(b, a);

    b->config_read_mask ^= 1;
    CHECK(!mf_plus_is_equal(a, b), "differing config mask is detected");
    mf_plus_copy(b, a);

    b->signature_present = false;
    CHECK(!mf_plus_is_equal(a, b), "differing signature_present is detected");
    mf_plus_copy(b, a);

    b->signature[0] ^= 0xFF;
    CHECK(!mf_plus_is_equal(a, b), "differing signature bytes are detected");

    mf_plus_free(a);
    mf_plus_free(b);
}

// Full mf_plus_save() -> mf_plus_load() round trip through the FlipperFormat stand-in, for an
// SL3 card with a partial, mixed-mask dump (the realistic case).
static void test_save_load_round_trip_sl3_partial(void) {
    MfPlusData* saved = mf_plus_alloc();
    populate_partial_sl3_1k(saved);
    const uint8_t uid[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    CHECK(mf_plus_set_uid(saved, uid, sizeof(uid)), "set_uid on the base iso14443_4a data");

    FlipperFormat* ff = flipper_format_alloc();
    CHECK(mf_plus_save(saved, ff), "mf_plus_save succeeds");

    MfPlusData* loaded = mf_plus_alloc();
    // mf_plus_load()'s SL3 branch only fires once security_level/size are already known from the
    // identity fields it just loaded -- exactly what mf_plus_load does in order (version/type/
    // security_level/size, THEN sl3_data_load), so no extra setup is needed here.
    CHECK(mf_plus_load(loaded, ff, 1), "mf_plus_load succeeds");

    CHECK(mf_plus_is_equal(saved, loaded), "save -> load round-trips to an identical MfPlusData");
    CHECK(loaded->security_level == MfPlusSecurityLevel3, "loaded security level is SL3");
    CHECK(loaded->size == MfPlusSize1K, "loaded size is 1K");
    CHECK(mf_plus_is_card_read(loaded), "loaded card is fully block-read (as saved)");

    uint8_t sectors_read, keys_found;
    mf_plus_get_read_sectors_and_keys(loaded, &sectors_read, &keys_found);
    CHECK(sectors_read == 16, "loaded: all 16 sectors read");
    CHECK(keys_found == 16, "loaded: exactly the 16 key-A's saved, no key-B's");
    for(uint8_t s = 0; s < 16; s++) {
        CHECK(mf_plus_is_key_found(loaded, s, MfPlusKeyTypeA), "loaded key A present per sector");
        CHECK(!mf_plus_is_key_found(loaded, s, MfPlusKeyTypeB), "loaded key B still unknown per sector");
    }
    CHECK(mf_plus_is_admin_key_found(loaded, MfPlusAdminKeyCardMaster), "loaded admin key (Card Master) present");
    CHECK(mf_plus_is_admin_key_found(loaded, MfPlusAdminKeyL3Switch), "loaded admin key (L3 Switch) present");
    CHECK(!mf_plus_is_admin_key_found(loaded, MfPlusAdminKeyCardConfig), "loaded admin key (Card Config) still unknown");
    CHECK(!mf_plus_is_admin_key_found(loaded, MfPlusAdminKeySL1CardAuth), "loaded admin key (SL1 Card Auth) still unknown");
    for(uint8_t c = 0; c < MF_PLUS_CONFIG_BLOCK_NUM - 1; c++) {
        CHECK(mf_plus_is_config_block_read(loaded, c), "loaded config block present");
    }
    CHECK(!mf_plus_is_config_block_read(loaded, MF_PLUS_CONFIG_BLOCK_NUM - 1), "loaded last config block still unread");
    CHECK(loaded->signature_present, "loaded signature present");
    CHECK(
        memcmp(loaded->signature, saved->signature, MF_PLUS_SIGNATURE_SIZE) == 0,
        "loaded signature bytes match");

    size_t uid_len = 0;
    const uint8_t* loaded_uid = mf_plus_get_uid(loaded, &uid_len);
    CHECK(uid_len == sizeof(uid), "loaded base card UID length round-trips");
    CHECK(memcmp(loaded_uid, uid, uid_len) == 0, "loaded base card UID bytes round-trip");

    mf_plus_free(saved);
    mf_plus_free(loaded);
    flipper_format_free(ff);
}

// SL0/SL1/SL2 cards never populate the SL3 payload; save must still succeed and load must not
// try to read fields that were never written (data->security_level != SL3 gates both).
static void test_save_load_round_trip_non_sl3(void) {
    MfPlusData* saved = mf_plus_alloc();
    saved->security_level = MfPlusSecurityLevel1;
    saved->size = MfPlusSize4K;
    saved->type = MfPlusTypeX;

    FlipperFormat* ff = flipper_format_alloc();
    CHECK(mf_plus_save(saved, ff), "mf_plus_save succeeds for a non-SL3 card");

    MfPlusData* loaded = mf_plus_alloc();
    CHECK(mf_plus_load(loaded, ff, 1), "mf_plus_load succeeds for a non-SL3 card");
    CHECK(mf_plus_is_equal(saved, loaded), "non-SL3 round trip is identical (no SL3 fields to lose)");
    CHECK(!mf_plus_is_card_read(loaded), "non-SL3 card never reads as fully read (block_count 0 path n/a here, but no blocks set)");
    CHECK(!loaded->signature_present, "non-SL3, no-signature card loads with signature_present false");

    mf_plus_free(saved);
    mf_plus_free(loaded);
    flipper_format_free(ff);
}

// A legacy metadata-only dump (pre-Phase-2 dumps already on disk) has no "Data format version"
// key at all -- mf_plus_sl3_data_load must accept that silently and leave the SL3 fields zeroed,
// per its documented contract, instead of failing the whole load.
static void test_legacy_dump_without_data_format_version(void) {
    MfPlusData* saved = mf_plus_alloc();
    saved->security_level = MfPlusSecurityLevel3;
    saved->size = MfPlusSize1K;

    FlipperFormat* ff = flipper_format_alloc();
    // Write only the identity fields a pre-Phase-2 build would have written -- no SL3 payload,
    // no "Data format version" key.
    CHECK(mf_plus_version_save(&saved->version, ff), "legacy: version saves");
    CHECK(mf_plus_type_save(&saved->type, ff), "legacy: type saves");
    CHECK(mf_plus_security_level_save(&saved->security_level, ff), "legacy: security level saves");
    CHECK(mf_plus_size_save(&saved->size, ff), "legacy: size saves");

    MfPlusData* loaded = mf_plus_alloc();
    loaded->security_level = MfPlusSecurityLevel3; // as if identity metadata was already loaded
    loaded->size = MfPlusSize1K;
    CHECK(mf_plus_sl3_data_load(loaded, ff), "sl3_data_load tolerates a missing Data format version key");
    CHECK(loaded->key_a_mask == 0 && loaded->key_b_mask == 0, "legacy load leaves key masks zeroed");
    CHECK(!mf_plus_is_card_read(loaded), "legacy load leaves the card unread");
    CHECK(!loaded->signature_present, "legacy load leaves signature_present false");

    mf_plus_free(saved);
    mf_plus_free(loaded);
    flipper_format_free(ff);
}

// A dump written by a hypothetical future format version must be refused, not silently
// misparsed as v1 (mf_plus_sl3_data_load's explicit dfv > mf_plus_data_format_version guard).
static void test_future_data_format_version_refused(void) {
    FlipperFormat* ff = flipper_format_alloc();
    uint32_t future_version = 2;
    CHECK(
        flipper_format_write_uint32(ff, "Data format version", &future_version, 1),
        "write a v2 Data format version marker");

    MfPlusData* loaded = mf_plus_alloc();
    loaded->security_level = MfPlusSecurityLevel3;
    loaded->size = MfPlusSize1K;
    CHECK(!mf_plus_sl3_data_load(loaded, ff), "sl3_data_load refuses a newer, unknown format version");

    mf_plus_free(loaded);
    flipper_format_free(ff);
}

// mf_plus_merge_update: overlay recovered fields from `fresh` onto `base` without ever
// clearing a field `base` already had.
static void test_merge_update_never_clears(void) {
    MfPlusData* base = mf_plus_alloc();
    base->security_level = MfPlusSecurityLevel3;
    base->size = MfPlusSize1K;
    MfPlusKey k0;
    fill_key(&k0, 0);
    mf_plus_set_key_found(base, 0, MfPlusKeyTypeA, &k0);
    MfPlusBlock b0;
    fill_block(&b0, 0);
    mf_plus_set_block_read(base, 0, &b0);
    base->signature_present = true;
    memset(base->signature, 0x11, MF_PLUS_SIGNATURE_SIZE);

    MfPlusData* fresh = mf_plus_alloc();
    fresh->security_level = MfPlusSecurityLevel3;
    fresh->size = MfPlusSize1K;
    // fresh recovers a DIFFERENT key/block and has no signature (cut-short re-read) --
    // merge must add the new field and keep base's signature rather than clearing it.
    MfPlusKey k1;
    fill_key(&k1, 1);
    mf_plus_set_key_found(fresh, 1, MfPlusKeyTypeA, &k1);
    MfPlusBlock b1;
    fill_block(&b1, 1);
    mf_plus_set_block_read(fresh, 1, &b1);

    mf_plus_merge_update(base, fresh);

    CHECK(mf_plus_is_key_found(base, 0, MfPlusKeyTypeA), "merge keeps base's original key");
    CHECK(mf_plus_is_key_found(base, 1, MfPlusKeyTypeA), "merge adds fresh's new key");
    CHECK(mf_plus_is_block_read(base, 0), "merge keeps base's original block");
    CHECK(mf_plus_is_block_read(base, 1), "merge adds fresh's new block");
    CHECK(base->signature_present, "merge never clears a signature fresh didn't have");
    uint8_t expect_sig[MF_PLUS_SIGNATURE_SIZE];
    memset(expect_sig, 0x11, sizeof(expect_sig));
    CHECK(
        memcmp(base->signature, expect_sig, MF_PLUS_SIGNATURE_SIZE) == 0,
        "merge leaves base's signature bytes untouched");

    // Now merge a fresh WITH a (different) signature: it must overwrite base's.
    MfPlusData* fresh2 = mf_plus_alloc();
    fresh2->signature_present = true;
    memset(fresh2->signature, 0x22, MF_PLUS_SIGNATURE_SIZE);
    mf_plus_merge_update(base, fresh2);
    uint8_t expect_sig2[MF_PLUS_SIGNATURE_SIZE];
    memset(expect_sig2, 0x22, sizeof(expect_sig2));
    CHECK(
        memcmp(base->signature, expect_sig2, MF_PLUS_SIGNATURE_SIZE) == 0,
        "merge overwrites signature when fresh has a newer one");

    mf_plus_free(base);
    mf_plus_free(fresh);
    mf_plus_free(fresh2);
}

// Corrupt/truncated hex fields in the saved format must load as "unknown", not crash or
// silently accept garbage -- exercises mf_plus_str_to_bytes's failure paths indirectly.
static void test_corrupt_field_loads_as_unknown(void) {
    MfPlusData* saved = mf_plus_alloc();
    saved->security_level = MfPlusSecurityLevel3;
    saved->size = MfPlusSize1K;
    MfPlusBlock b;
    fill_block(&b, 5);
    mf_plus_set_block_read(saved, 0, &b);

    FlipperFormat* ff = flipper_format_alloc();
    CHECK(mf_plus_save(saved, ff), "save for corruption test succeeds");

    // Corrupt "Block 0" in place: truncate it to a single byte pair.
    FFEntry* e = ff_find(ff, "Block 0");
    CHECK(e != NULL, "Block 0 entry exists in the saved format");
    free(e->value);
    e->value = ff_strdup("AB");

    MfPlusData* loaded = mf_plus_alloc();
    CHECK(mf_plus_load(loaded, ff, 1), "load still succeeds despite one corrupt field");
    CHECK(!mf_plus_is_block_read(loaded, 0), "corrupt/truncated block loads as unknown, not crashing");

    mf_plus_free(saved);
    mf_plus_free(loaded);
    flipper_format_free(ff);
}

int main(void) {
    test_accessors_and_geometry();
    test_masks_and_is_card_read();
    test_reset_zeroes_everything();
    test_copy_and_is_equal();
    test_save_load_round_trip_sl3_partial();
    test_save_load_round_trip_non_sl3();
    test_legacy_dump_without_data_format_version();
    test_future_data_format_version_refused();
    test_merge_update_never_clears();
    test_corrupt_field_loads_as_unknown();

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", g_failures);
        return 1;
    }
}
