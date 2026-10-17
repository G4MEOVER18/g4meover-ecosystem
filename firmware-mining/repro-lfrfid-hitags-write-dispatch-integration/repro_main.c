#include <stdio.h>
#include <stdint.h>
#include "lib/lfrfid/protocols/lfrfid_protocols.h"

int main(void) {
    LFRFIDWriteRequest req;
    req.write_type = LFRFIDWriteTypeHitagS;
    for(int i = 0; i < LFRFID_HITAGS_PAGE_SIZE; i++) {
        req.hitags.page4[i] = 0;
        req.hitags.page5[i] = 0;
    }
    printf("LFRFIDWriteTypeMax = %d\n", (int)LFRFIDWriteTypeMax);
    printf("sizeof(LFRFIDWriteRequest) = %zu\n", sizeof(LFRFIDWriteRequest));
    printf("OK: lfrfid_protocols.h + t5577.h + em4305.h + hitags.h compile together,\n");
    printf("    LFRFIDWriteTypeHitagS and LFRFIDHitagS resolve with no name collisions.\n");
    return 0;
}
