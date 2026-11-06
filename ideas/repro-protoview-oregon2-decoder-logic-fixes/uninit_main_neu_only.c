/* Standalone NEU-only counterpart to uninit_main.c, run on its own
 * under Valgrind Memcheck to confirm the fixed version never reads
 * deviceid[] before writing it, independent of the ALT run. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "oregon2_extract.h"

int main(void) {
    uint8_t buffer[8];
    memset(buffer,0,sizeof(buffer));

    Oregon2Result neu;
    oregon2_extract_NEU(buffer,&neu);
    if (neu.deviceid[0] == 0 && neu.deviceid[1] == 0)
        printf("NEU done (deviceid=0x0000, as expected for an all-zero buffer).\n");
    else
        printf("NEU done (deviceid=0x%02X%02X).\n",neu.deviceid[0],neu.deviceid[1]);
    return 0;
}
