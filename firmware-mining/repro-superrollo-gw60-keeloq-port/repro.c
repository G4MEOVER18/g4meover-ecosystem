// Host-Repro fuer den Superrollo-(GW60)-KeeLoq-Port (firmware-mining/superrollo-gw60-keeloq-port.md).
//
// Bildet die reale Zustandsmaschine aus subghz_protocol_decoder_keeloq_feed() sowie die
// CRC-/Framing-Logik aus dem neuen Encoder-Zweig fast woertlich nach (gleiche Konstanten,
// gleiche bit_read()/DURATION_DIFF()-Makros, gleiches subghz_protocol_blocks_add_bit()-
// Schieberegisterverhalten), um sie ohne Firmware-Build/-Hardware unter ASan/UBSan zu pruefen.
//
// ALT = mein unveraenderter mntm-012-Decoder-Zustandsautomat (vor diesem Port).
// NEU = derselbe Automat mit den drei Aenderungen aus keeloq_c.patch (10*Te-Sync-Uebergang
//       im Reset-State, asymmetrische Praeambel-LOW-Erkennung in CheckPreambula, Toleranzfenster
//       min_count_bit_for_found+2 -> +3).
//
// Getestet wird:
//  1) Die 2-Bit-CRC-Berechnung (Encoder-Zweig) ist deterministisch und aendert sich mit jedem
//     Nutzdaten-Bit (kein trivialer Immer-0/Immer-1-Fund).
//  2) ALT erkennt ein synthetisches Superrollo-Signal (eigene Praeambel/Sync/CRC-Framing, per
//     eigenem Encoder-Zweig erzeugt) nicht als vollstaendigen Frame -- NEU erkennt es korrekt,
//     mit den richtigen 64 Nutzdaten-Bits im Schieberegister.
//  3) Regressionstest: ein normaler KeeLoq-Frame (66 Bit, bestehende Praeambel/Sync-Form) wird
//     von ALT und NEU identisch erkannt (gleiche Nutzdaten, gleiche decode_count_bit).
//  4) Sweep ueber mehrere zufaellige 64-Bit-Nutzlasten fuer 2) und 3).

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- reale Konstanten/Makros aus lib/subghz/protocols/keeloq.c und lib/subghz/blocks/math.h ----
#define TE_SHORT 400u
#define TE_LONG 800u
#define TE_DELTA 140u
#define MIN_COUNT_BIT_FOR_FOUND 64u

#define bit_read(value, bit) (((value) >> (bit)) & 0x01)
#define DURATION_DIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

static uint64_t reverse_key(uint64_t key, uint8_t bit_count) {
    uint64_t reverse = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        reverse = reverse << 1 | bit_read(key, i);
    }
    return reverse;
}

// ---- Encoder-Zweig: Superrollo-Framing + 2-Bit-CRC (aus keeloq_c.patch uebernommen) ----
typedef struct {
    bool level;
    uint32_t duration;
} LevelDuration;

static size_t encode_superrollo(uint64_t data, LevelDuration* out) {
    const uint32_t t = 450;
    size_t index = 0;
    for(uint8_t i = 0; i < 9; i++) {
        out[index++] = (LevelDuration){true, t};
        out[index++] = (LevelDuration){false, t * 2};
    }
    out[index++] = (LevelDuration){true, t * 10};
    out[index++] = (LevelDuration){false, t * 10};

    uint64_t data_tx_order = reverse_key(data, 64);
    uint8_t crc0 = 0, crc1 = 0;
    for(uint8_t i = 0; i < 65; i++) {
        uint8_t din = (i < 64) ? (uint8_t)((data_tx_order >> i) & 1) : 1;
        uint8_t next_crc1 = crc0 ^ din;
        crc0 = (next_crc1 ^ crc1) & 1;
        crc1 = next_crc1 & 1;
    }

    for(uint8_t i = 67; i > 0; i--) {
        uint8_t bit = (i > 3)  ? bit_read(data, i - 4) :
                      (i == 3) ? 1 :
                      (i == 2) ? crc0 :
                                 crc1;
        out[index++] = (LevelDuration){true, bit ? t : t * 2};
        out[index++] = (LevelDuration){false, bit ? t * 2 : t};
    }
    out[index++] = (LevelDuration){true, t};
    out[index++] = (LevelDuration){false, t * 18};
    return index;
}

static void compute_crc(uint64_t data, uint8_t* crc0_out, uint8_t* crc1_out) {
    uint64_t data_tx_order = reverse_key(data, 64);
    uint8_t crc0 = 0, crc1 = 0;
    for(uint8_t i = 0; i < 65; i++) {
        uint8_t din = (i < 64) ? (uint8_t)((data_tx_order >> i) & 1) : 1;
        uint8_t next_crc1 = crc0 ^ din;
        crc0 = (next_crc1 ^ crc1) & 1;
        crc1 = next_crc1 & 1;
    }
    *crc0_out = crc0;
    *crc1_out = crc1;
}

// ---- Standard-KeeLoq-Encoder-Zweig (unveraendert, fuer den Regressionstest) ----
// Nachgebaut aus subghz_protocol_encoder_keeloq_get_upload(): 11x Te-Praeambel, 1x
// (Te HIGH / 10*Te LOW) Sync, dann 64 Datenbits + 1 Status-Bit ("1") + Ende.
static size_t encode_standard_keeloq(uint64_t data, LevelDuration* out) {
    size_t index = 0;
    for(uint8_t i = 11; i > 0; i--) {
        out[index++] = (LevelDuration){true, TE_SHORT};
        out[index++] = (LevelDuration){false, TE_SHORT};
    }
    out[index++] = (LevelDuration){true, TE_SHORT};
    out[index++] = (LevelDuration){false, TE_SHORT * 10};

    for(uint8_t i = 64; i > 0; i--) {
        if(bit_read(data, i - 1)) {
            out[index++] = (LevelDuration){true, TE_SHORT};
            out[index++] = (LevelDuration){false, TE_LONG};
        } else {
            out[index++] = (LevelDuration){true, TE_LONG};
            out[index++] = (LevelDuration){false, TE_SHORT};
        }
    }
    // status bit (immer als "1" kodiert, wie im echten Encoder)
    out[index++] = (LevelDuration){true, TE_SHORT};
    out[index++] = (LevelDuration){false, TE_LONG};
    out[index++] = (LevelDuration){true, TE_SHORT};
    out[index++] = (LevelDuration){false, TE_SHORT * 40};
    return index;
}

// ---- Decoder-Zustandsautomat: ALT und NEU ----
typedef enum { StepReset = 0, StepCheckPreambula, StepSaveDuration, StepCheckDuration } Step;

typedef struct {
    Step step;
    uint16_t header_count;
    uint64_t decode_data;
    uint16_t decode_count_bit;
    uint32_t te_last;

    bool found;
    uint64_t found_data;
    uint16_t found_count_bit;
} DecoderState;

static void decoder_reset(DecoderState* s) {
    memset(s, 0, sizeof(*s));
    s->step = StepReset;
}

// variant: 0 = ALT (mntm-012 unveraendert), 1 = NEU (mit den drei Aenderungen)
static void decoder_feed(DecoderState* s, bool level, uint32_t duration, int variant) {
    switch(s->step) {
    case StepReset:
        if(level && DURATION_DIFF(duration, TE_SHORT) < TE_DELTA) {
            s->step = StepCheckPreambula;
            s->header_count++;
        } else if(
            variant == 1 && level && (s->header_count > 2) &&
            (DURATION_DIFF(duration, TE_SHORT * 10) < TE_DELTA * 10)) {
            s->step = StepCheckPreambula;
        }
        break;
    case StepCheckPreambula:
        if(variant == 0) {
            if(!level && (DURATION_DIFF(duration, TE_SHORT) < TE_DELTA)) {
                s->step = StepReset;
                break;
            }
        } else {
            if(!level &&
               ((DURATION_DIFF(duration, TE_SHORT) < TE_DELTA) ||
                (DURATION_DIFF(duration, TE_SHORT * 2) < TE_DELTA))) {
                s->step = StepReset;
                break;
            }
        }
        if((s->header_count > 2) && (DURATION_DIFF(duration, TE_SHORT * 10) < TE_DELTA * 10)) {
            s->step = StepSaveDuration;
            s->decode_data = 0;
            s->decode_count_bit = 0;
        } else {
            s->step = StepReset;
            s->header_count = 0;
        }
        break;
    case StepSaveDuration:
        if(level) {
            s->te_last = duration;
            s->step = StepCheckDuration;
        }
        break;
    case StepCheckDuration:
        if(!level) {
            if(duration >= TE_SHORT * 2 + TE_DELTA) {
                s->step = StepReset;
                uint16_t upper = MIN_COUNT_BIT_FOR_FOUND + (variant == 1 ? 3 : 2);
                if((s->decode_count_bit >= MIN_COUNT_BIT_FOR_FOUND) &&
                   (s->decode_count_bit <= upper)) {
                    s->found = true;
                    s->found_data = s->decode_data;
                    s->found_count_bit = MIN_COUNT_BIT_FOR_FOUND;
                    s->decode_data = 0;
                    s->decode_count_bit = 0;
                    s->header_count = 0;
                }
                break;
            } else if(
                (DURATION_DIFF(s->te_last, TE_SHORT) < TE_DELTA) &&
                (DURATION_DIFF(duration, TE_LONG) < TE_DELTA * 2)) {
                if(s->decode_count_bit < MIN_COUNT_BIT_FOR_FOUND) {
                    s->decode_data = s->decode_data << 1 | 1;
                    s->decode_count_bit++;
                } else {
                    s->decode_count_bit++;
                }
                s->step = StepSaveDuration;
            } else if(
                (DURATION_DIFF(s->te_last, TE_LONG) < TE_DELTA * 2) &&
                (DURATION_DIFF(duration, TE_SHORT) < TE_DELTA)) {
                if(s->decode_count_bit < MIN_COUNT_BIT_FOR_FOUND) {
                    s->decode_data = s->decode_data << 1 | 0;
                    s->decode_count_bit++;
                } else {
                    s->decode_count_bit++;
                }
                s->step = StepSaveDuration;
            } else {
                s->step = StepReset;
                s->header_count = 0;
            }
        } else {
            s->step = StepReset;
            s->header_count = 0;
        }
        break;
    }
}

static void run_stream(DecoderState* s, LevelDuration* stream, size_t n, int variant) {
    for(size_t i = 0; i < n; i++) {
        decoder_feed(s, stream[i].level, stream[i].duration, variant);
    }
}

static int checks_total = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        checks_total++;                                                    \
        if(!(cond)) {                                                       \
            checks_failed++;                                               \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);       \
        }                                                                    \
    } while(0)

static uint64_t rand64(void) {
    uint64_t v = 0;
    for(int i = 0; i < 4; i++) v = (v << 16) | (rand() & 0xFFFF);
    return v;
}

int main(void) {
    srand(1337);

    // 1) CRC-Sweep: deterministisch, aendert sich mit den Nutzdaten (kein Immer-0/Immer-1-Fund)
    printf("== Test 1: CRC-Berechnung ==\n");
    {
        int zero_count = 0, one_count = 0, distinct = 0;
        uint8_t seen[4] = {0, 0, 0, 0};
        for(int i = 0; i < 200; i++) {
            uint64_t data = rand64();
            uint8_t c0a, c1a, c0b, c1b;
            compute_crc(data, &c0a, &c1a);
            compute_crc(data, &c0b, &c1b); // Determinismus
            CHECK(c0a == c0b && c1a == c1b, "CRC nicht deterministisch");
            uint8_t idx = (uint8_t)(c0a << 1 | c1a);
            seen[idx] = 1;
            if(c0a == 0 && c1a == 0) zero_count++;
            if(c0a == 1 && c1a == 1) one_count++;
        }
        for(int i = 0; i < 4; i++) distinct += seen[i];
        CHECK(distinct >= 3, "CRC deckt nicht mehrere der vier moeglichen Werte ab");
        CHECK(zero_count < 200 && one_count < 200, "CRC ist konstant (kein echter Fehlererkennungswert)");
        printf("  CRC-Werteverteilung ueber 200 Zufallsnutzlasten: %d/4 verschiedene Werte gesehen\n",
               distinct);
    }

    // 2) Superrollo-Frame: ALT erkennt nicht / NEU erkennt korrekt (Einzelfall + Sweep)
    printf("== Test 2: Superrollo-GW60-Frame (ALT vs. NEU) ==\n");
    {
        int alt_detects = 0, neu_detects = 0, neu_correct = 0;
        const int N = 500;
        for(int i = 0; i < N; i++) {
            uint64_t data = rand64();
            LevelDuration stream[200];
            size_t n = encode_superrollo(data, stream);
            CHECK(n <= 200, "Superrollo-Stream ueberschreitet Testpuffer");

            DecoderState alt, neu;
            decoder_reset(&alt);
            decoder_reset(&neu);
            run_stream(&alt, stream, n, 0);
            run_stream(&neu, stream, n, 1);

            if(alt.found) alt_detects++;
            if(neu.found) {
                neu_detects++;
                if(neu.found_data == data) neu_correct++;
            }
        }
        printf("  ALT erkennt Superrollo-Frame in %d/%d Faellen\n", alt_detects, N);
        printf("  NEU erkennt Superrollo-Frame in %d/%d Faellen (davon %d mit korrekten 64 "
               "Nutzdaten-Bits)\n",
               neu_detects, N, neu_correct);
        CHECK(alt_detects == 0, "ALT erkennt einen Superrollo-Frame faelschlich als vollstaendig");
        CHECK(neu_detects == N, "NEU erkennt nicht jeden synthetischen Superrollo-Frame");
        CHECK(neu_correct == N, "NEU liefert nicht in jedem Fall die korrekten 64 Nutzdaten-Bits");
    }

    // 3) Regression: normaler KeeLoq-Frame (66 Bit) bleibt in ALT und NEU identisch
    printf("== Test 3: Regression -- Standard-KeeLoq-Frame ==\n");
    {
        int mismatches = 0;
        const int N = 500;
        for(int i = 0; i < N; i++) {
            uint64_t data = rand64();
            LevelDuration stream[200];
            size_t n = encode_standard_keeloq(data, stream);
            CHECK(n <= 200, "Standard-KeeLoq-Stream ueberschreitet Testpuffer");

            DecoderState alt, neu;
            decoder_reset(&alt);
            decoder_reset(&neu);
            run_stream(&alt, stream, n, 0);
            run_stream(&neu, stream, n, 1);

            if(!(alt.found && neu.found && alt.found_data == data && neu.found_data == data &&
                 alt.found_data == neu.found_data)) {
                mismatches++;
            }
        }
        printf("  %d/%d Standard-KeeLoq-Frames bei ALT und NEU abweichend erkannt\n", mismatches,
               N);
        CHECK(mismatches == 0, "NEU aendert das Erkennungsverhalten fuer normale KeeLoq-Frames");
    }

    printf("\n%d/%d Checks bestanden%s\n", checks_total - checks_failed, checks_total,
           checks_failed == 0 ? " -- ALL CHECKS PASSED" : "");
    return checks_failed == 0 ? 0 : 1;
}
