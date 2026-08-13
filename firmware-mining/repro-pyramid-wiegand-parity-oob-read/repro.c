// Host-repro: protocol_pyramid_add_wiegand_parity() liest fuer das zweite (odd) Paritaetsbit
// aus "source + length / 2" -- length ist eine BIT-Zahl (24), "+" auf einem uint8_t* ist aber
// Byte-Arithmetik, verschiebt den Pointer also um 12 BYTES statt 12 BIT. wiegand[] im Aufrufer
// (protocol_pyramid_encode()) ist nur 3 Byte gross -> Lesezugriff bis zu 9 Byte hinter dem
// Puffer. ALT/NEU sind 1:1 aus lib/lfrfid/protocols/protocol_pyramid.c uebernommen (nur
// Funktionsnamen sind pro Variante eindeutig gemacht, keine sonstige Aenderung); die
// verwendeten bit_lib-Grundfunktionen (get_bit/set_bit/copy_bits) sind ebenfalls 1:1 aus
// lib/bit_lib/bit_lib.c uebernommen.
//
// Standardbuild prueft nur die NEUE (gefixte) Variante ueber den vollen FC-Raum (8 Bit) x
// Stichprobe CardNum (16 Bit) gegen eine unabhaengige Referenzimplementierung -- muss unter
// ASan/UBSan sauber durchlaufen und in jedem Fall den Referenzwert liefern:
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Mit -DDEMO_OLD_STATS wird zusaetzlich die ALTE (fehlerhafte) Variante auf einem 15-Byte-
// Puffer aufgerufen (wiegand[3] + 12 Byte bekanntes Fuellmuster dahinter, kein Sicherheitsabstand
// noetig) -- zeigt quantitativ, wie oft das odd-Paritaetsbit von echten Kartendaten abweicht:
//   gcc -O0 -Wall -Wextra -DDEMO_OLD_STATS -o repro_stats repro.c && ./repro_stats
//
// Mit -DDEMO_OLD_BUG wird die ALTE Variante mit einem exakt 3 Byte grossen Heap-Puffer
// aufgerufen (identisch zur Groesse von "uint8_t wiegand[3]" im echten Aufrufer) -- muss unter
// ASan mit heap-buffer-overflow abbrechen (Beleg des Speicherzugriffsfehlers, nicht nur ein
// falscher Wert):
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_OLD_BUG -o repro_bug repro.c && ./repro_bug

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- bit_lib.c, Stand mntm-012 (unveraendert) ----

static void bit_lib_set_bit(uint8_t* data, size_t position, bool bit) {
    if(bit) {
        data[position / 8] |= (uint8_t)(1u << (7 - (position % 8)));
    } else {
        data[position / 8] &= (uint8_t)~(1u << (7 - (position % 8)));
    }
}

static bool bit_lib_get_bit(const uint8_t* data, size_t position) {
    return (data[position / 8] >> (7 - (position % 8))) & 1;
}

static void bit_lib_copy_bits(
    uint8_t* data,
    size_t position,
    size_t length,
    const uint8_t* source,
    size_t source_position) {
    for(size_t i = 0; i < length; ++i) {
        bit_lib_set_bit(data, position + i, bit_lib_get_bit(source, source_position + i));
    }
}

// ---- ALT: mntm-012, lib/lfrfid/protocols/protocol_pyramid.c (unveraendert) ----

static bool old_get_parity(const uint8_t* bits, uint8_t type, int length) {
    int x;
    for(x = 0; length > 0; --length) x += bit_lib_get_bit(bits, (size_t)(length - 1));
    x %= 2;
    return x ^ type;
}

static void old_add_wiegand_parity(
    uint8_t* target,
    uint8_t target_position,
    uint8_t* source,
    uint8_t length) {
    bit_lib_set_bit(target, target_position, old_get_parity(source, 0 /* even */, length / 2));
    bit_lib_copy_bits(target, target_position + 1, length, source, 0);
    bit_lib_set_bit(
        target,
        target_position + length + 1,
        // Bug: verschiebt den Pointer um length/2=12 BYTES statt Bits.
        old_get_parity(source + length / 2, 1 /* odd */, length / 2));
}

// ---- NEU: PR #4426-Fassung (flipperdevices/flipperzero-firmware, Commit 9c173b46) ----

static bool new_get_parity(const uint8_t* bits, size_t position, uint8_t type, int length) {
    int x;
    for(x = 0; length > 0; --length)
        x += bit_lib_get_bit(bits, position + (size_t)(length - 1));
    x %= 2;
    return x ^ type;
}

static void new_add_wiegand_parity(
    uint8_t* target,
    uint8_t target_position,
    uint8_t* source,
    uint8_t length) {
    bit_lib_set_bit(target, target_position, new_get_parity(source, 0, 0 /* even */, length / 2));
    bit_lib_copy_bits(target, target_position + 1, length, source, 0);
    bit_lib_set_bit(
        target, target_position + length + 1, new_get_parity(source, length / 2, 1, length / 2));
}

// Referenz, unabhaengig von beiden Implementierungen: odd parity ueber Bits 12..23 von
// wiegand[3] (FC in Bits 0..7, CardNum in Bits 8..23 -- siehe protocol_pyramid_encode()).
static bool reference_odd_parity_bits_12_23(const uint8_t wiegand[3]) {
    int x = 0;
    for(int bitpos = 12; bitpos < 24; ++bitpos) x += bit_lib_get_bit(wiegand, (size_t)bitpos);
    return (bool)((x % 2) ^ 1);
}

static void build_wiegand(uint8_t wiegand[3], uint8_t fc, uint16_t card_num) {
    memset(wiegand, 0, 3);
    for(int i = 0; i < 8; i++) bit_lib_set_bit(wiegand, (size_t)i, (fc >> (7 - i)) & 1);
    for(int i = 0; i < 16; i++)
        bit_lib_set_bit(wiegand, (size_t)(8 + i), (card_num >> (15 - i)) & 1);
}

static bool run_new_over_wiegand(const uint8_t wiegand[3]) {
    uint8_t w[3];
    memcpy(w, wiegand, 3);
    uint8_t pre[16];
    memset(pre, 0, sizeof(pre));
    new_add_wiegand_parity(pre, 0, w, 24);
    return bit_lib_get_bit(pre, 25); // target_position(0) + length(24) + 1
}

int main(void) {
#if defined(DEMO_OLD_BUG)
    // Exakt so gross wie "uint8_t wiegand[3]" in protocol_pyramid_encode() -- kein
    // Sicherheitsabstand danach, wie im echten Aufrufer.
    uint8_t* wiegand = malloc(3);
    build_wiegand(wiegand, 0x13, 0x1234);
    uint8_t pre[16];
    memset(pre, 0, sizeof(pre));
    printf("== ALT, wiegand[3] exakt allokiert (erwartet: ASan heap-buffer-overflow) ==\n");
    old_add_wiegand_parity(pre, 0, wiegand, 24);
    printf("Odd-Paritaetsbit (ALT): %d -- diese Zeile sollte unter ASan nie erreicht werden.\n",
           bit_lib_get_bit(pre, 25));
    free(wiegand);
    return 1;
#elif defined(DEMO_OLD_STATS)
    printf("== ALT, wiegand[3] + 12 Byte Fuellmuster dahinter (kein ASan, Statistik) ==\n");
    int wrong = 0, total = 0;
    for(int fc = 0; fc <= 0xFF; fc++) {
        for(int cn = 0; cn <= 0xFFFF; cn += 257) { // Stichprobe, deckt vollen 16-Bit-Raum ab
            uint8_t arena[3 + 12];
            build_wiegand(arena, (uint8_t)fc, (uint16_t)cn);
            memset(arena + 3, 0xA5, 12); // bekanntes Fuellmuster statt Stack-/Heap-Muell
            bool ref = reference_odd_parity_bits_12_23(arena);

            uint8_t pre[16];
            memset(pre, 0, sizeof(pre));
            old_add_wiegand_parity(pre, 0, arena, 24);
            bool got = bit_lib_get_bit(pre, 25);

            total++;
            if(got != ref) wrong++;
        }
    }
    printf(
        "Odd-Paritaetsbit falsch ggue. den echten Kartendaten (Bits 12..23 von wiegand[]): "
        "%d/%d (%.1f%%)\n",
        wrong,
        total,
        100.0 * wrong / total);
    printf("(Wert haengt zusaetzlich vom Fuellmuster ab -- mit anderem Muster aendert sich der "
           "Anteil, bleibt aber in der Groessenordnung 'etwa die Haelfte', wie im Upstream-Fix "
           "beschrieben; mit echter Stack-Nachbarvariable statt festem Muster ist das Bit "
           "zusaetzlich unvorhersagbar von Compiler/Optimierung abhaengig.)\n");
    return (wrong > 0) ? 0 : 1;
#else
    printf("== NEU: Korrektheitspruefung ueber vollen FC-Raum x CardNum-Stichprobe ==\n");
    int mismatches = 0, total = 0;
    for(int fc = 0; fc <= 0xFF; fc++) {
        for(int cn = 0; cn <= 0xFFFF; cn += 257) {
            uint8_t wiegand[3];
            build_wiegand(wiegand, (uint8_t)fc, (uint16_t)cn);
            bool ref = reference_odd_parity_bits_12_23(wiegand);
            bool got = run_new_over_wiegand(wiegand);
            total++;
            if(got != ref) mismatches++;
        }
    }
    printf("Abweichungen von der Referenz-Paritaet: %d/%d\n", mismatches, total);
    if(mismatches == 0) {
        printf("PASS: NEU liefert in allen %d Faellen das korrekte odd-Paritaetsbit.\n", total);
        return 0;
    } else {
        printf("FAIL: NEU weicht in %d Faellen ab.\n", mismatches);
        return 1;
    }
#endif
}
