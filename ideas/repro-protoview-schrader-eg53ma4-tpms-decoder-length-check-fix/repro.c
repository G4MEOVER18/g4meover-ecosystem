/* Host repro for the ProtoView Schrader EG53MA4 TPMS decoder length-check
 * finding.
 *
 * Claim: protocols/tpms/schrader_eg53ma4.c decode(), line
 *   if (numbits-sync_len+8 < 8*10) return false;
 * has the same two compounding defects already documented for
 * protocols/tpms/citroen.c, protocols/tpms/ford.c and
 * protocols/tpms/renault.c (see the three matching files under ideas/):
 *
 *  1. Wrong magnitude: "8*10" (=80) is the number of *decoded* data bits
 *     (10 bytes), but the payload is Manchester-coded ("01"/"10" patterns
 *     in convert_from_line_code()), which consumes *two* raw bitmap bits
 *     per decoded bit. The real requirement is 10*8*2 (=160) raw bits
 *     after the sync match, not 80.
 *  2. Wrong timing: the check runs *before* bitmap_seek_bits() finds the
 *     actual sync offset, so it only verifies the *total* numbits under
 *     the implicit assumption that the sync match is at offset 0. If the
 *     match is found later in the buffer, the check says nothing about
 *     whether enough real bits remain *after* that offset.
 *
 * Independent confirmation in the same file: "info->pulses_count =
 * (off+10*8*2) - info->start_off;" a few lines below already uses the
 * correct factor 10*8*2 for the exact same quantity of raw bits -- the
 * pre-check above it does not. Unlike Renault, the subtracted constant
 * here ("sync_len-8" = 12) IS consistent with the real post-seek skip
 * ("off += sync_len-8;" two lines later) -- this file has only the two
 * defects above, not Renault's third, independent inconsistent-constant
 * defect.
 *
 * Consequence: exactly as in the already-documented Citroen/Ford/Renault
 * findings, decode_signal() in signal.c allocates the scratch bitmap
 * passed to every decoder with a bare malloc() (no memset), sized to a
 * fixed 4096 bytes, and passes that fixed byte size (not the real sampled
 * "numbits") as the length bound into convert_from_line_code(). Because
 * this check is both undersized and evaluated at the wrong point,
 * Schrader EG53MA4 TPMS decode() can walk past the genuinely sampled
 * "numbits" bits into that uninitialized/leftover scratch memory and
 * still report a complete, checksum-valid 10-byte Schrader EG53MA4 TPMS
 * message (Tire ID/Pressure/Temperature) that is partly NOT derived from
 * the RF signal actually captured in this call.
 *
 * Like Ford, Schrader EG53MA4's checksum is an additive sum of the first
 * 9 bytes mod 256 -- the hybrid/contamination construction below follows
 * the same scheme, this time contaminating exactly the two
 * user-displayed metric bytes (raw[7] Pressure, raw[8] Temperature) plus
 * the checksum byte raw[9], while leaving the Tire ID bytes (raw[4..6])
 * genuine, to make the "which displayed value is garbage" distinction
 * concrete.
 *
 * All bitmap primitives and the decode() logic below are transcribed
 * verbatim from antirez/protoview commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f (signal.c,
 * protocols/tpms/schrader_eg53ma4.c). The only changes: FURI_LOG_E() calls
 * and ProtoViewFieldSet handling are stubbed out (no Flipper framework on
 * the host), and the decoded raw[10] bytes plus the symbol count are
 * returned to the caller via out-parameters so the test can inspect them.
 * ALT keeps the original (buggy) check; MID applies only the magnitude
 * fix, still pre-seek; NEU applies the actual fix (inserted post-seek,
 * correct magnitude). Every other line of decode() logic -- sync search,
 * Manchester extraction, additive checksum -- is identical across all
 * three.
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

/* ========== Transcribed from protocols/tpms/schrader_eg53ma4.c ========== */
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
    const char *sync_pattern = "010101010101" "01100101";
    uint8_t sync_len = 12+8; /* We just use 12 preamble symbols + sync. */

    if (mode == CHECK_ORIGINAL) {
        /* ALT: the real upstream line. Compares against 8*10 (=80), the
         * *decoded* bit count, not the 160 raw bits actually consumed by
         * convert_from_line_code() for Manchester-coded data, and runs
         * before the sync offset is even known. */
        if (numbits-sync_len+8 < 8*10) return false;
    } else if (mode == CHECK_MAGNITUDE_ONLY) {
        /* MID: corrects the magnitude (10*8*2 raw bits) but is still
         * evaluated before bitmap_seek_bits(), so it implicitly assumes
         * the sync match is at offset 0. Keeps the same "+8" adjustment
         * (equivalent to subtracting sync_len-8=12) as the real upstream
         * line, since that part of the constant is not the bug here. */
        if (numbits-sync_len+8 < 10*8*2) return false;
    }
    /* CHECK_FIXED (NEU) does no pre-seek check at all here -- its check
     * runs below, after the real offset is known. */

    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    off += sync_len-8; /* Skip preamble, not sync that is part of the data. */

    if (mode == CHECK_FIXED) {
        /* NEU: the actual fix. Checked *after* the seek, using the real
         * match offset "off", with the corrected raw-bit magnitude
         * (10*8*2, two raw bits per Manchester-coded decoded bit). */
        if (off + 10*8*2 > numbits) return false;
    }

    uint8_t raw[10];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester code. */

    if (decoded < 10*8) return false; /* Require the full 10 bytes. */

    /* CRC is just all bytes added mod 256. */
    uint8_t crc = 0;
    for (int j = 0; j < 9; j++) crc += raw[j];
    if (crc != raw[9]) return false; /* Require sane CRC. */

    memcpy(out_raw,raw,10);
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
 * convention as schrader_eg53ma4.c's convert_from_line_code() call. */
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
    const char *sync_pattern = "010101010101" "01100101";
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

    /* "Message A": the genuine captured message. raw[0]'s upper nibble
     * must be 0x4 (binary 0100), because the sync pattern's last 8 raw
     * bits ("01100101") are simultaneously the Manchester encoding of
     * that nibble -- exactly as the real file's comment says ("already
     * part of the data stream (first nibble) corresponding to 0x4").
     * Checksum (sum of bytes 0..8 mod 256) fixed up to equal raw[9]. */
    uint8_t A[10] = {0x45,0x12,0x34,0xAB,0xCD,0xEF,0x00,0x50,0x5A,0x00};
    {
        uint8_t sum = 0;
        for (int j = 0; j < 9; j++) sum += A[j];
        A[9] = sum; /* fix up A[9] to the real checksum */
        assert(A[9] == sum);
    }

    /* "Hybrid": bytes 0..6 are A's real bytes (includes the full, genuine
     * Tire ID at raw[4..6]); bytes 7..8 (Pressure, Temperature -- the two
     * user-displayed metrics besides Tire ID) are a distinct, clearly-
     * marked "contamination" payload (0xDE/0xAD) representing unrelated
     * data left over in the scratch buffer from a previous
     * decode_signal() call, byte 9 solved so the *hybrid* still passes
     * the additive checksum -- exactly the dangerous case: contaminated
     * data that still looks like a well-formed, checksum-valid message. */
    uint8_t hybrid[10];
    memcpy(hybrid, A, 7);
    hybrid[7] = 0xDE;
    hybrid[8] = 0xAD;
    {
        uint8_t sum = 0;
        for (int j = 0; j < 9; j++) sum += hybrid[j];
        hybrid[9] = sum;
    }
    CHECK(memcmp(hybrid, A, 10) != 0, "setup: hybrid message differs from real message A");

    uint8_t hybrid_stream[32] = {0};
    manchester_encode(hybrid, 10, hybrid_stream, sizeof(hybrid_stream), 0); /* 160 bits */
    uint8_t pureA_stream[32] = {0};
    manchester_encode(A, 10, pureA_stream, sizeof(pureA_stream), 0); /* 160 bits */

    /* --- Scenario 1: sync match at offset 0 (the common case), capture
     * truncated right after the 7th real byte. Real signal provides only
     * 7 of the 10 bytes (112 of the 160 Manchester raw bits); the
     * remaining 3 bytes (48 raw bits, containing the 0xDE/0xAD marker and
     * the derived checksum byte) come from stale scratch memory beyond
     * "numbits". ALT's check only compares against 80 *decoded* bits
     * (should be 160 *raw* bits), so it wrongly lets this through even
     * though it is evaluated at offset 0, the one case a magnitude-only
     * fix (MID) is actually able to catch. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        for (int b = 0; b < 160; b++)
            bitmap_set(bits,numbytes,12+b, bitmap_get(hybrid_stream,32,b));
        uint32_t numbits = 12 + 112; /* = 124: only 7 of 10 bytes are "real" */

        printf("\n=== Scenario 1: off=0, truncated after byte 7 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[10] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        printf("ALT raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            rawALT[0],rawALT[1],rawALT[2],rawALT[3],rawALT[4],
            rawALT[5],rawALT[6],rawALT[7],rawALT[8],rawALT[9]);
        CHECK(okALT == true, "ALT (buggy check) reports a successful 10-byte decode from a 124-bit capture");
        CHECK(decALT == 80, "ALT decoded exactly 80 bits (the full 10-byte message)");
        CHECK(memcmp(rawALT, A, 10) != 0,
              "ALT's reported message differs from the real captured message A (contaminated)");
        CHECK(rawALT[7] == 0xDE && rawALT[8] == 0xAD,
              "ALT's Pressure/Temperature bytes (raw[7],raw[8]) are exactly the 0xDE/0xAD marker -- proves they came from beyond numbits, not from the captured RF signal");
        CHECK(memcmp(rawALT+4, A+4, 3) == 0,
              "ALT's Tire ID bytes (raw[4..6]) still match the real capture A -- contamination is localized to the Pressure/Temperature tail, as expected");

        uint8_t rawMID[10] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == false,
              "MID (magnitude-only fix) correctly rejects this offset-0 case");

        uint8_t rawNEU[10] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects the same 124-bit truncated capture");
    }

    /* --- Scenario 2: sync match found late (off=250), same 7-real/3-stale
     * byte split. numbits is large enough that even MID's corrected-
     * magnitude-but-still-pre-seek check passes (it only looks at the
     * *total* numbits, not at where the match actually landed) -- proving
     * that the magnitude fix alone is not enough. Only NEU, which re-checks
     * after the seek using the real offset, catches this. */
    {
        uint8_t bits[128] = {0};
        /* bits[0,250) left at 0 -- cannot match
         * "010101010101" "01100101" at any earlier position with an
         * all-zero prefix, so the first match is at 250. */
        write_sync(bits, numbytes, 250);
        for (int b = 0; b < 160; b++)
            bitmap_set(bits,numbytes,250+12+b, bitmap_get(hybrid_stream,32,b));
        uint32_t numbits = 250 + 12 + 112; /* = 374: only 7 of 10 bytes are "real" */

        printf("\n=== Scenario 2: off=250 (late match), truncated after byte 7 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[10] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true, "ALT (buggy check) also succeeds here (expected -- same bug as scenario 1)");
        CHECK(memcmp(rawALT, A, 10) != 0, "ALT's result is contaminated here too");

        uint8_t rawMID[10] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == true,
              "MID (magnitude-only, still pre-seek) WRONGLY accepts: numbits-12=362 >= 160 looks sufficient overall, but 250 of those bits are before the late sync match at off=262, not after it");
        CHECK(memcmp(rawMID, A, 10) != 0,
              "MID's result is contaminated too -- the magnitude-only fix alone does not prevent cross-contamination");
        CHECK(rawMID[7] == 0xDE && rawMID[8] == 0xAD,
              "MID's contaminated bytes are the same 0xDE/0xAD marker -- confirms it read past numbits exactly like ALT");

        uint8_t rawNEU[10] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects: off(250+12)+160=422 > numbits(374)");
    }

    /* --- Scenario 3: genuine, fully-captured message (numbits covers the
     * whole sync+160-bit message, nothing borrowed from beyond numbits).
     * Confirms the fix does not regress the legitimate case: ALT, MID and
     * NEU must all accept it and decode message A exactly. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        for (int b = 0; b < 160; b++)
            bitmap_set(bits,numbytes,12+b, bitmap_get(pureA_stream,32,b));
        uint32_t numbits = 12 + 160; /* = 172: the whole message was really sampled */

        printf("\n=== Scenario 3: genuine full capture (numbits=%u), no regression check ===\n", numbits);

        uint8_t rawALT[10] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true && decALT == 80 && memcmp(rawALT,A,10) == 0,
              "ALT decodes the genuine full capture correctly");

        uint8_t rawMID[10] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == true && decMID == 80 && memcmp(rawMID,A,10) == 0,
              "MID decodes the genuine full capture correctly (no regression)");

        uint8_t rawNEU[10] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == true && decNEU == 80 && memcmp(rawNEU,A,10) == 0,
              "NEU decodes the same genuine full capture correctly (no regression)");
    }

    /* --- Scenario 4: grossly insufficient capture (not even enough bits
     * for the 20-bit sync pattern itself to be found within numbits).
     * Confirms all three variants reject it. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        uint32_t numbits = 10; /* less than sync_len=20 */

        printf("\n=== Scenario 4: grossly short capture (numbits=%u), all must reject ===\n", numbits);
        uint8_t rawALT[10] = {0}; uint32_t decALT = 0;
        CHECK(decode_ALT(bits, numbytes, numbits, rawALT, &decALT) == false, "ALT rejects a grossly short capture");
        uint8_t rawMID[10] = {0}; uint32_t decMID = 0;
        CHECK(decode_MID(bits, numbytes, numbits, rawMID, &decMID) == false, "MID rejects a grossly short capture");
        uint8_t rawNEU[10] = {0}; uint32_t decNEU = 0;
        CHECK(decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU) == false, "NEU rejects a grossly short capture");
    }

    printf("\n%d/%d checks passed\n", checks_passed, checks_total);
    if (checks_passed != checks_total) { printf("SOME CHECKS FAILED\n"); return 1; }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
