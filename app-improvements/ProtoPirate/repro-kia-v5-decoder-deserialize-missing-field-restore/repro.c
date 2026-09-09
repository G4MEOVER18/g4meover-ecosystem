// Host-Repro (kein Flipper/keine ProtoPirate-Laufzeit noetig): reine Datentransformation.
//
// Modelliert den Kia-V5-Decoder-Zustand (Ausschnitt aus protocols/kia_v5.c) und stellt
// ALT (heutiges deserialize(), liest nur generic.data/data_count_bit) NEU (deserialize()
// rekonstruiert Yek/Sn/Btn/Cnt zusaetzlich aus generic.data, exakt dieselbe Formel wie
// KiaV5DecoderStepData in kia_protocol_decoder_v5_feed()) gegenueber.
//
// mixer_decode()/build_keystore_from_mfkey()/pp_reverse_bits8() sind 1:1 aus
// protocols/kia_v5.c bzw. protocols/protocols_common.c uebernommen; get_kia_v5_key() ist
// hier durch einen festen Test-Schluessel ersetzt (Original liest ihn aus dem
// Flipper-Schluesselspeicher) -- der genaue Schluesselwert ist fuer den Fund irrelevant,
// entscheidend ist nur, dass ALT und NEU denselben Schluessel/dieselbe Funktion nutzen wie
// der echte Live-Decode-Pfad.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- 1:1 aus protocols/protocols_common.c ----
static uint8_t pp_reverse_bits8(uint8_t value) {
    uint8_t result = 0;
    for(int i = 0; i < 8; i++) {
        if(value & (1 << i)) result |= (1 << (7 - i));
    }
    return result;
}

// ---- Test-Ersatz fuer protocols/keys.c:get_kia_v5_key() ----
static uint64_t get_kia_v5_key_stub(void) {
    return 0x1122334455667788ULL;
}

// ---- 1:1 aus protocols/kia_v5.c ----
static uint8_t keystore_bytes[8] = {0};

static void build_keystore_from_mfkey(uint8_t* result) {
    uint64_t mfkey = get_kia_v5_key_stub();
    for(int i = 0; i < 8; i++) {
        result[i] = (uint8_t)((mfkey >> ((7 - i) * 8)) & 0xFFU);
    }
}

static uint16_t mixer_decode(uint32_t encrypted) {
    uint8_t s0 = (encrypted & 0xFF);
    uint8_t s1 = (encrypted >> 8) & 0xFF;
    uint8_t s2 = (encrypted >> 16) & 0xFF;
    uint8_t s3 = (encrypted >> 24) & 0xFF;

    build_keystore_from_mfkey(keystore_bytes);

    int round_index = 1;
    for(size_t i = 0; i < 18; i++) {
        uint8_t r = keystore_bytes[round_index] & 0xFF;
        int steps = 8;
        while(steps > 0) {
            uint8_t base;
            if((s3 & 0x40) == 0) {
                base = (s3 & 0x02) == 0 ? 0x74 : 0x2E;
            } else {
                base = (s3 & 0x02) == 0 ? 0x3A : 0x5C;
            }

            if(s2 & 0x08) {
                base = (((base >> 4) & 0x0F) | ((base & 0x0F) << 4)) & 0xFF;
            }
            if(s1 & 0x01) {
                base = ((base & 0x3F) << 2) & 0xFF;
            }
            if(s0 & 0x01) {
                base = (base << 1) & 0xFF;
            }

            uint8_t temp = (s3 ^ s1) & 0xFF;
            s3 = ((s3 & 0x7F) << 1) & 0xFF;
            if(s2 & 0x80) {
                s3 |= 0x01;
            }
            s2 = ((s2 & 0x7F) << 1) & 0xFF;
            if(s1 & 0x80) {
                s2 |= 0x01;
            }
            s1 = ((s1 & 0x7F) << 1) & 0xFF;
            if(s0 & 0x80) {
                s1 |= 0x01;
            }
            s0 = ((s0 & 0x7F) << 1) & 0xFF;

            uint8_t chk = (base ^ (r ^ temp)) & 0xFF;
            if(chk & 0x80) {
                s0 |= 0x01;
            }
            r = ((r & 0x7F) << 1) & 0xFF;
            steps--;
        }
        round_index = (round_index - 1) & 0x7;
    }
    return (s0 + (s1 << 8)) & 0xFFFF;
}

// ---- Modell des Decoder-Zustands (Ausschnitt aus struct SubGhzProtocolDecoderKiaV5) ----
typedef struct {
    uint64_t data; // generic.data
    uint32_t serial; // generic.serial
    uint8_t btn; // generic.btn
    uint16_t cnt; // generic.cnt
    uint64_t yek;
    uint8_t crc;
} DecoderState;

#define POISON_SERIAL 0xDEADBEEFU
#define POISON_BTN    0xAAU
#define POISON_CNT    0xBEEFU
#define POISON_YEK    0xCAFEBABECAFEBABEULL
#define POISON_CRC    0x55U

static void poison(DecoderState* s) {
    // Simuliert den malloc()-Speicherinhalt vor dem Laden (kia_protocol_decoder_v5_alloc()
    // nutzt malloc(), kein calloc()) -- absichtlich NICHT 0, um zu beweisen, dass ALT
    // wirklich unveraendert laesst statt zufaellig zu nullen.
    s->serial = POISON_SERIAL;
    s->btn = POISON_BTN;
    s->cnt = POISON_CNT;
    s->yek = POISON_YEK;
    s->crc = POISON_CRC;
}

// Simuliert den in der Datei vorhandenen "CRC"-Wert (von serialize() geschrieben).
static const uint32_t FILE_CRC_FIELD = 3;

// ALT: heutiges kia_protocol_decoder_v5_deserialize() -- setzt nur generic.data (durch
// subghz_block_generic_deserialize_check_count_bit(), hier vereinfacht auf die reine
// Datenuebernahme reduziert, da diese Stock-Firmware-Funktion nur "Key"->data und
// "Bit"->data_count_bit fuellt), laesst yek/crc/serial/btn/cnt unangetastet.
static void deserialize_old(DecoderState* s, uint64_t file_data) {
    s->data = file_data;
    // yek/crc/serial/btn/cnt: keine Zeile in kia_protocol_decoder_v5_deserialize() ruehrt
    // sie an.
}

// NEU: gleiche Rueckrechnung wie KiaV5DecoderStepData in
// kia_protocol_decoder_v5_feed()/mixer_decode(), plus Lesen des bereits von serialize()
// geschriebenen "CRC"-Feldes.
static void deserialize_new(DecoderState* s, uint64_t file_data, uint32_t file_crc_field) {
    s->data = file_data;

    s->yek = 0;
    for(int i = 0; i < 8; i++) {
        uint8_t byte = (uint8_t)((s->data >> (i * 8)) & 0xFF);
        s->yek |= ((uint64_t)pp_reverse_bits8(byte) << ((7 - i) * 8));
    }
    s->serial = (uint32_t)((s->yek >> 32) & 0x0FFFFFFF);
    s->btn = (uint8_t)((s->yek >> 60) & 0x0F);
    s->cnt = mixer_decode((uint32_t)(s->yek & 0xFFFFFFFF));
    s->crc = (uint8_t)file_crc_field;
}

// Referenz: exakt dieselbe Rueckrechnung wie live in KiaV5DecoderStepData
// (kia_protocol_decoder_v5_feed()) direkt nach dem Manchester-Decode passieren wuerde, aus
// demselben generic.data heraus -- unabhaengiger Nachbau, um zu zeigen, dass NEU denselben
// Wert liefert wie ein echter Live-Empfang desselben Frames.
static void expected_from_live_decode(uint64_t data, uint32_t* serial, uint8_t* btn, uint16_t* cnt) {
    uint64_t yek = 0;
    for(int i = 0; i < 8; i++) {
        uint8_t byte = (uint8_t)((data >> (i * 8)) & 0xFF);
        uint8_t reversed = 0;
        for(int b = 0; b < 8; b++) {
            if(byte & (1 << b)) reversed |= (1 << (7 - b));
        }
        yek |= ((uint64_t)reversed << ((7 - i) * 8));
    }
    *serial = (uint32_t)((yek >> 32) & 0x0FFFFFFF);
    *btn = (uint8_t)((yek >> 60) & 0x0F);
    *cnt = mixer_decode((uint32_t)(yek & 0xFFFFFFFF));
}

typedef struct {
    const char* label;
    uint64_t data;
} TestCase;

int main(void) {
    TestCase cases[] = {
        {"typisch",        0x1A2B3C4D5E6F7081ULL},
        {"alle Nullen",    0x0000000000000000ULL},
        {"alle Einsen",    0xFFFFFFFFFFFFFFFFULL},
        {"alternierend",   0xA5A5A5A5A5A5A5A5ULL},
    };
    const size_t n = sizeof(cases) / sizeof(cases[0]);

    printf("== ALT (heutiges deserialize()) liefert immer den vergifteten Vor-Zustand ==\n");
    for(size_t i = 0; i < n; i++) {
        DecoderState s;
        poison(&s);
        deserialize_old(&s, cases[i].data);

        assert(s.serial == POISON_SERIAL);
        assert(s.btn == POISON_BTN);
        assert(s.cnt == POISON_CNT);
        assert(s.yek == POISON_YEK);
        assert(s.crc == POISON_CRC);
        printf(
            "  [%-12s] data=%016llX -> Sn=%08X Btn=%02X Cnt=%04X Yek=%016llX CRC=%02X "
            "(unveraendert vom Vor-Zustand, unabhaengig von data)\n",
            cases[i].label,
            (unsigned long long)cases[i].data,
            s.serial,
            s.btn,
            s.cnt,
            (unsigned long long)s.yek,
            s.crc);
    }

    printf("\n== NEU liefert exakten Round-Trip zum Live-Decode-Ergebnis desselben Frames ==\n");
    int all_ok = 1;
    for(size_t i = 0; i < n; i++) {
        DecoderState s;
        poison(&s);
        deserialize_new(&s, cases[i].data, FILE_CRC_FIELD);

        uint32_t exp_serial;
        uint8_t exp_btn;
        uint16_t exp_cnt;
        expected_from_live_decode(cases[i].data, &exp_serial, &exp_btn, &exp_cnt);

        int ok = (s.serial == exp_serial) && (s.btn == exp_btn) && (s.cnt == exp_cnt) &&
                 (s.crc == (uint8_t)FILE_CRC_FIELD);
        all_ok &= ok;

        printf(
            "  [%-12s] data=%016llX -> Sn=%08X Btn=%02X Cnt=%04X CRC=%02X  %s\n",
            cases[i].label,
            (unsigned long long)cases[i].data,
            s.serial,
            s.btn,
            s.cnt,
            s.crc,
            ok ? "OK (== Live-Decode-Ergebnis)" : "FEHLER");

        assert(s.serial == exp_serial);
        assert(s.btn == exp_btn);
        assert(s.cnt == exp_cnt);
        assert(s.crc == (uint8_t)FILE_CRC_FIELD);
    }

    printf("\n%s\n", all_ok ? "Alle Faelle OK." : "FEHLGESCHLAGEN.");
    return all_ok ? 0 : 1;
}
