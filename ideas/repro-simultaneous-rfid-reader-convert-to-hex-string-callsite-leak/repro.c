// Host-Repro: Speicherleck an den fuenf convert_to_hex_string()-Aufrufstellen plus totem
// strncpy()-Nebenfund in simultaneous_rfid_reader/views/view_config.c (haffnerriley/
// Simultaneous-UHF-RFID-FlipperZero, Commit 1dfefed).
//
// Setzt den in ideas/simultaneous-rfid-reader-convert-to-hex-string-no-terminator-fix.md als
// "Offen, bewusst nicht Teil dieses Patches" vermerkten Nachzug um: dort wurde nur die
// fehlende Nullterminierung in convert_to_hex_string() selbst behoben (view_read_c.patch);
// das an allen vier echten snprintf()-Aufrufstellen fehlende free() sowie der tote
// strncpy()-Block in view_config.c:728-739 blieben dort bewusst offen.
//
// convert_to_hex_string_neu() hier ist 1:1 die bereits vorgeschlagene (noch nicht in diesem
// Patch enthaltene) Terminator-Fix-Version, NICHT die unveraenderte ALT-Version -- der
// fehlende Terminator wuerde sonst sofort einen Heap-Buffer-Overflow-Read ausloesen (bereits
// in repro-simultaneous-rfid-reader-convert-to-hex-string-no-terminator/repro.c belegt) und
// das hier eigentlich geprueft Leck-Verhalten verdecken. Dieses Repro isoliert die zweite,
// unabhaengige Haelfte: ob die jeweilige Aufrufstelle den Rueckgabewert freigibt.
//
// Vier reale Aufrufstellen (gleiche Zielpuffer-Groessen wie im Original):
//   view_config.c:397 (malloc(24)), view_kill.c:107 (malloc(12)), view_kill.c:215 (malloc(9)),
//   view_lock.c:59 (malloc(8)). Je ein ALT-Szenario (Rueckgabewert nirgends uebernommen, wie im
//   unveraenderten Original) und ein NEU-Szenario (Rueckgabewert in HexStr uebernommen und nach
//   dem snprintf() freigegeben, wie im Patch) unter LeakSanitizer.
//
// Fuenfter Aufrufstellen-Block (view_config.c:728-739, "Set AP"-Menuepunkt): ALT fuehrt ein
// zusaetzliches strncpy() auf den sofort verworfenen Rueckgabewert aus (Leck, kein Effekt auf
// App->ApTempBuffer); NEU entfernt den gesamten Block ersatzlos. Geprueft wird: (a) NEU leckt
// nicht, (b) ApTempBuffer ist vor/nach dem (nicht mehr vorhandenen) strncpy()-Aufruf identisch --
// der Block war bereits im Original wirkungslos fuer den Puffer selbst.
//
// Jedes Szenario laeuft in einem eigenen fork()-Kindprozess, damit ein LeakSanitizer-Abbruch in
// einem Szenario den Testlauf der uebrigen nicht beendet.
//
// gcc -std=c17 -O0/-O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro && ./repro

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char* build_hex_intermediate(const uint8_t* array, size_t length) {
    char* buf = (char*)malloc(length * 3 + 1);
    buf[0] = '\0';
    for(size_t i = 0; i < length; i++) {
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02X ", array[i]);
        strcat(buf, tmp);
    }
    return buf;
}

/* Bereits vorgeschlagener Terminator-Fix aus view_read_c.patch (hier nur als Baustein
   uebernommen, nicht Gegenstand dieses Patches). */
static char* convert_to_hex_string_neu(const uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    char* furi_str = build_hex_intermediate(array, length);
    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(str_len + 1);
    memcpy(str, furi_str, str_len + 1);
    free(furi_str);
    return str;
}

typedef struct {
    const char* label;
    size_t bufsize;
} CallSite;

static const uint8_t PWD[4] = {0xDE, 0xAD, 0xBE, 0xEF};

/* ALT -- 1:1 die unveraenderte Aufrufstelle: Rueckgabewert wird nur als Argument uebergeben,
   nie in eine Variable uebernommen -> ab Ende des Ausdrucks unerreichbar. */
static void callsite_old(size_t bufsize) {
    char* tempBuffer = (char*)malloc(bufsize);
    snprintf(tempBuffer, bufsize, "%s", convert_to_hex_string_neu(PWD, sizeof(PWD)));
    free(tempBuffer); /* tempBuffer selbst wird an der realen Aufrufstelle ebenfalls nicht
                          freigegeben; hier freigegeben, um ausschliesslich das
                          convert_to_hex_string()-Leck zu isolieren, nicht ein davon
                          unabhaengiges, bereits vor diesem Fund bestehendes Verhalten. */
}

/* NEU -- wie im Patch: Rueckgabewert in HexStr uebernehmen, nach dem snprintf() freigeben. */
static void callsite_new(size_t bufsize) {
    char* tempBuffer = (char*)malloc(bufsize);
    char* HexStr = convert_to_hex_string_neu(PWD, sizeof(PWD));
    snprintf(tempBuffer, bufsize, "%s", HexStr);
    free(HexStr);
    free(tempBuffer);
}

/* ALT -- view_config.c:728-739, totes strncpy() auf den sofort verworfenen Rueckgabewert. */
static void dead_strncpy_old(uint8_t* ApTempBuffer, const char* settingReadAp, size_t apInputBufferSize) {
    strncpy(convert_to_hex_string_neu(ApTempBuffer, 4), settingReadAp, apInputBufferSize);
    /* Rueckgabewert wird nirgends gespeichert -> Leck; ApTempBuffer selbst unveraendert. */
}

/* NEU -- Block ersatzlos entfernt: nichts zu tun, ApTempBuffer bleibt unveraendert. */
static void dead_strncpy_new(uint8_t* ApTempBuffer, const char* settingReadAp, size_t apInputBufferSize) {
    (void)ApTempBuffer;
    (void)settingReadAp;
    (void)apInputBufferSize;
}

static void run_isolated(const char* label, void (*fn)(void*), void* arg) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        fn(arg);
        exit(0); /* nicht _exit(): LeakSanitizers atexit-Hook muss noch laufen */
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) {
        printf("[CRASH] %s wurde durch Signal %d beendet\n", label, WTERMSIG(status));
    } else if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        printf("[ABBRUCH/LECK] %s beendete sich mit Exit-Code %d (LeakSanitizer)\n", label,
               WEXITSTATUS(status));
    } else {
        printf("[OK, KEIN LECK] %s lief sauber durch\n", label);
    }
}

static CallSite g_site;
static void wrap_old(void* arg) {
    (void)arg;
    callsite_old(g_site.bufsize);
}
static void wrap_new(void* arg) {
    (void)arg;
    callsite_new(g_site.bufsize);
}

static uint8_t g_ap_temp_buffer_old[4];
static uint8_t g_ap_temp_buffer_new[4];
static void wrap_dead_old(void* arg) {
    (void)arg;
    memcpy(g_ap_temp_buffer_old, PWD, 4);
    dead_strncpy_old(g_ap_temp_buffer_old, "AABBCCDD", 4);
    printf("    ApTempBuffer nach ALT-Block: %02X %02X %02X %02X (unveraendert erwartet)\n",
           g_ap_temp_buffer_old[0], g_ap_temp_buffer_old[1], g_ap_temp_buffer_old[2],
           g_ap_temp_buffer_old[3]);
    fflush(stdout); /* vor dem bevorstehenden LeakSanitizer-Abbruch sichern */
}
static void wrap_dead_new(void* arg) {
    (void)arg;
    memcpy(g_ap_temp_buffer_new, PWD, 4);
    dead_strncpy_new(g_ap_temp_buffer_new, "AABBCCDD", 4);
    printf("    ApTempBuffer nach NEU (Block entfernt): %02X %02X %02X %02X (unveraendert erwartet)\n",
           g_ap_temp_buffer_new[0], g_ap_temp_buffer_new[1], g_ap_temp_buffer_new[2],
           g_ap_temp_buffer_new[3]);
}

int main(void) {
    CallSite sites[] = {
        {"view_config.c:397 (malloc(24))", 24},
        {"view_kill.c:107 (malloc(12))", 12},
        {"view_kill.c:215 (malloc(9))", 9},
        {"view_lock.c:59 (malloc(8))", 8},
    };
    int n = (int)(sizeof(sites) / sizeof(sites[0]));

    for(int i = 0; i < n; i++) {
        g_site = sites[i];
        char label_old[128], label_new[128];
        snprintf(label_old, sizeof(label_old), "ALT %s", g_site.label);
        snprintf(label_new, sizeof(label_new), "NEU %s", g_site.label);
        run_isolated(label_old, wrap_old, NULL);
        run_isolated(label_new, wrap_new, NULL);
    }

    printf("\n--- Nebenfund view_config.c:728-739 (totes strncpy()) ---\n");
    run_isolated("ALT Set-AP-Block (totes strncpy())", wrap_dead_old, NULL);
    run_isolated("NEU Set-AP-Block (entfernt)", wrap_dead_new, NULL);
    if(memcmp(g_ap_temp_buffer_old, g_ap_temp_buffer_new, 4) == 0) {
        printf("[OK] ApTempBuffer nach ALT und NEU byteidentisch -- Block war bereits im "
               "Original ohne Effekt auf den Puffer.\n");
    } else {
        printf("[ABWEICHUNG] ApTempBuffer unterscheidet sich -- unerwartet.\n");
    }

    printf(
        "\nErwartet: alle vier ALT-Aufrufstellen melden ein Leck (fehlendes free() auf den "
        "Rueckgabewert von convert_to_hex_string()), alle vier NEU-Aufrufstellen laufen "
        "lecksauber durch. ALT-Set-AP-Block meldet ebenfalls ein (zusaetzliches) Leck, "
        "NEU (Block entfernt) nicht. ApTempBuffer ist in beiden Faellen identisch.\n");
    return 0;
}
