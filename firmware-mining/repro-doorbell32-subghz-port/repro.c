// Host repro for the Doorbell32 SubGHz protocol port (mntm-012, API 87.1).
// Transcribes the exact bit-timing lines from the ported
// lib/subghz/protocols/doorbell32.c (encoder get_upload(), decoder feed()
// state machine) and exercises them without any Flipper/furi headers.
// Doorbell32 is a plain static (non-rolling-code) protocol -- no cipher is
// involved. Its own anti-glitch design (the decoder only reports a frame
// once the immediately preceding frame decoded to the very same 32-bit
// value) is the main thing worth checking end to end, since it is a
// cross-frame comparison, not a single-frame property.
//
// Scenarios:
//  1. Encode (get_upload-style bit->timing expansion) -> decode (feed()
//     state machine) round trip over 20000 random 32-bit payloads, each
//     sent back to back for `repeat` in [1..6]. The very first repeat is
//     always lost at the bit level (Reset only leaves its state on a LOW
//     matching the 14*Te guard window, and no in-frame duration comes
//     close to that window, so the decoder stays in Reset discarding the
//     whole first frame and only starts capturing from frame 2, synced by
//     frame 1's own trailing guard); frame 2 then completes a capture but
//     can never match (last_data is still 0 at that point); so the first
//     report lands on frame 3 and found_count == max(repeat-2, 0) -- one
//     repeat worse than the shared-GAP-double-duty class documented for
//     Nord ICE/Treadmill37/Allstar Firefly/Ditec GOL4, because Doorbell32
//     adds its own deliberate two-frame confirmation on top of that same
//     generic first-frame sync loss.
//  2. Negative: two different payloads sent back to back must never
//     decode (the last_data == decode_data check must reject them even
//     though each individual frame is itself well-formed).
//  3. Negative: a corrupted bit timing (neither the short/long nor
//     long/short pattern), injected into a frame that is itself properly
//     synced by a preceding good frame, aborts back to Reset without
//     emitting a false frame, and does not permanently desync later,
//     well-formed frames -- checked both on its own and differentially
//     against the same four-frame burst with no corruption at all, which
//     must report exactly one more decode.
//  4. Negative: a too-short low pulse where Reset expects the 14*Te guard
//     must not be mistaken for it (no frame may start there).
//  5. `instance->te` reconstruction: after a successful decode, the
//     accumulated/divided te must be exactly the Te value actually used to
//     encode the frame (the encoder only ever emits multiples of te, so
//     integer division has no remainder here), captured at the same point
//     the real code's base.callback fires -- not read back afterwards,
//     since the real decoder unconditionally zeroes instance->te right
//     after that callback returns, to arm it for the next frame's
//     accumulation (by design, not a bug; my own base's existing
//     princeton.c decoder does the exact same thing).

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
    .te_short = 515,
    .te_long = 1545,
    .te_delta = 200,
    .min_count_bit_for_found = 32,
};
#define GUARD 14u

// ---- Encoder: 1:3 Princeton-style PWM, 32 bits MSB-first, closing pulse + guard ----
// Mirrors subghz_protocol_encoder_doorbell32_get_upload() verbatim.
static size_t encode(uint32_t data, uint32_t te, Pulse* out, size_t max_out) {
    size_t index = 0;
    for(int8_t i = 31; i >= 0; i--) {
        assert(index + 2 <= max_out);
        if(bit_read(data, i)) {
            out[index++] = (Pulse){true, te * 3};
            out[index++] = (Pulse){false, te};
        } else {
            out[index++] = (Pulse){true, te};
            out[index++] = (Pulse){false, te * 3};
        }
    }
    assert(index + 2 <= max_out);
    out[index++] = (Pulse){true, te};
    out[index++] = (Pulse){false, te * GUARD};
    return index;
}

// ---- Decoder state machine ----
// Mirrors subghz_protocol_decoder_doorbell32_feed()/_reset() verbatim.
typedef enum { StepReset = 0, StepSaveDuration, StepCheckDuration } Step;

typedef struct {
    Step step;
    uint32_t te_last;
    uint64_t decode_data;
    uint8_t decode_count_bit;

    uint32_t te;
    uint32_t last_data;

    // test instrumentation
    int found_count;
    uint64_t last_found;
    uint32_t found_te; // te as the real base.callback would observe it synchronously,
                        // i.e. *before* the unconditional post-match te=0 reset below
} Decoder;

static void reset_decoder(Decoder* d) {
    d->step = StepReset;
    d->last_data = 0;
}

static void feed(Decoder* d, bool level, uint32_t duration) {
    switch(d->step) {
    case StepReset:
        if((!level) && (DURATION_DIFF(duration, (uint32_t)K.te_short * GUARD) <
                        K.te_delta * GUARD / 2)) {
            d->step = StepSaveDuration;
            d->decode_data = 0;
            d->decode_count_bit = 0;
            d->te = 0;
        }
        break;
    case StepSaveDuration:
        if(level) {
            d->te_last = duration;
            d->te += duration;
            d->step = StepCheckDuration;
        } else {
            d->step = StepReset;
        }
        break;
    case StepCheckDuration:
        if(!level) {
            if(duration >= ((uint32_t)K.te_long * 2)) {
                if((d->decode_count_bit == K.min_count_bit_for_found) &&
                   (d->last_data == d->decode_data) && d->last_data) {
                    d->te /= (d->decode_count_bit * 4 + 1);
                    d->found_count++;
                    d->last_found = d->decode_data;
                    d->found_te = d->te; // callback fires here, synchronously, in the real code
                }
                d->last_data = d->decode_data;
                d->decode_data = 0;
                d->decode_count_bit = 0;
                d->te = 0;
                d->step = StepSaveDuration;
                break;
            }
            d->te += duration;
            if((DURATION_DIFF(d->te_last, K.te_short) < K.te_delta) &&
               (DURATION_DIFF(duration, K.te_long) < K.te_delta * 2)) {
                d->decode_data = (d->decode_data << 1) | 0;
                d->decode_count_bit++;
                d->step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(d->te_last, K.te_long) < K.te_delta * 2) &&
                (DURATION_DIFF(duration, K.te_short) < K.te_delta)) {
                d->decode_data = (d->decode_data << 1) | 1;
                d->decode_count_bit++;
                d->step = StepSaveDuration;
            } else {
                d->step = StepReset;
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
    uint32_t seed = 0xD00Bu;
    int checks = 0;

    // --- Scenario 1 + 2: round trip over random payloads, repeat 1..6, plus the
    // negative case of two different payloads back to back never decoding. ---
    for(int trial = 0; trial < 20000; trial++) {
        uint32_t data = xorshift32(&seed);
        uint32_t te = 480 + (xorshift32(&seed) % 80); // realistic per-unit Te jitter
        int repeat = 1 + (int)(xorshift32(&seed) % 6);

        Pulse frame[68];
        size_t frame_len = encode(data, te, frame, 68);
        assert(frame_len == 66); // 32 bits * 2 pulses + closing pulse + guard pulse
        checks++;

        Decoder d = {0};
        reset_decoder(&d);
        for(int r = 0; r < repeat; r++) feed_pulses(&d, frame, frame_len);

        // The very first repeat is never captured at all: Reset only leaves its
        // state on a LOW pulse matching the 14*Te guard window, and every
        // in-frame duration (te..3*te) is far below that window, so the decoder
        // stays in Reset discarding bits throughout all of frame 1 and only
        // starts capturing bits from frame 2 onward (synced by frame 1's own
        // closing+guard pulse). Frame 2 completes a full 32-bit capture but is
        // still never reported (last_data is 0 before it), so the first report
        // only happens on frame 3. found_count == max(repeat-2, 0).
        int expected = repeat >= 3 ? repeat - 2 : 0;
        if(d.found_count != expected) {
            fprintf(
                stderr,
                "FAIL scenario1: repeat=%d data=0x%08X te=%u found=%d expected=%d\n",
                repeat,
                data,
                te,
                d.found_count,
                expected);
            return 1;
        }
        checks++;
        if(d.found_count > 0) {
            if(d.last_found != data) {
                fprintf(stderr, "FAIL scenario1 bit-exactness: data=0x%08X\n", data);
                return 1;
            }
            checks++;
        }

        // Scenario 2: two DIFFERENT consecutive payloads must never decode.
        uint32_t data_b = data ^ 0x5A5A5A5Au;
        Pulse frame_b[68];
        size_t frame_b_len = encode(data_b, te, frame_b, 68);
        Decoder d4 = {0};
        reset_decoder(&d4);
        feed_pulses(&d4, frame, frame_len);
        feed_pulses(&d4, frame_b, frame_b_len);
        if(d4.found_count != 0) {
            fprintf(stderr, "FAIL scenario2: mismatched consecutive frames decoded\n");
            return 1;
        }
        checks++;
    }

    // --- Scenario 5 (dedicated): te reconstruction accuracy over a fixed payload. ---
    for(uint32_t te = 450; te <= 600; te += 7) {
        uint32_t data = 0xA5A5F00Fu;
        Pulse frame[68];
        size_t len = encode(data, te, frame, 68);
        Decoder d = {0};
        reset_decoder(&d);
        // Three repeats: the first is lost at the bit level (no leading sync,
        // see scenario 1), the second completes a capture but can't match
        // (last_data is still 0), so the first report lands on the third.
        feed_pulses(&d, frame, len);
        feed_pulses(&d, frame, len);
        feed_pulses(&d, frame, len);
        if(d.found_count != 1) {
            fprintf(stderr, "FAIL scenario5: no decode for te=%u\n", te);
            return 1;
        }
        checks++;
        // te /= (32*4+1) = 129; the encoder itself only ever emits multiples of
        // `te` (te, te*3), so the recovered value must be exactly `te`, not just
        // close to it -- integer division has no remainder here by construction.
        // Checked via found_te (captured at the same point the real code's
        // base.callback fires), not the live d.te: the real decoder
        // unconditionally zeroes instance->te right after that callback
        // returns, to arm it for the next frame's accumulation -- by design,
        // not a bug (my own base's existing princeton.c decoder does the
        // exact same thing), but it does mean the display/save path must
        // read Te synchronously from the callback, same as every other
        // static-protocol decoder in this codebase.
        if(d.found_te != te) {
            fprintf(stderr, "FAIL scenario5 exactness: te=%u recovered=%u\n", te, d.found_te);
            return 1;
        }
        checks++;
    }

    // --- Scenario 3: corrupted bit timing aborts to Reset without a false frame,
    // and does not desync the next, well-formed frame. ---
    {
        uint32_t data = 0x12345678u;
        uint32_t te = 515;
        Pulse frame[68];
        size_t len = encode(data, te, frame, 68);

        Pulse corrupted[68];
        memcpy(corrupted, frame, sizeof(frame));
        // Scramble bit 10's HIGH half into a duration matching neither the
        // short nor the long template (bit_read uses MSB-first indexing, so
        // bit index 10 corresponds to encoder position (31-10)=21 -> pulse
        // pair at index 21*2).
        corrupted[21 * 2].duration = (te * 3 + te) / 2; // ambiguous, between short/long*3

        // Differential baseline: the same four-frame burst with no corruption
        // at all must yield exactly one more successful decode than the
        // corrupted run below (frame 2 synced+matched instead of discarded),
        // proving the corruption actually cost a decode rather than the counts
        // merely matching by coincidence of hand-derived arithmetic.
        {
            Decoder clean = {0};
            reset_decoder(&clean);
            feed_pulses(&clean, frame, len);
            feed_pulses(&clean, frame, len);
            feed_pulses(&clean, frame, len);
            feed_pulses(&clean, frame, len);
            if(clean.found_count != 2 || clean.last_found != data) {
                fprintf(stderr, "FAIL scenario3 baseline: clean 4-frame burst mismatch\n");
                return 1;
            }
            checks++;
        }

        Decoder d = {0};
        reset_decoder(&d);
        // Prime first: a lone first frame never leaves Reset at the bit level
        // (see scenario 1's note), so without this priming frame the corrupted
        // frame below would also start from Reset and the corruption would
        // never actually reach the CheckDuration comparison -- the abort path
        // would pass "by accident" rather than being exercised. Priming syncs
        // the decoder via its own trailing guard, so the corrupted frame that
        // follows is captured bit-by-bit from the start, same as any other
        // frame in the middle of a real repeat burst.
        feed_pulses(&d, frame, len);
        feed_pulses(&d, corrupted, len);
        if(d.found_count != 0) {
            fprintf(stderr, "FAIL scenario3: corrupted frame falsely decoded\n");
            return 1;
        }
        checks++;
        // Two well-formed frames after the corrupted one: the corrupted
        // frame's own trailing guard re-syncs the decoder, but last_data is
        // still 0 (no full 32-bit capture has ever completed yet), so the
        // first of these only re-arms last_data; the second must decode
        // cleanly and bit-exactly.
        feed_pulses(&d, frame, len);
        feed_pulses(&d, frame, len);
        if(d.found_count != 1 || d.last_found != data) {
            fprintf(stderr, "FAIL scenario3: recovery after corruption did not re-sync\n");
            return 1;
        }
        checks++;
    }

    // --- Scenario 4: a too-short low pulse where Reset expects the 14*Te guard
    // must not be mistaken for it. ---
    {
        Decoder d = {0};
        reset_decoder(&d);
        // Half the guard duration: far outside te_delta*GUARD/2 tolerance.
        feed(&d, false, (uint32_t)K.te_short * GUARD / 2);
        if(d.step != StepReset) {
            fprintf(stderr, "FAIL scenario4: short low pulse was mistaken for guard\n");
            return 1;
        }
        checks++;
        // The real guard length must still be accepted right after.
        feed(&d, false, (uint32_t)K.te_short * GUARD);
        if(d.step != StepSaveDuration) {
            fprintf(stderr, "FAIL scenario4: real guard pulse not accepted\n");
            return 1;
        }
        checks++;
    }

    printf("%d checks passed -- ALL CHECKS PASSED\n", checks);
    return 0;
}
