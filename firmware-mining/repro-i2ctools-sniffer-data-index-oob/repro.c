// Host-Repro: NaejEL/flipperzero-i2ctools, i2csniffer.c::SCLcallback()
//
// data_index (uint8_t, Feld von i2cFrame) wird im ACK-Zweig jedes vollstaendigen Bytes
// unbedingt hochgezaehlt, ohne Pruefung gegen MAX_MESSAGE_SIZE (128) -- anders als
// frame_index (SDAcallback, Start-Bedingung), der vor jedem Schreiben gegen MAX_RECORDS
// geprueft wird. Eine einzelne I2C-Transaktion mit mehr als 128 uebertragenen Bytes ohne
// zwischenzeitliche Stop-/Start-Bedingung laesst data_index auf 128 (und weiter) wachsen
// und indiziert dann data[]/ack[] (je uint8_t[128]/bool[128]) ausserhalb ihrer deklarierten
// Grenzen.
//
// OLD/NEW unten sind 1:1-Transkriptionen von SCLcallback() (GPIO-Lesewert durch den
// uebergebenen sda_bit-Parameter ersetzt, state/frame_index/bit_index/data_index-Handling
// unveraendert). NEW ergaenzt nur die vorgeschlagene Grenzpruefung.
//
// Build/Test:
//   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_o0 && ./repro_o0
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_o1 && ./repro_o1
//
// -fno-sanitize-recover=undefined macht den UBSan-Array-Bounds-Fund fatal (Standard ist
// nur eine Meldung ohne Abbruch) -- fuer den OLD-Fund-Fall laeuft der Treiber deshalb in
// einem eigenen fork()-Kindprozess, damit der Abbruch nur dieses eine Szenario beendet.

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 128
#define MAX_RECORDS 128

typedef enum { I2C_BUS_FREE, I2C_BUS_STARTED } i2cBusStates;

typedef struct {
    uint8_t data[MAX_MESSAGE_SIZE];
    bool ack[MAX_MESSAGE_SIZE];
    uint8_t bit_index;
    uint8_t data_index;
} i2cFrame;

typedef struct {
    bool started;
    bool first;
    i2cBusStates state;
    i2cFrame frames[MAX_RECORDS];
    uint8_t frame_index;
    uint8_t menu_index;
    uint8_t row_index;
} i2cSniffer;

static i2cSniffer* sniffer_alloc_at(uint8_t frame_index) {
    i2cSniffer* s = malloc(sizeof(i2cSniffer));
    memset(s, 0, sizeof(*s));
    s->state = I2C_BUS_STARTED;
    s->frame_index = frame_index;
    return s;
}

// ALT: 1:1 aus i2csniffer.c::SCLcallback() (GPIO-Lesewert -> sda_bit-Parameter)
static void SCLcallback_old(i2cSniffer* s, bool sda_bit) {
    if(s->state == I2C_BUS_FREE) return;
    uint8_t frame = s->frame_index;
    uint8_t bit = s->frames[frame].bit_index;
    uint8_t data_idx = s->frames[frame].data_index;
    if(bit < 8) {
        s->frames[frame].data[data_idx] <<= 1;
        s->frames[frame].data[data_idx] |= (int)sda_bit;
        s->frames[frame].bit_index++;
    } else {
        s->frames[frame].ack[data_idx] = !sda_bit;
        s->frames[frame].data_index++;
        s->frames[frame].bit_index = 0;
    }
}

// NEU: zusaetzliche Grenzpruefung vor jedem Zugriff auf data[]/ack[] des aktuellen Frames
static void SCLcallback_new(i2cSniffer* s, bool sda_bit) {
    if(s->state == I2C_BUS_FREE) return;
    uint8_t frame = s->frame_index;
    uint8_t bit = s->frames[frame].bit_index;
    uint8_t data_idx = s->frames[frame].data_index;
    if(data_idx >= MAX_MESSAGE_SIZE) return;
    if(bit < 8) {
        s->frames[frame].data[data_idx] <<= 1;
        s->frames[frame].data[data_idx] |= (int)sda_bit;
        s->frames[frame].bit_index++;
    } else {
        s->frames[frame].ack[data_idx] = !sda_bit;
        s->frames[frame].data_index++;
        s->frames[frame].bit_index = 0;
    }
}

typedef void (*scl_cb_t)(i2cSniffer*, bool);

// Treibt n_bytes vollstaendige I2C-Bytes (8 Datenbit + 1 ACK-Bit, je ein SCL-Puls) durch
// den Callback -- deterministisches, aber beliebiges Bitmuster je Byte, damit data[] nach
// dem Lauf exakt dem erwarteten Byte-Wert entspricht.
static void drive_bytes(i2cSniffer* s, scl_cb_t cb, int n_bytes) {
    for(int n = 0; n < n_bytes; n++) {
        uint8_t value = (uint8_t)(n * 7 + 3);
        for(int bitpos = 7; bitpos >= 0; bitpos--) {
            cb(s, (value >> bitpos) & 1);
        }
        cb(s, (n % 2) == 0); // ACK-Puls
    }
}

static int checks_total = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                               \
    do {                                                               \
        checks_total++;                                                \
        if(!(cond)) {                                                  \
            checks_failed++;                                           \
            printf("  FAIL: %s\n", msg);                               \
        }                                                              \
    } while(0)

static void scenario_regression_20_bytes(void) {
    printf("Szenario 1: Regression -- 20 Byte auf Frame 3 (weit unter MAX_MESSAGE_SIZE)\n");
    i2cSniffer* old_s = sniffer_alloc_at(3);
    i2cSniffer* new_s = sniffer_alloc_at(3);
    drive_bytes(old_s, SCLcallback_old, 20);
    drive_bytes(new_s, SCLcallback_new, 20);
    CHECK(memcmp(&old_s->frames[3], &new_s->frames[3], sizeof(i2cFrame)) == 0,
          "ALT/NEU-Frame weicht bei 20 Byte ab (Regression)");
    CHECK(old_s->frames[3].data_index == 20, "ALT data_index nach 20 Byte != 20");
    CHECK(old_s->frames[3].data[19] == (uint8_t)(19 * 7 + 3), "ALT letztes Byte falsch dekodiert");
    free(old_s);
    free(new_s);
}

static void scenario_boundary_128_bytes(void) {
    printf("Szenario 2: Grenzfall -- exakt 128 Byte (volle Kapazitaet, noch kein Ueberlauf)\n");
    i2cSniffer* old_s = sniffer_alloc_at(7);
    i2cSniffer* new_s = sniffer_alloc_at(7);
    drive_bytes(old_s, SCLcallback_old, MAX_MESSAGE_SIZE);
    drive_bytes(new_s, SCLcallback_new, MAX_MESSAGE_SIZE);
    CHECK(memcmp(&old_s->frames[7], &new_s->frames[7], sizeof(i2cFrame)) == 0,
          "ALT/NEU-Frame weicht bei exakt 128 Byte ab (Regression)");
    CHECK(old_s->frames[7].data_index == MAX_MESSAGE_SIZE, "ALT data_index nach 128 Byte != 128");
    CHECK(old_s->frames[7].data[127] == (uint8_t)(127 * 7 + 3), "ALT letztes Byte (Index 127) falsch");
    free(old_s);
    free(new_s);
}

static void scenario_fund_old_crashes_child(void) {
    printf("Szenario 3 (Fund): ALT, 129. Byte auf dem letzten Frame-Slot (129 > MAX_MESSAGE_SIZE) -- eigener Kindprozess\n");
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        i2cSniffer* s = sniffer_alloc_at(MAX_RECORDS - 1);
        drive_bytes(s, SCLcallback_old, MAX_MESSAGE_SIZE + 1); // 129. Byte schreibt data[128]/ack[128] -> OOB
        free(s);
        _exit(0); // wird bei funktionierendem Fund nicht erreicht (UBSan bricht vorher ab)
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool crashed = !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("  Kindprozess-Status: %s\n", crashed ? "ABGEBROCHEN (erwartet)" : "normal beendet (unerwartet)");
    CHECK(crashed, "ALT 129. Byte auf letztem Frame-Slot bricht NICHT ab (Fund nicht reproduziert)");
}

static void scenario_fix_new_survives(void) {
    printf("Szenario 4 (Fix): NEU, 129. Byte plus 300 weitere Taktpulse auf dem letzten Frame-Slot\n");
    i2cSniffer* s = sniffer_alloc_at(MAX_RECORDS - 1);
    drive_bytes(s, SCLcallback_new, MAX_MESSAGE_SIZE + 1);
    // weitere 300 SCL-Pulse treiben (entspricht ~33 weiteren Byte-Versuchen) -- NEU darf
    // dabei data_index nicht ueber MAX_MESSAGE_SIZE hinaus veraendern und nichts mehr in
    // data[]/ack[] schreiben.
    for(int i = 0; i < 300; i++) {
        SCLcallback_new(s, (i % 2) == 0);
    }
    CHECK(s->frames[MAX_RECORDS - 1].data_index == MAX_MESSAGE_SIZE,
          "NEU data_index wurde trotz Grenzpruefung ueber MAX_MESSAGE_SIZE hinaus veraendert");
    CHECK(s->frames[MAX_RECORDS - 1].data[127] == (uint8_t)(127 * 7 + 3),
          "NEU hat das letzte gueltige Byte (Index 127) nachtraeglich veraendert");
    printf("  NEU: data_index nach Ueberlaufversuch = %u (eingefroren bei %d), kein Crash\n",
           s->frames[MAX_RECORDS - 1].data_index, MAX_MESSAGE_SIZE);
    free(s);
}

static void scenario_equivalence_sweep(void) {
    printf("Szenario 5: Aequivalenztest -- Transaktionslaenge 1..128 Byte, ALT vs. NEU\n");
    bool all_equal = true;
    for(int n = 1; n <= MAX_MESSAGE_SIZE; n++) {
        i2cSniffer* old_s = sniffer_alloc_at(42);
        i2cSniffer* new_s = sniffer_alloc_at(42);
        drive_bytes(old_s, SCLcallback_old, n);
        drive_bytes(new_s, SCLcallback_new, n);
        if(memcmp(&old_s->frames[42], &new_s->frames[42], sizeof(i2cFrame)) != 0) {
            all_equal = false;
            printf("  Abweichung bei n=%d Byte\n", n);
        }
        free(old_s);
        free(new_s);
    }
    CHECK(all_equal, "ALT/NEU weichen fuer mindestens eine Laenge im gueltigen Bereich (1..128) ab");
}

int main(void) {
    scenario_regression_20_bytes();
    scenario_boundary_128_bytes();
    scenario_fund_old_crashes_child();
    scenario_fix_new_survives();
    scenario_equivalence_sweep();

    printf("\n%d/%d Einzelpruefungen bestanden\n", checks_total - checks_failed, checks_total);
    if(checks_failed > 0) {
        printf("ALLE CHECKS NICHT BESTANDEN (%d Fehler)\n", checks_failed);
        return 1;
    }
    printf("ALLE CHECKS BESTANDEN\n");
    return 0;
}
