/* Dedicated to the first of the two findings (uninitialized deviceid[]):
 * Valgrind Memcheck tracks, per byte, whether a value was ever written
 * before being read. oregon2_extract_ALT()'s very first touch of
 * deviceid[0] is "deviceid[0] |= nib[0] << 4;" -- a read-modify-write
 * that reads the never-initialized local before anything was stored
 * into it. Memcheck must report a "Use of uninitialised value" error
 * while running the ALT call and stay silent during the NEU call,
 * independent of what 'buffer' itself contains (the bug is about
 * deviceid[], not about the input data). Run under plain Valgrind
 * (no sanitizer instrumentation needed at compile time). */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "oregon2_extract.h"

int main(void) {
    uint8_t buffer[8];
    memset(buffer,0,sizeof(buffer));

    printf("Calling NEU (expect: no Memcheck report during this call)...\n");
    fflush(stdout);
    Oregon2Result neu;
    oregon2_extract_NEU(buffer,&neu);
    printf("NEU done.\n");
    fflush(stdout);

    printf("Calling ALT (expect: Memcheck reports use of uninitialised value "
           "for deviceid[] during this call)...\n");
    fflush(stdout);
    Oregon2Result alt;
    oregon2_extract_ALT(buffer,&alt);
    /* Force the tainted value to a sink Valgrind actually inspects
     * (a conditional branch on it), otherwise Memcheck's lazy tracking
     * never surfaces a report even though the read was uninitialized. */
    if (alt.deviceid[0] == 0 && alt.deviceid[1] == 0)
        printf("ALT done (deviceid happened to read back as 0x0000).\n");
    else
        printf("ALT done (deviceid=0x%02X%02X).\n",alt.deviceid[0],alt.deviceid[1]);
    return 0;
}
