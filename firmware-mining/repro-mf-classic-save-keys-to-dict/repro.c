// Host repro for firmware-mining/mf-classic-save-keys-to-dict-port.md
//
// Verifies, on the host, the three algorithms the new
// applications/main/nfc/scenes/nfc_scene_mf_classic_save_keys.c contributes:
//   - nfc_mf_classic_save_keys_push_unique() / _collect(): pull the found keys off a
//     MfClassicData-shaped struct via the found masks, deduplicated.
//   - nfc_mf_classic_save_keys_mark_known(): one pass per dictionary marking which collected
//     keys it already holds, rather than one dictionary scan per key.
// Both are extracted close to verbatim from the real source (memcmp-only, no Storage/stream
// dependency), with a minimal in-memory stand-in for KeysDict's get_total_keys/get_next_key
// contract (walk once, false when exhausted) standing in for the file-backed one.
//
// Build:  gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
// Run:    ./repro

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define MF_CLASSIC_KEY_SIZE          (6)
#define MF_CLASSIC_TOTAL_SECTORS_MAX (40) // 4K card
#define NFC_MF_CLASSIC_SAVE_KEYS_MAX (80) // verbatim from the real source

typedef struct {
    uint8_t data[MF_CLASSIC_KEY_SIZE];
} MfClassicKey;

typedef struct {
    MfClassicKey key_a;
    MfClassicKey key_b;
} MfClassicSectorTrailer;

// Stand-in for MfClassicData: just enough for the collector under test.
typedef struct {
    uint8_t num_sectors;
    uint64_t key_a_mask;
    uint64_t key_b_mask;
    MfClassicSectorTrailer sectors[MF_CLASSIC_TOTAL_SECTORS_MAX];
} FakeMfClassicData;

static bool furi_bit(uint64_t mask, uint8_t n) {
    return (mask >> n) & 1;
}

// ---- verbatim from nfc_scene_mf_classic_save_keys.c (renamed FURI_BIT -> furi_bit call site
// only; body identical) ----

static size_t nfc_mf_classic_save_keys_push_unique(
    MfClassicKey* keys,
    size_t key_count,
    const MfClassicKey* key) {
    if(key_count == NFC_MF_CLASSIC_SAVE_KEYS_MAX) return key_count;

    for(size_t i = 0; i < key_count; i++) {
        if(memcmp(keys[i].data, key->data, sizeof(MfClassicKey)) == 0) return key_count;
    }

    keys[key_count] = *key;
    return key_count + 1;
}

static size_t nfc_mf_classic_save_keys_collect(const FakeMfClassicData* data, MfClassicKey* keys) {
    uint8_t num_sectors = data->num_sectors;
    size_t key_count = 0;

    for(uint8_t sector = 0; sector < num_sectors; sector++) {
        const MfClassicSectorTrailer* sec_tr = &data->sectors[sector];

        if(furi_bit(data->key_a_mask, sector)) {
            key_count = nfc_mf_classic_save_keys_push_unique(keys, key_count, &sec_tr->key_a);
        }
        if(furi_bit(data->key_b_mask, sector)) {
            key_count = nfc_mf_classic_save_keys_push_unique(keys, key_count, &sec_tr->key_b);
        }
    }

    return key_count;
}

// Minimal in-memory stand-in for a KeysDict: same iteration contract as
// keys_dict_get_total_keys()/keys_dict_get_next_key() (walk once from the start, false when
// exhausted), backed by an array instead of a file stream.
typedef struct {
    const MfClassicKey* keys;
    size_t total;
    size_t cursor;
} FakeKeysDict;

static size_t fake_keys_dict_get_total_keys(FakeKeysDict* dict) {
    return dict->total;
}

static bool fake_keys_dict_get_next_key(FakeKeysDict* dict, uint8_t* out) {
    if(dict->cursor >= dict->total) return false;
    memcpy(out, dict->keys[dict->cursor].data, MF_CLASSIC_KEY_SIZE);
    dict->cursor++;
    return true;
}

// verbatim logic from nfc_mf_classic_save_keys_mark_known(), rebound to the fake dict above.
static void nfc_mf_classic_save_keys_mark_known(
    FakeKeysDict* dict,
    const MfClassicKey* keys,
    size_t key_count,
    bool* known,
    size_t* known_count) {
    if(fake_keys_dict_get_total_keys(dict) == 0) return;

    MfClassicKey dict_key;
    while(fake_keys_dict_get_next_key(dict, dict_key.data)) {
        for(size_t i = 0; i < key_count; i++) {
            if(known[i]) continue;
            if(memcmp(keys[i].data, dict_key.data, sizeof(MfClassicKey)) != 0) continue;

            known[i] = true;
            (*known_count)++;
            break;
        }
    }
}

// ---- test scaffolding ----

static MfClassicKey mk(uint8_t fill) {
    MfClassicKey k;
    memset(k.data, fill, sizeof(k.data));
    return k;
}

static int checks_passed = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if(cond) {                                                      \
            checks_passed++;                                            \
        } else {                                                        \
            checks_failed++;                                            \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                \
    } while(0)

int main(void) {
    // --- Test 1: collect only what the found masks say, deduplicated ---
    // 3 sectors: sector 0 both keys found (A == default 0xFF*6, B == 0xAA*6), sector 1 only key A
    // found and identical to sector 0's key A (must dedup to one entry), sector 2 nothing found
    // but its bytes are non-zero garbage from an unrelated card slot (must NOT be collected -
    // this is the "gate on the mask, not the bytes" requirement).
    {
        FakeMfClassicData data = {0};
        data.num_sectors = 3;
        data.sectors[0].key_a = mk(0xFF);
        data.sectors[0].key_b = mk(0xAA);
        data.sectors[1].key_a = mk(0xFF); // duplicate of sector 0's key A
        data.sectors[1].key_b = mk(0xBB); // not found -> must be ignored
        data.sectors[2].key_a = mk(0x41); // not found -> must be ignored despite nonzero bytes
        data.sectors[2].key_b = mk(0x42); // not found -> must be ignored despite nonzero bytes
        data.key_a_mask = (1u << 0) | (1u << 1);
        data.key_b_mask = (1u << 0);

        MfClassicKey keys[NFC_MF_CLASSIC_SAVE_KEYS_MAX];
        size_t count = nfc_mf_classic_save_keys_collect(&data, keys);

        CHECK(count == 2, "collect: expected 2 distinct keys (dedup sector 1's key A)");
        bool has_ff = false, has_aa = false, has_bb = false, has_41 = false;
        for(size_t i = 0; i < count; i++) {
            if(memcmp(keys[i].data, mk(0xFF).data, 6) == 0) has_ff = true;
            if(memcmp(keys[i].data, mk(0xAA).data, 6) == 0) has_aa = true;
            if(memcmp(keys[i].data, mk(0xBB).data, 6) == 0) has_bb = true;
            if(memcmp(keys[i].data, mk(0x41).data, 6) == 0) has_41 = true;
        }
        CHECK(has_ff && has_aa, "collect: both distinct found keys present");
        CHECK(!has_bb && !has_41, "collect: unfound sector bytes must not leak in");
    }

    // --- Test 2: a genuine all-zero key (mask bit set) is collected like any other key ---
    {
        FakeMfClassicData data = {0};
        data.num_sectors = 2;
        data.sectors[0].key_a = mk(0x00); // real key, all-zero, mask bit set
        data.sectors[1].key_a = mk(0x00); // unrecovered slot, also all-zero, mask bit clear
        data.key_a_mask = (1u << 0);

        MfClassicKey keys[NFC_MF_CLASSIC_SAVE_KEYS_MAX];
        size_t count = nfc_mf_classic_save_keys_collect(&data, keys);

        CHECK(count == 1, "collect: exactly one all-zero key collected (mask-gated, not byte-gated)");
    }

    // --- Test 3: full 4K sweep never overruns the fixed-size buffer ---
    // 40 sectors, both keys found, all distinct -> exactly 80 keys, matching
    // NFC_MF_CLASSIC_SAVE_KEYS_MAX exactly (the bound this constant documents).
    {
        FakeMfClassicData data = {0};
        data.num_sectors = MF_CLASSIC_TOTAL_SECTORS_MAX;
        data.key_a_mask = ~0ull;
        data.key_b_mask = ~0ull;
        for(uint8_t s = 0; s < MF_CLASSIC_TOTAL_SECTORS_MAX; s++) {
            data.sectors[s].key_a = mk((uint8_t)(2 * s));
            data.sectors[s].key_b = mk((uint8_t)(2 * s + 1));
        }

        MfClassicKey keys[NFC_MF_CLASSIC_SAVE_KEYS_MAX];
        size_t count = nfc_mf_classic_save_keys_collect(&data, keys);
        CHECK(count == 80, "collect: full 4K card yields exactly 80 distinct keys");
    }

    // --- Test 4: the push_unique overflow guard itself (defensive; unreachable via a real card,
    // since 40 sectors x 2 keys == NFC_MF_CLASSIC_SAVE_KEYS_MAX exactly) ---
    {
        MfClassicKey keys[NFC_MF_CLASSIC_SAVE_KEYS_MAX];
        size_t count = 0;
        for(int i = 0; i < NFC_MF_CLASSIC_SAVE_KEYS_MAX; i++) {
            MfClassicKey k = mk((uint8_t)i);
            count = nfc_mf_classic_save_keys_push_unique(keys, count, &k);
        }
        CHECK(count == NFC_MF_CLASSIC_SAVE_KEYS_MAX, "push_unique: fills buffer to capacity");

        MfClassicKey extra = mk(0xEE);
        count = nfc_mf_classic_save_keys_push_unique(keys, count, &extra);
        CHECK(
            count == NFC_MF_CLASSIC_SAVE_KEYS_MAX,
            "push_unique: refuses to grow count past capacity");
    }

    // --- Test 5: mark_known filters against system dict, then user dict, one pass each ---
    {
        MfClassicKey candidates[3] = {mk(0x11), mk(0x22), mk(0x33)};
        bool known[3] = {false, false, false};
        size_t known_count = 0;

        MfClassicKey system_keys[2] = {mk(0x11), mk(0xFF)}; // 0x11 is a factory key
        FakeKeysDict system_dict = {system_keys, 2, 0};
        nfc_mf_classic_save_keys_mark_known(&system_dict, candidates, 3, known, &known_count);

        MfClassicKey user_keys[1] = {mk(0x22)}; // 0x22 was saved in an earlier session
        FakeKeysDict user_dict = {user_keys, 1, 0};
        nfc_mf_classic_save_keys_mark_known(&user_dict, candidates, 3, known, &known_count);

        CHECK(known[0] && known[1] && !known[2], "mark_known: 0x11 (system) and 0x22 (user) known, 0x33 new");
        CHECK(known_count == 2, "mark_known: known_count sums across both dictionaries");
    }

    // --- Test 6: missing/empty dictionary (total==0) is a no-op, not a crash or false match ---
    {
        MfClassicKey candidates[1] = {mk(0x77)};
        bool known[1] = {false};
        size_t known_count = 0;

        FakeKeysDict empty_dict = {NULL, 0, 0};
        nfc_mf_classic_save_keys_mark_known(&empty_dict, candidates, 1, known, &known_count);

        CHECK(!known[0] && known_count == 0, "mark_known: empty dictionary marks nothing known");
    }

    // --- Test 7: full flow numbers line up (candidates == added + known) ---
    {
        FakeMfClassicData data = {0};
        data.num_sectors = 4;
        data.sectors[0].key_a = mk(0x11); // known (system)
        data.sectors[1].key_a = mk(0x22); // known (user)
        data.sectors[2].key_a = mk(0x33); // new
        data.sectors[3].key_a = mk(0x44); // new
        data.key_a_mask = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);

        MfClassicKey keys[NFC_MF_CLASSIC_SAVE_KEYS_MAX];
        size_t key_count = nfc_mf_classic_save_keys_collect(&data, keys);
        CHECK(key_count == 4, "full flow: 4 candidates collected");

        bool known[NFC_MF_CLASSIC_SAVE_KEYS_MAX] = {0};
        size_t known_count = 0;

        MfClassicKey system_keys[1] = {mk(0x11)};
        FakeKeysDict system_dict = {system_keys, 1, 0};
        nfc_mf_classic_save_keys_mark_known(&system_dict, keys, key_count, known, &known_count);

        MfClassicKey user_keys[1] = {mk(0x22)};
        FakeKeysDict user_dict = {user_keys, 1, 0};
        nfc_mf_classic_save_keys_mark_known(&user_dict, keys, key_count, known, &known_count);

        size_t added_count = 0;
        for(size_t i = 0; i < key_count; i++) {
            if(!known[i]) added_count++;
        }

        CHECK(known_count == 2, "full flow: 2 of 4 already known (one per dictionary)");
        CHECK(added_count == 2, "full flow: 2 of 4 would be newly appended");
        CHECK(added_count + known_count == key_count, "full flow: candidates == added + known");
    }

    fprintf(stderr, "%d checks passed, %d failed\n", checks_passed, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
