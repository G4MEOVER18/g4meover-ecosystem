// Host repro for the FAAC SLH decode-side manufacturer-key-selection bug found in
// lib/subghz/protocols/faac_slh.c::subghz_protocol_faac_slh_check_remote_controller()
// (mntm-012 / G4MEOVER-FW base, decode-only path -- the encode path is unaffected,
// see below).
//
// Reduced to the exact selection shape: a keystore is an ordered array of
// manufacture-code entries (name, type, key). Every entry whose type equals
// KEELOQ_LEARNING_FAAC is a candidate key for the Faac-SLH frame family
// (FAAC_SLH itself, plus any future manufacturer added under the same learning
// type, e.g. Unleashed's "Genius" -- see firmware-mining/faac-slh-...-fix.md).
//
// ALT is the real decode loop, ported verbatim in control flow:
//   for each keystore entry:
//     if entry.type == KEELOQ_LEARNING_FAAC:
//       decrypt = f(seed, entry.key); manufacture_name = entry.name;
//       break;              // <- breaks only the inner switch, not the for loop
// (no outer break -> the loop runs to completion, so the LAST matching entry
// in keystore order silently wins, overwriting every earlier match)
//
// NEU adds the missing outer break, mirroring the already-correct encode-side
// loop in the same file (subghz_protocol_faac_slh_encrypt(), which does stop at
// its first -- and, by construction, only possible -- name match).
//
// Property under test: appending a second KEELOQ_LEARNING_FAAC entry to an
// already-populated keystore must not change which key is used to decode a
// signal that was already being decoded correctly against the first entry.
// ALT violates this (regression on every existing Faac SLH signal the moment a
// second same-type manufacturer is added to the keystore); NEU preserves it.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define KEELOQ_LEARNING_SIMPLE 0
#define KEELOQ_LEARNING_NORMAL 1
#define KEELOQ_LEARNING_SECURE 2
#define KEELOQ_LEARNING_MAGIC_XOR_TYPE_1 3
#define KEELOQ_LEARNING_FAAC 4
#define KEELOQ_LEARNING_UNKNOWN 5

typedef struct {
    const char* name;
    int type;
    uint64_t key;
} ManufactureCode;

// Stand-in for subghz_protocol_keeloq_common_faac_learning()+_decrypt(): the
// bug is entirely about *which* key reaches this point, not about the cipher
// itself (already verified independently in earlier sessions), so a simple
// injective mix is enough to tell "decoded with key X" apart from "decoded
// with key Y".
static uint32_t fake_decrypt(uint32_t code_hop, uint64_t key) {
    return code_hop ^ (uint32_t)(key & 0xFFFFFFFFu) ^ (uint32_t)(key >> 32);
}

// ALT: subghz_protocol_faac_slh_check_remote_controller()'s real selection
// loop, decode side (lib/subghz/protocols/faac_slh.c ~line 607-618).
static void select_key_ALT(
    const ManufactureCode* keystore,
    size_t keystore_len,
    uint32_t code_hop,
    uint32_t seed,
    uint32_t* decrypt_out,
    const char** manufacture_name_out) {
    (void)seed;
    uint32_t decrypt = 0;
    const char* manufacture_name = NULL;
    for(size_t i = 0; i < keystore_len; i++) {
        const ManufactureCode* manufacture_code = &keystore[i];
        switch(manufacture_code->type) {
        case KEELOQ_LEARNING_FAAC:
            decrypt = fake_decrypt(code_hop, manufacture_code->key);
            manufacture_name = manufacture_code->name;
            break; // only breaks the switch -- the real bug
        }
    }
    *decrypt_out = decrypt;
    *manufacture_name_out = manufacture_name;
}

// NEU: same loop with the missing outer break restored, so the first
// KEELOQ_LEARNING_FAAC entry wins instead of the last.
static void select_key_NEU(
    const ManufactureCode* keystore,
    size_t keystore_len,
    uint32_t code_hop,
    uint32_t seed,
    uint32_t* decrypt_out,
    const char** manufacture_name_out) {
    (void)seed;
    uint32_t decrypt = 0;
    const char* manufacture_name = NULL;
    for(size_t i = 0; i < keystore_len; i++) {
        const ManufactureCode* manufacture_code = &keystore[i];
        bool matched = false;
        switch(manufacture_code->type) {
        case KEELOQ_LEARNING_FAAC:
            decrypt = fake_decrypt(code_hop, manufacture_code->key);
            manufacture_name = manufacture_code->name;
            matched = true;
            break;
        }
        if(matched) break;
    }
    *decrypt_out = decrypt;
    *manufacture_name_out = manufacture_name;
}

typedef struct {
    uint32_t decrypt;
    const char* manufacture_name;
} Result;

static Result run(
    void (*select)(const ManufactureCode*, size_t, uint32_t, uint32_t, uint32_t*, const char**),
    const ManufactureCode* keystore,
    size_t len,
    uint32_t code_hop,
    uint32_t seed) {
    Result r = {0, NULL};
    select(keystore, len, code_hop, seed, &r.decrypt, &r.manufacture_name);
    return r;
}

int main(void) {
    const uint32_t code_hop = 0xDEADBEEFu;
    const uint32_t seed = 0x12345678u;

    // Today's presumed keystore shape: exactly one FAAC-learning entry, some
    // unrelated other-type entries around it (order matters for the bug, so
    // exercise several positions).
    ManufactureCode one_faac_a[] = {
        {"NiceFlorS", KEELOQ_LEARNING_NORMAL, 0x1111},
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0xAAAA},
        {"CAME", KEELOQ_LEARNING_SECURE, 0x2222},
    };
    Result alt_a = run(select_key_ALT, one_faac_a, 3, code_hop, seed);
    Result neu_a = run(select_key_NEU, one_faac_a, 3, code_hop, seed);
    assert(strcmp(alt_a.manufacture_name, "FAAC_SLH") == 0);
    assert(strcmp(neu_a.manufacture_name, "FAAC_SLH") == 0);
    assert(alt_a.decrypt == neu_a.decrypt);
    printf("[ok] single FAAC-learning entry: ALT and NEU agree (%s, 0x%08x)\n",
           alt_a.manufacture_name,
           alt_a.decrypt);

    // Real-world upgrade scenario: the keystore gains a second FAAC-learning
    // manufacturer (e.g. "Genius", per Unleashed commit 178a99e5), appended
    // after the existing FAAC_SLH entry -- the natural way a keystore file
    // grows (new lines added at the end).
    ManufactureCode two_faac[] = {
        {"NiceFlorS", KEELOQ_LEARNING_NORMAL, 0x1111},
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0xAAAA},
        {"CAME", KEELOQ_LEARNING_SECURE, 0x2222},
        {"Genius", KEELOQ_LEARNING_FAAC, 0xBBBB},
    };
    Result alt_b = run(select_key_ALT, two_faac, 4, code_hop, seed);
    Result neu_b = run(select_key_NEU, two_faac, 4, code_hop, seed);

    // NEU: unaffected by the append -- still the first FAAC-learning entry,
    // same key, same label, same decrypted counter as before the keystore
    // update. Existing captured/saved FAAC_SLH signals keep decoding exactly
    // as they did before "Genius" was added.
    assert(strcmp(neu_b.manufacture_name, "FAAC_SLH") == 0);
    assert(neu_b.decrypt == neu_a.decrypt);
    printf("[ok] NEU: appending a second entry leaves existing FAAC_SLH decode untouched "
           "(%s, 0x%08x)\n",
           neu_b.manufacture_name,
           neu_b.decrypt);

    // ALT: silently flips to the newly appended entry for every signal,
    // including ones that were (and still are) genuine FAAC_SLH captures --
    // wrong key, wrong decrypted counter, wrong manufacturer label in the
    // saved .sub file and on-screen, with no error of any kind.
    assert(strcmp(alt_b.manufacture_name, "Genius") == 0);
    assert(alt_b.decrypt != alt_a.decrypt);
    printf("[ok] ALT: same append silently switches every decode to the new entry "
           "(%s, 0x%08x) -- regression reproduced\n",
           alt_b.manufacture_name,
           alt_b.decrypt);

    // Order independence check: NEU must pick strictly by first-occurrence,
    // not by name, type count, or any other incidental property -- put three
    // FAAC-learning entries in a row and confirm index 0 always wins.
    ManufactureCode three_faac[] = {
        {"Z", KEELOQ_LEARNING_FAAC, 0x0A0A},
        {"A", KEELOQ_LEARNING_FAAC, 0x0B0B},
        {"M", KEELOQ_LEARNING_FAAC, 0x0C0C},
    };
    Result neu_c = run(select_key_NEU, three_faac, 3, code_hop, seed);
    assert(strcmp(neu_c.manufacture_name, "Z") == 0);
    printf("[ok] NEU: first-occurrence rule holds with 3 same-type entries in a row (%s)\n",
           neu_c.manufacture_name);

    // Zero-entry edge case: neither loop must touch decrypt/manufacture_name
    // when the keystore has no FAAC-learning entry at all (e.g. a keystore
    // file stripped down for testing) -- both must leave the out-params at
    // their initialised defaults, no uninitialised read either way.
    ManufactureCode none_faac[] = {
        {"NiceFlorS", KEELOQ_LEARNING_NORMAL, 0x1111},
        {"CAME", KEELOQ_LEARNING_SECURE, 0x2222},
    };
    Result alt_d = run(select_key_ALT, none_faac, 2, code_hop, seed);
    Result neu_d = run(select_key_NEU, none_faac, 2, code_hop, seed);
    assert(alt_d.manufacture_name == NULL && alt_d.decrypt == 0);
    assert(neu_d.manufacture_name == NULL && neu_d.decrypt == 0);
    printf("[ok] zero-entry edge case: ALT and NEU agree (no match, no crash)\n");

    printf("ALL CHECKS PASSED\n");
    return 0;
}
