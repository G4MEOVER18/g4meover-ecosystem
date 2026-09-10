// ALT: mag_encode() nach dem bereits vorgeschlagenen Kapazitaets-Fix
// (mag-encode-fixed-buffer-overflow-fix.md) -- verhindert den Overflow,
// meldet eine Kappung aber an niemanden. Reiner Bit-Pack-Nachbau, keine
// Furi-/Flipper-Typen noetig.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

// Identisch zum bereits vorgeschlagenen Kapazitaets-Fix aus
// mag-encode-fixed-buffer-overflow-fix.md -- kappt korrekt, aber ohne
// jedes Signal an den Aufrufer.
uint16_t mag_encode_capacity_limited(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes) {
    uint16_t raw_bits_count = 0;
    uint16_t output_count = 0;
    int tmp, crc, lrc = 0;

    const uint16_t raw_bit_capacity = (uint16_t)(out_raw_capacity_bytes * 8);
    const uint16_t max_data_bits =
        (raw_bit_capacity > track_bits) ? (uint16_t)(raw_bit_capacity - track_bits) : 0;

    for(int i = 0; *(data + i) != 0; i++) {
        if((uint16_t)(raw_bits_count + track_bits) > max_data_bits) break;

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

static uint16_t run(const char* s, uint8_t track_bits, uint8_t offset) {
    uint8_t raw[64] = {0};
    uint8_t manchester[128] = {0};
    char buf[256];
    strcpy(buf, s);
    return mag_encode_capacity_limited(buf, manchester, raw, track_bits, offset, sizeof(raw));
}

static char* make_track(int n, char fill) {
    static char buf[256];
    memset(buf, fill, (size_t)n);
    buf[n] = 0;
    return buf;
}

int main(void) {
    // Track 1: track_bits=7, ascii offset 32 (Space=0). Grenze liegt laut
    // mag-encode-fixed-buffer-overflow-fix.md bei 72 Zeichen (73. wird gekappt).
    uint16_t bits_72 = run(make_track(72, 'A'), 7, 32);
    uint16_t bits_73 = run(make_track(73, 'A'), 7, 32);

    printf("ALT Track1: 72 Zeichen -> %u Bit, 73 Zeichen -> %u Bit\n", bits_72, bits_73);

    // Kernbefund: die Funktion liefert fuer 72 und 73 Eingabezeichen exakt
    // denselben Bitcount/dieselbe Bitfolge zurueck -- der Aufrufer kann aus
    // dem Rueckgabewert allein nicht unterscheiden, ob die Eingabe komplett
    // gesendet wurde oder ob am Ende ein Zeichen stillschweigend verworfen
    // wurde.
    assert(bits_72 == bits_73);
    printf("ALT: 72- und 73-Zeichen-Eingabe erzeugen identischen Output -- "
           "keine Moeglichkeit, die Kappung des 73. Zeichens zu erkennen.\n");

    // Track 2/3: track_bits=5, ascii offset 48 ('0'=0). Grenze liegt bei 101
    // Zeichen (102. wird gekappt).
    uint16_t bits2_101 = run(make_track(101, '1'), 5, 48);
    uint16_t bits2_102 = run(make_track(102, '1'), 5, 48);
    printf("ALT Track2/3: 101 Zeichen -> %u Bit, 102 Zeichen -> %u Bit\n", bits2_101, bits2_102);
    assert(bits2_101 == bits2_102);
    printf("ALT: dieselbe blinde Kappung bei Track 2/3.\n");

    return 0;
}
