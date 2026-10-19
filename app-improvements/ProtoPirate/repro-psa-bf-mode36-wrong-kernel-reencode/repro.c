// Host repro: ProtoPirate PSA "Type:0x36" manual-generation re-encode uses the WRONG
// bruteforce-kernel key schedule for any code whose key was actually recovered via the
// second bruteforce kernel (BF2).
//
// Every function below that is not explicitly marked NEU/ALT-only is copied verbatim
// (same lines, only renamed where two different source files define a symbol with the
// same name) from the reviewed commit of G4MEOVER18/ProtoPirate (e3d9da5):
//   - protocols/psa_crypto.c       (buffer build/teardown helpers, TEA forward encrypt,
//                                   both reverse-engineered key schedules)
//   - protocols/psa_crypto_bf.c    (BF1 schedule build/encrypt, BF2 raw-key decrypt,
//                                   ALT: today's single shared fill-from-buffer helper)
//   - protocols/psa.c              (ALT: psa_build_buffer_mode36(), the live manual/replay
//                                   re-encode path, reduced to its pure-logic core — the
//                                   SubGhz upload/level_duration framing is stripped since
//                                   it is irrelevant to which bytes end up in buffer[0..9])
//
// NEU-tagged functions are the proposed fix: psa_bf_fill_state_from_buffer_bf2() is the
// already-documented, separately proposed correction for the BF2 counter/CRC field
// conflation (app-improvements/ProtoPirate/psa-bf2-counter-crc-buffer8-conflation-fix.md),
// extended here to also mark the result with the new Type 0x37 instead of 0x36.
// psa_build_buffer_mode37() is the new re-encode path this fix adds.
//
// Test method: for a batch of synthetic (serial, counter, button) tuples, a "real remote"
// is simulated by running the real forward TEA encryption with each kernel's real key
// schedule (BF1: schedule-derived working key via psa_crypto_tea_encrypt; BF2: raw
// schedule XOR counter) to produce a ciphertext (Key1/Key2 payload bytes). That ciphertext
// is then fed through the UNMODIFIED, verbatim BF1/BF2 match conditions from
// psa_brute_force_run() (copied, not re-derived) to confirm it is accepted as a genuine
// find by the existing search logic -- this is the cross-check that the synthetic BF2
// wire format used here matches what the real decrypt-side code actually verifies, not
// just an internally-consistent invention. The found fields are then run back through the
// re-encode path (ALT vs NEU) and the regenerated ciphertext is compared byte-for-byte
// against the original.
//
// Build: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O1 repro.c -o repro && ./repro
// (also checked at -O0, identical result)

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEA_DELTA  0x9E3779B9U
#define TEA_ROUNDS 32

/* ---- protocols/psa_crypto.c (verbatim) ---- */

static const uint32_t psa_crypto_bf1_key_schedule[4] = {
    0x4A434915U,
    0xD6743C2BU,
    0x1F29D308U,
    0xE6B79A64U,
};

static const uint32_t psa_crypto_bf2_key_schedule[4] = {
    0x4039C240U,
    0xEDA92CABU,
    0x4306C02AU,
    0x02192A04U,
};

static void psa_crypto_tea_encrypt(uint32_t* v0, uint32_t* v1, const uint32_t* key) {
    uint32_t sum = 0;
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t k_idx1 = sum & 3;
        uint32_t temp = key[k_idx1] + sum;
        sum = sum + TEA_DELTA;
        *v0 = *v0 + (temp ^ (((*v1 >> 5) ^ (*v1 << 4)) + *v1));
        uint32_t k_idx2 = (sum >> 11) & 3;
        temp = key[k_idx2] + sum;
        *v1 = *v1 + (temp ^ (((*v0 >> 5) ^ (*v0 << 4)) + *v0));
    }
}

/* psa_crypto_setup_byte_buffer()/psa_crypto_prepare_tea_data() are intentionally not
   reproduced here: this bug and fix live entirely at the buffer[2..9] <-> (v0,v1) level,
   which both of those helpers already pass through unchanged (byte order verified
   separately, see psa-bf2-counter-crc-buffer8-conflation-fix.md "Zusaetzlich geprueft"). */

static uint8_t psa_crypto_tea_crc(uint32_t v0, uint32_t v1) {
    uint32_t crc = ((v0 >> 24) & 0xFF) + ((v0 >> 16) & 0xFF) + ((v0 >> 8) & 0xFF) + (v0 & 0xFF);
    crc += ((v1 >> 24) & 0xFF) + ((v1 >> 16) & 0xFF) + ((v1 >> 8) & 0xFF);
    return (uint8_t)(crc & 0xFF);
}

static uint16_t psa_crypto_crc16_bf2(uint8_t* buffer, int length) {
    uint16_t crc = 0;
    for(int i = 0; i < length; i++) {
        crc = crc ^ ((uint16_t)buffer[i] << 8);
        for(int j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
            crc = crc & 0xFFFF;
        }
    }
    return crc & 0xFFFF;
}

static void psa_crypto_unpack_tea_result_to_buffer(uint8_t* buffer, uint32_t v0, uint32_t v1) {
    buffer[2] = (uint8_t)((v0 >> 24) & 0xFF);
    buffer[3] = (uint8_t)((v0 >> 16) & 0xFF);
    buffer[4] = (uint8_t)((v0 >> 8) & 0xFF);
    buffer[5] = (uint8_t)(v0 & 0xFF);
    buffer[6] = (uint8_t)((v1 >> 24) & 0xFF);
    buffer[7] = (uint8_t)((v1 >> 16) & 0xFF);
    buffer[8] = (uint8_t)((v1 >> 8) & 0xFF);
    buffer[9] = (uint8_t)(v1 & 0xFF);
}

/* ---- protocols/psa_crypto_bf.c (verbatim) ---- */

typedef struct {
    uint32_t s0[TEA_ROUNDS];
    uint32_t s1[TEA_ROUNDS];
} PsaTeaSchedule;

static void psa_bf_tea_build_schedule(const uint32_t* key, PsaTeaSchedule* out) {
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t sum0 = (uint32_t)((uint64_t)i * TEA_DELTA);
        uint32_t sum1 = (uint32_t)((uint64_t)(i + 1) * TEA_DELTA);
        out->s0[i] = key[sum0 & 3] + sum0;
        out->s1[i] = key[(sum1 >> 11) & 3] + sum1;
    }
}

static inline void psa_bf_tea_encrypt_with_schedule(
    uint32_t* v0,
    uint32_t* v1,
    const PsaTeaSchedule* sched) {
    for(int i = 0; i < TEA_ROUNDS; i++) {
        *v0 += (sched->s0[i] ^ (((*v1 >> 5) ^ (*v1 << 4)) + *v1));
        *v1 += (sched->s1[i] ^ (((*v0 >> 5) ^ (*v0 << 4)) + *v0));
    }
}

static inline void psa_bf_tea_decrypt(uint32_t* v0, uint32_t* v1, const uint32_t* key) {
    uint32_t sum = TEA_DELTA * TEA_ROUNDS;
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t k_idx2 = (sum >> 11) & 3;
        uint32_t temp = key[k_idx2] + sum;
        sum = sum - TEA_DELTA;
        *v1 = *v1 - (temp ^ (((*v0 >> 5) ^ (*v0 << 4)) + *v0));
        uint32_t k_idx1 = sum & 3;
        temp = key[k_idx1] + sum;
        *v0 = *v0 - (temp ^ (((*v1 >> 5) ^ (*v1 << 4)) + *v1));
    }
}

typedef struct {
    uint8_t decrypted_button;
    uint32_t decrypted_serial;
    uint32_t decrypted_counter;
    uint16_t decrypted_crc;
    uint32_t decrypted_seed;
    uint8_t decrypted_type;
} PsaBfState;

#define PSA_TYPE_BF1 0x36U
#define PSA_TYPE_BF2 0x37U /* NEU: distinct marker, was 0x36 (same as BF1) before this fix */

/* ALT: today's single shared fill function, used (bugged, for BF2) and unused (correct,
   for BF1) at both call sites. Kept here only as the BF1 reference / ALT-BF2 input. */
static void psa_bf_fill_state_from_buffer(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = PSA_TYPE_BF1;
}

/* NEU: already-proposed BF2 field split (counter/CRC fix), extended here to also set the
   new Type 0x37 so the encoder can tell which kernel produced the result. */
static void psa_bf_fill_state_from_buffer_bf2(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = ((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = PSA_TYPE_BF2;
}

/* ---- protocols/psa.c (reduced to pure buffer logic, ALT = status quo) ---- */

#define PSA_CRYPTO_BF1_CONST_U4 0x0E0F5C41U
#define PSA_CRYPTO_BF1_CONST_U5 0x0F5C4123U
#define PSA_CRYPTO_BF1_START    0x23000000U
#define PSA_CRYPTO_BF1_END      0x24000000U
#define PSA_CRYPTO_BF2_START    0xF3000000U
#define PSA_CRYPTO_BF2_END      0xF4000000U

/* ALT: psa_build_buffer_mode36(), stripped of logging / SubGhz upload framing. Always
   rebuilds the key using the BF1 kernel, whatever the actual origin of the decrypted
   fields was -- this is today's live code for every Type:0x36 manual-generation/replay. */
static void psa_build_buffer_mode36_ALT(
    uint32_t serial,
    uint32_t counter,
    uint8_t button,
    uint16_t crc8_in /* only the low 8 bits are meaningful here, matching live code */,
    uint8_t* buffer_out_2_9 /* 8 bytes: buffer[2..9] */) {
    uint32_t v0 = ((serial & 0xFFFFFF) << 8) | ((button & 0xF) << 4) | ((counter >> 24) & 0xF);
    uint32_t v1 = ((counter & 0xFFFFFF) << 8) | (crc8_in & 0xFF);

    uint8_t crc = psa_crypto_tea_crc(v0, v1);
    v1 = (v1 & 0xFFFFFF00) | crc;

    uint32_t bf_counter = PSA_CRYPTO_BF1_START | (serial & 0xFFFFFF);

    uint32_t wk2 = PSA_CRYPTO_BF1_CONST_U4;
    uint32_t wk3 = bf_counter;
    psa_crypto_tea_encrypt(&wk2, &wk3, psa_crypto_bf1_key_schedule);

    uint32_t wk0 = (bf_counter << 8) | 0x0E;
    uint32_t wk1 = PSA_CRYPTO_BF1_CONST_U5;
    psa_crypto_tea_encrypt(&wk0, &wk1, psa_crypto_bf1_key_schedule);

    uint32_t working_key[4] = {wk0, wk1, wk2, wk3};
    psa_crypto_tea_encrypt(&v0, &v1, working_key);

    uint8_t tmp[10] = {0};
    psa_crypto_unpack_tea_result_to_buffer(tmp, v0, v1);
    memcpy(buffer_out_2_9, &tmp[2], 8);
}

/* NEU: psa_build_buffer_mode37(), mirrors the BF1 function above exactly but with the BF2
   kernel's key derivation (direct schedule XOR counter, no TEA sub-encrypt step) and the
   BF2 16-bit CRC occupying the full low 16 bits of v1 (bytes 8-9), matching the real
   match condition in psa_brute_force_run()'s BF2 loop (verified against it below). */
static void psa_build_buffer_mode37_NEU(
    uint32_t serial,
    uint32_t counter,
    uint8_t button,
    uint8_t* buffer_out_2_9 /* 8 bytes: buffer[2..9] */) {
    uint32_t v0 = ((serial & 0xFFFFFF) << 8) | ((button & 0xF) << 4) | ((counter >> 24) & 0xF);
    uint32_t v1 = ((counter >> 8) & 0xFFFF) << 16;

    uint8_t crc_buffer[6] = {
        (uint8_t)((v0 >> 24) & 0xFF),
        (uint8_t)((v0 >> 8) & 0xFF),
        (uint8_t)((v0 >> 16) & 0xFF),
        (uint8_t)(v0 & 0xFF),
        (uint8_t)((v1 >> 24) & 0xFF),
        (uint8_t)((v1 >> 16) & 0xFF),
    };
    uint16_t crc16 = psa_crypto_crc16_bf2(crc_buffer, 6);
    v1 |= crc16;

    uint32_t bf_counter = PSA_CRYPTO_BF2_START | (serial & 0xFFFFFF);
    uint32_t working_key[4] = {
        psa_crypto_bf2_key_schedule[0] ^ bf_counter,
        psa_crypto_bf2_key_schedule[1] ^ bf_counter,
        psa_crypto_bf2_key_schedule[2] ^ bf_counter,
        psa_crypto_bf2_key_schedule[3] ^ bf_counter,
    };
    psa_crypto_tea_encrypt(&v0, &v1, working_key);

    uint8_t tmp[10] = {0};
    psa_crypto_unpack_tea_result_to_buffer(tmp, v0, v1);
    memcpy(buffer_out_2_9, &tmp[2], 8);
}

/* ---- test harness ---- */

static int g_checks = 0;
static int g_fail = 0;

static void check(int cond, const char* what) {
    g_checks++;
    if(!cond) {
        g_fail++;
        printf("  FAIL: %s\n", what);
    }
}

/* Simulate a real BF1-kernel remote: forward-encrypt (serial, counter, button) into the
   Key1/Key2 payload bytes the air interface would carry, exactly as psa_build_buffer_mode36
   does today (this function and the ALT re-encoder above are therefore identical by
   construction for BF1 -- that is the point: BF1 round-trips because it always used its
   own kernel). */
static void simulate_bf1_remote(uint32_t serial, uint32_t counter, uint8_t button, uint8_t* out_2_9) {
    psa_build_buffer_mode36_ALT(serial, counter, button, 0 /* crc computed inside */, out_2_9);
}

/* Simulate a real BF2-kernel remote. Field layout/key-derivation per psa_build_buffer_mode37_NEU. */
static void simulate_bf2_remote(uint32_t serial, uint32_t counter, uint8_t button, uint8_t* out_2_9) {
    psa_build_buffer_mode37_NEU(serial, counter, button, out_2_9);
}

static void run_bf1_case(uint32_t serial, uint32_t counter, uint8_t button) {
    serial &= 0xFFFFFF;
    counter &= 0xFFFFFFF; /* 28 bits, matches v0's nibble + v1's 24-bit counter field */
    button &= 0xF;

    uint8_t original[8];
    simulate_bf1_remote(serial, counter, button, original);

    /* Recover (key1_low/high, key2_low) the way the decoder does, then re-derive v0/v1
       exactly as psa_crypto_prepare_tea_data() + psa_decrypt_router() would. */
    uint8_t full_buffer[10] = {0};
    memcpy(&full_buffer[2], original, 8);
    uint32_t dec_v0 = ((uint32_t)full_buffer[3] << 16) | ((uint32_t)full_buffer[2] << 24) |
                      ((uint32_t)full_buffer[4] << 8) | (uint32_t)full_buffer[5];
    uint32_t dec_v1 = ((uint32_t)full_buffer[7] << 16) | ((uint32_t)full_buffer[6] << 24) |
                      ((uint32_t)full_buffer[8] << 8) | (uint32_t)full_buffer[9];

    uint32_t bf_counter = PSA_CRYPTO_BF1_START | serial;
    PsaTeaSchedule sched;
    psa_bf_tea_build_schedule(psa_crypto_bf1_key_schedule, &sched);
    uint32_t wk2 = PSA_CRYPTO_BF1_CONST_U4, wk3 = bf_counter;
    psa_bf_tea_encrypt_with_schedule(&wk2, &wk3, &sched);
    uint32_t wk0 = (bf_counter << 8) | 0x0E, wk1 = PSA_CRYPTO_BF1_CONST_U5;
    psa_bf_tea_encrypt_with_schedule(&wk0, &wk1, &sched);
    uint32_t working_key[4] = {wk0, wk1, wk2, wk3};

    uint32_t plain_v0 = dec_v0, plain_v1 = dec_v1;
    psa_bf_tea_decrypt(&plain_v0, &plain_v1, working_key);

    /* Verbatim BF1 match condition from psa_brute_force_run(). */
    check((bf_counter & 0xFFFFFF) == (plain_v0 >> 8), "BF1 match: counter vs serial field");
    uint8_t crc = psa_crypto_tea_crc(plain_v0, plain_v1);
    check(crc == (plain_v1 & 0xFF), "BF1 match: 8-bit CRC");

    uint8_t plain_buffer[10] = {0};
    psa_crypto_unpack_tea_result_to_buffer(plain_buffer, plain_v0, plain_v1);
    PsaBfState state = {0};
    psa_bf_fill_state_from_buffer(&state, plain_buffer);
    check(state.decrypted_serial == serial, "BF1 fill: serial");
    check(state.decrypted_type == PSA_TYPE_BF1, "BF1 fill: type");

    uint8_t regenerated[8];
    psa_build_buffer_mode36_ALT(
        state.decrypted_serial, state.decrypted_counter, state.decrypted_button,
        state.decrypted_crc, regenerated);
    check(memcmp(original, regenerated, 8) == 0, "BF1 round-trip: regenerated key bytes match (regression)");
}

static void run_bf2_case(uint32_t serial, uint32_t counter, uint8_t button) {
    serial &= 0xFFFFFF;
    counter &= 0xFFFFFF00U; /* BF2's wire format never carries a nonzero low counter byte */
    button &= 0xF;

    uint8_t original[8];
    simulate_bf2_remote(serial, counter, button, original);

    uint8_t full_buffer[10] = {0};
    memcpy(&full_buffer[2], original, 8);
    uint32_t dec_v0 = ((uint32_t)full_buffer[3] << 16) | ((uint32_t)full_buffer[2] << 24) |
                      ((uint32_t)full_buffer[4] << 8) | (uint32_t)full_buffer[5];
    uint32_t dec_v1 = ((uint32_t)full_buffer[7] << 16) | ((uint32_t)full_buffer[6] << 24) |
                      ((uint32_t)full_buffer[8] << 8) | (uint32_t)full_buffer[9];

    uint32_t bf_counter = PSA_CRYPTO_BF2_START | serial;
    uint32_t working_key[4] = {
        psa_crypto_bf2_key_schedule[0] ^ bf_counter,
        psa_crypto_bf2_key_schedule[1] ^ bf_counter,
        psa_crypto_bf2_key_schedule[2] ^ bf_counter,
        psa_crypto_bf2_key_schedule[3] ^ bf_counter,
    };
    uint32_t plain_v0 = dec_v0, plain_v1 = dec_v1;
    psa_bf_tea_decrypt(&plain_v0, &plain_v1, working_key);

    /* Verbatim BF2 match condition from psa_brute_force_run(): proves the synthetic wire
       format used by simulate_bf2_remote() above is accepted by the REAL, unmodified
       search/match logic, not just internally self-consistent. */
    check((bf_counter & 0xFFFFFF) == (plain_v0 >> 8), "BF2 match: counter vs serial field");
    uint8_t crc_buffer[6] = {
        (uint8_t)((plain_v0 >> 24) & 0xFF),
        (uint8_t)((plain_v0 >> 8) & 0xFF),
        (uint8_t)((plain_v0 >> 16) & 0xFF),
        (uint8_t)(plain_v0 & 0xFF),
        (uint8_t)((plain_v1 >> 24) & 0xFF),
        (uint8_t)((plain_v1 >> 16) & 0xFF),
    };
    uint16_t crc16 = psa_crypto_crc16_bf2(crc_buffer, 6);
    uint16_t expected_crc = (uint16_t)(plain_v1 & 0xFFFF);
    check(crc16 == expected_crc, "BF2 match: 16-bit CRC");

    uint8_t plain_buffer[10] = {0};
    psa_crypto_unpack_tea_result_to_buffer(plain_buffer, plain_v0, plain_v1);

    /* ALT: today's live code path -- shared fill function (buggy for BF2) feeding the
       BF1-only re-encoder (the only one that exists today). */
    PsaBfState alt_state = {0};
    psa_bf_fill_state_from_buffer(&alt_state, plain_buffer);
    uint8_t alt_regenerated[8];
    psa_build_buffer_mode36_ALT(
        alt_state.decrypted_serial, alt_state.decrypted_counter, alt_state.decrypted_button,
        alt_state.decrypted_crc, alt_regenerated);
    int alt_matches = memcmp(original, alt_regenerated, 8) == 0;
    check(!alt_matches, "BF2 ALT: status quo regenerates WRONG key bytes (expected mismatch)");

    /* NEU: fixed BF2 fill function feeding the new BF2-kernel re-encoder. */
    PsaBfState neu_state = {0};
    psa_bf_fill_state_from_buffer_bf2(&neu_state, plain_buffer);
    check(neu_state.decrypted_type == PSA_TYPE_BF2, "BF2 NEU fill: type is 0x37");
    check(neu_state.decrypted_crc == crc16, "BF2 NEU fill: full 16-bit CRC recovered");
    uint8_t neu_regenerated[8];
    psa_build_buffer_mode37_NEU(
        neu_state.decrypted_serial, neu_state.decrypted_counter, neu_state.decrypted_button,
        neu_regenerated);
    check(memcmp(original, neu_regenerated, 8) == 0, "BF2 NEU: regenerated key bytes match (fix)");
}

int main(void) {
    uint32_t serials[] = {0x000000, 0x000001, 0x123456, 0x7FFFFF, 0xABCDEF, 0xFFFFFF};
    uint32_t counters[] = {0x0000000, 0x0000001, 0x0A5B3C7, 0x8F00001, 0xFFFFFFF};
    uint8_t buttons[] = {0x0, 0x1, 0x8, 0xF};

    int bf1_cases = 0, bf2_cases = 0;
    for(size_t si = 0; si < sizeof(serials) / sizeof(serials[0]); si++) {
        for(size_t ci = 0; ci < sizeof(counters) / sizeof(counters[0]); ci++) {
            for(size_t bi = 0; bi < sizeof(buttons) / sizeof(buttons[0]); bi++) {
                run_bf1_case(serials[si], counters[ci], buttons[bi]);
                bf1_cases++;
                run_bf2_case(serials[si], counters[ci], buttons[bi]);
                bf2_cases++;
            }
        }
    }

    printf(
        "BF1 cases: %d, BF2 cases: %d, total checks: %d, failed: %d\n",
        bf1_cases,
        bf2_cases,
        g_checks,
        g_fail);

    if(g_fail > 0) {
        printf("UNEXPECTED FAILURES (see FAIL lines above)\n");
        return 1;
    }
    printf("All expected outcomes confirmed: BF1 round-trips today; BF2 round-trips ONLY "
           "with the NEU fix; ALT (status quo) mismatches on every BF2 case (exit 0).\n");
    return 0;
}
