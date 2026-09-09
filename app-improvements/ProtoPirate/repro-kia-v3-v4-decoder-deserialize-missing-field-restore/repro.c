/* Isolated host repro for the ProtoPirate Kia V3/V4 decoder deserialize()
 * missing-field-restore bug (see ../kia-v3-v4-decoder-deserialize-missing-field-restore-fix.md).
 *
 * Models only the pure data transforms involved -- no hardware, no Flipper
 * runtime, no FlipperFormat/file I/O. Build and run:
 *   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* --- verbatim from protocols/protocols_common.c --- */
static uint8_t pp_reverse_bits8(uint8_t value) {
    value = (uint8_t)(((value >> 4U) | (value << 4U)) & 0xFFU);
    value = (uint8_t)(((value & 0x33U) << 2U) | ((value >> 2U) & 0x33U));
    value = (uint8_t)(((value & 0x55U) << 1U) | ((value >> 1U) & 0x55U));
    return value;
}

static void pp_u64_to_bytes_be(uint64_t data, uint8_t bytes[8]) {
    for(size_t i = 0; i < 8; i++) {
        bytes[i] = (uint8_t)((data >> ((7U - i) * 8U)) & 0xFFU);
    }
}

static uint64_t pp_bytes_to_u64_be(const uint8_t bytes[8]) {
    uint64_t data = 0;
    for(size_t i = 0; i < 8; i++) {
        data = (data << 8U) | bytes[i];
    }
    return data;
}

/* Inverse of kia_v3_v4_process_buffer()'s field extraction, used only to
 * construct a synthetic wire frame carrying a known target serial/btn/
 * encrypted -- independent of the deserialize logic under test. */
static void build_frame(uint32_t serial, uint8_t btn, uint32_t encrypted, uint8_t b[8]) {
    b[0] = pp_reverse_bits8((uint8_t)(encrypted & 0xFF));
    b[1] = pp_reverse_bits8((uint8_t)((encrypted >> 8) & 0xFF));
    b[2] = pp_reverse_bits8((uint8_t)((encrypted >> 16) & 0xFF));
    b[3] = pp_reverse_bits8((uint8_t)((encrypted >> 24) & 0xFF));
    b[4] = pp_reverse_bits8((uint8_t)(serial & 0xFF));
    b[5] = pp_reverse_bits8((uint8_t)((serial >> 8) & 0xFF));
    b[6] = pp_reverse_bits8((uint8_t)((serial >> 16) & 0xFF));

    const uint8_t serial_top = (uint8_t)((serial >> 24) & 0xFF);
    int found = 0;
    for(int cand = 0; cand <= 0xFF; cand++) {
        const uint8_t b7 = (uint8_t)cand;
        const uint8_t top = pp_reverse_bits8((uint8_t)(b7 & 0xF0));
        const uint8_t decoded_btn = (uint8_t)((pp_reverse_bits8(b7) & 0xF0) >> 4);
        if(top == serial_top && decoded_btn == btn) {
            b[7] = b7;
            found = 1;
            break;
        }
    }
    assert(found);
}

/* Verbatim from kia_v3_v4_process_buffer() (protocols/kia_v3_v4.c), the
 * live-decode reference both deserialize variants are checked against. */
static void decode_fields_live(const uint8_t b[8], uint32_t* serial, uint8_t* btn) {
    *serial = ((uint32_t)pp_reverse_bits8(b[7] & 0xF0) << 24) |
              ((uint32_t)pp_reverse_bits8(b[6]) << 16) |
              ((uint32_t)pp_reverse_bits8(b[5]) << 8) | (uint32_t)pp_reverse_bits8(b[4]);
    *btn = (uint8_t)((pp_reverse_bits8(b[7]) & 0xF0) >> 4);
}

typedef struct {
    uint64_t data;
    uint32_t serial;
    uint8_t btn;
    uint32_t cnt;
    uint32_t decrypted;
} Generic;

/* ALT: current kia_protocol_decoder_v3_v4_deserialize() -- restores
 * Encrypted/Decrypted/KIAVersion/CRC but never touches
 * generic.serial/btn/cnt, which get_string() displays unchanged. */
static void deserialize_alt(Generic* g, uint32_t decrypted_from_file) {
    g->decrypted = decrypted_from_file;
    /* serial/btn/cnt intentionally left untouched -- this is the bug */
}

/* NEW: patched deserialize -- recompute serial/btn from generic.data with
 * the exact formula kia_v3_v4_process_buffer() uses on live receive, and
 * cnt from the already-restored Decrypted field (same &0xFFFF live uses). */
static void deserialize_new(Generic* g, uint32_t decrypted_from_file) {
    g->decrypted = decrypted_from_file;
    uint8_t b[8];
    pp_u64_to_bytes_be(g->data, b);
    decode_fields_live(b, &g->serial, &g->btn);
    g->cnt = g->decrypted & 0xFFFF;
}

static int run_case(uint32_t serial, uint8_t btn, uint16_t cnt, uint32_t encrypted, const char* name) {
    uint8_t b[8];
    build_frame(serial, btn, encrypted, b);
    const uint64_t data = pp_bytes_to_u64_be(b);
    const uint32_t decrypted_field = cnt; /* stand-in for instance->decrypted; only its low 16 bits matter here, matching &0xFFFF in both feed() and the fix */

    /* Poison the pre-deserialize struct the way a freshly malloc()'d (not
     * calloc()'d) SubGhzProtocolDecoderKiaV3V4 could look, to prove ALT
     * genuinely leaves serial/btn/cnt untouched rather than accidentally 0. */
    const Generic poisoned = {
        .data = data, .serial = 0xDEADBEEFu, .btn = 0xAAu, .cnt = 0xBEEFu, .decrypted = 0};
    Generic alt = poisoned;
    Generic neu = poisoned;

    deserialize_alt(&alt, decrypted_field);
    deserialize_new(&neu, decrypted_field);

    const int neu_ok =
        (neu.serial == serial) && (neu.btn == btn) && ((neu.cnt & 0xFFFFu) == (cnt & 0xFFFFu));
    const int alt_matches =
        (alt.serial == serial) && (alt.btn == btn) && ((alt.cnt & 0xFFFFu) == (cnt & 0xFFFFu));

    printf(
        "[%-10s] data=%016llX  want: Sn=%08X Btn=%X Cnt=%04X\n"
        "               ALT:  Sn=%08X Btn=%X Cnt=%04X (%s)\n"
        "               NEU:  Sn=%08X Btn=%X Cnt=%04X (%s)\n",
        name,
        (unsigned long long)data,
        serial,
        btn,
        cnt,
        alt.serial,
        alt.btn,
        alt.cnt,
        alt_matches ? "matches -- unexpected" : "stale/wrong, as the live bug produces",
        neu.serial,
        neu.btn,
        neu.cnt,
        neu_ok ? "exact round-trip" : "WRONG");

    return neu_ok && !alt_matches;
}

int main(void) {
    int all_ok = 1;
    all_ok &= run_case(0x0ABCDEFu, 0x3, 0x0ABC, 0x11223344u, "typical");
    all_ok &= run_case(0x0000000u, 0x0, 0x0000, 0x00000000u, "all-zero");
    all_ok &= run_case(0x0FFFFFFu, 0xF, 0xFFFF, 0xFFFFFFFFu, "max-values");
    all_ok &= run_case(0x0123456u, 0x7, 0x1234, 0xAABBCCDDu, "mixed");

    if(all_ok) {
        printf("\nAll cases: NEU exact round-trip, ALT stale/wrong as expected.\n");
        return 0;
    }
    printf("\nFAILURE: repro did not behave as expected.\n");
    return 1;
}
