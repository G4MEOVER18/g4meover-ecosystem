/* Host-repro for lib/lfrfid/tools/hitagmicro.c (Unleashed dev, commit f62964c).
 *
 * hitagmicro.c has no self-test of its own (unlike hitags.c, which already ships
 * hitags_selftest()). This repro instead runs the real, byte-for-byte unmodified
 * hitagmicro_write() against the mocks/ shim, which records every furi_delay_us()
 * call in order (host_shim.c) instead of discarding it. Because every bit the
 * module transmits passes through a furi_delay_us(96|160|288|64) call before
 * anything else happens, replaying that recorded sequence reconstructs the exact
 * on-wire bitstream the real, unmodified frame builders/CRC computed - without
 * calling any of hitagmicro.c's own static helpers directly and without touching
 * hardware.
 *
 * Those reconstructed frames are compared against an independently written
 * reference frame builder + CRC-16 below (same documented algorithm -
 * poly 0x1021, init 0x0000, refin=false, refout=true, flags(5)+cmd(6)[+page(8)]
 * [+data(32)]+crc(16) - but a structurally different implementation: bit-at-a-time
 * instead of hitagmicro_crc16()'s byte-realignment trick), the way other ports in
 * this repo cross-check a transcribed decoder against an independent second
 * implementation of the same spec.
 */
#include "hitagmicro.h"
#include "host_shim.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* --- Protocol constants, cited the same way hitagmicro.c cites them (Proxmark3
 * include/protocols.h / armsrc/hitag_common.h) - public frame-format facts, not
 * anything taken from hitagmicro.c's own code. --- */
#define FLAGS        0x04
#define CMD_READ_UID 0x02
#define CMD_SYSINFO  0x17
#define CMD_READ     0x12
#define CMD_LOGIN    0x28
#define CMD_WRITE    0x14
#define PAGE_CONFIG  0xFF

#define GAP     64
#define BIT0    96
#define BIT1    160
#define SOF_VIO 288

static int g_failures = 0;
#define CHECK(cond, msg)                                     \
    do {                                                     \
        if(!(cond)) {                                        \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++;                                    \
        }                                                     \
    } while(0)

/* ---------------------------------------------------------------- reference frame builder */

typedef struct {
    uint8_t buf[16];
    size_t nbits;
} RefFrame;

static void ref_set_bit(uint8_t* buf, size_t pos, int bit) {
    if(bit)
        buf[pos / 8] |= (uint8_t)(1u << (7 - (pos % 8)));
    else
        buf[pos / 8] &= (uint8_t)~(1u << (7 - (pos % 8)));
}

static uint16_t ref_reverse16(uint16_t v) {
    uint16_t r = 0;
    for(int i = 0; i < 16; i++) {
        r = (uint16_t)((r << 1) | (v & 1));
        v = (uint16_t)(v >> 1);
    }
    return r;
}

/* Bit-at-a-time CRC-16 (poly 0x1021, init 0, refin=false, refout=true), operating
 * directly on an MSB-first bit array - deliberately not byte-realigned, unlike
 * hitagmicro_crc16()'s front-padding approach, so a transcription bug in either
 * implementation is unlikely to reproduce identically in the other. */
static uint16_t ref_crc16(const uint8_t* bitbuf, size_t nbits) {
    uint16_t crc = 0x0000;
    for(size_t i = 0; i < nbits; i++) {
        int bit = (bitbuf[i / 8] >> (7 - (i % 8))) & 1;
        crc = (uint16_t)(crc ^ ((uint16_t)bit << 15));
        if(crc & 0x8000) {
            crc = (uint16_t)((crc << 1) ^ 0x1021);
        } else {
            crc = (uint16_t)(crc << 1);
        }
    }
    return ref_reverse16(crc);
}

static void ref_put_lsb(RefFrame* f, unsigned value, int nbits) {
    for(int i = 0; i < nbits; i++) {
        ref_set_bit(f->buf, f->nbits, (int)((value >> i) & 1));
        f->nbits++;
    }
}

static void ref_put_msb_bytes(RefFrame* f, const uint8_t* src, int nbits) {
    for(int i = 0; i < nbits; i++) {
        int bit = (src[i / 8] >> (7 - (i % 8))) & 1;
        ref_set_bit(f->buf, f->nbits, bit);
        f->nbits++;
    }
}

static void ref_put_crc(RefFrame* f) {
    uint16_t crc = ref_crc16(f->buf, f->nbits);
    for(int i = 0; i < 16; i++) {
        ref_set_bit(f->buf, f->nbits, (int)((crc >> i) & 1));
        f->nbits++;
    }
}

static RefFrame ref_cmd(uint8_t cmd) {
    RefFrame f = {{0}, 0};
    ref_put_lsb(&f, FLAGS, 5);
    ref_put_lsb(&f, cmd, 6);
    ref_put_crc(&f);
    return f;
}

static RefFrame ref_read_cfg(void) {
    RefFrame f = {{0}, 0};
    ref_put_lsb(&f, FLAGS, 5);
    ref_put_lsb(&f, CMD_READ, 6);
    ref_put_lsb(&f, PAGE_CONFIG, 8);
    ref_put_lsb(&f, 0x00, 8);
    ref_put_crc(&f);
    return f;
}

static RefFrame ref_login(const uint8_t* password) {
    RefFrame f = {{0}, 0};
    ref_put_lsb(&f, FLAGS, 5);
    ref_put_lsb(&f, CMD_LOGIN, 6);
    ref_put_msb_bytes(&f, password, 32);
    ref_put_crc(&f);
    return f;
}

static RefFrame ref_write(uint8_t page, const uint8_t* data) {
    RefFrame f = {{0}, 0};
    ref_put_lsb(&f, FLAGS, 5);
    ref_put_lsb(&f, CMD_WRITE, 6);
    ref_put_lsb(&f, page, 8);
    ref_put_msb_bytes(&f, data, 32);
    ref_put_crc(&f);
    return f;
}

static bool ref_eq(const RefFrame* a, const RefFrame* b) {
    if(a->nbits != b->nbits) return false;
    size_t full_bytes = a->nbits / 8;
    if(memcmp(a->buf, b->buf, full_bytes) != 0) return false;
    size_t rem = a->nbits % 8;
    if(rem) {
        uint8_t mask = (uint8_t)(0xFF << (8 - rem));
        if((a->buf[full_bytes] & mask) != (b->buf[full_bytes] & mask)) return false;
    }
    return true;
}

/* ---------------------------------------------------------------- delay-trace decoder */

#define MAX_CAPTURED_FRAMES 32

/* Replays host_shim's recorded furi_delay_us() stream, reconstructing every
 * frame hitagmicro_write() actually transmitted, in order. A frame starts at the
 * SOF pattern (gap,bit0,gap,violation) = (64,96,64,288) and ends at the first
 * lone gap (64) not immediately followed by a bit/violation duration - that is
 * always the EOF gap, since the next furi_delay_us() after it is one of the big
 * WAIT, CHARGE, POWERDOWN, LATCH_HOLD or COLD_RESET constants, none of which
 * equal 64, 96, 160 or 288. */
static int decode_frames(RefFrame* out, int max_frames) {
    size_t n = host_shim_delay_count();
    int count = 0;
    size_t i = 0;
    while(i < n && count < max_frames) {
        if(i + 3 < n && host_shim_delay_at(i) == GAP && host_shim_delay_at(i + 1) == BIT0 &&
           host_shim_delay_at(i + 2) == GAP && host_shim_delay_at(i + 3) == SOF_VIO) {
            i += 4;
            RefFrame f = {{0}, 0};
            for(;;) {
                if(i + 1 >= n) break;
                uint32_t gap = host_shim_delay_at(i);
                uint32_t next = host_shim_delay_at(i + 1);
                if(gap != GAP) break;
                if(next == BIT0 || next == BIT1) {
                    ref_set_bit(f.buf, f.nbits, next == BIT1 ? 1 : 0);
                    f.nbits++;
                    i += 2;
                } else {
                    i += 1; /* EOF gap */
                    break;
                }
            }
            out[count++] = f;
        } else {
            i++;
        }
    }
    return count;
}

/* ---------------------------------------------------------------- test driver */

static void run_session(
    const uint8_t* password,
    const LFRFIDHitagMicro* data,
    const char* variant_label) {
    host_shim_reset();
    hitagmicro_write(data, password);

    CHECK(host_shim_field_on_count() == 9, "field_on_count != 9 (3 write sessions + 6 latch cycles)");
    CHECK(
        host_shim_field_off_count() == 9,
        "field_off_count != 9 (3 write sessions + 6 latch cycles)");

    RefFrame captured[MAX_CAPTURED_FRAMES];
    int n = decode_frames(captured, MAX_CAPTURED_FRAMES);
    CHECK(n == 18, "expected 18 transmitted frames (3 sessions x 6: UID/SYS/CFG/LOGIN/WRITEx2)");
    if(n != 18) {
        printf("  (%s: got %d frames, skipping per-frame comparison)\n", variant_label, n);
        return;
    }

    RefFrame expect_uid = ref_cmd(CMD_READ_UID);
    RefFrame expect_sys = ref_cmd(CMD_SYSINFO);
    RefFrame expect_cfg = ref_read_cfg();
    RefFrame expect_login = ref_login(password);
    RefFrame expect_write[3] = {
        ref_write(0x00, data->block0),
        ref_write(0x01, data->block1),
        ref_write(PAGE_CONFIG, data->config),
    };

    for(int session = 0; session < 3; session++) {
        int base = session * 6;
        CHECK(ref_eq(&captured[base + 0], &expect_uid), "READ UID frame mismatch");
        CHECK(ref_eq(&captured[base + 1], &expect_sys), "SYSINFO frame mismatch");
        CHECK(ref_eq(&captured[base + 2], &expect_cfg), "READ config frame mismatch");
        CHECK(ref_eq(&captured[base + 3], &expect_login), "LOGIN frame mismatch");
        CHECK(ref_eq(&captured[base + 4], &expect_write[session]), "WRITE frame (1st send) mismatch");
        CHECK(
            ref_eq(&captured[base + 5], &expect_write[session]),
            "WRITE frame (resend) mismatch, or resend differs from 1st send");
    }
    /* The three WRITE pages, in transmission order, must be block0, block1, then
     * config - the module's own comment claims config is written last so a
     * failed/partial run cannot leave the tag "reconfigured-but-dataless". This
     * confirms that from the real, unmodified call sequence, not just the comment. */
    CHECK(
        ref_eq(&captured[4], &expect_write[0]) && ref_eq(&captured[10], &expect_write[1]) &&
            ref_eq(&captured[16], &expect_write[2]),
        "WRITE order is not block0, block1, config (config-last claim not upheld)");

    printf("  %s: 18/18 frames matched, 9/9 field cycles, config-last order confirmed\n", variant_label);
}

int main(void) {
    /* CRC-16 edge case the real function special-cases explicitly
     * ("if(bitlength == 0) return 0;") - the bit-at-a-time reference naturally
     * agrees without needing a special case, which is itself a useful check. */
    CHECK(ref_crc16(NULL, 0) == 0, "ref_crc16(empty) != 0");

    /* Variant table: three known chip families plus the explicit invalid case. */
    static const struct {
        HitagMicroVariant variant;
        uint8_t password[4];
        const char* name;
    } variants[] = {
        {HitagMicroVariant8265, {0x00, 0x00, 0x00, 0x00}, "8265"},
        {HitagMicroVariant8210, {0x9A, 0xC4, 0x99, 0x9C}, "8210"},
        {HitagMicroVariantH55, {0x49, 0x6B, 0x0E, 0x59}, "H5.5"},
    };
    for(size_t i = 0; i < 3; i++) {
        const uint8_t* pw = hitagmicro_variant_password(variants[i].variant);
        CHECK(pw != NULL, "variant_password returned NULL for a valid variant");
        CHECK(memcmp(pw, variants[i].password, 4) == 0, "variant_password value mismatch");
        CHECK(
            strcmp(hitagmicro_variant_name(variants[i].variant), variants[i].name) == 0,
            "variant_name value mismatch");
    }
    CHECK(hitagmicro_variant_password(HitagMicroVariantCount) == NULL, "invalid variant password != NULL");
    CHECK(
        strcmp(hitagmicro_variant_name(HitagMicroVariantCount), "Unknown") == 0,
        "invalid variant name != \"Unknown\"");

    /* Three end-to-end hitagmicro_write() runs, one per chip variant, each with
     * distinct, non-trivial EM4100 payload bytes so a byte-order/bit-order bug
     * could not hide behind a degenerate all-zero/all-same test vector. */
    LFRFIDHitagMicro data1 = {
        .block0 = {0xDE, 0xAD, 0xBE, 0xEF},
        .block1 = {0x12, 0x34, 0x56, 0x78},
        .config = {0x00, 0x14, 0x80, 0x00}, /* representative EM4100 TTF config word */
    };
    run_session(variants[0].password, &data1, "8265 (password 00000000)");

    LFRFIDHitagMicro data2 = {
        .block0 = {0x01, 0x23, 0x45, 0x67},
        .block1 = {0x89, 0xAB, 0xCD, 0xEF},
        .config = {0x00, 0x14, 0x80, 0x00},
    };
    run_session(variants[1].password, &data2, "8210 (password 9AC4999C)");

    LFRFIDHitagMicro data3 = {
        .block0 = {0xFF, 0x00, 0xA5, 0x5A},
        .block1 = {0x0F, 0xF0, 0x3C, 0xC3},
        .config = {0x00, 0x14, 0x80, 0x00},
    };
    run_session(variants[2].password, &data3, "H5.5 (password 496B0E59)");

    if(g_failures == 0) {
        printf("ALLE CHECKS BESTANDEN\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", g_failures);
        return 1;
    }
}
