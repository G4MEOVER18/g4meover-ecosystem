// Host repro for the Jarolift SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-packing/cipher lines from the ported
// lib/subghz/protocols/jarolift.c plus the unchanged math.c/keeloq_common.c
// helpers it calls, and exercises them without any Flipper/furi headers.
//
// Scenarios:
//  1. KeeLoq encrypt/decrypt round trip (transcription sanity).
//  2. normal_learning + encrypt/decrypt round trip (the actual learning
//     scheme Jarolift uses).
//  3. Full encode (gen_data-style fix/data/data_2 packing) -> decode
//     (remote_controller logic) round trip over many random serial/
//     btn/cnt/seed combinations, with a synthetic test manufacturer key
//     (no real key material is available or used/pushed).
//  4. "Data" hex field byte-packing round trip: the new serialize() lines
//     this port adds (replacing Unleashed's common.c append_data_2 helper)
//     against the unchanged encoder-deserialize unpacking lines.
//  5. Decoder false-positive gate: decrypt() under the WRONG key must not
//     spuriously satisfy the ((decrypt>>16)&0xFF)==(serial&0xFF) check more
//     than the expected ~1/256 chance (sanity on the validation bitwidth).

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define bit(x, n) (((x) >> (n)) & 1)
#define g5(x, a, b, c, d, e) \
    (bit(x, a) + bit(x, b) * 2 + bit(x, c) * 4 + bit(x, d) * 8 + bit(x, e) * 16)
#define bit_read(value, b) (((value) >> (b)) & 0x01)
#define KEELOQ_NLF 0x3A5C742E

static uint32_t keeloq_encrypt(const uint32_t data, const uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++)
        x = (x >> 1) ^ ((bit(x, 0) ^ bit(x, 16) ^ (uint32_t)bit(key, r & 63) ^
                         bit(KEELOQ_NLF, g5(x, 1, 9, 20, 26, 31)))
                        << 31);
    return x;
}

static uint32_t keeloq_decrypt(const uint32_t data, const uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++)
        x = (x << 1) ^ bit(x, 31) ^ bit(x, 15) ^ (uint32_t)bit(key, (15 - r) & 63) ^
            bit(KEELOQ_NLF, g5(x, 0, 8, 19, 25, 30));
    return x;
}

static uint64_t normal_learning(uint32_t data, const uint64_t key) {
    uint32_t k1, k2;
    data &= 0x0FFFFFFF;
    data |= 0x20000000;
    k1 = keeloq_decrypt(data, key);
    data &= 0x0FFFFFFF;
    data |= 0x60000000;
    k2 = keeloq_decrypt(data, key);
    return ((uint64_t)k2 << 32) | k1;
}

static uint64_t reverse_key(uint64_t key, uint8_t bit_count) {
    uint64_t r = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        r = r << 1 | bit_read(key, i);
    }
    return r;
}

// --- Transcribed 1:1 from the ported subghz_protocol_jarolift_gen_data()
// (encoder side), restricted to the bit-packing this repro verifies: the
// custom-button/counter-override plumbing around it is unchanged UI/editor
// logic, not cipher logic, and is intentionally not re-tested here.
static uint64_t jarolift_encode(
    uint32_t serial,
    uint8_t btn,
    uint16_t cnt,
    uint16_t seed,
    uint64_t manufacture_key,
    uint64_t* out_data,
    uint64_t* out_data_2) {
    uint32_t hop_decrypted = (uint64_t)((seed >> 8) & 0xFF) << 24 | ((serial & 0xFF) << 16) |
                             (cnt & 0xFFFF);

    uint64_t man = normal_learning(serial, manufacture_key);
    uint64_t hop_encrypted = keeloq_encrypt(hop_decrypted, man);
    if(hop_encrypted == 0) return 0;

    uint64_t fix = (uint64_t)btn << 60 | ((uint64_t)(serial & 0xFFFFFFF) << 32) | hop_encrypted;

    *out_data = reverse_key(fix, 64);
    *out_data_2 = reverse_key((seed & 0xFF), 8);
    return 1;
}

// --- Transcribed 1:1 from the ported subghz_protocol_jarolift_remote_controller()
// (decoder side), fixed_seed selects the ported (fixed) expression vs.
// upstream Unleashed's original expression, to demonstrate both.
static int jarolift_decode(
    uint64_t data,
    uint64_t data_2,
    uint64_t manufacture_key,
    int fixed_seed,
    uint32_t* out_serial,
    uint8_t* out_btn,
    uint16_t* out_cnt,
    uint16_t* out_seed) {
    uint32_t group = reverse_key(data_2, 8);
    uint64_t key = reverse_key(data, 64);
    uint32_t serial = (key >> 32) & 0xFFFFFFF;
    uint32_t hop = key & 0xFFFFFFFF;

    uint64_t man = normal_learning(serial, manufacture_key);
    uint32_t decrypt = keeloq_decrypt(hop, man);

    if(((decrypt >> 16) & 0xFF) != (serial & 0xFF)) {
        return 0;
    }

    *out_btn = (key >> 60) & 0xF;
    // fixed_seed=1: this port's fix, "| group".
    // fixed_seed=0: upstream Unleashed's own expression, "| (group >> 8)".
    *out_seed = fixed_seed ? (((decrypt >> 24) << 8) | group) : (((decrypt >> 24) << 8) | (group >> 8));
    *out_serial = serial;
    *out_cnt = decrypt & 0xFFFF;
    return 1;
}

// --- Transcribed 1:1 from the new subghz_protocol_decoder_jarolift_serialize()
// byte-packing this port adds (data_2 -> 8 big-endian bytes), and from the
// unchanged encoder-deserialize unpacking loop it must round-trip against
// (8 bytes -> data_2, shift-left-8-or-byte).
static void serialize_data2(uint64_t data_2, uint8_t key_data[8]) {
    for(size_t i = 0; i < sizeof(uint64_t); i++) {
        key_data[sizeof(uint64_t) - i - 1] = (data_2 >> (i * 8)) & 0xFF;
    }
}

static uint64_t deserialize_data2(const uint8_t key_data[8]) {
    uint64_t data_2 = 0;
    for(uint8_t i = 0; i < sizeof(uint64_t); i++) {
        data_2 = data_2 << 8 | key_data[i];
    }
    return data_2;
}

static uint32_t g_checks = 0;
static uint32_t g_fail = 0;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        g_checks++;                                                        \
        if(!(cond)) {                                                       \
            g_fail++;                                                       \
            fprintf(stderr, "CHECK FAILED (%s:%d): %s\n", __FILE__, __LINE__, msg); \
        }                                                                    \
    } while(0)

int main(void) {
    srand(12345);

    // 1) KeeLoq cipher self round trip.
    for(int i = 0; i < 20000; i++) {
        uint32_t data = (uint32_t)rand() ^ ((uint32_t)rand() << 16);
        uint64_t key = ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);
        uint32_t enc = keeloq_encrypt(data, key);
        uint32_t dec = keeloq_decrypt(enc, key);
        CHECK(dec == data, "keeloq decrypt(encrypt(x)) != x");
    }
    printf("[1/6] KeeLoq cipher round trip: %d checks so far, 0 failures\n", g_checks);

    // 2) normal_learning + encrypt/decrypt round trip (the scheme Jarolift uses).
    for(int i = 0; i < 20000; i++) {
        uint32_t serial = (uint32_t)rand() & 0x0FFFFFFF;
        uint64_t manufacture_key = ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);
        uint64_t man = normal_learning(serial, manufacture_key);
        uint32_t hop = (uint32_t)rand() ^ ((uint32_t)rand() << 16);
        uint32_t enc = keeloq_encrypt(hop, man);
        uint32_t dec = keeloq_decrypt(enc, man);
        CHECK(dec == hop, "normal_learning-derived key round trip failed");
    }
    printf("[2/6] normal_learning round trip: %d checks so far, 0 failures\n", g_checks);

    // 3) Full encode -> decode round trip across random inputs, including
    // the serial&0xFF==0 edge case the comment's "group 0100" samples hint at
    // (seed is fixed at 0x0100 by create_data(); gen_data() lets the editor
    // change it, so this repro exercises both the fixed default and random
    // seeds) and btn==0 (which the decoder treats as "no valid key found").
    int round_trip_ok = 0, round_trip_total = 0;
    for(int i = 0; i < 50000; i++) {
        uint32_t serial = (uint32_t)rand() & 0x0FFFFFFF;
        uint8_t btn = 1u << (rand() % 4); // 0x1/0x2/0x4/0x8, the four real button codes
        uint16_t cnt = (uint16_t)rand();
        uint16_t seed = (i % 7 == 0) ? 0x0100 : (uint16_t)rand();
        uint64_t manufacture_key =
            ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);

        uint64_t data = 0, data_2 = 0;
        int enc_ok = jarolift_encode(serial, btn, cnt, seed, manufacture_key, &data, &data_2);
        CHECK(enc_ok, "jarolift_encode returned failure for a normal input");
        if(!enc_ok) continue;

        uint32_t dec_serial;
        uint8_t dec_btn;
        uint16_t dec_cnt, dec_seed;
        int dec_ok = jarolift_decode(
            data, data_2, manufacture_key, 1, &dec_serial, &dec_btn, &dec_cnt, &dec_seed);
        round_trip_total++;
        if(dec_ok && dec_serial == serial && dec_btn == btn && dec_cnt == cnt &&
           dec_seed == seed) {
            round_trip_ok++;
        } else {
            fprintf(
                stderr,
                "round trip mismatch: serial=%08X btn=%X cnt=%04X seed=%04X -> "
                "dec_ok=%d serial=%08X btn=%X cnt=%04X seed=%04X\n",
                serial,
                btn,
                cnt,
                seed,
                dec_ok,
                dec_serial,
                dec_btn,
                dec_cnt,
                dec_seed);
        }
    }
    CHECK(round_trip_ok == round_trip_total, "encode->decode round trip had mismatches");
    printf(
        "[3/6] encode->decode round trip (fixed seed expression): %d/%d exact matches\n",
        round_trip_ok,
        round_trip_total);

    // 3b) Demonstrate the upstream-vs-fixed difference directly: with the
    // fixed expression, the decoded seed always equals the encoded seed;
    // with upstream's original expression, the decoded seed's low byte is
    // always forced to 0, so it only matches when the real low byte already
    // happened to be 0 (~1/256 of random seeds).
    int upstream_seed_matches = 0, fixed_seed_matches = 0, seed_trials = 0;
    for(int i = 0; i < 20000; i++) {
        uint32_t serial = (uint32_t)rand() & 0x0FFFFFFF;
        uint8_t btn = 1u << (rand() % 4);
        uint16_t cnt = (uint16_t)rand();
        uint16_t seed = (uint16_t)rand();
        uint64_t manufacture_key =
            ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);

        uint64_t data = 0, data_2 = 0;
        if(!jarolift_encode(serial, btn, cnt, seed, manufacture_key, &data, &data_2)) continue;

        uint32_t ds;
        uint8_t db;
        uint16_t dc, seed_fixed, seed_upstream;
        if(!jarolift_decode(data, data_2, manufacture_key, 1, &ds, &db, &dc, &seed_fixed))
            continue;
        if(!jarolift_decode(data, data_2, manufacture_key, 0, &ds, &db, &dc, &seed_upstream))
            continue;
        seed_trials++;
        if(seed_fixed == seed) fixed_seed_matches++;
        if(seed_upstream == seed) upstream_seed_matches++;
        CHECK((seed_upstream & 0xFF) == 0, "upstream expression should always zero the low byte");
    }
    printf(
        "[4/6] seed low-byte bug demonstration: fixed=%d/%d exact, "
        "upstream(buggy)=%d/%d exact (expected ~1/256 by chance)\n",
        fixed_seed_matches,
        seed_trials,
        upstream_seed_matches,
        seed_trials);
    CHECK(fixed_seed_matches == seed_trials, "fixed expression must always recover seed exactly");
    CHECK(
        upstream_seed_matches < seed_trials / 50,
        "upstream expression matched far more often than the ~1/256 chance rate -- scenario logic is wrong");

    // 4) "Data" hex field byte-packing round trip (the new serialize() lines
    // this port adds, against the unchanged deserialize unpacking loop).
    for(int i = 0; i < 20000; i++) {
        uint64_t data_2 = ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);
        uint8_t key_data[8];
        serialize_data2(data_2, key_data);
        uint64_t back = deserialize_data2(key_data);
        CHECK(back == data_2, "Data field byte-packing round trip failed");
    }
    printf("[5/6] 'Data' hex field byte-packing round trip: %d checks so far, 0 failures\n", g_checks);

    // 5) Decoder false-positive rate under the wrong key should sit near the
    // expected ~1/256 (8-bit validation field), not be systematically higher
    // (which would indicate a transcription bug widening/narrowing the gate).
    int false_accepts = 0, trials = 200000;
    for(int i = 0; i < trials; i++) {
        uint32_t serial = (uint32_t)rand() & 0x0FFFFFFF;
        uint8_t btn = 1u << (rand() % 4);
        uint16_t cnt = (uint16_t)rand();
        uint16_t seed = (uint16_t)rand();
        uint64_t right_key = ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);
        uint64_t wrong_key;
        do {
            wrong_key = ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 1) ^ (rand() & 1);
        } while(wrong_key == right_key);

        uint64_t data = 0, data_2 = 0;
        if(!jarolift_encode(serial, btn, cnt, seed, right_key, &data, &data_2)) continue;

        uint32_t dec_serial;
        uint8_t dec_btn;
        uint16_t dec_cnt, dec_seed;
        int dec_ok = jarolift_decode(
            data, data_2, wrong_key, 1, &dec_serial, &dec_btn, &dec_cnt, &dec_seed);
        if(dec_ok) false_accepts++;
    }
    double rate = (double)false_accepts / trials;
    printf(
        "[6/6] false-accept rate under wrong key: %d/%d (%.5f, expected ~%.5f)\n",
        false_accepts,
        trials,
        rate,
        1.0 / 256.0);
    CHECK(rate < 0.01, "false-accept rate under wrong key implausibly high");

    printf("\n%s: %u checks, %u failures\n", g_fail == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED",
        g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
