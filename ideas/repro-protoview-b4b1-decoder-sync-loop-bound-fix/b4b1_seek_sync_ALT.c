/* Literal extraction of the sync-search block from protocols/b4b1.c::decode()
 * (antirez/protoview, commit fd2f6edd43cb813c37a7d912c7dd88f9732d298f),
 * ALT = the current, unmodified upstream code: the sync_patterns[] array was
 * expanded from 3 to 6 entries by commit cc32c5e ("b4b1: more robust message
 * detection"), but the loop bound was left at 3. Byte-identical to the real
 * decode() up to and including the BITMAP_SEEK_NOT_FOUND check; everything
 * after (convert_from_line_code() / field extraction) is irrelevant to this
 * finding and left out. */
#include "mocks.h"
#include "b4b1_seek_sync.h"

bool b4b1_seek_sync_ALT(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                         uint32_t *out_off, int *out_j) {
    if (numbits < 30) return false;

    const char *sync_patterns[6] = {
        "100000000000000000000000000000011101", /* 30 times gap + one. */
        "100000000000000000000000000000010001", /* 30 times gap + zero. */
        "1000000000000000000000000000000011101", /* 31 times gap + one. */
        "1000000000000000000000000000000010001", /* 31 times gap + zero. */
        "10000000000000000000000000000000011101", /* 32 times gap + one. */
        "10000000000000000000000000000000010001", /* 32 times gap + zero. */
    };

    uint32_t off;
    int j;
    for (j = 0; j < 3; j++) {
        off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_patterns[j]);
        if (off != BITMAP_SEEK_NOT_FOUND) break;
    }
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    *out_off = off;
    *out_j = j;
    return true;
}
