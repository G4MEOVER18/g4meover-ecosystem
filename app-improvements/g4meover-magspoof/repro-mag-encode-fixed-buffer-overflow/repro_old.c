// Host repro (ALT / vulnerable): exact bit-packing logic of mag_encode() from
// helpers/mag_helpers.c (zacharyweiss/magspoof_flipper), extracted verbatim
// (only Furi-specific types dropped -- no behavioral change), called against
// oversized track data the way mag_spoof() does with its fixed 64/128-byte
// on-stack buffers.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// ---- verbatim from mag_helpers.c ---------------------------------------

uint16_t add_bit(int value, uint8_t* out, uint16_t count) {
    uint8_t bit = count % 8;
    uint8_t byte = count / 8;
    if(value) {
        out[byte] |= 0x01;
    }
    if(bit < 7) out[byte] <<= 1;
    return count + 1;
}

uint16_t add_bit_manchester(int value, uint8_t* out, uint16_t count) {
    static int toggle = 0;
    toggle ^= 0x01;
    count = add_bit(toggle, out, count);
    if(value) toggle ^= 0x01;
    count = add_bit(toggle, out, count);
    return count;
}

uint16_t mag_encode(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset) {
    uint16_t raw_bits_count = 0;
    uint16_t output_count = 0;
    int tmp, crc, lrc = 0;

    for(int i = 0; *(data + i) != 0; i++) {
        crc = 1;
        tmp = *(data + i) - track_ascii_offset;

        for(int j = 0; j < track_bits - 1; j++) {
            crc ^= tmp & 1;
            lrc ^= (tmp & 1) << j;
            raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
            output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
            tmp >>= 1;
        }
        raw_bits_count = add_bit(crc, out_raw, raw_bits_count);
        output_count = add_bit_manchester(crc, out_manchester, output_count);
    }

    tmp = lrc;
    crc = 1;
    for(int j = 0; j < track_bits - 1; j++) {
        crc ^= tmp & 0x01;
        raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
        output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
        tmp >>= 1;
    }
    raw_bits_count = add_bit(crc, out_raw, raw_bits_count);
    output_count = add_bit_manchester(crc, out_manchester, output_count);

    return output_count;
}

// ---- test harness --------------------------------------------------------

// mag_spoof()'s exact stack buffers for track 1 (bitlen[0]=7, sublen[0]=32)
static void run_case(const char* label, int track1_len) {
    printf("-- %s: track1_len=%d --\n", label, track1_len);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        char* data1 = malloc(track1_len + 1);
        memset(data1, 'A', track1_len); // 'A' = 0x41, valid track-1 alnum char
        data1[track1_len] = '\0';

        uint8_t bits_t1_raw[64] = {0};
        uint8_t bits_t1_manchester[128] = {0};
        uint16_t n = mag_encode(data1, bits_t1_manchester, bits_t1_raw, 7, 32);
        printf("   encoded %u manchester bits, no crash\n", n);
        free(data1);
        _exit(0);
    }
    int status;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) {
        printf("   CHILD KILLED by signal %d (%s)\n", WTERMSIG(status), strsignal(WTERMSIG(status)));
    } else {
        printf("   child exit code %d\n", WEXITSTATUS(status));
    }
}

int main(void) {
    // Safe case: within the ~72-char track-1 capacity of a 64-byte/7-bit buffer.
    run_case("within assumed 68-char track-1 limit", 60);
    // Overflow case: 100-char track 1 -- well within what UART capture (up to
    // 4096 bytes), a loaded .mag file (unbounded string field), or even the
    // manual "Enter track data" keyboard input (up to 149 chars) can produce.
    run_case("oversized track-1 data (100 chars)", 100);
    return 0;
}
