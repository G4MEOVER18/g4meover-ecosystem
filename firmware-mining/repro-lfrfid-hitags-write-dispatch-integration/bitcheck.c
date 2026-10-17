#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Reference, already-shipped-and-trusted formula from protocol_em4100.c's T5577 branch:
//   request->t5577.block[1] = protocol->encoded_data >> 32;   // bits 63..32
//   request->t5577.block[2] = protocol->encoded_data;         // bits 31..0 (implicit uint32_t truncation)
// My new HitagS branch expresses the SAME bits as MSB-first byte arrays instead of 32-bit words:
//   page4[i] = (encoded_data >> (56 - i*8)) & 0xFF;  // should equal the 4 bytes of block[1], MSB first
//   page5[i] = (encoded_data >> (24 - i*8)) & 0xFF;  // should equal the 4 bytes of block[2], MSB first

static int check(uint64_t encoded_data) {
    uint32_t block1 = (uint32_t)(encoded_data >> 32);
    uint32_t block2 = (uint32_t)(encoded_data);

    uint8_t page4[4], page5[4];
    for(int i = 0; i < 4; i++) {
        page4[i] = (encoded_data >> (56 - i * 8)) & 0xFF;
        page5[i] = (encoded_data >> (24 - i * 8)) & 0xFF;
    }

    // Reassemble page4/page5 MSB-first into 32-bit words and compare against block1/block2.
    uint32_t page4_word = ((uint32_t)page4[0] << 24) | ((uint32_t)page4[1] << 16) |
                           ((uint32_t)page4[2] << 8) | page4[3];
    uint32_t page5_word = ((uint32_t)page5[0] << 24) | ((uint32_t)page5[1] << 16) |
                           ((uint32_t)page5[2] << 8) | page5[3];

    int ok = (page4_word == block1) && (page5_word == block2);
    printf("%s  encoded_data=0x%016llX  block1=0x%08X page4=0x%08X  block2=0x%08X page5=0x%08X\n",
           ok ? "PASS" : "FAIL", (unsigned long long)encoded_data, block1, page4_word, block2,
           page5_word);
    return ok;
}

int main(void) {
    uint64_t vectors[] = {
        0x0000000000000000ULL,
        0xFFFFFFFFFFFFFFFFULL,
        0xFF806000001C6AC1ULL, // realistic-shaped EM4100 frame (header 0xFF8, payload, parity, stop)
        0xAA55AA55AA55AA55ULL, // alternating
        0x0123456789ABCDEFULL,
        0x8000000000000001ULL,
        0x0000000100000000ULL, // isolates bit 32 (first bit of the lower word)
        0x0000000080000000ULL, // isolates bit 31 (last bit of the upper word)
    };
    int n = sizeof(vectors) / sizeof(vectors[0]);
    int all_ok = 1;
    for(int i = 0; i < n; i++) {
        if(!check(vectors[i])) all_ok = 0;
    }
    printf(all_ok ? "ALLE CHECKS BESTANDEN\n" : "FEHLER\n");
    return all_ok ? 0 : 1;
}
