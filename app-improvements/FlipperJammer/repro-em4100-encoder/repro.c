/*
 * Host-kompilierbares Repro fuer den EM4100-Encoder-Bug in
 * FlipperJammer/modules/spoofer.c (em4100_encode()).
 *
 * ALT: 1:1 aus dem Original uebernommen (nur uint16_t statt der echten
 *      Firmware-Typen, da hier keine Flipper-Header verfuegbar sind).
 * NEU: der vorgeschlagene Fix, jetzt mit echter LevelDuration-artiger
 *      Level/Duration-Struktur statt eines reinen Dauer-Arrays.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
 *   ./repro
 *
 * Erwartete Ausgabe: siehe Testhinweis in der .md-Datei im selben Ordner.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/* ALT: 1:1 aus modules/spoofer.c (Original)                           */
/* ------------------------------------------------------------------ */

static uint16_t em4100_encode_old(uint32_t facility, uint32_t card_id, uint16_t* buf) {
    uint64_t data = ((uint64_t)0x01 << 32) |
                    ((uint64_t)(facility & 0xFF) << 24) |
                    ((uint64_t)(card_id  & 0xFFFF) << 8);

    uint16_t i = 0;

    for(int h = 0; h < 9; h++) {
        buf[i++] = 64;
        buf[i++] = 64;
    }

    for(int bit = 39; bit >= 0; bit--) {
        uint8_t b = (uint8_t)((data >> bit) & 1);
        if(b) {
            buf[i++] = 64; /* space */
            buf[i++] = 64; /* mark  */
        } else {
            buf[i++] = 64; /* mark  */
            buf[i++] = 64; /* space */
        }
    }

    buf[i++] = 64;

    return i;
}

/* ------------------------------------------------------------------ */
/* NEU: Fix — LevelDuration-artige Struktur (Level explizit, nicht     */
/*      implizit ueber die Array-Position), plus vollstaendiger        */
/*      64-Bit-EM4100-Frame inkl. 10 Zeilen- und 4 Spaltenparitaets-    */
/*      Bits (bislang komplett gefehlt).                               */
/* ------------------------------------------------------------------ */

typedef struct {
    bool     level;
    uint32_t duration;
} LevelDurationSim; /* Host-Abbild von lib/toolbox/level_duration.h */

static inline LevelDurationSim ld_make(bool level, uint32_t duration) {
    LevelDurationSim ld = {level, duration};
    return ld;
}

#define EM4100_HALF_BIT_TICKS 64

static uint16_t em4100_encode_new(uint32_t facility, uint32_t card_id, LevelDurationSim* buf) {
    uint64_t data = ((uint64_t)0x01 << 32) |
                    ((uint64_t)(facility & 0xFF) << 24) |
                    ((uint64_t)(card_id  & 0xFFFF) << 8);

    uint16_t i = 0;

    /* Manchester (EM4100): Bit=1 -> High-dann-Low, Bit=0 -> Low-dann-High */
    #define EM4100_EMIT_BIT(bitval)                                              \
        do {                                                                      \
            bool first_high = (bitval) ? true : false;                           \
            buf[i++] = ld_make(first_high, EM4100_HALF_BIT_TICKS);               \
            buf[i++] = ld_make(!first_high, EM4100_HALF_BIT_TICKS);              \
        } while(0)

    /* 9 Header-Bits (alle '1') */
    for(int h = 0; h < 9; h++) EM4100_EMIT_BIT(1);

    /* 10 Zeilen a 4 Datenbits + 1 gerades Zeilenparitaetsbit,
       plus laufende Spaltenparitaet ueber alle 10 Zeilen */
    uint8_t col_parity[4] = {0, 0, 0, 0};
    for(int row = 0; row < 10; row++) {
        uint8_t nibble = (uint8_t)((data >> (36 - row * 4)) & 0xF);
        uint8_t row_parity = 0;
        for(int b = 3; b >= 0; b--) {
            uint8_t bit = (nibble >> b) & 1;
            row_parity ^= bit;
            col_parity[3 - b] ^= bit;
            EM4100_EMIT_BIT(bit);
        }
        EM4100_EMIT_BIT(row_parity);
    }

    /* 4 Spaltenparitaetsbits */
    for(int c = 0; c < 4; c++) EM4100_EMIT_BIT(col_parity[c]);

    /* 1 Stop-Bit (fest 0) */
    EM4100_EMIT_BIT(0);

    #undef EM4100_EMIT_BIT
    return i; /* 9 + 50 + 4 + 1 = 64 logische Bits * 2 = 128 Eintraege */
}

/* ------------------------------------------------------------------ */
/* Decoder fuer NEU — liest den erzeugten Level/Duration-Stream        */
/* zurueck und prueft Header, Ziel-Nibbles sowie beide Paritaeten.     */
/* Dient nur der Verifikation dieses Repros, ist kein Firmware-Code.   */
/* ------------------------------------------------------------------ */

static bool em4100_decode_new(
    const LevelDurationSim* buf,
    uint16_t                len,
    uint32_t*               out_facility,
    uint32_t*               out_card_id) {
    if(len != 128) return false;

    uint16_t idx = 0;
    for(int h = 0; h < 9; h++) {
        if(!buf[idx].level) return false; /* Header-Bit muss '1' dekodieren */
        idx += 2;
    }

    uint8_t nibbles[10];
    uint8_t col_parity_seen[4] = {0, 0, 0, 0};
    for(int row = 0; row < 10; row++) {
        uint8_t nib = 0;
        uint8_t row_parity = 0;
        for(int b = 3; b >= 0; b--) {
            bool bit = buf[idx].level; /* erste Haelfte = HIGH => Bit 1 */
            idx += 2;
            nib |= (bit ? 1 : 0) << b;
            row_parity ^= bit ? 1 : 0;
            col_parity_seen[3 - b] ^= bit ? 1 : 0;
        }
        bool parity_bit = buf[idx].level;
        idx += 2;
        if((parity_bit ? 1 : 0) != row_parity) return false; /* Zeilenparitaet falsch */
        nibbles[row] = nib;
    }

    for(int c = 0; c < 4; c++) {
        bool parity_bit = buf[idx].level;
        idx += 2;
        if((parity_bit ? 1 : 0) != col_parity_seen[c]) return false; /* Spaltenparitaet falsch */
    }

    /* Stop-Bit ignorieren (fest 0) */

    uint64_t data = 0;
    for(int row = 0; row < 10; row++) data = (data << 4) | nibbles[row];

    uint8_t  version  = (uint8_t)(data >> 32);
    uint8_t  facility = (uint8_t)(data >> 24);
    uint16_t card_id  = (uint16_t)(data >> 8);
    if(version != 0x01) return false;

    *out_facility = facility;
    *out_card_id  = card_id;
    return true;
}

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    /* --- Test 1: ALT ist datenunabhaengig ------------------------- */
    /* Zwei klar unterschiedliche Kartendaten muessen im Buffer zu
       unterschiedlichen Werten fuehren, wenn der Encoder die Daten
       tatsaechlich kodiert. ALT schreibt in beiden if/else-Zweigen
       denselben Literalwert (64) und ignoriert damit faktisch den
       berechneten Bitwert. */
    uint16_t buf_a[128], buf_b[128];
    uint16_t n_a = em4100_encode_old(0x11, 0x2222, buf_a);
    uint16_t n_b = em4100_encode_old(0x99, 0xAAAA, buf_b);

    printf("[ALT] Laenge A=%u B=%u\n", n_a, n_b);
    assert(n_a == n_b);
    int identical = (memcmp(buf_a, buf_b, n_a * sizeof(uint16_t)) == 0);
    printf("[ALT] Buffer fuer facility=0x11/card_id=0x2222 vs. "
           "facility=0x99/card_id=0xAAAA identisch: %s\n",
           identical ? "JA (Bug bestaetigt)" : "nein");
    assert(identical); /* ALT: Bug reproduziert sich zuverlaessig */

    /* --- Test 2: ALT hat auch keine Paritaetsbits (nur 50 statt     */
    /*     64 logische Bits, 100 statt 128 Eintraege) ---------------- */
    printf("[ALT] Eintraege gesamt: %u (erwartet fuer echtes EM4100: 128)\n", n_a);
    /* 9 Header-Bits + 40 Datenbits je 2 Eintraege (Manchester-Halbperioden),
       plus 1 einzelner Stop-Eintrag (kein Bitpaar) — die 14 Zeilen-/
       Spaltenparitaetsbits aus dem Kommentar am Funktionsanfang fehlen
       komplett: (9+40)*2 + 1 = 99 statt der fuer ein echtes 64-Bit-
       EM4100-Frame noetigen 128 Eintraege. */
    assert(n_a == 99);

    /* --- Test 3: NEU kodiert unterschiedliche Karten unterschiedlich */
    LevelDurationSim buf_c[128], buf_d[128];
    uint16_t n_c = em4100_encode_new(0x11, 0x2222, buf_c);
    uint16_t n_d = em4100_encode_new(0x99, 0xAAAA, buf_d);
    printf("[NEU] Laenge A=%u B=%u (erwartet 128)\n", n_c, n_d);
    assert(n_c == 128 && n_d == 128);
    int identical_new = (memcmp(buf_c, buf_d, n_c * sizeof(LevelDurationSim)) == 0);
    printf("[NEU] Buffer fuer beide Karten identisch: %s\n",
           identical_new ? "ja (FEHLER)" : "NEIN (korrekt unterschiedlich)");
    assert(!identical_new);

    /* --- Test 4: NEU laesst sich korrekt zurueckdekodieren,          */
    /*     inklusive Zeilen- und Spaltenparitaetspruefung ------------- */
    uint32_t dec_facility, dec_card_id;
    bool ok_c = em4100_decode_new(buf_c, n_c, &dec_facility, &dec_card_id);
    printf("[NEU] Decode A: ok=%d facility=0x%02X card_id=0x%04X (erwartet 0x11/0x2222)\n",
           ok_c, dec_facility, dec_card_id);
    assert(ok_c && dec_facility == 0x11 && dec_card_id == 0x2222);

    bool ok_d = em4100_decode_new(buf_d, n_d, &dec_facility, &dec_card_id);
    printf("[NEU] Decode B: ok=%d facility=0x%02X card_id=0x%04X (erwartet 0x99/0xAAAA)\n",
           ok_d, dec_facility, dec_card_id);
    assert(ok_d && dec_facility == 0x99 && dec_card_id == 0xAAAA);

    /* --- Test 5: absichtlich verfaelschtes Zeilenparitaetsbit muss   */
    /*     vom Decoder erkannt werden (beweist, dass die Paritaet      */
    /*     tatsaechlich aus den Daten berechnet wird, nicht nur         */
    /*     mitgefuehrt wird) ------------------------------------------ */
    LevelDurationSim buf_e[128];
    uint16_t n_e = em4100_encode_new(0x11, 0x2222, buf_e);
    buf_e[18].level = !buf_e[18].level; /* erstes Datenbit der ersten Zeile kippen */
    bool ok_e = em4100_decode_new(buf_e, n_e, &dec_facility, &dec_card_id);
    printf("[NEU] Decode mit verfaelschtem Bit: ok=%d (erwartet 0, Paritaetsfehler erkannt)\n",
           ok_e);
    assert(!ok_e);

    printf("\nAlle Tests bestanden.\n");
    return 0;
}
