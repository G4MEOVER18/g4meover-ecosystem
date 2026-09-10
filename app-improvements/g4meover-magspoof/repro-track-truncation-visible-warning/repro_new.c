// NEU: mag_encode() zusaetzlich mit optionalem out_truncated-Ausgabeparameter.
// Reiner Bit-Pack-Nachbau, keine Furi-/Flipper-Typen noetig.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

uint16_t add_bit(int value, uint8_t* out, uint16_t count) {
    uint8_t bit = count % 8;
    uint8_t byte = count / 8;
    if(value) out[byte] |= 0x01;
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

// NEU: zusaetzlicher optionaler out_truncated-Parameter (NULL erlaubt).
uint16_t mag_encode_with_truncation_flag(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes,
    bool* out_truncated) {
    uint16_t raw_bits_count = 0;
    uint16_t output_count = 0;
    int tmp, crc, lrc = 0;

    if(out_truncated) *out_truncated = false;

    const uint16_t raw_bit_capacity = (uint16_t)(out_raw_capacity_bytes * 8);
    const uint16_t max_data_bits =
        (raw_bit_capacity > track_bits) ? (uint16_t)(raw_bit_capacity - track_bits) : 0;

    for(int i = 0; *(data + i) != 0; i++) {
        if((uint16_t)(raw_bits_count + track_bits) > max_data_bits) {
            if(out_truncated) *out_truncated = true;
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

static uint16_t run(const char* s, uint8_t track_bits, uint8_t offset, bool* truncated) {
    uint8_t raw[64] = {0};
    uint8_t manchester[128] = {0};
    char buf[256];
    strcpy(buf, s);
    return mag_encode_with_truncation_flag(
        buf, manchester, raw, track_bits, offset, sizeof(raw), truncated);
}

static char* make_track(int n, char fill) {
    static char buf[256];
    memset(buf, fill, (size_t)n);
    buf[n] = 0;
    return buf;
}

int main(void) {
    bool truncated;

    // Track 1: Grenze bei 72 Zeichen (73. wird gekappt).
    uint16_t bits_72 = run(make_track(72, 'A'), 7, 32, &truncated);
    printf("NEU Track1: 72 Zeichen -> %u Bit, truncated=%d\n", bits_72, truncated);
    assert(truncated == false);

    uint16_t bits_73 = run(make_track(73, 'A'), 7, 32, &truncated);
    printf("NEU Track1: 73 Zeichen -> %u Bit, truncated=%d\n", bits_73, truncated);
    assert(truncated == true);

    // Bitausgabe bleibt identisch zu ALT (reine Zusatzinformation, kein
    // Verhaltensunterschied im gesendeten Signal).
    assert(bits_72 == bits_73);

    // Track 2/3: Grenze bei 101 Zeichen (102. wird gekappt).
    uint16_t bits2_101 = run(make_track(101, '1'), 5, 48, &truncated);
    printf("NEU Track2/3: 101 Zeichen -> %u Bit, truncated=%d\n", bits2_101, truncated);
    assert(truncated == false);

    uint16_t bits2_102 = run(make_track(102, '1'), 5, 48, &truncated);
    printf("NEU Track2/3: 102 Zeichen -> %u Bit, truncated=%d\n", bits2_102, truncated);
    assert(truncated == true);

    // Regressionsfall: deutlich kuerzerer, realer Track (z. B. 40 Zeichen) --
    // truncated muss false bleiben, Bitcount unveraendert zu einem Aufruf
    // ohne out_truncated-Parameter (NULL ist gueltig).
    uint16_t bits_short_noflag = run(make_track(40, 'B'), 7, 32, NULL);
    uint16_t bits_short_flag = run(make_track(40, 'B'), 7, 32, &truncated);
    assert(bits_short_noflag == bits_short_flag);
    assert(truncated == false);
    printf("NEU Regressionsfall (40 Zeichen, NULL vs. &truncated): identischer "
           "Bitcount (%u), truncated=%d\n", bits_short_flag, truncated);

    printf("Alle Faelle bestanden: NEU unterscheidet vollstaendig <-> gekappt, "
           "ALT konnte es nicht.\n");
    return 0;
}
