// Host-Mini-Repro: SWD Probe (g3gg0/flipper-swd_probe) kopiert an zwei Stellen einen vom
// Dateibrowser gelieferten Pfad (FuriString, keine Laengenbegrenzung -- Flipper-SD-Pfade koennen
// durch verschachtelte Ordner deutlich laenger als 128 Byte werden) per strcpy() ungeprueft in
// ein festes char[MAX_FILE_LENGTH]-Feld (MAX_FILE_LENGTH = 128):
//   1. swd_execute_script() (swd_probe_app.c:2097ff) -> ScriptContext.filename, das Feld liegt in
//      einem malloc()-allozierten Struct (echter Heap-Buffer-Overflow).
//   2. Der "Down"-Taste-Handler in ModePageScan/ModePageFound (swd_probe_app.c:2929ff) ->
//      AppFSM.script_detected (ebenfalls Teil eines malloc()-allozierten Structs).
// Beide Stellen sind bei normaler Bedienung (kurzer Pfad) unauffaellig -- erst ein .swd-Skript
// tief verschachtelter Ordner macht den Pfad lang genug, um die 128 Byte zu ueberschreiten.
//
// Dieses Repro bildet beide strcpy()-Stellen 1:1 nach (gleiche Feldgroesse, gleiche
// malloc()-Platzierung, gleiche Kontrollstruktur). ALT enthaelt die ungeprueften strcpy()s, NEU
// (Compile mit -DFIXED) hat die im Port-Vorschlag beschriebene Laengenpruefung davor -- exakt wie
// im Patch (firmware-mining/swd-probe-app-port/swd_probe_app_c.patch).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_LENGTH 128

typedef struct sScriptContext {
    char filename[MAX_FILE_LENGTH];
} ScriptContext;

typedef struct {
    char state_string[64];
    char script_detected[MAX_FILE_LENGTH];
    ScriptContext* script;
} AppFSM;

static int g_overflow_detected = 0;

#ifdef FIXED
static int execute_script_fixed(AppFSM* ctx, const char* filename) {
    if(strlen(filename) >= MAX_FILE_LENGTH) {
        snprintf(ctx->state_string, sizeof(ctx->state_string), "Path too long");
        return 0;
    }
    ctx->script = malloc(sizeof(ScriptContext));
    strcpy(ctx->script->filename, filename);
    return 1;
}

static int filebrowser_down_fixed(AppFSM* ctx, const char* path) {
    if(strlen(path) < sizeof(ctx->script_detected)) {
        strcpy(ctx->script_detected, path);
        return 1;
    }
    snprintf(ctx->state_string, sizeof(ctx->state_string), "Path too long");
    return 0;
}
#else
static int execute_script_old(AppFSM* ctx, const char* filename) {
    ctx->script = malloc(sizeof(ScriptContext));
    strcpy(ctx->script->filename, filename); // <- kein Laengencheck, Heap-OOB moeglich
    return 1;
}

static int filebrowser_down_old(AppFSM* ctx, const char* path) {
    strcpy(ctx->script_detected, path); // <- kein Laengencheck, Heap-OOB moeglich
    return 1;
}
#endif

// baut einen plausiblen, tief verschachtelten SD-Kartenpfad (>127 Byte) nach.
static void build_long_path(char* buf, size_t cap) {
    size_t pos = 0;
    const char* segment = "nested_project_folder_name/";
    while(pos + strlen(segment) < cap - 1) {
        memcpy(buf + pos, segment, strlen(segment));
        pos += strlen(segment);
    }
    strcpy(buf + pos, "script.swd");
}

int main(void) {
    // Szenario 1: normaler, kurzer Pfad -- muss in ALT wie NEU identisch funktionieren
    // (Regressionstest).
    const char* short_path = "/ext/swd_scripts/dump_flash.swd";
    AppFSM ctx_short = {0};
#ifdef FIXED
    int ok1 = execute_script_fixed(&ctx_short, short_path);
    int ok2 = filebrowser_down_fixed(&ctx_short, short_path);
#else
    int ok1 = execute_script_old(&ctx_short, short_path);
    int ok2 = filebrowser_down_old(&ctx_short, short_path);
#endif
    if(!ok1 || !ok2 || strcmp(ctx_short.script->filename, short_path) != 0 ||
       strcmp(ctx_short.script_detected, short_path) != 0) {
        fprintf(stderr, "REGRESSION FAIL: kurzer Pfad falsch behandelt\n");
        return 1;
    }
    printf("Regression (kurzer Pfad): OK, script_detected=%s\n", ctx_short.script_detected);
    free(ctx_short.script);

    // Szenario 2: Grenzfall genau MAX_FILE_LENGTH-1 Zeichen (127 + NUL = 128, passt exakt) --
    // muss in ALT wie NEU noch erfolgreich sein.
    char boundary_path[MAX_FILE_LENGTH];
    memset(boundary_path, 'A', MAX_FILE_LENGTH - 1);
    boundary_path[MAX_FILE_LENGTH - 1] = '\0';
    AppFSM ctx_boundary = {0};
#ifdef FIXED
    int ok3 = execute_script_fixed(&ctx_boundary, boundary_path);
#else
    int ok3 = execute_script_old(&ctx_boundary, boundary_path);
#endif
    if(!ok3 || strcmp(ctx_boundary.script->filename, boundary_path) != 0) {
        fprintf(stderr, "GRENZFALL FAIL: 127-Zeichen-Pfad haette passen muessen\n");
        return 1;
    }
    printf("Grenzfall (127 Zeichen, passt exakt): OK\n");
    free(ctx_boundary.script);

    // Szenario 3: Fund -- ein tief verschachtelter, >127 Byte langer Pfad.
    char long_path[512];
    build_long_path(long_path, sizeof(long_path));
    printf("Langer Pfad (%zu Byte): %s\n", strlen(long_path), long_path);

    AppFSM ctx_long = {0};
#ifdef FIXED
    int ok4 = execute_script_fixed(&ctx_long, long_path);
    if(ok4) {
        fprintf(stderr, "FIX FAIL: haette >127-Byte-Pfad ablehnen muessen\n");
        return 1;
    }
    printf("Fix-Nachweis (execute_script): Pfad korrekt abgelehnt, state_string='%s'\n",
           ctx_long.state_string);

    AppFSM ctx_long2 = {0};
    int ok5 = filebrowser_down_fixed(&ctx_long2, long_path);
    if(ok5) {
        fprintf(stderr, "FIX FAIL: haette >127-Byte-Pfad ablehnen muessen (script_detected)\n");
        return 1;
    }
    printf("Fix-Nachweis (script_detected): Pfad korrekt abgelehnt, state_string='%s'\n",
           ctx_long2.state_string);
#else
    // ALT: strcpy() schreibt ueber das 128-Byte-Feld hinaus -- AddressSanitizer bricht hier mit
    // heap-buffer-overflow ab (ctx_long.script ist malloc()-alloziert).
    g_overflow_detected = 1;
    int ok4 = execute_script_old(&ctx_long, long_path);
    (void)ok4;
    // wird nie erreicht -- ASan bricht bereits in strcpy() ab.
    printf("FUND NICHT AUSGELOEST (unerwartet): %s\n", ctx_long.script->filename);
#endif

    (void)g_overflow_detected;
    return 0;
}

// Ergebnis (gcc -std=c11 -O0/-O1 -g -Wall -Wextra -fsanitize=address,undefined
// -fno-sanitize-recover=undefined):
//
// ALT (ohne -DFIXED):
//   Regression (kurzer Pfad): OK, script_detected=/ext/swd_scripts/dump_flash.swd
//   Grenzfall (127 Zeichen, passt exakt): OK
//   Langer Pfad (496 Byte): nested_project_folder_name/.../script.swd
//   ==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... WRITE of size 497
//       #0 ... in strcpy
//       #1 ... in execute_script_old repro.c:57
//       #2 ... in main repro.c
//   0x... is located 0 bytes after 128-byte region ... allocated by ... in execute_script_old
//   SUMMARY: AddressSanitizer: heap-buffer-overflow
//   Exit-Code: 1 (Abbruch) -- Fund reproduziert, kein manuell ausgeloester Bruch
//
// NEU (mit -DFIXED):
//   Regression (kurzer Pfad): OK, script_detected=/ext/swd_scripts/dump_flash.swd
//   Grenzfall (127 Zeichen, passt exakt): OK
//   Langer Pfad (496 Byte): nested_project_folder_name/.../script.swd
//   Fix-Nachweis (execute_script): Pfad korrekt abgelehnt, state_string='Path too long'
//   Fix-Nachweis (script_detected): Pfad korrekt abgelehnt, state_string='Path too long'
//   Exit-Code: 0 (sauberer Lauf, identisch unter -O0 und -O1)
