#ifndef B4B1_SEEK_SYNC_H
#define B4B1_SEEK_SYNC_H
#include <stdint.h>
#include <stdbool.h>

/* Returns true if a sync pattern was found. On success, *out_off is the
 * bitmap offset bitmap_seek_bits() returned and *out_j is the index into
 * sync_patterns[] that matched (both exactly as info->start_off / the local
 * 'j' would hold in the real decode()). */
bool b4b1_seek_sync_ALT(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                         uint32_t *out_off, int *out_j);
bool b4b1_seek_sync_NEU(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                         uint32_t *out_off, int *out_j);

#endif
