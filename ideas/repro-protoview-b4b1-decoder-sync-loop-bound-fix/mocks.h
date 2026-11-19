/* Literal copies of the bitmap primitives used by protocols/b4b1.c,
 * taken verbatim from antirez/protoview signal.c / app.h
 * (commit fd2f6edd43cb813c37a7d912c7dd88f9732d298f). No reimplementation:
 * this is the real code the decoder relies on, so the repro exercises the
 * actual sync-search behaviour, not a model of it. */
#ifndef MOCKS_H
#define MOCKS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BITMAP_SEEK_NOT_FOUND UINT32_MAX

void bitmap_set(uint8_t *b, uint32_t blen, uint32_t bitpos, bool val);
bool bitmap_get(uint8_t *b, uint32_t blen, uint32_t bitpos);
void bitmap_set_pattern(uint8_t *b, uint32_t blen, uint32_t off, const char *pat);
bool bitmap_match_bits(uint8_t *b, uint32_t blen, uint32_t bitpos, const char *bits);
uint32_t bitmap_seek_bits(uint8_t *b, uint32_t blen, uint32_t startpos, uint32_t maxbits, const char *bits);

#endif
