// Host repro: FDX-B "Data Block" sub-field bit order/boundaries, ALT (mntm-012, bei mir) vs.
// NEU (DarkFlippers/unleashed-firmware, Commit 97d6628f3, "Make FDX-B readout more
// descriptive (#4297)"). Transcribes the field-extraction lines from
// protocol_fdx_b_render_data() literally (both fassungen), plus bit_lib_get_bit(),
// bit_lib_get_bits() und bit_lib_reverse_8_fast() byte-genau aus lib/bit_lib/bit_lib.c.
//
// Ground truth wird unabhaengig von den bit_lib-Funktionen berechnet (direkte
// Bit-fuer-Bit-Definition), nicht durch erneutes Aufrufen derselben Funktionen, um eine
// zirkulaere Verifikation zu vermeiden.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- bit_lib.c, woertlich uebernommen (nur die hier benoetigten Funktionen) ----

static bool bit_lib_get_bit(const uint8_t* data, size_t position) {
    return (data[position / 8] >> (7 - (position % 8))) & 1;
}

static uint8_t bit_lib_get_bits(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else {
        uint8_t value = (data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift);
        value = value >> (8 - length);
        return value;
    }
}

static uint8_t bit_lib_reverse_8_fast(uint8_t byte) {
    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
    return byte;
}

// ---- protocol_fdx_b.c, render_data()-Feldauswertung, woertlich ----

typedef struct {
    bool block_status;
    bool rudi_bit;
    uint8_t reserved;
    uint8_t user_info;
    uint8_t replacement_number;
    bool animal_flag;
} FieldsAlt;

// ALT: bei mir (mntm-012), unveraendert seit Erstfassung (Commit 9bfb641d3, "New LF-RFID
// subsystem (#1601)").
static FieldsAlt render_fields_alt(const uint8_t* data) {
    FieldsAlt f;
    f.block_status = bit_lib_get_bit(data, 48);
    f.rudi_bit = bit_lib_get_bit(data, 49);
    f.reserved = bit_lib_get_bits(data, 50, 5);
    f.user_info = bit_lib_get_bits(data, 55, 5);
    f.replacement_number = bit_lib_get_bits(data, 60, 3);
    f.animal_flag = bit_lib_get_bit(data, 63);
    return f;
}

typedef struct {
    bool block_status;
    bool rudi_bit;
    uint8_t visual_start_digit;
    uint8_t reserved;
    uint8_t user_info;
    uint8_t replacement_number;
    bool animal_flag;
} FieldsNeu;

// NEU: DarkFlippers/unleashed-firmware, Commit 97d6628f3.
static FieldsNeu render_fields_neu(const uint8_t* data) {
    FieldsNeu f;
    f.block_status = bit_lib_get_bit(data, 48);
    f.rudi_bit = bit_lib_get_bit(data, 49);
    f.visual_start_digit = bit_lib_reverse_8_fast(bit_lib_get_bits(data, 50, 3) << 5);
    f.reserved = bit_lib_reverse_8_fast(bit_lib_get_bits(data, 53, 2) << 6);
    f.user_info = bit_lib_reverse_8_fast(bit_lib_get_bits(data, 55, 5) << 3);
    f.replacement_number = bit_lib_reverse_8_fast(bit_lib_get_bits(data, 60, 3) << 5);
    f.animal_flag = bit_lib_get_bit(data, 63);
    return f;
}

// ---- Ground truth: direkte Bit-fuer-Bit-Definition, unabhaengig von bit_lib ----
// MSB-zuerst-gelesenes Feld [pos, pos+len) in LSB-zuerst-Reihenfolge zurueckgegeben
// (erstes gelesenes Bit traegt das niedrigstwertige Gewicht) -- genau das Ergebnis, das
// "extrahieren dann volles Byte reversen" fuer ein in ein Byte links-buendig geschobenes
// Feld erzeugt.
static uint32_t ref_reversed_field(const uint8_t* data, size_t pos, uint8_t len) {
    uint32_t value = 0;
    for(uint8_t i = 0; i < len; i++) {
        bool bit = (data[(pos + i) / 8] >> (7 - ((pos + i) % 8))) & 1;
        if(bit) value |= (1u << i);
    }
    return value;
}

// Ground truth fuer eine MSB-zuerst gelesene, nicht reversed Bitfolge [pos, pos+len)
// (erstes gelesenes Bit traegt das hoechstwertige Gewicht) -- unabhaengige Kontrolle fuer
// bit_lib_get_bits() selbst.
static uint32_t ref_msb_field(const uint8_t* data, size_t pos, uint8_t len) {
    uint32_t value = 0;
    for(uint8_t i = 0; i < len; i++) {
        bool bit = (data[(pos + i) / 8] >> (7 - ((pos + i) % 8))) & 1;
        value = (value << 1) | (bit ? 1 : 0);
    }
    return value;
}

static void set_bit(uint8_t* data, size_t position, bool bit) {
    if(bit) {
        data[position / 8] |= 1u << (7 - (position % 8));
    } else {
        data[position / 8] &= ~(1u << (7 - (position % 8)));
    }
}

// Setzt bits48..63 (die hier betrachteten zwei Bytes) aus einer Liste expliziter
// Bitwerte, MSB zuerst (data_bits[0] -> bit48, ..., data_bits[15] -> bit63).
static void build_data_block(uint8_t* data, const bool* data_bits) {
    memset(data, 0, 11);
    for(int i = 0; i < 16; i++) {
        set_bit(data, 48 + i, data_bits[i]);
    }
}

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond, label)                                               \
    do {                                                                 \
        checks_run++;                                                   \
        if(!(cond)) {                                                   \
            checks_failed++;                                             \
            printf("  FAIL: %s\n", label);                               \
        } else {                                                        \
            printf("  ok: %s\n", label);                                 \
        }                                                                \
    } while(0)

int main(void) {
    // Szenario A: gemischtes, nicht-palindromes Bitmuster ueber alle vier Felder, damit
    // ALT/NEU/Ground-Truth fuer jedes Feld unterscheidbare Werte liefern.
    // Reihenfolge (bit48..63): block_status, rudi_bit, visual_start_digit(3),
    // reserved(2), user_info(5), replacement_number(3), animal_flag.
    {
        bool bits[16] = {
            1, 0, // block_status, rudi_bit
            1, 1, 0, // visual_start_digit raw = 110b = 6
            1, 0, // reserved raw = 10b = 2
            1, 0, 1, 1, 0, // user_info raw = 10110b = 22
            1, 1, 0, // replacement_number raw = 110b = 6
            1 // animal_flag
        };
        uint8_t data[11]; // FDXB_DECODED_DATA_SIZE in der echten Firmware
        build_data_block(data, bits);

        FieldsAlt alt = render_fields_alt(data);
        FieldsNeu neu = render_fields_neu(data);

        printf("== Szenario A: gemischtes Bitmuster ==\n");

        // Ground truth fuer die (seit dem Fix) korrekten Feldgrenzen/Bitreihenfolgen.
        uint32_t gt_visual_start_digit = ref_reversed_field(data, 50, 3);
        uint32_t gt_reserved = ref_reversed_field(data, 53, 2);
        uint32_t gt_user_info = ref_reversed_field(data, 55, 5);
        uint32_t gt_replacement_number = ref_reversed_field(data, 60, 3);

        CHECK(gt_visual_start_digit == 3, "Ground Truth Visual Start Digit = reversed(110b) = 3");
        CHECK(gt_reserved == 1, "Ground Truth RFU = reversed(10b) = 1");

        // User Info raw=10110b=22 (5 Bit, b4..b0 = 1,0,1,1,0). Reversed (LSB-zuerst gelesen,
        // erstes gelesenes Bit = b4 wird niedrigstwertig): bit-fuer-Bit Gewicht:
        // i=0 (erstes gelesenes Bit, Originalposition 55) = 1 -> Gewicht 1
        // i=1 (Position 56) = 0 -> Gewicht 2
        // i=2 (Position 57) = 1 -> Gewicht 4
        // i=3 (Position 58) = 1 -> Gewicht 8
        // i=4 (Position 59) = 0 -> Gewicht 16
        // Summe = 1 + 4 + 8 = 13
        CHECK(gt_user_info == 13, "Ground Truth User Info (korrigiert) = 13");
        CHECK(gt_replacement_number == 3, "Ground Truth Replacement Number = reversed(110b) = 3");

        // NEU muss exakt der Ground Truth entsprechen (das ist der eigentliche Zweck des Fixes).
        CHECK(neu.visual_start_digit == gt_visual_start_digit, "NEU Visual Start Digit == Ground Truth");
        CHECK(neu.reserved == gt_reserved, "NEU RFU == Ground Truth");
        CHECK(neu.user_info == gt_user_info, "NEU User Info == Ground Truth");
        CHECK(neu.replacement_number == gt_replacement_number, "NEU Replacement Number == Ground Truth");

        // ALT hat weder die richtigen Feldgrenzen (reserved verschluckt die 3
        // Visual-Start-Digit-Bits) noch die richtige Bitreihenfolge (keine Reversal).
        CHECK(alt.reserved == ref_msb_field(data, 50, 5),
              "ALT RFU liest MSB-zuerst 5 Bit ab Position 50 (verschluckt Visual Start Digit)");
        CHECK(alt.reserved != gt_reserved,
              "ALT RFU-Wert weicht vom (seit dem Fix) korrekten RFU-Wert ab");
        CHECK(alt.user_info == ref_msb_field(data, 55, 5),
              "ALT User Info liest MSB-zuerst (keine Reversal)");
        CHECK(alt.user_info != gt_user_info,
              "ALT User-Info-Wert weicht vom korrekten Wert ab");
        CHECK(alt.replacement_number == ref_msb_field(data, 60, 3),
              "ALT Replacement Number liest MSB-zuerst (keine Reversal)");
        CHECK(alt.replacement_number != gt_replacement_number,
              "ALT Replacement-Number-Wert weicht vom korrekten Wert ab");

        // block_status/rudi_bit/animal_flag sind einzelne Bits, von Reversal/Feldgrenzen
        // unberuehrt -- beide Fassungen muessen hier identisch bleiben (keine Regression).
        CHECK(alt.block_status == neu.block_status, "block_status unveraendert (keine Regression)");
        CHECK(alt.rudi_bit == neu.rudi_bit, "rudi_bit unveraendert (keine Regression)");
        CHECK(alt.animal_flag == neu.animal_flag, "animal_flag unveraendert (keine Regression)");
    }

    // Szenario B: alle 16 Bits 0 -- Regressionsfall, beide Fassungen muessen 0 liefern.
    {
        bool bits[16] = {0};
        uint8_t data[11]; // FDXB_DECODED_DATA_SIZE in der echten Firmware
        build_data_block(data, bits);
        FieldsAlt alt = render_fields_alt(data);
        FieldsNeu neu = render_fields_neu(data);
        printf("== Szenario B: alle Bits 0 (Regression) ==\n");
        CHECK(neu.visual_start_digit == 0 && neu.reserved == 0 && neu.user_info == 0 &&
                  neu.replacement_number == 0,
              "NEU: alle Felder 0 bei Eingabe 0");
        CHECK(alt.reserved == 0 && alt.user_info == 0 && alt.replacement_number == 0,
              "ALT: alle Felder 0 bei Eingabe 0 (keine Regression)");
    }

    // Szenario C: alle 16 Bits 1 -- Regressionsfall, jedes Feld muss auf seinen eigenen
    // Maximalwert sowohl bei ALT als auch bei NEU abbilden (Reversal eines reinen
    // Einsenmusters aendert den Wert nicht).
    {
        bool bits[16];
        for(int i = 0; i < 16; i++) bits[i] = true;
        uint8_t data[11]; // FDXB_DECODED_DATA_SIZE in der echten Firmware
        build_data_block(data, bits);
        FieldsAlt alt = render_fields_alt(data);
        FieldsNeu neu = render_fields_neu(data);
        printf("== Szenario C: alle Bits 1 (Regression) ==\n");
        CHECK(neu.visual_start_digit == 0x7 && neu.reserved == 0x3 && neu.user_info == 0x1F &&
                  neu.replacement_number == 0x7,
              "NEU: alle Felder auf Maximalwert bei Eingabe 1");
        CHECK(alt.reserved == 0x1F && alt.user_info == 0x1F && alt.replacement_number == 0x7,
              "ALT: alte Feldgrenzen/-werte bei Eingabe 1 (reserved deckt 5 statt 2 Bit ab)");
    }

    // Szenario D: nur Visual Start Digit gesetzt (100b = 4), alles andere 0 -- zeigt am
    // deutlichsten, dass ALT dieses Feld komplett verschluckt (in "reserved" absorbiert,
    // dort aber auch falsch angeordnet), waehrend NEU es korrekt und als eigenes Feld zeigt.
    {
        bool bits[16] = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint8_t data[11]; // FDXB_DECODED_DATA_SIZE in der echten Firmware
        build_data_block(data, bits);
        FieldsAlt alt = render_fields_alt(data);
        FieldsNeu neu = render_fields_neu(data);
        printf("== Szenario D: nur Visual Start Digit = 100b (MSB-zuerst) gesetzt ==\n");
        CHECK(neu.visual_start_digit == 1,
              "NEU Visual Start Digit = reversed(100b) = 1 (korrekt als eigenes Feld sichtbar)");
        CHECK(neu.reserved == 0, "NEU RFU bleibt 0 (sauber von Visual Start Digit getrennt)");
        CHECK(alt.reserved == 16,
              "ALT zeigt diese Bits unter 'reserved' = 10000b = 16 -- Visual Start Digit existiert bei ALT gar nicht als Feld");
    }

    printf("\n%d/%d Pruefungen bestanden.\n", checks_run - checks_failed, checks_run);
    if(checks_failed == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("CHECKS FAILED: %d\n", checks_failed);
        return 1;
    }
}
