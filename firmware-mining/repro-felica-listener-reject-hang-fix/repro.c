// Host-Repro: Kontrollfluss von felica_listener_run() (NfcEventTypeRxEnd-Zweig) in
// lib/nfc/protocols/felica/felica_listener.c, ALT (mntm-012-Stand, identisch zu
// Unleashed-dev vor PR #4465) vs. NEU (eigener, auf das reine "hang after rejected
// commands"-Teilproblem beschraenkter Fix, Rest von PR #4465 nicht uebernommen).
//
// Prueft NUR den Kontrollfluss (welchen Wert `command` am Ende annimmt), nicht die
// eigentliche FeliCa-Bitlogik -- die haengt an Nfc/BitBuffer/SimpleArray-Typen, die hier
// nicht nachgebaut werden. Jeder Verzweigungspfad der echten Funktion ist 1:1 als
// Fallunterscheidung über Testparameter abgebildet.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum { NfcCommandContinue, NfcCommandReset } NfcCommand;

typedef enum {
    FelicaErrorNone,
    FelicaErrorNotPresent,
    FelicaErrorColResFailed,
    FelicaErrorBufferOverflow,
    FelicaErrorCommunication,
    FelicaErrorFieldOff,
    FelicaErrorWrongCrc,
    FelicaErrorProtocol,
    FelicaErrorTimeout,
    FelicaErrorFeatureUnsupported,
    FelicaErrorIgnore, // neu, nur NEU-Pfad
} FelicaError;

// Testparameter -- bilden exakt die Fallunterscheidungen der echten Funktion ab.
typedef struct {
    bool crc_ok;
    bool length_ok;
    bool is_polling_cmd;
    bool polling_is_hw_leak; // request->polling.system_code == FELICA_SYSTEM_CODE_CODE
    FelicaError system_code_result; // nur relevant wenn is_polling_cmd && !polling_is_hw_leak
    bool idm_ok; // nur relevant wenn !is_polling_cmd
    FelicaError process_request_result; // nur relevant wenn !is_polling_cmd && idm_ok
} Scenario;

static bool felica_crc_check_mock(const Scenario* s) {
    return s->crc_ok;
}
static bool felica_listener_check_block_list_size_mock(const Scenario* s) {
    return s->length_ok;
}
static FelicaError felica_listener_process_system_code_mock(const Scenario* s) {
    return s->system_code_result;
}
static bool felica_listener_check_idm_mock(const Scenario* s) {
    return s->idm_ok;
}
static FelicaError felica_listener_process_request_mock(const Scenario* s) {
    return s->process_request_result;
}

// ALT: mntm-012-Stand / Unleashed-dev vor PR #4465 (byte-identisch gegen frische
// Zweitkopie verifiziert, siehe Report) -- `command` wird in jedem Pfad AUSSER dem
// Erfolgsfall und dem einen Spezialfall FelicaErrorFeatureUnsupported auf Continue
// (Default) belassen.
static NfcCommand felica_listener_run_rxend_ALT(const Scenario* s) {
    NfcCommand command = NfcCommandContinue;
    do {
        if(!felica_crc_check_mock(s)) {
            break;
        }
        if(!felica_listener_check_block_list_size_mock(s)) {
            break;
        }
        if(s->is_polling_cmd) {
            if(!s->polling_is_hw_leak) {
                FelicaError error = felica_listener_process_system_code_mock(s);
                if(error == FelicaErrorFeatureUnsupported) {
                    command = NfcCommandReset;
                } else if(error != FelicaErrorNone) {
                    // nur geloggt
                }
                break;
            } else {
                // "Hardware Polling command leaking through" -- nur geloggt
                break;
            }
        } else if(!felica_listener_check_idm_mock(s)) {
            break;
        }
        FelicaError error = felica_listener_process_request_mock(s);
        (void)error; // nur geloggt
    } while(0);
    return command;
}

// NEU: eigener, minimaler Fix -- `error` wird auf JEDEM Pfad gesetzt, am Ende wird bei
// jedem error != FelicaErrorNone zurueckgesetzt, nicht nur bei FelicaErrorFeatureUnsupported.
static NfcCommand felica_listener_run_rxend_NEU(const Scenario* s) {
    NfcCommand command = NfcCommandContinue;
    FelicaError error = FelicaErrorNone;
    do {
        if(!felica_crc_check_mock(s)) {
            error = FelicaErrorWrongCrc;
            break;
        }
        if(!felica_listener_check_block_list_size_mock(s)) {
            error = FelicaErrorProtocol;
            break;
        }
        if(s->is_polling_cmd) {
            if(!s->polling_is_hw_leak) {
                error = felica_listener_process_system_code_mock(s);
                break;
            } else {
                error = FelicaErrorIgnore;
                break;
            }
        } else if(!felica_listener_check_idm_mock(s)) {
            error = FelicaErrorIgnore;
            break;
        }
        error = felica_listener_process_request_mock(s);
    } while(0);

    if(error != FelicaErrorNone) {
        command = NfcCommandReset;
    }
    return command;
}

typedef struct {
    const char* name;
    Scenario scenario;
    NfcCommand expected_neu; // was der Fix in diesem Fall liefern MUSS
} TestCase;

int main(void) {
    TestCase cases[] = {
        {"CRC falsch",
         {.crc_ok = false, .length_ok = true, .is_polling_cmd = false},
         NfcCommandReset},
        {"Laenge falsch",
         {.crc_ok = true, .length_ok = false, .is_polling_cmd = false},
         NfcCommandReset},
        {"Hardware-Polling-Leak",
         {.crc_ok = true,
          .length_ok = true,
          .is_polling_cmd = true,
          .polling_is_hw_leak = true},
         NfcCommandReset},
        {"Polling mit fremdem System-Code, nicht unterstuetzt",
         {.crc_ok = true,
          .length_ok = true,
          .is_polling_cmd = true,
          .polling_is_hw_leak = false,
          .system_code_result = FelicaErrorFeatureUnsupported},
         NfcCommandReset},
        {"Polling mit fremdem System-Code, erfolgreich beantwortet (NDEF/Lite-S)",
         {.crc_ok = true,
          .length_ok = true,
          .is_polling_cmd = true,
          .polling_is_hw_leak = false,
          .system_code_result = FelicaErrorNone},
         NfcCommandContinue},
        {"Falsche IDm (fremde/zurueckgesetzte Karte antwortet nicht)",
         {.crc_ok = true, .length_ok = true, .is_polling_cmd = false, .idm_ok = false},
         NfcCommandReset},
        {"Gueltiger Request, Verarbeitung/Frame-Exchange schlaegt fehl (z.B. Field Off "
         "waehrend TX)",
         {.crc_ok = true,
          .length_ok = true,
          .is_polling_cmd = false,
          .idm_ok = true,
          .process_request_result = FelicaErrorTimeout},
         NfcCommandReset},
        {"Gueltiger Request, erfolgreich verarbeitet",
         {.crc_ok = true,
          .length_ok = true,
          .is_polling_cmd = false,
          .idm_ok = true,
          .process_request_result = FelicaErrorNone},
         NfcCommandContinue},
    };
    size_t n = sizeof(cases) / sizeof(cases[0]);

    int mismatches_neu_vs_expected = 0;
    int alt_vs_neu_differs = 0;
    int alt_leaves_continue_on_rejection = 0;

    printf("%-70s %-10s %-10s %-10s\n", "Szenario", "ALT", "NEU", "erwartet");
    for(size_t i = 0; i < n; i++) {
        NfcCommand alt = felica_listener_run_rxend_ALT(&cases[i].scenario);
        NfcCommand neu = felica_listener_run_rxend_NEU(&cases[i].scenario);
        const char* alt_s = alt == NfcCommandReset ? "Reset" : "Continue";
        const char* neu_s = neu == NfcCommandReset ? "Reset" : "Continue";
        const char* exp_s = cases[i].expected_neu == NfcCommandReset ? "Reset" : "Continue";
        printf("%-70s %-10s %-10s %-10s\n", cases[i].name, alt_s, neu_s, exp_s);

        if(neu != cases[i].expected_neu) mismatches_neu_vs_expected++;
        if(alt != neu) {
            alt_vs_neu_differs++;
            // Bug-Kriterium: ALT bleibt bei einer abgelehnten Anfrage (erwartet=Reset)
            // auf Continue stehen, obwohl kein Response gesendet wurde.
            if(cases[i].expected_neu == NfcCommandReset && alt == NfcCommandContinue) {
                alt_leaves_continue_on_rejection++;
            }
        }
    }

    printf("\nALT weicht von NEU ab in %d/%zu Faellen.\n", alt_vs_neu_differs, n);
    printf(
        "Davon ALT=Continue bei abgelehnter Anfrage (= der 'Hang'-Bug): %d Faelle.\n",
        alt_leaves_continue_on_rejection);
    printf(
        "NEU weicht vom erwarteten Zielverhalten ab: %d Faelle.\n", mismatches_neu_vs_expected);

    if(mismatches_neu_vs_expected != 0) {
        printf("ERGEBNIS: FEHLER -- NEU verhaelt sich nicht wie erwartet.\n");
        return 1;
    }
    if(alt_leaves_continue_on_rejection == 0) {
        printf("ERGEBNIS: FEHLER -- ALT-Bug im Repro nicht reproduziert.\n");
        return 1;
    }
    printf(
        "ERGEBNIS: bestaetigt -- ALT laesst %d von %zu abgelehnten Anfragen ohne Reset, "
        "NEU setzt in jedem dieser Faelle korrekt zurueck; der Erfolgsfall "
        "(Continue) bleibt in beiden Versionen unveraendert.\n",
        alt_leaves_continue_on_rejection,
        n);
    return 0;
}
