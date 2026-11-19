/* Literal copies from antirez/protoview signal.c
 * (commit fd2f6edd43cb813c37a7d912c7dd88f9732d298f), byte-identical to the
 * real implementation. */
#include "mocks.h"

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

void bitmap_set_pattern(uint8_t *b, uint32_t blen, uint32_t off, const char *pat) {
    uint32_t i = 0;
    while(pat[i]) {
        bitmap_set(b,blen,i+off,pat[i] == '1');
        i++;
    }
}

bool bitmap_match_bits(uint8_t *b, uint32_t blen, uint32_t bitpos, const char *bits) {
    for (size_t j = 0; bits[j]; j++) {
        bool expected = (bits[j] == '1') ? true : false;
        if (bitmap_get(b,blen,bitpos+j) != expected) return false;
    }
    return true;
}

uint32_t bitmap_seek_bits(uint8_t *b, uint32_t blen, uint32_t startpos, uint32_t maxbits, const char *bits) {
    uint32_t endpos = startpos+blen*8;
    uint32_t end2 = startpos+maxbits;
    if (end2 < endpos) endpos = end2;
    for (uint32_t j = startpos; j < endpos; j++)
        if (bitmap_match_bits(b,blen,j,bits)) return j;
    return BITMAP_SEEK_NOT_FOUND;
}
