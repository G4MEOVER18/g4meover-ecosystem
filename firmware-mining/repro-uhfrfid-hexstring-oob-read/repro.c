// Host-Repro: convertToHexString() in uhf_app.c (UHF-RFID-App, RogueMaster-Buendel)
//
// Bildet die Groessenrechnung von convertToHexString() 1:1 aus dem gelesenen Quelltext nach.
// Der Zwischenstring, den furi_string_cat_printf()/furi_string_get_cstr() im Original liefern
// (eine NUL-terminierte Folge von "XX "-Hex-Byte-Gruppen), wird hier ohne FuriString aufgebaut
// (build_hex_intermediate) -- fuer die gefundene Luecke (malloc/memcpy-Groesse NACH dem Aufbau)
// ohne Bedeutung, die Groessen/Kopierlogik danach ist wortgetreu uebernommen.
//
// Szenario A (nicht-leerer Read, Normalfall auf jedem erfolgreichen EPC/TID/USER-Read):
//   ALT allokiert genau strlen(furi_str) Bytes und kopiert ebenso viele Bytes -- der
//   zurueckgegebene String traegt keine Nullterminierung. Jeder Aufrufer, der ihn als C-String
//   behandelt (hier nachgebildet per strlen(), wie es widget_add_string_multiline_element()
//   intern tut), liest ueber die Allokation hinaus.
// Szenario B (leerer Read, epc/tid/user->size == 0, z. B. direkt nach uhf_tag_alloc() vor einem
//   erfolgreichen Read): ALT gibt einen statischen String-Literal-Zeiger (" ") zurueck, den
//   beide Aufrufstellen (uhf_scene_read_tag_success.c, uhf_scene_device_info.c indirekt ueber
//   denselben Funktionskoerper) anschliessend per free() freigeben -- Freigabe von
//   Nicht-Heap-Speicher.
//
// Jedes Szenario laeuft in einem eigenen fork()-Kindprozess, damit ein ASan-Abbruch in einem
// Szenario den Testlauf der uebrigen nicht beendet.
//
// gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

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

/* ALT -- 1:1 Groessenrechnung aus uhf_app.c::convertToHexString() */
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
   Leerfall zurueckgeben (freibar). */
static char* convert_to_hex_string_neu(const uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    char* furi_str = build_hex_intermediate(array, length);
    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(str_len + 1);
    memcpy(str, furi_str, str_len + 1); /* Terminator wird mitkopiert */
    free(furi_str);
    return str;
}

static void scenario_a_alt(void) {
    const uint8_t epc[4] = {0xDE, 0xAD, 0xBE, 0xEF}; /* typische EPC-Bank-Groesse */
    printf("=== Szenario A, ALT (nicht-leerer Read, EPC-Bank 4 Byte) ===\n");
    char* str = convert_to_hex_string_alt(epc, sizeof(epc));
    /* Wie widget_add_string_multiline_element(): behandelt den Rueckgabewert als C-String. */
    size_t len = strlen(str);
    printf(
        "[UNERWARTET DURCHGELAUFEN] strlen() ergab %zu (allokiert waren nur %zu Byte ohne "
        "Terminator)\n",
        len,
        sizeof(epc) * 3);
    free(str);
}

static void scenario_a_neu(void) {
    const uint8_t epc[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    printf("=== Szenario A, NEU (nicht-leerer Read, EPC-Bank 4 Byte) ===\n");
    char* str = convert_to_hex_string_neu(epc, sizeof(epc));
    size_t len = strlen(str);
    printf(
        "[OK] strlen() = %zu, erwartet %zu -> %s\n",
        len,
        sizeof(epc) * 3,
        len == sizeof(epc) * 3 ? "stimmt ueberein" : "WEICHT AB");
    free(str);
}

static void scenario_b_alt(void) {
    printf("=== Szenario B, ALT (leerer Read, size == 0) ===\n");
    char* str = convert_to_hex_string_alt(NULL, 0);
    /* uhf_scene_read_tag_success.c / uhf_scene_device_info.c: free(epc) unconditional. */
    free(str);
    printf("[UNERWARTET DURCHGELAUFEN] free() auf String-Literal hat nicht abgebrochen\n");
}

static void scenario_b_neu(void) {
    printf("=== Szenario B, NEU (leerer Read, size == 0) ===\n");
    char* str = convert_to_hex_string_neu(NULL, 0);
    printf("[OK] Rueckgabe ist ein echter, 1 Byte grosser Heap-String (strlen=%zu)\n", strlen(str));
    free(str);
    printf("[OK] free() auf Heap-Allokation lief sauber durch\n");
}

/* Fuehrt `fn` in einem Kindprozess aus; meldet, ob es sauber durchlief oder per Signal
   (z. B. ASan-Abbruch bei SIGABRT) beendet wurde. */
static void run_isolated(const char* label, void (*fn)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        fn();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) {
        printf("[CRASH] %s wurde durch Signal %d beendet (ASan/UBSan-Abbruch)\n", label, WTERMSIG(status));
    } else if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        printf("[CRASH] %s beendete sich mit Exit-Code %d\n", label, WEXITSTATUS(status));
    } else {
        printf("[KEIN ABBRUCH] %s lief sauber durch\n", label);
    }
    printf("\n");
}

int main(void) {
    run_isolated("Szenario A (ALT)", scenario_a_alt);
    run_isolated("Szenario A (NEU)", scenario_a_neu);
    run_isolated("Szenario B (ALT)", scenario_b_alt);
    run_isolated("Szenario B (NEU)", scenario_b_neu);
    printf(
        "Erwartet: A/ALT und B/ALT brechen ab (Heap-Buffer-Overflow-Read bzw. Free auf "
        "Nicht-Heap-Speicher), A/NEU und B/NEU laufen sauber durch.\n");
    return 0;
}
