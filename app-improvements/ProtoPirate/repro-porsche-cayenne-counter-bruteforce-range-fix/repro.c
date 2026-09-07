// ALT vs. NEU: Porsche/Touareg (Cayenne-Klasse) Zaehler-Wiederherstellung im Decoder.
// porsche_cayenne_compute_frame() 1:1 aus protocols/porsche_touareg.c uebernommen
// (reiner Bit-/Rotationscode, keine Flipper-/SubGhz-Abhaengigkeiten).
//
// Baut/laeuft ohne Zusatzbibliotheken:
//   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void porsche_cayenne_compute_frame(
    uint32_t serial24,
    uint8_t btn,
    uint16_t counter,
    uint8_t frame_type,
    uint8_t* pkt) {
    uint8_t b0 = (uint8_t)((btn << 4) | (frame_type & 0x07));
    uint8_t b1 = (serial24 >> 16) & 0xFF;
    uint8_t b2 = (serial24 >> 8) & 0xFF;
    uint8_t b3 = serial24 & 0xFF;

    uint16_t cnt = counter + 1;
    uint8_t cnt_lo = cnt & 0xFF;
    uint8_t cnt_hi = (cnt >> 8) & 0xFF;

    uint8_t r_h = b3;
    uint8_t r_m = b1;
    uint8_t r_l = b2;

#define ROTATE24(rh, rm, rl)                 \
    do {                                     \
        uint8_t _ch = ((rh) >> 7) & 1U;      \
        uint8_t _cm = ((rm) >> 7) & 1U;      \
        uint8_t _cl = ((rl) >> 7) & 1U;      \
        (rh) = (uint8_t)(((rh) << 1) | _cm); \
        (rm) = (uint8_t)(((rm) << 1) | _cl); \
        (rl) = (uint8_t)(((rl) << 1) | _ch); \
    } while(0)

    for(uint8_t i = 0; i < 4; i++) {
        ROTATE24(r_h, r_m, r_l);
    }
    for(uint16_t i = 0; i < cnt_lo; i++) {
        ROTATE24(r_h, r_m, r_l);
    }

#undef ROTATE24

    uint8_t a9a = r_h ^ b0;

    uint8_t nb9b_p1 = (uint8_t)((~cnt_lo << 2) & 0xFC) ^ r_m;
    uint8_t nb9b_p2 = (uint8_t)((~cnt_hi << 2) & 0xFC) ^ r_m;
    uint8_t nb9b_p3 = (uint8_t)((~cnt_hi >> 6) & 0x03) ^ r_m;
    uint8_t a9b = (nb9b_p1 & 0xCC) | (nb9b_p2 & 0x30) | (nb9b_p3 & 0x03);

    uint8_t nb9c_p1 = (uint8_t)((~cnt_lo >> 2) & 0x3F) ^ r_l;
    uint8_t nb9c_p2 = (uint8_t)((~cnt_hi & 0x03) << 6) ^ r_l;
    uint8_t nb9c_p3 = (uint8_t)((~cnt_hi >> 2) & 0x3F) ^ r_l;
    uint8_t a9c = (nb9c_p1 & 0x33) | (nb9c_p2 & 0xC0) | (nb9c_p3 & 0x0C);

    pkt[0] = b0;
    pkt[1] = b1;
    pkt[2] = b2;
    pkt[3] = b3;
    pkt[4] = (uint8_t)(((a9a >> 2) & 0x3F) | ((~cnt_lo & 0x03U) << 6));
    pkt[5] = (uint8_t)((~cnt_lo & 0xC0U) | ((a9a & 0x03U) << 4) | (a9b & 0x0CU) |
                       ((~cnt_lo >> 2) & 0x03U));
    pkt[6] = (uint8_t)(((a9b & 0x03U) << 6) | ((a9c >> 2) & 0x3CU) | ((~cnt_lo >> 4) & 0x03U));
    pkt[7] = (uint8_t)(((a9b >> 4) & 0x0FU) | ((a9c & 0x0FU) << 4));
}

// ALT: porsche_cayenne_parse_data(), Suchschleife 1:1 aus protocols/porsche_touareg.c.
static uint16_t recover_cnt_ALT(uint32_t serial, uint8_t btn, uint8_t frame_type, const uint8_t* pkt) {
    uint8_t try_pkt[8];
    for(uint16_t try_cnt = 1; try_cnt <= 256; try_cnt++) {
        porsche_cayenne_compute_frame(serial, btn, (uint16_t)(try_cnt - 1), frame_type, try_pkt);
        if(try_pkt[4] == pkt[4] && try_pkt[5] == pkt[5] && try_pkt[6] == pkt[6] &&
           try_pkt[7] == pkt[7]) {
            return try_cnt;
        }
    }
    return 0;
}

// NEU: Suchbereich auf den vollen uint16_t-Wertebereich des Zaehlerfelds erweitert.
static uint16_t recover_cnt_NEU(uint32_t serial, uint8_t btn, uint8_t frame_type, const uint8_t* pkt) {
    uint8_t try_pkt[8];
    for(uint32_t try_cnt = 1; try_cnt <= 65535; try_cnt++) {
        porsche_cayenne_compute_frame(serial, btn, (uint16_t)(try_cnt - 1), frame_type, try_pkt);
        if(try_pkt[4] == pkt[4] && try_pkt[5] == pkt[5] && try_pkt[6] == pkt[6] &&
           try_pkt[7] == pkt[7]) {
            return (uint16_t)try_cnt;
        }
    }
    return 0;
}

int main(void) {
    const uint32_t serial = 0xABCDEF;
    const uint8_t btn = 0x2;
    const uint8_t frame_type = 0x04;

    // Deckt beide Seiten der 256er-Grenze ab, plus realistische spaetere
    // Zaehlerstaende (jeder Tastendruck erhoeht den Zaehler um 1).
    const uint16_t real_counters[] = {1, 50, 200, 255, 256, 257, 300, 1000, 5000, 30000, 65000, 65535};
    const size_t n = sizeof(real_counters) / sizeof(real_counters[0]);

    int alt_fail = 0, neu_fail = 0;
    printf("%-10s %-18s %-18s\n", "real_cnt", "ALT (Bereich<=256)", "NEU (Bereich<=65535)");
    for(size_t t = 0; t < n; t++) {
        uint16_t real_cnt = real_counters[t];
        uint8_t pkt[8];
        // Sender-Seite: get_upload() reicht (cnt - 1) als counter-Parameter durch
        // (siehe subghz_protocol_encoder_porsche_cayenne_get_upload()).
        uint16_t enc_cnt = (real_cnt > 0) ? (uint16_t)(real_cnt - 1) : 0;
        porsche_cayenne_compute_frame(serial, btn, enc_cnt, frame_type, pkt);

        uint16_t got_alt = recover_cnt_ALT(serial, btn, frame_type, pkt);
        uint16_t got_neu = recover_cnt_NEU(serial, btn, frame_type, pkt);

        int ok_alt = (got_alt == real_cnt);
        int ok_neu = (got_neu == real_cnt);
        if(!ok_alt) alt_fail++;
        if(!ok_neu) neu_fail++;

        printf(
            "%-10u %-5u [%-7s]    %-5u [%-7s]\n",
            real_cnt,
            got_alt,
            ok_alt ? "OK" : "FALSCH",
            got_neu,
            ok_neu ? "OK" : "FALSCH");
    }

    printf("\nALT: %d/%zu Faelle liefern einen falschen Zaehler (0 statt real_cnt).\n", alt_fail, n);
    printf("NEU: %d/%zu Faelle falsch.\n", neu_fail, n);

    return (neu_fail == 0 && alt_fail > 0) ? 0 : 1;
}
