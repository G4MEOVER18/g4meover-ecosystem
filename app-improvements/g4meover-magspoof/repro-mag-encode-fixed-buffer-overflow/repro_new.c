// Host repro (NEU / fixed): same harness as repro_old.c, but mag_encode()
// now takes the caller's actual out_raw capacity (in bytes) and stops
// encoding further characters -- always leaving room for the trailing LRC
// byte -- once that capacity would be exceeded, instead of writing past it.
// out_manchester is exactly 2x out_raw in both real call sites
// (mag_spoof(), debug_mag_string()), so bounding on the raw side already
// bounds the manchester side identically -- verified in the port proposal.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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

// NEW parameter: out_raw_capacity_bytes -- the real, fixed size of the
// caller's out_raw buffer (out_manchester is assumed/verified to be 2x that).
uint16_t mag_encode(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes) {
    uint16_t raw_bits_count = 0;
    uint16_t output_count = 0;
    int tmp, crc, lrc = 0;

    // Reserve room for the trailing LRC byte (track_bits bits) up front, so
    // the per-character loop never eats into space the LRC write needs.
    const uint16_t raw_bit_capacity = (uint16_t)(out_raw_capacity_bytes * 8);
    const uint16_t max_data_bits =
        (raw_bit_capacity > track_bits) ? (uint16_t)(raw_bit_capacity - track_bits) : 0;

    for(int i = 0; *(data + i) != 0; i++) {
        if((uint16_t)(raw_bits_count + track_bits) > max_data_bits) {
            // Would overrun out_raw/out_manchester (or leave no room for the
            // LRC byte) -- stop encoding further characters.
            break;
        }

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

static void run_case(const char* label, int track1_len) {
    printf("-- %s: track1_len=%d --\n", label, track1_len);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        char* data1 = malloc(track1_len + 1);
        memset(data1, 'A', track1_len);
        data1[track1_len] = '\0';

        uint8_t bits_t1_raw[64] = {0};
        uint8_t bits_t1_manchester[128] = {0};
        uint16_t n = mag_encode(data1, bits_t1_manchester, bits_t1_raw, 7, 32, sizeof(bits_t1_raw));
        printf("   encoded %u manchester bits, no crash (truncated if input was oversized)\n", n);
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
    run_case("within assumed 68-char track-1 limit", 60);
    run_case("oversized track-1 data (100 chars)", 100);
    return 0;
}
