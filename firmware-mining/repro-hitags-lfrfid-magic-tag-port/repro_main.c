/* Driver for the real, unmodified lib/lfrfid/tools/hitags.c (Unleashed dev branch,
 * commit af2e764) compiled against this repo's actual mntm-012 furi_hal_rfid.h
 * signatures (stubbed, since hitags_selftest() never calls them) and the real
 * lib/bit_lib/bit_lib.c (copied verbatim from G4MEOVER-FW tag b359f6f). Proves the
 * module's own CRC-8/frame-builder/anticollision-decoder self-test, which the
 * Unleashed unit test suite runs on target, passes unmodified on the host too -
 * i.e. the pure-logic two thirds of the module needs no change to run on mntm-012. */
#include <stdio.h>
#include "hitags.h"

const char* hitags_selftest(void);

int main(void) {
    const char* failure = hitags_selftest();
    if(failure) {
        printf("FAIL: %s\n", failure);
        return 1;
    }
    printf("PASS: hitags_selftest() - all datasheet/Proxmark3 vectors and decoder guards"
           " hold unmodified against mntm-012's furi_hal_rfid.h + bit_lib.c\n");
    return 0;
}
