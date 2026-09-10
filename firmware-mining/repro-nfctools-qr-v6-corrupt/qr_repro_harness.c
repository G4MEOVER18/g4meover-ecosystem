// Host harness around the (unmodified or patched) nfc_tools_qr_encode(): renders
// the encoded module matrix to a plain PGM bitmap so it can be decoded with any
// standard QR reader (e.g. Python's pyzbar) for a real, spec-level pass/fail
// check instead of just re-deriving the same buffer content.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nfc_tools_qrcodegen.h"

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "usage: %s <text> <out.pgm>\n", argv[0]);
        return 2;
    }
    static uint8_t tmp[QRCODEGEN_BUF_MAX];
    static uint8_t qr[QRCODEGEN_BUF_MAX];
    bool ok = nfc_tools_qr_encode(argv[1], tmp, qr);
    if(!ok) {
        fprintf(stderr, "encode failed\n");
        return 1;
    }
    int sz = nfc_tools_qr_size(qr);
    int scale = 8;
    int quiet = 4;
    int total = sz + 2 * quiet;
    int px = total * scale;
    FILE* f = fopen(argv[2], "wb");
    fprintf(f, "P5\n%d %d\n255\n", px, px);
    for(int y = 0; y < px; y++) {
        for(int x = 0; x < px; x++) {
            int mx = x / scale - quiet;
            int my = y / scale - quiet;
            unsigned char v = 255;
            if(mx >= 0 && mx < sz && my >= 0 && my < sz) {
                if(nfc_tools_qr_module(qr, mx, my)) v = 0;
            }
            fputc(v, f);
        }
    }
    fclose(f);
    fprintf(stderr, "version=%d size=%d\n", qr[0], sz);
    return 0;
}
