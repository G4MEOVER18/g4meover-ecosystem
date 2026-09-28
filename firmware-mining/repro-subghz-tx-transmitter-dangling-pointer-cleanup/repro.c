// Host-Repro: subghz_txrx_tx_start() Fehlerpfad -- dangling instance->transmitter nach Free.
//
// Bildet den exakten Ausschnitt aus applications/main/subghz/helpers/subghz_txrx.c (mntm-012)
// nach: lib/subghz/transmitter.c's SubGhzTransmitter (Struct mit protocol_instance-Zeiger),
// subghz_transmitter_free() (free(protocol_instance); free(instance);),
// subghz_transmitter_get_protocol_instance() (nur ein Non-NULL-"furi_check(instance)", keine
// Gueltigkeitspruefung), sowie den Fehlerpfad in subghz_txrx_tx_start():
//   if(ret != SubGhzTxRxStartTxStateOk) {
//       if(instance->transmitter) subghz_transmitter_free(instance->transmitter);   // ALT
//       ...
//   }
// gegenueber dem Port (NEU): zusaetzlich instance->transmitter = NULL; nach dem Free, exakt wie
// Unleashed-dev es fuer PR #1104 beschreibt ("the transmitter pointer is cleared after every
// teardown instead of being left dangling").
//
// Nachgebauter Aufrufer: subghz_txrx_set_raw_file_encoder_worker_callback_end() (dieselbe Datei)
// greift ebenfalls nur ueber instance->transmitter zu und ruft
// subghz_transmitter_get_protocol_instance(instance->transmitter) auf -- exakt derselbe
// "nur Non-NULL, keine Gueltigkeit geprueft"-Zugriffspfad. In der heutigen Verdrahtung wird dieser
// Aufrufer nur nach einem *erfolgreichen* subghz_tx_start() erreicht (siehe
// subghz_scene_read_raw.c), weshalb dieser Fund kein heute auf einem bekannten Aufrufpfad
// ausloesbarer Absturz ist -- die Portierung ist Haertung/Konsistenz mit dem Upstream-Fix, nicht
// die Behebung eines bestaetigten aktiven Exploits. Dieses Repro zeigt stattdessen die Klasse von
// Fehler, vor der die Portierung schuetzt: sobald IRGENDEIN Aufrufer (heutiger oder kuenftiger)
// instance->transmitter nach einem fehlgeschlagenen tx_start() liest, ohne zusaetzlich den
// txrx_state zu pruefen -- exakt das Zugriffsmuster, das der bestehende
// subghz_txrx_set_raw_file_encoder_worker_callback_end()-Aufrufer bereits verwendet --, liest ALT
// freigegebenen Heap-Speicher, NEU nicht (der NULL-Zeiger wird stattdessen kontrolliert
// uebersprungen, exakt wie der bestehende `if(instance->transmitter)`-Wächter es an anderer
// Stelle bereits tut).
//
// Kompilieren/Ausfuehren:
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// ALT: Kind-Prozess (fork) stirbt durch ASan "heap-use-after-free" (WIFSIGNALED/non-zero Exit).
// NEU: Kind-Prozess beendet sich sauber, keine ASan-Meldung.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { StateOk, StateErrorOnlyRx, StateErrorParserOthers } TxStartState;

typedef enum { TxRxIDLE, TxRxTx } TxRxState;

// entspricht SubGhzProtocolEncoderBase* protocol_instance (hier: ein einfacher Nutzdaten-Block,
// wie ihn jeder echte *_encoder_alloc() liefert)
typedef struct {
    uint32_t magic;
    uint32_t counter;
} ProtocolInstance;

// entspricht struct SubGhzTransmitter (lib/subghz/transmitter.c)
typedef struct {
    ProtocolInstance* protocol_instance;
} Transmitter;

// entspricht struct SubGhzTxRx (nur die fuer diesen Fund relevanten Felder)
typedef struct {
    Transmitter* transmitter;
    TxRxState txrx_state;
} TxRx;

static Transmitter* transmitter_alloc_init(void) {
    Transmitter* t = malloc(sizeof(Transmitter));
    t->protocol_instance = malloc(sizeof(ProtocolInstance));
    t->protocol_instance->magic = 0xC0FFEE;
    t->protocol_instance->counter = 0;
    return t;
}

// entspricht subghz_transmitter_free() 1:1 (furi_check(instance) hier als einfacher abort()
// bei NULL nachgebildet, wie im echten Code -- wird in diesem Repro nie mit NULL aufgerufen)
static void transmitter_free(Transmitter* instance) {
    if(!instance) abort();
    free(instance->protocol_instance);
    free(instance);
}

// entspricht subghz_transmitter_get_protocol_instance() 1:1: nur ein Non-NULL-Check, keine
// Gueltigkeitspruefung des Zeigers selbst.
static ProtocolInstance* transmitter_get_protocol_instance(Transmitter* instance) {
    if(!instance) abort();
    return instance->protocol_instance;
}

// entspricht dem Fehlerpfad in subghz_txrx_tx_start(): simuliert den "ErrorOnlyRx"-Fall
// (Transmitter erfolgreich alloziert/deserialisiert, aber die abschliessende Sendefreigabe
// schlaegt fehl -- Regionssperre/keine TX-Hardware verfuegbar) und den anschliessenden Cleanup.
static TxStartState tx_start(TxRx* instance, int simulate_tx_enable_ok, int with_null_reset) {
    instance->transmitter = transmitter_alloc_init();

    TxStartState ret;
    if(simulate_tx_enable_ok) {
        ret = StateOk;
        instance->txrx_state = TxRxTx;
    } else {
        ret = StateErrorOnlyRx;
    }

    if(ret != StateOk) {
        if(instance->transmitter) {
            transmitter_free(instance->transmitter);
            if(with_null_reset) {
                instance->transmitter = NULL; // NEU
            }
            // ALT: instance->transmitter bleibt auf den soeben freigegebenen Speicher zeigend
        }
        if(instance->txrx_state != TxRxIDLE) {
            instance->txrx_state = TxRxIDLE;
        }
    }
    return ret;
}

// entspricht subghz_txrx_set_raw_file_encoder_worker_callback_end(): greift nur ueber
// instance->transmitter zu, ohne txrx_state zu pruefen -- exakt derselbe Zugriffspfad wie im
// echten Code (helpers/subghz_txrx.c).
static void accessor_reads_transmitter(TxRx* instance) {
    if(instance->transmitter) {
        ProtocolInstance* p = transmitter_get_protocol_instance(instance->transmitter);
        // liest/schreibt durch den (bei ALT bereits freigegebenen) Zeiger --
        // exakt das Verhalten von subghz_protocol_raw_file_encoder_worker_set_callback_end(),
        // das seinerseits in das protocol_instance-Objekt schreibt.
        p->counter += 1;
        printf("  accessor read: magic=0x%x counter=%u\n", p->magic, p->counter);
    } else {
        printf("  accessor read: transmitter == NULL, korrekt uebersprungen\n");
    }
}

static int run_case(const char* label, int with_null_reset) {
    pid_t pid = fork();
    if(pid == 0) {
        TxRx instance = {0};
        instance.txrx_state = TxRxIDLE;

        // 1) fehlgeschlagener Sendeversuch (ErrorOnlyRx) -- exakt der Pfad aus dem Fund
        TxStartState st = tx_start(&instance, /*simulate_tx_enable_ok=*/0, with_null_reset);
        if(st != StateErrorOnlyRx) abort();

        // 2) irgendein Aufrufer liest instance->transmitter danach, ohne txrx_state zu pruefen
        //    (exakt das Muster von subghz_txrx_set_raw_file_encoder_worker_callback_end())
        accessor_reads_transmitter(&instance);

        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    int crashed = WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    printf("[%s] %s\n", label, crashed ? "ABGESTUERZT (wie erwartet)" : "sauber beendet");
    return crashed;
}

int main(void) {
    printf("== ALT (kein instance->transmitter = NULL nach Free) ==\n");
    int alt_crashed = run_case("ALT", /*with_null_reset=*/0);

    printf("== NEU (instance->transmitter = NULL nach Free) ==\n");
    int neu_crashed = run_case("NEU", /*with_null_reset=*/1);

    printf("\n");
    if(alt_crashed && !neu_crashed) {
        printf("ALLE CHECKS BESTANDEN: ALT liest nachweislich freigegebenen Speicher "
               "(heap-use-after-free), NEU nicht.\n");
        return 0;
    }
    printf("FEHLGESCHLAGEN: erwartetes Verhalten nicht bestaetigt (ALT crashed=%d, NEU crashed=%d).\n",
           alt_crashed,
           neu_crashed);
    return 1;
}
