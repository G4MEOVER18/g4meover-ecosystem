/* Host repro for the ProtoView Renault TPMS decoder length-check finding.
 *
 * Claim: protocols/tpms/renault.c decode(), line
 *   if (numbits-12 < 9*8) return false;
 * has the same two compounding defects already documented for
 * protocols/tpms/citroen.c and protocols/tpms/ford.c (see
 * ideas/protoview-citroen-tpms-decoder-length-check-fix.md and
 * ideas/protoview-ford-tpms-decoder-length-check-fix.md), plus one
 * additional, independent defect specific to this file:
 *
 *  1. Wrong magnitude: "9*8" (=72) is the number of *decoded* data bits
 *     (9 bytes), but the payload is Manchester-coded ("01"/"10" patterns
 *     in convert_from_line_code()), which consumes *two* raw bitmap bits
 *     per decoded bit. The real requirement is 9*8*2 (=144) raw bits
 *     after the sync match, not 72.
 *  2. Wrong timing: the check runs *before* bitmap_seek_bits() finds the
 *     actual sync offset, so it only verifies the *total* numbits under
 *     the implicit assumption that the sync match is at offset 0. If the
 *     match is found later in the buffer, the check says nothing about
 *     whether enough real bits remain *after* that offset.
 *  3. Wrong constant (specific to Renault): the check subtracts 12, but
 *     the code two lines below it (off += 20;) shows the real
 *     preamble+sync skip is 20 bits -- the same value as
 *     strlen(sync_pattern) ("01010101010101010110", 20 characters). The
 *     check's own "12" does not match either the sync pattern length or
 *     the actual skip applied right after it, so even a check that only
 *     fixed the magnitude (9*8 -> 9*8*2) but left the "12" alone would
 *     still accept 8 fewer raw bits than genuinely required -- an
 *     additional, independent slack on top of defect 1.
 *
 * Independent confirmation in the same file: "info->pulses_count =
 * (off+8*9*2) - info->start_off;" a few lines below already uses the
 * correct factor 8*9*2 for the exact same quantity of raw bits -- the
 * pre-check nine lines above it does not.
 *
 * Consequence: exactly as in the already-documented Citroen/Ford
 * findings, decode_signal() in signal.c allocates the scratch bitmap
 * passed to every decoder with a bare malloc() (no memset), sized to a
 * fixed 4096 bytes, and passes that fixed byte size (not the real sampled
 * "numbits") as the length bound into convert_from_line_code(). Because
 * this check is undersized, evaluated at the wrong point, AND subtracts
 * the wrong constant, Renault TPMS decode() can walk past the genuinely
 * sampled "numbits" bits into that uninitialized/leftover scratch memory
 * and still report a complete, CRC-valid 9-byte Renault TPMS message
 * (Tire ID/Pressure/Temperature/Flags/Unknown1/Unknown2) that is partly
 * NOT derived from the RF signal actually captured in this call.
 *
 * Renault uses a real CRC8 (poly 7, init 0) over the first 8 bytes,
 * exactly like crc.c's crc8() -- the hybrid/contamination construction
 * below solves the last contaminated byte so the hybrid message still
 * passes that CRC, same technique as the Citroen/Ford repros adapted to
 * this file's checksum.
 *
 * All bitmap primitives, crc8(), and the decode() logic below are
 * transcribed verbatim from antirez/protoview commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f (signal.c, crc.c,
 * protocols/tpms/renault.c). The only changes: FURI_LOG_E() calls and
 * ProtoViewFieldSet handling are stubbed out (no Flipper framework on the
 * host), and the decoded "raw[9]" bytes plus the symbol count are
 * returned to the caller via out-parameters so the test can inspect them.
 * ALT keeps the original (buggy) check; MID applies the magnitude fix
 * AND the constant fix (12->20), the most natural "obvious" partial fix,
 * but still pre-seek; NEU applies the actual fix (relocated post-seek,
 * correct magnitude). Every other line of decode() logic -- sync search,
 * Manchester extraction, CRC8 -- is identical across all three.
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

/* ========================= Verbatim from crc.c ========================= */

uint8_t crc8(const uint8_t *data, size_t len, uint8_t init, uint8_t poly)
{
    uint8_t crc = init;
    size_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ poly);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ============== Transcribed from protocols/tpms/renault.c ============== */
/* Note: exactly as in the real decode_signal() call site in signal.c, every
 * call here passes "numbytes" (the scratch buffer's *byte* capacity, fixed
 * at 4096 bytes in the real app) as the "blen"/"len" argument to
 * bitmap_get/bitmap_seek_bits/convert_from_line_code -- NOT "numbits" (the
 * number of actually-sampled bits). This is the real decoder's own design,
 * reproduced faithfully here. */

typedef enum { CHECK_ORIGINAL, CHECK_MAGNITUDE_ONLY, CHECK_FIXED } CheckMode;

static bool decode_core(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                         CheckMode mode, uint8_t *out_raw, uint32_t *out_decoded)
{
    const char *sync_pattern = "01010101010101010110"; /* 20 chars. */

    if (mode == CHECK_ORIGINAL) {
        /* ALT: the real upstream line. Compares against 9*8 (=72), the
         * *decoded* bit count, not the 144 raw bits actually consumed by
         * convert_from_line_code() for Manchester-coded data, subtracts
         * 12 (which matches neither strlen(sync_pattern)==20 nor the
         * "off += 20" skip two lines below in the real decode()), and
         * runs before the sync offset is even known. */
        if (numbits-12 < 9*8) return false;
    } else if (mode == CHECK_MAGNITUDE_ONLY) {
        /* MID: the most natural "obvious" partial fix -- corrects both
         * the magnitude (9*8*2 raw bits) and the mismatched constant
         * (12 -> 20, matching the real skip), but is still evaluated
         * before bitmap_seek_bits(), so it implicitly assumes the sync
         * match is at offset 0. */
        if (numbits-20 < 9*8*2) return false;
    }
    /* CHECK_FIXED (NEU) does no pre-seek check at all here -- its check
     * runs below, after the real offset is known. */

    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    off += 20; /* Skip preamble. */

    if (mode == CHECK_FIXED) {
        /* NEU: the actual fix. Checked *after* the seek, using the real
         * match offset "off", with the corrected raw-bit magnitude
         * (9*8*2, two raw bits per Manchester-coded decoded bit). */
        if (off + 9*8*2 > numbits) return false;
    }

    uint8_t raw[9];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester. */

    if (decoded < 8*9) return false; /* Require the full 9 bytes. */
    if (crc8(raw,8,0,7) != raw[8]) return false; /* Require sane CRC. */

    memcpy(out_raw,raw,9);
    *out_decoded = decoded;
    return true;
}

static bool decode_ALT(uint8_t *bits, uint32_t numbytes, uint32_t numbits, uint8_t *out_raw, uint32_t *out_decoded) {
    return decode_core(bits,numbytes,numbits,CHECK_ORIGINAL,out_raw,out_decoded);
}
static bool decode_MID(uint8_t *bits, uint32_t numbytes, uint32_t numbits, uint8_t *out_raw, uint32_t *out_decoded) {
    return decode_core(bits,numbytes,numbits,CHECK_MAGNITUDE_ONLY,out_raw,out_decoded);
}
static bool decode_NEU(uint8_t *bits, uint32_t numbytes, uint32_t numbits, uint8_t *out_raw, uint32_t *out_decoded) {
    return decode_core(bits,numbytes,numbits,CHECK_FIXED,out_raw,out_decoded);
}

/* ============================== Test harness ============================== */

/* Manchester-encode "nbytes" bytes of "data" (MSB-first per byte, matching
 * bitmap_get/bitmap_set's bit numbering) into "outbits" starting at bit
 * offset "outoff", using the same zero_pattern="01"/one_pattern="10"
 * convention as renault.c's convert_from_line_code() call. */
static void manchester_encode(const uint8_t *data, int nbytes,
                               uint8_t *outbits, uint32_t outbuflen, uint32_t outoff)
{
    for (int k = 0; k < nbytes*8; k++) {
        bool bit = bitmap_get((uint8_t*)data,nbytes,k);
        uint32_t pos = outoff + k*2;
        if (bit) {
            bitmap_set(outbits,outbuflen,pos+0,true);
            bitmap_set(outbits,outbuflen,pos+1,false);
        } else {
            bitmap_set(outbits,outbuflen,pos+0,false);
            bitmap_set(outbits,outbuflen,pos+1,true);
        }
    }
}

static void write_sync(uint8_t *bits, uint32_t buflen, uint32_t pos) {
    const char *sync_pattern = "01010101010101010110";
    for (size_t j = 0; sync_pattern[j]; j++)
        bitmap_set(bits,buflen,pos+j,sync_pattern[j]=='1');
}

static int checks_passed = 0, checks_total = 0;
#define CHECK(cond, desc) do { \
    checks_total++; \
    if (cond) { checks_passed++; printf("[PASS] %s\n", desc); } \
    else printf("[FAIL] %s\n", desc); \
} while(0)

int main(void) {
    const uint32_t numbytes = 128; /* 1024 bits scratch, plenty for every scenario below. */

    /* "Message A": the genuine captured message (CRC8(poly=7,init=0) of
     * bytes 0..7 equals byte 8, i.e. a valid Renault TPMS CRC). */
    uint8_t A[9] = {0x6c,0x34,0x16,0xab,0xcd,0xef,0xff,0xff,0x00};
    A[8] = crc8(A,8,0,7);

    /* "Hybrid": bytes 0..5 (Flags/Pressure/Temperature/Tire ID) are A's
     * real bytes; bytes 6..7 (Unknown1/Unknown2) are a distinct, clearly-
     * marked "contamination" payload (0xDE/0xAD) representing unrelated
     * data left over in the scratch buffer from a previous
     * decode_signal() call, byte 8 (CRC) solved so the *hybrid* still
     * passes the CRC8 check -- exactly the dangerous case: contaminated
     * data that still looks like a well-formed, checksum-valid message. */
    uint8_t hybrid[9];
    memcpy(hybrid, A, 6);
    hybrid[6] = 0xDE;
    hybrid[7] = 0xAD;
    hybrid[8] = crc8(hybrid,8,0,7);
    CHECK(memcmp(hybrid, A, 9) != 0, "setup: hybrid message differs from real message A");

    uint8_t hybrid_stream[40] = {0};
    manchester_encode(hybrid, 9, hybrid_stream, sizeof(hybrid_stream), 0); /* 144 bits */
    uint8_t pureA_stream[40] = {0};
    manchester_encode(A, 9, pureA_stream, sizeof(pureA_stream), 0); /* 144 bits */

    /* --- Scenario 1: sync match at offset 0 (the common case), capture
     * truncated right after the 6th real byte. Real signal provides only
     * 6 of the 9 bytes (96 of the 144 Manchester raw bits); the
     * remaining 3 bytes (48 raw bits, containing the 0xDE/0xAD marker and
     * the derived CRC byte) come from stale scratch memory beyond
     * "numbits". ALT's check only compares against 72 *decoded* bits
     * (should be 144 *raw* bits) and additionally subtracts the wrong
     * constant (12 instead of 20), so it wrongly lets this through. MID
     * (magnitude+constant fixed, still pre-seek) correctly rejects this
     * offset-0 case -- a byte-aligned cut is large enough that the
     * magnitude+constant fix alone already catches it here. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        for (int b = 0; b < 144; b++)
            bitmap_set(bits,numbytes,20+b, bitmap_get(hybrid_stream,40,b));
        uint32_t numbits = 20 + 96; /* = 116: only 6 of 9 bytes are "real" */

        printf("\n=== Scenario 1: off=0, truncated after byte 6 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        printf("ALT raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            rawALT[0],rawALT[1],rawALT[2],rawALT[3],rawALT[4],rawALT[5],rawALT[6],rawALT[7],rawALT[8]);
        CHECK(okALT == true, "ALT (buggy check) reports a successful 9-byte decode from a 116-bit capture");
        CHECK(decALT == 72, "ALT decoded exactly 72 bits (the full 9-byte message)");
        CHECK(memcmp(rawALT, A, 9) != 0,
              "ALT's reported message differs from the real captured message A (contaminated)");
        CHECK(rawALT[6] == 0xDE && rawALT[7] == 0xAD,
              "ALT's Unknown1/Unknown2 bytes (raw[6],raw[7]) are exactly the 0xDE/0xAD marker -- proves they came from beyond numbits, not from the captured RF signal");
        CHECK(rawALT[0] == A[0] && rawALT[3] == A[3] && rawALT[4] == A[4] && rawALT[5] == A[5],
              "ALT's Flags/Pressure/Temperature/Tire-ID bytes still match the real capture A -- contamination is localized to the tail, as expected");

        uint8_t rawMID[9] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == false,
              "MID (magnitude+constant fix) correctly rejects this offset-0, byte-aligned case");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects the same 116-bit truncated capture");
    }

    /* --- Scenario 2: sync match found late (off=250), same 6-real/3-stale
     * byte split. numbits is large enough that even MID's corrected-
     * magnitude-and-constant-but-still-pre-seek check passes (it only
     * looks at the *total* numbits, not at where the match actually
     * landed) -- proving that fixing both defect 1 and defect 3 is still
     * not enough without also fixing defect 2 (the timing). Only NEU,
     * which re-checks after the seek using the real offset, catches
     * this. */
    {
        uint8_t bits[128] = {0};
        /* bits[0,250) left at 0 -- cannot match "...0110" (an all-'1'/'0'
         * alternating pattern ending "0110") at any earlier position with
         * an all-zero prefix, so the first match is at 250. */
        write_sync(bits, numbytes, 250);
        for (int b = 0; b < 144; b++)
            bitmap_set(bits,numbytes,250+20+b, bitmap_get(hybrid_stream,40,b));
        uint32_t numbits = 250 + 20 + 96; /* = 366: only 6 of 9 bytes are "real" */

        printf("\n=== Scenario 2: off=250 (late match), truncated after byte 6 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true, "ALT (buggy check) also succeeds here (expected -- same bug as scenario 1)");
        CHECK(memcmp(rawALT, A, 9) != 0, "ALT's result is contaminated here too");

        uint8_t rawMID[9] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == true,
              "MID (magnitude+constant fix, still pre-seek) WRONGLY accepts: numbits-20=346 >= 144 looks sufficient overall, but 250 of those bits are before the late sync match at off=250, not after it");
        CHECK(memcmp(rawMID, A, 9) != 0,
              "MID's result is contaminated too -- fixing the magnitude and the constant alone does not prevent cross-contamination");
        CHECK(rawMID[6] == 0xDE && rawMID[7] == 0xAD,
              "MID's contaminated bytes are the same 0xDE/0xAD marker -- confirms it read past numbits exactly like ALT");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects: off(250+20)+144=414 > numbits(366)");
    }

    /* --- Scenario 3: genuine, fully-captured message (numbits covers the
     * whole sync+144-bit message, nothing borrowed from beyond numbits).
     * Confirms the fix does not regress the legitimate case: ALT, MID and
     * NEU must all accept it and decode message A exactly. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        for (int b = 0; b < 144; b++)
            bitmap_set(bits,numbytes,20+b, bitmap_get(pureA_stream,40,b));
        uint32_t numbits = 20 + 144; /* = 164: the whole message was really sampled */

        printf("\n=== Scenario 3: genuine full capture (numbits=%u), no regression check ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true && decALT == 72 && memcmp(rawALT,A,9) == 0,
              "ALT decodes the genuine full capture correctly");

        uint8_t rawMID[9] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == true && decMID == 72 && memcmp(rawMID,A,9) == 0,
              "MID decodes the genuine full capture correctly (no regression)");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == true && decNEU == 72 && memcmp(rawNEU,A,9) == 0,
              "NEU decodes the same genuine full capture correctly (no regression)");
    }

    /* --- Scenario 4: grossly insufficient capture (not even enough bits
     * for the 20-bit sync pattern itself to be found within numbits).
     * Confirms all three variants reject it. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        uint32_t numbits = 10; /* less than strlen(sync_pattern)=20 */

        printf("\n=== Scenario 4: grossly short capture (numbits=%u), all must reject ===\n", numbits);
        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        CHECK(decode_ALT(bits, numbytes, numbits, rawALT, &decALT) == false, "ALT rejects a grossly short capture");
        uint8_t rawMID[9] = {0}; uint32_t decMID = 0;
        CHECK(decode_MID(bits, numbytes, numbits, rawMID, &decMID) == false, "MID rejects a grossly short capture");
        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        CHECK(decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU) == false, "NEU rejects a grossly short capture");
    }

    printf("\n%d/%d checks passed\n", checks_passed, checks_total);
    if (checks_passed != checks_total) { printf("SOME CHECKS FAILED\n"); return 1; }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
