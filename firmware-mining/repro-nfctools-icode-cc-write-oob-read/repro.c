/*
 * Host-Repro: ICODE/ISO15693-NDEF-Schreibpfad aus NFC Tools (wakdev),
 * core/nfc_tools_icode.c, iso15693_write_ndef_blocks() + nfcv_build_write_blk().
 *
 * ALT ist 1:1 aus dem echten Funktionskoerper transkribiert: `cc_expected` ist
 * ein lokales 4-Byte-Array (nur die vier Capability-Container-Bytes des NFC
 * Forum Type 5 Tag, E1 40 <mlen> 00), `nfcv_build_write_blk()` haengt davon
 * aber `block_size` Bytes an den TX-Frame an -- `block_size` ist die vom Tag
 * selbst waehrend des ISO15693-Pollings gemeldete physische Blockgroesse
 * (`iso15693_3_get_block_size()`), die laut ISO/IEC 15693-3 (VICC Memory
 * Size, 5-Bit-Feld, Wert 0..31 fuer Blockgroesse 1..32 Byte) 1..32 Byte
 * betragen kann. Der einzige Guard vor dem Aufruf
 * (`core/nfc_tools_icode.c:518`/`561`) ist `block_size < 4` -- ein Tag, der
 * eine Blockgroesse von z. B. 8 oder 32 Byte meldet (laut Standard zulaessig,
 * in der eigenen ISO15693-Recherche in
 * `firmware-mining/nfc-iso15693-read-multi-blocks-and-parser-overrun-fix.md`
 * bereits als variabler, aus `system_info` gelesener Wert bestaetigt, nicht
 * als fest auf 4 Byte limitiert), passiert diesen Guard unverandert.
 *
 * `nfcv_build_write_blk()` selbst ist hier auf die fuer den Fund relevante
 * Kopieroperation reduziert (Flag-/Kommando-/Blocknummer-Byte plus der
 * eigentliche `memcpy(..., data, block_size)`-Aufruf); der echte Code nutzt
 * dafuer `bit_buffer_append_bytes()`, das intern denselben
 * Laengenparameter unveraendert an memcpy weiterreicht.
 *
 * `cc_expected`/der Fix-Puffer werden hier wie im echten Aufrufkontext auf
 * dem Stack allokiert, damit ein Overread unter AddressSanitizer zuverlaessig
 * als stack-buffer-overflow sichtbar wird. Jedes Szenario laeuft in einem
 * eigenen fork()-Kindprozess, damit ein Crash in einem Szenario den Rest des
 * Testlaufs nicht abbricht.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- 1:1 aus nfc_tools_icode.c:389 (nfcv_build_write_blk), auf die
 * fundrelevante Kopie reduziert ---- */
static size_t nfcv_build_write_blk(uint8_t* tx, uint8_t blk, const uint8_t* data, uint8_t block_size) {
    size_t n = 0;
    tx[n++] = 0x02; /* ISO15693_3_REQ_FLAG_DATA_RATE_HI, Platzhalter */
    tx[n++] = 0x21; /* ISO15693_3_CMD_WRITE_BLOCK, Platzhalter */
    tx[n++] = blk;
    memcpy(tx + n, data, block_size); /* nfc_tools_icode.c:394 -- der eigentliche Fund */
    n += block_size;
    return n;
}

/* ---- ALT: 1:1 aus iso15693_write_ndef_blocks(), CC-Schreibzweig
 * (nfc_tools_icode.c:417-440) ---- */
static size_t build_cc_write_frame_old(uint8_t* tx, uint16_t block_count, uint8_t block_size) {
    uint8_t mlen = (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
    const uint8_t cc_expected[4] = {0xE1, 0x40, mlen, 0x00}; /* nfc_tools_icode.c:418 */
    return nfcv_build_write_blk(tx, 0, cc_expected, block_size); /* nfc_tools_icode.c:440 */
}

/* ---- NEU: CC-Puffer auf die tatsaechliche Blockgroesse dimensioniert
 * (32 Byte deckt den gesamten laut ISO15693-3 moeglichen Blockgroessenbereich
 * ab), Bytes ab Offset 4 bleiben wie von der NFC-Forum-Type-5-Tag-Spezifikation
 * fuer ungenutzte CC-Bytes vorgesehen auf 0x00 ---- */
static size_t build_cc_write_frame_new(uint8_t* tx, uint16_t block_count, uint8_t block_size) {
    uint8_t mlen = (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
    uint8_t cc_block[32] = {0};
    cc_block[0] = 0xE1;
    cc_block[1] = 0x40;
    cc_block[2] = mlen;
    cc_block[3] = 0x00;
    return nfcv_build_write_blk(tx, 0, cc_block, block_size);
}

typedef size_t (*build_fn)(uint8_t*, uint16_t, uint8_t);

static int run_case(
    const char* name, build_fn fn, uint16_t block_count, uint8_t block_size, int expect_crash) {
    pid_t pid = fork();
    if(pid == 0) {
        uint8_t tx[64];
        size_t n = fn(tx, block_count, block_size);
        fprintf(
            stderr,
            "[child] %s: frame_len=%zu cc_bytes=%02X %02X %02X %02X (no crash)\n",
            name,
            n,
            tx[3],
            tx[4],
            tx[5],
            tx[6]);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    int crashed = !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    const char* verdict = (crashed == expect_crash) ? "OK  " : "FAIL";
    printf(
        "[%s] %-26s block_count=%-4u block_size=%-3u -> %s\n",
        verdict,
        name,
        (unsigned)block_count,
        (unsigned)block_size,
        crashed ? "CRASHED (ASan stack overread)" : "survived");
    return (crashed == expect_crash) ? 0 : 1;
}

int main(void) {
    int fails = 0;

    printf("=== ALT (iso15693_write_ndef_blocks CC-Schreibzweig, verbatim) ===\n");
    /* Regression: Standard-ICODE-SLIX/SLI-Tag mit 4-Byte-Blockgroesse -- der
     * mit Abstand haeufigste Fall, vom Guard `block_size < 4` als einziger
     * zulaessiger Wert effektiv erwartet */
    fails += run_case("ALT regression (bs=4)", build_cc_write_frame_old, 64, 4, 0);
    /* Fund: Tag meldet eine laut ISO15693-3 gueltige, aber >4 Byte grosse
     * Blockgroesse (z. B. 8 Byte) -- passiert den `block_size < 4`-Guard */
    fails += run_case("ALT FUND (bs=8)", build_cc_write_frame_old, 32, 8, 1);
    /* Fund-Grenzfall: maximale laut Standard zulaessige Blockgroesse (32 Byte) */
    fails += run_case("ALT FUND (bs=32)", build_cc_write_frame_old, 16, 32, 1);

    printf("\n=== NEU (auf Blockgroesse dimensionierter CC-Puffer) ===\n");
    fails += run_case("NEU regression (bs=4)", build_cc_write_frame_new, 64, 4, 0);
    fails += run_case("NEU (bs=8)", build_cc_write_frame_new, 32, 8, 0);
    fails += run_case("NEU (bs=32)", build_cc_write_frame_new, 16, 32, 0);

    /* Byte-fuer-Byte-Aequivalenz der ersten vier CC-Bytes (E1 40 mlen 00) im
     * Regressionsfall bs=4, fuer alle praktisch relevanten block_count-Werte */
    printf("\n=== Aequivalenz ALT/NEU der CC-Bytes bei bs=4 (Regressionsfall) ===\n");
    for(uint16_t bc = 2; bc <= 256; bc++) {
        uint8_t tx_old[64], tx_new[64];
        size_t n_old = build_cc_write_frame_old(tx_old, bc, 4);
        size_t n_new = build_cc_write_frame_new(tx_new, bc, 4);
        if(n_old != n_new || memcmp(tx_old, tx_new, n_old) != 0) {
            printf("[FAIL] Diskrepanz bei block_count=%u (bs=4)\n", (unsigned)bc);
            fails++;
        }
    }
    printf("[OK  ] ALT und NEU identisch fuer alle block_count bei bs=4\n");

    printf("\n%s\n", fails == 0 ? "ALLE CHECKS BESTANDEN" : "FEHLER GEFUNDEN");
    return fails == 0 ? 0 : 1;
}
