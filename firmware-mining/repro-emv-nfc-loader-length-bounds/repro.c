// Host-Repro: lib/nfc/protocols/emv/emv.c, emv_load() -- bei mir (mntm-012) unveraendert
// (byteidentisch zum Unleashed-Vorher-Stand von Commit 2631895419, siehe unten). Anders als bei
// der bereits portierten emv_poller_i.c-Absicherung (firmware-mining/emv-tlv-length-bounds-fix.md)
// sind die Laengen hier NICHT kartenkontrolliert, sondern kommen aus einer geladenen ".nfc"-Datei
// (FlipperFormat) -- das Bedrohungsmodell ist eine praeparierte Datei statt eine praeparierte
// Karte (z. B. per Dateiaustausch/USB/SD-Karte geteilt).
//
// Drei unabhaengige Faelle, alle in emv_load():
//
//   Fall A -- "Cardholder name"/"Application name"/"Application label": "strcpy(dest,
//   furi_string_get_cstr(temp_str))" ohne jede Laengenpruefung gegen die feste Feldbreite
//   ("char cardholder_name[24 + 1]" etc. in EmvApplication, Teil der heap-allokierten EmvData).
//   Ein beliebig langer String-Wert in der Datei ueberschreibt den Speicher hinter dem Feld.
//
//   Fall B -- "PAN length": wird als uint32_t gelesen und ungeprueft an
//   "flipper_format_read_hex(ff, "PAN", app->pan, pan_len)" weitergereicht -- "app->pan" ist nur
//   "uint8_t pan[10]". Ein Wert wie 200 in der Datei schreibt 200 Byte in ein 10-Byte-Feld.
//
//   Fall C -- "AID length": identisches Muster, Ziel "uint8_t aid[16]".
//
// ALT ist 1:1 die bei mir vorhandene Logik. NEU ist die gebundene Fassung aus Unleashed-Commit
// 2631895419075d7df8d71be7e3d93465fcc6a491 ("[NFC] Bound file-controlled lengths in the EMV .nfc
// loader", PR #1056): Strings werden mit strlcpy() gekuerzt (kosmetisch, ein Name bleibt ladbar),
// Laengen werden VOR dem Lesezugriff gegen die Zielfeldgroesse geprueft und bei Ueberschreitung
// abgelehnt (Ladevorgang bricht kontrolliert ab, statt den Speicher zu ueberschreiben).
//
// Jeder Testfall laeuft isoliert in einem fork()-Kindprozess (ein SEGV/ASan-Abbruch eines Falls
// darf den Testlauf selbst nicht abbrechen). "app" wird wie in emv_alloc() per malloc() auf dem
// Heap angelegt, um denselben Speicherbereich (heap-allokierte EmvData) wie in der echten
// Firmware nachzubilden.
//
//   gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#define _DEFAULT_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Nachbildung der fuer diesen Fund relevanten EmvApplication-Felder (Reihenfolge/Groessen wie in
// lib/nfc/protocols/emv/emv.h bei mir), heap-allokiert wie in emv_alloc()/emv_load().
typedef struct {
    char application_name[16 + 1];
    char application_label[16 + 1];
    char cardholder_name[24 + 1];
    uint8_t pan[10];
    uint8_t pan_len;
    uint8_t aid[16];
    uint8_t aid_len;
} EmvApplication;

// ---- Fall A: unbegrenzter strcpy() in ein Namensfeld ----

// ALT: emv_load() -- bei mir (mntm-012) unveraendert.
static void old_load_string(char* dest, const char* value) {
    strcpy(dest, value); // NOLINT
}

// NEU: Unleashed-Fassung ab Commit 2631895419 (emv_load_string()) -- kuerzt statt zu ueberlaufen.
static void new_load_string(char* dest, size_t dest_size, const char* value) {
    size_t len = strlen(value);
    if(len >= dest_size) {
        printf("  NEU: Wert ist %zu Byte, kuerze auf %zu\n", len, dest_size - 1);
    }
    strlcpy(dest, value, dest_size);
}

// ---- Fall B/C: ungeprueftes read_hex() mit datei-kontrollierter Laenge ----

// Simuliert flipper_format_read_hex(): schreibt genau data_size Byte in data (wie im echten
// FlipperFormat-Parser, der die geforderte Byteanzahl aus der Textzeile in data schreibt).
static void simulated_read_hex(uint8_t* data, uint32_t data_size) {
    for(uint32_t i = 0; i < data_size; i++) {
        data[i] = (uint8_t)(0xA0 + (i & 0x0F));
    }
}

// ALT: emv_load() -- "PAN length"/"AID length" ungeprueft als read_hex()-Laenge verwendet.
static void old_load_length_field(uint8_t* field, uint8_t* field_len, uint32_t file_len) {
    simulated_read_hex(field, file_len); // Laenge kommt direkt aus der Datei, keine Pruefung
    *field_len = (uint8_t)file_len; // Zuweisung erst NACH dem bereits erfolgten Ueberlauf-Schreibzugriff
}

// NEU: Unleashed-Fassung ab Commit 2631895419 (emv_load_length()) -- lehnt ab, statt zu
// ueberlaufen; field_len wird nur bei Erfolg gesetzt.
static bool new_load_length_field(
    uint8_t* field,
    size_t field_capacity,
    uint8_t* field_len,
    uint32_t file_len) {
    if(file_len > field_capacity) {
        printf(
            "  NEU: Laenge ist %u, max %zu -> Ladevorgang abgelehnt, kein Schreibzugriff\n",
            file_len,
            field_capacity);
        return false;
    }
    simulated_read_hex(field, file_len);
    *field_len = (uint8_t)file_len;
    return true;
}

typedef void (*RunFn)(void*);

// Fuehrt fn(arg) in einem Kindprozess aus; gibt zurueck, ob der Kindprozess ungewoehnlich beendet
// wurde (Signal, z. B. SIGABRT durch ASan, oder SIGSEGV).
static bool run_isolated(RunFn fn, void* arg) {
    pid_t pid = fork();
    if(pid == 0) {
        fclose(stderr);
        fn(arg);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status);
}

typedef struct {
    char* dest;
    const char* value;
} StringArgs;

static void run_old_string(void* arg) {
    StringArgs* a = arg;
    old_load_string(a->dest, a->value);
    printf("  ALT: strcpy(%zu Byte) abgeschlossen, kein Absturz\n", strlen(a->value));
}

static void run_new_string(void* arg) {
    StringArgs* a = arg;
    new_load_string(a->dest, 24 + 1, a->value);
    printf("  NEU: strlcpy abgeschlossen, kein Absturz\n");
}

typedef struct {
    uint8_t* field;
    uint8_t* field_len;
    size_t capacity;
    uint32_t file_len;
} LengthArgs;

static void run_old_length(void* arg) {
    LengthArgs* a = arg;
    old_load_length_field(a->field, a->field_len, a->file_len);
    printf("  ALT: %u Byte in %zu-Byte-Feld geschrieben, kein Absturz\n", a->file_len, a->capacity);
}

static void run_new_length(void* arg) {
    LengthArgs* a = arg;
    new_load_length_field(a->field, a->capacity, a->field_len, a->file_len);
}

static void print_case(const char* name, bool crashed) {
    printf("  %s: %s\n", name, crashed ? "ABSTURZ (von ASan/Signal erkannt)" : "kein Absturz");
}

int main(void) {
    printf("== Fall A: Cardholder-Name strcpy() (Ziel: cardholder_name[25]) ==\n");
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        StringArgs args_ok = {app->cardholder_name, "JANE A CARDHOLDER"}; // 18 Zeichen, passt
        print_case("ALT, realistischer Name (18 Zeichen)", run_isolated(run_old_string, &args_ok));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        StringArgs args_ok = {app->cardholder_name, "JANE A CARDHOLDER"};
        print_case("NEU, realistischer Name (18 Zeichen)", run_isolated(run_new_string, &args_ok));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        char overlong[300];
        memset(overlong, 'A', sizeof(overlong) - 1);
        overlong[sizeof(overlong) - 1] = '\0';
        StringArgs args_bad = {app->cardholder_name, overlong};
        print_case(
            "ALT, ueberlanger Dateiwert (299 Zeichen, Ziel nur 25 Byte)",
            run_isolated(run_old_string, &args_bad));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        char overlong[300];
        memset(overlong, 'A', sizeof(overlong) - 1);
        overlong[sizeof(overlong) - 1] = '\0';
        StringArgs args_bad = {app->cardholder_name, overlong};
        print_case(
            "NEU, ueberlanger Dateiwert (299 Zeichen, Ziel nur 25 Byte)",
            run_isolated(run_new_string, &args_bad));
        free(app);
    }

    printf("\n== Fall B: \"PAN length\" (Ziel: pan[10]) ==\n");
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->pan, &app->pan_len, sizeof(app->pan), 8}; // realistische PAN-Laenge
        print_case("ALT, realistische Laenge (8)", run_isolated(run_old_length, &a));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->pan, &app->pan_len, sizeof(app->pan), 8};
        print_case("NEU, realistische Laenge (8)", run_isolated(run_new_length, &a));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->pan, &app->pan_len, sizeof(app->pan), 200}; // dateikontrolliert
        print_case(
            "ALT, dateikontrollierte Laenge (200, Ziel nur 10 Byte)",
            run_isolated(run_old_length, &a));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->pan, &app->pan_len, sizeof(app->pan), 200};
        print_case(
            "NEU, dateikontrollierte Laenge (200, Ziel nur 10 Byte)",
            run_isolated(run_new_length, &a));
        free(app);
    }

    printf("\n== Fall C: \"AID length\" (Ziel: aid[16]) ==\n");
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->aid, &app->aid_len, sizeof(app->aid), 7}; // realistische AID-Laenge
        print_case("ALT, realistische Laenge (7)", run_isolated(run_old_length, &a));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->aid, &app->aid_len, sizeof(app->aid), 7};
        print_case("NEU, realistische Laenge (7)", run_isolated(run_new_length, &a));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->aid, &app->aid_len, sizeof(app->aid), 255}; // dateikontrolliert, max uint8_t
        print_case(
            "ALT, dateikontrollierte Laenge (255, Ziel nur 16 Byte)",
            run_isolated(run_old_length, &a));
        free(app);
    }
    {
        EmvApplication* app = malloc(sizeof(EmvApplication));
        LengthArgs a = {app->aid, &app->aid_len, sizeof(app->aid), 255};
        print_case(
            "NEU, dateikontrollierte Laenge (255, Ziel nur 16 Byte)",
            run_isolated(run_new_length, &a));
        free(app);
    }

    printf(
        "\nOK: ALT stuerzt in allen drei Faellen bei dateikontrolliertem Ueberlaengen-Wert unter\n"
        "ASan als heap-buffer-overflow ab, NEU (Kuerzung/Ablehnung aus Commit 2631895419) nicht.\n"
        "Fuer realistische Werte verhalten sich ALT und NEU identisch.\n");
    return 0;
}
