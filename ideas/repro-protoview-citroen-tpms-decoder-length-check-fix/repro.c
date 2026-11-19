/* Host repro for the ProtoView Citroen TPMS decoder length-check finding.
 *
 * Claim: protocols/tpms/citroen.c decode(), line
 *   if (numbits-sync_len < 8*10) return false; -- "Expect 10 bytes."
 * has two compounding defects:
 *
 *  1. Wrong magnitude: "8*10" (=80) is the number of *decoded* data bits
 *     (10 bytes), but the payload is Manchester-coded ("01"/"10" patterns
 *     in convert_from_line_code()), which consumes *two* raw bitmap bits
 *     per decoded bit. The real requirement is 8*10*2 (=160) raw bits
 *     after the sync match, not 80. This is a different defect from the
 *     already-documented KeeLoq finding (a no-op check that ignores
 *     sync_len algebraically) -- here sync_len is used correctly, but the
 *     payload-length term is short by exactly half.
 *  2. Wrong timing: the check runs *before* bitmap_seek_bits() finds the
 *     actual sync offset, so it only verifies the *total* numbits under
 *     the implicit assumption that the sync match is at offset 0. If the
 *     match is found later in the buffer (startpos=0, but the search can
 *     return any offset up to numbits-sync_len), the check says nothing
 *     about whether enough real bits remain *after* that offset.
 *
 * Consequence: exactly as in the already-documented KeeLoq finding,
 * decode_signal() in signal.c allocates the scratch bitmap passed to every
 * decoder with a bare malloc() (no memset), sized to a fixed 4096 bytes,
 * and passes that fixed byte size (not the real sampled "numbits") as the
 * length bound into convert_from_line_code(). Because this check is both
 * undersized and evaluated at the wrong point, Citroen TPMS decode() can
 * walk past the genuinely sampled "numbits" bits into that uninitialized/
 * leftover scratch memory and still report a complete, checksum-valid
 * 10-byte Citroen TPMS message (Tire ID/Pressure/Temperature/Battery) that
 * is partly NOT derived from the RF signal actually captured in this call.
 *
 * This repro also demonstrates that a "naive" fix which only corrects the
 * magnitude (8*10*2 instead of 8*10) but leaves the check running *before*
 * the seek (MID below) is NOT sufficient on its own: it only closes the
 * hole for the common case where the sync match happens to be found at
 * offset 0. The actual fix (NEU) re-checks *after* the seek, using the
 * real match offset, with the corrected magnitude.
 *
 * All bitmap primitives and the decode() logic below are transcribed
 * verbatim from antirez/protoview commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f (signal.c,
 * protocols/tpms/citroen.c). The only changes: FURI_LOG_E() calls and
 * ProtoViewFieldSet handling are stubbed out (no Flipper framework on the
 * host), and the decoded "raw[10]" bytes plus the symbol count are
 * returned to the caller via out-parameters so the test can inspect them.
 * ALT keeps the original (buggy) check; MID applies only the magnitude
 * fix, still pre-seek; NEU applies the actual fix (relocated post-seek,
 * correct magnitude). Every other line of decode() logic -- sync search,
 * Manchester extraction, XOR checksum -- is identical across all three.
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

/* ================ Transcribed from protocols/tpms/citroen.c ================ */
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
    uint32_t sync_len = 17;
    const char *sync_pattern = "10101010101010110";

    if (mode == CHECK_ORIGINAL) {
        /* ALT: the real upstream line. Compares against 8*10 (=80), the
         * *decoded* bit count, not the 160 raw bits actually consumed by
         * convert_from_line_code() for Manchester-coded data, and runs
         * before the sync offset is even known. */
        if (numbits-sync_len < 8*10) return false;
    } else if (mode == CHECK_MAGNITUDE_ONLY) {
        /* MID: corrects the magnitude (8*10*2 raw bits) but is still
         * evaluated before bitmap_seek_bits(), so it implicitly assumes
         * the sync match is at offset 0. */
        if (numbits-sync_len < 8*10*2) return false;
    }
    /* CHECK_FIXED (NEU) does no pre-seek check at all here -- its check
     * runs below, after the real offset is known. */

    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    off += sync_len; /* Skip preamble + sync. */

    if (mode == CHECK_FIXED) {
        /* NEU: the actual fix. Checked *after* the seek, using the real
         * match offset "off", with the corrected raw-bit magnitude
         * (8*10*2, two raw bits per Manchester-coded decoded bit). */
        if (off + 8*10*2 > numbits) return false;
    }

    uint8_t raw[10];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester. */

    if (decoded < 8*10) return false; /* Require the full 10 bytes. */

    uint8_t crc = 0;
    for (int j = 1; j < 10; j++) crc ^= raw[j];
    if (crc != 0) return false; /* Require sane checksum. */

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
 * convention as citroen.c's convert_from_line_code() call. */
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
    const char *sync_pattern = "10101010101010110";
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

    /* "Message A": the genuine captured message (XOR of bytes 1..9 is 0,
     * i.e. a valid Citroen TPMS checksum). */
    uint8_t A[10] = {0x00,0x11,0x22,0x33,0x44,0x03,0x50,0x46,0x64,0x35};
    {
        uint8_t x = 0;
        for (int j = 1; j < 9; j++) x ^= A[j];
        assert((x ^ A[9]) == 0); /* sanity: A[9] is the correct checksum byte */
    }

    /* "Hybrid": bytes 0..6 are A's real bytes; bytes 7..9 are a distinct,
     * clearly-marked "contamination" payload (0xDE/0xAD) representing
     * unrelated data left over in the scratch buffer from a previous
     * decode_signal() call. B9 is solved so the *hybrid* still passes the
     * XOR checksum -- exactly the dangerous case: cross-contaminated data
     * that still looks like a well-formed, checksum-valid message. */
    uint8_t hybrid[10];
    memcpy(hybrid, A, 7);
    hybrid[7] = 0xDE;
    hybrid[8] = 0xAD;
    {
        uint8_t x = 0;
        for (int j = 1; j < 7; j++) x ^= A[j];
        x ^= hybrid[7];
        x ^= hybrid[8];
        hybrid[9] = x;
    }
    {
        uint8_t x = 0;
        for (int j = 1; j < 9; j++) x ^= hybrid[j];
        assert((x ^ hybrid[9]) == 0); /* sanity: hybrid checksum is valid too */
    }
    CHECK(memcmp(hybrid, A, 10) != 0, "setup: hybrid message differs from real message A");

    uint8_t hybrid_stream[32] = {0};
    manchester_encode(hybrid, 10, hybrid_stream, sizeof(hybrid_stream), 0); /* 160 bits */
    uint8_t pureA_stream[32] = {0};
    manchester_encode(A, 10, pureA_stream, sizeof(pureA_stream), 0); /* 160 bits */

    /* --- Scenario 1: sync match at offset 0 (the common case), capture
     * truncated right after the 7th real byte. Real signal provides only
     * 7 of the 10 bytes (112 of the 160 Manchester raw bits); the
     * remaining 3 bytes (48 raw bits, containing the 0xDE/0xAD marker)
     * come from stale scratch memory beyond "numbits". ALT's check only
     * compares against 80 *decoded* bits (should be 160 *raw* bits), so it
     * wrongly lets this through even though it is evaluated at offset 0,
     * the one case a magnitude-only fix (MID) is actually able to catch. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        for (int b = 0; b < 160; b++)
            bitmap_set(bits,numbytes,17+b, bitmap_get(hybrid_stream,32,b));
        uint32_t numbits = 17 + 112; /* = 129: only 7 of 10 bytes are "real" */

        printf("\n=== Scenario 1: off=0, truncated after byte 7 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[10] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        printf("ALT raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            rawALT[0],rawALT[1],rawALT[2],rawALT[3],rawALT[4],rawALT[5],rawALT[6],rawALT[7],rawALT[8],rawALT[9]);
        CHECK(okALT == true, "ALT (buggy check) reports a successful 10-byte decode from a 129-bit capture");
        CHECK(decALT == 80, "ALT decoded exactly 80 bits (the full 10-byte message)");
        CHECK(memcmp(rawALT, A, 10) != 0,
              "ALT's reported message differs from the real captured message A (contaminated)");
        CHECK(rawALT[7] == 0xDE && rawALT[8] == 0xAD,
              "ALT's Temperature/Battery bytes (raw[7],raw[8]) are exactly the 0xDE/0xAD marker -- proves they came from beyond numbits, not from the captured RF signal");
        CHECK(rawALT[1] == A[1] && rawALT[4] == A[4],
              "ALT's Tire ID bytes (raw[1..4]) still match the real capture A -- contamination is localized to the tail, as expected");

        uint8_t rawMID[10] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == false,
              "MID (magnitude-only fix) correctly rejects this offset-0 case");

        uint8_t rawNEU[10] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects the same 129-bit truncated capture");
    }

    /* --- Scenario 2: sync match found late (off=250), same 7-real/3-stale
     * byte split. numbits is large enough that even MID's corrected-
     * magnitude-but-still-pre-seek check passes (it only looks at the
     * *total* numbits, not at where the match actually landed) -- proving
     * that the magnitude fix alone is not enough. Only NEU, which re-checks
     * after the seek using the real offset, catches this. */
    {
        uint8_t bits[128] = {0};
        /* bits[0,250) left at 0 -- cannot match "10101010101010110" (starts
         * with '1') at any position, so the first and only match is at 250. */
        write_sync(bits, numbytes, 250);
        for (int b = 0; b < 160; b++)
            bitmap_set(bits,numbytes,250+17+b, bitmap_get(hybrid_stream,32,b));
        uint32_t numbits = 250 + 17 + 112; /* = 379: only 7 of 10 bytes are "real" */

        printf("\n=== Scenario 2: off=250 (late match), truncated after byte 7 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[10] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true, "ALT (buggy check) also succeeds here (expected -- same bug as scenario 1)");
        CHECK(memcmp(rawALT, A, 10) != 0, "ALT's result is contaminated here too");

        uint8_t rawMID[10] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == true,
              "MID (magnitude-only, still pre-seek) WRONGLY accepts: numbits-17=362 >= 160 looks sufficient overall, but 48 of those bits are before the late sync match at off=250, not after it");
        CHECK(memcmp(rawMID, A, 10) != 0,
              "MID's result is contaminated too -- the magnitude-only fix alone does not prevent cross-contamination");
        CHECK(rawMID[7] == 0xDE && rawMID[8] == 0xAD,
              "MID's Temperature/Battery bytes are the same 0xDE/0xAD marker -- confirms it read past numbits exactly like ALT");

        uint8_t rawNEU[10] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects: off(250)+160=410 > numbits(379)");
    }

    /* --- Scenario 3: genuine, fully-captured message (numbits covers the
     * whole sync+160-bit message, nothing borrowed from beyond numbits).
     * Confirms the fix does not regress the legitimate case: ALT, MID and
     * NEU must all accept it and decode message A exactly. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        for (int b = 0; b < 160; b++)
            bitmap_set(bits,numbytes,17+b, bitmap_get(pureA_stream,32,b));
        uint32_t numbits = 17 + 160; /* = 177: the whole message was really sampled */

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
     * for the 17-bit sync pattern itself to be found within numbits).
     * Confirms all three variants reject it. */
    {
        uint8_t bits[128] = {0};
        write_sync(bits, numbytes, 0);
        uint32_t numbits = 10; /* less than sync_len=17 */

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
