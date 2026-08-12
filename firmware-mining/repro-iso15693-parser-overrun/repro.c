/* Host-compilable model of lib/signal_reader/parsers/iso15693/iso15693_parser.c
 * iso15693_parser_parse_1_out_of_256(), reduced to the append-loop that matters
 * here, plus a stand-in for lib/toolbox/bit_buffer.c's bit_buffer_append_byte()
 * (furi_check(new_size <= capacity) before the write, mirrored 1:1 below as
 * assert()+abort() since furi_check() itself aborts the process/reboots the
 * device on failure - it does not throw or return).
 *
 * FURI_HAL_NFC_ISO15693_MAX_FRAME_SIZE is 1024 (targets/f7/furi_hal/furi_hal_nfc_iso15693.c),
 * so parsed_frame's real capacity is 1024 bytes.
 *
 * Build/run (both pre-fix and post-fix):
 *   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -DFIX=0 repro.c -o repro_prefix && ./repro_prefix
 *   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -DFIX=1 repro.c -o repro_postfix && ./repro_postfix
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define CAPACITY 1024U

typedef struct {
    uint8_t* data;
    size_t capacity_bytes;
    size_t size_bytes;
} BitBuffer;

static BitBuffer* bit_buffer_alloc(size_t capacity_bytes) {
    BitBuffer* buf = malloc(sizeof(BitBuffer));
    buf->data = malloc(capacity_bytes); /* exact-size heap allocation, like the real bit_buffer_alloc() */
    buf->capacity_bytes = capacity_bytes;
    buf->size_bytes = 0;
    return buf;
}

/* 1:1 with bit_buffer_append_byte(): furi_check() aborts the process before any write
 * happens whenever the check fails - so on real hardware this is a hard crash/reboot,
 * never a silent heap overflow. We reproduce that same abort-before-write behavior here. */
static void bit_buffer_append_byte(BitBuffer* buf, uint8_t byte) {
    const size_t new_size = buf->size_bytes + 1;
    if(new_size > buf->capacity_bytes) {
        fprintf(
            stderr,
            "furi_check(new_data_size_bytes <= buf->capacity_bytes) FAILED "
            "(%zu > %zu) -> abort (device crash/reboot on hardware)\n",
            new_size,
            buf->capacity_bytes);
        abort();
    }
    buf->data[buf->size_bytes] = byte;
    buf->size_bytes = new_size;
}

/* Reduced iso15693_parser_parse_1_out_of_256() inner loop: for every non-zero
 * byte in the raw bitstream, every set bit is decoded as one appended byte.
 * With a clean signal exactly one bit is set per position; RF noise can set
 * several bits per byte, so a single noisy bitstream byte can append up to 8
 * bytes to parsed_frame in one pass - an 8x amplification versus input length. */
static void parse_1_out_of_256_noise_burst(BitBuffer* parsed_frame, const uint8_t* bitstream, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(bitstream[i] != 0x00) {
            for(size_t j = 0; j < 8; j++) {
                if((bitstream[i] >> j) & 1U) {
#if FIX
                    /* upstream fix (commit 2fe56050): reject overlong/noisy frames
                     * instead of overflowing parsed_frame */
                    if(parsed_frame->size_bytes >= parsed_frame->capacity_bytes) {
                        fprintf(stderr, "overlong frame detected -> parse fail (clean reject, no abort)\n");
                        return;
                    }
#endif
                    bit_buffer_append_byte(parsed_frame, (uint8_t)(j / 2));
                }
            }
        }
    }
}

int main(void) {
    BitBuffer* parsed_frame = bit_buffer_alloc(CAPACITY);

    /* Craft a noise burst well past 1024 decoded bytes: 200 bytes of 0xFF
     * (all 8 bits set) = up to 1600 appended bytes, no EOF/zero terminator in
     * between - models sustained RF noise near the Flipper while it is
     * listening for an ISO15693 field (furi_hal_nfc_iso15693.c bit-bang RX path). */
    uint8_t noise[200];
    memset(noise, 0xFF, sizeof(noise));

    printf(
        "capacity=%u bytes, feeding %zu noise bytes (up to %zu decoded appends), FIX=%d\n",
        CAPACITY,
        sizeof(noise),
        sizeof(noise) * 8,
        FIX);

    parse_1_out_of_256_noise_burst(parsed_frame, noise, sizeof(noise));

    printf(
        "done: parsed_frame size=%zu/%zu bytes, no crash\n",
        parsed_frame->size_bytes,
        parsed_frame->capacity_bytes);

    free(parsed_frame->data);
    free(parsed_frame);
    return 0;
}
