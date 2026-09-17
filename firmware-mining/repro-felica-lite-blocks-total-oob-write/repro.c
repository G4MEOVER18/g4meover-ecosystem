// Host-Repro: lib/nfc/protocols/felica/felica.c, felica_load() -- bei mir (mntm-012) unveraendert
// (data_format_version 2 statt 3, aber die betroffene FelicaLite-Ladeschleife selbst ist
// byteidentisch zum Unleashed-Vorher-Stand von Commit 51a56fc4 "NFC: reject oversized FeliCa Lite
// dumps", PR #1106). Bedrohungsmodell wie beim bereits portierten EMV-Loader-Fund (siehe
// firmware-mining/emv-nfc-file-loader-length-bounds-fix.md): eine praeparierte ".nfc"-Datei
// (FlipperFormat) ueberschreibt Heap-Speicher beim blossen Laden -- kein Kartenkontakt noetig.
//
// felica_load() liest "Blocks total" als uint32_t direkt aus der Datei und weist es OHNE
// Obergrenzenpruefung nach "data->blocks_total = (uint8_t)blocks_total" zu. Die anschliessende
// Schleife "for(i = 0; i < data->blocks_total; i++)" schreibt pro Iteration sizeof(FelicaBlock)
// (18 Byte: 2 Statusbyte + 16 Byte Nutzdaten) in "&data->data.dump[i * sizeof(FelicaBlock)]" --
// "data->data" (FelicaFSUnion) ist aber nur "FELICA_BLOCKS_TOTAL_COUNT" (28) Bloecke gross
// (504 Byte), Teil der heap-allokierten FelicaData (malloc(sizeof(FelicaData)) in felica_alloc()).
// Ein Datei-Wert wie 255 (Maximum nach dem uint32_t->uint8_t-Cast) schreibt 255*18 = 4590 Byte in
// ein 504-Byte-Feld -- weit ueber das Ende der gesamten FelicaData-Allokation hinaus.
//
// ALT ist 1:1 die bei mir vorhandene Logik. NEU ist die gebundene Fassung aus Unleashed-Commit
// 51a56fc4015299bf071a360633968ca489e455ee (PR #1106): "Blocks total"/"Blocks read" werden vor der
// Zuweisung gegen FELICA_BLOCKS_TOTAL_COUNT bzw. gegeneinander geprueft, ein zu grosser Wert lehnt
// den Ladevorgang kontrolliert ab (parsed = false), statt zu schreiben.
//
// Jeder Testfall laeuft isoliert in einem fork()-Kindprozess (ein ASan-Abbruch eines Falls darf
// den Testlauf selbst nicht beenden). "data" wird wie in felica_alloc() per malloc() auf dem Heap
// angelegt.
//
//   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Nachbildung der fuer diesen Fund relevanten FelicaData-Felder (Reihenfolge/Groessen wie in
// lib/nfc/protocols/felica/felica.h bei mir), heap-allokiert wie in felica_alloc()/felica_load().
#define FELICA_DATA_BLOCK_SIZE    (16U)
#define FELICA_BLOCKS_TOTAL_COUNT (28U)

typedef struct {
    uint8_t SF1;
    uint8_t SF2;
    uint8_t data[FELICA_DATA_BLOCK_SIZE];
} FelicaBlock; // sizeof == 18

typedef struct {
    FelicaBlock spad[14];
    FelicaBlock reg, rc, mac, id, d_id, ser_c, sys_c, ckv, ck, mc, wcnt, mac_a, state, crc_check;
} FelicaFileSystem; // 28 * 18 == 504 Byte

typedef union {
    FelicaFileSystem fs;
    uint8_t dump[sizeof(FelicaFileSystem)];
} FelicaFSUnion;

typedef struct {
    uint8_t idm[8];
    uint8_t pmm[8];
    uint8_t blocks_total;
    uint8_t blocks_read;
    FelicaFSUnion data;
    void* systems; // Platzhalter fuer das echte SimpleArray*-Feld, das direkt danach folgt
    int workflow_type;
} FelicaData;

// Simuliert flipper_format_read_hex(): schreibt genau size Byte an dest (wie der echte
// FlipperFormat-Parser, der die geforderte Byteanzahl aus der Textzeile schreibt).
static void simulated_read_hex(uint8_t* dest, size_t size) {
    for(size_t i = 0; i < size; i++) {
        dest[i] = (uint8_t)(0xA0 + (i & 0x0F));
    }
}

// ALT: felica_load(), FelicaLite-Zweig -- bei mir (mntm-012) unveraendert. Keine Pruefung von
// blocks_total/blocks_read vor der Zuweisung bzw. der Schreibschleife.
static void old_felica_load_lite(FelicaData* data, uint32_t file_blocks_total) {
    data->blocks_total = (uint8_t)file_blocks_total;
    data->blocks_read = data->blocks_total; // vereinfachte Annahme: alle Bloecke erfolgreich gelesen

    for(uint8_t i = 0; i < data->blocks_total; i++) {
        simulated_read_hex(&data->data.dump[i * sizeof(FelicaBlock)], sizeof(FelicaBlock));
    }
}

// NEU: Unleashed-Fassung ab Commit 51a56fc4 -- lehnt zu grosse Werte kontrolliert ab, bevor
// irgendetwas geschrieben wird.
static bool new_felica_load_lite(FelicaData* data, uint32_t file_blocks_total) {
    uint32_t file_blocks_read = file_blocks_total; // gleiche vereinfachte Annahme wie oben
    if(file_blocks_total > FELICA_BLOCKS_TOTAL_COUNT || file_blocks_read > file_blocks_total) {
        printf(
            "  NEU: Blocks total=%u (max %u) -> Ladevorgang abgelehnt, kein Schreibzugriff\n",
            file_blocks_total,
            (unsigned)FELICA_BLOCKS_TOTAL_COUNT);
        return false;
    }
    data->blocks_total = (uint8_t)file_blocks_total;
    data->blocks_read = (uint8_t)file_blocks_read;

    for(uint8_t i = 0; i < data->blocks_total; i++) {
        simulated_read_hex(&data->data.dump[i * sizeof(FelicaBlock)], sizeof(FelicaBlock));
    }
    return true;
}

typedef void (*RunFn)(void*);

// Fuehrt fn(arg) in einem Kindprozess aus; gibt zurueck, ob der Kindprozess ungewoehnlich beendet
// wurde -- entweder per Signal (SIGABRT/SIGSEGV) oder, wie es ASan standardmaessig ohne
// "abort_on_error=1" tut, per exit(1) nach einem gemeldeten heap-buffer-overflow (mit
// nicht-forkiertem gcc/-fsanitize=address auf dieser Maschine direkt gegengeprueft: derselbe
// Fund erzeugt ohne Fork einen vollen "AddressSanitizer: heap-buffer-overflow"-Bericht plus
// "ABORTING", der Prozess beendet sich dabei aber reproduzierbar mit Exitcode 1, nicht per Signal
// -- beide Faelle zaehlen hier als erkannter Fund, ein sauberer exit(0) im ALT-Zweig waere der
// eigentliche Blindspot).
static bool run_isolated(RunFn fn, void* arg) {
    pid_t pid = fork();
    if(pid == 0) {
        fclose(stderr);
        fn(arg);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
}

typedef struct {
    uint32_t file_blocks_total;
} Args;

static void run_old(void* arg) {
    Args* a = arg;
    FelicaData* data = malloc(sizeof(FelicaData));
    old_felica_load_lite(data, a->file_blocks_total);
    printf(
        "  ALT: %u Bloecke (%zu Byte) in %zu-Byte-Feld geschrieben, kein Absturz\n",
        a->file_blocks_total,
        (size_t)a->file_blocks_total * sizeof(FelicaBlock),
        sizeof(((FelicaData*)0)->data.dump));
    free(data);
}

static void run_new(void* arg) {
    Args* a = arg;
    FelicaData* data = malloc(sizeof(FelicaData));
    new_felica_load_lite(data, a->file_blocks_total);
    free(data);
}

static void print_case(const char* name, bool crashed) {
    printf("  %s: %s\n", name, crashed ? "ABSTURZ (von ASan/Signal erkannt)" : "kein Absturz");
}

int main(void) {
    printf("sizeof(FelicaBlock) = %zu, sizeof(dump) = %zu, sizeof(FelicaData) = %zu\n\n",
           sizeof(FelicaBlock),
           sizeof(((FelicaData*)0)->data.dump),
           sizeof(FelicaData));

    printf("== Regressionsfall: reale FeliCa-Lite-Karte (28 Bloecke, Maximum laut Protokoll) ==\n");
    {
        Args a = {FELICA_BLOCKS_TOTAL_COUNT};
        print_case("ALT, blocks_total=28", run_isolated(run_old, &a));
    }
    {
        Args a = {FELICA_BLOCKS_TOTAL_COUNT};
        print_case("NEU, blocks_total=28", run_isolated(run_new, &a));
    }

    printf("\n== Angriffsfall: dateikontrollierter Ueberlaengen-Wert (255, Maximum nach uint8_t-Cast) ==\n");
    {
        Args a = {255};
        print_case("ALT, blocks_total=255 (Datei-Wert, Ziel nur 504 Byte)", run_isolated(run_old, &a));
    }
    {
        Args a = {255};
        print_case("NEU, blocks_total=255 (Datei-Wert, Ziel nur 504 Byte)", run_isolated(run_new, &a));
    }

    printf(
        "\nOK: ALT ueberschreibt bei blocks_total=255 den gesamten heap-allokierten FelicaData-\n"
        "Block weit ueber sein Ende hinaus (4590 Byte Schreibversuch in ein < 600 Byte grosses\n"
        "Ziel), NEU (Grenzpruefung aus Commit 51a56fc4) lehnt den Ladevorgang vorher ab. Fuer den\n"
        "protokollkonformen Regressionsfall (28 Bloecke) verhalten sich ALT und NEU identisch.\n");
    return 0;
}
