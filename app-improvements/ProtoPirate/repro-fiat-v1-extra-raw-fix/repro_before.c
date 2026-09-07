// Host repro: fiat_v1.c "Extra"/extra_data residual-bit round trip
// (bit-packing exactly as in protocols/fiat_v1.c, unmodified logic, ASan/UBSan build)
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define FIAT_MARELLI_MAX_DATA_BITS 104

static uint8_t get_raw_bit(const uint8_t* raw, uint8_t bit_index) {
    return (raw[bit_index / 8] >> (7 - (bit_index % 8))) & 1U;
}

// --- exact copy of the live-capture accumulation from
// subghz_protocol_decoder_fiat_marelli_feed() (lines 357-361 of fiat_v1.c) ---
static void live_capture_build_words(const uint8_t raw_data[13], uint64_t* data_out, uint32_t* extra_out) {
    uint64_t data = 0;
    uint32_t extra_data = 0;
    for(uint8_t bit_count = 0; bit_count < FIAT_MARELLI_MAX_DATA_BITS; bit_count++) {
        uint32_t new_bit = get_raw_bit(raw_data, bit_count);
        if(bit_count < 64) {
            data = (data << 1) | new_bit;
        } else {
            extra_data = (extra_data << 1) | new_bit; // <-- extra_data is uint32_t, 40 bits fed in
        }
    }
    *data_out = data;
    *extra_out = extra_data;
}

// --- exact copy of fiat_marelli_rebuild_raw_data() from fiat_v1.c (deserialize path) ---
static void rebuild_raw_data(uint8_t raw_data[13], uint64_t key, uint32_t extra_data, uint8_t data_count_bit) {
    memset(raw_data, 0, 13);

    for(uint8_t i = 0; i < 8; i++) {
        raw_data[i] = (uint8_t)(key >> (56 - i * 8));
    }

    uint8_t extra_bits = (data_count_bit > 64) ? (data_count_bit - 64) : 0;
    for(uint8_t i = 0; i < extra_bits && i < 32; i++) { // <-- "&& i < 32" cap, exactly as upstream
        uint8_t byte_idx = 8 + (i / 8);
        uint8_t bit_pos = 7 - (i % 8);
        if(extra_data & (1UL << (extra_bits - 1 - i))) {
            raw_data[byte_idx] |= (1U << bit_pos);
        }
    }
}

int main(void) {
    // A representative 104-bit frame (13 bytes), fully populated with a
    // recognizable, non-trivial bit pattern in the residual 5 bytes (8..12).
    uint8_t original[13] = {
        0xFF, 0xFC,             // preamble residue
        0xDE, 0xAD, 0xBE, 0xEF, // serial
        0x81,                   // Button:4 | Epoch:4
        0x2B,                   // Counter:5 | Scramble:2 | Fixed:1
        0x11, 0x22, 0x33, 0x44, 0x55 // "encrypted payload" (40 bits) incl. CRC byte
    };

    uint64_t data;
    uint32_t extra_data;
    live_capture_build_words(original, &data, &extra_data);
    printf("data (64-bit, bytes 0-7)      = %016lx\n", (unsigned long)data);
    printf("extra_data (uint32_t) after capturing 40 residual bits = %08x\n", extra_data);
    printf("  (expected 40-bit value would be 0x1122334455, only 32 bits fit -> truncated)\n");

    // Round-trip exactly as a save-to-file + load-from-file cycle would:
    // serialize() stores `extra_data` and `Extra_bits=40`; deserialize() calls
    // fiat_marelli_rebuild_raw_data() with those two values.
    uint8_t reconstructed[13];
    rebuild_raw_data(reconstructed, data, extra_data, FIAT_MARELLI_MAX_DATA_BITS);

    printf("\nbyte | original | reconstructed after save+load round trip\n");
    int mismatches = 0;
    for(int i = 0; i < 13; i++) {
        int mismatch = original[i] != reconstructed[i];
        mismatches += mismatch;
        printf(" %2d  |   %02X     |   %02X%s\n", i, original[i], reconstructed[i], mismatch ? "   <-- MISMATCH" : "");
    }
    printf("\ntotal corrupted bytes out of 13: %d\n", mismatches);
    assert(mismatches > 0 && "expected bug to reproduce: bytes 8 and 12 corrupted");
    assert(reconstructed[8] != original[8] && "raw_data[8] (first residual byte) must be lost to 32-bit truncation");
    assert(reconstructed[12] == 0 && "raw_data[12] (CRC byte) must be zeroed by the 'i < 32' cap");
    printf("\nConfirmed: byte 8 corrupted by extra_data truncation (40 bits -> 32-bit type),\n");
    printf("and byte 12 (CRC) always ends up 0x00 because of the additional 'i < 32' loop cap\n");
    printf("in fiat_marelli_rebuild_raw_data(), independent of the truncation.\n");
    return 0;
}
