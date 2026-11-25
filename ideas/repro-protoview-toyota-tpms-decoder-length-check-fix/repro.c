/* Host repro for the ProtoView Toyota TPMS decoder length-check finding.
 *
 * Claim: protocols/tpms/toyota.c decode(), line
 *   if (numbits-6 < 64*2) return false; /+ Ask for 64 bit of data (each bit
 *                                          is two symbols in the bitmap). +/
 * has the same two compounding defects already documented for
 * protocols/tpms/citroen.c, protocols/tpms/ford.c, protocols/tpms/renault.c
 * and protocols/tpms/schrader_eg53ma4.c:
 *
 *  1. Wrong magnitude -- but a different flavor than the other four files.
 *     There the bug was a missing factor of 2 (comparing against the
 *     *decoded* bit count instead of the *raw* bitmap bit count). Here the
 *     factor of 2 is already applied ("64*2"), but the base constant itself
 *     is wrong: "64" is the number of decoded *data* bits the top-of-file
 *     doc comment still describes ("Preamble + sync + 64 bits of data."),
 *     but the actual implementation below reads a 9-byte raw[9] buffer and
 *     requires "decoded < 8*9" (=72, not 64) to accept it -- the format was
 *     extended by one byte (a trailing CRC byte, checked via
 *     "crc8(raw,8,0x80,7) != raw[8]") after the doc comment and this check
 *     were written, and the check was never updated to match. The real
 *     requirement is 8*9*2 (=144) raw bits after the sync match, not 128.
 *  2. Wrong timing: exactly as in the four already-documented files, the
 *     check runs *before* the "for (j = 0; sync[j]; j++)" loop even calls
 *     bitmap_seek_bits(), so it only verifies the *total* "numbits" under
 *     the implicit assumption that the sync match is at offset 0. A late
 *     match says nothing is caught by this check at all.
 *
 * Independent confirmation in the same file: "info->pulses_count =
 * (off+8*9*2) - info->start_off;" a few lines below already uses the
 * correct factor 8*9*2 for the exact same quantity of raw bits -- the
 * pre-check well above it does not, and uses a different, smaller base
 * constant (64 instead of 72) on top of that.
 *
 * A side note on the "-6" subtracted in the pre-check: the real post-match
 * skip is "off += strlen(sync[j])-2", which is 6 for the two 8-character
 * sync candidates ("00111100"/"00111101") but 7 for the two 9-character
 * ones ("001111100"/"001111101"). The pre-check's flat "-6" silently
 * assumes the shorter candidate always matches, a third, minor slack on
 * top of defect 1 -- not exercised separately below since the real fix
 * (checking after the seek, using the real "off") makes it moot regardless
 * of which candidate matched.
 *
 * Consequence: exactly as in the four already-documented TPMS findings,
 * decode_signal() in signal.c allocates the scratch bitmap passed to every
 * decoder with a bare malloc() (no memset), sized to a fixed 4096 bytes,
 * and passes that fixed byte size (not the real sampled "numbits") as the
 * length bound into convert_from_diff_manchester(). Because this check is
 * undersized and evaluated at the wrong point, Toyota TPMS decode() can
 * walk past the genuinely sampled "numbits" bits into that
 * uninitialized/leftover scratch memory and still report a complete,
 * CRC8-valid 9-byte Toyota TPMS message (Tire ID/Pressure/Temperature)
 * that is partly NOT derived from the RF signal actually captured in this
 * call -- in the worst case (scenario 1 below) the single byte that is
 * missing is the CRC byte itself, i.e. the one byte whose entire purpose
 * is to catch exactly this kind of corruption.
 *
 * All bitmap primitives, crc8(), and the decode() logic below are
 * transcribed verbatim from antirez/protoview commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f (signal.c, crc.c,
 * protocols/tpms/toyota.c). The only changes: FURI_LOG_E() calls and
 * ProtoViewFieldSet handling are stubbed out (no Flipper framework on the
 * host), and the decoded "raw[9]" bytes plus the symbol count are returned
 * to the caller via out-parameters so the test can inspect them. ALT keeps
 * the original (buggy) check; MID applies the magnitude fix only (64->72
 * decoded bits, i.e. 144 raw bits), but still pre-seek; NEU applies the
 * actual fix (relocated post-seek, correct magnitude). Every other line of
 * decode() logic -- sync search, differential-Manchester extraction,
 * CRC8 -- is identical across all three.
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

uint32_t convert_from_diff_manchester(uint8_t *buf, uint64_t buflen, uint8_t *bits, uint32_t len, uint32_t off, bool previous)
{
    uint32_t decoded = 0;
    len *= 8; /* Convert to bits. */
    for (uint32_t j = off; j < len; j += 2) {
        bool b0 = bitmap_get(bits,len,j);
        bool b1 = bitmap_get(bits,len,j+1);
        if (b0 == previous) break; /* Each new bit must switch value. */
        bitmap_set(buf,buflen,decoded++,b0 == b1);
        previous = b1;
        if (decoded/8 == buflen) break; /* No space left on target buffer. */
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

/* ============== Transcribed from protocols/tpms/toyota.c =============== */
/* Note: exactly as in the real decode_signal() call site in signal.c, every
 * call here passes "numbytes" (the scratch buffer's *byte* capacity, fixed
 * at 4096 bytes in the real app) as the "blen"/"len" argument to
 * bitmap_get/bitmap_seek_bits/convert_from_diff_manchester -- NOT "numbits"
 * (the number of actually-sampled bits). This is the real decoder's own
 * design, reproduced faithfully here. */

typedef enum { CHECK_ORIGINAL, CHECK_MAGNITUDE_ONLY, CHECK_FIXED } CheckMode;

static bool decode_core(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                         CheckMode mode, uint8_t *out_raw, uint32_t *out_decoded)
{
    if (mode == CHECK_ORIGINAL) {
        /* ALT: the real upstream line. "64" is the decoded-data-bit count
         * from the stale top-of-file doc comment (8 bytes); the actual
         * raw[9] buffer and the "decoded < 8*9" check three lines below
         * need 72 decoded bits (9 bytes), i.e. 144 raw bits -- not the 128
         * this check demands. Evaluated before the sync offset is known. */
        if (numbits-6 < 64*2) return false;
    } else if (mode == CHECK_MAGNITUDE_ONLY) {
        /* MID: the most natural "obvious" partial fix -- corrects the
         * magnitude to the real 8*9*2 (=144) raw bits, but is still
         * evaluated before bitmap_seek_bits(), so it implicitly assumes
         * the sync match is at offset 0. */
        if (numbits-6 < 8*9*2) return false;
    }
    /* CHECK_FIXED (NEU) does no pre-seek check at all here -- its check
     * runs below, after the real offset is known. */

    char *sync[] = {
        "00111100",
        "001111100",
        "00111101",
        "001111101",
        NULL
    };

    int j;
    uint32_t off = 0;
    for (j = 0; sync[j]; j++) {
        off = bitmap_seek_bits(bits,numbytes,0,numbits,sync[j]);
        if (off != BITMAP_SEEK_NOT_FOUND) {
            off += strlen(sync[j])-2;
            break;
        }
    }
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    if (mode == CHECK_FIXED) {
        /* NEU: the actual fix. Checked *after* the seek, using the real
         * match offset "off", with the corrected raw-bit magnitude
         * (8*9*2, two raw bits per differentially-Manchester-coded
         * decoded bit). Also sidesteps the "-6 vs -7" slack noted above,
         * since it uses the real "off" directly. */
        if (off + 8*9*2 > numbits) return false;
    }

    uint8_t raw[9];
    uint32_t decoded =
        convert_from_diff_manchester(raw,sizeof(raw),bits,numbytes,off,true);

    if (decoded < 8*9) return false; /* Require the full 8 bytes. (sic, real
                                         upstream comment -- 8*9 is actually
                                         9 bytes.) */
    if (crc8(raw,8,0x80,7) != raw[8]) return false; /* Require sane CRC. */

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

/* Differentially-Manchester-encode "nbytes" bytes of "data" (MSB-first per
 * byte, matching bitmap_get/bitmap_set's bit numbering) into "outbits"
 * starting at bit offset "outoff", given that the symbol immediately
 * preceding "outoff" had value "previous" -- the exact inverse of
 * convert_from_diff_manchester()'s decode rule above: each 2-symbol pair
 * starts with a forced transition (b0 != previous), and then b1 == b0 iff
 * the data bit is 1. */
static void diffmanchester_encode(const uint8_t *data, int nbytes, bool previous,
                                   uint8_t *outbits, uint32_t outbuflen, uint32_t outoff)
{
    for (int k = 0; k < nbytes*8; k++) {
        bool v = bitmap_get((uint8_t*)data, nbytes, k);
        bool b0 = !previous;
        bool b1 = v ? b0 : previous;
        bitmap_set(outbits, outbuflen, outoff + k*2 + 0, b0);
        bitmap_set(outbits, outbuflen, outoff + k*2 + 1, b1);
        previous = b1;
    }
}

static void write_sync_literal(uint8_t *bits, uint32_t buflen, uint32_t pos, const char *pat) {
    for (size_t j = 0; pat[j]; j++)
        bitmap_set(bits,buflen,pos+j, pat[j]=='1');
}

static int checks_passed = 0, checks_total = 0;
#define CHECK(cond, desc) do { \
    checks_total++; \
    if (cond) { checks_passed++; printf("[PASS] %s\n", desc); } \
    else printf("[FAIL] %s\n", desc); \
} while(0)

int main(void) {
    const uint32_t numbytes = 128; /* 1024 bits scratch, plenty for every scenario below. */
    const char *sync0 = "00111100"; /* 8 chars -> off = start_off + 6. */

    /* "Message A": the genuine captured message. raw[0]'s MSB is forced to
     * 1 so that the first differentially-Manchester-encoded data symbol
     * pair (b0=0,b1=0, since previous=true going in) matches the literal
     * last two characters of sync0 ("..1100"[-2:] == "00") at the exact
     * position where the encoded payload begins -- exactly mirroring how
     * the real sync string's tail doubles as the first forced symbol in
     * the real decode(). CRC8(poly=7,init=0x80) of bytes 0..7 equals
     * byte 8, i.e. a valid Toyota TPMS CRC. */
    uint8_t A[9] = {0xc2,0x34,0x56,0x78,0xaa,0x55,0x3c,0x00,0x00};
    A[8] = crc8(A,8,0x80,7);

    uint8_t pureA_stream[40] = {0};
    diffmanchester_encode(A, 9, true, pureA_stream, sizeof(pureA_stream), 0); /* 144 bits */

    /* --- Scenario 1: sync match at offset 0 (the common case), capture
     * truncated right after the 8th real byte -- i.e. only the CRC byte
     * (raw[8]) itself is missing from the real RF capture. 128 real raw
     * bits is exactly ALT's (wrong) threshold, so ALT wrongly lets this
     * through; MID (magnitude fixed to 144, still pre-seek) correctly
     * rejects it here since 128 real raw bits genuinely fall short of 144.
     * The stale raw[8] is deliberately set to the one value that makes
     * this the worst case: the actual CRC8 of the real bytes 0..7, i.e.
     * leftover scratch memory that happens (worst case) to validate the
     * very field whose job is to catch corruption. */
    {
        uint8_t bits[128] = {0};
        write_sync_literal(bits, numbytes, 0, sync0);
        for (int b = 0; b < 144; b++)
            bitmap_set(bits,numbytes,6+b, bitmap_get(pureA_stream,40,b));
        uint32_t numbits = 6 + 128; /* = 134: only 8 of 9 bytes are "real" */

        printf("\n=== Scenario 1: off=0, truncated after byte 8 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        printf("ALT raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            rawALT[0],rawALT[1],rawALT[2],rawALT[3],rawALT[4],rawALT[5],rawALT[6],rawALT[7],rawALT[8]);
        CHECK(okALT == true, "ALT (buggy check) reports a successful 9-byte decode from a 134-bit capture");
        CHECK(decALT == 72, "ALT decoded exactly 72 bits (the full 9-byte message)");
        CHECK(memcmp(rawALT, A, 9) == 0,
              "ALT's reported message happens to equal A here only because the stale CRC byte was deliberately solved to match -- the real signal never provided raw[8]");
        CHECK(rawALT[0] == A[0] && rawALT[4] == A[4] && rawALT[5] == A[5] && rawALT[6] == A[6],
              "ALT's Tire-ID/Pressure/Temperature bytes are the real captured values (they were within numbits)");

        uint8_t rawMID[9] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == false,
              "MID (magnitude fix only) correctly rejects this offset-0, byte-aligned case");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects the same 134-bit truncated capture");
    }

    /* --- Scenario 2: sync match found late (off=250), truncated after the
     * 6th real byte (raw[6]/raw[7] contaminated with a 0xDE/0xAD marker,
     * raw[8]'s CRC solved over the hybrid). numbits is large enough that
     * even MID's corrected-magnitude-but-still-pre-seek check passes (it
     * only looks at the *total* numbits, not at where the match actually
     * landed) -- proving that fixing the magnitude alone is still not
     * enough without also fixing the timing. Only NEU, which re-checks
     * after the seek using the real offset, catches this. Unlike scenario
     * 1, the contamination here reaches displayed fields: Temperature
     * depends on raw[5]/raw[6], and raw[6] is the 0xDE marker. */
    {
        uint8_t hybrid[9];
        memcpy(hybrid, A, 6);
        hybrid[6] = 0xDE;
        hybrid[7] = 0xAD;
        hybrid[8] = crc8(hybrid,8,0x80,7);
        CHECK(memcmp(hybrid, A, 9) != 0, "setup: hybrid message differs from real message A");

        uint8_t hybrid_stream[40] = {0};
        diffmanchester_encode(hybrid, 9, true, hybrid_stream, sizeof(hybrid_stream), 0); /* 144 bits */

        uint8_t bits[128] = {0};
        /* bits[0,250) left at 0 -- sync0 contains a run of 1s, so it cannot
         * match anywhere in an all-zero prefix, meaning the first real
         * match is at 250. */
        write_sync_literal(bits, numbytes, 250, sync0);
        for (int b = 0; b < 144; b++)
            bitmap_set(bits,numbytes,250+6+b, bitmap_get(hybrid_stream,40,b));
        uint32_t numbits = 250 + 6 + 96; /* = 352: only 6 of 9 bytes are "real" */

        printf("\n=== Scenario 2: off=250 (late match), truncated after byte 6 (numbits=%u) ===\n", numbits);

        uint8_t rawALT[9] = {0}; uint32_t decALT = 0;
        bool okALT = decode_ALT(bits, numbytes, numbits, rawALT, &decALT);
        CHECK(okALT == true, "ALT (buggy check) also succeeds here (expected -- same bug as scenario 1)");
        CHECK(memcmp(rawALT, A, 9) != 0, "ALT's result is contaminated here too");

        uint8_t rawMID[9] = {0}; uint32_t decMID = 0;
        bool okMID = decode_MID(bits, numbytes, numbits, rawMID, &decMID);
        CHECK(okMID == true,
              "MID (magnitude fix, still pre-seek) WRONGLY accepts: numbits-6=346 >= 144 looks sufficient overall, but 250 of those bits are before the late sync match at off=250, not after it");
        CHECK(memcmp(rawMID, A, 9) != 0,
              "MID's result is contaminated too -- fixing the magnitude alone does not prevent cross-contamination");
        CHECK(rawMID[6] == 0xDE && rawMID[7] == 0xAD,
              "MID's contaminated Temperature-affecting byte and its neighbour are exactly the 0xDE/0xAD marker -- confirms it read past numbits exactly like ALT");

        uint8_t rawNEU[9] = {0}; uint32_t decNEU = 0;
        bool okNEU = decode_NEU(bits, numbytes, numbits, rawNEU, &decNEU);
        CHECK(okNEU == false,
              "NEU (actual fix) correctly rejects: off(250+6)+144=400 > numbits(352)");
    }

    /* --- Scenario 3: genuine, fully-captured message (numbits covers the
     * whole sync+144-bit message, nothing borrowed from beyond numbits).
     * Confirms the fix does not regress the legitimate case: ALT, MID and
     * NEU must all accept it and decode message A exactly. */
    {
        uint8_t bits[128] = {0};
        write_sync_literal(bits, numbytes, 0, sync0);
        for (int b = 0; b < 144; b++)
            bitmap_set(bits,numbytes,6+b, bitmap_get(pureA_stream,40,b));
        uint32_t numbits = 6 + 144; /* = 150: the whole message was really sampled */

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
     * for the shortest 8-bit sync candidate itself to be found within
     * numbits). Confirms all three variants reject it. */
    {
        uint8_t bits[128] = {0};
        write_sync_literal(bits, numbytes, 0, sync0);
        uint32_t numbits = 4; /* less than strlen(sync0)=8 */

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
