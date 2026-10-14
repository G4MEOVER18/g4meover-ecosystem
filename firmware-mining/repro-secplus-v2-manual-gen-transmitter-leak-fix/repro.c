// Host-Repro: subghz_txrx_gen_secplus_v2_protocol() -- fehlender subghz_transmitter_free().
//
// Bildet applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c (mntm-012) 1:1
// nach: die Funktion alloziert einen reinen Wegwerf-Transmitter nur, um
// instance->fff_data per subghz_protocol_secplus_v2_create_data() zu befuellen, und gibt ihn
// im Erfolgsfall zurueck -- ALT gibt ihn dabei nie wieder frei. Alle sieben Schwesterfunktionen
// in derselben Datei (subghz_txrx_gen_keeloq_protocol(), _gen_keeloq_bft_protocol(),
// _gen_nice_flor_s_protocol(), _gen_faac_slh_protocol(), _gen_alutech_at_4n_protocol(),
// _gen_came_atomo_protocol(), _gen_somfy_telis_protocol(), _gen_phoenix_v2_protocol())
// alloziere denselben Wegwerf-Transmitter nach demselben Muster und geben ihn danach sofort
// wieder frei -- subghz_txrx_gen_secplus_v2_protocol() ist die einzige Ausnahme.
//
// Aufrufer (subghz_scene_set_counter.c / subghz_scene_set_type.c): jedes Mal, wenn der Nutzer
// in "Add Manually -> Security+ 2.0" den Zaehler/Button bestaetigt -- auch wiederholt, ohne die
// SubGhz-App dabei zu verlassen (zurueck zu "Set Type", erneut "Set Counter") --, ueberschreibt
// die Funktion instance->transmitter mit einer frischen Allokation; die vorherige wird dabei
// unerreichbar, ohne je freigegeben worden zu sein.
//
// Die echten Typen (SubGhzTransmitter, SubGhzTxRx, subghz_transmitter_alloc_init/_free,
// subghz_protocol_secplus_v2_create_data) stammen aus der SubGHz-Bibliothek und sind auf dem
// Host nicht verfuegbar -- hier durch echte malloc()/free()-Wrapper um einen Platzhalter
// derselben Rolle ersetzt, mit demselben Aufrufmuster wie im echten Code.
//
// Kompilieren/Ausfuehren (unter ASan mit aktiver Leak-Erkennung, Default unter Linux):
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
//   ./repro 5 alt   # erwartet: LeakSanitizer meldet 5 verlorene Bloecke, Exit != 0
//   ./repro 5 neu   # erwartet: keine Meldung, Exit 0
//
// ALT (unpatched): meldet N verlorene Bloecke (einer je simulierter "Zaehler bestaetigen"-Runde).
// NEU (patched): beendet sich sauber, keine verlorenen Bloecke.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// entspricht SubGhzTransmitter (lib/subghz/transmitter.c) -- opaker Typ, Inhalt fuer diesen
// Fund irrelevant.
typedef struct {
    unsigned dummy;
} TransmitterMock;

// entspricht struct SubGhzTxRx (nur das fuer diesen Fund relevante Feld).
typedef struct {
    TransmitterMock* transmitter;
} TxRxMock;

static TransmitterMock* transmitter_alloc_init_mock(void) {
    return calloc(1, sizeof(TransmitterMock));
}
static void transmitter_free_mock(TransmitterMock* instance) {
    if(!instance) abort();
    free(instance);
}
// entspricht subghz_protocol_secplus_v2_create_data(): befuellt instance->fff_data ueber den
// uebergebenen Transmitter -- Inhalt fuer diesen Fund irrelevant.
static void secplus_v2_create_data_mock(TransmitterMock* t, unsigned serial, unsigned btn, unsigned cnt) {
    (void)t;
    (void)serial;
    (void)btn;
    (void)cnt;
}

// --- ALT: verbatim Form von subghz_txrx_gen_secplus_v2_protocol() (mntm-012, unveraendert) ---
static int gen_secplus_v2_ALT(TxRxMock* instance, unsigned serial, unsigned btn, unsigned cnt) {
    int ret = 0;
    instance->transmitter = transmitter_alloc_init_mock();
    // subghz_txrx_set_preset(...) hier ausgelassen: aendert nur instance->preset, fuer die
    // Lebensdauer von instance->transmitter irrelevant.
    if(instance->transmitter) {
        secplus_v2_create_data_mock(instance->transmitter, serial, btn, cnt);
        ret = 1;
    }
    return ret; // <-- fehlendes subghz_transmitter_free(instance->transmitter) hier
}

// --- NEU: dieselbe Form, mit dem fehlenden Free ergaenzt (Port des Upstream-Fixes) ---
static int gen_secplus_v2_NEU(TxRxMock* instance, unsigned serial, unsigned btn, unsigned cnt) {
    int ret = 0;
    instance->transmitter = transmitter_alloc_init_mock();
    if(instance->transmitter) {
        secplus_v2_create_data_mock(instance->transmitter, serial, btn, cnt);
        ret = 1;
    }
    transmitter_free_mock(instance->transmitter); // NEU
    return ret;
}

int main(int argc, char** argv) {
    // Simuliert einen Nutzer, der in "Add Manually -> Security+ 2.0" wiederholt den
    // Zaehler/Button bestaetigt, ohne die SubGhz-App dabei zu verlassen -- dieselbe
    // langlebige SubGhzTxRx-Instanz wird dabei wiederverwendet, exakt wie im echten Code.
    int rounds = (argc > 1) ? atoi(argv[1]) : 5;
    int use_neu = (argc > 2) && (strcmp(argv[2], "neu") == 0);

    TxRxMock instance = {0};
    for(int i = 0; i < rounds; i++) {
        int ok = use_neu ? gen_secplus_v2_NEU(&instance, 0x12345678u, 1u, (unsigned)i)
                          : gen_secplus_v2_ALT(&instance, 0x12345678u, 1u, (unsigned)i);
        if(!ok) {
            fprintf(stderr, "Generierung unerwartet fehlgeschlagen (Runde %d)\n", i);
            return 2;
        }
    }

    printf(
        "Simulierte %d Runden \"Add Manually -> Security+ 2.0 -> Zaehler bestaetigen\" (%s).\n",
        rounds,
        use_neu ? "NEU, Fix angewendet" : "ALT, unveraendert");
    printf("Beende jetzt -- ASan LeakSanitizer meldet alles, was noch alloziert ist.\n");
    return 0;
}
