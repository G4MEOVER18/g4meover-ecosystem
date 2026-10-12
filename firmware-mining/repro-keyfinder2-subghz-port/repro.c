// Host repro for the KeyFinder2 SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-timing lines from the ported
// lib/subghz/protocols/keyfinder2.c (encoder get_upload(), decoder feed()
// five-state machine) and exercises them without any Flipper/furi headers.
//
// KeyFinder2 is a plain static (non-rolling-code) protocol: btn(3 wide bits)
// | "10011"(5 narrow bits, unchecked) | btn(3 narrow bits), 11 bits total.
// Unlike every other static protocol I have ported so far from this same
// Unleashed dev-diff batch (Doorbell32, Ditec GOL4, Allstar Firefly, Nord
// ICE, Treadmill37), its Reset state recognizes the very first edge of a
// frame directly (a HIGH pulse matching the wide short/long template) --
// it needs no preceding guard/gap pulse to sync on, and it has no
// `last_data == decode_data` cross-frame confirmation before firing. Net
// effect, verified below: every single repeat decodes starting with the
// very first one (no warm-up loss), but also nothing here ever cross-checks
// two repeats against each other before reporting -- a lone, correctly
// timed 11-bit burst is accepted on its own. Both properties are the real,
// unmodified upstream design (verified against the actual keyfinder2.c:
// no `last_data` field, no `te` field, fires on
// `decode_count_bit == min_count_bit_for_found` alone); nothing was changed
// porting it, this just confirms and documents the contrast.
//
// Scenarios:
//  1. Encode (get_upload-style bit->timing expansion) -> decode (feed()
//     state machine) round trip over 20000 random 11-bit payloads, each
//     sent back to back for `repeat` in [1..6]. found_count must equal
//     repeat exactly (every repeat decodes, including the very first),
//     and every decode must be bit-exact.
//  2. Negative: a corrupted bit timing in the wide header portion aborts
//     the current frame back to Reset without a false decode, and does not
//     prevent the very next, well-formed frame from decoding (no priming
//     needed to resync, unlike the GUARD-based protocols).
//  3. Negative: same, but the corrupted bit is in the narrow trailer
//     portion instead of the wide header.
//  4. Header/narrow boundary: the state machine switches from
//     wide-bit-pair decoding (SaveWide/CheckWide) to narrow-bit-pair
//     decoding (SaveDuration/CheckDuration) exactly after the 3rd bit,
//     never earlier or later, checked by probing decoder step directly
//     after each of the 11 bits of one frame.
//  5. Upload buffer sizing: get_upload() never writes past the 32-slot
//     encoder buffer for the protocol's fixed 11-bit frame (22 slots used).

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
    .te_short = 350,
    .te_long = 1020,
    .te_delta = 150,
    .min_count_bit_for_found = 11,
};
#define WIDE_SHORT 1500u
#define WIDE_LONG  2500u
#define WIDE_DELTA 350u
#define HEADER_BITS 3u

// ---- Encoder: mirrors subghz_protocol_encoder_keyfinder2_get_upload() verbatim. ----
static size_t encode(uint16_t data, uint8_t data_count_bit, Pulse* out, size_t max_out) {
    size_t index = 0;
    for(uint8_t i = data_count_bit; i > 0; i--) {
        bool wide = (i > data_count_bit - HEADER_BITS);
        uint32_t te_short = wide ? WIDE_SHORT : K.te_short;
        uint32_t te_long = wide ? WIDE_LONG : K.te_long;

        assert(index + 2 <= max_out);
        if(bit_read(data, i - 1)) {
            out[index++] = (Pulse){true, te_short};
            out[index++] = (Pulse){false, te_long};
        } else {
            out[index++] = (Pulse){true, te_long};
            out[index++] = (Pulse){false, te_short};
        }
    }
    return index;
}

// ---- Decoder state machine: mirrors subghz_protocol_decoder_keyfinder2_feed()/_reset()
// verbatim. ----
typedef enum { StepReset = 0, StepSaveWide, StepCheckWide, StepSaveDuration, StepCheckDuration } Step;

typedef struct {
    Step step;
    uint32_t te_last;
    uint64_t decode_data;
    uint8_t decode_count_bit;

    // test instrumentation
    int found_count;
    uint64_t last_found;
} Decoder;

static void reset_decoder(Decoder* d) {
    d->step = StepReset;
}

static void add_bit(Decoder* d, uint8_t bit) {
    d->decode_data = (d->decode_data << 1) | bit;
    d->decode_count_bit++;
}

static void feed(Decoder* d, bool level, uint32_t duration) {
    switch(d->step) {
    case StepReset:
        if((level) &&
           ((DURATION_DIFF(duration, WIDE_SHORT) < WIDE_DELTA) ||
            (DURATION_DIFF(duration, WIDE_LONG) < WIDE_DELTA))) {
            d->decode_data = 0;
            d->decode_count_bit = 0;
            d->te_last = duration;
            d->step = StepCheckWide;
        }
        break;
    case StepSaveWide:
        if(level) {
            d->te_last = duration;
            d->step = StepCheckWide;
        } else {
            d->step = StepReset;
        }
        break;
    case StepCheckWide:
        if(!level) {
            if((DURATION_DIFF(d->te_last, WIDE_SHORT) < WIDE_DELTA) &&
               (DURATION_DIFF(duration, WIDE_LONG) < WIDE_DELTA)) {
                add_bit(d, 1);
            } else if(
                (DURATION_DIFF(d->te_last, WIDE_LONG) < WIDE_DELTA) &&
                (DURATION_DIFF(duration, WIDE_SHORT) < WIDE_DELTA)) {
                add_bit(d, 0);
            } else {
                d->step = StepReset;
                break;
            }
            d->step = (d->decode_count_bit == HEADER_BITS) ? StepSaveDuration : StepSaveWide;
        } else {
            d->step = StepReset;
        }
        break;
    case StepSaveDuration:
        if(level) {
            d->te_last = duration;
            d->step = StepCheckDuration;
        } else {
            d->step = StepReset;
        }
        break;
    case StepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(d->te_last, K.te_short) < K.te_delta) &&
               (DURATION_DIFF(duration, K.te_long) < K.te_delta)) {
                add_bit(d, 1);
            } else if(
                (DURATION_DIFF(d->te_last, K.te_long) < K.te_delta) &&
                (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                add_bit(d, 0);
            } else {
                d->step = StepReset;
                break;
            }

            if(d->decode_count_bit == K.min_count_bit_for_found) {
                d->found_count++;
                d->last_found = d->decode_data;
                d->step = StepReset;
            } else {
                d->step = StepSaveDuration;
            }
        } else {
            d->step = StepReset;
        }
        break;
    }
}

static void feed_pulses(Decoder* d, const Pulse* p, size_t count) {
    for(size_t i = 0; i < count; i++) feed(d, p[i].level, p[i].duration);
}

static uint32_t xorshift32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

int main(void) {
    uint32_t seed = 0x2F19Du;
    int checks = 0;

    // --- Scenario 1: round trip over random payloads, repeat 1..6, every
    // repeat must decode (no warm-up loss), bit-exact. ---
    for(int trial = 0; trial < 20000; trial++) {
        uint16_t data = (uint16_t)(xorshift32(&seed) & 0x7FF); // 11 bits
        int repeat = 1 + (int)(xorshift32(&seed) % 6);

        Pulse frame[32];
        size_t frame_len = encode(data, K.min_count_bit_for_found, frame, 32);
        assert(frame_len == 22); // 11 bits * 2 pulses
        checks++;

        Decoder d = {0};
        reset_decoder(&d);
        for(int r = 0; r < repeat; r++) feed_pulses(&d, frame, frame_len);

        if(d.found_count != repeat) {
            fprintf(
                stderr,
                "FAIL scenario1: repeat=%d data=0x%03X found=%d expected=%d\n",
                repeat,
                data,
                d.found_count,
                repeat);
            return 1;
        }
        checks++;
        if(d.last_found != data) {
            fprintf(stderr, "FAIL scenario1 bit-exactness: data=0x%03X\n", data);
            return 1;
        }
        checks++;

        // Single, un-repeated frame with nothing before or after it must also
        // decode on its own -- no priming, unlike the GUARD-synced siblings.
        Decoder d_single = {0};
        reset_decoder(&d_single);
        feed_pulses(&d_single, frame, frame_len);
        if(d_single.found_count != 1 || d_single.last_found != data) {
            fprintf(stderr, "FAIL scenario1 single-frame: data=0x%03X\n", data);
            return 1;
        }
        checks++;
    }

    // --- Scenario 2: corrupted bit in the WIDE header aborts to Reset without
    // a false decode, and the very next well-formed frame still decodes
    // (no resync priming needed). ---
    {
        uint16_t data = 0x6B5u; // 101 10110 101 pattern, exercises both bit values
        Pulse frame[32];
        size_t len = encode(data, K.min_count_bit_for_found, frame, 32);

        Pulse corrupted[32];
        memcpy(corrupted, frame, sizeof(frame));
        // Pulse pair index 2 (0-based) is the third and last of the 3 wide
        // header bits: the encode() loop emits pairs 0,1,2 for i=11,10,9,
        // all wide since i > data_count_bit - HEADER_BITS (11-3=8) holds for
        // each of them.
        corrupted[2 * 2].duration = (WIDE_SHORT + WIDE_LONG) / 2; // ambiguous

        Decoder d = {0};
        reset_decoder(&d);
        feed_pulses(&d, corrupted, len);
        if(d.found_count != 0) {
            fprintf(stderr, "FAIL scenario2: corrupted header falsely decoded\n");
            return 1;
        }
        checks++;
        feed_pulses(&d, frame, len);
        if(d.found_count != 1 || d.last_found != data) {
            fprintf(stderr, "FAIL scenario2: recovery after header corruption failed\n");
            return 1;
        }
        checks++;
    }

    // --- Scenario 3: same, but the corruption is in the narrow trailer. ---
    {
        uint16_t data = 0x3A7u;
        Pulse frame[32];
        size_t len = encode(data, K.min_count_bit_for_found, frame, 32);

        Pulse corrupted[32];
        memcpy(corrupted, frame, sizeof(frame));
        // Pulse pair index 7 falls within the narrow trailer range (pairs
        // 3-10, i.e. i=8..1), well past the 3-bit wide header (pairs 0-2).
        corrupted[7 * 2].duration = (K.te_short + K.te_long) / 2; // ambiguous

        Decoder d = {0};
        reset_decoder(&d);
        feed_pulses(&d, corrupted, len);
        if(d.found_count != 0) {
            fprintf(stderr, "FAIL scenario3: corrupted trailer falsely decoded\n");
            return 1;
        }
        checks++;
        feed_pulses(&d, frame, len);
        if(d.found_count != 1 || d.last_found != data) {
            fprintf(stderr, "FAIL scenario3: recovery after trailer corruption failed\n");
            return 1;
        }
        checks++;
    }

    // --- Scenario 4: the wide/narrow mode switch happens exactly after the
    // 3rd decoded bit, never earlier or later. ---
    {
        uint16_t data = 0x555u;
        Pulse frame[32];
        encode(data, K.min_count_bit_for_found, frame, 32);

        Decoder d = {0};
        reset_decoder(&d);
        for(uint8_t bit_idx = 1; bit_idx <= 11; bit_idx++) {
            feed(&d, frame[(bit_idx - 1) * 2].level, frame[(bit_idx - 1) * 2].duration);
            feed(&d, frame[(bit_idx - 1) * 2 + 1].level, frame[(bit_idx - 1) * 2 + 1].duration);
            if(bit_idx < 3) {
                if(d.step != StepSaveWide) {
                    fprintf(stderr, "FAIL scenario4: bit %u not in wide mode\n", bit_idx);
                    return 1;
                }
            } else if(bit_idx == 3) {
                if(d.step != StepSaveDuration) {
                    fprintf(stderr, "FAIL scenario4: bit 3 did not switch to narrow mode\n");
                    return 1;
                }
            } else if(bit_idx < 11) {
                if(d.step != StepSaveDuration) {
                    fprintf(stderr, "FAIL scenario4: bit %u left narrow mode early\n", bit_idx);
                    return 1;
                }
            } else {
                if(d.step != StepReset || d.found_count != 1) {
                    fprintf(stderr, "FAIL scenario4: final bit did not complete the frame\n");
                    return 1;
                }
            }
            checks++;
        }
    }

    // --- Scenario 5: upload buffer sizing, 11 bits -> 22 slots, must not
    // exceed the 32-slot encoder buffer (checked via the assert in encode()
    // itself, plus an explicit bound here). ---
    {
        Pulse frame[32];
        size_t len = encode(0x7FFu, K.min_count_bit_for_found, frame, 32);
        if(len != 22 || len > 32) {
            fprintf(stderr, "FAIL scenario5: unexpected upload length %zu\n", len);
            return 1;
        }
        checks++;
    }

    printf("%d checks passed -- ALL CHECKS PASSED\n", checks);
    return 0;
}
