// Host repro for the Nord ICE SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-timing lines from the ported
// lib/subghz/protocols/nord_ice.c (encoder get_upload(), decoder feed()
// state machine, check_remote_controller()) and exercises them without any
// Flipper/furi headers. Nord ICE is a plain static (non-rolling-code)
// protocol -- no cipher is involved, so this repro is purely about the
// encode<->decode bit-timing round trip and the serial/button field
// extraction, plus the four real remote-button samples quoted in the
// upstream decoder's own header comment.
//
// Scenarios:
//  1. Four real samples from the decoder's own doc comment (0x9467688A..
//     0x9467648A, buttons 1-4 of one remote): check_remote_controller()
//     must extract the same Serial for all four and the expected raw 6-bit
//     Btn field for each.
//  2. Encode (get_upload-style bit->timing expansion) -> decode (feed()
//     state machine) round trip over 20000 random 33-bit data values,
//     repeated 1-5 times back-to-back (mirrors the real encoder.repeat
//     loop). The single trailing 7500us gap of each repeat does double
//     duty (ends the current key AND arms Reset for the next one), so a
//     back-to-back burst only *completes* a decode on every other
//     repeat (repeat 1, then 3, 5, ...): the repeat in between only
//     re-arms Reset without being decoded itself, since nothing after
//     its own trailing gap feeds Reset's "is this a gap" check before
//     the following repeat's bits start arriving as plain HIGH/LOW
//     pairs. This is unchanged upstream feed() behavior, not something
//     this port introduces -- checked here as exactly
//     found_count == ceil(repeat/2), with every completed decode
//     bit-exact to the encoded data.
//  3. Negative cases: a short gap (not >= te_short*25-te_delta*11) must
//     not be mistaken for the end-of-key marker; a corrupted bit timing
//     (neither the short/long nor long/short pattern) must abort the
//     current key back to Reset without emitting a short frame.

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
    .te_short = 300,
    .te_long = 800,
    .te_delta = 150,
    .min_count_bit_for_found = 33,
};

// --- Transcribed 1:1 from the ported check_remote_controller() ---
static void check_remote_controller(uint64_t data, uint32_t* serial, uint8_t* btn) {
    *serial = (uint32_t)((data >> 15) << 9 | (data & 0x1FF));
    *btn = (uint8_t)((data >> 9) & 0x3F);
}

// --- Transcribed 1:1 from the ported subghz_protocol_encoder_nord_ice_get_upload() ---
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
                buf[index++] = (Pulse){false, (uint32_t)K.te_short * 25};
            } else {
                buf[index++] = (Pulse){false, (uint32_t)K.te_short};
            }
        } else {
            buf[index++] = (Pulse){true, (uint32_t)K.te_short};
            if(i == 1) {
                buf[index++] = (Pulse){false, (uint32_t)K.te_short * 25};
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
        if((!level) && (DURATION_DIFF(duration, K.te_short * 25) < K.te_delta * 11)) {
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
            if((DURATION_DIFF(s->te_last, K.te_short) < K.te_delta) &&
               (DURATION_DIFF(duration, K.te_long) < K.te_delta)) {
                s->decode_data = s->decode_data << 1 | 0;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) &&
                (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                s->decode_data = s->decode_data << 1 | 1;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else if(DURATION_DIFF(duration, K.te_short * 25) < K.te_delta * 11) {
                if(DURATION_DIFF(s->te_last, K.te_short) < K.te_delta) {
                    s->decode_data = s->decode_data << 1 | 0;
                    s->decode_count_bit++;
                }
                if(DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) {
                    s->decode_data = s->decode_data << 1 | 1;
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
    // --- Scenario 1: real remote samples from the upstream doc comment ---
    // 0x9467688A btn 1 = 10010100011001110 110100 010001010
    // 0x9467308A btn 2 = 10010100011001110 011000 010001010
    // 0x9467628A btn 3 = 10010100011001110 110001 010001010
    // 0x9467648A btn 4 = 10010100011001110 110010 010001010
    struct {
        uint64_t data;
        uint8_t expect_btn;
    } samples[] = {
        {0x9467688A, 0x34},
        {0x9467308A, 0x18},
        {0x9467628A, 0x31},
        {0x9467648A, 0x32},
    };
    uint32_t sample_serial[4];
    for(int i = 0; i < 4; i++) {
        uint32_t serial;
        uint8_t btn;
        check_remote_controller(samples[i].data, &serial, &btn);
        assert(btn == samples[i].expect_btn);
        sample_serial[i] = serial;
    }
    for(int i = 1; i < 4; i++) {
        assert(sample_serial[i] == sample_serial[0]);
    }
    printf("Scenario 1 OK: 4/4 real samples -- same Serial, correct raw Btn field each\n");

    // --- Scenario 2: encode -> decode round trip, repeat 1..5x, 20000 random keys ---
    int trials = 20000;
    int total_repeats_checked = 0;
    for(int t = 0; t < trials; t++) {
        uint64_t data = ((uint64_t)xrand() << 1) & 0x1FFFFFFFFULL; // 33-bit random value
        int repeat = 1 + (int)(xrand() % 5);

        Pulse buf[8 + 5 * 66];
        size_t n = 0;
        // Leading idle-low gap, as a real capture would have before the
        // first repeat's framing gap is recognized.
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 25};
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
        Pulse buf[8 + 66];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 25};
        n += encode_key(0x9467688A, 33, buf, sizeof(buf) / sizeof(buf[0]), n);
        // Corrupt the final gap duration to something clearly too short
        // (half the threshold) -- same bit pattern, broken framing.
        buf[n - 1].duration = (uint32_t)K.te_short * 10;
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario 3a OK: short trailing gap correctly rejected as end-of-key\n");
    }

    // --- Scenario 3b: a bit pulse pair matching neither 0 nor 1 timing aborts cleanly ---
    {
        DecState s = {0};
        Pulse buf[8 + 66];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 25};
        n += encode_key(0x9467688A, 33, buf, sizeof(buf) / sizeof(buf[0]), n);
        // Corrupt one interior bit's HIGH duration (buf[9], the 5th bit's
        // HIGH half -- buf[0] is the leading gap, then HIGH/LOW pairs) to a
        // value matching neither te_short nor te_long (a garbled
        // over-the-air bit). te_short*5 sits outside both +-te_delta
        // windows (300/800 +-150), unlike te_short*3=900 which would still
        // fall inside the te_long=800 +-150 window.
        buf[9].duration = (uint32_t)K.te_short * 5;
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario 3b OK: garbled bit timing aborts the key, no false decode\n");
    }

    printf("ALL CHECKS PASSED\n");
    return 0;
}
