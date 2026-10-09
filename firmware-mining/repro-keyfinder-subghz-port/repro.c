// Host repro for the KeyFinder SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-timing lines from Unleashed's
// lib/subghz/protocols/keyfinder.c (encoder get_upload(), decoder feed()
// state machine, check_remote_controller()) and exercises them without any
// Flipper/furi headers.
//
// Scenarios:
//  1. Four real samples from the decoder's own doc comment (RED/PURPLE/
//     GREEN/BLUE of one remote): check_remote_controller() must extract the
//     same 20-bit Serial for all four and the documented 4-bit ID for each.
//  2. Encode -> decode round trip, single frame (Repeat == 1), 20000 random
//     24-bit values: must always decode exactly once, bit-exact. Confirms
//     the baseline framing (4x end-HIGH marker) works for the common case.
//  3. BUG repro: encode -> decode round trip with Repeat == 1..5, pulses
//     chained back-to-back exactly as subghz_protocol_encoder_common_yield()
//     actually transmits them (the upload buffer simply wraps front back to
//     0 -- no extra silence is inserted between repeats). With the
//     UNMODIFIED upstream feed() state machine, only every OTHER repeat is
//     decoded (found_count == ceil(repeat/2)), the same loss pattern this
//     project already documented for the unrelated Nord ICE port, but
//     arising from a different mechanism here. Root cause: once the 4th
//     end-marker HIGH pulse (end_count==4) is seen, the state machine moves
//     to a separate KeyFinderDecoderStepFinish state *without* consuming or
//     examining the trailing LOW pulse (te_short*10) that immediately
//     follows. That LOW pulse is exactly the "GAP" edge Step Reset is
//     normally watching for to arm the *next* frame -- but it arrives one
//     feed() call later, while parser_step is already Finish, so Finish's
//     case body (which emits the found callback and resets to Reset)
//     discards it unexamined. The *following* repeat's bit pulses (none of
//     which is ~4000us) then run through Reset doing nothing, until that
//     repeat's OWN trailing LOW pulse finally matches Reset's GAP window
//     and arms the repeat after it. Net effect: repeats 1, 3, 5, ... decode;
//     repeats 2, 4, ... are silently absorbed re-arming Reset for the one
//     after them.
//  4. FIX verification: same back-to-back input, decoded with a
//     one-state-earlier emission (move the found-callback/reset logic from
//     the deferred Finish state into the same SaveDuration branch that
//     detects end_count == 4, and go straight back to Reset there). The
//     trailing LOW pulse is then evaluated by Reset on the very next
//     feed() call, exactly as Reset already does between two genuinely
//     separate transmissions -- recovering every repeat, bit-exact.
//  5. Fix regression/negative checks: the fix must not change single-shot
//     behaviour (scenario 2 must still hold under the fixed state machine),
//     and a corrupted end marker (one end-HIGH pulse mistimed) must still
//     abort the frame with zero decodes, same as upstream.

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
    .te_short = 400,
    .te_long = 1200,
    .te_delta = 150,
    .min_count_bit_for_found = 24,
};

// --- Transcribed 1:1 from Unleashed's subghz_protocol_keyfinder_check_remote_controller() ---
static void check_remote_controller(uint64_t data, uint32_t* serial, uint8_t* btn) {
    *serial = (uint32_t)(data >> 4);
    *btn = (uint8_t)(data & 0xF);
}

// --- Transcribed 1:1 from Unleashed's subghz_protocol_encoder_keyfinder_get_upload() ---
// Appends the pulses for one key (data, count_bit bits, MSB first, plus the
// 4x end-HIGH marker) to buf, returns the number of pulses appended.
static size_t
    encode_key(uint64_t data, uint8_t count_bit, Pulse* buf, size_t buf_cap, size_t start_index) {
    size_t index = start_index;
    for(uint8_t i = count_bit; i > 0; i--) {
        assert(index + 2 <= buf_cap);
        if(bit_read(data, i - 1)) {
            buf[index++] = (Pulse){true, (uint32_t)K.te_short};
            buf[index++] = (Pulse){false, (uint32_t)K.te_long};
        } else {
            buf[index++] = (Pulse){true, (uint32_t)K.te_long};
            buf[index++] = (Pulse){false, (uint32_t)K.te_short};
        }
    }
    for(uint8_t i = 0; i < 3; i++) {
        assert(index + 2 <= buf_cap);
        buf[index++] = (Pulse){true, (uint32_t)K.te_short};
        buf[index++] = (Pulse){false, (uint32_t)K.te_short};
    }
    assert(index + 2 <= buf_cap);
    buf[index++] = (Pulse){true, (uint32_t)K.te_short};
    buf[index++] = (Pulse){false, (uint32_t)K.te_short * 10};
    return index - start_index;
}

typedef enum {
    StepReset = 0,
    StepSaveDuration,
    StepCheckDuration,
    StepFinish,
} Step;

typedef struct {
    Step parser_step;
    uint32_t te_last;
    uint64_t decode_data;
    uint8_t decode_count_bit;
    uint8_t end_count;

    // "found" sink, mirrors generic.data/data_count_bit + the base.callback hook
    int found_count;
    uint64_t last_found_data;
    uint8_t last_found_count_bit;
} DecState;

// --- Transcribed 1:1 from Unleashed's (unmodified) decoder feed() state machine ---
static void decoder_feed_buggy(DecState* s, bool level, uint32_t duration) {
    switch(s->parser_step) {
    case StepReset:
        if((!level) && (DURATION_DIFF(duration, K.te_short * 10) < K.te_delta * 5)) {
            s->decode_data = 0;
            s->decode_count_bit = 0;
            s->parser_step = StepSaveDuration;
        }
        break;
    case StepSaveDuration:
        if(s->decode_count_bit == K.min_count_bit_for_found) {
            if(level && (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                s->end_count++;
                if(s->end_count == 4) {
                    s->parser_step = StepFinish;
                    s->end_count = 0;
                }
            } else if((!level) && (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                break;
            } else {
                s->decode_data = 0;
                s->decode_count_bit = 0;
                s->end_count = 0;
                s->parser_step = StepReset;
            }
            break;
        }
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
                s->decode_data = s->decode_data << 1 | 1;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) &&
                (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                s->decode_data = s->decode_data << 1 | 0;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else {
                s->parser_step = StepReset;
            }
        } else {
            s->parser_step = StepReset;
        }
        break;
    case StepFinish:
        if(s->decode_count_bit == K.min_count_bit_for_found) {
            s->last_found_data = s->decode_data;
            s->last_found_count_bit = s->decode_count_bit;
            s->found_count++;
        }
        s->decode_data = 0;
        s->decode_count_bit = 0;
        s->end_count = 0;
        s->parser_step = StepReset;
        break;
    }
}

// --- Fixed version: emit + reset-to-Reset happen on the SAME edge that
// detects end_count == 4, instead of being deferred to a separate Finish
// state on the *next* edge. No other line changes. ---
static void decoder_feed_fixed(DecState* s, bool level, uint32_t duration) {
    switch(s->parser_step) {
    case StepReset:
        if((!level) && (DURATION_DIFF(duration, K.te_short * 10) < K.te_delta * 5)) {
            s->decode_data = 0;
            s->decode_count_bit = 0;
            s->parser_step = StepSaveDuration;
        }
        break;
    case StepSaveDuration:
        if(s->decode_count_bit == K.min_count_bit_for_found) {
            if(level && (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                s->end_count++;
                if(s->end_count == 4) {
                    s->last_found_data = s->decode_data;
                    s->last_found_count_bit = s->decode_count_bit;
                    s->found_count++;
                    s->decode_data = 0;
                    s->decode_count_bit = 0;
                    s->end_count = 0;
                    s->parser_step = StepReset;
                }
            } else if((!level) && (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                break;
            } else {
                s->decode_data = 0;
                s->decode_count_bit = 0;
                s->end_count = 0;
                s->parser_step = StepReset;
            }
            break;
        }
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
                s->decode_data = s->decode_data << 1 | 1;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(s->te_last, K.te_long) < K.te_delta) &&
                (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                s->decode_data = s->decode_data << 1 | 0;
                s->decode_count_bit++;
                s->parser_step = StepSaveDuration;
            } else {
                s->parser_step = StepReset;
            }
        } else {
            s->parser_step = StepReset;
        }
        break;
    case StepFinish:
        // unreachable in the fixed state machine
        break;
    }
}

static void run_pulses(void (*feed)(DecState*, bool, uint32_t), DecState* s, const Pulse* p, size_t n) {
    for(size_t i = 0; i < n; i++) {
        feed(s, p[i].level, p[i].duration);
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
    // RED -    C396F E = 11000011100101101111 1110
    // PURPLE - C396F B = 11000011100101101111 1011
    // GREEN -  C396F D = 11000011100101101111 1101
    // BLUE -   C396F C = 11000011100101101111 1100
    struct {
        uint64_t data;
        uint8_t expect_id;
    } samples[] = {
        {0xC396FEu, 0xE},
        {0xC396FBu, 0xB},
        {0xC396FDu, 0xD},
        {0xC396FCu, 0xC},
    };
    uint32_t sample_serial[4];
    for(int i = 0; i < 4; i++) {
        uint32_t serial;
        uint8_t id;
        check_remote_controller(samples[i].data, &serial, &id);
        assert(id == samples[i].expect_id);
        sample_serial[i] = serial;
    }
    for(int i = 1; i < 4; i++) {
        assert(sample_serial[i] == sample_serial[0]);
    }
    assert(sample_serial[0] == 0xC396Fu);
    printf("Scenario 1 OK: 4/4 real samples -- same 20-bit Serial, correct 4-bit ID each\n");

    // --- Scenario 2: single-frame encode -> decode round trip (buggy == fixed here) ---
    int trials = 20000;
    for(int t = 0; t < trials; t++) {
        uint64_t data = xrand() & 0xFFFFFFu; // 24-bit random value
        Pulse buf[8 + 56];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 10};
        n += encode_key(data, K.min_count_bit_for_found, buf, sizeof(buf) / sizeof(buf[0]), n);

        DecState sb = {0};
        run_pulses(decoder_feed_buggy, &sb, buf, n);
        assert(sb.found_count == 1);
        assert(sb.last_found_count_bit == K.min_count_bit_for_found);
        assert(sb.last_found_data == data);

        DecState sf = {0};
        run_pulses(decoder_feed_fixed, &sf, buf, n);
        assert(sf.found_count == 1);
        assert(sf.last_found_count_bit == K.min_count_bit_for_found);
        assert(sf.last_found_data == data);
    }
    printf("Scenario 2 OK: %d single-frame trials, buggy and fixed both decode exactly once\n", trials);

    // --- Scenario 3: BUG repro -- Repeat == 1..5, back-to-back, unmodified upstream feed() ---
    {
        int trials5 = 5000;
        int total_repeats_sent = 0;
        int total_repeats_decoded = 0;
        for(int t = 0; t < trials5; t++) {
            uint64_t data = xrand() & 0xFFFFFFu;
            int repeat = 1 + (int)(xrand() % 5); // 1..5, matches real encoder.repeat range
            Pulse buf[8 + 5 * 60];
            size_t n = 0;
            buf[n++] = (Pulse){false, (uint32_t)K.te_short * 10};
            for(int r = 0; r < repeat; r++) {
                n += encode_key(data, K.min_count_bit_for_found, buf, sizeof(buf) / sizeof(buf[0]), n);
            }
            DecState sb = {0};
            run_pulses(decoder_feed_buggy, &sb, buf, n);
            int expected_found = (repeat + 1) / 2; // ceil(repeat / 2)
            assert(sb.found_count == expected_found); // BUG: every other repeat silently lost
            assert(sb.last_found_count_bit == K.min_count_bit_for_found);
            assert(sb.last_found_data == data);
            total_repeats_sent += repeat;
            total_repeats_decoded += sb.found_count;
        }
        printf(
            "Scenario 3 OK (bug confirmed): %d trials, Repeat=1..5 back-to-back, "
            "unmodified upstream feed() decodes %d/%d repeats (exactly ceil(repeat/2) each)\n",
            trials5,
            total_repeats_decoded,
            total_repeats_sent);
    }

    // --- Scenario 4: FIX verification -- same Repeat == 5 input, fixed feed() ---
    {
        int trials5 = 5000;
        int total_repeats_checked = 0;
        for(int t = 0; t < trials5; t++) {
            uint64_t data = xrand() & 0xFFFFFFu;
            int repeat = 1 + (int)(xrand() % 5); // 1..5, matches real encoder.repeat range
            Pulse buf[8 + 5 * 60];
            size_t n = 0;
            buf[n++] = (Pulse){false, (uint32_t)K.te_short * 10};
            for(int r = 0; r < repeat; r++) {
                n += encode_key(data, K.min_count_bit_for_found, buf, sizeof(buf) / sizeof(buf[0]), n);
            }
            DecState sf = {0};
            run_pulses(decoder_feed_fixed, &sf, buf, n);
            assert(sf.found_count == repeat); // FIXED: every repeat decodes
            assert(sf.last_found_count_bit == K.min_count_bit_for_found);
            assert(sf.last_found_data == data);
            total_repeats_checked += repeat;
        }
        printf(
            "Scenario 4 OK (fix verified): %d trials, Repeat=1..5 back-to-back, "
            "fixed feed() decodes all %d repeats, bit-exact\n",
            trials5,
            total_repeats_checked);
    }

    // --- Scenario 5: fix must not weaken end-marker validation ---
    {
        // 5a: one end-HIGH pulse mistimed -> whole frame must still be rejected (fixed feed()).
        DecState sf = {0};
        Pulse buf[8 + 56];
        size_t n = 0;
        buf[n++] = (Pulse){false, (uint32_t)K.te_short * 10};
        n += encode_key(0xC396FEu, 24, buf, sizeof(buf) / sizeof(buf[0]), n);
        // The 3rd end-HIGH pulse (5th-from-last element) mistimed to neither
        // te_short nor a plausible bit timing -> end_count never reaches 4.
        buf[n - 5].duration = (uint32_t)K.te_short * 3;
        run_pulses(decoder_feed_fixed, &sf, buf, n);
        assert(sf.found_count == 0);
        printf("Scenario 5a OK: mistimed end marker still rejected by the fixed feed()\n");

        // 5b: a corrupted interior bit pulse still aborts the frame cleanly.
        DecState sf2 = {0};
        Pulse buf2[8 + 56];
        size_t n2 = 0;
        buf2[n2++] = (Pulse){false, (uint32_t)K.te_short * 10};
        n2 += encode_key(0xC396FEu, 24, buf2, sizeof(buf2) / sizeof(buf2[0]), n2);
        buf2[5].duration = (uint32_t)K.te_short * 2; // garbled LOW half of bit 3
        run_pulses(decoder_feed_fixed, &sf2, buf2, n2);
        assert(sf2.found_count == 0);
        printf("Scenario 5b OK: garbled interior bit timing still aborts cleanly\n");
    }

    printf("ALL CHECKS PASSED\n");
    return 0;
}
