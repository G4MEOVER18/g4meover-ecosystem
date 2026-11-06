#ifndef MOCKS_H
#define MOCKS_H
#include <stdint.h>
#include <stdbool.h>

/* bitmap_get()/bitmap_set() copied verbatim from protoview's signal.c
 * (functions used unmodified by the Oregon2 decoder under test). */
void bitmap_set(uint8_t *b, uint32_t blen, uint32_t bitpos, bool val);
bool bitmap_get(uint8_t *b, uint32_t blen, uint32_t bitpos);

/* Sets the 4 bits of nibble 'idx' (0..15) inside an 8-byte buffer so that
 * decoding it with the real nib[0] extraction formula yields 'value'
 * (0..15). Mirrors the bit order protoview's own nib[0] computation
 * expects (bit idx*4+0 is the LSB of the nibble, idx*4+3 is the MSB). */
void set_nibble(uint8_t *buffer, int idx, uint8_t value);

#endif
