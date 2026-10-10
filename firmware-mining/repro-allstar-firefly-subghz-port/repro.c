// Host repro for the Allstar Firefly SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-timing lines from the ported
// lib/subghz/protocols/allstar_firefly.c (encoder get_upload(), decoder
// feed() state machine, get_string()'s DIP-switch decoding) and exercises
// them without any Flipper/furi headers. Allstar Firefly is a plain static
// (non-rolling-code) 18-bit protocol with no cipher -- this repro is purely
// about the encode<->decode bit-timing round trip and the trinary
// (+/0/-) DIP-switch display decoding.
//
// Scenarios:
//  1. The upstream header's own DIP save-file example ("Key: +--000++-")
//     decoded by hand to its 18-bit data value, fed through the same
//     2-bit-group -> '+'/'0'/'-' mapping get_string() uses, must reproduce
//     the same nine-character pattern.
//  2. Encode (get_upload-style bit->timing expansion) -> decode (feed()
//     state machine) round trip over 20000 random 18-bit data values,
//     repeated 1-5 times back-to-back (mirrors the real encoder.repeat
//     loop, default 5). The single trailing ~30000us gap of each repeat
//     does double duty (ends the current key AND arms Reset for the next
//     one), so a back-to-back burst only *completes* a decode on every
//     other repeat (repeat 1, then 3, 5, ...): the repeat in between only
//     re-arms Reset without being decoded itself, since its own bits are
//     scanned while still in Reset (ignored) until its own trailing gap is
//     reached. Same mechanism already found and documented for Nord ICE
//     and Treadmill37 -- unchanged upstream feed() behavior, not something
//     this port introduces -- checked here as exactly
//     found_count == ceil(repeat/2), with every completed decode
//     bit-exact to the encoded data.
//  3. Negative cases: a short trailing gap (not within te_delta*5 of
//     te_short*50) must not be mistaken for the end-of-key marker; a
//     corrupted bit timing (neither the long/short nor short/long pattern)
//     must abort the current key back to Reset without emitting a short
//     frame.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define bit_read(value, b) (((value) >> (b)) & 0x01)
#define DURATION_DIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

typedef struct {
    bool level;
    uint32_t duration;
} Pulse;

typedef struct {
    uint16_t te_short;
    uint16_t te_long;
    uint16_t te_delta;
    uint8_t min_count_bit_for_found;
} Const;

static const Const K = {
    .te_short = 600,
    .te_long = 4000,
    .te_delta = 300,
    .min_count_bit_for_found = 18,
};

#define DIP_P 0b11 //(+)
#define DIP_O 0b10 //(0)
#define DIP_N 0b00 //(-)

// --- Transcribed 1:1 from the ported get_string()'s SHOW_DIP_P logic ---
// Returns '+', '0', '-' for a valid 2-bit group (same three characters the
// "Key: +--000++-" save-file convention uses), '?' for the fourth,
// never-encoded combination (0b01), exactly like three independent
// SHOW_DIP_P passes would (each row only lights up on its own check_dip
// value, so a 0b01 group would print '_' on *all three* rows -- this
// helper collapses that into one '?' character for easier assertions).
static char dip_char(uint8_t group) {
    if(group == DIP_P) return '+';
    if(group == DIP_O) return '0';
    if(group == DIP_N) return '-';
    return '?';
}

// Decodes all nine 2-bit groups (MSB-first, shifts 0x10..0x0) of an 18-bit
// value into the nine-character pattern get_string() would print.
static void decode_dip_pattern(uint64_t data, char out[10]) {
    int shifts[9] = {0x10, 0xE, 0xC, 0xA, 0x8, 0x6, 0x4, 0x2, 0x0};
    for(int i = 0; i < 9; i++) {
        out[i] = dip_char((uint8_t)((data >> shifts[i]) & 0x3));
    }
    out[9] = '\0';
}

// --- Transcribed 1:1 from the ported subghz_protocol_encoder_allstar_firefly_get_upload() ---
// Appends the pulses for one key (data, count_bit bits, MSB first) to buf,
// returns the number of pulses appended.
static size_t
    encode_key(uint64_t data, uint8_t count_bit, Pulse* buf, size_t buf_cap, size_t start_index) {
    size_t index = start_index;
    for(uint8_t i = count_bit; i > 0; i--) {
        assert(index + 2 <= buf_cap);
        if(bit_read(data, i - 1)) {
            buf[index++] = (Pulse){true, (uint32_t)K.te_long};
            if(i == 1) {
                buf[index++] = (Pulse){false, (uint32_t)(K.te_short * 50) + 400};
            } else {
                buf[index++] = (Pulse){false, (uint32_t)K.te_short};
            }
        } else {
            buf[index++] = (Pulse){true, (uint32_t)K.te_short};
            if(i == 1) {
                buf[index++] = (Pulse){false, (uint32_t)(K.te_short * 50) + 400};
            } else {
                buf[index++] = (Pulse){false, (uint32_t)K.te_long};
            }
        }
    }
    return index - start_index;
}

// --- Transcribed 1:1 from the ported decoder feed() state machine ---
typedef enum {
    StepReset = 0,
    StepSaveDuration,
    StepCheckDuration,
} Step;

typedef struct {
    Step parser_step;
    uint32_t te_last;
    uint64_t decode_data;
    uint8_t decode_count_bit;

    // "found" sink, mirrors generic.data/data_count_bit + the base.callback hook
    int found_count;
    uint64_t last_found_data;
    uint8_t last_found_count_bit;
} DecState;

static void decoder_feed(DecState* s, bool level, uint32_t duration) {
    switch(s->parser_step) {
    case StepReset:
        if((!level) && (DURATION_DIFF(duration, K.te_short * 50) < K.te_delta * 5)) {
            s->decode_data = 0;
            s->decode_count_bit = 0;
            s->parser_step = StepSaveDuration;
        }
        break;
    case StepSaveDuration:
        if(level) {
            s->te_last = duration;
            s->parser_step = StepCheckDuration;
        } else {
            s->parser_step = StepReset;
        }
        break;
    case StepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) &&
               (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                s->decode_data = s->decode_data << 1 | 1;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(s->te_last, K.te_short) < K.te_delta) &&
                (DURATION_DIFF(duration, K.te_long) < K.te_delta)) {
                s->decode_data = s->decode_data << 1 | 0;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else if(DURATION_DIFF(duration, K.te_short * 50) < K.te_delta * 5) {
                if(DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) {
                    s->decode_data = s->decode_data << 1 | 1;
                    s->decode_count_bit++;
                }
                if(DURATION_DIFF(s->te_last, K.te_short) < K.te_delta) {
                    s->decode_data = s->decode_data << 1 | 0;
                    s->decode_count_bit++;
                }
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
        } else {
            s->parser_step = StepReset;
        }
        break;
    }
}

static void run_pulses(DecState* s, const Pulse* p, size_t n) {
    for(size_t i = 0; i < n; i++) {
        decoder_feed(s, p[i].level, p[i].duration);
    }
}

static uint32_t rnd_state = 0xC0FFEEu;
static uint32_t xrand(void) {
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return rnd_state;
}

int main(void) {
    // --- Scenario 1: upstream header's own DIP save-file example ---
    // allstar_firefly.h: `Key: +--000++-` (9 trinary symbols, MSB-first).
    // Hand-decoded via the same '+' =0b11 / '0' =0b10 / '-' =0b00 mapping
    // the decoder's DIP_P/DIP_O/DIP_N macros use, packed MSB-first into an
    // 18-bit value: +=11,-=00,-=00,0=10,0=10,0=10,+=11,+=11,-=00
    // -> 0b110000101010111100 = 0x30ABC.
    {
        uint64_t data = 0x30ABC;
        assert(data < (1ULL << 18));
        char pattern[10];
        decode_dip_pattern(data, pattern);
        assert(strcmp(pattern, "+--000++-") == 0);
        printf("Scenario 1 OK: header DIP example 0x30ABC decodes back to \"+--000++-\"\n");
    }

    // --- Scenario 2: encode -> decode round trip, repeat 1..5x, 20000 random keys ---
    int trials = 20000;
    int total_repeats_checked = 0;
    for(int t = 0; t < trials; t++) {
        uint64_t data = xrand() & 0x3FFFFU; // 18-bit random value
        int repeat = 1 + (int)(xrand() % 5);

        Pulse buf[8 + 5 * 40];
        size_t n = 0;
        // Leading idle-low gap, as a real capture would have before the
        // first repeat's framing gap is recognized.
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 50};
        for(int r = 0; r < repeat; r++) {
            n += encode_key(data, K.min_count_bit_for_found, buf, sizeof(buf) / sizeof(buf[0]), n);
        }

        DecState s = {0};
        run_pulses(&s, buf, n);

        int expected_found = (repeat + 1) / 2; // ceil(repeat / 2)
        assert(s.found_count == expected_found);
        assert(s.last_found_count_bit == K.min_count_bit_for_found);
        assert(s.last_found_data == data);
        total_repeats_checked += expected_found;
    }
    printf(
        "Scenario 2 OK: %d trials, %d total repeats decoded, all bit-exact\n",
        trials,
        total_repeats_checked);

    // --- Scenario 3a: a too-short trailing gap must not be read as end-of-key ---
    {
        DecState s = {0};
        Pulse buf[8 + 40];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 50};
        n += encode_key(0x30ABC, 18, buf, sizeof(buf) / sizeof(buf[0]), n);
        // Corrupt the final gap duration to something clearly too short
        // (half the threshold) -- same bit pattern, broken framing.
        buf[n - 1].duration = (uint32_t)K.te_short * 25;
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario 3a OK: short trailing gap correctly rejected as end-of-key\n");
    }

    // --- Scenario 3b: a bit pulse pair matching neither 0 nor 1 timing aborts cleanly ---
    {
        DecState s = {0};
        Pulse buf[8 + 40];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 50};
        n += encode_key(0x30ABC, 18, buf, sizeof(buf) / sizeof(buf[0]), n);
        // Corrupt one interior bit's HIGH duration (buf[5], the 3rd bit's
        // HIGH half -- buf[0] is the leading gap, then HIGH/LOW pairs) to a
        // value matching neither te_short nor te_long (a garbled
        // over-the-air bit). te_short*4=2400 sits outside both +-te_delta
        // windows (600/4000 +-300).
        buf[5].duration = (uint32_t)K.te_short * 4;
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario 3b OK: garbled bit timing aborts the key, no false decode\n");
    }

    printf("ALL CHECKS PASSED\n");
    return 0;
}
