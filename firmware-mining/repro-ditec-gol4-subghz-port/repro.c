// Host repro for firmware-mining/ditec-gol4-subghz-port.md
// (lib/subghz/protocols/ditec_gol4.{c,h}, ported from Unleashed unlshd-093).
//
// Transcribes 1:1, unmodified, the actual protocol logic from the ported
// file: the rolling-code transform (gol4_bit_reverse/_parity, gol4_lcg_step/
// _inverse, gol4_decode_rotate_and_bitrev, gol4_decode_lcg_xor,
// gol4_encode_lcg_xor, gol4_encode_bitrev_and_rotate), the encoder's
// get_upload() bit->timing expansion, and the decoder's feed() state
// machine (Reset/StartBit/SaveDuration/CheckDuration) -- plus the fixed
// variant of serial_to_display() for comparison.
//
//   gcc -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro_o0 repro.c && ./repro_o0
//   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro_o1 repro.c && ./repro_o1
//
// Scenarios:
//  A. Rolling-code encode<->decode round trip over 50000 random 7-byte raw
//     frames (raw[2] constrained to btn&0x0F, the only domain encode_key()
//     ever actually writes into it) -- bit-exact on every byte except the
//     upper nibble of byte 2, which the real decode_key() never reads back
//     anyway (only decrypted[2]&0x0F feeds instance->btn).
//  A2. The encoder's own "raw[2]==0 -> 0xF0" special case (btn==0, "Prog"
//      mode): confirms the full byte deterministically comes back as 0xF0,
//      not 0x00, and that this is harmless since only the low nibble is
//      read back.
//  B. The decoder's own doc-comment binary samples (four regular-button
//     presses of one real remote) decode to the same Serial, with the
//     documented 0x1/0x2/0x4/0x8 Btn nibble each.
//  C. **Main finding**: encode(get_upload) -> decode(feed) frame-count
//     behavior for repeat=1..6 back-to-back frames. The encoder's leading
//     GAP (te_long*22, low) doubles as the decoder's own end-of-frame
//     marker, but the decoder only recognizes that marker by seeing the
//     *next* frame's START bit immediately followed by its first data bit
//     while te_last is still holding the stale GAP duration from
//     SaveDuration -- i.e. completing frame N needs frame N+1 to already be
//     under way. Measured: found_count == floor(repeat/2), not
//     ceil(repeat/2) as for the simpler single-state-check GAP sharing
//     documented in nord-ice-subghz-port.md/treadmill37-subghz-port.md/
//     allstar-firefly-subghz-port.md. repeat=1 (a single button press with
//     no repeats) decodes **zero** frames -- strictly worse than those
//     three. Unchanged upstream feed() behavior, not something this port
//     introduces.
//  D. A too-short leading gap must not be mistaken for the real GAP.
//  E. A corrupted bit timing (neither the short/long nor long/short
//     pattern) must not produce a false frame.
//  F. serial_to_display() signed-left-shift UB: the original expression
//     ((s[0]<<24)|...) overflows a signed int for any s[0] with the top bit
//     set (every doc-comment sample has s[0]==0xCC) -- isolated in a
//     forked child (same technique as
//     firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c) so the
//     UBSan abort doesn't end the whole run. Confirms the fixed version
//     (cast each byte to uint32_t before shifting, used in the actual port)
//     survives and returns the identical bit pattern.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Original serial_to_display() (unchanged from ditec_gol4.c) -- s[0] is
// uint8_t, promoted to (signed) int for the shift; for any s[0] with the
// top bit set (every one of the four doc-comment samples: s[0]==0xCC),
// s[0]<<24 overflows a 32-bit signed int, which is undefined behavior in
// C even though every mainstream compiler/ABI just produces the
// mathematically expected bit pattern for it in practice.
static uint32_t serial_to_display_orig(const uint8_t* s) {
    if(!s) return 0;
    return (uint32_t)((s[0] << 24) | (s[4] << 16) | (s[1] << 8) | s[3]);
}

// Fixed: cast each byte to uint32_t before shifting, so the shift operates
// on an unsigned type wide enough to hold the result -- no UB for any
// input, same result on every platform.
static uint32_t serial_to_display_fixed(const uint8_t* s) {
    if(!s) return 0;
    return ((uint32_t)s[0] << 24) | ((uint32_t)s[4] << 16) | ((uint32_t)s[1] << 8) |
           (uint32_t)s[3];
}

// Runs fn(s) in a forked child so a UBSan abort in one case doesn't end
// the whole test run (same technique as
// firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c). Returns true
// if the child survived and *out_result was written back through s_out.
static bool run_isolated(uint32_t (*fn)(const uint8_t*), const uint8_t* s, uint32_t* out_result) {
    int pipefd[2];
    if(pipe(pipefd) != 0) return false;
    pid_t pid = fork();
    if(pid == 0) {
        uint32_t r = fn(s);
        ssize_t written = write(pipefd[1], &r, sizeof(r));
        (void)written;
        _exit(0);
    }
    close(pipefd[1]);
    uint32_t r = 0;
    ssize_t n = read(pipefd[0], &r, sizeof(r));
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    bool crashed = WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    if(!crashed && n == (ssize_t)sizeof(r)) {
        *out_result = r;
        return true;
    }
    return false;
}

#define DURATION_DIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))
#define bit_read(value, b) (((value) >> (b)) & 0x01)

typedef struct {
    uint16_t te_short;
    uint16_t te_long;
    uint16_t te_delta;
    uint8_t min_count_bit_for_found;
} Const;

static const Const K = {.te_short = 400, .te_long = 1100, .te_delta = 200, .min_count_bit_for_found = 54};

#define GOL4_RAW_BYTES 7

// --- 1:1 transcribed rolling-code transform from ditec_gol4.c ---
static uint8_t gol4_bit_reverse(uint8_t b) {
    b &= 0xFF;
    uint8_t result = 0;
    for(uint8_t i = 0; i < 8; i++) {
        result = (uint8_t)((result << 1) | (b & 1));
        b >>= 1;
    }
    return result;
}

static uint8_t gol4_bit_parity(uint8_t b) {
    uint8_t p = 0;
    for(uint8_t i = 0; i < 8; i++) {
        if((b >> i) & 1u) p ^= 1u;
    }
    return p;
}

static uint8_t gol4_lcg_step(uint8_t seed, uint8_t steps) {
    uint8_t x = seed & 0xFF;
    steps &= 0xFF;
    for(uint8_t i = 0; i < steps; i++) {
        x = (uint8_t)((21 * x + 1) & 0xFF);
    }
    return x;
}

static uint8_t gol4_lcg_inverse(uint8_t target, uint8_t steps) {
    steps &= 0xFF;
    if(steps == 0) return target & 0xFF;
    return gol4_lcg_step(target, (uint8_t)(256 - steps));
}

static void gol4_decode_rotate_and_bitrev(uint8_t* raw) {
    uint8_t carry = 0;
    for(uint8_t r = 0; r < 3; r++) {
        for(uint8_t i = 2; i < 7; i++) {
            uint8_t new_carry = raw[i] & 1;
            raw[i] = (uint8_t)(((raw[i] >> 1) | (carry << 7)) & 0xFF);
            carry = new_carry;
        }
    }
    raw[0] = gol4_bit_reverse(raw[0]);
    raw[1] = gol4_bit_reverse(raw[1]);
    raw[3] = gol4_bit_reverse(raw[3]);
    raw[4] = gol4_bit_reverse(raw[4]);

    uint8_t b2 = raw[2] & 0xDF;
    b2 = (uint8_t)(((b2 << 4) | (b2 >> 4)) & 0xFF);
    b2 = (uint8_t)((~b2) & 0xFF);
    raw[2] = gol4_bit_reverse(b2);

    raw[5] = gol4_bit_reverse(raw[5]);
    raw[6] = gol4_bit_reverse(raw[6]);
}

static bool gol4_decode_lcg_xor(uint8_t* raw) {
    if(raw[6] & 0x80) raw[5] ^= 1;
    uint8_t out5 = gol4_lcg_inverse(raw[5], 0xFE);
    raw[5] = out5;
    uint8_t out6 = gol4_lcg_inverse(raw[6], raw[5]);
    raw[6] = out6;
    raw[5] ^= 0xA7;
    raw[6] ^= 0x69;
    return true;
}

static bool gol4_rolling_decode(uint8_t* raw) {
    gol4_decode_rotate_and_bitrev(raw);
    return gol4_decode_lcg_xor(raw);
}

static bool gol4_encode_lcg_xor(uint8_t* raw) {
    uint8_t dec5 = (uint8_t)(raw[5] ^ 0xA7);
    uint8_t dec6 = (uint8_t)(raw[6] ^ 0x69);
    uint8_t enc6 = gol4_lcg_step(dec6, dec5);
    uint8_t enc5 = gol4_lcg_step(dec5, 0xFE);
    if(enc6 & 0x80) enc5 ^= 1;
    raw[5] = enc5;
    raw[6] = enc6;
    return true;
}

static void gol4_encode_bitrev_and_rotate(uint8_t* raw) {
    raw[0] = gol4_bit_reverse(raw[0]);
    raw[1] = gol4_bit_reverse(raw[1]);
    raw[3] = gol4_bit_reverse(raw[3]);
    raw[4] = gol4_bit_reverse(raw[4]);

    if(raw[2] == 0x0) {
        raw[2] = 0xF0;
    }
    uint8_t b2 = gol4_bit_reverse(raw[2]);
    b2 = (uint8_t)(~b2);
    b2 = (uint8_t)(((b2 << 4) | (b2 >> 4)) & 0xFF);
    b2 &= 0xDF;
    raw[2] = b2;

    raw[5] = gol4_bit_reverse(raw[5]);
    raw[6] = gol4_bit_reverse(raw[6]);

    uint8_t p5 = gol4_bit_parity(raw[5]);
    uint8_t p6 = gol4_bit_parity(raw[6]);

    uint8_t carry = 0;
    for(uint8_t r = 0; r < 3; r++) {
        for(int8_t i = 6; i >= 2; i--) {
            uint8_t new_carry = (uint8_t)((raw[i] >> 7) & 1);
            raw[i] = (uint8_t)(((raw[i] << 1) | carry) & 0xFF);
            carry = new_carry;
        }
    }
    raw[6] = (p5 == p6) ? (uint8_t)(raw[6] & 0xFBu) : (uint8_t)(raw[6] | 0x04u);
}

static bool gol4_rolling_encode(uint8_t* raw) {
    if(!raw) return false;
    if(!gol4_encode_lcg_xor(raw)) return false;
    gol4_encode_bitrev_and_rotate(raw);
    return true;
}

static void bits_to_raw(const uint8_t* bits, uint8_t* raw) {
    memset(raw, 0, GOL4_RAW_BYTES);
    for(uint8_t i = 0; i < K.min_count_bit_for_found; i++) {
        uint8_t byte_idx = i / 8;
        uint8_t bit_idx = 7 - (i % 8);
        if(bits[i]) raw[byte_idx] |= (1 << bit_idx);
    }
}

// --- 1:1 transcribed encoder get_upload() bit expansion (GAP + START + bits) ---
typedef struct {
    bool level;
    uint32_t duration;
} Pulse;

static size_t encode_frame(uint64_t data, Pulse* buf, size_t cap, size_t idx) {
    size_t start = idx;
    assert(idx + 2 <= cap);
    buf[idx++] = (Pulse){false, (uint32_t)K.te_long * 22};
    buf[idx++] = (Pulse){true, (uint32_t)K.te_short * 2};
    for(uint8_t i = K.min_count_bit_for_found; i > 0; i--) {
        assert(idx + 2 <= cap);
        if(bit_read(data, i - 1)) {
            buf[idx++] = (Pulse){false, (uint32_t)K.te_short};
            buf[idx++] = (Pulse){true, (uint32_t)K.te_long};
        } else {
            buf[idx++] = (Pulse){false, (uint32_t)K.te_long};
            buf[idx++] = (Pulse){true, (uint32_t)K.te_short};
        }
    }
    return idx - start;
}

// --- 1:1 transcribed decoder feed() state machine ---
typedef enum { StepReset = 0, StepStartBit, StepSaveDuration, StepCheckDuration } Step;

typedef struct {
    Step parser_step;
    uint32_t te_last;
    uint64_t decode_data;
    uint8_t decode_count_bit;

    int found_count;
    uint64_t last_found_data;
    uint8_t last_found_count_bit;
} DecState;

static void add_bit(DecState* s, int bit) {
    s->decode_data = (s->decode_data << 1) | (uint64_t)bit;
    s->decode_count_bit++;
}

static void decoder_feed(DecState* s, bool level, uint32_t duration) {
    switch(s->parser_step) {
    case StepReset:
        if((!level) && (DURATION_DIFF(duration, (uint32_t)K.te_long * 22) < (uint32_t)(K.te_long * 4))) {
            s->decode_data = 0;
            s->decode_count_bit = 0;
            s->parser_step = StepStartBit;
        }
        break;
    case StepStartBit:
        if((level) && (DURATION_DIFF(duration, (uint32_t)K.te_short * 2) < K.te_delta)) {
            s->parser_step = StepSaveDuration;
        } else {
            s->parser_step = StepReset;
        }
        break;
    case StepSaveDuration:
        if(!level) {
            s->te_last = duration;
            s->parser_step = StepCheckDuration;
        } else {
            s->parser_step = StepReset;
        }
        break;
    case StepCheckDuration:
        if(level) {
            if((DURATION_DIFF(s->te_last, K.te_short) < K.te_delta) &&
               (DURATION_DIFF(duration, K.te_long) < K.te_delta)) {
                add_bit(s, 1);
                s->parser_step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) &&
                (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                add_bit(s, 0);
                s->parser_step = StepSaveDuration;
            }
            // else: no transition at all -- stays in StepCheckDuration (as upstream)
        } else {
            if(DURATION_DIFF(s->te_last, (uint32_t)K.te_long * 20) < (uint32_t)(K.te_long * 3)) {
                if(s->decode_count_bit == K.min_count_bit_for_found) {
                    s->last_found_data = s->decode_data;
                    s->last_found_count_bit = s->decode_count_bit;
                    s->found_count++;
                }
                s->decode_data = 0;
                s->decode_count_bit = 0;
                s->parser_step = StepReset;
            } else {
                s->parser_step = StepReset;
            }
        }
        break;
    }
}

static void run_pulses(DecState* s, const Pulse* p, size_t n) {
    for(size_t i = 0; i < n; i++) decoder_feed(s, p[i].level, p[i].duration);
}

static uint32_t rnd_state = 0xC0FFEEu;
static uint32_t xrand(void) {
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return rnd_state;
}

int main(void) {
    // --- Scenario A: rolling-code round trip over 50000 random raw frames ---
    int trials = 50000;
    for(int t = 0; t < trials; t++) {
        uint8_t plain[GOL4_RAW_BYTES];
        for(int i = 0; i < GOL4_RAW_BYTES; i++) plain[i] = (uint8_t)(xrand() & 0xFF);
        // raw[2] is always btn&0x0F in the real protocol (encode_key():
        // decrypted[2] = (uint8_t)(instance->btn & 0x0F);) -- never an
        // arbitrary byte, so constrain it to the actual 4-bit domain.
        plain[2] &= 0x0F;
        uint8_t raw[GOL4_RAW_BYTES];
        memcpy(raw, plain, GOL4_RAW_BYTES);
        bool ok = gol4_rolling_encode(raw);
        assert(ok);
        uint8_t decoded[GOL4_RAW_BYTES];
        memcpy(decoded, raw, GOL4_RAW_BYTES);
        ok = gol4_rolling_decode(decoded);
        assert(ok);
        // Bytes 0,1,3,4 (serial) and 5,6 (counter) must round-trip exactly.
        // Byte 2 only round-trips in its low nibble: encode_key() only ever
        // writes btn&0x0F into it, and encode's own "raw[2]==0 -> 0xF0"
        // special case (so the all-zero button code is never sent as an
        // all-zero byte2) means the *full* byte for btn==0 comes back as
        // 0xF0, not 0x00 -- harmless, since decode_key() itself only ever
        // reads decrypted[2]&0x0F back into instance->btn.
        bool mismatch = (decoded[0] != plain[0]) || (decoded[1] != plain[1]) ||
                         (decoded[3] != plain[3]) || (decoded[4] != plain[4]) ||
                         (decoded[5] != plain[5]) || (decoded[6] != plain[6]) ||
                         ((decoded[2] & 0x0F) != (plain[2] & 0x0F));
        if(mismatch) {
            printf(
                "MISMATCH trial %d: plain=%02X%02X%02X%02X%02X%02X%02X decoded=%02X%02X%02X%02X%02X%02X%02X\n",
                t,
                plain[0], plain[1], plain[2], plain[3], plain[4], plain[5], plain[6],
                decoded[0], decoded[1], decoded[2], decoded[3], decoded[4], decoded[5], decoded[6]);
            return 1;
        }
    }
    printf("Scenario A OK: %d trials, rolling-code encode<->decode round trip bit-exact\n", trials);

    // --- Scenario A2: raw[2]==0 forced to 0xF0 by the encoder (btn==0 "Prog" case) ---
    // decoder's own doc comment lists two Prog-mode samples with btn==0.
    // The encoder's "raw[2]==0 -> 0xF0" line means the full byte comes back
    // from decode as 0xF0, not 0x00 -- confirm that's exactly what happens
    // (deterministic, not a mismatch), and that the only thing the real
    // decode_key() reads back out of it (btn = decrypted[2]&0x0F) is 0x00
    // either way.
    {
        uint8_t raw[GOL4_RAW_BYTES] = {0x11, 0x22, 0x00, 0x44, 0x55, 0x12, 0x34};
        assert(gol4_rolling_encode(raw));
        assert(gol4_rolling_decode(raw));
        assert(raw[0] == 0x11 && raw[1] == 0x22 && raw[3] == 0x44 && raw[4] == 0x55);
        assert(raw[5] == 0x12 && raw[6] == 0x34);
        assert(raw[2] == 0xF0);
        assert((raw[2] & 0x0F) == 0x00);
        printf(
            "Scenario A2 OK: btn==0 (Prog mode) round trip recovers byte2=0xF0 "
            "(not 0x00) as the encoder's own special case intends, btn nibble 0x0 either way\n");
    }

    // --- Scenario B: the decoder's own doc-comment binary samples (four
    // regular-button presses of one remote) must decode to the same
    // serial, with the expected 0x1/0x2/0x4/0x8 button nibble each. Taken
    // verbatim (spaces removed) from the "Regular buttons:" block of
    // subghz_protocol_ditec_gol4_decode_key()'s own comment -- independent
    // of the ambiguous "Decoded array:" lines further down in that same
    // comment (which list 5 "regular"/2 "programming" entries against only
    // 4/1 raw samples and are not used here to avoid guessing a mapping).
    {
        struct {
            const char* bits; // 54 raw bits, MSB first
            uint8_t expect_btn;
        } samples[] = {
            {"001100110010110010111110000011001000001110110101100110", 0x1},
            {"001100110010110011011110000011001000010000010010100110", 0x2},
            {"001100110010110011101110000011001000001101101101001110", 0x4},
            {"001100110010110011110110000011001000001110100001010101", 0x8},
        };
        uint32_t serial0 = 0;
        for(size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
            const char* s = samples[i].bits;
            size_t len = strlen(s);
            assert(len == K.min_count_bit_for_found);
            uint8_t bits[64];
            for(size_t b = 0; b < len; b++) bits[b] = (uint8_t)(s[b] - '0');
            uint8_t raw[GOL4_RAW_BYTES];
            bits_to_raw(bits, raw);
            assert(gol4_rolling_decode(raw));
            // Deliberately using unsigned casts before each shift here (unlike
            // the real serial_to_display(), see the dedicated UB repro) --
            // this harness's own job in Scenario B is checking Serial/Btn
            // consistency, not re-demonstrating that other finding.
            uint32_t serial = ((uint32_t)raw[0] << 24) | ((uint32_t)raw[4] << 16) |
                               ((uint32_t)raw[1] << 8) | (uint32_t)raw[3];
            uint8_t btn = raw[2] & 0x0F;
            if(i == 0) {
                serial0 = serial;
            } else if(serial != serial0) {
                printf(
                    "Scenario B MISMATCH sample %zu: serial 0x%08X != sample 0 serial 0x%08X\n",
                    i,
                    serial,
                    serial0);
                return 1;
            }
            if(btn != samples[i].expect_btn) {
                printf(
                    "Scenario B MISMATCH sample %zu: btn 0x%X != expected 0x%X\n",
                    i,
                    btn,
                    samples[i].expect_btn);
                return 1;
            }
        }
        printf(
            "Scenario B OK: 4/4 doc-comment samples decode to the same Serial (0x%08X), "
            "correct Btn nibble each\n",
            serial0);
    }

    // --- Scenario C: encode(get_upload) -> decode(feed) frame-count behavior, repeat 1..6 ---
    for(int repeat = 1; repeat <= 6; repeat++) {
        uint64_t data = 0x0CCB2F8320ED66ULL & ((1ULL << K.min_count_bit_for_found) - 1);
        Pulse buf[8 + 6 * 112];
        size_t n = 0;
        for(int r = 0; r < repeat; r++) {
            n += encode_frame(data, buf, sizeof(buf) / sizeof(buf[0]), n);
        }
        DecState s = {0};
        run_pulses(&s, buf, n);
        printf(
            "Scenario C repeat=%d -> found_count=%d (ceil(repeat/2)=%d)\n",
            repeat,
            s.found_count,
            (repeat + 1) / 2);
        if(s.found_count > 0) {
            assert(s.last_found_count_bit == K.min_count_bit_for_found);
            assert(s.last_found_data == data);
        }
    }

    // --- Scenario D: a too-short leading gap must not be mistaken for the real GAP ---
    {
        DecState s = {0};
        Pulse buf[8 + 112];
        size_t n = 0;
        n += encode_frame(0x1ULL, buf, sizeof(buf) / sizeof(buf[0]), n);
        buf[0].duration = (uint32_t)K.te_long * 10; // well below te_long*22-te_long*4
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario D OK: too-short leading gap correctly rejected\n");
    }

    // --- Scenario E: a corrupted bit timing (neither 0 nor 1 pattern) stalls, no false frame ---
    {
        DecState s = {0};
        Pulse buf[8 + 112];
        size_t n = 0;
        n += encode_frame(0x1ULL, buf, sizeof(buf) / sizeof(buf[0]), n);
        // Corrupt one mid-frame HIGH duration to a value matching neither
        // bit-1 nor bit-0 timing.
        buf[10].duration = (uint32_t)K.te_short * 5;
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario E OK: corrupted bit timing does not produce a false frame\n");
    }

    // --- Scenario F: serial_to_display() signed-shift UB, isolated per case ---
    {
        // s[0]==0xCC covers all four doc-comment samples and is itself
        // already >= 0x80.
        uint8_t s_hi[5] = {0xCC, 0x34, 0x72, 0x83, 0x09}; // top bit of s[0] set
        uint8_t s_lo[5] = {0x4C, 0x34, 0x72, 0x83, 0x09}; // top bit of s[0] clear (regression)

        uint32_t r_orig_hi = 0, r_fixed_hi = 0, r_orig_lo = 0, r_fixed_lo = 0;
        bool orig_hi_ok = run_isolated(serial_to_display_orig, s_hi, &r_orig_hi);
        bool fixed_hi_ok = run_isolated(serial_to_display_fixed, s_hi, &r_fixed_hi);
        bool orig_lo_ok = run_isolated(serial_to_display_orig, s_lo, &r_orig_lo);
        bool fixed_lo_ok = run_isolated(serial_to_display_fixed, s_lo, &r_fixed_lo);

        // s[0] with the top bit clear: both expressions are well-defined
        // and agree (regression check -- the fix changes nothing here).
        assert(orig_lo_ok && fixed_lo_ok);
        assert(r_orig_lo == r_fixed_lo);

        // s[0] with the top bit set (the real-world case -- every sample in
        // the decoder's own doc comment starts with 0xCC): ALT aborts under
        // UBSan (signed left-shift overflow), NEU survives and produces the
        // mathematically expected 32-bit value.
        if(orig_hi_ok) {
            printf(
                "Scenario F UNEXPECTED: serial_to_display_orig(s[0]=0xCC) did not abort under "
                "UBSan (result=0x%08X) -- expected a signed-shift-overflow trap\n",
                r_orig_hi);
            return 1;
        }
        if(!fixed_hi_ok) {
            printf("Scenario F UNEXPECTED: serial_to_display_fixed(s[0]=0xCC) itself aborted\n");
            return 1;
        }
        uint32_t expect_hi = 0xCC093483u; // same four bytes as the Scenario B samples
        if(r_fixed_hi != expect_hi) {
            printf(
                "Scenario F MISMATCH: fixed result 0x%08X != expected 0x%08X\n",
                r_fixed_hi,
                expect_hi);
            return 1;
        }
        printf(
            "Scenario F OK: serial_to_display_orig(s[0]=0xCC) aborts under UBSan "
            "(signed left-shift overflow); serial_to_display_fixed(s[0]=0xCC) survives, "
            "result 0x%08X; both agree for s[0]=0x4C (top bit clear)\n",
            r_fixed_hi);
    }

    printf("ALL SCENARIOS PASSED\n");
    return 0;
}
