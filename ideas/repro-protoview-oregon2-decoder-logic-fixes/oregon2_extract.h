#ifndef OREGON2_EXTRACT_H
#define OREGON2_EXTRACT_H
#include <stdint.h>

typedef struct {
    uint8_t deviceid[2];
    float tempval;
    int humval;
} Oregon2Result;

/* ALT = protoview/protocols/oregon2.c::decode(), nibble-processing part,
 * taken over word for word (commit fd2f6edd43cb813c37a7d912c7dd88f9732d298f).
 * NEU = same block with the two fixes from oregon2_c.patch applied. */
void oregon2_extract_ALT(uint8_t *buffer, Oregon2Result *out);
void oregon2_extract_NEU(uint8_t *buffer, Oregon2Result *out);

#endif
