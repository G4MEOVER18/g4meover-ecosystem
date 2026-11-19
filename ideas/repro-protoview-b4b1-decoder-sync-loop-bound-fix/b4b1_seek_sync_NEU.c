/* Same extraction as b4b1_seek_sync_ALT.c, with the proposed fix applied:
 * loop bound raised from 3 to 6 so that all six entries of sync_patterns[]
 * (not just the first half) are actually tried, matching the array the
 * decoder itself declares. Otherwise byte-identical. */
#include "mocks.h"
#include "b4b1_seek_sync.h"

bool b4b1_seek_sync_NEU(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
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
    for (j = 0; j < 6; j++) {
        off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_patterns[j]);
        if (off != BITMAP_SEEK_NOT_FOUND) break;
    }
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    *out_off = off;
    *out_j = j;
    return true;
}
