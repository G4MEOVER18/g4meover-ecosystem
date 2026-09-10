// Host repro for the UHF RFID (YRM100/(Q)M100) app's ring-buffer OOB read.
//
// Mirrors the exact access pattern of setup_and_send_rx() in uhf_module.c:
//   uhf_buffer_reset(buffer);
//   <bytes trickle in one at a time via the UART RX callback, exactly like
//    uhf_uart_default_rx_callback() calling uhf_buffer_append_single()>
//   uint8_t* data = uhf_buffer_get_data(buffer);
//   size_t length = uhf_buffer_get_size(buffer);
//   data[0] ...            (FRAME_START check)
//   data[length - 1] ...   (FRAME_END check)
//   data[length - 2] ...   (checksum byte)
//   checksum(data + 1, length - 3)  (reads data[1 .. length-3])
//
// Capacity matches the real UHF_UART_RX_BUFFER_SIZE (250, uhf_uart.h).
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "uhf_buffer.h"

#define UHF_UART_RX_BUFFER_SIZE 250
#define FRAME_START 0xBB
#define FRAME_END   0x7E

static uint8_t checksum(const uint8_t* data, size_t length) {
    uint64_t sum_val = 0x00;
    for(size_t i = 0; i < length; i++) sum_val += data[i];
    return (uint8_t)(sum_val % 0x100);
}

// Same "wait for response, one byte at a time, FRAME_END terminates" shape
// as uhf_uart_default_rx_callback() + setup_and_send_rx()'s polling loop.
static void feed_response(Buffer* buf, const uint8_t* resp, size_t resp_len) {
    for(size_t i = 0; i < resp_len; i++) {
        if(uhf_is_buffer_closed(buf)) break;
        uint8_t b = resp[i];
        if(b == FRAME_END) {
            uhf_buffer_append_single(buf, b);
            uhf_buffer_close(buf);
        } else {
            uhf_buffer_append_single(buf, b);
        }
    }
}

static int run_case(const char* label, size_t resp_len) {
    Buffer* buf = uhf_buffer_alloc(UHF_UART_RX_BUFFER_SIZE);
    uhf_buffer_reset(buf);

    // Build a syntactically valid oversized response: FRAME_START, payload
    // bytes that deliberately avoid the FRAME_END byte value (so the frame
    // is only recognized as complete at the very last byte, exactly like a
    // real large TID/USER-bank read or multi-tag inventory reply whose
    // payload just happens not to contain 0x7E before the real terminator),
    // then FRAME_END as the last byte.
    uint8_t* resp = malloc(resp_len);
    resp[0] = FRAME_START;
    for(size_t i = 1; i + 1 < resp_len; i++) {
        uint8_t v = (uint8_t)(i * 37 + 11);
        if(v == FRAME_END) v++;
        resp[i] = v;
    }
    resp[resp_len - 1] = FRAME_END;

    feed_response(buf, resp, resp_len);

    uint8_t* data = uhf_buffer_get_data(buf);
    size_t length = uhf_buffer_get_size(buf);
    printf(
        "[%s] resp_len=%zu capacity=%zu size=%zu head=%zu tail=%zu closed=%d\n",
        label, resp_len, buf->capacity, buf->size, buf->head, buf->tail, buf->closed);

    int ok = 1;
    if(length > 0) {
        // Exactly setup_and_send_rx()'s validation sequence.
        uint8_t first = data[0];
        uint8_t last = data[length - 1];
        uint8_t crc_byte = length >= 2 ? data[length - 2] : 0;
        uint8_t crc_calc = length >= 3 ? checksum(data + 1, length - 3) : 0;
        printf(
            "        data[0]=0x%02X data[len-1]=0x%02X data[len-2]=0x%02X checksum=0x%02X\n",
            first, last, crc_byte, crc_calc);
        ok = (first == FRAME_START) && (last == FRAME_END);
    }

    free(resp);
    uhf_buffer_free(buf);
    return ok;
}

int main(int argc, char** argv) {
    // Run one case per process invocation so an ASan abort on the oversized
    // case doesn't hide the (deliberately unaffected) normal-size result.
    //   argv[1] == "small" -> response fits inside the 250-byte capacity,
    //                          regression check that the fix changes nothing.
    //   argv[1] == "large" -> response exceeds capacity by 50 bytes (e.g.
    //                          TID+USER bank read, or a multi-tag inventory
    //                          reply) -> triggers the ring-buffer wraparound.
    //                          Under ALT this reads out of bounds of the
    //                          250-byte heap allocation; under NEU the
    //                          buffer stops accepting bytes once full and
    //                          the truncated frame is safely rejected by
    //                          the FRAME_END check.
    const char* which = argc > 1 ? argv[1] : "small";
    if(strcmp(which, "large") == 0) {
        int ok = run_case("oversized 300B response", 300);
        printf("oversized-response frame recognized as valid: %s (expected: no, once truncated safely)\n",
               ok ? "yes" : "no");
    } else if(strcmp(which, "exact") == 0) {
        // Response exactly fills the 250-byte capacity -> no wraparound
        // needed, must keep working identically under ALT and NEU.
        int ok = run_case("exact 250B response", 250);
        printf("exact-capacity response frame recognized: %s (expected: yes)\n", ok ? "yes" : "no");
    } else {
        int ok = run_case("normal 40B response", 40);
        printf("small-response frame recognized: %s (expected: yes)\n", ok ? "yes" : "no");
    }
    return 0;
}
