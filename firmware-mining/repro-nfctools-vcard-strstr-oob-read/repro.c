/*
 * Host repro for the nfctools vCard-summary out-of-bounds read.
 *
 * Reduces the tnf==0x02 && type=="text/vcard" branch of ndef_fill_record()
 * (applications/external/nfctools/core/nfc_tools_ndef.c) to its control
 * flow. That branch locates the "FN:" field with strstr((const char*)payload,
 * "FN:") to build the record summary. `payload` is a raw slice into a heap
 * buffer that nfc_tools_scene_read_tag.c/nfc_tools_mfc.c/nfc_tools_desfire.c
 * allocate with malloc(data_len)/malloc(total) sized to the exact tag data
 * length -- it is never NUL-terminated and carries no trailing byte beyond
 * `payload_len`. strstr() has no length bound: on a tag whose vCard-typed
 * NDEF payload contains no "FN:" substring inside its own bytes, it keeps
 * reading past the end of the allocation looking for one (or for the NUL
 * strstr relies on). Even where it *does* find a stray "FN:" byte pattern
 * beyond `payload_len`, the follow-up bounds arithmetic
 *     size_t max = payload_len - (fn - payload) - 3;
 * underflows (fn - payload >= payload_len), turning `max` into a huge
 * size_t and letting the copy loop read arbitrarily far past the buffer.
 *
 * The sibling function in the same file, ndef_decode_vcard(), already scans
 * for the same fields with a `len`-bounded loop instead of strstr() -- this
 * branch is the one place that regressed to the unbounded call.
 *
 * ALT reproduces the vulnerable code verbatim (strstr + the same pointer
 * arithmetic). NEU replaces the search with a length-bounded scan, mirroring
 * ndef_decode_vcard()'s existing pattern in the same file.
 *
 * Build:  gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 * Run:    ./repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VALUE_SZ 64

/* ALT: verbatim logic from ndef_fill_record()'s text/vcard branch. */
static bool vcard_summary_old(const uint8_t* payload, size_t payload_len, char* value) {
    value[0] = '\0';
    const char* fn = strstr((const char*)payload, "FN:");
    if(fn) {
        const char* end = fn + 3;
        size_t max = payload_len - (size_t)(fn - (const char*)payload) - 3;
        size_t w = 0;
        while(w < max && w + 1 < VALUE_SZ && end[w] != '\r' && end[w] != '\n') {
            value[w] = end[w];
            w++;
        }
        value[w] = '\0';
        return true;
    }
    return false;
}

/* NEU: fix -- bound the "FN:" search to payload_len, like ndef_decode_vcard()
 * already does in the same file for its own field scan. */
static bool vcard_summary_new(const uint8_t* payload, size_t payload_len, char* value) {
    value[0] = '\0';
    const char* src = (const char*)payload;
    const char* fn = NULL;
    for(size_t i = 0; i + 3 <= payload_len; i++) {
        if(memcmp(src + i, "FN:", 3) == 0) {
            fn = src + i;
            break;
        }
    }
    if(fn) {
        const char* end = fn + 3;
        size_t max = payload_len - (size_t)(fn - src) - 3;
        size_t w = 0;
        while(w < max && w + 1 < VALUE_SZ && end[w] != '\r' && end[w] != '\n') {
            value[w] = end[w];
            w++;
        }
        value[w] = '\0';
        return true;
    }
    return false;
}

int main(void) {
    char value[VALUE_SZ];

    /* Case 1 (regression check, run first since case 2 aborts the process):
     * a real "FN:" field fully inside payload_len bytes -- ALT and NEU must
     * produce the same, correct result. */
    {
        const char* body = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Jane Doe\r\nEND:VCARD\r\n";
        size_t payload_len = strlen(body);
        uint8_t* payload = malloc(payload_len);
        memcpy(payload, body, payload_len);

        bool found_new = vcard_summary_new(payload, payload_len, value);
        printf("case 1 (well-formed FN:) -- NEU: found=%d value=\"%s\"\n", found_new, value);
        char value_old_buf[VALUE_SZ];
        char* value_old = value_old_buf;
        bool found_old = vcard_summary_old(payload, payload_len, value_old);
        printf("case 1 (well-formed FN:) -- ALT: found=%d value=\"%s\"\n", found_old, value_old);

        if(found_new != found_old || strcmp(value, value_old) != 0) {
            fprintf(stderr, "REGRESSION: fix changes behavior on a well-formed record\n");
            free(payload);
            return 1;
        }
        free(payload);
    }

    /* Case 2: malicious vCard payload -- no "FN:" and no NUL anywhere inside
     * the allocation (a crafted tag can freely choose these bytes). Mirrors
     * the real allocation exactly: malloc(payload_len), no extra byte. */
    {
        const char* body = "BEGIN:VCARD\r\nVERSION:3.0\r\nEND:VCARD\r\n";
        size_t payload_len = strlen(body);
        uint8_t* payload = malloc(payload_len);
        memcpy(payload, body, payload_len);

        printf("case 2 (no FN:, unterminated payload) -- NEU: ");
        fflush(stdout);
        bool found_new = vcard_summary_new(payload, payload_len, value);
        printf("found=%d value=\"%s\" (expected: not found, no OOB access)\n", found_new, value);

        printf("case 2 (no FN:, unterminated payload) -- ALT: ");
        fflush(stdout);
        /* On real hardware this either hard-faults or, if adjacent heap bytes
         * happen to contain "FN:"/NUL, leaks/misreads them into `value`.
         * Under ASan the allocation's redzone makes this deterministic: the
         * very next byte read past `payload_len` is a heap-buffer-overflow. */
        bool found_old = vcard_summary_old(payload, payload_len, value);
        printf("found=%d value=\"%s\" (unreachable under ASan)\n", found_old, value);

        free(payload);
    }

    printf("all reachable checks passed\n");
    return 0;
}
