/*
 * Host-Repro: Beninca-ARC-AES-SubGHz-Protokoll (Unleashed lib/subghz/protocols/beninca_arc.c
 * + aes_common.c, Tag unlshd-093).
 *
 * Zweck:
 *  1) Die AES-128-Primitive aus aes_common.c (SBox, ShiftRows, MixColumns, KeySchedule) wortwoertlich
 *     transkribiert und gegen den offiziellen FIPS-197-Testvektor verifiziert (unabhaengige Quelle,
 *     kein Vergleich nur gegen sich selbst).
 *  2) Die protokollspezifische Schluesselableitung + Encrypt/Decrypt (get_..._aes_key(),
 *     subghz_protocol_beninca_arc_encrypt/decrypt()) transkribiert und im Rundlauf verifiziert.
 *  3) Die Decoder-Bit-Erfassungslogik aus subghz_protocol_decoder_beninca_arc_feed()
 *     (Zustand BenincaARCDecoderHighLevel, "Half data captured" / "Full data captured") als
 *     reine Zustandsmaschine nachgebildet -- einmal ALT (wortgetreu, inkl. "decode_data != 0"-Gate)
 *     und einmal NEU (vorgeschlagener Fix: explizites Bool-Flag statt Daten-Ungleich-Null-Test) --
 *     und die darin liegende Randfall-Luecke gezielt nachgewiesen UND ihre reale Eintrittswahrschein-
 *     lichkeit (ueber viele echte AES-Verschluesselungen) gemessen.
 *
 * Kompiliert und ausgefuehrt mit:
 *   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
 *       repro.c -o repro_o0 && ./repro_o0
 *   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
 *       repro.c -o repro_o1 && ./repro_o1
 *
 * Keine Furi-/Flipper-Abhaengigkeiten, keine echten Fernbedienungs-/Schluesseldaten -- alle
 * Testwerte sind synthetisch/zufaellig oder der oeffentliche FIPS-197-Beispielvektor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/* ------------------------------------------------------------------------------------------- *
 * 1) AES-128 -- woertliche Transkription aus Unleasheds lib/subghz/protocols/aes_common.c
 * ------------------------------------------------------------------------------------------- */

static const uint8_t aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab,
    0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4,
    0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71,
    0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6,
    0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb,
    0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45,
    0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44,
    0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a,
    0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49,
    0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25,
    0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e,
    0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1,
    0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb,
    0x16};

static const uint8_t aes_sbox_inv[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7,
    0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde,
    0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42,
    0xfa, 0xc3, 0x4e, 0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
    0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c,
    0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15,
    0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84, 0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7,
    0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc,
    0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73, 0x96, 0xac, 0x74, 0x22, 0xe7, 0xad,
    0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d,
    0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
    0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4, 0x1f, 0xdd, 0xa8,
    0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f, 0x60, 0x51,
    0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0,
    0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c,
    0x7d};

static const uint8_t aes_rcon[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

static uint8_t gf_mul2(uint8_t x) {
    return ((x >> 7) * 0x1b) ^ (uint8_t)(x << 1);
}

static void aes_subbytes(uint8_t* state) {
    for(uint8_t row = 0; row < 4; row++) {
        for(uint8_t col = 0; col < 4; col++) {
            state[row + col * 4] = aes_sbox[state[row + col * 4]];
        }
    }
}

static void aes_subbytes_inv(uint8_t* state) {
    for(uint8_t row = 0; row < 4; row++) {
        for(uint8_t col = 0; col < 4; col++) {
            state[row + col * 4] = aes_sbox_inv[state[row + col * 4]];
        }
    }
}

static void aes_shiftrows(uint8_t* state) {
    uint8_t temp;

    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

static void aes_shiftrows_inv(uint8_t* state) {
    uint8_t temp;

    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;

    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

static void aes_mixcolumns(uint8_t* state) {
    uint8_t a, b, c, d;
    for(uint8_t i = 0; i < 4; i++) {
        a = state[i * 4];
        b = state[i * 4 + 1];
        c = state[i * 4 + 2];
        d = state[i * 4 + 3];

        uint8_t a2 = gf_mul2(a);
        uint8_t b2 = gf_mul2(b);
        uint8_t c2 = gf_mul2(c);
        uint8_t d2 = gf_mul2(d);

        state[i * 4] = a2 ^ b2 ^ b ^ c ^ d;
        state[i * 4 + 1] = a ^ b2 ^ c2 ^ c ^ d;
        state[i * 4 + 2] = a ^ b ^ c2 ^ d2 ^ d;
        state[i * 4 + 3] = a2 ^ a ^ b ^ c ^ d2;
    }
}

static void aes_mixcolumns_inv(uint8_t* state) {
    uint8_t a, b, c, d;
    for(uint8_t i = 0; i < 4; i++) {
        a = state[i * 4];
        b = state[i * 4 + 1];
        c = state[i * 4 + 2];
        d = state[i * 4 + 3];

        uint8_t a2 = gf_mul2(a);
        uint8_t a4 = gf_mul2(a2);
        uint8_t a8 = gf_mul2(a4);
        uint8_t b2 = gf_mul2(b);
        uint8_t b4 = gf_mul2(b2);
        uint8_t b8 = gf_mul2(b4);
        uint8_t c2 = gf_mul2(c);
        uint8_t c4 = gf_mul2(c2);
        uint8_t c8 = gf_mul2(c4);
        uint8_t d2 = gf_mul2(d);
        uint8_t d4 = gf_mul2(d2);
        uint8_t d8 = gf_mul2(d4);

        state[i * 4] = (a8 ^ a4 ^ a2) ^ (b8 ^ b2 ^ b) ^ (c8 ^ c4 ^ c) ^ (d8 ^ d);
        state[i * 4 + 1] = (a8 ^ a) ^ (b8 ^ b4 ^ b2) ^ (c8 ^ c2 ^ c) ^ (d8 ^ d4 ^ d);
        state[i * 4 + 2] = (a8 ^ a4 ^ a) ^ (b8 ^ b) ^ (c8 ^ c4 ^ c2) ^ (d8 ^ d2 ^ d);
        state[i * 4 + 3] = (a8 ^ a2 ^ a) ^ (b8 ^ b4 ^ b) ^ (c8 ^ c) ^ (d8 ^ d4 ^ d2);
    }
}

static void aes_addroundkey(uint8_t* state, const uint8_t* round_key) {
    for(uint8_t col = 0; col < 4; col++) {
        state[col * 4] ^= round_key[col * 4];
        state[col * 4 + 1] ^= round_key[col * 4 + 1];
        state[col * 4 + 2] ^= round_key[col * 4 + 2];
        state[col * 4 + 3] ^= round_key[col * 4 + 3];
    }
}

static void aes_key_expansion(const uint8_t* key, uint8_t* round_keys) {
    for(uint8_t i = 0; i < 16; i++) {
        round_keys[i] = key[i];
    }

    for(uint8_t i = 4; i < 44; i++) {
        uint8_t prev_word_idx = (i - 1) * 4;
        uint8_t b0 = round_keys[prev_word_idx];
        uint8_t b1 = round_keys[prev_word_idx + 1];
        uint8_t b2 = round_keys[prev_word_idx + 2];
        uint8_t b3 = round_keys[prev_word_idx + 3];

        if((i % 4) == 0) {
            uint8_t new_b0 = aes_sbox[b1] ^ aes_rcon[(i / 4) - 1];
            uint8_t new_b1 = aes_sbox[b2];
            uint8_t new_b2 = aes_sbox[b3];
            uint8_t new_b3 = aes_sbox[b0];
            b0 = new_b0;
            b1 = new_b1;
            b2 = new_b2;
            b3 = new_b3;
        }

        uint8_t back_word_idx = (i - 4) * 4;
        b0 ^= round_keys[back_word_idx];
        b1 ^= round_keys[back_word_idx + 1];
        b2 ^= round_keys[back_word_idx + 2];
        b3 ^= round_keys[back_word_idx + 3];

        uint8_t curr_word_idx = i * 4;
        round_keys[curr_word_idx] = b0;
        round_keys[curr_word_idx + 1] = b1;
        round_keys[curr_word_idx + 2] = b2;
        round_keys[curr_word_idx + 3] = b3;
    }
}

static void aes128_encrypt(const uint8_t* expanded_key, uint8_t* data) {
    uint8_t state[16];
    memcpy(state, data, 16);

    aes_addroundkey(state, &expanded_key[0]);

    for(uint8_t round = 1; round < 10; round++) {
        aes_subbytes(state);
        aes_shiftrows(state);
        aes_mixcolumns(state);
        aes_addroundkey(state, &expanded_key[round * 16]);
    }

    aes_subbytes(state);
    aes_shiftrows(state);
    aes_addroundkey(state, &expanded_key[160]);

    memcpy(data, state, 16);
}

static void aes128_decrypt(const uint8_t* expanded_key, uint8_t* data) {
    uint8_t state[16];
    memcpy(state, data, 16);

    aes_addroundkey(state, &expanded_key[160]);

    for(uint8_t round = 9; round > 0; round--) {
        aes_shiftrows_inv(state);
        aes_subbytes_inv(state);
        aes_addroundkey(state, &expanded_key[round * 16]);
        aes_mixcolumns_inv(state);
    }

    aes_shiftrows_inv(state);
    aes_subbytes_inv(state);
    aes_addroundkey(state, &expanded_key[0]);

    memcpy(data, state, 16);
}

static void reverse_bits_in_bytes(uint8_t* data, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        uint8_t step1 = ((byte & 0x55) << 1) | ((byte >> 1) & 0x55);
        uint8_t step2 = ((step1 & 0x33) << 2) | ((step1 >> 2) & 0x33);
        data[i] = ((step2 & 0x0F) << 4) | (step2 >> 4);
    }
}

/* ------------------------------------------------------------------------------------------- *
 * 2) Protokoll-Ebene -- Transkription aus beninca_arc.c (ohne Furi/Keystore-Abhaengigkeit;
 *    mfkey wird direkt uebergeben statt aus SubGhzKeystore gelesen zu werden -- entspricht
 *    exakt dem, was get_subghz_protocol_beninca_arc_aes_key() aus dem Keystore holt)
 * ------------------------------------------------------------------------------------------- */

static void beninca_arc_derive_aes_key(uint64_t mfkey, uint8_t* aes_key) {
    uint32_t derived_lo = (uint32_t)(mfkey & 0xFFFFFFFF);
    uint32_t derived_hi = (uint32_t)((mfkey >> 32) & 0xFFFFFFFF);

    uint64_t val64_a = ((uint64_t)derived_hi << 32) | derived_lo;
    for(uint8_t i = 0; i < 8; i++) {
        aes_key[i] = (val64_a >> (56 - i * 8)) & 0xFF;
    }

    uint32_t new_lo = ((derived_hi >> 24) & 0xFF) | ((derived_hi >> 8) & 0xFF00) |
                      ((derived_hi << 8) & 0xFF0000) | ((derived_hi << 24) & 0xFF000000);
    uint32_t new_hi = ((derived_lo >> 24) & 0xFF) | ((derived_lo >> 8) & 0xFF00) |
                      ((derived_lo << 8) & 0xFF0000) | ((derived_lo << 24) & 0xFF000000);

    uint64_t val64_b = ((uint64_t)new_hi << 32) | new_lo;
    for(uint8_t i = 0; i < 8; i++) {
        aes_key[i + 8] = (val64_b >> (56 - i * 8)) & 0xFF;
    }
}

typedef struct {
    uint32_t serial;
    uint8_t btn;
    uint32_t cnt;
    uint32_t seed;
    uint64_t data; /* generic->data -- erste 64 Bit des Funkrahmens */
    uint64_t data_2; /* generic->data_2 -- zweite 64 Bit */
} GenericBlock;

/* 1:1 aus subghz_protocol_beninca_arc_encrypt() */
static void beninca_arc_encrypt(GenericBlock* g, uint64_t mfkey, uint64_t middle_bytes) {
    uint8_t plaintext[16];

    plaintext[0] = (g->serial >> 24) & 0xFF;
    plaintext[1] = (g->serial >> 16) & 0xFF;
    plaintext[2] = (g->serial >> 8) & 0xFF;
    plaintext[3] = g->serial & 0xFF;
    plaintext[4] = g->btn;
    plaintext[5] = (middle_bytes >> 32) & 0xFF;
    plaintext[6] = (middle_bytes >> 24) & 0xFF;
    plaintext[7] = (middle_bytes >> 16) & 0xFF;
    plaintext[8] = (middle_bytes >> 8) & 0xFF;
    plaintext[9] = middle_bytes & 0xFF;
    plaintext[10] = (g->cnt >> 24) & 0xFF;
    plaintext[11] = (g->cnt >> 16) & 0xFF;
    plaintext[12] = (g->cnt >> 8) & 0xFF;
    plaintext[13] = g->cnt & 0xFF;
    plaintext[14] = (g->seed >> 8) & 0xFF;
    plaintext[15] = g->seed & 0xFF;

    uint8_t aes_key[16];
    beninca_arc_derive_aes_key(mfkey, aes_key);

    uint8_t expanded_key[176];
    aes_key_expansion(aes_key, expanded_key);

    aes128_encrypt(expanded_key, plaintext);

    reverse_bits_in_bytes(plaintext, 16);

    g->data = 0;
    g->data_2 = 0;
    for(uint8_t i = 0; i < 8; i++) {
        g->data = (g->data << 8) | plaintext[i];
        g->data_2 = (g->data_2 << 8) | plaintext[i + 8];
    }
}

/* 1:1 aus subghz_protocol_beninca_arc_decrypt() */
static uint64_t beninca_arc_decrypt(GenericBlock* g, uint64_t mfkey) {
    uint8_t encrypted_data[16];

    for(uint8_t i = 0; i < 8; i++) {
        encrypted_data[i] = (g->data >> (56 - i * 8)) & 0xFF;
        encrypted_data[i + 8] = (g->data_2 >> (56 - i * 8)) & 0xFF;
    }

    reverse_bits_in_bytes(encrypted_data, 16);

    uint8_t aes_key[16];
    beninca_arc_derive_aes_key(mfkey, aes_key);

    uint8_t expanded_key[176];
    aes_key_expansion(aes_key, expanded_key);

    aes128_decrypt(expanded_key, encrypted_data);

    g->serial = ((uint32_t)encrypted_data[0] << 24) | ((uint32_t)encrypted_data[1] << 16) |
                ((uint32_t)encrypted_data[2] << 8) | encrypted_data[3];
    g->btn = encrypted_data[4];

    uint64_t middle_bytes = ((uint64_t)encrypted_data[5] << 32) | ((uint64_t)encrypted_data[6] << 24) |
                            ((uint64_t)encrypted_data[7] << 16) | ((uint64_t)encrypted_data[8] << 8) |
                            encrypted_data[9];

    g->cnt = ((uint32_t)encrypted_data[10] << 24) | ((uint32_t)encrypted_data[11] << 16) |
             ((uint32_t)encrypted_data[12] << 8) | encrypted_data[13];
    g->seed = ((uint16_t)encrypted_data[14] << 8) | encrypted_data[15];

    return middle_bytes;
}

/* ------------------------------------------------------------------------------------------- *
 * 3) Decoder-Bit-Erfassungslogik aus subghz_protocol_decoder_beninca_arc_feed(),
 *    Zustand BenincaARCDecoderHighLevel -- reine Zustandsmaschine ueber einen Bitstrom,
 *    Timing/GAP-Erkennung ausgeklammert (dort liegt kein Fund, siehe Doku).
 *    decode_count_bit/decode_data entsprechen 1:1 SubGhzBlockDecoder-Feldern,
 *    subghz_protocol_blocks_add_bit() ist decode_data = decode_data<<1|bit; decode_count_bit++;
 * ------------------------------------------------------------------------------------------- */

#define MIN_COUNT_BIT_FOR_FOUND 128

typedef struct {
    uint64_t decode_data;
    uint8_t decode_count_bit;
    uint64_t data; /* generic.data nach Erfassung der ersten Haelfte */
    uint64_t data_2; /* generic.data_2 nach Erfassung der zweiten Haelfte */
    bool full_captured;
} DecoderStateAlt;

/* ALT: wortgetreue Nachbildung der realen Bedingung in beninca_arc.c
 *   if ((decode_count_bit == min_count_bit_for_found/2) && (decode_data != 0)) { ... }
 *   else if (decode_count_bit == min_count_bit_for_found) { ... }
 */
static void decoder_alt_add_bit(DecoderStateAlt* s, uint8_t bit) {
    s->decode_data = (s->decode_data << 1) | bit;
    s->decode_count_bit++;

    if((s->decode_count_bit == (MIN_COUNT_BIT_FOR_FOUND / 2)) && (s->decode_data != 0)) {
        s->data = s->decode_data;
        s->decode_data = 0;
    } else if(s->decode_count_bit == MIN_COUNT_BIT_FOR_FOUND) {
        s->data_2 = s->decode_data;
        s->full_captured = true;
    }
}

typedef struct {
    uint64_t decode_data;
    uint8_t decode_count_bit;
    uint64_t data;
    uint64_t data_2;
    bool half_captured; /* NEU: explizites Flag statt "decode_data != 0" */
    bool full_captured;
} DecoderStateFixed;

/* NEU: vorgeschlagener Fix -- Haelftenerfassung haengt nur noch am Bitzaehler,
 * nicht mehr am Inhalt von decode_data. */
static void decoder_fixed_add_bit(DecoderStateFixed* s, uint8_t bit) {
    s->decode_data = (s->decode_data << 1) | bit;
    s->decode_count_bit++;

    if(s->decode_count_bit == (MIN_COUNT_BIT_FOR_FOUND / 2)) {
        s->data = s->decode_data;
        s->decode_data = 0;
        s->half_captured = true;
    } else if(s->decode_count_bit == MIN_COUNT_BIT_FOR_FOUND) {
        s->data_2 = s->decode_data;
        s->full_captured = true;
    }
}

static void feed_bits_alt(DecoderStateAlt* s, const uint64_t data128_hi, const uint64_t data128_lo) {
    for(int i = 63; i >= 0; i--) decoder_alt_add_bit(s, (uint8_t)((data128_hi >> i) & 1));
    for(int i = 63; i >= 0; i--) decoder_alt_add_bit(s, (uint8_t)((data128_lo >> i) & 1));
}

static void
    feed_bits_fixed(DecoderStateFixed* s, const uint64_t data128_hi, const uint64_t data128_lo) {
    for(int i = 63; i >= 0; i--) decoder_fixed_add_bit(s, (uint8_t)((data128_hi >> i) & 1));
    for(int i = 63; i >= 0; i--) decoder_fixed_add_bit(s, (uint8_t)((data128_lo >> i) & 1));
}

/* ------------------------------------------------------------------------------------------- *
 * Test-Infrastruktur
 * ------------------------------------------------------------------------------------------- */

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        g_checks++;                                                              \
        if(!(cond)) {                                                           \
            g_failed++;                                                         \
            fprintf(stderr, "  CHECK FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        }                                                                        \
    } while(0)

static uint64_t xorshift_state = 0x9E3779B97F4A7C15ULL;
static uint64_t xorshift64(void) {
    xorshift_state ^= xorshift_state << 13;
    xorshift_state ^= xorshift_state >> 7;
    xorshift_state ^= xorshift_state << 17;
    return xorshift_state;
}

int main(void) {
    printf("== Test 1: AES-128 gegen den offiziellen FIPS-197-Testvektor ==\n");
    {
        /* FIPS-197 Appendix B / C.1: Key = 000102030405060708090a0b0c0d0e0f,
         * Plaintext = 00112233445566778899aabbccddeeff,
         * Ciphertext = 69c4e0d86a7b0430d8cdb78070b4c55a */
        uint8_t key[16];
        for(int i = 0; i < 16; i++) key[i] = (uint8_t)i;
        static const uint8_t block_init[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        uint8_t block[16];
        memcpy(block, block_init, 16);
        static const uint8_t expected_ct[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
                                                 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};

        uint8_t expanded[176];
        aes_key_expansion(key, expanded);

        uint8_t enc[16];
        memcpy(enc, block, 16);
        aes128_encrypt(expanded, enc);
        CHECK(memcmp(enc, expected_ct, 16) == 0);
        printf("  Verschluesselung stimmt mit FIPS-197-Vektor ueberein: %s\n",
               memcmp(enc, expected_ct, 16) == 0 ? "ja" : "NEIN");

        uint8_t dec[16];
        memcpy(dec, enc, 16);
        aes128_decrypt(expanded, dec);
        CHECK(memcmp(dec, block, 16) == 0);
        printf("  Entschluesselung liefert wieder den Klartext: %s\n",
               memcmp(dec, block, 16) == 0 ? "ja" : "NEIN");
    }

    printf("== Test 2: AES-128 Rundlauf ueber 20000 zufaellige (Key, Block)-Paare ==\n");
    {
        int ok = 0;
        for(int t = 0; t < 20000; t++) {
            uint8_t key[16], block[16], orig[16];
            for(int i = 0; i < 16; i++) key[i] = (uint8_t)(xorshift64() & 0xFF);
            for(int i = 0; i < 16; i++) {
                block[i] = (uint8_t)(xorshift64() & 0xFF);
                orig[i] = block[i];
            }
            uint8_t expanded[176];
            aes_key_expansion(key, expanded);
            aes128_encrypt(expanded, block);
            CHECK(memcmp(block, orig, 16) != 0 || t == 0); /* Ciphertext != Klartext (fast immer) */
            aes128_decrypt(expanded, block);
            if(memcmp(block, orig, 16) == 0) ok++;
        }
        CHECK(ok == 20000);
        printf("  %d/20000 Rundlaeufe liefern exakt den Original-Block zurueck\n", ok);
    }

    printf("== Test 3: Beninca-ARC Encrypt/Decrypt-Rundlauf (Protokollfelder) ==\n");
    {
        int ok = 0;
        const int N = 50000;
        for(int t = 0; t < N; t++) {
            GenericBlock g = {0};
            g.serial = (uint32_t)xorshift64();
            g.btn = (uint8_t)(xorshift64() & 0xFF);
            g.cnt = (uint32_t)xorshift64();
            g.seed = 0xAA55;
            uint64_t mfkey = xorshift64();
            uint64_t middle = xorshift64() & 0xFFFFFFFFFFULL; /* 40 Bit */

            uint32_t orig_serial = g.serial;
            uint8_t orig_btn = g.btn;
            uint32_t orig_cnt = g.cnt;
            uint32_t orig_seed = g.seed;

            beninca_arc_encrypt(&g, mfkey, middle);
            uint64_t middle_dec = beninca_arc_decrypt(&g, mfkey);

            bool match = (g.serial == orig_serial) && (g.btn == orig_btn) &&
                         (g.cnt == orig_cnt) && (g.seed == orig_seed) && (middle_dec == middle);
            if(match) ok++;
        }
        CHECK(ok == N);
        printf("  %d/%d Encrypt->Decrypt-Rundlaeufe liefern Serial/Btn/Cnt/Seed/Mc exakt zurueck\n",
               ok,
               N);
    }

    printf("== Test 4: Wie oft sind die ersten 64 Bit echten Chiffretexts tatsaechlich 0? ==\n");
    {
        int zero_count = 0;
        const int N = 200000;
        for(int t = 0; t < N; t++) {
            GenericBlock g = {0};
            g.serial = (uint32_t)xorshift64();
            g.btn = (uint8_t)(xorshift64() & 0xFF);
            g.cnt = (uint32_t)xorshift64();
            g.seed = 0xAA55;
            uint64_t mfkey = xorshift64();
            uint64_t middle = xorshift64() & 0xFFFFFFFFFFULL;
            beninca_arc_encrypt(&g, mfkey, middle);
            if(g.data == 0) zero_count++;
        }
        CHECK(zero_count == 0);
        printf("  %d/%d echte Verschluesselungen haben data==0 (erste Rahmenhaelfte) -- "
               "erwartungsgemaess praktisch nie (Wahrscheinlichkeit ca. 1/2^64)\n",
               zero_count,
               N);
    }

    printf("== Test 5: Fund -- ALT verliert die erste Rahmenhaelfte, wenn deren 64 Bit "
           "zufaellig komplett 0 sind ==\n");
    {
        /* Synthetischer, pathologischer Fall: erste Haelfte = 0, zweite Haelfte != 0.
         * Bei realem AES-Chiffretext laut Test 4 praktisch nie der Fall -- hier gezielt
         * konstruiert, um die Logikluecke nachzuweisen, nicht um einen realistischen
         * Funkempfang zu behaupten. */
        DecoderStateAlt alt = {0};
        alt.data = 0xDEADBEEFDEADBEEFULL; /* Alt-Wert von einem fruiheren, abgeschlossenen Paket */
        feed_bits_alt(&alt, 0x0000000000000000ULL, 0x1122334455667788ULL);

        CHECK(alt.full_captured == true); /* Full-Capture-Callback wird trotzdem ausgeloest */
        CHECK(alt.data_2 == 0x1122334455667788ULL); /* zweite Haelfte korrekt */
        CHECK(alt.data == 0xDEADBEEFDEADBEEFULL); /* ERSTE Haelfte: stiller, alter Wert -- NICHT 0 */
        printf("  ALT: generic.data_2 = %016llx (korrekt), generic.data blieb bei %016llx "
               "(Altwert eines fruiheren Pakets statt 0)\n",
               (unsigned long long)alt.data_2,
               (unsigned long long)alt.data);

        DecoderStateFixed fixed = {0};
        fixed.data = 0xDEADBEEFDEADBEEFULL;
        feed_bits_fixed(&fixed, 0x0000000000000000ULL, 0x1122334455667788ULL);

        CHECK(fixed.full_captured == true);
        CHECK(fixed.half_captured == true);
        CHECK(fixed.data == 0x0000000000000000ULL); /* NEU: korrekt als 0 erfasst */
        CHECK(fixed.data_2 == 0x1122334455667788ULL);
        printf("  NEU:  generic.data   = %016llx (korrekt als 0 erfasst, half_captured=%d)\n",
               (unsigned long long)fixed.data,
               fixed.half_captured);
    }

    printf("== Test 6: Regression -- normale (nicht-pathologische) 128-Bit-Rahmen verhalten "
           "sich bei ALT und NEU identisch ==\n");
    {
        int identical = 0;
        const int N = 20000;
        for(int t = 0; t < N; t++) {
            uint64_t hi = xorshift64();
            uint64_t lo = xorshift64();
            if(hi == 0) hi = 1; /* den pathologischen Fall hier bewusst ausschliessen */

            DecoderStateAlt alt = {0};
            feed_bits_alt(&alt, hi, lo);

            DecoderStateFixed fixed = {0};
            feed_bits_fixed(&fixed, hi, lo);

            bool same = (alt.data == fixed.data) && (alt.data_2 == fixed.data_2) &&
                        (alt.full_captured == fixed.full_captured) && (alt.data == hi) &&
                        (alt.data_2 == lo);
            if(same) identical++;
        }
        CHECK(identical == N);
        printf("  %d/%d zufaellige Rahmen: ALT und NEU liefern identisches generic.data/data_2 "
               "(Fix aendert das Normalverhalten nicht)\n",
               identical,
               N);
    }

    printf("\n%d/%d Checks bestanden -- %s\n",
           g_checks - g_failed,
           g_checks,
           g_failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return g_failed == 0 ? 0 : 1;
}
