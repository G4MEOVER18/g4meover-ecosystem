#include "mocks.h"
#include "oregon2_extract.h"

#define DEBUG_MSG 0
#define TAG "x"
#define FURI_LOG_E(...) do {} while (0)

/* Nibble-processing block of protoview/protocols/oregon2.c::decode(),
 * taken over word for word from commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f -- this is the ALT (buggy,
 * currently upstream) version. 'buffer' replaces the function's local
 * 'buffer[8]' that convert_from_line_code() would have filled in the
 * real decode(); here the caller fills it directly via set_nibble(). */
void oregon2_extract_ALT(uint8_t *buffer, Oregon2Result *out) {
    uint8_t raw[8] = {0};
    char temp[3] = {0}, hum[2] = {0};
    uint8_t deviceid[2];
    for (int j = 0; j < 64; j += 4) {
        uint8_t nib[1];
        nib[0] = (bitmap_get(buffer,8,j+0) |
                  bitmap_get(buffer,8,j+1) << 1 |
                  bitmap_get(buffer,8,j+2) << 2 |
                  bitmap_get(buffer,8,j+3) << 3);
        if (DEBUG_MSG) FURI_LOG_E(TAG, "Not inverted nibble[%d]: %x", j/4, (unsigned int)nib[0]);
        raw[j/8] |= nib[0] << (4-(j%4));
        switch(j/4) {
        case 1: deviceid[0] |= nib[0]; break;
        case 0: deviceid[0] |= nib[0] << 4; break;
        case 3: deviceid[1] |= nib[0]; break;
        case 2: deviceid[1] |= nib[0] << 4; break;
        case 10: temp[0] = nib[0]; break;
        /* Fixme: take the temperature sign from nibble 11. */
        case 9: temp[1] = nib[0]; break;
        case 8: temp[2] = nib[0]; break;
        case 13: hum[0] = nib[0]; break;
        case 12: hum[1] = nib[0]; break;
        }
    }

    float tempval = ((temp[0]-'0')*10) +
                     (temp[1]-'0') +
                    ((float)(temp[2]-'0')*0.1);
    int humval = (hum[0]-'0')*10 + (hum[1]-'0');

    out->deviceid[0] = deviceid[0];
    out->deviceid[1] = deviceid[1];
    out->tempval = tempval;
    out->humval = humval;
    (void)raw;
}
