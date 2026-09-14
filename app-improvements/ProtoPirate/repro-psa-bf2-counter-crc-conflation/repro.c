// Host-Repro: PsaBfState-Fuellfunktion nach BF2-Fund vermischt CRC16-Bytes mit Counter-Daten
// Modelliert byte-genau psa_crypto_bf.c: psa_bf_fill_state_from_buffer() (ALT, unveraendert fuer
// BF1) und die neue BF2-spezifische Variante (NEU), gegen die im selben Quelltext bereits
// vorhandene BF2-Erfolgspruefung (psa_crypto_crc16_bf2 ueber buffer[2..7], erwartet == dec_v1
// & 0xFFFF, d.h. buffer[8]:buffer[9] als volles 16-Bit-CRC).
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

// --- 1:1 aus protocols/psa_crypto.c ---
void psa_crypto_unpack_tea_result_to_buffer(uint8_t* buffer, uint32_t v0, uint32_t v1) {
    buffer[2] = (uint8_t)((v0 >> 24) & 0xFF);
    buffer[3] = (uint8_t)((v0 >> 16) & 0xFF);
    buffer[4] = (uint8_t)((v0 >> 8) & 0xFF);
    buffer[5] = (uint8_t)(v0 & 0xFF);
    buffer[6] = (uint8_t)((v1 >> 24) & 0xFF);
    buffer[7] = (uint8_t)((v1 >> 16) & 0xFF);
    buffer[8] = (uint8_t)((v1 >> 8) & 0xFF);
    buffer[9] = (uint8_t)(v1 & 0xFF);
}

uint16_t psa_crypto_crc16_bf2(uint8_t* buffer, int length) {
    uint16_t crc = 0;
    for(int i = 0; i < length; i++) {
        crc = crc ^ ((uint16_t)buffer[i] << 8);
        for(int j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
            crc = crc & 0xFFFF;
        }
    }
    return crc & 0xFFFF;
}

typedef struct {
    uint8_t decrypted_button;
    uint32_t decrypted_serial;
    uint32_t decrypted_counter;
    uint16_t decrypted_crc;
    uint32_t decrypted_seed;
    uint8_t decrypted_type;
} PsaBfState;

// --- ALT: psa_bf_fill_state_from_buffer() 1:1 aus psa_crypto_bf.c (aktuell fuer BF1 UND BF2
// verwendet) ---
static void psa_bf_fill_state_from_buffer_ALT(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = 0x36;
}

// --- NEU: eigene BF2-Variante, nur am BF2-Erfolgs-Aufrufort eingesetzt (BF1-Pfad/-Funktion
// bleibt exakt unveraendert) ---
static void psa_bf_fill_state_from_buffer_bf2_NEU(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = ((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = 0x36;
}

// Baut wie psa_brute_force_run() (BF2-Zweig) crc_buffer[6] aus dec_v0/dec_v1 und liefert das
// tatsaechlich gegen dec_v1&0xFFFF verglichene CRC16 -- identisch zur echten Pruefung im
// Quelltext, keine eigene Interpretation.
static uint16_t real_bf2_crc16(uint32_t dec_v0, uint32_t dec_v1) {
    uint8_t crc_buffer[6] = {
        (uint8_t)((dec_v0 >> 24) & 0xFF),
        (uint8_t)((dec_v0 >> 8) & 0xFF),
        (uint8_t)((dec_v0 >> 16) & 0xFF),
        (uint8_t)(dec_v0 & 0xFF),
        (uint8_t)((dec_v1 >> 24) & 0xFF),
        (uint8_t)((dec_v1 >> 16) & 0xFF),
    };
    return psa_crypto_crc16_bf2(crc_buffer, 6);
}

// BF1-Erfolgspruefung (psa_crypto_tea_crc) 1:1 aus psa_crypto.c/psa_crypto_bf.c, zum
// Regressionsnachweis, dass der BF1-Pfad vom Fund/Fix unberuehrt bleibt.
uint8_t psa_crypto_tea_crc(uint32_t v0, uint32_t v1) {
    uint32_t crc = ((v0 >> 24) & 0xFF) + ((v0 >> 16) & 0xFF) + ((v0 >> 8) & 0xFF) + (v0 & 0xFF);
    crc += ((v1 >> 24) & 0xFF) + ((v1 >> 16) & 0xFF) + ((v1 >> 8) & 0xFF);
    return (uint8_t)(crc & 0xFF);
}

static void bf1_regression_check(void) {
    // BF1-Erfolgsfall: buffer[8] ist echte Counter-Nutzlast (v1-Byte1), buffer[9] allein das
    // 8-Bit-CRC (siehe psa_crypto_bf.c Zeile 93-94: "crc == (dec_v1 & 0xFF)"). Fuer den
    // BF1-Aufrufort bleibt psa_bf_fill_state_from_buffer_ALT() unveraendert im Einsatz --
    // hier nur zur Dokumentation, dass Fund/Fix diesen Pfad nicht veraendern.
    uint32_t dec_v0 = 0x12345678u;
    for(uint32_t v1_hi3 = 0; v1_hi3 < 0x1000000u; v1_hi3 += 0x111111u) {
        uint32_t probe_v1 = v1_hi3 << 8;
        uint8_t crc = psa_crypto_tea_crc(dec_v0, probe_v1);
        uint32_t dec_v1 = probe_v1 | crc; // erfuellt die reale BF1-Erfolgspruefung exakt
        assert(psa_crypto_tea_crc(dec_v0, dec_v1) == (dec_v1 & 0xFF));

        uint8_t buffer[16] = {0};
        psa_crypto_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
        PsaBfState alt = {0};
        psa_bf_fill_state_from_buffer_ALT(&alt, buffer);

        // buffer[8] (v1-Byte1) ist hier echte Counter-Nutzlast -- muss weiterhin im Counter
        // landen, buffer[9] allein bleibt das vollstaendige (8-Bit-)CRC.
        assert((alt.decrypted_counter & 0xFF) == buffer[8]);
        assert(alt.decrypted_crc == buffer[9]);
    }
    printf("BF1-Regressionscheck: psa_bf_fill_state_from_buffer_ALT() bleibt fuer den "
           "BF1-Erfolgsfall unveraendert korrekt (Counter/CRC-Aufteilung wie vor dem Fix).\n");
}

int main(void) {
    bf1_regression_check();

    int cases = 0, alt_wrong_crc = 0, alt_wrong_counter = 0, neu_wrong = 0;

    // Testraum: verschiedene dec_v0-Werte, dec_v1-Oberteil (Bits 31-16, "echte" Nutzdaten) frei
    // gewaehlt; dec_v1-Unterteil (Bits 15-0) direkt auf das dazu gehoerige echte CRC16 gesetzt,
    // damit die reale BF2-Erfolgspruefung ("crc16 == dec_v1 & 0xFFFF") in jedem Testfall wie bei
    // einem echten Fund erfuellt ist.
    uint32_t v0_samples[] = {0x00000012u, 0xAABBCC34u, 0x11223344u, 0xFFFFFF0Fu, 0x00000000u};
    uint32_t v1_hi_samples[] = {0x00000000u, 0x00001234u, 0x0000FFFFu, 0x0000ABCDu};

    for(size_t i = 0; i < sizeof(v0_samples) / sizeof(v0_samples[0]); i++) {
        for(size_t j = 0; j < sizeof(v1_hi_samples) / sizeof(v1_hi_samples[0]); j++) {
            uint32_t dec_v0 = v0_samples[i];
            uint32_t v1_hi = v1_hi_samples[j] << 16;

            // v1's Unterhaelfte zunaechst 0, um das reale CRC16 fuer diesen Fall zu berechnen
            uint32_t dec_v1_probe = v1_hi;
            uint16_t real_crc = real_bf2_crc16(dec_v0, dec_v1_probe);
            uint32_t dec_v1 = v1_hi | real_crc; // erfuellt die reale BF2-Erfolgspruefung exakt

            // Gegenprobe: die reale Pruefung akzeptiert diesen synthetischen Fund tatsaechlich
            assert(real_bf2_crc16(dec_v0, dec_v1) == (dec_v1 & 0xFFFF));

            uint8_t buffer[16] = {0};
            psa_crypto_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);

            PsaBfState alt = {0}, neu = {0};
            psa_bf_fill_state_from_buffer_ALT(&alt, buffer);
            psa_bf_fill_state_from_buffer_bf2_NEU(&neu, buffer);

            cases++;
            int alt_crc_ok = (alt.decrypted_crc == real_crc);
            int alt_counter_contaminated = ((alt.decrypted_counter & 0xFF) == buffer[8]) &&
                                            (buffer[8] != 0);
            int neu_crc_ok = (neu.decrypted_crc == real_crc);
            int neu_counter_clean = ((neu.decrypted_counter & 0xFF) == 0);

            if(!alt_crc_ok) alt_wrong_crc++;
            if(alt_counter_contaminated) alt_wrong_counter++;
            if(!neu_crc_ok || !neu_counter_clean) neu_wrong++;

            printf(
                "case v0=%08X v1=%08X realCRC16=%04X | ALT crc=%04X (%s) cnt=%08X (%s) | "
                "NEU crc=%04X (%s) cnt=%08X (%s)\n",
                dec_v0,
                dec_v1,
                real_crc,
                alt.decrypted_crc,
                alt_crc_ok ? "OK" : "FALSCH",
                alt.decrypted_counter,
                alt_counter_contaminated ? "durch CRC-Byte verunreinigt" : "unverunreinigt",
                neu.decrypted_crc,
                neu_crc_ok ? "OK" : "FALSCH",
                neu.decrypted_counter,
                neu_counter_clean ? "sauber" : "FALSCH");
            fflush(stdout);
        }
    }

    printf(
        "\n%d Faelle, ALT falsches CRC in %d/%d, ALT verunreinigter Counter in %d/%d, "
        "NEU fehlerhaft in %d/%d\n",
        cases,
        alt_wrong_crc,
        cases,
        alt_wrong_counter,
        cases,
        neu_wrong,
        cases);

    // Regressionscheck BF1-Pfad: psa_bf_fill_state_from_buffer_ALT() bleibt fuer den BF1-Aufrufort
    // unveraendert im Einsatz -- hier nur zur Dokumentation, dass die BF1-Semantik (CRC ==
    // buffer[9] allein, siehe psa_crypto_bf.c Zeile 93-94) von diesem Fund/Fix nicht beruehrt
    // wird: buffer[8] ist im BF1-Erfolgsfall echte Counter-Nutzlast, kein CRC-Byte.
    assert(alt_wrong_crc == cases - 1); // ALT falsch in allen Faellen ausser dem entarteten Alles-Null-Fall (CRC16-Oberbyte zufaellig 0)
    assert(alt_wrong_counter == cases - 1); // dieselbe Ausnahme wie oben
    assert(neu_wrong == 0);              // NEU liefert in KEINEM Testfall ein falsches Ergebnis

    printf("\nAlle Kernaussagen bestaetigt (exit 0).\n");
    return 0;
}
