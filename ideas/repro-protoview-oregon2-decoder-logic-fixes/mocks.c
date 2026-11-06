#include "mocks.h"

/* Verbatim copy of protoview/signal.c::bitmap_set(). */
void bitmap_set(uint8_t *b, uint32_t blen, uint32_t bitpos, bool val) {
    uint32_t byte = bitpos/8;
    uint32_t bit = 7-(bitpos&7);
    if (byte >= blen) return;
    if (val)
        b[byte] |= 1<<bit;
    else
        b[byte] &= ~(1<<bit);
}

/* Verbatim copy of protoview/signal.c::bitmap_get(). */
bool bitmap_get(uint8_t *b, uint32_t blen, uint32_t bitpos) {
    uint32_t byte = bitpos/8;
    uint32_t bit = 7-(bitpos&7);
    if (byte >= blen) return 0;
    return (b[byte] & (1<<bit)) != 0;
}

void set_nibble(uint8_t *buffer, int idx, uint8_t value) {
    int j = idx*4;
    for (int k = 0; k < 4; k++)
        bitmap_set(buffer,8,j+k,(value>>k)&1);
}
