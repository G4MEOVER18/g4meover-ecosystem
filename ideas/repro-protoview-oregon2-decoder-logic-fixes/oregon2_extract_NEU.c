#include "mocks.h"
#include "oregon2_extract.h"

#define DEBUG_MSG 0
#define TAG "x"
#define FURI_LOG_E(...) do {} while (0)

/* Same block as oregon2_extract_ALT.c, with the two fixes of
 * ideas/protoview-oregon2-decoder-logic-fixes/oregon2_c.patch applied:
 * deviceid[] is zero-initialized, and temp[]/hum[] are used as the raw
 * digit values they actually hold instead of being treated a second
 * time as ASCII digit characters. */
void oregon2_extract_NEU(uint8_t *buffer, Oregon2Result *out) {
    uint8_t raw[8] = {0};
    char temp[3] = {0}, hum[2] = {0};
    uint8_t deviceid[2] = {0};
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

    float tempval = (temp[0]*10) +
                     temp[1] +
                    ((float)temp[2]*0.1);
    int humval = hum[0]*10 + hum[1];

    out->deviceid[0] = deviceid[0];
    out->deviceid[1] = deviceid[1];
    out->tempval = tempval;
    out->humval = humval;
    (void)raw;
}
