// Host repro for the can_transceiver (Chainmanner/Flipper-CAN-Transceiver) RX-history
// entry builder, scenes/can_transceiver_scene_canrx.c,
// CANTransceiver_SceneCANRx_MessageRxCallback().
//
// Transcribes the exact struct layout from can_transceiver_rxhistory.h
// (CANTransceiver_RxHistory_Entry, three bitfields sharing one byte:
// u4FrameDataLen:4, bExtendedIDFrame:1, bRemoteTransferFrame:1, reserved:2) and the exact
// field-population sequence from the real callback:
//
//   CANTransceiver_RxHistory_Entry pNewEntry;                 // local, NOT initialized
//   pNewEntry.u32FrameID = u32FrameID_Rx & 0x1FFFFFFF;
//   if (u32FrameID_Rx & 0x80000000) pNewEntry.bExtendedIDFrame = true;      // only ever set true
//   if (u32FrameID_Rx & 0x40000000) pNewEntry.bRemoteTransferFrame = true; // only ever set true
//   pNewEntry.u8pFrameData[0..7] = ...;
//   pNewEntry.u4FrameDataLen = u8FrameDataLen_Rx & 0x0F;
//   pNewEntry.u32Timestamp = furi_get_tick();
//   CANTransceiver_RxHistory_AddEntry(pInstance->pRxHistory, &pNewEntry);   // raw memcpy of pNewEntry
//
// Neither bExtendedIDFrame nor bRemoteTransferFrame has an "else" branch clearing it to false.
// pNewEntry is a plain automatic (stack) variable with no initializer and no memset -- when a
// received CAN frame is a plain standard, non-remote data frame (both marker bits 0 in
// u32FrameID_Rx), both flag bits keep whatever indeterminate value already occupied that byte
// on the stack. Because u4FrameDataLen is a sibling bitfield in the very same byte, the
// compiler implements "pNewEntry.u4FrameDataLen = ..." as a read-modify-write of that whole
// byte -- it does NOT touch the other two bits, so whatever was already there (real stack
// garbage from whichever function ran there before) survives into the entry that gets copied
// into the RX history array, displayed on-screen (view_canrx.c's "F:X"/"F:R" flag column) and
// written into the CSV capture export.
//
// Forcing an automatic variable's real initial stack garbage to a specific byte pattern on a
// host build is inherently compiler-/ABI-/sanitizer-dependent and not something to rely on for
// a repro. Instead, FillPriorContent() makes "whatever was already in that memory" an explicit,
// controlled input (a memset over the whole entry before the real field-population sequence
// runs) standing in for it -- the mechanism under test (a conditional bitfield write with no
// "else", followed by a sibling bitfield's unconditional read-modify-write of the same byte) is
// identical regardless of how that prior content actually got there on real hardware.
//
// Scenarios:
//  1. Regression -- both marker bits SET (genuine extended + remote frame): ALT and FIX agree
//     (bExtendedIDFrame == bRemoteTransferFrame == true) regardless of prior content, since both
//     bits are explicitly written true on this path. The bug only hides the "false" case.
//  2. BUG repro -- a plain standard, non-remote data frame (both marker bits 0), prior content
//     set to several different non-zero byte patterns before the builder's field-population
//     sequence runs: ALT reports bExtendedIDFrame/bRemoteTransferFrame as whatever bits happened
//     to be set in that pattern -- a real standard data frame is misrepresented as extended
//     and/or remote, entirely dependent on unrelated prior memory content. FIX always reports
//     false/false.
//  3. Confirms the corruption is confined to the two conditionally-set bits: u4FrameDataLen
//     (always unconditionally assigned) and u32FrameID/u8pFrameData/u32Timestamp come out
//     correct under ALT too -- this is not a general memory-safety bug, it is specifically the
//     two "only ever set true" bitfields.
//  4. FIX verification across the full byte-poison range (0x00..0xFF): with the fix (memset the
//     whole entry to 0 before populating it), the two flag bits are always exactly what the
//     input's marker bits say, independent of the poison byte -- 256/256 patterns correct, vs.
//     only the 64 patterns with both relevant bits already 0 being correct under ALT (confirmed
//     by exhaustive sweep).

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ==== Exact struct from can_transceiver_rxhistory.h ====
typedef struct {
    uint32_t u32FrameID;
    uint8_t u8pFrameData[8];
    uint32_t u32Timestamp;
    uint8_t u4FrameDataLen : 4;
    uint8_t bExtendedIDFrame : 1;
    uint8_t bRemoteTransferFrame : 1;
    uint8_t reserved : 2;
} CANTransceiver_RxHistory_Entry;

// ==== "Prior stack content" helper ====
// The real pNewEntry is a plain automatic variable with no initializer: whatever bytes happen
// to occupy that stack slot when the function is entered (leftovers from whichever function
// ran there before -- IRQ handling, a previous scene callback, anything) are its initial
// value. That is inherently non-portable to force via actual stack reuse across a host build
// (compiler/ABI/sanitizer dependent), so instead of hoping for accidental reuse, this repro
// makes "prior content" an explicit, controlled input: pPriorContent stands in for whatever
// u8PoisonPattern-filled stack bytes a real run would have left behind. The mechanism being
// proven (a conditional bitfield write with no "else", followed by a sibling bitfield's
// unconditional read-modify-write of the same byte) is identical either way -- only the source
// of the initial byte value differs.
static void FillPriorContent(CANTransceiver_RxHistory_Entry* pEntry, uint8_t u8Pattern) {
    memset(pEntry, u8Pattern, sizeof(*pEntry));
}

// ==== ALT: faithful transcription of the shipped callback (bug present) ====
static void BuildEntry_ALT(
    uint32_t u32FrameID_Rx,
    uint8_t u8FrameDataLen_Rx,
    const uint8_t* u8pFrameData_Rx,
    CANTransceiver_RxHistory_Entry* pOut,
    uint8_t u8PriorContentPattern) {
    CANTransceiver_RxHistory_Entry pNewEntry; // NOTE: no initializer, no memset -- as shipped.
    FillPriorContent(&pNewEntry, u8PriorContentPattern); // stands in for indeterminate initial stack content.

    pNewEntry.u32FrameID = u32FrameID_Rx & 0x1FFFFFFF;
    if (u32FrameID_Rx & 0x80000000)
        pNewEntry.bExtendedIDFrame = true;
    if (u32FrameID_Rx & 0x40000000)
        pNewEntry.bRemoteTransferFrame = true;
    pNewEntry.u8pFrameData[0] = u8pFrameData_Rx[0];
    pNewEntry.u8pFrameData[1] = u8pFrameData_Rx[1];
    pNewEntry.u8pFrameData[2] = u8pFrameData_Rx[2];
    pNewEntry.u8pFrameData[3] = u8pFrameData_Rx[3];
    pNewEntry.u8pFrameData[4] = u8pFrameData_Rx[4];
    pNewEntry.u8pFrameData[5] = u8pFrameData_Rx[5];
    pNewEntry.u8pFrameData[6] = u8pFrameData_Rx[6];
    pNewEntry.u8pFrameData[7] = u8pFrameData_Rx[7];
    pNewEntry.u4FrameDataLen = u8FrameDataLen_Rx & 0x0F;
    pNewEntry.u32Timestamp = 0; // stand-in for furi_get_tick(), irrelevant to the bug.

    *pOut = pNewEntry; // stand-in for CANTransceiver_RxHistory_AddEntry()'s memcpy.
}

// ==== FIX: same sequence, additive memset up front ====
static void BuildEntry_FIX(
    uint32_t u32FrameID_Rx,
    uint8_t u8FrameDataLen_Rx,
    const uint8_t* u8pFrameData_Rx,
    CANTransceiver_RxHistory_Entry* pOut,
    uint8_t u8PriorContentPattern) {
    CANTransceiver_RxHistory_Entry pNewEntry;
    FillPriorContent(&pNewEntry, u8PriorContentPattern); // same prior content as ALT, see below.
    memset(&pNewEntry, 0, sizeof(pNewEntry)); // NEU -- overrides it regardless of what was there.

    pNewEntry.u32FrameID = u32FrameID_Rx & 0x1FFFFFFF;
    if (u32FrameID_Rx & 0x80000000)
        pNewEntry.bExtendedIDFrame = true;
    if (u32FrameID_Rx & 0x40000000)
        pNewEntry.bRemoteTransferFrame = true;
    pNewEntry.u8pFrameData[0] = u8pFrameData_Rx[0];
    pNewEntry.u8pFrameData[1] = u8pFrameData_Rx[1];
    pNewEntry.u8pFrameData[2] = u8pFrameData_Rx[2];
    pNewEntry.u8pFrameData[3] = u8pFrameData_Rx[3];
    pNewEntry.u8pFrameData[4] = u8pFrameData_Rx[4];
    pNewEntry.u8pFrameData[5] = u8pFrameData_Rx[5];
    pNewEntry.u8pFrameData[6] = u8pFrameData_Rx[6];
    pNewEntry.u8pFrameData[7] = u8pFrameData_Rx[7];
    pNewEntry.u4FrameDataLen = u8FrameDataLen_Rx & 0x0F;
    pNewEntry.u32Timestamp = 0;

    *pOut = pNewEntry;
}

typedef void (*BuilderFn)(uint32_t, uint8_t, const uint8_t*, CANTransceiver_RxHistory_Entry*, uint8_t);

static CANTransceiver_RxHistory_Entry RunWithPoison(
    BuilderFn pBuilder,
    uint8_t u8PoisonPattern,
    uint32_t u32FrameID_Rx,
    uint8_t u8FrameDataLen_Rx,
    const uint8_t* u8pFrameData_Rx) {
    CANTransceiver_RxHistory_Entry out;
    memset(&out, 0xAB, sizeof(out)); // scratch the *output* slot too, must be fully overwritten either way.
    pBuilder(u32FrameID_Rx, u8FrameDataLen_Rx, u8pFrameData_Rx, &out, u8PoisonPattern);
    return out;
}

static void PrintResult(const char* pLabel, CANTransceiver_RxHistory_Entry e) {
    printf(
        "%-28s ID=0x%08lx Len=%u Ext=%u Rem=%u\n",
        pLabel,
        (unsigned long)e.u32FrameID,
        (unsigned)e.u4FrameDataLen,
        (unsigned)e.bExtendedIDFrame,
        (unsigned)e.bRemoteTransferFrame);
}

int main(void) {
    const uint8_t u8pData[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

    printf("=== Scenario 1: regression, genuine extended+remote frame ===\n");
    {
        uint32_t u32FrameID_Rx = 0x80000000u | 0x40000000u | 0x123u;
        uint8_t u8PoisonPatterns[] = {0x00, 0xFF, 0x55, 0xAA};
        for (size_t i = 0; i < sizeof(u8PoisonPatterns); i++) {
            CANTransceiver_RxHistory_Entry alt =
                RunWithPoison(BuildEntry_ALT, u8PoisonPatterns[i], u32FrameID_Rx, 8, u8pData);
            CANTransceiver_RxHistory_Entry fix =
                RunWithPoison(BuildEntry_FIX, u8PoisonPatterns[i], u32FrameID_Rx, 8, u8pData);
            assert(alt.bExtendedIDFrame == true && alt.bRemoteTransferFrame == true);
            assert(fix.bExtendedIDFrame == true && fix.bRemoteTransferFrame == true);
        }
        printf("OK: explicitly-true path unaffected by poison, ALT == FIX, 4/4 poison patterns.\n\n");
    }

    printf("=== Scenario 2: BUG -- plain standard, non-remote frame, varying stack poison ===\n");
    {
        uint32_t u32FrameID_Rx = 0x123u; // both marker bits 0: real frame is neither extended nor remote.
        uint8_t u8PoisonPatterns[] = {0xFF, 0x55, 0xAA, 0x01, 0x02, 0x03};
        int iBugObserved = 0;
        for (size_t i = 0; i < sizeof(u8PoisonPatterns); i++) {
            CANTransceiver_RxHistory_Entry alt =
                RunWithPoison(BuildEntry_ALT, u8PoisonPatterns[i], u32FrameID_Rx, 8, u8pData);
            CANTransceiver_RxHistory_Entry fix =
                RunWithPoison(BuildEntry_FIX, u8PoisonPatterns[i], u32FrameID_Rx, 8, u8pData);

            char i8pLabel[48];
            snprintf(i8pLabel, sizeof(i8pLabel), "ALT poison=0x%02x", u8PoisonPatterns[i]);
            PrintResult(i8pLabel, alt);
            snprintf(i8pLabel, sizeof(i8pLabel), "FIX poison=0x%02x", u8PoisonPatterns[i]);
            PrintResult(i8pLabel, fix);

            // FIX must always report the true state of this standard, non-remote frame.
            assert(fix.bExtendedIDFrame == false);
            assert(fix.bRemoteTransferFrame == false);

            if (alt.bExtendedIDFrame != false || alt.bRemoteTransferFrame != false)
                iBugObserved++;
        }
        printf(
            "\nALT misreported Ext/Rem flags for %d of %zu poison patterns (expected: all nonzero "
            "patterns with either relevant bit set trigger it).\n",
            iBugObserved,
            sizeof(u8PoisonPatterns));
        assert(iBugObserved > 0);
        printf("OK: bug reproduced -- ALT's flags depend on unrelated prior stack content, FIX always correct.\n\n");
    }

    printf("=== Scenario 3: corruption confined to the two conditionally-set bits ===\n");
    {
        uint32_t u32FrameID_Rx = 0x1ABCDu;
        CANTransceiver_RxHistory_Entry alt = RunWithPoison(BuildEntry_ALT, 0xFF, u32FrameID_Rx, 5, u8pData);
        assert(alt.u32FrameID == (u32FrameID_Rx & 0x1FFFFFFF));
        assert(alt.u4FrameDataLen == 5); // unconditionally assigned, poison has no effect here.
        for (int i = 0; i < 8; i++)
            assert(alt.u8pFrameData[i] == u8pData[i]);
        printf("OK: FrameID/DataLen/FrameData all correct even under ALT -- only the two flag bits are affected.\n\n");
    }

    printf("=== Scenario 4: FIX verified across the full byte-poison range (0x00..0xFF) ===\n");
    {
        uint32_t u32FrameID_Rx = 0x321u; // standard, non-remote.
        int iAltCorrect = 0, iFixCorrect = 0;
        for (int iPoison = 0; iPoison <= 0xFF; iPoison++) {
            CANTransceiver_RxHistory_Entry alt =
                RunWithPoison(BuildEntry_ALT, (uint8_t)iPoison, u32FrameID_Rx, 8, u8pData);
            CANTransceiver_RxHistory_Entry fix =
                RunWithPoison(BuildEntry_FIX, (uint8_t)iPoison, u32FrameID_Rx, 8, u8pData);
            if (alt.bExtendedIDFrame == false && alt.bRemoteTransferFrame == false)
                iAltCorrect++;
            if (fix.bExtendedIDFrame == false && fix.bRemoteTransferFrame == false)
                iFixCorrect++;
        }
        printf("ALT correct for %d/256 poison bytes, FIX correct for %d/256.\n", iAltCorrect, iFixCorrect);
        assert(iFixCorrect == 256);
        assert(iAltCorrect < 256);
        printf("OK: FIX is deterministic (256/256), ALT depends on the poison byte (%d/256).\n\n", iAltCorrect);
    }

    printf("ALL SCENARIOS PASSED.\n");
    return 0;
}
