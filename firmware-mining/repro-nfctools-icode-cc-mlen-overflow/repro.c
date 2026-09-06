// Host repro for the nfctools icode.c NDEF-write CC.MLEN truncation bug.
// Reduces iso15693_write_ndef_blocks() in core/nfc_tools_icode.c to the pure
// MLEN arithmetic (no NFC/poller/BitBuffer dependency).
//
// OLD: uint8_t mlen = (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
//      -- silently wraps when the true value exceeds 0xFF.
// NEW: compute the same 32-bit value, refuse to proceed if it does not fit
//      in the 4-byte (short) NFC Forum Type 5 Tag Capability Container's
//      one-byte MLEN field instead of writing a wrong, wrapped value.
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdint.h>
#include <stdio.h>

static uint8_t mlen_old(uint16_t block_count, uint8_t block_size) {
    return (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
}

static int mlen_new(uint16_t block_count, uint8_t block_size, uint8_t* out_mlen) {
    uint32_t mlen_full = ((uint32_t)(block_count - 1) * block_size) >> 3;
    if(mlen_full > 0xFF) return 0; // capacity needs the extended CC format, not implemented here
    *out_mlen = (uint8_t)mlen_full;
    return 1;
}

typedef struct {
    const char* label;
    uint16_t block_count;
    uint8_t block_size;
} TestCase;

int main(void) {
    TestCase cases[] = {
        {"ICODE SLIX2-like tag (80 blocks x 4 bytes, 320 bytes total)", 80, 4},
        {"exact short-CC boundary (256 blocks x 8 bytes, 2040 bytes user capacity)", 256, 8},
        {"just over the boundary (256 blocks x 9 bytes, 2295 bytes user capacity)", 256, 9},
        {"large-capacity ISO15693 tag (256 blocks x 32 bytes, 8160 bytes user capacity)",
         256,
         32},
    };

    int any_mismatch = 0;

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint16_t bc = cases[i].block_count;
        uint8_t bs = cases[i].block_size;
        uint32_t true_mlen = ((uint32_t)(bc - 1) * bs) >> 3;
        uint8_t old_v = mlen_old(bc, bs);
        uint8_t new_v = 0;
        int new_ok = mlen_new(bc, bs, &new_v);

        printf("case: %s\n", cases[i].label);
        printf("  block_count=%u block_size=%u true_mlen=%u (0x%02X)\n",
               (unsigned)bc,
               (unsigned)bs,
               (unsigned)true_mlen,
               (unsigned)true_mlen);
        if(true_mlen == old_v) {
            printf("  OLD: writes CC.MLEN=0x%02X -- correct\n", (unsigned)old_v);
        } else {
            printf(
                "  OLD: writes CC.MLEN=0x%02X -- WRONG (wrapped, true value does not fit "
                "in one byte)\n",
                (unsigned)old_v);
            any_mismatch = 1;
        }
        if(new_ok) {
            printf("  NEW: writes CC.MLEN=0x%02X\n", (unsigned)new_v);
        } else {
            printf("  NEW: refuses NDEF write (\"Tag too large for NDEF\") instead of a wrong CC\n");
        }
        printf("\n");
    }

    if(!any_mismatch) {
        printf("ERROR: expected at least one wrap case, found none\n");
        return 1;
    }
    printf("OK: OLD silently wraps CC.MLEN for over-capacity tags, NEW detects and refuses.\n");
    return 0;
}
