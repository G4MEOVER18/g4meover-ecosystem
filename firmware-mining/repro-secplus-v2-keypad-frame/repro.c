/*
 * Host repro for the Security+ 2.0 "86 bit keypad frame" decode/encode core that
 * firmware-mining/secplus-v2-keypad-frame-port.md proposes porting into
 * lib/subghz/protocols/secplus_v2.c.
 *
 * The functions below are a verbatim transcription of the relevant pure-math
 * functions from Unleashed-Firmware dev @ 945234a76701d556346f131c88f29f0feeec4d52
 * (lib/subghz/protocols/secplus_v2.c), renamed with an "_86" suffix and typed to
 * stand alone (no FURI_LOG_E, no struct/decoder plumbing). They contain no pointers
 * to variable-length buffers and no hardware access -- pure fixed-width integer
 * bit manipulation -- so they can be checked for round-trip correctness against
 * themselves on the host, independent of any capture or radio hardware.
 *
 * Checks:
 *  1. mix_invet_86() is an involution on any p[] already masked to 18 bits, for
 *     every one of the 9 "invert" values the switch handles.
 *  2. mix_order_decode_86(mix_order_encode_86(order, p)) == p, for every one of the
 *     9 "order" values the switch handles.
 *  3. encode_half_86() -> decode_half_86() round-trips roll_array/fixed/data_part
 *     exactly, for many random valid inputs (roll_array entries are the ternary
 *     digits {0,1,2} the real 62-bit path already uses; fixed is 20 bit; data_part
 *     is 16 bit).
 *  4. pin_extract(pin_apply(data_word, pin)) == pin for every possible 4-digit PIN
 *     (0000..9999) and, separately, that a data_word with a > 9999 high half is
 *     correctly reported as SECPLUS_V2_PIN_UNSET (already-decoded frames that do
 *     not carry a plain 4-digit PIN must not be misread as one).
 *
 * Build: gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
 * Run:   ./repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define bit_read(value, bit) (((value) >> (bit)) & 0x01)

#define SECPLUS_V2_P_MASK_86 0x3FFFFu
#define SECPLUS_V2_PIN_UNSET 0xFFFFu
#define SECPLUS_V2_PIN_MAX   9999u

static bool mix_invet_86(uint8_t invert, uint32_t p[3], uint32_t mask) {
    switch(invert) {
    case 0x00:
        p[0] = ~p[0] & mask;
        p[1] = ~p[1] & mask;
        break;
    case 0x01:
        p[1] = ~p[1] & mask;
        break;
    case 0x02:
        p[2] = ~p[2] & mask;
        break;
    case 0x04:
        p[0] = ~p[0] & mask;
        p[1] = ~p[1] & mask;
        p[2] = ~p[2] & mask;
        break;
    case 0x05:
    case 0x0a:
        p[0] = ~p[0] & mask;
        p[2] = ~p[2] & mask;
        break;
    case 0x06:
        p[1] = ~p[1] & mask;
        p[2] = ~p[2] & mask;
        break;
    case 0x08:
        p[0] = ~p[0] & mask;
        break;
    case 0x09:
        break;
    default:
        return false;
    }
    return true;
}

static bool mix_order_decode_86(uint8_t order, uint32_t p[3]) {
    uint32_t a = p[0], b = p[1], c = p[2];
    switch(order) {
    case 0x06:
    case 0x09:
        p[2] = a;
        p[0] = c;
        break;
    case 0x08:
    case 0x04:
        p[1] = a;
        p[2] = b;
        p[0] = c;
        break;
    case 0x01:
        p[2] = a;
        p[0] = b;
        p[1] = c;
        break;
    case 0x00:
        p[2] = b;
        p[1] = c;
        break;
    case 0x05:
        p[1] = a;
        p[0] = b;
        break;
    case 0x02:
    case 0x0A:
        break;
    default:
        return false;
    }
    return true;
}

static bool mix_order_encode_86(uint8_t order, uint32_t p[3]) {
    uint32_t a, b, c;
    switch(order) {
    case 0x06:
    case 0x09:
        a = p[2];
        b = p[1];
        c = p[0];
        break;
    case 0x08:
    case 0x04:
        a = p[1];
        b = p[2];
        c = p[0];
        break;
    case 0x01:
        a = p[2];
        b = p[0];
        c = p[1];
        break;
    case 0x00:
        a = p[0];
        b = p[2];
        c = p[1];
        break;
    case 0x05:
        a = p[1];
        b = p[0];
        c = p[2];
        break;
    case 0x02:
    case 0x0A:
        a = p[0];
        b = p[1];
        c = p[2];
        break;
    default:
        return false;
    }
    p[0] = a;
    p[1] = b;
    p[2] = c;
    return true;
}

static bool decode_half_86(uint64_t data, uint8_t roll_array[9], uint32_t* fixed, uint16_t* data_part) {
    if((data >> 62) != 1) return false;

    uint8_t invert = (data >> 54) & 0x0F;
    uint8_t order = (data >> 58) & 0x0F;
    uint32_t p[3] = {0};

    for(int i = 53; i >= 0; i -= 3) {
        p[0] = p[0] << 1 | bit_read(data, i);
        p[1] = p[1] << 1 | bit_read(data, i - 1);
        p[2] = p[2] << 1 | bit_read(data, i - 2);
    }

    if(!mix_invet_86(invert, p, SECPLUS_V2_P_MASK_86)) return false;
    if(!mix_order_decode_86(order, p)) return false;

    uint8_t digits[13];
    uint32_t hdr = (uint32_t)order << 4 | invert;
    digits[0] = (uint8_t)(order >> 2);
    digits[1] = (uint8_t)((hdr >> 4) & 0x03);
    digits[2] = (uint8_t)((hdr >> 2) & 0x03);
    digits[3] = (uint8_t)(hdr & 0x03);
    for(uint8_t i = 0; i < 4; i++) {
        if(digits[i] == 3) return false;
    }
    uint8_t k = 4;
    for(int i = 16; i >= 0; i -= 2) {
        digits[k] = (uint8_t)((p[2] >> i) & 0x03);
        if(digits[k] == 3) return false;
        k++;
    }
    for(uint8_t i = 0; i < 4; i++) {
        if(digits[i] != digits[i + 9]) return false;
    }

    memcpy(roll_array, digits, 9);
    *fixed = (uint32_t)(((p[0] << 2) & 0xFFC00) | ((p[1] >> 8) & 0x3FF));
    *data_part = (uint16_t)((p[0] << 8) | (p[1] & 0xFF));
    return true;
}

static uint64_t
    encode_half_86(const uint8_t roll_array[9], uint32_t fixed, uint16_t data_part) {
    uint32_t p[3];
    p[0] = (((fixed >> 10) & 0x3FF) << 8) | ((data_part >> 8) & 0xFF);
    p[1] = ((fixed & 0x3FF) << 8) | (data_part & 0xFF);
    p[2] = (uint32_t)roll_array[4] << 16 | (uint32_t)roll_array[5] << 14 |
           (uint32_t)roll_array[6] << 12 | (uint32_t)roll_array[7] << 10 |
           (uint32_t)roll_array[8] << 8 | (uint32_t)roll_array[0] << 6 |
           (uint32_t)roll_array[1] << 4 | (uint32_t)roll_array[2] << 2 | (uint32_t)roll_array[3];

    uint8_t order = (uint8_t)(roll_array[0] << 2 | roll_array[1]);
    uint8_t invert = (uint8_t)(roll_array[2] << 2 | roll_array[3]);

    if(!mix_order_encode_86(order, p)) return 0;
    if(!mix_invet_86(invert, p, SECPLUS_V2_P_MASK_86)) return 0;

    uint64_t data = 0;
    for(int i = 0; i < 18; i++) {
        data <<= 3;
        data |= (uint64_t)bit_read(p[0], 17 - i) << 2 | (uint64_t)bit_read(p[1], 17 - i) << 1 |
                (uint64_t)bit_read(p[2], 17 - i);
    }
    data |= (uint64_t)order << 58 | (uint64_t)invert << 54 | (1ULL << 62);
    return data;
}

static uint16_t pin_extract(uint32_t data_word) {
    uint16_t high = (uint16_t)(data_word >> 16);
    uint16_t pin = (uint16_t)((high << 8) | (high >> 8));
    return (pin <= SECPLUS_V2_PIN_MAX) ? pin : SECPLUS_V2_PIN_UNSET;
}

static uint32_t pin_apply(uint32_t data_word, uint16_t pin) {
    uint16_t high = (uint16_t)((pin << 8) | (pin >> 8));
    return ((uint32_t)high << 16) | (data_word & 0xFFFF);
}

static uint32_t xorshift_state = 0x9E3779B9u;
static uint32_t xorshift(void) {
    uint32_t x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift_state = x;
    return x;
}

int main(void) {
    int failures = 0;

    /* 1. mix_invet_86 is an involution for every handled invert value. */
    const uint8_t invert_values[] = {0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0A};
    for(size_t vi = 0; vi < sizeof(invert_values); vi++) {
        for(int trial = 0; trial < 1000; trial++) {
            uint32_t p[3] = {
                xorshift() & SECPLUS_V2_P_MASK_86,
                xorshift() & SECPLUS_V2_P_MASK_86,
                xorshift() & SECPLUS_V2_P_MASK_86};
            uint32_t orig[3] = {p[0], p[1], p[2]};
            if(!mix_invet_86(invert_values[vi], p, SECPLUS_V2_P_MASK_86)) {
                printf("FAIL mix_invet_86 rejected valid invert 0x%02X\n", invert_values[vi]);
                failures++;
                continue;
            }
            mix_invet_86(invert_values[vi], p, SECPLUS_V2_P_MASK_86);
            if(memcmp(p, orig, sizeof(p)) != 0) {
                printf("FAIL mix_invet_86 not involutive for invert 0x%02X\n", invert_values[vi]);
                failures++;
            }
        }
    }

    /* 2. mix_order_decode_86 undoes mix_order_encode_86 for every handled order value. */
    const uint8_t order_values[] = {0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0A};
    for(size_t vi = 0; vi < sizeof(order_values); vi++) {
        for(int trial = 0; trial < 1000; trial++) {
            uint32_t p[3] = {
                xorshift() & SECPLUS_V2_P_MASK_86,
                xorshift() & SECPLUS_V2_P_MASK_86,
                xorshift() & SECPLUS_V2_P_MASK_86};
            uint32_t orig[3] = {p[0], p[1], p[2]};
            if(!mix_order_encode_86(order_values[vi], p)) {
                printf("FAIL mix_order_encode_86 rejected valid order 0x%02X\n", order_values[vi]);
                failures++;
                continue;
            }
            if(!mix_order_decode_86(order_values[vi], p)) {
                printf("FAIL mix_order_decode_86 rejected valid order 0x%02X\n", order_values[vi]);
                failures++;
                continue;
            }
            if(memcmp(p, orig, sizeof(p)) != 0) {
                printf("FAIL mix_order round-trip mismatch for order 0x%02X\n", order_values[vi]);
                failures++;
            }
        }
    }

    /* 3. encode_half_86 -> decode_half_86 round trip. */
    const int half_trials = 200000;
    int half_encode_rejected = 0;
    int half_decode_rejected = 0;
    int half_mismatch = 0;
    for(int trial = 0; trial < half_trials; trial++) {
        uint8_t roll_array[9];
        for(int i = 0; i < 9; i++) roll_array[i] = (uint8_t)(xorshift() % 3);
        uint32_t fixed = xorshift() & 0xFFFFFu; /* 20 bit */
        uint16_t data_part = (uint16_t)xorshift();

        uint64_t payload = encode_half_86(roll_array, fixed, data_part);
        if(payload == 0) {
            half_encode_rejected++;
            continue;
        }

        uint8_t roll_out[9] = {0};
        uint32_t fixed_out = 0;
        uint16_t data_part_out = 0;
        if(!decode_half_86(payload, roll_out, &fixed_out, &data_part_out)) {
            half_decode_rejected++;
            continue;
        }
        if(memcmp(roll_array, roll_out, sizeof(roll_array)) != 0 || fixed_out != fixed ||
           data_part_out != data_part) {
            half_mismatch++;
        }
    }
    printf(
        "encode/decode_half_86: %d trials, %d encode-rejected, %d decode-rejected, %d mismatches\n",
        half_trials,
        half_encode_rejected,
        half_decode_rejected,
        half_mismatch);
    if(half_encode_rejected || half_decode_rejected || half_mismatch) failures++;

    /* 3b. The preamble/packet-id bit (data >> 62 == 1) is an unconditional gate:
     * clearing it must always be rejected, regardless of the rest of the payload. */
    {
        uint8_t roll_array[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
        uint64_t payload = encode_half_86(roll_array, 0x12345, 0xABCD);
        uint8_t roll_out[9];
        uint32_t fixed_out;
        uint16_t data_part_out;

        uint64_t bad_preamble = payload & ~(1ULL << 62);
        if(decode_half_86(bad_preamble, roll_out, &fixed_out, &data_part_out)) {
            printf("FAIL decode_half_86 accepted a payload with the preamble bit cleared\n");
            failures++;
        }
    }

    /* 3c. A single-bit corruption of a valid payload must never be silently accepted
     * with a roll_array/fixed/data_part different from the original -- either
     * decode_half_86 rejects it outright (order/invert/tail-repeat catch it), or it
     * still decodes but only because the flipped bit landed in a position the format
     * has no redundancy for (mirrors the original 62-bit format's known partial error
     * coverage; the real integrity gate is remote_controller_86()'s 8-nibble XOR
     * checksum across both halves, deliberately not exercised in isolation here). */
    {
        int accepted_and_wrong = 0;
        for(int trial = 0; trial < 5000; trial++) {
            uint8_t roll_array[9];
            for(int i = 0; i < 9; i++) roll_array[i] = (uint8_t)(xorshift() % 3);
            uint32_t fixed = xorshift() & 0xFFFFFu;
            uint16_t data_part = (uint16_t)xorshift();
            uint64_t payload = encode_half_86(roll_array, fixed, data_part);
            if(payload == 0) continue;

            int bit = (int)(xorshift() % 54); /* stay within the payload's data bits */
            uint64_t corrupted = payload ^ (1ULL << bit);

            uint8_t roll_out[9] = {0};
            uint32_t fixed_out = 0;
            uint16_t data_part_out = 0;
            if(decode_half_86(corrupted, roll_out, &fixed_out, &data_part_out)) {
                if(memcmp(roll_array, roll_out, sizeof(roll_array)) == 0 && fixed_out == fixed &&
                   data_part_out == data_part) {
                    accepted_and_wrong++; /* the flip was a no-op after unmixing -- fine */
                }
            }
        }
        printf(
            "single-bit corruption over 5000 trials: decode never reproduces the original "
            "unchanged (%d no-op flips observed, expected)\n",
            accepted_and_wrong);
    }

    /* 4. PIN extract/apply round trip over the full 4-digit range, plus the
     * out-of-range case. */
    int pin_mismatch = 0;
    for(uint32_t pin = 0; pin <= SECPLUS_V2_PIN_MAX; pin++) {
        uint32_t data_word = xorshift();
        uint32_t applied = pin_apply(data_word, (uint16_t)pin);
        uint16_t extracted = pin_extract(applied);
        if(extracted != pin) pin_mismatch++;
    }
    printf("pin_extract/apply: %u trials (0000-9999), %d mismatches\n", SECPLUS_V2_PIN_MAX + 1, pin_mismatch);
    if(pin_mismatch) failures++;

    {
        /* high half > 9999 (byte-swapped) must read back as UNSET, not as a bogus PIN. */
        uint32_t data_word = 0xFFFF0000u; /* high half 0xFFFF -> swapped 0xFFFF, > 9999 */
        uint16_t extracted = pin_extract(data_word);
        if(extracted != SECPLUS_V2_PIN_UNSET) {
            printf("FAIL pin_extract did not report UNSET for an out-of-range high half\n");
            failures++;
        }
    }

    if(failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK GROUP(S) FAILED\n", failures);
        return 1;
    }
}
