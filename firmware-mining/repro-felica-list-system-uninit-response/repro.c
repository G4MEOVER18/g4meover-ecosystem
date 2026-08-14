// Host-Mini-Repro: felica_poller_state_handler_list_system() dereferenziert
// response_system_code auch auf dem Error-Pfad von felica_poller_list_system_code(),
// bei dem die Funktion *response_ptr nie schreibt (echte Signatur: mntm-012
// lib/nfc/protocols/felica/felica_poller_i.c::felica_poller_list_system_code()).
// Kein Flipper-SDK nötig: 1:1 nachgebaute Struct-Form + Kontrollfluss, keine
// tatsächliche uninitialisierte Speicherstelle nötig, um den Bug zu zeigen — der
// Bug liegt im Kontrollfluss (Dereferenzierung vor der Erfolgspruefung), nicht im
// konkreten Bitmuster des uninitialisierten Werts. Um das deterministisch und
// portabel zu demonstrieren, wird der "uninitialisiert gebliebene" Pointer
// bewusst auf eine ungueltige Adresse (0x1) gesetzt -- das simuliert nur den
// Fall, dass der reale Stack-Muell zufaellig KEINE gueltige Adresse ist; ein
// echter Stackwert kann natuerlich auch eine "zufaellig gueltige" Adresse mit
// Speicherinhalt sein und dann ohne Absturz falsche Daten liefern, was in der
// Praxis schwerer zu erkennen waere als ein klarer Crash.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { FelicaErrorNone, FelicaErrorTimeout, FelicaErrorProtocol } FelicaError;

typedef struct {
    uint8_t system_count;
    uint8_t system_code[2 * 16];
} FelicaListSystemCodeCommandResponse;

typedef struct {
    uint8_t systems_total;
    int state;
} FelicaPollerMock;

#define INVALID_SENTINEL ((FelicaListSystemCodeCommandResponse*)0x1)

// Mock von felica_poller_list_system_code(): bildet exakt die drei Rueckgabepfade
// der echten Funktion nach (Timeout im frame_exchange, zu kurze Antwort, Erfolg).
// *response_ptr wird nur im Erfolgsfall geschrieben -- wie im echten Code.
static FelicaError mock_list_system_code(
    FelicaListSystemCodeCommandResponse** response_ptr,
    FelicaError simulated_error) {
    if(simulated_error != FelicaErrorNone) {
        return simulated_error; // *response_ptr bleibt unberuehrt, wie im Original
    }
    static FelicaListSystemCodeCommandResponse real_response = {.system_count = 2};
    *response_ptr = &real_response;
    return FelicaErrorNone;
}

// ALT (mntm-012, fehlerhaft): dereferenziert response_system_code, bevor der
// Fehlerstatus geprueft wird -- 1:1 der reale Kontrollfluss aus
// felica_poller_state_handler_list_system().
static void handler_old(FelicaPollerMock* instance, FelicaError simulated_error) {
    FelicaListSystemCodeCommandResponse* response_system_code = INVALID_SENTINEL;
    FelicaError error = mock_list_system_code(&response_system_code, simulated_error);

    instance->systems_total = response_system_code->system_count; // <- Bug: vor Fehlerpruefung
    (void)instance->systems_total;

    if(error == FelicaErrorNone) {
        instance->state = 1;
    } else if(error != FelicaErrorTimeout) {
        instance->state = 2;
    }
}

// NEU (Fix aus PR #4428): Dereferenzierung erst innerhalb des Erfolgszweigs.
static void handler_new(FelicaPollerMock* instance, FelicaError simulated_error) {
    FelicaListSystemCodeCommandResponse* response_system_code = INVALID_SENTINEL;
    FelicaError error = mock_list_system_code(&response_system_code, simulated_error);

    if(error == FelicaErrorNone) {
        instance->systems_total = response_system_code->system_count; // jetzt sicher
        instance->state = 1;
    } else if(error != FelicaErrorTimeout) {
        instance->state = 2;
    }
}

typedef void (*handler_fn)(FelicaPollerMock*, FelicaError);

// Fuehrt den Handler in einem Kindprozess aus, damit ein SIGSEGV (durch das
// Dereferenzieren von Adresse 0x1) den Testlauf selbst nicht abbricht.
static int run_isolated(handler_fn fn, FelicaError simulated_error) {
    pid_t pid = fork();
    if(pid == 0) {
        FelicaPollerMock instance = {0};
        fn(&instance, simulated_error);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) return WTERMSIG(status);
    // AddressSanitizer faengt SIGSEGV selbst ab, meldet es und beendet den
    // Prozess per _exit(1) statt das Signal roh durchschlagen zu lassen --
    // aus Sicht des Elternprozesses per waitpid() daher ein normaler Exit
    // mit Code != 0, kein WIFSIGNALED. Beides zaehlt hier als "Absturz".
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) return -1;
    return 0;
}

static const char* error_name(FelicaError e) {
    switch(e) {
    case FelicaErrorNone:
        return "FelicaErrorNone (Erfolg)";
    case FelicaErrorTimeout:
        return "FelicaErrorTimeout";
    case FelicaErrorProtocol:
        return "FelicaErrorProtocol (zu kurze Antwort)";
    }
    return "?";
}

int main(void) {
    FelicaError cases[] = {FelicaErrorNone, FelicaErrorTimeout, FelicaErrorProtocol};
    int all_ok = 1;

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FelicaError e = cases[i];
        int sig_old = run_isolated(handler_old, e);
        int sig_new = run_isolated(handler_new, e);

        printf("== Fall: %s ==\n", error_name(e));
        printf("  ALT (mntm-012): %s\n",
               sig_old != 0 ? "ABSTURZ (SEGV bei Dereferenzierung vor Fehlerpruefung)"
                            : "kein Absturz");
        printf("  NEU (Fix):      %s\n", sig_new != 0 ? "ABSTURZ" : "kein Absturz");

        // Unter AddressSanitizer faengt der SEGV-Handler den Zugriff auf 0x1 ab,
        // meldet ihn (siehe Ausgabe oben) und beendet den Prozess per abort() ->
        // SIGABRT statt des rohen SIGSEGV. Fuer den Test zaehlt daher jedes
        // Abbruchsignal als "Absturz", nicht nur SIGSEGV.
        int expect_crash_old = (e != FelicaErrorNone); // jeder Error-Pfad dereferenziert ALT
        int old_matches = expect_crash_old ? (sig_old != 0) : (sig_old == 0);
        int new_ok = (sig_new == 0); // NEU darf in keinem der drei Faelle abstuerzen
        if(!old_matches || !new_ok) all_ok = 0;

        printf("  Erwartung ALT (%s): %s\n",
               expect_crash_old ? "Absturz" : "kein Absturz",
               old_matches ? "bestaetigt" : "NICHT bestaetigt");
        printf("  Erwartung NEU (kein Absturz): %s\n\n", new_ok ? "bestaetigt" : "NICHT bestaetigt");
    }

    printf(all_ok ? "ERGEBNIS: bestaetigt -- ALT stuerzt bei jedem Fehlerpfad ab, NEU nicht.\n"
                  : "ERGEBNIS: Abweichung von der Erwartung, siehe oben.\n");
    return all_ok ? 0 : 1;
}
