/*
 * Host repro for the Nice O-Code encoder integration proposed in
 * firmware-mining/nice-o-code-encoder-port.md (Phase 3 of the Port-Skizze in
 * firmware-mining/nice-o-code-subghz-port.md: encoder struct fields + .sub "IC" field on
 * the encoder side + per-parcel masked re-encryption in get_upload(), encoder only, no
 * change to feed()/the decoder -- those are firmware-mining/nice-o-code-decode-display-port.md,
 * already proposed and verified separately in firmware-mining/repro-nice-o-code-decode-display/repro.c).
 *
 * The cipher/mask/parcel primitives below are the same transcription already verified in
 * firmware-mining/repro-nice-o-code-subghz/repro.c and reused unmodified in
 * firmware-mining/repro-nice-o-code-decode-display/repro.c. This file adds a third,
 * independent layer: a minimal EncoderState mirroring exactly the new ic/o_code fields
 * and the new get_upload()/deserialize() logic added to SubGhzProtocolEncoderNiceFlorS,
 * to verify the *integration point* itself -- not just the cipher primitives it calls --
 * and, critically, that a file produced by this encoder is actually decodable by the
 * already-verified decoder logic for every one of the 16 transmitted parcels.
 *
 * Scenario 5 below closes the "Nice One (72 Bit) + O-Code kombiniert" item left open under
 * nice-o-code-encoder-port.md's "Offen": it transcribes the unmodified Nice One 72-bit tail
 * computation (get_bit_array()/nice_one_crc()/nice_one_get_data(), lib/subghz/protocols/
 * nice_flor_s.c, mntm-012) and exercises it together with this patch's per-parcel o_code
 * re-encryption, since get_upload()'s real Nice One branch runs on the same per-parcel 52-bit
 * body this patch already varies.
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

/* ---- Nice One (72-bit) tail, transcribed 1:1 from the unmodified nice_flor_s.c (mntm-012):
 * subghz_protocol_blocks_get_bit_array() (lib/subghz/blocks/encoder.c), subghz_protocol_
 * nice_one_crc() and subghz_protocol_nice_one_get_data(). Untouched by this patch -- the
 * encoder's existing NICE_ONE_COUNT_BIT branch in get_upload() runs on a *copy* of the
 * per-parcel 52-bit body (header byte + enc_part) this patch already varies for o_code, so
 * this is exercised here to close the "Nice One + O-Code kombiniert" open item. ---- */

static bool get_bit_array(const uint8_t* p, size_t idx) {
    return (bool)((p[idx >> 3] >> (7 - (idx & 7))) & 1);
}

static uint8_t nice_one_crc(const uint8_t* p) {
    uint8_t crc = 0;
    uint8_t crc_data = 0xff;
    for(uint8_t i = 4; i < 68; i++) {
        if(get_bit_array(p, i)) {
            crc = crc_data ^ 1;
        } else {
            crc = crc_data;
        }
        crc_data >>= 1;
        if(crc & 0x01) {
            crc_data ^= 0x97;
        }
    }
    crc = 0;
    for(uint8_t i = 0; i < 8; i++) {
        crc <<= 1;
        if((crc_data >> i) & 0x01) crc |= 1;
    }
    return crc;
}

/* p is a 10-byte array; p[0..6] must already hold the parcel's 52-bit on-air body
 * (big-endian, same layout as the existing "data" field), p[7..9] are scratch/output. */
static void nice_one_get_data(uint8_t* p, uint8_t num_parcel, uint8_t hold_bit) {
    uint8_t k = (uint8_t)((0x0f ^ (p[0] & 0x0Fu) ^ num_parcel) << 4);
    p[1] = (uint8_t)((p[1] & 0x0f) | k);
    k = (uint8_t)((num_parcel < 4) ? 0x8f : 0x80);
    hold_bit = (uint8_t)(hold_bit ? 0x10 : 0);
    k = (uint8_t)(num_parcel ^ k);
    p[7] = k;
    p[8] = (uint8_t)(hold_bit ^ (uint8_t)(k << 4));

    uint8_t crc = nice_one_crc(p);
    p[8] = (uint8_t)(p[8] | (crc >> 4));
    p[9] = (uint8_t)(crc << 4);
}

/* 1:1 with get_upload()'s "if(data_count_bit == NICE_ONE_COUNT_BIT)" branch: add_data[0..6]
 * is a *copy* of the already-produced 52-bit body, never the body itself -- the real caller
 * passes loops[i] for both num_parcel and hold_bit. Returns the 24-bit add_data[7..9], of
 * which the real encoder transmits only the top 20 bits (bits 23..4). */
static uint32_t nice_one_tail_for_parcel(uint64_t body52, uint8_t parcel_index) {
    uint8_t add_data[10] = {0};
    for(size_t i = 0; i < 7; i++) {
        add_data[i] = (uint8_t)((body52 >> (48 - i * 8)) & 0xFF);
    }
    nice_one_get_data(add_data, parcel_index, parcel_index);
    uint32_t data_2 = 0;
    for(size_t j = 7; j < 10; j++) {
        data_2 = (data_2 << 8) + add_data[j];
    }
    return data_2;
}

/* 1:1 with the decode-side dispatch already proposed/verified in
 * firmware-mining/nice-o-code-decode-display-port.md / repro-nice-o-code-decode-display.
 * Reused here unmodified, as the receiving end against which this encoder's output is
 * checked. */
static void get_string_decode(
    uint64_t data,
    bool o_code,
    uint16_t ic,
    const uint8_t* table,
    uint32_t* out_serial,
    uint16_t* out_cnt,
    uint8_t* out_btn) {
    if(o_code) {
        uint8_t n = nice_o_get_parcel(data);
        uint32_t mask_32;
        uint16_t mask_16;
        nice_o_mask(n, &mask_32, &mask_16);
        uint64_t decrypted =
            neu_decrypt_ic(data & 0xFFFFFFFFFFFULL, (uint16_t)(ic ^ mask_16), table);
        *out_cnt = (uint16_t)decrypted;
        *out_serial = (uint32_t)((decrypted >> 16) ^ mask_32) & 0x0FFFFFFFu;
        *out_btn = (uint8_t)((data >> 48) & 0xF);
    } else {
        uint64_t decrypt = alt_decrypt(data, table);
        *out_cnt = (uint16_t)(decrypt & 0xFFFF);
        *out_serial = (uint32_t)((decrypt >> 16) & 0xFFFFFFFu);
        *out_btn = (uint8_t)((decrypt >> 48) & 0xF);
    }
}

/* ---- transcription of the proposed encoder integration (Phase 3 patch) ---- */

typedef struct {
    uint64_t data; /* instance->generic.data, loaded "Key" field */
    uint32_t serial; /* instance->generic.serial */
    uint16_t cnt; /* instance->generic.cnt */
    uint8_t btn; /* instance->generic.btn */
    uint16_t ic; /* instance->ic (new) */
    bool o_code; /* instance->o_code (new) */
} EncoderState;

/* 1:1 with subghz_protocol_encoder_nice_flor_s_alloc()'s new two lines */
static void encoder_alloc(EncoderState* st) {
    memset(st, 0, sizeof(*st));
    st->ic = NICE_FLOR_S_IC_PLAIN;
    st->o_code = false;
}

/* subghz_protocol_nice_flor_s_remote_controller()'s plain branch (file_name != NULL),
 * exactly as already transcribed in repro-nice-o-code-decode-display/repro.c. */
static void alt_remote_controller(EncoderState* st, const uint8_t* table) {
    uint64_t decrypt = alt_decrypt(st->data, table);
    st->cnt = (uint16_t)(decrypt & 0xFFFF);
    st->serial = (uint32_t)((decrypt >> 16) & 0xFFFFFFFu);
    st->btn = (uint8_t)((decrypt >> 48) & 0xF);
}

/* 1:1 with the new "IC" hex field read plus the new o_code/plain branch added to
 * subghz_protocol_encoder_nice_flor_s_deserialize(), run after instance->generic.data has
 * already been populated from the file's "Key" field (subghz_block_generic_deserialize(),
 * unmodified by this patch). ic_field_present/ic_field_value stand in for
 * flipper_format_read_hex()'s return value and parsed bytes. */
static void encoder_deserialize(
    EncoderState* st,
    bool ic_field_present,
    uint16_t ic_field_value,
    const uint8_t* table) {
    if(ic_field_present) {
        st->ic = ic_field_value;
        st->o_code = true;
    } else {
        st->ic = NICE_FLOR_S_IC_PLAIN;
        st->o_code = false;
    }

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

/* 1:1 with the new per-parcel branch added inside
 * subghz_protocol_encoder_nice_flor_s_get_upload()'s existing for(i=0..15) loop. Returns
 * the full on-air "data" field (P0/P1 header byte plus 44-bit body) for each of the 16
 * parcels a button press transmits -- everything get_upload() itself derives from this
 * value (timing/levels) is unchanged by this patch and out of scope here, exactly as in
 * the decoder repro above. */
static void encoder_get_upload_frames(
    const EncoderState* st,
    uint8_t btn,
    const uint8_t* table,
    uint64_t out_frames[16]) {
    uint64_t decrypt = ((uint64_t)st->serial << 16) | st->cnt;
    uint64_t enc_part = alt_encrypt(decrypt, table);

    for(int i = 0; i < 16; i++) {
        uint64_t loop_n = (uint64_t)i;

        if(st->o_code) {
            uint32_t mask_32 = 0;
            uint16_t mask_16 = 0;
            nice_o_mask((uint8_t)loop_n, &mask_32, &mask_16);
            uint64_t plain = ((uint64_t)((st->serial ^ mask_32) & 0x0FFFFFFFu) << 16) | st->cnt;
            enc_part = neu_encrypt_ic(plain, (uint16_t)(st->ic ^ mask_16), table);
        }

        uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ loop_n));
        out_frames[i] = ((uint64_t)byte << 44) | (enc_part & 0xFFFFFFFFFFFULL);
    }
}

/* Builds the full on-air "data" field for one plain Flor-S parcel, as a real capture (or
 * subghz_protocol_nice_flor_s_create_data()) would produce it -- used here only to build
 * the "Key" field a .sub file would carry, i.e. encoder_deserialize()'s input. */
static uint64_t build_plain_frame(
    uint32_t serial,
    uint16_t cnt,
    uint8_t btn,
    uint8_t n,
    const uint8_t* table) {
    uint64_t body = alt_encrypt(((uint64_t)serial << 16) | cnt, table);
    uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ n));
    return ((uint64_t)byte << 44) | (body & 0xFFFFFFFFFFFULL);
}

/* Builds the full on-air "data" field for one O-Code parcel, per the pipeline already
 * verified in firmware-mining/repro-nice-o-code-subghz/repro.c step 6 -- used here only to
 * build the "Key" field a hand-edited O-Code .sub file would carry (one real captured
 * parcel), i.e. encoder_deserialize()'s input. */
static uint64_t build_o_code_frame(
    uint32_t serial,
    uint16_t cnt,
    uint8_t btn,
    uint16_t ic,
    uint8_t n,
    const uint8_t* table) {
    uint32_t mask_32;
    uint16_t mask_16;
    nice_o_mask(n, &mask_32, &mask_16);
    uint64_t plain = (((uint64_t)(serial ^ mask_32) & 0x0FFFFFFFu) << 16) | cnt;
    uint64_t enc_body = neu_encrypt_ic(plain, (uint16_t)(ic ^ mask_16), table);
    uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ n));
    return ((uint64_t)byte << 44) | (enc_body & 0xFFFFFFFFFFFULL);
}

/* ---- test harness ---- */

static uint64_t g_checks = 0;
static uint64_t g_failures = 0;

#define CHECK(cond, msg, ...)                                                           \
    do {                                                                                \
        g_checks++;                                                                     \
        if(!(cond)) {                                                                   \
            g_failures++;                                                               \
            fprintf(stderr, "FAIL %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                                \
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
    uint64_t rng = 0xB16B00B5ULL;

    /* --- 1. Backward-compat regression: o_code == false (every pre-existing .sub file)
     *        must leave get_upload()'s per-parcel branch a no-op -- enc_part computed once
     *        before the loop is reused unchanged on every iteration, byte for byte
     *        identical to the unmodified encoder, and deserialize() must still recover
     *        (serial, cnt, btn) via the untouched plain remote_controller() path. --- */
    for(int t = 0; t < 2000; t++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint8_t key_n = (uint8_t)(xorshift64(&rng) & 0x0F);

        EncoderState st;
        encoder_alloc(&st);
        st.data = build_plain_frame(serial, cnt, btn, key_n, g_table);
        encoder_deserialize(&st, false, 0, g_table); /* no "IC" field on this file */
        CHECK(!st.o_code, "regression: deserialize with absent IC must leave o_code false");
        CHECK(st.serial == serial && st.cnt == cnt && st.btn == btn,
              "regression: deserialize must recover the frame it was built from");

        uint64_t frames[16];
        encoder_get_upload_frames(&st, btn, g_table, frames);

        uint64_t expect_enc_part = alt_encrypt(((uint64_t)serial << 16) | cnt, g_table);
        for(int i = 0; i < 16; i++) {
            uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ (uint8_t)i));
            uint64_t want = ((uint64_t)byte << 44) | (expect_enc_part & 0xFFFFFFFFFFFULL);
            CHECK(frames[i] == want,
                  "regression: frame[%d]=0x%013llX diverges from unmodified encoder "
                  "want=0x%013llX",
                  i, (unsigned long long)frames[i], (unsigned long long)want);
        }
        /* same ciphertext body repeated 16 times, exactly like the unmodified encoder */
        for(int i = 1; i < 16; i++) {
            CHECK((frames[i] & 0xFFFFFFFFFFFULL) == (frames[0] & 0xFFFFFFFFFFFULL),
                  "regression: plain mode must repeat one ciphertext across all 16 parcels");
        }
    }
    fprintf(
        stderr,
        "[1/5] backward-compat regression: %llu checks so far, %llu failures\n",
        (unsigned long long)g_checks,
        (unsigned long long)g_failures);

    /* --- 2. O-Code end-to-end: load a file holding one real captured parcel ("Key" +
     *        "IC"), recover (serial, cnt, btn) via deserialize(), generate the 16 transmit
     *        frames via get_upload(), then decode *each* of those 16 frames with the
     *        already-verified decoder logic (repro-nice-o-code-decode-display) and confirm
     *        every one independently reconstructs the original (serial, cnt, btn) -- the
     *        actual Send -> live Receive round trip this patch exists to enable. --- */
    for(int i = 0; i < 200000; i++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint16_t ic = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t key_n = (uint8_t)(xorshift64(&rng) & 0x0F);

        EncoderState st;
        encoder_alloc(&st);
        st.data = build_o_code_frame(serial, cnt, btn, ic, key_n, g_table);
        encoder_deserialize(&st, true, ic, g_table);
        CHECK(st.o_code, "e2e: deserialize with present IC must set o_code true");
        CHECK(st.serial == serial && st.cnt == cnt && st.btn == btn,
              "e2e: deserialize must recover the captured parcel's (serial,cnt,btn)");

        uint64_t frames[16];
        encoder_get_upload_frames(&st, btn, g_table, frames);

        bool saw_difference = false;
        for(int n = 0; n < 16; n++) {
            uint32_t got_serial;
            uint16_t got_cnt;
            uint8_t got_btn;
            get_string_decode(
                frames[n], true, ic, g_table, &got_serial, &got_cnt, &got_btn);
            CHECK(got_serial == serial,
                  "e2e: parcel %d serial mismatch got=0x%x want=0x%x", n, got_serial, serial);
            CHECK(got_cnt == cnt, "e2e: parcel %d cnt mismatch got=0x%x want=0x%x", n, got_cnt,
                  cnt);
            CHECK(got_btn == btn, "e2e: parcel %d btn mismatch got=0x%x want=0x%x", n, got_btn,
                  btn);
            if(n > 0 && (frames[n] & 0xFFFFFFFFFFFULL) != (frames[0] & 0xFFFFFFFFFFFULL)) {
                saw_difference = true;
            }

            /* decoding the same frame under a wrong ic must not also recover the truth */
            if(ic != 0xFFFF) {
                uint32_t wrong_serial;
                uint16_t wrong_cnt;
                uint8_t wrong_btn;
                get_string_decode(
                    frames[n],
                    true,
                    (uint16_t)(ic + 1),
                    g_table,
                    &wrong_serial,
                    &wrong_cnt,
                    &wrong_btn);
                CHECK(wrong_serial != serial || wrong_cnt != cnt,
                      "e2e: parcel %d wrong ic=0x%x still recovered the true (serial,cnt)", n,
                      (unsigned)(ic + 1));
            }
        }
        CHECK(saw_difference,
              "e2e: O-Code mode must vary ciphertext across the 16 parcels, unlike plain mode");
    }
    fprintf(
        stderr,
        "[2/5] O-Code Send->Receive round trip over all 16 parcels: %llu checks so far, "
        "%llu failures\n",
        (unsigned long long)g_checks,
        (unsigned long long)g_failures);

    /* --- 3. .sub "IC" field presence/absence symmetry on the encoder side, mirroring the
     *        decoder's own already-verified symmetry check. --- */
    for(int t = 0; t < 1000; t++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);

        EncoderState st;
        encoder_alloc(&st);
        st.data = build_plain_frame(serial, cnt, btn, 0, g_table);
        encoder_deserialize(&st, false, 0xDEAD, g_table);
        CHECK(!st.o_code, "round trip: absent IC must leave o_code false regardless of the "
                           "(unused) parsed value");
        CHECK(st.ic == NICE_FLOR_S_IC_PLAIN, "round trip: absent IC must leave ic == PLAIN");
    }
    fprintf(
        stderr,
        "[3/5] absent-IC round trip: %llu checks so far, %llu failures\n",
        (unsigned long long)g_checks,
        (unsigned long long)g_failures);

    /* --- 4. Fixed worked example, printed for manual cross-check against a real .sub file
     *        edited by hand (IC field in hex, big-endian, same convention as the existing
     *        "Key" field in that file). --- */
    {
        fill_random_table(g_table, &rng);
        uint32_t serial = 0x0ABCDEFu & 0x0FFFFFFFu;
        uint16_t cnt = 0x1234;
        uint8_t btn = 0x2;
        uint16_t ic = 0x55AA;
        uint8_t key_n = 5;

        EncoderState st;
        encoder_alloc(&st);
        st.data = build_o_code_frame(serial, cnt, btn, ic, key_n, g_table);
        encoder_deserialize(&st, true, ic, g_table);

        uint64_t frames[16];
        encoder_get_upload_frames(&st, btn, g_table, frames);

        uint32_t got_serial;
        uint16_t got_cnt;
        uint8_t got_btn;
        get_string_decode(frames[9], true, ic, g_table, &got_serial, &got_cnt, &got_btn);

        fprintf(
            stderr,
            "[4/5] worked example: IC=%04X Key-frame=0x%013llX -> TX parcel[9]=0x%013llX -> "
            "decoded Sn:%05lX Cnt:%04X Btn:%02X (want Sn:%05lX Cnt:%04X Btn:%02X)\n",
            ic,
            (unsigned long long)st.data,
            (unsigned long long)frames[9],
            (unsigned long)got_serial,
            got_cnt,
            got_btn,
            (unsigned long)serial,
            cnt,
            btn);
        CHECK(got_serial == serial && got_cnt == cnt && got_btn == btn,
              "worked example mismatch");
    }

    /* --- 5. Nice One (72 Bit) + O-Code kombiniert, the "Offen" item from the previous
     *        session: the real NICE_ONE_COUNT_BIT branch in get_upload() runs on a *copy*
     *        of the per-parcel 52-bit body this patch already varies for o_code, never on
     *        the body itself -- verify both halves independently. --- */
    for(int t = 0; t < 50000; t++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint16_t ic = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t key_n = (uint8_t)(xorshift64(&rng) & 0x0F);

        EncoderState st;
        encoder_alloc(&st);
        st.data = build_o_code_frame(serial, cnt, btn, ic, key_n, g_table);
        encoder_deserialize(&st, true, ic, g_table);

        uint64_t frames[16];
        encoder_get_upload_frames(&st, btn, g_table, frames);

        uint32_t tails[16];
        for(int n = 0; n < 16; n++) {
            tails[n] = nice_one_tail_for_parcel(frames[n], (uint8_t)n);
        }

        bool saw_tail_difference = false;
        for(int n = 0; n < 16; n++) {
            /* 5a. the Nice One tail is computed from a *copy* of the 52-bit body -- the
             *     body itself, and therefore its O-Code decode, must be byte-identical to
             *     the already-verified o_code-only case (scenario 2), independent of
             *     whether a Nice One tail was additionally computed for the same parcel. */
            uint32_t got_serial;
            uint16_t got_cnt;
            uint8_t got_btn;
            get_string_decode(
                frames[n], true, ic, g_table, &got_serial, &got_cnt, &got_btn);
            CHECK(got_serial == serial && got_cnt == cnt && got_btn == btn,
                  "nice-one+o-code: parcel %d body decode mismatch", n);

            /* 5b. determinism: recomputing the tail for the same body/parcel index must
             *     reproduce the same 24-bit value. */
            CHECK(nice_one_tail_for_parcel(frames[n], (uint8_t)n) == tails[n],
                  "nice-one+o-code: parcel %d tail is not deterministic", n);

            /* 5c. independent cross-check: rebuild add_data[0..6] directly from
             *     (serial,cnt,btn,ic,n) via the mask/encrypt_ic formulas (bypassing
             *     frames[n]/encoder_get_upload_frames() entirely) and confirm the same
             *     tail results -- a second, independently-assembled path to the same
             *     52-bit body. */
            uint32_t mask_32;
            uint16_t mask_16;
            nice_o_mask((uint8_t)n, &mask_32, &mask_16);
            uint64_t plain = ((uint64_t)((serial ^ mask_32) & 0x0FFFFFFFu) << 16) | cnt;
            uint64_t enc_body = neu_encrypt_ic(plain, (uint16_t)(ic ^ mask_16), g_table);
            uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ (uint8_t)n));
            uint64_t independent_body = ((uint64_t)byte << 44) | (enc_body & 0xFFFFFFFFFFFULL);
            CHECK(independent_body == frames[n],
                  "nice-one+o-code: parcel %d body diverges from independent rebuild", n);
            CHECK(nice_one_tail_for_parcel(independent_body, (uint8_t)n) == tails[n],
                  "nice-one+o-code: parcel %d tail diverges when fed the independently "
                  "rebuilt body",
                  n);

            if(n > 0 && tails[n] != tails[0]) saw_tail_difference = true;
        }
        /* 5d. sensitivity: across 16 parcels with varying num_parcel (and, for o_code,
         *     varying body), the tail must not collapse to one constant value. */
        CHECK(saw_tail_difference,
              "nice-one+o-code: tail must vary across the 16 parcels, not stay constant");
    }
    fprintf(
        stderr,
        "[5/5] Nice One (72 Bit) + O-Code combined: %llu checks so far, %llu failures\n",
        (unsigned long long)g_checks,
        (unsigned long long)g_failures);

    /* --- 5e. Nice One + plain (o_code == false) backward-compat: same body across all 16
     *         parcels (scenario 1), but the Nice One tail must still vary per parcel via
     *         num_parcel alone, and must match an independent rebuild -- the counterpart to
     *         5a-5d for the pre-existing plain path, which this patch does not touch. --- */
    for(int t = 0; t < 2000; t++) {
        fill_random_table(g_table, &rng);
        uint32_t serial = (uint32_t)(xorshift64(&rng) & 0x0FFFFFFFu);
        uint16_t cnt = (uint16_t)(xorshift64(&rng) & 0xFFFF);
        uint8_t btn = (uint8_t)(xorshift64(&rng) & 0x0F);
        uint8_t key_n = (uint8_t)(xorshift64(&rng) & 0x0F);

        EncoderState st;
        encoder_alloc(&st);
        st.data = build_plain_frame(serial, cnt, btn, key_n, g_table);
        encoder_deserialize(&st, false, 0, g_table);

        uint64_t frames[16];
        encoder_get_upload_frames(&st, btn, g_table, frames);

        uint64_t expect_enc_part = alt_encrypt(((uint64_t)serial << 16) | cnt, g_table);
        bool saw_tail_difference = false;
        uint32_t tail0 = 0;
        for(int n = 0; n < 16; n++) {
            uint8_t byte = (uint8_t)(btn << 4 | (0xF ^ btn ^ (uint8_t)n));
            uint64_t independent_body =
                ((uint64_t)byte << 44) | (expect_enc_part & 0xFFFFFFFFFFFULL);
            CHECK(independent_body == frames[n],
                  "nice-one+plain: parcel %d body diverges from independent rebuild", n);

            uint32_t tail = nice_one_tail_for_parcel(frames[n], (uint8_t)n);
            CHECK(nice_one_tail_for_parcel(independent_body, (uint8_t)n) == tail,
                  "nice-one+plain: parcel %d tail diverges when fed the independently "
                  "rebuilt body",
                  n);
            if(n == 0) {
                tail0 = tail;
            } else if(tail != tail0) {
                saw_tail_difference = true;
            }
        }
        CHECK(saw_tail_difference,
              "nice-one+plain: tail must still vary across parcels via num_parcel alone");
    }
    fprintf(
        stderr,
        "[5/5 cont'd] Nice One (72 Bit) + plain backward-compat: %llu checks so far, %llu "
        "failures\n",
        (unsigned long long)g_checks,
        (unsigned long long)g_failures);

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED (%llu checks)\n", (unsigned long long)g_checks);
        return 0;
    } else {
        printf("%llu of %llu CHECKS FAILED\n", (unsigned long long)g_failures,
               (unsigned long long)g_checks);
        return 1;
    }
}
