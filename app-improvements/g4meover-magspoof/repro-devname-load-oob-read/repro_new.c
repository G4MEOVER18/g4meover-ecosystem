// repro_new.c -- reproduces the proposed fix: mag_device_load_data() uses
// strlcpy(dev->dev_name, filename_stem, sizeof(dev->dev_name)) instead of
// strncpy(dev->dev_name, filename_stem, MAG_DEV_NAME_MAX_LEN). Host-only,
// no Furi/Flipper types needed.
//
// Build: gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_new repro_new.c
// Run:   ./repro_new
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAG_DEV_NAME_MAX_LEN 22

// Reproduced unchanged from the arm-none-eabi-gcc newlib C library the
// firmware links against (newlib/libc/string/strlcpy.c, OpenBSD-derived --
// the same function mag_device_set_name() already calls in mag_device.c).
// glibc has no built-in strlcpy, so the host repro supplies the identical
// reference implementation instead of a stand-in.
static size_t host_strlcpy(char* dst, const char* src, size_t siz) {
    char* d = dst;
    const char* s = src;
    size_t n = siz;

    if(n != 0) {
        while(--n != 0) {
            if((*d++ = *s++) == '\0') break;
        }
    }

    if(n == 0) {
        if(siz != 0) *d = '\0';
        while(*s++)
            ;
    }

    return (size_t)(s - src - 1);
}

typedef struct {
    char dev_name[MAG_DEV_NAME_MAX_LEN + 1];
} MagDeviceNew;

// Fixed mag_device_load_data(): siz is the full buffer size
// (sizeof(dev->dev_name) == MAG_DEV_NAME_MAX_LEN + 1), so strlcpy() always
// leaves a NUL inside the buffer no matter how long filename_stem is --
// termination no longer depends on a caller doing a follow-up copy.
static void mag_device_load_data_new(MagDeviceNew* dev, const char* filename_stem) {
    host_strlcpy(dev->dev_name, filename_stem, sizeof(dev->dev_name));
}

static void check_len(const char* label, MagDeviceNew* dev, size_t expect_len) {
    size_t len = strlen(dev->dev_name); // always in-bounds now
    printf("%s: dev_name=\"%s\" len=%zu\n", label, dev->dev_name, len);
    if(len != expect_len) {
        fprintf(stderr, "%s: FAILED, expected len=%zu\n", label, expect_len);
        exit(1);
    }
}

int main(void) {
    // Same 22-character stem that overflowed the ALT version.
    char filename_stem_22[MAG_DEV_NAME_MAX_LEN + 1];
    memset(filename_stem_22, 'F', MAG_DEV_NAME_MAX_LEN);
    filename_stem_22[MAG_DEV_NAME_MAX_LEN] = '\0';

    MagDeviceNew* dev = malloc(sizeof(MagDeviceNew));
    if(!dev) return 1;
    memset(dev, 'A', sizeof(*dev)); // same uninitialized-heap poisoning as repro_old

    mag_device_load_data_new(dev, filename_stem_22);
    // Full 22-char stem fits in a 23-byte buffer (22 chars + terminator) --
    // no truncation, no overflow, always NUL-terminated.
    check_len("22-char stem", dev, 22);
    free(dev);

    // Regression: a normal, much longer-than-realistic-but-still-over-limit
    // stem still truncates safely and terminates.
    dev = malloc(sizeof(MagDeviceNew));
    memset(dev, 'A', sizeof(*dev));
    mag_device_load_data_new(dev, "ThisFilenameStemIsDefinitelyLongerThan22Characters");
    check_len("50-char stem (truncated)", dev, MAG_DEV_NAME_MAX_LEN);
    free(dev);

    // Regression: a normal short name (well under the limit) behaves
    // identically to before -- full name preserved, correctly terminated.
    dev = malloc(sizeof(MagDeviceNew));
    memset(dev, 'A', sizeof(*dev));
    mag_device_load_data_new(dev, "TestCard1");
    check_len("short stem (regression)", dev, 9);
    free(dev);

    printf("ALL CHECKS PASSED\n");
    return 0;
}
