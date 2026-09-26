// Host repro (NEU / fixed): same copy step as repro_old.c, with the one-word
// fix applied (n = size + 1, so strncpy's "only null-terminates when
// strlen(src) < n" rule is satisfied and the allocated terminator byte is
// always written).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char* strsignal(int sig);

static char* neu_copy_track(const char* src, size_t size) {
    char* dst = malloc(size + 1);
    strncpy(dst, src, size + 1); // fix: copy the terminator too
    return dst;
}

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
            char* prior = malloc(size + 1);
            memset(prior, 'X', size + 1);
            free(prior);
        }

        char* data1 = neu_copy_track(track, size);
        printf("   data1[size] (must be 0) = %d\n", (unsigned char)data1[size]);

        size_t scanned = mag_encode_scan_len(data1);
        printf("   mag_encode() scans %zu bytes for a %zu-byte track (must match)\n", scanned, size);
        if(data1[size] != 0 || scanned != size) {
            fprintf(stderr, "   CHECK FAILED\n");
            free(data1);
            _exit(1);
        }
        free(data1);
        _exit(0);
    }
    int status;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) {
        printf(
            "   CHILD KILLED by signal %d (%s)\n", WTERMSIG(status), strsignal(WTERMSIG(status)));
    } else {
        printf("   child exit code %d\n", WEXITSTATUS(status));
    }
}

int main(void) {
    const char* track2 = ";4111111111111111=25121010000000000000?";

    run_case("first-ever allocation of this size", track2, 0);
    run_case("same buffer size reused after a prior send", track2, 1);

    printf("ALL CHECKS PASSED\n");
    return 0;
}
