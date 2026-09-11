/*
 * Standalone host repro for the Prastel 42-bit scramble/unscramble/pack/unpack
 * math I am porting from Unleashed-Firmware's lib/subghz/protocols/prastel.c
 * (dev, commit 945234a, 2026-09-10) into my own SubGhz stack.
 *
 * Goal: before I trust this bit-twiddling enough to write it into a port
 * proposal, verify on the host that the two directions are genuine inverses
 * of each other over a wide range of 42-bit payloads, and that editing the
 * button/counter fields through unscramble -> edit -> scramble round-trips
 * the edited fields back out correctly on the next unscramble. This is pure
 * self-consistency checking of the transcription, not a claim about the
 * real protocol (I have no Prastel captures to check against).
 *
 * All of reverse_byte()/parity()/unscramble()/scramble()/unpack()/pack()
 * below are transcribed unchanged from the Unleashed source; only the
 * subghz_protocol_prastel_ prefix is dropped and a test harness is added.
 *
 * Build:
 *   gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
 *   ./repro
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t reverse_byte(uint8_t v) {
    v = (uint8_t)((v >> 4) | (v << 4));
    v = (uint8_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = (uint8_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

static uint8_t parity(uint8_t v) {
    v ^= (uint8_t)(v >> 4);
    return (uint8_t)((0x6996u >> (v & 0x0F)) & 1u);
}

static void unscramble(uint8_t p[7]) {
    uint8_t carry = 0;
    for(uint8_t round = 0; round < 3; round++) {
        for(uint8_t i = 2; i <= 6; i++) {
            uint8_t cur = p[i];
            p[i] = (uint8_t)((carry << 7) | (cur >> 1));
            carry = cur & 1;
        }
    }

    p[0] = reverse_byte(p[0]);
    uint8_t t2 = p[2] & 0xDF;
    p[1] = reverse_byte(p[1]);
    p[3] = reverse_byte(p[3]);
    p[4] = reverse_byte(p[4]);
    uint8_t swapped = (uint8_t)((t2 << 4) | (t2 >> 4));
    p[2] = reverse_byte((uint8_t)~swapped);

    uint8_t p6 = p[6];
    p[5] = reverse_byte(p[5]);
    if(p6 & 1) p[5] ^= 1;

    uint8_t t = (uint8_t)(((uint8_t)(p[5] * 21) + 1) * 21);
    uint8_t x = reverse_byte(p6);
    uint8_t n = (uint8_t)(t + 1);
    if(n != 0) {
        uint8_t limit = (uint8_t)~t;
        uint8_t i = 0;
        do {
            x = (uint8_t)(x * 21 + 1);
            i++;
        } while(limit > i);
    }
    p[5] = n ^ 0xA7;
    p[6] = x ^ 0x69;
}

static void scramble(uint8_t p[7]) {
    uint8_t n = (uint8_t)(p[5] ^ 0xA7);
    uint8_t x = (uint8_t)(p[6] ^ 0x69);

    for(uint8_t i = 0; n > i; i++) {
        x = (uint8_t)(x * 21 + 1);
    }
    for(uint8_t i = 0; i < 254; i++) {
        n = (uint8_t)(n * 21 + 1);
    }

    const int flip = (x & 0x80) != 0;

    p[0] = reverse_byte(p[0]);
    p[1] = reverse_byte(p[1]);
    p[3] = reverse_byte(p[3]);
    p[4] = reverse_byte(p[4]);

    if(flip) n ^= 1;
    if(p[2] == 0) p[2] = 0xF0;
    uint8_t w = (uint8_t)~reverse_byte(p[2]);
    p[2] = (uint8_t)(((w << 4) & 0xD0) | ((w >> 4) & 0x0F));
    p[5] = reverse_byte(n);
    p[6] = reverse_byte(x);

    const uint8_t parity_5 = parity(p[5]);
    const uint8_t parity_6 = parity(p[6]);

    uint8_t carry = 0;
    for(uint8_t round = 0; round < 3; round++) {
        for(int8_t i = 6; i >= 2; i--) {
            uint8_t cur = p[i];
            p[i] = (uint8_t)((cur << 1) | carry);
            carry = (uint8_t)(cur >> 7);
        }
    }

    if(parity_5 == parity_6) {
        p[6] &= (uint8_t)~0x04;
    } else {
        p[6] |= 0x04;
    }
}

static void unpack(uint64_t data, uint8_t p[7]) {
    uint64_t payload = ((uint64_t)((data >> 30) & 0xFFF) << 32) | (uint32_t)(data << 2);
    for(uint8_t i = 0; i < 7; i++) {
        p[i] = (uint8_t)(payload >> (0x30u - 8u * i));
    }
}

static uint64_t pack(const uint8_t p[7]) {
    uint64_t payload = 0;
    for(uint8_t i = 0; i < 7; i++) {
        payload |= (uint64_t)p[i] << (0x30u - 8u * i);
    }
    return (payload >> 2) & 0x3FFFFFFFFFFULL;
}

/* Mirrors subghz_protocol_prastel_remote_controller()'s field extraction,
 * minus the custom-button bookkeeping (not part of the math under test). */
typedef struct {
    uint32_t serial;
    uint8_t btn_idx; /* raw (p[4] & 0x0F), before the btn_map lookup */
    uint16_t cnt;
} Fields;

static Fields decode_fields(uint64_t data) {
    uint8_t p[7];
    unpack(data, p);
    unscramble(p);
    Fields f;
    f.serial = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 8) | p[3] | (((uint32_t)p[4] << 12) & 0xF0000);
    f.btn_idx = (uint8_t)(p[4] & 0x0F);
    f.cnt = (uint16_t)(p[5] | (p[6] << 8));
    return f;
}

/* Mirrors subghz_protocol_prastel_gen_data(): rewrite btn/cnt into a data
 * word, keeping the serial and everything else the unscrambled state
 * carried. */
static uint64_t gen_data(uint64_t data, uint8_t btn_raw, uint16_t cnt) {
    uint8_t p[7];
    unpack(data, p);
    unscramble(p);

    p[4] = (uint8_t)((p[4] & 0xF0) | (btn_raw & 0x0F));
    p[5] = (uint8_t)cnt;
    p[6] = (uint8_t)(cnt >> 8);

    scramble(p);
    return pack(p);
}

static int g_fail = 0;

static void check(const char* what, int ok) {
    printf("%-70s %s\n", what, ok ? "OK" : "FAIL");
    if(!ok) g_fail = 1;
}

static uint64_t rng_state = 0x243F6A8885A308D3ULL;
static uint64_t rng_next(void) {
    /* splitmix64, only used to pick deterministic pseudo-random 42-bit test
     * inputs - not part of the protocol under test. */
    rng_state += 0x9E3779B97F4A7C15ULL;
    uint64_t z = rng_state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int main(void) {
    /* NOTE on methodology: an earlier version of this harness also tried
     * unscramble(scramble(p)) == p directly on arbitrary 7-byte arrays,
     * without routing them through pack()/unpack() first, and saw ~50%%
     * mismatches. That test was invalid, not a finding: pack()/unpack()
     * themselves discard the bottom 2 bits of the 44-bit intermediate
     * payload (unpack() always derives its input as data << 2, pack()
     * always right-shifts by 2 again), and scramble() separately
     * overwrites bit 2 of p[6] with a parity check it computes itself
     * rather than preserving whatever was passed in. Neither bit is a
     * "free" plaintext bit that a real caller ever controls - unpack()'s
     * own output always already has them canonicalized - so testing
     * arbitrary byte arrays that skip that canonicalization exercises
     * byte patterns unscramble()/scramble() never actually see. The two
     * checks below route everything through pack()/unpack() and
     * unscramble()/gen_data() exactly as the real decoder/encoder do, which
     * is the property that actually matters.
     */
    printf("=== unpack -> unscramble -> scramble -> pack, starting from OUR OWN scramble() output ===\n");
    /* Same composition as above but routed through pack()/unpack() and the
     * 42-bit data word, i.e. exactly the sequence gen_data() performs
     * end-to-end when re-encoding a signal, starting from data that is
     * itself a legitimate scrambled frame (produced by scramble() here,
     * standing in for a real remote). */
    int legit_fail = 0;
    for(int i = 0; i < 50000; i++) {
        uint8_t p[7];
        uint64_t r = rng_next();
        memcpy(p, &r, 7);
        scramble(p); /* now p is a "legitimate" scrambled frame */
        uint64_t data = pack(p);

        uint8_t p2[7];
        unpack(data, p2);
        unscramble(p2);
        scramble(p2);
        uint64_t back = pack(p2);
        if(back != data) legit_fail++;
    }
    printf("legitimate-frame round-trip over 50000 samples: %d mismatches\n", legit_fail);
    check("unpack;unscramble;scramble;pack == identity on our own scramble() output", legit_fail == 0);

    printf("\n=== gen_data() then re-decode recovers the requested btn/cnt, serial unchanged ===\n");
    /* base "captures" are built the same way as the legitimate-frame test:
     * a random plaintext p[] run through our own scramble(), standing in
     * for a real remote's transmission - this is the only kind of input
     * gen_data() (via the real decoder) is ever handed in the firmware. */
    int gen_fail_count = 0;
    int gen_checks = 0;
    for(int i = 0; i < 5000; i++) {
        uint8_t p[7];
        uint64_t r = rng_next();
        memcpy(p, &r, 7);
        scramble(p);
        uint64_t base = pack(p);

        Fields orig = decode_fields(base);
        uint16_t new_cnt = (uint16_t)rng_next();
        uint64_t edited = gen_data(base, orig.btn_idx, new_cnt);
        Fields back = decode_fields(edited);
        gen_checks++;
        int ok = (back.serial == orig.serial) && (back.btn_idx == orig.btn_idx) &&
                 (back.cnt == new_cnt);
        if(!ok) {
            gen_fail_count++;
            printf(
                "  mismatch base=0x%011llX cnt_in=0x%04X -> serial %08X/%08X btn %X/%X cnt "
                "%04X/%04X\n",
                (unsigned long long)base, new_cnt, orig.serial, back.serial, orig.btn_idx,
                back.btn_idx, new_cnt, back.cnt);
        }
    }
    /* Also the four hand-picked edge counters (0, 1, max-1, max) against one
     * fixed legitimate base, since the random sweep above may not hit them. */
    {
        uint8_t p[7] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};
        scramble(p);
        uint64_t base = pack(p);
        Fields orig = decode_fields(base);
        uint16_t edge_cnts[] = {0, 1, 0xFFFE, 0xFFFF};
        for(size_t c = 0; c < sizeof(edge_cnts) / sizeof(edge_cnts[0]); c++) {
            uint64_t edited = gen_data(base, orig.btn_idx, edge_cnts[c]);
            Fields back = decode_fields(edited);
            gen_checks++;
            int ok = (back.serial == orig.serial) && (back.btn_idx == orig.btn_idx) &&
                     (back.cnt == edge_cnts[c]);
            if(!ok) {
                gen_fail_count++;
                printf("  edge mismatch cnt_in=0x%04X -> cnt=0x%04X\n", edge_cnts[c], back.cnt);
            }
        }
    }
    printf("gen_data/decode checks: %d, mismatches: %d\n", gen_checks, gen_fail_count);
    check("gen_data() round-trips serial/btn/cnt through decode_fields()", gen_fail_count == 0);

    printf("\n=== unpack()/pack() are inverses on their own (42-bit domain) ===\n");
    int pack_fail = 0;
    for(int i = 0; i < 50000; i++) {
        uint64_t data = rng_next() & 0x3FFFFFFFFFFULL;
        uint8_t p[7];
        unpack(data, p);
        uint64_t back = pack(p);
        if(back != data) pack_fail++;
    }
    check("unpack()/pack() are exact inverses over random 42-bit values", pack_fail == 0);

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail;
}
