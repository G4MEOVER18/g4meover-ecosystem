/*
 * Host repro for the Nice O-Code port proposal (firmware-mining/nice-o-code-subghz-port.md).
 *
 * Verifies, without any Flipper firmware headers or hardware, the four new pure
 * functions proposed for lib/subghz/protocols/nice_flor_s.c:
 *   - subghz_protocol_nice_flor_s_encrypt_ic() / _decrypt_ic()  (IC-parameterised cipher)
 *   - subghz_protocol_nice_o_mask()                             (per-parcel obfuscation mask)
 *   - subghz_protocol_nice_o_get_parcel()                       (parcel index recovery)
 *
 * "ALT" below is my own existing subghz_protocol_nice_flor_s_encrypt()/_decrypt() from
 * lib/subghz/protocols/nice_flor_s.c (Momentum, tag mntm-012), transcribed 1:1 except that
 * the rainbow-table byte lookup reads an in-memory array instead of an SD card file (the
 * cipher itself never inspects the table's content beyond a byte lookup, so any 32-byte
 * table exercises the same code paths as a real one). "NEU" is the proposed IC-parameterised
 * pair plus the two new Nice O-Code helper functions, ported from Unleashed-Firmware
 * (github.com/DarkFlippers/unleashed-firmware, dev, commit 945234a) lib/subghz/protocols/
 * nice_flor_s.c, adapted to call my own per-byte table reader instead of Unleashed's
 * buffer-caching one (see "Warum kein Patch fuer die Tabellen-Zwischenspeicherung" in the
 * .md).
 *
 * Build: gcc -O0/-O1 -std=c11 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- fake rainbow table (32 bytes, only byte 0..31 is ever addressed) ---- */

static uint8_t g_table[32];

static uint8_t table_get_byte(const uint8_t* table, uint32_t address) {
    /* mirrors subghz_protocol_nice_flor_s_get_byte_in_file()'s NULL/OOB guards */
    if(!table) return 0;
    if(address >= 32) return 0;
    return table[address];
}

static inline void magic_xor(uint8_t* p, uint8_t k) {
    for(uint8_t i = 1; i < 6; i++) {
        p[i] ^= k;
    }
}

/* ---- ALT: my existing plain Nice Flor-S cipher (nice_flor_s.c, mntm-012) ---- */

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

/* ---- NEU: proposed IC-parameterised cipher + Nice O-Code helpers ---- */

#define NICE_FLOR_S_IC_PLAIN 0xFFFFu

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

/* ---- test harness ---- */

static uint64_t g_checks = 0;
static uint64_t g_failures = 0;

#define CHECK(cond, msg, ...)                                                  \
    do {                                                                      \
        g_checks++;                                                           \
        if(!(cond)) {                                                         \
            g_failures++;                                                     \
            fprintf(stderr, "FAIL %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                      \
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

int main(void) {
    uint64_t rng = 0x1234567890abcdefULL;

    /* --- 1. Plain-equivalence regression: NEU at IC_PLAIN must match ALT bit for bit,
     *        across many random 44-bit bodies and several independent tables. --- */
    for(int t = 0; t < 20; t++) {
        fill_random_table(g_table, &rng);
        for(int i = 0; i < 20000; i++) {
            uint64_t data = xorshift64(&rng) & 0xFFFFFFFFFFFULL; /* 44 bits */
            uint64_t alt_enc = alt_encrypt(data, g_table);
            uint64_t neu_enc = neu_encrypt_ic(data, NICE_FLOR_S_IC_PLAIN, g_table);
            CHECK(alt_enc == neu_enc, "encrypt mismatch at IC_PLAIN: data=0x%llx table=%d",
                  (unsigned long long)data, t);

            uint64_t alt_dec = alt_decrypt(alt_enc, g_table);
            uint64_t neu_dec = neu_decrypt_ic(neu_enc, NICE_FLOR_S_IC_PLAIN, g_table);
            CHECK(alt_dec == neu_dec, "decrypt mismatch at IC_PLAIN: data=0x%llx table=%d",
                  (unsigned long long)data, t);
            CHECK(alt_dec == data, "ALT round-trip broke: data=0x%llx table=%d",
                  (unsigned long long)data, t);
        }
    }
    fprintf(stderr, "[1/5] plain-equivalence: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 2a. Cipher round-trip across the FULL ic range (0..65535), fixed data/table. --- */
    fill_random_table(g_table, &rng);
    for(uint32_t ic = 0; ic <= 0xFFFF; ic++) {
        uint64_t data = 0x0A1B2C3D4E5ULL & 0xFFFFFFFFFFFULL;
        uint64_t enc = neu_encrypt_ic(data, (uint16_t)ic, g_table);
        uint64_t dec = neu_decrypt_ic(enc, (uint16_t)ic, g_table);
        CHECK(dec == data, "round-trip failed at ic=0x%x", ic);
        /* decrypting under the WRONG ic must not silently also recover the plaintext
         * (would indicate the ic has no effect on part of the cipher) */
        if(ic != 0) {
            uint64_t wrong = neu_decrypt_ic(enc, (uint16_t)(ic - 1), g_table);
            CHECK(wrong != data, "ic=0x%x and ic=0x%x both decrypt to the same plaintext", ic,
                  ic - 1);
        }
    }
    fprintf(stderr, "[2a/5] full ic-range round-trip: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 2b. Cipher round-trip fuzzed over data/ic/table jointly. --- */
    for(int i = 0; i < 200000; i++) {
        fill_random_table(g_table, &rng);
        uint64_t data = xorshift64(&rng) & 0xFFFFFFFFFFFULL;
        uint16_t ic = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint64_t enc = neu_encrypt_ic(data, ic, g_table);
        uint64_t dec = neu_decrypt_ic(enc, ic, g_table);
        CHECK(dec == data, "fuzzed round-trip failed: data=0x%llx ic=0x%x",
              (unsigned long long)data, ic);
    }
    fprintf(stderr, "[2b/5] fuzzed round-trip: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 3. get_parcel() depends only on P0/P1 (bits 44..51), independent of the
     *        44-bit ciphertext body below it. --- */
    for(int trial = 0; trial < 20000; trial++) {
        uint8_t n = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint64_t body = xorshift64(&rng) & 0xFFFFFFFFFFFULL;
        uint64_t frame = ((uint64_t)btn << 48) | ((uint64_t)(0xF ^ btn ^ n) << 44) | body;
        uint8_t recovered = nice_o_get_parcel(frame);
        CHECK(recovered == n, "get_parcel: expected n=%u got %u (btn=%u body=0x%llx)", n,
              recovered, btn, (unsigned long long)body);
    }
    fprintf(stderr, "[3/5] get_parcel independence: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    /* --- 4. mask(): pure/deterministic, and every mask_32 fits the 28 bit serial field. --- */
    uint32_t masks_32[16];
    uint16_t masks_16[16];
    for(uint8_t n = 0; n < 16; n++) {
        uint32_t m32a, m32b;
        uint16_t m16a, m16b;
        nice_o_mask(n, &m32a, &m16a);
        nice_o_mask(n, &m32b, &m16b); /* called twice: must be pure */
        CHECK(m32a == m32b && m16a == m16b, "mask() not deterministic for n=%u", n);
        CHECK((m32a & ~0x0FFFFFFFu) == 0, "mask_32 exceeds 28 bits for n=%u: 0x%x", n, m32a);
        masks_32[n] = m32a;
        masks_16[n] = m16a;
    }
    /* informational: dump the 16 masks so a future companion app can sanity-check its
     * own transcription against this table without re-deriving the formula */
    fprintf(stderr, "[4/5] nice_o_mask() table (n: mask_32 mask_16):\n");
    for(uint8_t n = 0; n < 16; n++) {
        fprintf(stderr, "       %2u: 0x%07x 0x%04x\n", n, masks_32[n], masks_16[n]);
    }
    fprintf(stderr, "       %llu checks so far, %llu failures\n", (unsigned long long)g_checks,
            (unsigned long long)g_failures);

    /* --- 5. Full O-Code pipeline: build a synthetic on-air frame the way the proposed
     *        subghz_protocol_nice_flor_s_remote_controller() o_code branch would consume
     *        it, then recover (serial, cnt, btn) exactly as documented in the .md's
     *        Port-Skizze for that (not-yet-ported) integration point. --- */
    for(int i = 0; i < 200000; i++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint16_t ic = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t n = (uint8_t)(xorshift64(&rng) & 0x0F);

        uint32_t mask_32;
        uint16_t mask_16;
        nice_o_mask(n, &mask_32, &mask_16);

        uint64_t plain = (((uint64_t)(serial ^ mask_32) & 0x0FFFFFFFu) << 16) | cnt;
        uint64_t enc_body = neu_encrypt_ic(plain, (uint16_t)(ic ^ mask_16), g_table);

        uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ n));
        uint64_t frame = ((uint64_t)byte << 44) | (enc_body & 0xFFFFFFFFFFFULL);

        uint8_t recovered_n = nice_o_get_parcel(frame);
        CHECK(recovered_n == n, "pipeline: parcel index mismatch");

        uint32_t rmask_32;
        uint16_t rmask_16;
        nice_o_mask(recovered_n, &rmask_32, &rmask_16);
        uint64_t decrypted =
            neu_decrypt_ic(frame & 0xFFFFFFFFFFFULL, (uint16_t)(ic ^ rmask_16), g_table);

        uint16_t recovered_cnt = (uint16_t)decrypted;
        uint32_t recovered_serial = (uint32_t)((decrypted >> 16) ^ rmask_32) & 0x0FFFFFFFu;
        uint8_t recovered_btn = (uint8_t)((frame >> 48) & 0x0Fu);

        CHECK(recovered_cnt == cnt, "pipeline: cnt mismatch got=0x%x want=0x%x", recovered_cnt,
              cnt);
        CHECK(recovered_serial == serial, "pipeline: serial mismatch got=0x%x want=0x%x",
              recovered_serial, serial);
        CHECK(recovered_btn == btn, "pipeline: btn mismatch got=0x%x want=0x%x", recovered_btn,
              btn);

        /* decrypting the SAME frame with an unrelated installer code must not also
         * recover the true serial (sanity check that ic actually gates the result) */
        uint16_t bad_ic = (uint16_t)(ic + 1);
        uint64_t bad_decrypted =
            neu_decrypt_ic(frame & 0xFFFFFFFFFFFULL, (uint16_t)(bad_ic ^ rmask_16), g_table);
        uint32_t bad_serial = (uint32_t)((bad_decrypted >> 16) ^ rmask_32) & 0x0FFFFFFFu;
        if((uint16_t)(ic + 1) != ic) {
            CHECK(bad_serial != serial || (uint16_t)bad_decrypted != cnt,
                  "pipeline: wrong ic=0x%x still recovered the true (serial,cnt)", bad_ic);
        }
    }
    fprintf(stderr, "[5/5] full O-Code pipeline round-trip: %llu checks so far, %llu failures\n",
            (unsigned long long)g_checks, (unsigned long long)g_failures);

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED (%llu checks)\n", (unsigned long long)g_checks);
        return 0;
    } else {
        printf("%llu of %llu CHECKS FAILED\n", (unsigned long long)g_failures,
               (unsigned long long)g_checks);
        return 1;
    }
}
