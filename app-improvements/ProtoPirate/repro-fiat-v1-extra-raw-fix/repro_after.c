// Host repro: fiat_v1.c "ExtraRaw" round trip AFTER the fix
// (exact copy of the patched serialize/deserialize logic, ASan/UBSan build)
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define FIAT_MARELLI_MAX_DATA_BITS 104

static uint8_t get_raw_bit(const uint8_t* raw, uint8_t bit_index) {
    return (raw[bit_index / 8] >> (7 - (bit_index % 8))) & 1U;
}

static void live_capture_build_words(const uint8_t raw_data[13], uint64_t* data_out) {
    uint64_t data = 0;
    for(uint8_t bit_count = 0; bit_count < 64; bit_count++) {
        data = (data << 1) | get_raw_bit(raw_data, bit_count);
    }
    *data_out = data;
}

// --- fixed fiat_marelli_rebuild_raw_data() ---
static void rebuild_raw_data_fixed(
    uint8_t raw_data[13],
    uint64_t key,
    const uint8_t* extra_raw,
    uint8_t extra_raw_len,
    uint32_t* extra_data_out) {
    memset(raw_data, 0, 13);

    for(uint8_t i = 0; i < 8; i++) {
        raw_data[i] = (uint8_t)(key >> (56 - i * 8));
    }

    for(uint8_t i = 0; i < extra_raw_len && (8U + i) < 13; i++) {
        raw_data[8 + i] = extra_raw[i];
    }

    *extra_data_out = ((uint32_t)raw_data[9] << 24) | ((uint32_t)raw_data[10] << 16) |
                      ((uint32_t)raw_data[11] << 8) | (uint32_t)raw_data[12];
}

int main(void) {
    uint8_t original[13] = {
        0xFF, 0xFC, 0xDE, 0xAD, 0xBE, 0xEF, 0x81, 0x2B, 0x11, 0x22, 0x33, 0x44, 0x55};

    uint64_t data;
    live_capture_build_words(original, &data);

    // serialize(): now writes raw_data[8..12] directly (5 bytes) instead of
    // packing 40 bits into a uint32_t.
    uint8_t extra_raw_on_disk[5];
    memcpy(extra_raw_on_disk, &original[8], 5);

    // deserialize(): reads those 5 bytes back and rebuilds raw_data.
    uint8_t reconstructed[13];
    uint32_t extra_data;
    rebuild_raw_data_fixed(reconstructed, data, extra_raw_on_disk, sizeof(extra_raw_on_disk), &extra_data);

    printf("byte | original | reconstructed after save+load round trip (fixed)\n");
    int mismatches = 0;
    for(int i = 0; i < 13; i++) {
        int mismatch = original[i] != reconstructed[i];
        mismatches += mismatch;
        printf(" %2d  |   %02X     |   %02X%s\n", i, original[i], reconstructed[i], mismatch ? "   <-- MISMATCH" : "");
    }
    printf("\ntotal corrupted bytes out of 13: %d\n", mismatches);
    printf("extra_data (for get_hash_data) = %08x (matches live-capture truncated value 0x22334455)\n", extra_data);
    assert(mismatches == 0 && "all 13 bytes must round-trip exactly after the fix");
    assert(extra_data == 0x22334455);
    printf("\nConfirmed: full byte-exact round trip after the fix, no data loss.\n");
    return 0;
}
