/* Host repro for the ProtoView KeeLoq decoder length-check finding.
 *
 * Claim: protocols/keeloq.c decode(), line
 *   if (numbits-sync_len+sync_len < 3*66) return false;
 * is algebraically a no-op (the "-sync_len" and "+sync_len" cancel out in
 * unsigned arithmetic regardless of the value of numbits), so it is
 * equivalent to "if (numbits < 3*66) return false;" and does NOT verify
 * that at least 3*66 actually-sampled bits remain *after* the sync pattern
 * is consumed, unlike every sibling decoder in the same codebase
 * (protocols/b4b1.c, protocols/oregon2.c, protocols/pvchat.c,
 * protocols/tpms/citroen.c, protocols/tpms/ford.c,
 * protocols/tpms/schrader_eg53ma4.c, protocols/tpms/toyota.c), which all
 * use the idiom "numbits - sync_len < <data_bits>".
 *
 * Consequence: decode_signal() in signal.c allocates the scratch bitmap
 * passed to every decoder with a bare malloc() (no memset), sized to a
 * fixed 4096 bytes, and passes that fixed size (not the real "numbits" of
 * sampled data) as the length bound into convert_from_line_code(). Because
 * the KeeLoq length check is a no-op, the decoder can walk past the
 * genuinely sampled "numbits" bits into that uninitialized/leftover
 * scratch memory and still report a complete, well-formed 66-bit KeeLoq
 * message (encrypted rolling code + remote ID + buttons + battery flag)
 * that is partly or wholly NOT derived from the RF signal that was
 * actually captured in this call.
 *
 * All bitmap primitives and the decode() logic below are transcribed
 * verbatim from antirez/protoview commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f (signal.c, protocols/keeloq.c).
 * The only changes: FURI_LOG_E() calls and ProtoViewFieldSet handling are
 * stubbed out (no Flipper framework on the host), and the decoded "raw[9]"
 * bytes plus the symbol count are returned to the caller via out-parameters
 * so the test can inspect them. ALT keeps the original (buggy) length
 * check; NEU uses the fix (matching the established sibling idiom). Every
 * other line of decode() logic -- sync search, gap counting, line-code
 * extraction, bit-reversal -- is identical between ALT and NEU.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/* ======================= Verbatim from signal.c ======================= */

void bitmap_set(uint8_t *b, uint32_t blen, uint32_t bitpos, bool val) {
    uint32_t byte = bitpos/8;
    uint32_t bit = 7-(bitpos&7);
    if (byte >= blen) return;
    if (val)
        b[byte] |= 1<<bit;
    else
        b[byte] &= ~(1<<bit);
}

bool bitmap_get(uint8_t *b, uint32_t blen, uint32_t bitpos) {
    uint32_t byte = bitpos/8;
    uint32_t bit = 7-(bitpos&7);
    if (byte >= blen) return 0;
    return (b[byte] & (1<<bit)) != 0;
}

bool bitmap_match_bits(uint8_t *b, uint32_t blen, uint32_t bitpos, const char *bits) {
    for (size_t j = 0; bits[j]; j++) {
        bool expected = (bits[j] == '1') ? true : false;
        if (bitmap_get(b,blen,bitpos+j) != expected) return false;
    }
    return true;
}

#define BITMAP_SEEK_NOT_FOUND UINT32_MAX

uint32_t bitmap_seek_bits(uint8_t *b, uint32_t blen, uint32_t startpos, uint32_t maxbits, const char *bits) {
    uint32_t endpos = startpos+blen*8;
    uint32_t end2 = startpos+maxbits;
    if (end2 < endpos) endpos = end2;
    for (uint32_t j = startpos; j < endpos; j++)
        if (bitmap_match_bits(b,blen,j,bits)) return j;
    return BITMAP_SEEK_NOT_FOUND;
}

void bitmap_reverse_bytes_bits(uint8_t *p, uint32_t len) {
    for (uint32_t j = 0; j < len; j++) {
        uint32_t b = p[j];
        b = (b&0xf0)>>4 | (b&0x0f)<<4;
        b = (b&0xcc)>>2 | (b&0x33)<<2;
        b = (b&0xaa)>>1 | (b&0x55)<<1;
        p[j] = b;
    }
}

uint32_t convert_from_line_code(uint8_t *buf, uint64_t buflen, uint8_t *bits, uint32_t len, uint32_t off, const char *zero_pattern, const char *one_pattern)
{
    uint32_t decoded = 0;
    len *= 8;
    while(off < len) {
        bool bitval;
        if (bitmap_match_bits(bits,len,off,zero_pattern)) {
            bitval = false;
            off += strlen(zero_pattern);
        } else if (bitmap_match_bits(bits,len,off,one_pattern)) {
            bitval = true;
            off += strlen(one_pattern);
        } else {
            break;
        }
        bitmap_set(buf,buflen,decoded++,bitval);
        if (decoded/8 == buflen) break;
    }
    return decoded;
}

/* =================== Transcribed from protocols/keeloq.c =================== */
/* Note: bitmap_match_bits()'s internal uses of "len" above are in BITS in
 * convert_from_line_code() (it does "len *= 8" itself), but every call site
 * in decode() passes "numbytes" (a byte count) as the "blen"/"len" argument
 * to bitmap_get/bitmap_seek_bits/convert_from_line_code, exactly as the
 * real keeloq.c does -- i.e. the *byte* capacity of the scratch buffer, not
 * the number of actually-sampled bits ("numbits"). This matches the real
 * decode_signal() call site in signal.c, which calls
 * Decoders[j]->decode(bitmap, bitmap_size, bits, info) with bitmap_size
 * being the fixed 4096-byte scratch allocation. */

static bool decode_core(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                         bool use_fixed_check, uint8_t *out_raw, uint32_t *out_decoded)
{
    const char *sync_pattern = "101010101010101010101010" "0000";
    uint8_t sync_len = 24+4;

    if (use_fixed_check) {
        /* NEU: matches the idiom used by every sibling decoder
         * (b4b1.c, oregon2.c, pvchat.c, tpms/citroen.c, tpms/ford.c,
         * tpms/schrader_eg53ma4.c, tpms/toyota.c): ensure at least
         * 3*66 bits remain once the sync pattern itself is accounted
         * for. */
        if (numbits-sync_len < 3*66) return false;
    } else {
        /* ALT: the real upstream line. Algebraically numbits-sync_len+
         * sync_len == numbits for every numbits/sync_len value (the
         * subtraction and addition cancel in unsigned arithmetic, no
         * wraparound needed to see this -- it's a plain identity), so
         * this is equivalent to "if (numbits < 3*66) return false;"
         * and ignores sync_len entirely despite appearing to use it. */
        if (numbits-sync_len+sync_len < 3*66) return false;
    }

    uint32_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    off += sync_len;

    uint8_t gap_len = 0;
    while(gap_len <= 7 && bitmap_get(bits,numbytes,off+gap_len) == 0)
        gap_len++;
    if (gap_len < 3 || gap_len > 7) return false;

    off += gap_len;

    uint8_t raw[9] = {0};
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "110","100");
    if (decoded < 66) return false;

    bitmap_reverse_bytes_bits(raw,sizeof(raw));

    memcpy(out_raw,raw,9);
    *out_decoded = decoded;
    return true;
}

static bool decode_ALT(uint8_t *bits, uint32_t numbytes, uint32_t numbits, uint8_t *out_raw, uint32_t *out_decoded) {
    return decode_core(bits,numbytes,numbits,false,out_raw,out_decoded);
}
static bool decode_NEU(uint8_t *bits, uint32_t numbytes, uint32_t numbits, uint8_t *out_raw, uint32_t *out_decoded) {
    return decode_core(bits,numbytes,numbits,true,out_raw,out_decoded);
}

/* =================== Transcribed from build_message() in keeloq.c =================== */
/* Only the bit-stream shape matters for this repro (not real pulse
 * durations), so we emit the symbol bits directly instead of raw samples:
 * this produces exactly the same 198-bit stream that
 * convert_signal_to_bits() would have produced from build_message()'s
 * pulses at the matching sample rate. */
static void encode_keeloq_data_bits(uint8_t *data9 /* pre-reverse, as in decode()'s "raw" */,
                                     uint8_t *out_stream_bits /* 198 bits, MSB-first bitmap */)
{
    /* Mirror build_message(): take the field bytes (here: data9 is
     * already the desired POST-reverse "raw" layout used by decode(),
     * so we must reverse it back to get the on-air LSB-first layout,
     * exactly as build_message() does with its own "data[9]" before the
     * symbol loop). */
    uint8_t data[9];
    memcpy(data,data9,9);
    bitmap_reverse_bytes_bits(data,sizeof(data)); /* Keeloq is LSB first. */

    for (int j = 0; j < 66; j++) {
        bool bit = bitmap_get(data,9,j);
        if (bit) {
            bitmap_set(out_stream_bits,198/8+1,j*3+0,true);
            bitmap_set(out_stream_bits,198/8+1,j*3+1,false);
            bitmap_set(out_stream_bits,198/8+1,j*3+2,false);
        } else {
            bitmap_set(out_stream_bits,198/8+1,j*3+0,true);
            bitmap_set(out_stream_bits,198/8+1,j*3+1,true);
            bitmap_set(out_stream_bits,198/8+1,j*3+2,false);
        }
    }
}

/* ============================== Test harness ============================== */

static void build_preamble_sync_gap(uint8_t *buf, uint32_t buflen) {
    /* 12 pairs alternating 1/0 = 24 bits, then "0000" = 28 bits total
     * (the literal sync_pattern), then 5 more zero bits of gap (within
     * the decoder's accepted 3..7 range) -- 33 bits total, matching a
     * real capture converted at the nominal symbol rate. */
    uint32_t pos = 0;
    for (int i = 0; i < 12; i++) {
        bitmap_set(buf,buflen,pos++,true);
        bitmap_set(buf,buflen,pos++,false);
    }
    for (int i = 0; i < 4; i++) bitmap_set(buf,buflen,pos++,false);
    assert(pos == 28);
    for (int i = 0; i < 5; i++) bitmap_set(buf,buflen,pos++,false);
    assert(pos == 33);
}

static int checks_passed = 0, checks_total = 0;
#define CHECK(cond, desc) do { \
    checks_total++; \
    if (cond) { checks_passed++; printf("[PASS] %s\n", desc); } \
    else printf("[FAIL] %s\n", desc); \
} while(0)

int main(void) {
    const uint32_t numbytes = 64; /* 512 bits, plenty for sync+gap+198 bits = 231 bits (29 bytes) */

    /* "Message A": the genuinely captured signal. Arbitrary but fixed
     * payload (post-reverse "raw" layout, as produced by a correct decode): */
    uint8_t dataA[9] = {0x11,0x22,0x33,0x44, 0x55,0x66,0x77,0x88, 0x03};
    /* "Message B": represents unrelated data left over in the scratch
     * buffer from a previous decode_signal() call (a previous capture).
     * Deliberately very different from A so cross-contamination is
     * unmistakable in the output. */
    uint8_t dataB[9] = {0xDE,0xAD,0xBE,0xEF, 0xCA,0xFE,0xBA,0xBE, 0x3C};

    uint8_t streamA[32] = {0}, streamB[32] = {0};
    encode_keeloq_data_bits(dataA, streamA);
    encode_keeloq_data_bits(dataB, streamB);

    /* --- Scenario 1: truncated real capture, "ghost" tail from a stale
     * buffer continues the line code cleanly.
     * Real signal provides only 62 of the 66 data bits (186 of 198
     * symbol-bits); the remaining 12 symbol-bits (4 bits) are taken from
     * message B's stream, simulating uninitialized/leftover scratch
     * memory that happens to still hold a previous capture's valid line
     * code (plausible: decode_signal() malloc()s and free()s the same
     * 4096-byte size on every single call, so re-use of the same heap
     * region across captures is the common case, not an edge case). */
    {
        /* Real signal covers only the first 55 of the 66 data bits (165
         * of the 198 symbol-bits); the remaining 11 decoded bits
         * (indices 55..65, symbol-bits [165,198)) come from message B's
         * stream. Decoded bits 56..63 fall entirely inside raw[7] (the
         * "buttons" field, bits 7..4, plus 4 of the 28 "id" bits, bits
         * 3..0 -- see keeloq.c's "buttons = raw[7]>>4" and the
         * subsequent id-field assembly), so raw[7] ends up *entirely*
         * sourced from B, giving a clean, field-aligned proof of
         * cross-contamination instead of a partial-bit one. */
        uint8_t bits[64] = {0};
        build_preamble_sync_gap(bits, numbytes);
        for (int j = 0; j < 165; j++)
            bitmap_set(bits,numbytes,33+j, bitmap_get(streamA,32,j));
        for (int j = 165; j < 198; j++)
            bitmap_set(bits,numbytes,33+j, bitmap_get(streamB,32,j));

        uint32_t numbits = 33 + 165; /* = 198: exactly ALT's (meaningless) threshold */

        printf("\n=== Scenario 1: truncated capture (numbits=%u), ghost tail = message B ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        printf("ALT raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            rawALT[0],rawALT[1],rawALT[2],rawALT[3],rawALT[4],rawALT[5],rawALT[6],rawALT[7],rawALT[8]);
        CHECK(okALT == true, "ALT (buggy check) reports a successful 66-bit decode from a 198-bit capture");
        CHECK(decALT == 66, "ALT decoded exactly 66 bits (the full message)");
        CHECK(memcmp(rawALT, dataA, 9) != 0,
              "ALT's reported message differs from the real captured message A (contaminated by B)");
        CHECK(rawALT[7] == dataB[7],
              "ALT's raw[7] (buttons field + 4 ID bits) is entirely message B's byte -- proves it came from beyond numbits, not from the captured RF signal");
        CHECK(rawALT[0] == dataA[0] && rawALT[1] == dataA[1],
              "ALT's leading bytes (encr[0..1]) still match the real capture A -- the contamination is localized to the tail, as expected");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (fixed check) correctly rejects the same 198-bit truncated capture");
    }

    /* --- Scenario 2: same truncated capture, but with no plausible
     * "ghost" continuation at all (the trailing region is all zero bits,
     * e.g. a freshly zeroed buffer) -- confirms ALT's acceptance in
     * Scenario 1 was not a fluke of a specific ghost payload: a clean
     * all-zero tail cannot form "110"/"100" symbols, so ALT must fail at
     * convert_from_line_code() itself, demonstrating that what makes
     * Scenario 1 dangerous is specifically that heap garbage is RF-shaped
     * (itself a product of the same buffer being reused for genuine
     * line-coded captures call after call), not that the bug is somehow
     * harmless. */
    {
        uint8_t bits[64] = {0};
        build_preamble_sync_gap(bits, numbytes);
        for (int j = 0; j < 165; j++)
            bitmap_set(bits,numbytes,33+j, bitmap_get(streamA,32,j));
        /* bits[33+165 .. 33+198) left at 0 -- not a valid "110"/"100" run. */
        uint32_t numbits = 33 + 165;

        printf("\n=== Scenario 2: same truncated capture, all-zero tail (sanity check) ===\n");
        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == false,
              "ALT correctly fails when the out-of-bounds tail does not happen to look like line code");
    }

    /* --- Scenario 3: a genuine, fully-captured message (numbits covers
     * the whole sync+gap+198-bit message, nothing borrowed from beyond
     * numbits). Confirms the fix does not regress the legitimate case:
     * both ALT and NEU must accept it and decode message A exactly. */
    {
        uint8_t bits[64] = {0};
        build_preamble_sync_gap(bits, numbytes);
        for (int j = 0; j < 198; j++)
            bitmap_set(bits,numbytes,33+j, bitmap_get(streamA,32,j));
        uint32_t numbits = 33 + 198; /* = 231: the whole message was really sampled */

        printf("\n=== Scenario 3: genuine full capture (numbits=%u), no regression check ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true && decALT == 66 && memcmp(rawALT,dataA,9) == 0,
              "ALT decodes the genuine full capture correctly");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == true && decNEU == 66 && memcmp(rawNEU,dataA,9) == 0,
              "NEU decodes the same genuine full capture correctly (no regression)");
    }

    /* --- Scenario 4: grossly insufficient capture (well under even
     * ALT's reduced threshold). Confirms both reject -- the bug is
     * specifically about the narrow window where numbits is enough for
     * ALT's "numbits < 198" but not enough to actually cover sync_len +
     * 198 bits of real data. */
    {
        uint8_t bits[64] = {0};
        build_preamble_sync_gap(bits, numbytes);
        uint32_t numbits = 40; /* way under 198 */

        printf("\n=== Scenario 4: grossly short capture (numbits=%u), both must reject ===\n", numbits);
        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == false, "ALT rejects a grossly short capture");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false, "NEU rejects a grossly short capture");
    }

    printf("\n%d/%d checks passed\n", checks_passed, checks_total);
    if (checks_passed != checks_total) { printf("SOME CHECKS FAILED\n"); return 1; }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
