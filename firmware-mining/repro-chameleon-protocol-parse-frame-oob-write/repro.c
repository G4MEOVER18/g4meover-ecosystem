// Host-Repro: Chameleon (RogueMaster-Bundle), lib/chameleon_protocol/chameleon_protocol.c ::
// chameleon_protocol_parse_frame()
//
// Die LEN-Feldlaenge einer eingehenden Chameleon-Ultra-Protokollantwort (ueber USB-CDC/UART
// oder BLE vom gekoppelten Geraet empfangen, siehe lib/uart_handler/uart_handler.c,
// lib/ble_handler/ble_handler.c) wird 1:1 aus den ersten acht Frame-Bytes gelesen (uint16_t,
// 0..65535) und direkt als memcpy()-Laenge in den von chameleon_app_rx_callback()
// (chameleon_app.c) uebergebenen Zielpuffer verwendet. Dieser Zielpuffer ist in jedem
// bekannten Aufrufer exakt `uint8_t data_buffer[512]` (== CHAMELEON_MAX_DATA_LEN, siehe
// chameleon_app_i.h) gross. Anders als chameleon_protocol_build_frame() (Sendepfad), das
// data_len bereits explizit gegen CHAMELEON_MAX_DATA_LEN prueft, fehlt diese Pruefung im
// Empfangspfad komplett -- ein Peer, der ueber den gekoppelten UART/BLE-Link antwortet (das
// eigene Chameleon-Ultra-Geraet, aber ebenso ein MITM/kompromittiertes Zweitgeraet auf
// derselben Verbindung), kann mit einer korrekt gegengecheckten LRC2 und einer LEN > 512 einen
// Heap-Buffer-Overflow im Flipper-seitigen Empfangspuffer ausloesen, einfach durch Senden
// eines laengeren, aber sonst gueltigen Frames.
//
// OLD/NEW unten sind 1:1-Transkriptionen der SOF/LRC1/CMD/STATUS/LEN/LRC2-Extraktion und des
// DATA-memcpy() aus chameleon_protocol_parse_frame() (LRC3-Verifikation nach dem memcpy()
// unveraendert uebernommen, da sie den Fund selbst nicht beeinflusst). NEU ergaenzt nur die
// vorgeschlagene Grenzpruefung direkt nach dem Extrahieren von LEN, vor jeder Nutzung als
// Kopierlaenge.
//
// Build/Test:
//   gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro_o0 && ./repro_o0
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro_o1 && ./repro_o1
//
// Der Fund-Fall (ALT, LEN > 512) laeuft in einem eigenen fork()-Kindprozess, da ASan einen
// Heap-Buffer-Overflow per Default fatal abbricht (kein Rueckkehrpfad in den Elternprozess).

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHAMELEON_SOF 0x11
#define CHAMELEON_LRC1 0xEF
#define CHAMELEON_MAX_DATA_LEN 512
#define CHAMELEON_FRAME_OVERHEAD 10

static uint8_t calculate_lrc(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum + 1);
}

// Baut einen vollstaendig gueltigen Frame (SOF/LRC1/CMD/STATUS/LEN/LRC2/DATA/LRC3) direkt mit
// der gewuenschten (potenziell zu grossen) data_len -- entspricht einer Antwort, die ein Peer
// auf dem UART/BLE-Link tatsaechlich senden kann, unabhaengig von CHAMELEON_MAX_DATA_LEN.
static uint8_t* build_frame(uint16_t cmd, uint16_t status, uint16_t data_len, size_t* out_len) {
    size_t total = CHAMELEON_FRAME_OVERHEAD + data_len;
    uint8_t* frame = malloc(total);
    size_t idx = 0;
    frame[idx++] = CHAMELEON_SOF;
    frame[idx++] = CHAMELEON_LRC1;
    frame[idx++] = (cmd >> 8) & 0xFF;
    frame[idx++] = cmd & 0xFF;
    frame[idx++] = (status >> 8) & 0xFF;
    frame[idx++] = status & 0xFF;
    frame[idx++] = (data_len >> 8) & 0xFF;
    frame[idx++] = data_len & 0xFF;
    frame[idx] = calculate_lrc(&frame[2], 6);
    idx++;
    for(uint16_t i = 0; i < data_len; i++) {
        frame[idx++] = (uint8_t)(i * 3 + 7); // deterministisches, aber beliebiges Fuellmuster
    }
    frame[idx] = calculate_lrc(&frame[9], data_len);
    idx++;
    *out_len = idx;
    return frame;
}

// ALT: 1:1 aus chameleon_protocol_parse_frame() (SOF/LRC1/CMD/STATUS/LEN/LRC2-Extraktion plus
// DATA-memcpy(); LRC3-Verifikation nach dem memcpy() ausgelassen, beeinflusst den Fund nicht).
static bool parse_frame_old(
    const uint8_t* frame_data,
    size_t frame_len,
    uint16_t* cmd,
    uint16_t* status,
    uint8_t* data,
    uint16_t* data_len) {
    if(frame_len < CHAMELEON_FRAME_OVERHEAD) return false;
    if(frame_data[0] != CHAMELEON_SOF || frame_data[1] != CHAMELEON_LRC1) return false;

    *cmd = ((uint16_t)frame_data[2] << 8) | frame_data[3];
    *status = ((uint16_t)frame_data[4] << 8) | frame_data[5];
    *data_len = ((uint16_t)frame_data[6] << 8) | frame_data[7];

    uint8_t expected_lrc2 = calculate_lrc(&frame_data[2], 6);
    if(frame_data[8] != expected_lrc2) return false;

    size_t expected_len = CHAMELEON_FRAME_OVERHEAD + *data_len;
    if(frame_len != expected_len) return false;

    if(*data_len > 0) {
        memcpy(data, &frame_data[9], *data_len); // <- ungeprueft gegen die Zielpuffergroesse
    }
    return true;
}

// NEU: zusaetzliche Grenzpruefung direkt nach dem Extrahieren von LEN, vor jeder Nutzung als
// Kopierlaenge (spiegelt die bereits im Sendepfad vorhandene Pruefung in
// chameleon_protocol_build_frame()).
static bool parse_frame_new(
    const uint8_t* frame_data,
    size_t frame_len,
    uint16_t* cmd,
    uint16_t* status,
    uint8_t* data,
    uint16_t* data_len) {
    if(frame_len < CHAMELEON_FRAME_OVERHEAD) return false;
    if(frame_data[0] != CHAMELEON_SOF || frame_data[1] != CHAMELEON_LRC1) return false;

    *cmd = ((uint16_t)frame_data[2] << 8) | frame_data[3];
    *status = ((uint16_t)frame_data[4] << 8) | frame_data[5];
    *data_len = ((uint16_t)frame_data[6] << 8) | frame_data[7];

    if(*data_len > CHAMELEON_MAX_DATA_LEN) return false;

    uint8_t expected_lrc2 = calculate_lrc(&frame_data[2], 6);
    if(frame_data[8] != expected_lrc2) return false;

    size_t expected_len = CHAMELEON_FRAME_OVERHEAD + *data_len;
    if(frame_len != expected_len) return false;

    if(*data_len > 0) {
        memcpy(data, &frame_data[9], *data_len);
    }
    return true;
}

typedef bool (*parse_fn_t)(const uint8_t*, size_t, uint16_t*, uint16_t*, uint8_t*, uint16_t*);

static int checks_total = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                      \
    do {                                       \
        checks_total++;                        \
        if(!(cond)) {                           \
            checks_failed++;                     \
            printf("  FAIL: %s\n", msg);          \
        }                                           \
    } while(0)

static void scenario_regression_20_bytes(void) {
    printf("Szenario 1: Regression -- 20 Byte Nutzdaten (weit unter CHAMELEON_MAX_DATA_LEN)\n");
    size_t flen;
    uint8_t* frame = build_frame(2000, 0x0200, 20, &flen);
    uint8_t* dest_old = malloc(CHAMELEON_MAX_DATA_LEN);
    uint8_t* dest_new = malloc(CHAMELEON_MAX_DATA_LEN);
    uint16_t cmd_o, status_o, len_o, cmd_n, status_n, len_n;
    bool ok_o = parse_frame_old(frame, flen, &cmd_o, &status_o, dest_old, &len_o);
    bool ok_n = parse_frame_new(frame, flen, &cmd_n, &status_n, dest_new, &len_n);
    CHECK(ok_o && ok_n, "ALT/NEU lehnen eine gueltige 20-Byte-Antwort ab (Regression)");
    CHECK(len_o == 20 && len_n == 20, "ALT/NEU liefern falsche data_len fuer 20 Byte");
    CHECK(memcmp(dest_old, dest_new, 20) == 0, "ALT/NEU-Nutzdaten weichen bei 20 Byte ab");
    free(frame);
    free(dest_old);
    free(dest_new);
}

static void scenario_boundary_512_bytes(void) {
    printf("Szenario 2: Grenzfall -- exakt 512 Byte (volle Zielpuffergroesse, noch kein Ueberlauf)\n");
    size_t flen;
    uint8_t* frame = build_frame(2008, 0x0200, CHAMELEON_MAX_DATA_LEN, &flen);
    uint8_t* dest_old = malloc(CHAMELEON_MAX_DATA_LEN);
    uint8_t* dest_new = malloc(CHAMELEON_MAX_DATA_LEN);
    uint16_t cmd_o, status_o, len_o, cmd_n, status_n, len_n;
    bool ok_o = parse_frame_old(frame, flen, &cmd_o, &status_o, dest_old, &len_o);
    bool ok_n = parse_frame_new(frame, flen, &cmd_n, &status_n, dest_new, &len_n);
    CHECK(ok_o && ok_n, "ALT/NEU lehnen eine gueltige 512-Byte-Antwort ab (Regression)");
    CHECK(len_o == CHAMELEON_MAX_DATA_LEN && len_n == CHAMELEON_MAX_DATA_LEN,
          "ALT/NEU liefern falsche data_len fuer 512 Byte");
    CHECK(memcmp(dest_old, dest_new, CHAMELEON_MAX_DATA_LEN) == 0,
          "ALT/NEU-Nutzdaten weichen beim 512-Byte-Grenzfall ab");
    free(frame);
    free(dest_old);
    free(dest_new);
}

static void scenario_fund_old_overflows_child(uint16_t fund_len, const char* label) {
    printf(
        "Szenario (Fund): ALT, Antwort mit LEN = %u Byte (> %u) -- eigener Kindprozess [%s]\n",
        fund_len,
        CHAMELEON_MAX_DATA_LEN,
        label);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        size_t flen;
        uint8_t* frame = build_frame(2000, 0x0200, fund_len, &flen);
        uint8_t* dest = malloc(CHAMELEON_MAX_DATA_LEN); // exakte Groesse wie app->data_buffer
        uint16_t cmd, status, len;
        parse_frame_old(frame, flen, &cmd, &status, dest, &len); // memcpy() schreibt ueber dest hinaus
        free(frame);
        free(dest);
        _exit(0); // wird bei funktionierendem Fund nicht erreicht (ASan bricht vorher ab)
    }
    int st = 0;
    waitpid(pid, &st, 0);
    bool crashed = !(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    printf("  Kindprozess-Status: %s\n", crashed ? "ABGEBROCHEN (erwartet)" : "normal beendet (unerwartet)");
    CHECK(crashed, "ALT ueberlebt eine ueberlange Antwort unerwartet (Fund nicht reproduziert)");
}

static void scenario_fix_new_rejects(uint16_t fund_len) {
    printf("Szenario (Fix): NEU, dieselbe %u-Byte-Antwort wird abgelehnt statt kopiert\n", fund_len);
    size_t flen;
    uint8_t* frame = build_frame(2000, 0x0200, fund_len, &flen);
    uint8_t* dest = malloc(CHAMELEON_MAX_DATA_LEN);
    memset(dest, 0xAA, CHAMELEON_MAX_DATA_LEN); // Wachposten-Muster, darf unveraendert bleiben
    uint16_t cmd, status, len = 0;
    bool ok = parse_frame_new(frame, flen, &cmd, &status, dest, &len);
    CHECK(!ok, "NEU akzeptiert eine ueberlange Antwort statt sie abzulehnen");
    bool untouched = true;
    for(int i = 0; i < CHAMELEON_MAX_DATA_LEN; i++) {
        if(dest[i] != 0xAA) untouched = false;
    }
    CHECK(untouched, "NEU hat trotz Ablehnung in den Zielpuffer geschrieben");
    free(frame);
    free(dest);
}

static void scenario_equivalence_sweep(void) {
    printf("Szenario: Aequivalenztest -- data_len 0..512 Byte, ALT vs. NEU\n");
    bool all_equal = true;
    for(int n = 0; n <= CHAMELEON_MAX_DATA_LEN; n++) {
        size_t flen;
        uint8_t* frame = build_frame(2008, 0x0200, (uint16_t)n, &flen);
        uint8_t* dest_old = malloc(CHAMELEON_MAX_DATA_LEN);
        uint8_t* dest_new = malloc(CHAMELEON_MAX_DATA_LEN);
        uint16_t cmd_o, status_o, len_o, cmd_n, status_n, len_n;
        bool ok_o = parse_frame_old(frame, flen, &cmd_o, &status_o, dest_old, &len_o);
        bool ok_n = parse_frame_new(frame, flen, &cmd_n, &status_n, dest_new, &len_n);
        if(!ok_o || !ok_n || len_o != len_n || memcmp(dest_old, dest_new, n) != 0) {
            all_equal = false;
            printf("  Abweichung bei n=%d Byte\n", n);
        }
        free(frame);
        free(dest_old);
        free(dest_new);
    }
    CHECK(all_equal, "ALT/NEU weichen fuer mindestens eine Laenge im gueltigen Bereich (0..512) ab");
}

int main(void) {
    scenario_regression_20_bytes();
    scenario_boundary_512_bytes();
    scenario_fund_old_overflows_child(513, "minimal ueber der Grenze");
    scenario_fund_old_overflows_child(2000, "typische ueberlange Antwort");
    scenario_fund_old_overflows_child(65535, "maximal moegliche LEN (uint16_t)");
    scenario_fix_new_rejects(513);
    scenario_fix_new_rejects(65535);
    scenario_equivalence_sweep();

    printf("\n%d/%d Einzelpruefungen bestanden\n", checks_total - checks_failed, checks_total);
    if(checks_failed > 0) {
        printf("ALLE CHECKS NICHT BESTANDEN (%d Fehler)\n", checks_failed);
        return 1;
    }
    printf("ALLE CHECKS BESTANDEN\n");
    return 0;
}
