/*
 * Host repro for the sampling/timing state machine in furi_hal_nfc_texkom.c (Phase 4b of the
 * Texkom port). The functions under test (furi_hal_nfc_texkom_feed/_handle_impulse/_cut_frame,
 * furi_hal_nfc_texkom_coding, furi_hal_nfc_texkom_listener_tx) are transcribed 1:1 from the
 * Unleashed dev source (md5 38c7f707f2d8c9156cdd726068cc9644, confirmed against two independent
 * fetches). Everything that actually touches the STM32WB55/ST25R3916 (register writes,
 * signal_reader_start/stop, furi_hal_nfc_transparent_mode_enter/_exit, the SPI handle) is outside
 * this test's scope and stays hardware-only; texkom_signal_tx() is mocked as a recorder since its
 * own internals were already host-verified in the Phase 3 session.
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- minimal stand-ins for furi_hal/furi primitives used by the transcribed code ---- */

#define FURI_BIT(x, n) (((x) >> (n)) & 1U)
#define UNUSED(x) (void)(x)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define furi_assert(x) assert(x)
#define furi_check(x) assert(x)

typedef enum { FuriHalNfcErrorNone, FuriHalNfcErrorDataFormat } FuriHalNfcError;

/* ---- verbatim constants from furi_hal_nfc_texkom.c ---- */

#define FURI_HAL_NFC_TEXKOM_RX_TIMEOUT_MS (20U)
#define FURI_HAL_NFC_TEXKOM_SAMPLE_RATE_F64MHZ (127U)
#define FURI_HAL_NFC_TEXKOM_SAMPLE_PERIOD_US ((FURI_HAL_NFC_TEXKOM_SAMPLE_RATE_F64MHZ + 1U) / 64U)
#define FURI_HAL_NFC_TEXKOM_SAMPLES(us) ((us) / FURI_HAL_NFC_TEXKOM_SAMPLE_PERIOD_US)
#define FURI_HAL_NFC_TEXKOM_BUFF_SIZE (32U)
#define FURI_HAL_NFC_TEXKOM_REFRACTORY FURI_HAL_NFC_TEXKOM_SAMPLES(12U)
#define FURI_HAL_NFC_TEXKOM_FRAME_GAP FURI_HAL_NFC_TEXKOM_SAMPLES(200U)
#define FURI_HAL_NFC_TEXKOM_TK17_GAPS (64U)
#define FURI_HAL_NFC_TEXKOM_TK13_GAPS (127U)
#define FURI_HAL_NFC_TEXKOM_MAX_GAPS (FURI_HAL_NFC_TEXKOM_TK13_GAPS)

_Static_assert(
    FURI_HAL_NFC_TEXKOM_FRAME_GAP <= UINT8_MAX,
    "A gap is stored in a byte, so the saturation point has to fit in one");

#define FURI_HAL_NFC_TEXKOM_TYPE_BYTE (2U)
#define FURI_HAL_NFC_TEXKOM_TYPE_TK17 (0xCAU)
#define TEXKOM_SIGNAL_FRAME_SIZE (8U)
#define BITS_IN_BYTE (8U)

typedef enum { TexkomSignalCodingTk13, TexkomSignalCodingTk17, TexkomSignalCodingNum } TexkomSignalCoding;

/* ---- verbatim capture struct ---- */

typedef struct {
    uint32_t since_impulse;
    bool impulse_seen;
    bool last_level;

    uint8_t gaps[FURI_HAL_NFC_TEXKOM_MAX_GAPS];
    size_t gap_count;
    bool overflow;

    size_t frame_count;

    int thread_id; /* stand-in for FuriThreadId: value is never dereferenced by the code under test */

    size_t longest_train;
} FuriHalNfcTexkomCapture;

/* furi_thread_flags_set() just signals a waiting thread on real hardware; here it only needs to
 * be callable, so it is mocked as a call counter. */
static int g_flags_set_calls = 0;
static void mock_furi_thread_flags_set(int thread_id, uint32_t flags) {
    UNUSED(thread_id);
    UNUSED(flags);
    g_flags_set_calls++;
}
#define FuriHalNfcEventInternalTypeTransparentDataReceived 1u

/* ---- verbatim: furi_hal_nfc_texkom_cut_frame ---- */
static void furi_hal_nfc_texkom_cut_frame(FuriHalNfcTexkomCapture* capture) {
    if(!capture->overflow && (capture->gap_count == FURI_HAL_NFC_TEXKOM_TK13_GAPS ||
                              capture->gap_count == FURI_HAL_NFC_TEXKOM_TK17_GAPS)) {
        capture->frame_count = capture->gap_count;
        mock_furi_thread_flags_set(
            capture->thread_id, FuriHalNfcEventInternalTypeTransparentDataReceived);
        return;
    }

    capture->gap_count = 0;
    capture->overflow = false;
    capture->impulse_seen = false;
}

/* ---- verbatim: furi_hal_nfc_texkom_handle_impulse ---- */
static void furi_hal_nfc_texkom_handle_impulse(FuriHalNfcTexkomCapture* capture) {
    if(capture->since_impulse < FURI_HAL_NFC_TEXKOM_REFRACTORY) {
        return;
    }

    if(capture->impulse_seen) {
        if(capture->gap_count < FURI_HAL_NFC_TEXKOM_MAX_GAPS) {
            capture->gaps[capture->gap_count++] = capture->since_impulse;
            if(capture->gap_count > capture->longest_train) {
                capture->longest_train = capture->gap_count;
            }
        } else {
            capture->overflow = true;
        }
    }

    capture->impulse_seen = true;
    capture->since_impulse = 0;
}

/* ---- verbatim: furi_hal_nfc_texkom_feed ---- */
static void
    furi_hal_nfc_texkom_feed(FuriHalNfcTexkomCapture* capture, const uint8_t* data, size_t bytes) {
    for(size_t i = 0; i < bytes; i++) {
        const uint8_t byte = data[i];

        if(byte == 0 && !capture->last_level) {
            if(!capture->impulse_seen) continue;
            if(capture->since_impulse + BITS_IN_BYTE < FURI_HAL_NFC_TEXKOM_FRAME_GAP) {
                capture->since_impulse += BITS_IN_BYTE;
                continue;
            }
        }

        for(size_t bit = 0; bit < BITS_IN_BYTE; bit++) {
            const bool level = FURI_BIT(byte, bit);

            if(capture->since_impulse < FURI_HAL_NFC_TEXKOM_FRAME_GAP) {
                capture->since_impulse++;
            } else if(capture->impulse_seen) {
                furi_hal_nfc_texkom_cut_frame(capture);
                if(capture->frame_count) return;
            }

            if(level && !capture->last_level) {
                furi_hal_nfc_texkom_handle_impulse(capture);
            }
            capture->last_level = level;
        }
    }
}

/* ---- verbatim: furi_hal_nfc_texkom_coding ---- */
static TexkomSignalCoding furi_hal_nfc_texkom_coding(const uint8_t* frame) {
    return (frame[FURI_HAL_NFC_TEXKOM_TYPE_BYTE] == FURI_HAL_NFC_TEXKOM_TYPE_TK17) ?
               TexkomSignalCodingTk17 :
               TexkomSignalCodingTk13;
}

/* ---- verbatim-shaped: furi_hal_nfc_texkom_listener_tx, with texkom_signal_tx() mocked ---- */
typedef struct TexkomSignal TexkomSignal; /* opaque, as in the real header - never dereferenced here */
static int g_dummy_signal_instance;
static TexkomSignal* furi_hal_nfc_texkom_signal = (TexkomSignal*)&g_dummy_signal_instance;

static int g_signal_tx_calls = 0;
static TexkomSignalCoding g_signal_tx_last_coding;
static uint8_t g_signal_tx_last_frame[TEXKOM_SIGNAL_FRAME_SIZE];
static void mock_texkom_signal_tx(TexkomSignal* instance, TexkomSignalCoding coding, const uint8_t* frame) {
    assert(instance == furi_hal_nfc_texkom_signal);
    g_signal_tx_calls++;
    g_signal_tx_last_coding = coding;
    memcpy(g_signal_tx_last_frame, frame, TEXKOM_SIGNAL_FRAME_SIZE);
}

static FuriHalNfcError furi_hal_nfc_texkom_listener_tx(const uint8_t* tx_data, size_t tx_bits) {
    furi_check(furi_hal_nfc_texkom_signal);
    furi_check(tx_data);

    if(tx_bits != TEXKOM_SIGNAL_FRAME_SIZE * BITS_IN_BYTE) {
        return FuriHalNfcErrorDataFormat;
    }

    mock_texkom_signal_tx(furi_hal_nfc_texkom_signal, furi_hal_nfc_texkom_coding(tx_data), tx_data);

    return FuriHalNfcErrorNone;
}

/* ====================================================================================
 * Test helpers
 * ==================================================================================== */

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        checks_run++;                                                          \
        if(!(cond)) {                                                          \
            checks_failed++;                                                   \
            fprintf(stderr, "CHECK FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        }                                                                       \
    } while(0)

static void capture_init(FuriHalNfcTexkomCapture* capture) {
    memset(capture, 0, sizeof(*capture));
    capture->since_impulse = FURI_HAL_NFC_TEXKOM_FRAME_GAP; /* same saturation poller_rx() seeds */
    capture->thread_id = 1;
}

/* Builds a bit-level waveform (packed 8 bits/byte, LSB-first per FURI_BIT) out of `count` symbols.
 * Symbol k is `periods[k]` samples long: (periods[k] - pulse_width) low bits, then pulse_width
 * high bits. With a constant pulse_width across all symbols, the rising edge of symbol k lands
 * exactly periods[k] samples after the rising edge of symbol k-1 - i.e. periods[1..count-1] are
 * exactly the gap values furi_hal_nfc_texkom_feed() is expected to record (periods[0] only seeds
 * the very first, unrecorded impulse). Appends `trailing_low` extra low bits after the last
 * symbol (e.g. to force the inter-frame pause). Returns the number of bits written. */
static size_t build_waveform(
    uint8_t* bits,
    size_t bits_cap,
    const uint32_t* periods,
    size_t count,
    uint32_t pulse_width,
    uint32_t trailing_low) {
    size_t n = 0;
    for(size_t k = 0; k < count; k++) {
        assert(periods[k] > pulse_width);
        uint32_t low = periods[k] - pulse_width;
        for(uint32_t i = 0; i < low; i++) {
            assert(n < bits_cap);
            bits[n++] = 0;
        }
        for(uint32_t i = 0; i < pulse_width; i++) {
            assert(n < bits_cap);
            bits[n++] = 1;
        }
    }
    for(uint32_t i = 0; i < trailing_low; i++) {
        assert(n < bits_cap);
        bits[n++] = 0;
    }
    return n;
}

static void pack_bits(const uint8_t* bits, size_t n_bits, uint8_t* bytes_out, size_t* n_bytes_out) {
    size_t n_bytes = (n_bits + BITS_IN_BYTE - 1) / BITS_IN_BYTE;
    memset(bytes_out, 0, n_bytes);
    for(size_t i = 0; i < n_bits; i++) {
        if(bits[i]) bytes_out[i / BITS_IN_BYTE] |= (uint8_t)(1u << (i % BITS_IN_BYTE));
    }
    *n_bytes_out = n_bytes;
}

/* ====================================================================================
 * 1. TK13-shaped frame (128 impulses -> 127 gaps), two alternating period values.
 * ==================================================================================== */
static void test_tk13_shaped_frame(void) {
    const uint32_t pulse_width = 2;
    uint32_t periods[128];
    periods[0] = 50; /* seed impulse; value irrelevant, only needs to exceed REFRACTORY */
    for(size_t k = 1; k < 128; k++) {
        periods[k] = (k % 2 == 0) ? 20 : 40; /* two distinguishable gap lengths */
    }

    uint8_t bits[16384];
    size_t n_bits = build_waveform(
        bits, sizeof(bits), periods, 128, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 10);

    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, n_bits, bytes, &n_bytes);

    FuriHalNfcTexkomCapture capture;
    capture_init(&capture);
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);

    CHECK(capture.frame_count == FURI_HAL_NFC_TEXKOM_TK13_GAPS);
    CHECK(capture.gap_count == FURI_HAL_NFC_TEXKOM_TK13_GAPS);
    CHECK(!capture.overflow);
    for(size_t k = 1; k < 128; k++) {
        CHECK(capture.gaps[k - 1] == periods[k]);
    }
}

/* ====================================================================================
 * 2. TK17-shaped frame (65 impulses -> 64 gaps).
 * ==================================================================================== */
static void test_tk17_shaped_frame(void) {
    const uint32_t pulse_width = 3;
    uint32_t periods[65];
    periods[0] = 30;
    for(size_t k = 1; k < 65; k++) {
        periods[k] = 15 + (uint32_t)(k % 4) * 10; /* four distinguishable lengths, like TK17's 2-bit symbol */
    }

    uint8_t bits[16384];
    size_t n_bits = build_waveform(
        bits, sizeof(bits), periods, 65, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 15);

    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, n_bits, bytes, &n_bytes);

    FuriHalNfcTexkomCapture capture;
    capture_init(&capture);
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);

    CHECK(capture.frame_count == FURI_HAL_NFC_TEXKOM_TK17_GAPS);
    for(size_t k = 1; k < 65; k++) {
        CHECK(capture.gaps[k - 1] == periods[k]);
    }
}

/* ====================================================================================
 * 3. Fast-path/slow-path equivalence: feed the exact same TK13-shaped stream once as one block
 *    (letting the byte==0 fast path kick in on the long zero runs) and once one byte at a time
 *    (still hits the fast path per call, but on single bytes, exercising the early `continue`
 *    before the first impulse and the per-byte accumulation after it) - both must land on the
 *    same final state.
 * ==================================================================================== */
static void test_fast_slow_path_equivalence(void) {
    const uint32_t pulse_width = 2;
    uint32_t periods[128];
    periods[0] = 60;
    for(size_t k = 1; k < 128; k++) {
        periods[k] = (k % 3 == 0) ? 30 : ((k % 3 == 1) ? 50 : 70);
    }

    uint8_t bits[16384];
    size_t n_bits = build_waveform(
        bits, sizeof(bits), periods, 128, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 10);
    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, n_bits, bytes, &n_bytes);

    FuriHalNfcTexkomCapture whole;
    capture_init(&whole);
    furi_hal_nfc_texkom_feed(&whole, bytes, n_bytes);

    FuriHalNfcTexkomCapture one_byte;
    capture_init(&one_byte);
    for(size_t i = 0; i < n_bytes && one_byte.frame_count == 0; i++) {
        furi_hal_nfc_texkom_feed(&one_byte, &bytes[i], 1);
    }

    CHECK(whole.frame_count == FURI_HAL_NFC_TEXKOM_TK13_GAPS);
    CHECK(one_byte.frame_count == whole.frame_count);
    CHECK(one_byte.gap_count == whole.gap_count);
    CHECK(memcmp(one_byte.gaps, whole.gaps, whole.gap_count) == 0);
}

/* ====================================================================================
 * 4. Overflow: a train that keeps impulsing past 127 gaps without ever pausing must be rejected
 *    (frame_count stays 0) once the pause finally arrives, and longest_train must report the
 *    cap (127), not the unbounded impulse count.
 * ==================================================================================== */
static void test_overflow_rejected(void) {
    const uint32_t pulse_width = 2;
    uint32_t periods[150];
    periods[0] = 40;
    for(size_t k = 1; k < 150; k++) {
        periods[k] = 20;
    }

    uint8_t bits[16384];
    size_t n_bits = build_waveform(
        bits, sizeof(bits), periods, 150, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 10);
    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, n_bits, bytes, &n_bytes);

    FuriHalNfcTexkomCapture capture;
    capture_init(&capture);
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);

    CHECK(capture.frame_count == 0); /* 149 gaps is neither 127 nor 64: rejected */
    CHECK(capture.gap_count == 0); /* cut_frame() reset it on rejection */
    CHECK(!capture.overflow); /* reset by cut_frame() too */
    CHECK(capture.longest_train == FURI_HAL_NFC_TEXKOM_MAX_GAPS); /* capped, not 149 */
}

/* ====================================================================================
 * 5. Garbage train discarded, valid TK17 train immediately following in the SAME feed() call
 *    must still be captured correctly - proves cut_frame()'s reset doesn't leak state into the
 *    next train.
 * ==================================================================================== */
static void test_garbage_then_valid_frame(void) {
    const uint32_t pulse_width = 2;

    /* Garbage: 10 impulses, then a pause - not a valid gap count, gets discarded. */
    uint32_t garbage[10];
    for(size_t k = 0; k < 10; k++) garbage[k] = 25;
    uint8_t bits[16384];
    size_t n = build_waveform(
        bits, sizeof(bits), garbage, 10, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 5);

    /* Valid TK17 train right after, in the same buffer. */
    uint32_t periods[65];
    periods[0] = 45;
    for(size_t k = 1; k < 65; k++) periods[k] = 18 + (uint32_t)(k % 2) * 30;
    uint8_t bits2[16384];
    size_t n2 = build_waveform(
        bits2, sizeof(bits2), periods, 65, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 5);

    CHECK(n + n2 <= sizeof(bits));
    memcpy(bits + n, bits2, n2);
    size_t total_bits = n + n2;

    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, total_bits, bytes, &n_bytes);

    FuriHalNfcTexkomCapture capture;
    capture_init(&capture);
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);

    CHECK(capture.frame_count == FURI_HAL_NFC_TEXKOM_TK17_GAPS);
    for(size_t k = 1; k < 65; k++) {
        CHECK(capture.gaps[k - 1] == periods[k]);
    }
}

/* ====================================================================================
 * 6. Refractory absorption: a flicker right after an edge (a brief extra high blip within the
 *    refractory window) must not be registered as its own impulse/gap.
 * ==================================================================================== */
static void test_refractory_absorbs_ringing(void) {
    const uint32_t pulse_width = 2;
    uint32_t periods[20];
    periods[0] = 40;
    for(size_t k = 1; k < 20; k++) periods[k] = 35;

    uint8_t bits[16384];
    size_t n = build_waveform(bits, sizeof(bits), periods, 20, pulse_width, 0);

    /* Inject a one-sample ringing blip one sample after the 5th rising edge (well inside the
     * REFRACTORY window of 6 samples), by flipping a low bit back to high. Locate the 5th rising
     * edge's first high bit and perturb the bit right after it (which must currently be low, the
     * low-start of the following symbol, in the version without the blip - still high). */
    size_t offset = 0;
    for(size_t k = 0; k < 5; k++) offset += periods[k];
    /* offset now points at the first high bit of the 6th symbol; +pulse_width lands just past
     * the pulse, in what should be a low run. Make it blip high for one sample. */
    size_t blip_index = offset + pulse_width;
    CHECK(blip_index < n);
    CHECK(bits[blip_index] == 0);
    bits[blip_index] = 1;

    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, n, bytes, &n_bytes);

    FuriHalNfcTexkomCapture capture;
    capture_init(&capture);
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);

    /* Still mid-train (no trailing pause appended), so no frame cut yet - check the gaps
     * accumulated so far instead. */
    CHECK(capture.frame_count == 0);
    CHECK(capture.gap_count == 19); /* unaffected by the blip: still exactly periods[1..19] */
    for(size_t k = 1; k < 20; k++) {
        CHECK(capture.gaps[k - 1] == periods[k]);
    }
}

/* ====================================================================================
 * 7. Early return once frame_count is set: bytes appended after a valid cut (even ones shaped
 *    like another perfectly valid frame) must be ignored within the SAME feed() call.
 * ==================================================================================== */
static void test_early_return_after_cut(void) {
    const uint32_t pulse_width = 2;
    uint32_t periods[128];
    periods[0] = 50;
    for(size_t k = 1; k < 128; k++) periods[k] = 25;

    uint8_t bits[16384];
    size_t n = build_waveform(
        bits, sizeof(bits), periods, 128, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 10);

    /* Append a second, equally valid-looking TK13 train with DIFFERENT gap values right after -
     * must never be touched. */
    uint32_t periods2[128];
    periods2[0] = 50;
    for(size_t k = 1; k < 128; k++) periods2[k] = 60;
    uint8_t bits2[16384];
    size_t n2 = build_waveform(
        bits2, sizeof(bits2), periods2, 128, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 10);

    uint8_t combined[32768];
    CHECK(n + n2 <= sizeof(combined));
    memcpy(combined, bits, n);
    memcpy(combined + n, bits2, n2);

    uint8_t bytes[sizeof(combined) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(combined, n + n2, bytes, &n_bytes);

    FuriHalNfcTexkomCapture capture;
    capture_init(&capture);
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);

    CHECK(capture.frame_count == FURI_HAL_NFC_TEXKOM_TK13_GAPS);
    for(size_t k = 1; k < 128; k++) {
        CHECK(capture.gaps[k - 1] == 25); /* first train's value, never overwritten by the second */
    }

    /* Mirrors the reader callback's own guard (`if(capture->frame_count) return;`): a second feed
     * call on the same capture, even with fresh valid data, must also be a no-op. */
    FuriHalNfcTexkomCapture before = capture;
    furi_hal_nfc_texkom_feed(&capture, bytes, n_bytes);
    CHECK(memcmp(&before, &capture, sizeof(capture)) == 0);
}

/* ====================================================================================
 * 8. Split across several feed() calls, as the real 32-byte signal reader blocks would: the
 *    running state in `capture` must reconstruct the exact same frame as one single call with
 *    the concatenated bytes.
 * ==================================================================================== */
static void test_split_across_calls(void) {
    const uint32_t pulse_width = 2;
    uint32_t periods[65];
    periods[0] = 55;
    for(size_t k = 1; k < 65; k++) periods[k] = 22 + (uint32_t)(k % 5) * 12;

    uint8_t bits[16384];
    size_t n = build_waveform(
        bits, sizeof(bits), periods, 65, pulse_width, FURI_HAL_NFC_TEXKOM_FRAME_GAP + 8);
    uint8_t bytes[sizeof(bits) / BITS_IN_BYTE + 1];
    size_t n_bytes;
    pack_bits(bits, n, bytes, &n_bytes);

    FuriHalNfcTexkomCapture whole;
    capture_init(&whole);
    furi_hal_nfc_texkom_feed(&whole, bytes, n_bytes);
    CHECK(whole.frame_count == FURI_HAL_NFC_TEXKOM_TK17_GAPS);

    FuriHalNfcTexkomCapture split;
    capture_init(&split);
    const size_t block = FURI_HAL_NFC_TEXKOM_BUFF_SIZE; /* real signal_reader block size */
    for(size_t off = 0; off < n_bytes && split.frame_count == 0; off += block) {
        size_t chunk = MIN(block, n_bytes - off);
        furi_hal_nfc_texkom_feed(&split, bytes + off, chunk);
    }

    CHECK(split.frame_count == whole.frame_count);
    CHECK(memcmp(split.gaps, whole.gaps, whole.gap_count) == 0);
}

/* ====================================================================================
 * 9. furi_hal_nfc_texkom_coding(): TK17 type byte selects TK17 coding, everything else (TK13,
 *    TK15, MMBIT type bytes, and garbage) selects TK13 coding.
 * ==================================================================================== */
static void test_coding_selection(void) {
    uint8_t frame[TEXKOM_SIGNAL_FRAME_SIZE] = {0xFF, 0xFF, 0x00, 0, 0, 0, 0, 0};

    frame[FURI_HAL_NFC_TEXKOM_TYPE_BYTE] = FURI_HAL_NFC_TEXKOM_TYPE_TK17;
    CHECK(furi_hal_nfc_texkom_coding(frame) == TexkomSignalCodingTk17);

    uint8_t other_types[] = {0x00 /* TK13 */, 0x55 /* TK15, illustrative */, 0xAB /* MMBIT, illustrative */, 0x01};
    for(size_t i = 0; i < sizeof(other_types); i++) {
        frame[FURI_HAL_NFC_TEXKOM_TYPE_BYTE] = other_types[i];
        CHECK(furi_hal_nfc_texkom_coding(frame) == TexkomSignalCodingTk13);
    }
}

/* ====================================================================================
 * 10. furi_hal_nfc_texkom_listener_tx(): frame-size validation and coding dispatch, with
 *     texkom_signal_tx() mocked.
 * ==================================================================================== */
static void test_listener_tx_dispatch(void) {
    uint8_t frame[TEXKOM_SIGNAL_FRAME_SIZE] = {0xFF, 0xFF, FURI_HAL_NFC_TEXKOM_TYPE_TK17, 1, 2, 3, 4, 5};

    g_signal_tx_calls = 0;
    FuriHalNfcError err = furi_hal_nfc_texkom_listener_tx(frame, TEXKOM_SIGNAL_FRAME_SIZE * BITS_IN_BYTE);
    CHECK(err == FuriHalNfcErrorNone);
    CHECK(g_signal_tx_calls == 1);
    CHECK(g_signal_tx_last_coding == TexkomSignalCodingTk17);
    CHECK(memcmp(g_signal_tx_last_frame, frame, TEXKOM_SIGNAL_FRAME_SIZE) == 0);

    frame[FURI_HAL_NFC_TEXKOM_TYPE_BYTE] = 0x00;
    g_signal_tx_calls = 0;
    err = furi_hal_nfc_texkom_listener_tx(frame, TEXKOM_SIGNAL_FRAME_SIZE * BITS_IN_BYTE);
    CHECK(err == FuriHalNfcErrorNone);
    CHECK(g_signal_tx_last_coding == TexkomSignalCodingTk13);

    /* Wrong bit count (one bit short, one bit over, and a wildly different size) must be
     * rejected WITHOUT ever reaching texkom_signal_tx(). */
    g_signal_tx_calls = 0;
    err = furi_hal_nfc_texkom_listener_tx(frame, TEXKOM_SIGNAL_FRAME_SIZE * BITS_IN_BYTE - 1);
    CHECK(err == FuriHalNfcErrorDataFormat);
    CHECK(g_signal_tx_calls == 0);

    err = furi_hal_nfc_texkom_listener_tx(frame, TEXKOM_SIGNAL_FRAME_SIZE * BITS_IN_BYTE + 1);
    CHECK(err == FuriHalNfcErrorDataFormat);
    CHECK(g_signal_tx_calls == 0);

    err = furi_hal_nfc_texkom_listener_tx(frame, 0);
    CHECK(err == FuriHalNfcErrorDataFormat);
    CHECK(g_signal_tx_calls == 0);
}

int main(void) {
    /* Pins the three derived constants actually driving every scenario below, so a future
     * transcription error in the macros themselves (not just in the functions) shows up here
     * first instead of silently changing what every other scenario is testing against. */
    CHECK(FURI_HAL_NFC_TEXKOM_SAMPLE_PERIOD_US == 2);
    CHECK(FURI_HAL_NFC_TEXKOM_REFRACTORY == 6);
    CHECK(FURI_HAL_NFC_TEXKOM_FRAME_GAP == 100);

    test_tk13_shaped_frame();
    test_tk17_shaped_frame();
    test_fast_slow_path_equivalence();
    test_overflow_rejected();
    test_garbage_then_valid_frame();
    test_refractory_absorbs_ringing();
    test_early_return_after_cut();
    test_split_across_calls();
    test_coding_selection();
    test_listener_tx_dispatch();

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
