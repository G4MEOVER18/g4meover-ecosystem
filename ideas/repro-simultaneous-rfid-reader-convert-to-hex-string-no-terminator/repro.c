// Host-Repro: convert_to_hex_string() in simultaneous_rfid_reader/views/view_read.c
// (haffnerriley/Simultaneous-UHF-RFID-FlipperZero, Commit 1dfefed)
//
// Bildet die Groessenrechnung von convert_to_hex_string() 1:1 aus dem gelesenen Quelltext nach.
// Den Zwischenstring, den furi_string_cat_printf()/furi_string_get_cstr() im Original liefern
// (eine NUL-terminierte Folge von "XX "-Hex-Byte-Gruppen), baut build_hex_intermediate() ohne
// FuriString nach -- fuer die Groessen-/Kopierlogik danach (das eigentlich Geprüfte) ohne
// Bedeutung, wortgetreu aus dem Original uebernommen.
//
// Dieselbe Funktion (identisch bis auf den Namen) ist bereits als convertToHexString() in
// uhf_rfid/uhf_app.c gefunden und dokumentiert (firmware-mining/uhfrfid-hexstring-oob-read-fix.md,
// repro dort). Dieses Repro prueft den Fork-Ableger in den views/*.c-Dateien dieses Projekts, die
// in der vorigen Sitzung noch offen waren, und die davon abweichenden Aufrufstellen (hier nie ein
// free() auf den Rueckgabewert, dort unconditional free() -> andere Zweitfolge, siehe Szenario B).
//
// Szenario A (nicht-leerer Aufruf, der in diesem Projekt einzige real vorkommende Fall -- alle
//   fuenf Aufrufstellen in view_config.c/view_kill.c/view_lock.c uebergeben feste 4-Byte-Puffer):
//   ALT allokiert genau strlen(furi_str) Byte und kopiert ebenso viele Byte -- keine
//   Nullterminierung. Jede Aufrufstelle reicht das Ergebnis direkt in
//   snprintf(tempBuffer, N, "%s", ...) weiter (view_lock.c:59 hat mit N=8 den kleinsten
//   Zielpuffer unter den fuenf Stellen -- hier 1:1 nachgebildet) -- liest beim Aufbau des "%s"
//   ueber die Allokation hinaus.
// Szenario B (Leck, unabhaengig von Szenario A): keine der fuenf echten Aufrufstellen ruft
//   jemals free() auf den Rueckgabewert von convert_to_hex_string() auf -- anders als bei
//   uhf_rfid (dort fuehrt genau dieses free() im Leerfall zum Absturz auf Nicht-Heap-Speicher).
//   Hier fuehrt derselbe fehlende Terminator-Fix (NEU) allein NICHT zu einem sauberen Lauf:
//   LeakSanitizer markiert den fehlenden free() an der (unveraendert nachgebildeten) Aufrufstelle
//   weiterhin als Leck. Erst wenn die Aufrufstelle selbst den Rueckgabewert in eine Variable
//   uebernimmt und freigibt (KORRIGIERTER AUFRUFER), ist der Lauf vollstaendig lecksauber.
//
// Jedes Szenario laeuft in einem eigenen fork()-Kindprozess, damit ein ASan-Abbruch in einem
// Szenario den Testlauf der uebrigen nicht beendet.
//
// gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
// (LeakSanitizer ist unter Linux in -fsanitize=address standardmaessig mit aktiv.)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Baut denselben Zwischenstring auf, den furi_string_cat_printf(..., "%02X ", array[i]) plus
   furi_string_get_cstr() im Original liefern wuerde (NUL-terminiert, 3 Zeichen pro Byte). */
static char* build_hex_intermediate(const uint8_t* array, size_t length) {
    char* buf = (char*)malloc(length * 3 + 1);
    buf[0] = '\0';
    for(size_t i = 0; i < length; i++) {
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02X ", array[i]);
        strcat(buf, tmp);
    }
    return buf; /* strlen(buf) == length * 3, exakt wie furi_string_get_cstr() im Original */
}

/* ALT -- 1:1 Groessenrechnung aus views/view_read.c::convert_to_hex_string() */
static char* convert_to_hex_string_alt(const uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        return (char*)" "; /* String-Literal, wie im Original */
    }
    char* furi_str = build_hex_intermediate(array, length);
    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(sizeof(char) * str_len); /* fehlt: +1 fuer den Terminator */
    memcpy(str, furi_str, str_len); /* kopiert das '\0' von furi_str NICHT mit */
    free(furi_str);
    return str;
}

/* NEU -- Fix-Vorschlag: Terminator mit einplanen und mitkopieren, echten Heap-String auch im
   Leerfall zurueckgeben (freibar). Entspricht dem bereits vorgeschlagenen Fix in
   uhf_rfid/uhf_app.c::convertToHexString(). */
static char* convert_to_hex_string_neu(const uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    char* furi_str = build_hex_intermediate(array, length);
    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(str_len + 1);
    memcpy(str, furi_str, str_len + 1); /* Terminator wird mitgekopiert */
    free(furi_str);
    return str;
}

static void scenario_a_alt(void) {
    const uint8_t pwd[4] = {0xDE, 0xAD, 0xBE, 0xEF}; /* z.B. SetPwdTempBuffer/ApTempBuffer */
    printf("=== Szenario A, ALT (view_lock.c:59 -- snprintf(tempBuffer, 8, \"%%s\", ...)) ===\n");
    char tempBuffer[8]; /* 1:1 view_lock.c:57 (malloc(8), hier als Stack-Puffer, gleiche Groesse) */
    snprintf(tempBuffer, sizeof(tempBuffer), "%s", convert_to_hex_string_alt(pwd, sizeof(pwd)));
    printf("[UNERWARTET DURCHGELAUFEN] tempBuffer=\"%s\"\n", tempBuffer);
}

static void scenario_a_neu(void) {
    const uint8_t pwd[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    printf("=== Szenario A, NEU (Terminator-Fix, Aufrufstelle unveraendert: kein free()) ===\n");
    char tempBuffer[8];
    char* str = convert_to_hex_string_neu(pwd, sizeof(pwd));
    snprintf(tempBuffer, sizeof(tempBuffer), "%s", str);
    printf("[OK, kein Overflow] tempBuffer=\"%s\" (erwartet, 7 Byte + Nullbyte abgeschnitten)\n",
           tempBuffer);
    /* Absichtlich KEIN free(str) -- bildet die unveraenderte Aufrufstelle nach. */
}

static void scenario_b_neu_fixed_caller(void) {
    const uint8_t pwd[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    printf("=== Szenario B, NEU + korrigierter Aufrufer (Rueckgabewert wird freigegeben) ===\n");
    char tempBuffer[8];
    char* str = convert_to_hex_string_neu(pwd, sizeof(pwd));
    snprintf(tempBuffer, sizeof(tempBuffer), "%s", str);
    free(str);
    printf("[OK] tempBuffer=\"%s\", Rueckgabewert freigegeben -- kein Leck\n", tempBuffer);
}

/* Fuehrt `fn` in einem Kindprozess aus; meldet, ob es sauber durchlief oder per Signal/Exit-Code
   (ASan/UBSan/LeakSanitizer-Abbruch) beendet wurde. */
static void run_isolated(const char* label, void (*fn)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        fn();
        exit(0); /* nicht _exit(): LeakSanitizers atexit-Hook muss noch laufen */
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) {
        printf("[CRASH] %s wurde durch Signal %d beendet (ASan/UBSan-Abbruch)\n", label, WTERMSIG(status));
    } else if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        printf("[ABBRUCH/LECK] %s beendete sich mit Exit-Code %d (z.B. LeakSanitizer)\n", label,
               WEXITSTATUS(status));
    } else {
        printf("[KEIN ABBRUCH] %s lief sauber durch\n", label);
    }
    printf("\n");
}

int main(void) {
    run_isolated("Szenario A, ALT (Heap-Buffer-Overflow-Read, realer Aufrufstellen-Puffer)",
                 scenario_a_alt);
    run_isolated("Szenario A, NEU (Terminator-Fix allein, Aufrufstelle unveraendert)",
                 scenario_a_neu);
    run_isolated("Szenario B, NEU + korrigierter Aufrufer (Terminator-Fix + free())",
                 scenario_b_neu_fixed_caller);
    printf(
        "Erwartet: Szenario A/ALT bricht ab (Heap-Buffer-Overflow-Read in snprintf's \"%%s\"); "
        "Szenario A/NEU laeuft nicht ab, aber LeakSanitizer meldet den weiterhin fehlenden "
        "free() an der Aufrufstelle als Leck; Szenario B laeuft vollstaendig sauber durch -- "
        "der Funktions-Fix alleine reicht nicht, jede der fuenf Aufrufstellen muss den "
        "Rueckgabewert zusaetzlich selbst freigeben.\n");
    return 0;
}
