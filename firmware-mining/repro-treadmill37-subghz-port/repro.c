// Host repro for the Treadmill37 SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-timing lines from the ported
// lib/subghz/protocols/treadmill37.c (encoder get_upload(), decoder feed()
// state machine, check_remote_controller()) and exercises them without any
// Flipper/furi headers. Treadmill37 is a plain static (non-rolling-code)
// protocol -- no cipher is involved, so this repro is purely about the
// encode<->decode bit-timing round trip and the serial/button field
// extraction, plus the six real remote samples quoted in the upstream
// decoder's own header comment.
//
// Scenarios:
//  1. Six real samples from the decoder's own doc comment (0x1800001830 ..
//     0x180001334A): check_remote_controller() must extract the documented
//     serial/button split -- same 20-bit serial for all six, the expected
//     16-bit button field for each, bit 0 (the trailing "stop" bit) always
//     0. Also confirms the upstream sample comment's bit-width labelling
//     is off by 3 bits (shows a 23-char serial block, i.e. treats the
//     value as 40 bit, while min_count_bit_for_found=37 and
//     check_remote_controller()'s ">> 17" only ever yields a 20-bit
//     serial) -- a corrected, 37-bit-accurate version of the same six
//     samples is used here and in the ported file's own comment.
//  2. Encode (get_upload-style bit->timing expansion) -> decode (feed()
//     state machine) round trip over 20000 random 37-bit data values,
//     repeated 1-5 times back-to-back (mirrors the real encoder.repeat
//     loop, which replays the same upload buffer with no extra gap
//     between repeats -- see lib/subghz/protocols/common.c
//     subghz_protocol_encoder_common_yield() upstream). The single
//     trailing 6000us gap of each repeat does double duty (ends the
//     current key AND arms Reset for the next one), so a back-to-back
//     burst only *completes* a decode on every other repeat (repeat 1,
//     then 3, 5, ...): the repeat in between only re-arms Reset without
//     being decoded itself. This is unchanged upstream feed() behavior
//     (byte-for-byte identical state machine to the already-ported Nord
//     ICE protocol, which documented the same property as inherent to the
//     shared-GAP framing, not a bug) -- checked here as exactly
//     found_count == ceil(repeat/2), with every completed decode
//     bit-exact to the encoded data.
//  3. Negative cases: a short trailing gap (not >= te_short*20-te_delta*4)
//     must not be mistaken for the end-of-key marker; a corrupted bit
//     timing (neither the short/long nor long/short pattern) must abort
//     the current key back to Reset without emitting a short frame.

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
    .te_long = 900,
    .te_delta = 150,
    .min_count_bit_for_found = 37,
};

// --- Transcribed 1:1 from the ported check_remote_controller() ---
static void check_remote_controller(uint64_t data, uint32_t* serial, uint32_t* cnt) {
    *serial = (uint32_t)(data >> 17);
    *cnt = (uint32_t)((data >> 1) & 0xFFFF);
}

// --- Transcribed 1:1 from the ported subghz_protocol_encoder_treadmill37_get_upload() ---
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
                buf[index++] = (Pulse){false, (uint32_t)K.te_short * 20};
            } else {
                buf[index++] = (Pulse){false, (uint32_t)K.te_short};
            }
        } else {
            buf[index++] = (Pulse){true, (uint32_t)K.te_short};
            if(i == 1) {
                buf[index++] = (Pulse){false, (uint32_t)K.te_short * 20};
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
        if((!level) && (DURATION_DIFF(duration, K.te_short * 20) < K.te_delta * 4)) {
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
            } else if(DURATION_DIFF(duration, K.te_short * 20) < K.te_delta * 4) {
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
    // --- Scenario 1: real remote samples from the upstream doc comment,
    // re-split against the actual 37-bit width (see file header) ---
    // 1800001830 = 11000000000000000000 0000110000011000 0
    // 180000061E = 11000000000000000000 0000001100001111 0
    // 180000142C = 11000000000000000000 0000101000010110 0
    // 1800000C24 = 11000000000000000000 0000011000010010 0
    // 180001556C = 11000000000000000000 1010101010110110 0
    // 180001334A = 11000000000000000000 1001100110100101 0
    struct {
        uint64_t data;
        uint32_t expect_cnt;
    } samples[] = {
        {0x1800001830ULL, 0x0C18},
        {0x180000061EULL, 0x030F},
        {0x180000142CULL, 0x0A16},
        {0x1800000C24ULL, 0x0612},
        {0x180001556CULL, 0xAAB6},
        {0x180001334AULL, 0x99A5},
    };
    uint32_t sample_serial[6];
    for(int i = 0; i < 6; i++) {
        uint32_t serial, cnt;
        check_remote_controller(samples[i].data, &serial, &cnt);
        assert(cnt == samples[i].expect_cnt);
        assert((samples[i].data & 1) == 0); // documented "stop" bit, always 0
        sample_serial[i] = serial;
        assert(serial <= 0xFFFFF); // 20-bit field (data >> 17 of a 37-bit value)
    }
    for(int i = 1; i < 6; i++) {
        assert(sample_serial[i] == sample_serial[0]);
    }
    printf("Scenario 1 OK: 6/6 real samples -- same 20-bit Serial, correct Btn field each, "
           "stop bit 0\n");

    // --- Scenario 2: encode -> decode round trip, repeat 1..5x, 20000 random keys ---
    int trials = 20000;
    int total_repeats_checked = 0;
    for(int t = 0; t < trials; t++) {
        uint64_t data = ((uint64_t)xrand() << 5 | (xrand() & 0x1F)) & 0x1FFFFFFFFFULL;
        data |= (1ULL << 36); // keep bit_length()==37, like every real sample
        int repeat = 1 + (int)(xrand() % 5);

        Pulse buf[8 + 5 * 74];
        size_t n = 0;
        // Leading idle-low gap, as a real capture would have before the
        // first repeat's framing gap is recognized.
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 20};
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
        Pulse buf[8 + 74];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 20};
        n += encode_key(0x1800001830ULL, 37, buf, sizeof(buf) / sizeof(buf[0]), n);
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
        Pulse buf[8 + 74];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 20};
        n += encode_key(0x1800001830ULL, 37, buf, sizeof(buf) / sizeof(buf[0]), n);
        // Corrupt one interior bit's HIGH duration (buf[9], the 5th bit's
        // HIGH half -- buf[0] is the leading gap, then HIGH/LOW pairs) to a
        // value matching neither te_short nor te_long (a garbled
        // over-the-air bit). te_short*5=1500 sits outside both +-te_delta
        // windows (300/900 +-150).
        buf[9].duration = (uint32_t)K.te_short * 5;
        run_pulses(&s, buf, n);
        assert(s.found_count == 0);
        printf("Scenario 3b OK: garbled bit timing aborts the key, no false decode\n");
    }

    printf("ALL CHECKS PASSED\n");
    return 0;
}
