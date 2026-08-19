// Host-Mini-Repro: mf_desfire_poller_read_key_versions() (live Kartenlesung) und
// mf_desfire_load() (Laden eines .nfc-Dumps von SD) rufen simple_array_init() mit
// einer Elementanzahl auf, die direkt aus einem Kartenfeld (KeySettings2-Byte, low
// nibble = MaxKeys) bzw. einem Dump-Feld stammt und legitim 0 sein kann (echte
// Signaturen: mntm-012 lib/nfc/protocols/mf_desfire/mf_desfire_poller_i.c::
// mf_desfire_poller_read_key_versions() und mf_desfire.c::mf_desfire_load()).
// simple_array_init() selbst hat intern furi_check(count > 0) (lib/toolbox/
// simple_array.c) -- ein Aufruf mit count==0 loest also in jedem Fall einen
// furi_check-Abbruch (Geraeteabsturz/-neustart) aus, ob nun der Aufrufer selbst noch
// vorher zusaetzlich prueft (Poller-Pfad, ALT) oder gar nicht (Load-Pfad, ALT).
// furi_check() wird hier als assert-artiger Prozessabbruch nachgebildet (die reale
// Funktion ruft furi_crash() -> haelt das Geraet an); simple_array_init() bildet nur
// den fuer den Bug relevanten Kontrollfluss nach (kein echtes Alloc/Init noetig).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

// Nachbildung von furi_check(): bricht den Prozess ab, wenn cond falsch ist --
// wie die reale furi_check()-Kette (furi_check -> furi_crash -> Geraeteanhalt).
#define FURI_CHECK(cond)          \
    do {                          \
        if(!(cond)) abort();      \
    } while(0)

// Nachbildung von simple_array_init() (lib/toolbox/simple_array.c): hat selbst
// intern furi_check(count > 0) -- unabhaengig davon, ob der Aufrufer vorher schon
// prueft. Ein Aufruf mit count==0 stuerzt hier IMMER ab.
static void simple_array_init_mock(uint32_t count) {
    FURI_CHECK(count > 0);
    // echte Funktion: malloc + optionale Elementinitialisierung, fuer den Bug nicht
    // relevant, hier ausgelassen.
}

// ---- Live-Kartenlesung: mf_desfire_poller_read_key_versions() ----
// ALT (mntm-012, fehlerhaft): furi_check(count > 0) direkt im Poller, danach
// zusaetzlich der interne Check in simple_array_init_mock() -- beide stuerzen bei
// count==0 identisch ab, das ALT-Verhalten insgesamt aendert sich dadurch nicht.
static void read_key_versions_old(uint32_t count) {
    FURI_CHECK(count > 0);
    simple_array_init_mock(count);
}

// NEU (Fix aus PR #4439): count==0 ist ein legitimer Kartenwert (Applikation ohne
// Schluessel) -- wird jetzt abgefangen statt an simple_array_init() durchgereicht.
static void read_key_versions_new(uint32_t count) {
    if(count > 0) {
        simple_array_init_mock(count);
    } else {
        printf("    [LOG] Application reports zero keys, key versions skipped\n");
    }
}

// ---- Dump-Laden von SD: mf_desfire_load(), Master-Key-Zweig ----
// ALT (mntm-012, fehlerhaft): kein Guard vor simple_array_init() -- der interne
// Check dort greift bei count==0.
static void load_master_keys_old(uint32_t count) {
    simple_array_init_mock(count);
}

// NEU (Fix): derselbe Guard wie im Poller-Pfad.
static void load_master_keys_new(uint32_t count) {
    if(count > 0) {
        simple_array_init_mock(count);
    } else {
        printf("    [LOG] PICC reports zero master keys, key versions skipped\n");
    }
}

typedef void (*path_fn)(uint32_t);

// Fuehrt den Pfad in einem Kindprozess aus, damit ein abort() (SIGABRT) den
// Testlauf selbst nicht abbricht.
static int run_isolated(path_fn fn, uint32_t count) {
    pid_t pid = fork();
    if(pid == 0) {
        fn(count);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) return WTERMSIG(status);
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) return -1;
    return 0;
}

static int check_case(
    const char* label,
    path_fn old_fn,
    path_fn new_fn,
    uint32_t count,
    int expect_crash_old) {
    int sig_old = run_isolated(old_fn, count);
    int sig_new = run_isolated(new_fn, count);

    printf("== %s (count=%u) ==\n", label, count);
    printf("  ALT (mntm-012): %s\n",
           sig_old != 0 ? "ABSTURZ (furi_check-Abbruch)" : "kein Absturz");
    printf("  NEU (Fix):      %s\n\n", sig_new != 0 ? "ABSTURZ" : "kein Absturz");

    int old_matches = expect_crash_old ? (sig_old != 0) : (sig_old == 0);
    int new_ok = (sig_new == 0); // NEU darf in keinem Fall abstuerzen
    return old_matches && new_ok;
}

int main(void) {
    int all_ok = 1;

    // Live-Kartenlesung: eine Applikation mit MaxKeys=0 (legitimer DESFire-Wert,
    // Upstream-Issue #3835) triggert den Poller-Pfad bei jedem Lesevorgang.
    all_ok &= check_case("Poller: Applikation MaxKeys=0", read_key_versions_old, read_key_versions_new, 0, 1);
    all_ok &= check_case("Poller: Applikation MaxKeys=3 (Sanity)", read_key_versions_old, read_key_versions_new, 3, 0);

    // Dump-Laden von SD: PICC-Master-Key-Settings mit MaxKeys=0 im gespeicherten
    // .nfc-File (aus genau so einer Karte gespeichert, oder manuell editiert).
    all_ok &= check_case("Load: PICC MasterKeys=0", load_master_keys_old, load_master_keys_new, 0, 1);
    all_ok &= check_case("Load: PICC MasterKeys=1 (Sanity)", load_master_keys_old, load_master_keys_new, 1, 0);

    printf(all_ok
               ? "ERGEBNIS: bestaetigt -- ALT stuerzt bei count==0 auf beiden Pfaden ab, NEU nicht.\n"
               : "ERGEBNIS: Abweichung von der Erwartung, siehe oben.\n");
    return all_ok ? 0 : 1;
}
