// Host-Repro fuer den Fund "malloc() ohne NULL-Absicherung in *_alloc()"
// (ford_v0.c/kia_v1.c/kia_v2.c/scher_khan.c/subaru.c, protocols/*.c).
//
// Bildet das in allen neun Fundstellen identische Strukturmuster nach:
//   Instance* instance = malloc(sizeof(Instance));
//   instance->base.protocol = ...;          // Schreibzugriff ohne Zwischenpruefung
//   instance->generic.protocol_name = ...;  // zweiter Schreibzugriff
//
// ALT: exakt dieses Muster, mit malloc() durch einen Stub ersetzt, der bei
// simuliertem Speichermangel NULL liefert (wie es echtes malloc() auf dem
// Geraet bei erschoepftem Heap ebenfalls tun wuerde) -- der folgende
// Schreibzugriff ueber den NULL-Pointer ist undefiniertes Verhalten.
// NEU: derselbe Ablauf, aber mit einer furi_check()-Simulation direkt nach
// dem Alloc-Aufruf (wie in den elf bereits korrekt abgesicherten
// Geschwister-Funktionen desselben Repos, u.a. honda_static.c/kia_v0.c/
// kia_v3_v4.c/kia_v5.c/kia_v6.c/star_line.c) -- bei NULL kontrollierter
// Abbruch mit Diagnosemeldung, VOR jedem Schreibzugriff.
//
// gcc -O0 -Wall -Wextra -o repro repro.c && ./repro
// (bewusst ohne AddressSanitizer: ASan faengt den NULL-Pointer-Zugriff selbst
// ab und meldet ihn als kontrollierten Prozessabbruch mit Exit-Code -- das
// wuerde den hier gezielt sichtbar zu machenden Unterschied zwischen einem
// rohen SIGSEGV (ALT) und einem kontrollierten furi_check()-Abbruch (NEU)
// verschleiern.)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Minimaler Nachbau der in allen betroffenen Dateien identischen Struktur-
// Form: ein SubGhzProtocolDecoderBase-artiges "base.protocol"-Feld und ein
// SubGhzBlockGeneric-artiges "generic.protocol_name"-Feld, beides in
// *_alloc() unmittelbar nach malloc() beschrieben.
typedef struct {
    const void* protocol;
} BaseLike;

typedef struct {
    const char* protocol_name;
} GenericLike;

typedef struct {
    BaseLike base;
    GenericLike generic;
} InstanceLike;

static const char* const kProtocolName = "TestProtocol";

// Simuliert malloc() unter Speichermangel: liefert NULL, wie es echtes
// malloc() auf dem Geraet bei erschoepftem Heap ebenfalls tun wuerde.
static void* out_of_memory_malloc(size_t size) {
    (void)size;
    return NULL;
}

// ALT: exakte Kopie des in ford_v0.c/kia_v1.c/kia_v2.c/scher_khan.c/
// subaru.c vorgefundenen Musters -- kein furi_check(), kein NULL-Test.
static void* alloc_old(void) {
    InstanceLike* instance = out_of_memory_malloc(sizeof(InstanceLike));
    instance->base.protocol = kProtocolName; // Schreibzugriff via NULL
    instance->generic.protocol_name = kProtocolName; // zweiter Schreibzugriff via NULL
    return instance;
}

// Simuliert furi_check(): bricht bei NULL sofort kontrolliert ab, mit
// Diagnosemeldung -- exakt das in elf Geschwister-Funktionen dieses Repos
// bereits vorhandene Muster.
static void furi_check_sim(const void* ptr, const char* where) {
    if(ptr == NULL) {
        fprintf(stderr, "furi_check: assertion failed (NULL) in %s -- controlled abort\n", where);
        _exit(42); // eindeutiger, kontrollierter Exit-Code statt eines Hardware-Faults
    }
}

// NEU: derselbe Ablauf, mit furi_check() direkt nach malloc(), vor jedem
// Schreibzugriff.
static void* alloc_new(void) {
    InstanceLike* instance = out_of_memory_malloc(sizeof(InstanceLike));
    furi_check_sim(instance, "alloc_new");
    instance->base.protocol = kProtocolName;
    instance->generic.protocol_name = kProtocolName;
    return instance;
}

typedef void* (*AllocFn)(void);

// Fuehrt fn() in einem eigenen Kindprozess aus und meldet, wie er endete --
// dieselbe fork-isolierte Vorgehensweise wie in frueheren Sitzungen dieses
// Repos (z. B. beim NFC-FeliCa-list_system-Fund), damit ein SIGSEGV in ALT
// nicht den gesamten Testlauf abbricht.
static void run_isolated(const char* label, AllocFn fn) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        void* result = fn();
        (void)result;
        _exit(0); // wird bei ALT nie erreicht (SIGSEGV vorher)
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status)) {
        printf("%s: kontrolliert beendet, exit code %d\n", label, WEXITSTATUS(status));
    } else if(WIFSIGNALED(status)) {
        printf(
            "%s: durch Signal beendet (%s) -- unkontrollierter Absturz ueber "
            "NULL-Pointer-Schreibzugriff\n",
            label,
            strsignal(WTERMSIG(status)));
    } else {
        printf("%s: unerwarteter Status 0x%x\n", label, status);
    }
}

int main(void) {
    printf("=== ALT: subghz_protocol_*_alloc() ohne NULL-Absicherung, Speicher erschoepft ===\n");
    run_isolated("ALT", alloc_old);

    printf("\n=== NEU: dieselbe Funktion mit furi_check() direkt nach malloc() ===\n");
    run_isolated("NEU", alloc_new);

    return 0;
}
