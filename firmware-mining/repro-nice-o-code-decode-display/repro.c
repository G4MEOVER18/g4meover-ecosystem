/*
 * Host repro for the Nice O-Code decode/display integration proposed in
 * firmware-mining/nice-o-code-decode-display-port.md (Phase 2 of the Port-Skizze in
 * firmware-mining/nice-o-code-subghz-port.md: struct fields + .sub "IC" field +
 * get_string() dispatch, decoder only, no change to feed()/the encoder).
 *
 * Every function below with an "alt_"/"neu_" prefix is the same transcription used in
 * firmware-mining/repro-nice-o-code-subghz/repro.c (already verified: plain-equivalence
 * at IC_PLAIN, full ic-range round-trip, fuzzed round-trip). This file adds a second,
 * independent layer on top: a minimal DecoderState mirroring exactly the new
 * ic/o_code fields and the new get_string()/deserialize()/reset() logic added to
 * SubGhzProtocolDecoderNiceFlorS, to verify the *integration point* itself (the dispatch
 * in get_string(), the "IC" hex field round trip, and reset() clearing stale state) --
 * not just the cipher primitives it calls.
 *
 * Build: gcc -O0/-O1 -std=c11 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NICE_FLOR_S_IC_PLAIN 0xFFFFu

/* ---- fake rainbow table (32 bytes, only byte 0..31 is ever addressed) ---- */

static uint8_t g_table[32];

static uint8_t table_get_byte(const uint8_t* table, uint32_t address) {
    if(!table) return 0;
    if(address >= 32) return 0;
    return table[address];
}

static inline void magic_xor(uint8_t* p, uint8_t k) {
    for(uint8_t i = 1; i < 6; i++) {
        p[i] ^= k;
    }
}

/* ---- ALT: existing plain Nice Flor-S cipher (nice_flor_s.c, mntm-012) ---- */

static uint64_t alt_encrypt(uint64_t data, const uint8_t* table) {
    uint8_t* p = (uint8_t*)&data;
    uint8_t k = 0;
    for(uint8_t y = 0; y < 2; y++) {
        k = table_get_byte(table, p[0] & 0x1f);
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0xe0;
        k = table_get_byte(table, p[0] >> 3) + 0x25;
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0x7;
        if(y == 0) {
            k = p[0];
            p[0] = p[1];
            p[1] = k;
        }
    }
    p[5] = ~p[5] & 0x0f;
    k = ~p[4];
    p[4] = ~p[0];
    p[0] = ~p[2];
    p[2] = k;
    k = ~p[3];
    p[3] = ~p[1];
    p[1] = k;
    return data;
}

static uint64_t alt_decrypt(uint64_t data, const uint8_t* table) {
    uint8_t* p = (uint8_t*)&data;
    uint8_t k = 0;
    k = ~p[4];
    p[5] = ~p[5];
    p[4] = ~p[2];
    p[2] = ~p[0];
    p[0] = k;
    k = ~p[3];
    p[3] = ~p[1];
    p[1] = k;
    for(uint8_t y = 0; y < 2; y++) {
        k = table_get_byte(table, p[0] >> 3) + 0x25;
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0x7;
        k = table_get_byte(table, p[0] & 0x1f);
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0xe0;
        if(y == 0) {
            k = p[0];
            p[0] = p[1];
            p[1] = k;
        }
    }
    return data;
}

/* ---- NEU: Nice O-Code helpers (already proposed/verified, reused unmodified) ---- */

static uint64_t neu_encrypt_ic(uint64_t data, uint16_t ic, const uint8_t* table) {
    uint8_t* p = (uint8_t*)&data;
    uint8_t k = 0;
    for(uint8_t y = 0; y < 2; y++) {
        k = table_get_byte(table, p[0] & 0x1f);
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0xe0;
        k = table_get_byte(table, p[0] >> 3) + 0x25;
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0x7;
        if(y == 0) {
            k = p[0];
            p[0] = p[1];
            p[1] = k;
        }
    }
    p[5] = ~p[5] & 0x0f;
    k = ~p[4];
    p[4] = p[0] ^ (uint8_t)(ic >> 8);
    p[0] = ~p[2];
    p[2] = k;
    k = p[1] ^ (uint8_t)ic;
    p[1] = ~p[3];
    p[3] = k;
    return data;
}

static uint64_t neu_decrypt_ic(uint64_t data, uint16_t ic, const uint8_t* table) {
    uint8_t* p = (uint8_t*)&data;
    uint8_t k = 0;
    k = p[4] ^ (uint8_t)(ic >> 8);
    p[5] = ~p[5];
    p[4] = ~p[2];
    p[2] = ~p[0];
    p[0] = k;
    k = p[3] ^ (uint8_t)ic;
    p[3] = ~p[1];
    p[1] = k;
    for(uint8_t y = 0; y < 2; y++) {
        k = table_get_byte(table, p[0] >> 3) + 0x25;
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0x7;
        k = table_get_byte(table, p[0] & 0x1f);
        magic_xor(p, k);
        p[5] &= 0x0f;
        p[0] ^= k & 0xe0;
        if(y == 0) {
            k = p[0];
            p[0] = p[1];
            p[1] = k;
        }
    }
    return data;
}

static void nice_o_mask(uint8_t n, uint32_t* mask_32, uint16_t* mask_16) {
    const uint32_t a = (uint32_t)(0x0Eu - n) & 0x0Fu;
    const uint32_t b = (uint32_t)(n + 2u);
    const uint32_t odd = (uint32_t)n & 1u;
    const uint32_t c = b + (odd << 1);
    const uint32_t bn = b & 0x0Fu;
    const uint32_t cn = c & 0x0Fu;

    uint32_t m = (a << 8) | (bn << 4) | (bn << 20) | (a << 24) | cn | (cn << 16);
    m |= (((((b ^ (uint32_t)(0x0Eu - n) ^ c) & 0x0Fu) + (odd << 2)) << 12) & 0xFFFFu);

    if(mask_32) *mask_32 = m & 0x0FFFFFFFu;
    if(mask_16) *mask_16 = (uint16_t)(((m & 0x00FFu) << 8) | ((m >> 8) & 0x00FFu));
}

static uint8_t nice_o_get_parcel(uint64_t data) {
    return (uint8_t)((((data >> 48) & 0x0Fu) ^ ((data >> 44) & 0x0Fu)) ^ 0x0Fu);
}

/* ---- transcription of the proposed decoder integration (Phase 2 patch) ---- */

typedef struct {
    uint64_t data; /* instance->generic.data */
    uint32_t serial; /* instance->generic.serial */
    uint16_t cnt; /* instance->generic.cnt */
    uint8_t btn; /* instance->generic.btn */
    uint16_t ic; /* instance->ic (new) */
    bool o_code; /* instance->o_code (new) */
} DecoderState;

/* 1:1 with subghz_protocol_decoder_nice_flor_s_alloc()'s new two lines */
static void decoder_alloc(DecoderState* st) {
    memset(st, 0, sizeof(*st));
    st->ic = NICE_FLOR_S_IC_PLAIN;
    st->o_code = false;
}

/* 1:1 with subghz_protocol_decoder_nice_flor_s_reset()'s new two lines */
static void decoder_reset(DecoderState* st) {
    st->ic = NICE_FLOR_S_IC_PLAIN;
    st->o_code = false;
}

/* 1:1 with the new "IC" hex field read added to
 * subghz_protocol_decoder_nice_flor_s_deserialize(); ic_field_present/ic_field_value
 * stand in for flipper_format_read_hex()'s return value and parsed bytes. */
static void deserialize_ic_field(DecoderState* st, bool ic_field_present, uint16_t ic_field_value) {
    if(ic_field_present) {
        st->ic = ic_field_value;
        st->o_code = true;
    } else {
        st->ic = NICE_FLOR_S_IC_PLAIN;
        st->o_code = false;
    }
}

/* 1:1 with the new "IC" hex field write added to
 * subghz_protocol_decoder_nice_flor_s_serialize(). */
static bool serialize_ic_field(const DecoderState* st, uint16_t* out_ic) {
    if(!st->o_code) return false;
    *out_ic = st->ic;
    return true;
}

/* subghz_protocol_nice_flor_s_remote_controller()'s plain branch (file_name != NULL),
 * minus the custom-button bookkeeping, which this patch does not touch. */
static void alt_remote_controller(DecoderState* st, const uint8_t* table) {
    uint64_t decrypt = alt_decrypt(st->data, table);
    st->cnt = (uint16_t)(decrypt & 0xFFFF);
    st->serial = (uint32_t)((decrypt >> 16) & 0xFFFFFFFu);
    st->btn = (uint8_t)((decrypt >> 48) & 0xF);
}

/* 1:1 with the new dispatch at the top of
 * subghz_protocol_decoder_nice_flor_s_get_string(). */
static void get_string_decode(DecoderState* st, const uint8_t* table) {
    if(st->o_code) {
        uint8_t n = nice_o_get_parcel(st->data);
        uint32_t mask_32;
        uint16_t mask_16;
        nice_o_mask(n, &mask_32, &mask_16);
        uint64_t decrypted =
            neu_decrypt_ic(st->data & 0xFFFFFFFFFFFULL, (uint16_t)(st->ic ^ mask_16), table);
        st->cnt = (uint16_t)decrypted;
        st->serial = (uint32_t)((decrypted >> 16) ^ mask_32) & 0x0FFFFFFFu;
        st->btn = (uint8_t)((st->data >> 48) & 0xF);
    } else {
        alt_remote_controller(st, table);
    }
}

/* ---- test harness ---- */

static uint64_t g_checks = 0;
static uint64_t g_failures = 0;

#define CHECK(cond, msg, ...)                                                            \
    do {                                                                                 \
        g_checks++;                                                                      \
        if(!(cond)) {                                                                    \
            g_failures++;                                                                \
            fprintf(stderr, "FAIL %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
        }                                                                                 \
    } while(0)

static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static void fill_random_table(uint8_t* table, uint64_t* rng) {
    for(int i = 0; i < 32; i++) table[i] = (uint8_t)(xorshift64(rng) & 0xFF);
}

/* Builds the full on-air "data" field for a plain Flor-S frame, as
 * subghz_protocol_encoder_nice_flor_s_get_upload() would (same ciphertext repeated,
 * P0/P1 header byte set for one of the 16 repeats). */
static uint64_t build_plain_frame(uint32_t serial, uint16_t cnt, uint8_t btn, uint8_t n,
                                   const uint8_t* table) {
    uint64_t body = alt_encrypt(((uint64_t)serial << 16) | cnt, table);
    uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ n));
    return ((uint64_t)byte << 44) | (body & 0xFFFFFFFFFFFULL);
}

/* Builds the full on-air "data" field for an O-Code frame, per the pipeline already
 * verified in firmware-mining/repro-nice-o-code-subghz/repro.c step 6. */
static uint64_t build_o_code_frame(uint32_t serial, uint16_t cnt, uint8_t btn, uint16_t ic,
                                    uint8_t n, const uint8_t* table) {
    uint32_t mask_32;
    uint16_t mask_16;
    nice_o_mask(n, &mask_32, &mask_16);
    uint64_t plain = (((uint64_t)(serial ^ mask_32) & 0x0FFFFFFFu) << 16) | cnt;
    uint64_t enc_body = neu_encrypt_ic(plain, (uint16_t)(ic ^ mask_16), table);
    uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ n));
    return ((uint64_t)byte << 44) | (enc_body & 0xFFFFFFFFFFFULL);
}

int main(void) {
    uint64_t rng = 0xA5A5C0FFEEULL;

    /* --- 1. Backward-compat regression: o_code == false (every pre-existing .sub file,
     *        and every live capture per reset()) must take the untouched plain path and
     *        reproduce the exact (serial, cnt, btn) the frame was built from. --- */
    for(int t = 0; t < 2000; t++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint8_t n = (uint8_t)(xorshift64(&rng) & 0x0F);

        DecoderState st;
        decoder_alloc(&st);
        deserialize_ic_field(&st, false, 0); /* no "IC" field on this file */
        CHECK(!st.o_code, "regression: alloc+absent-IC must leave o_code false");
        CHECK(st.ic == NICE_FLOR_S_IC_PLAIN, "regression: alloc+absent-IC must leave ic == PLAIN");

        st.data = build_plain_frame(serial, cnt, btn, n, g_table);
        get_string_decode(&st, g_table);

        CHECK(st.cnt == cnt, "regression: cnt mismatch got=0x%x want=0x%x", st.cnt, cnt);
        CHECK(st.serial == serial, "regression: serial mismatch got=0x%x want=0x%x", st.serial,
              serial);
        CHECK(st.btn == btn, "regression: btn mismatch got=0x%x want=0x%x", st.btn, btn);

        /* dispatch must be a no-op versus calling the unmodified plain path directly */
        DecoderState direct;
        memset(&direct, 0, sizeof(direct));
        direct.data = st.data;
        alt_remote_controller(&direct, g_table);
        CHECK(direct.cnt == st.cnt && direct.serial == st.serial && direct.btn == st.btn,
              "regression: get_string_decode() dispatch diverged from direct remote_controller()");
    }
    fprintf(stderr, "[1/5] backward-compat regression: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 2. O-Code end-to-end through the real integration points (deserialize's IC
     *        field -> get_string()'s dispatch), not just the raw cipher functions. --- */
    for(int i = 0; i < 200000; i++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint16_t ic = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t n = (uint8_t)(xorshift64(&rng) & 0x0F);

        DecoderState st;
        decoder_alloc(&st);
        deserialize_ic_field(&st, true, ic); /* "IC" field present on this file */
        CHECK(st.o_code, "e2e: present-IC must set o_code true");
        CHECK(st.ic == ic, "e2e: present-IC must store the parsed value");

        st.data = build_o_code_frame(serial, cnt, btn, ic, n, g_table);
        get_string_decode(&st, g_table);

        CHECK(st.cnt == cnt, "e2e: cnt mismatch got=0x%x want=0x%x", st.cnt, cnt);
        CHECK(st.serial == serial, "e2e: serial mismatch got=0x%x want=0x%x", st.serial, serial);
        CHECK(st.btn == btn, "e2e: btn mismatch got=0x%x want=0x%x", st.btn, btn);

        /* "IC" field written back out (serialize) must reproduce exactly what was read */
        uint16_t written_ic = 0;
        bool present = serialize_ic_field(&st, &written_ic);
        CHECK(present, "e2e: serialize must emit IC when o_code is set");
        CHECK(written_ic == ic, "e2e: serialize must round-trip the exact ic value");

        /* decoding under an unrelated ic must not also recover the true (serial, cnt) */
        if(ic != 0xFFFF) {
            DecoderState wrong = st;
            wrong.ic = (uint16_t)(ic + 1);
            get_string_decode(&wrong, g_table);
            CHECK(wrong.serial != serial || wrong.cnt != cnt,
                  "e2e: wrong ic=0x%x still recovered the true (serial,cnt)", wrong.ic);
        }
    }
    fprintf(stderr, "[2/5] O-Code end-to-end via deserialize+get_string: %llu checks so far, "
                     "%llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 3. reset() must clear a previously loaded O-Code IC (Port-Skizze step 1's
     *        documented requirement) -- confirmed here by actually exercising the
     *        cross-contamination scenario it guards against: load an O-Code file, then
     *        receive a brand-new plain live frame without the UI ever reallocating the
     *        decoder instance. --- */
    for(int t = 0; t < 2000; t++) {
        fill_random_table(g_table, &rng);
        uint16_t stale_ic = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        DecoderState st;
        decoder_alloc(&st);
        deserialize_ic_field(&st, true, stale_ic);
        CHECK(st.o_code, "reset: setup must leave o_code true before reset");

        decoder_reset(&st);
        CHECK(!st.o_code, "reset: must clear o_code");
        CHECK(st.ic == NICE_FLOR_S_IC_PLAIN, "reset: must clear ic to PLAIN");

        /* a subsequent live (plain) capture, never o_code, must decode correctly and
         * independently of stale_ic -- it is never read once o_code is false */
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint8_t n = (uint8_t)(xorshift64(&rng) & 0x0F);
        st.data = build_plain_frame(serial, cnt, btn, n, g_table);
        get_string_decode(&st, g_table);
        CHECK(st.cnt == cnt && st.serial == serial && st.btn == btn,
              "reset: post-reset live plain frame decoded incorrectly (stale_ic=0x%x)",
              stale_ic);
    }
    fprintf(stderr, "[3/5] reset() clears stale O-Code state: %llu checks so far, %llu "
                     "failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 4. .sub field round trip is symmetric for the absent case too (serialize must
     *        never emit "IC" when o_code is false, matching every pre-existing file). --- */
    for(int t = 0; t < 1000; t++) {
        DecoderState st;
        decoder_alloc(&st);
        deserialize_ic_field(&st, false, 0);
        uint16_t written_ic = 0xDEAD;
        bool present = serialize_ic_field(&st, &written_ic);
        CHECK(!present, "round trip: serialize must not emit IC when o_code is false");
        CHECK(written_ic == 0xDEAD, "round trip: out-param must be left untouched when absent");
    }
    fprintf(stderr, "[4/5] absent-IC round trip: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 5. Fixed worked example, printed for manual cross-check against a real .sub
     *        file edited by hand (IC field in hex, big-endian, matching the "Key" field's
     *        existing byte order convention in this file). --- */
    {
        fill_random_table(g_table, &rng);
        uint32_t serial = 0x0ABCDEFu & 0x0FFFFFFFu;
        uint16_t cnt = 0x1234;
        uint8_t btn = 0x2;
        uint16_t ic = 0x55AA;
        uint8_t n = 7;
        uint64_t frame = build_o_code_frame(serial, cnt, btn, ic, n, g_table);

        DecoderState st;
        decoder_alloc(&st);
        deserialize_ic_field(&st, true, ic);
        st.data = frame;
        get_string_decode(&st, g_table);

        fprintf(stderr,
                "[5/5] worked example: frame=0x%013llX IC=%04X -> Sn:%05lX Cnt:%04X Btn:%02X "
                "(want Sn:%05lX Cnt:%04X Btn:%02X)\n",
                (unsigned long long)frame, ic, (unsigned long)st.serial, st.cnt, st.btn,
                (unsigned long)serial, cnt, btn);
        CHECK(st.serial == serial && st.cnt == cnt && st.btn == btn,
              "worked example mismatch");
    }

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED (%llu checks)\n", (unsigned long long)g_checks);
        return 0;
    } else {
        printf("%llu of %llu CHECKS FAILED\n", (unsigned long long)g_failures,
               (unsigned long long)g_checks);
        return 1;
    }
}
