// Host repro for nfctools' t4t_read_binary() (core/nfc_tools_desfire.c).
// Isolates the exact copy logic (no Flipper/NFC/poller dependencies) to show
// that ALT (as in the app) writes past caller-supplied output buffers when a
// T4T READ BINARY response carries more data than the caller asked for/has
// room for, and that NEU (clamped to `length`, the caller's declared Le /
// buffer capacity) does not.
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mirrors ApduResp from nfc_tools_desfire.c (fixed 258-byte buffer, matches
// the app's own bit_buffer_alloc(258) cap on the RX side).
typedef struct {
    uint8_t data[258];
    size_t len;
    bool ok;
} ApduResp;

// ALT: t4t_read_binary() exactly as in core/nfc_tools_desfire.c (sw_ok()
// inlined as "r->ok && r->len >= 2 && trailing SW == 90 00", the only path
// exercised here).
static bool t4t_read_binary_old(const ApduResp* r, uint8_t length, uint8_t* out, size_t* out_len) {
    (void)length; // NOT used to bound the copy -- this is the bug
    bool sw_ok = r->ok && r->len >= 2 && r->data[r->len - 2] == 0x90 && r->data[r->len - 1] == 0x00;
    if(!sw_ok || r->len < 2) return false;
    size_t dlen = r->len - 2;
    if(out_len) *out_len = dlen;
    if(out && dlen) memcpy(out, r->data, dlen);
    return true;
}

// NEU: same function, one added clamp line.
static bool t4t_read_binary_new(const ApduResp* r, uint8_t length, uint8_t* out, size_t* out_len) {
    bool sw_ok = r->ok && r->len >= 2 && r->data[r->len - 2] == 0x90 && r->data[r->len - 1] == 0x00;
    if(!sw_ok || r->len < 2) return false;
    size_t dlen = r->len - 2;
    if(dlen > length) dlen = length; // NEW: never exceed the caller's declared Le / buffer size
    if(out_len) *out_len = dlen;
    if(out && dlen) memcpy(out, r->data, dlen);
    return true;
}

static ApduResp make_resp(size_t payload_len) {
    ApduResp r = {0};
    r.ok = true;
    for(size_t i = 0; i < payload_len; i++) r.data[i] = (uint8_t)(0x41 + (i % 26));
    r.data[payload_len] = 0x90;
    r.data[payload_len + 1] = 0x00;
    r.len = payload_len + 2;
    return r;
}

int main(void) {
    // Case 1: well-behaved DESFire T4T tag replying to
    // "READ BINARY offset=0 length=15" (the app's CC-file read, CC_SIZE=15)
    // with exactly the 15 bytes asked for. Both variants must behave
    // identically -- regression check.
    {
        ApduResp r = make_resp(15);
        uint8_t* out_old = malloc(15);
        uint8_t* out_new = malloc(15);
        size_t len_old = 0, len_new = 0;
        bool ok_old = t4t_read_binary_old(&r, 15, out_old, &len_old);
        bool ok_new = t4t_read_binary_new(&r, 15, out_new, &len_new);
        printf(
            "case 1 (well-behaved, 15/15 bytes) -- ALT: ok=%d len=%zu | NEU: ok=%d len=%zu | "
            "match=%d\n",
            ok_old,
            len_old,
            ok_new,
            len_new,
            (ok_old == ok_new && len_old == len_new && memcmp(out_old, out_new, len_old) == 0));
        free(out_old);
        free(out_new);
    }

    // Case 2: rogue/malicious T4T card. The app requests the 15-byte CC
    // (`t4t_read_binary(poller, 0, (uint8_t)CC_SIZE, cc, &cc_read)` with
    // `uint8_t cc[CC_SIZE];` on the stack) but the attacker-controlled card
    // answers with 200 bytes of payload before the 90 00 status word --
    // nothing on the RF/host side enforces the Le the reader asked for.
    {
        ApduResp r = make_resp(200);
        uint8_t cc_new[15]; // exact stack layout used by the real cc[CC_SIZE]
        size_t len_new = 0;
        printf("case 2 (malicious tag, 200 bytes into a 15-byte cc[CC_SIZE]) -- ");
        fflush(stdout);
        bool ok_new = t4t_read_binary_new(&r, 15, cc_new, &len_new);
        printf("NEU: ok=%d len=%zu (expected: truncated to 15, no OOB write)\n", ok_new, len_new);

        printf("case 2 -- ALT: ");
        fflush(stdout);
        uint8_t cc_old[15];
        size_t len_old = 0;
        // Under ASan this line aborts before returning: memcpy() writes
        // 200 bytes into a 15-byte stack buffer.
        t4t_read_binary_old(&r, 15, cc_old, &len_old);
        printf("ok, len=%zu (should not be reached under ASan)\n", len_old);
    }

    return 0;
}
