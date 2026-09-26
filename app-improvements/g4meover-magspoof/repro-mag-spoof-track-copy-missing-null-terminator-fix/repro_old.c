// Host repro (ALT / vulnerable): exact copy step from mag_spoof()
// (helpers/mag_helpers.c, zacharyweiss/magspoof_flipper) that turns each
// track's FuriString into a plain C string before it is handed to
// mag_encode(). Extracted verbatim (only the FuriString calls are replaced
// by an equivalent plain-C source string of the same length/content -- no
// behavioral change to the copy step itself):
//
//     char *data1, *data2, *data3;
//     data1 = malloc(furi_string_size(ft1) + 1);
//     ...
//     strncpy(data1, furi_string_get_cstr(ft1), furi_string_size(ft1));
//     ...
//
// mag_encode() (helpers/mag_helpers.c) then scans this buffer with a plain
// "read until the first zero byte" loop:
//
//     for(int i = 0; *(data + i) != 0; i++) { ... }
//
// strncpy(dst, src, n) only null-terminates dst when strlen(src) < n. Here
// n == furi_string_size(ft), i.e. n == strlen(src) exactly -- so strlen(src)
// is NEVER less than n, and the extra byte malloc'd for the terminator
// (data[size]) is left exactly as malloc() returned it: uninitialized.
//
// On real hardware (and on glibc), malloc() does not zero freed-then-reused
// memory -- a chunk that already held non-zero bytes from an earlier,
// same-size allocation keeps them until something overwrites them. The
// MagSpoof app allocates/frees these exact per-track buffers on every single
// "Emulate" -> "Send" press, so by the second send in a session the
// allocator is reliably handing back a chunk it just freed one send ago.
// This repro reproduces that allocator behavior deterministically (glibc
// tcache: free() immediately followed by an equal-size malloc() returns the
// same chunk) instead of relying on chance -- tested below as its own case.
// Note: AddressSanitizer additionally fills every malloc() with a non-zero
// pattern (0xbe) itself, specifically to surface exactly this bug class
// (trusting an uninitialized byte to be zero); with ASan enabled even the
// very first allocation of a given size is affected, not only a reused one.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// glibc declares strsignal() only under certain feature-test macros; the
// prototype isn't needed for correctness here, just to silence the
// -Wimplicit-function-declaration warning under -std=c11.
char* strsignal(int sig);

static char* alt_copy_track(const char* src, size_t size) {
    char* dst = malloc(size + 1);
    strncpy(dst, src, size); // <-- the bug: must be size + 1
    return dst;
}

// mag_encode()'s own termination condition (helpers/mag_helpers.c), isolated
// from the bit-packing it does per character -- it is this scan, not the
// bit-packing, that walks off the end of a non-terminated buffer.
static size_t mag_encode_scan_len(const char* data) {
    size_t i = 0;
    while(*(data + i) != 0) i++;
    return i;
}

static void run_case(const char* label, const char* track, int poison_heap_first) {
    printf("-- %s --\n", label);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        size_t size = strlen(track);

        if(poison_heap_first) {
            // Simulate the allocator state after a prior send in the same
            // session: an equal-size buffer that already held non-zero
            // track data was just malloc'd+freed for the previous track.
            char* prior = malloc(size + 1);
            memset(prior, 'X', size + 1); // non-zero leftover, like real card data
            free(prior);
        }

        char* data1 = alt_copy_track(track, size);
        printf(
            "   data1[size] (should read as the null terminator, i.e. 0) = %d\n",
            (unsigned char)data1[size]);

        size_t scanned = mag_encode_scan_len(data1); // this is what mag_encode() does
        printf("   mag_encode() would scan %zu bytes for a %zu-byte track\n", scanned, size);
        free(data1);
        _exit(0);
    }
    int status;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) {
        printf(
            "   CHILD KILLED by signal %d (%s) -- read past the allocation\n",
            WTERMSIG(status),
            strsignal(WTERMSIG(status)));
    } else {
        printf("   child exit code %d\n", WEXITSTATUS(status));
    }
}

int main(void) {
    const char* track2 = ";4111111111111111=25121010000000000000?";

    // Very first allocation of this size in the process.
    run_case("first-ever allocation of this size", track2, 0);

    // Same size allocated and freed once already (exactly what happens
    // after the first "Emulate" -> "Send" in a session) -- the allocator
    // reuses the chunk, the leftover non-zero byte from the previous
    // track surfaces, and mag_encode()'s scan walks off the end of the
    // allocation.
    run_case("same buffer size reused after a prior send (realistic repeat use)", track2, 1);

    return 0;
}
