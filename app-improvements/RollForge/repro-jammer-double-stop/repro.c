// Host-kompilierbares Modell des Jammer-Modul-Bugs (module_jammer.c, jm_input()
// InputKeyUp/InputKeyDown waehrend JM_JAMMING).
//
// Bildet nur den fuer den Bug relevanten Teil der subghz_devices-Zustandsmaschine
// nach: async_tx darf laut rf_stop_current()-Kommentar in rollforge_rf.c
// ("furi_hal prueft: stop_async_tx braucht state==AsyncTx ... Nur den passenden
// Stop aufrufen!") nur aufgerufen werden, wenn der Treiber aktuell wirklich in
// AsyncTx ist -- genau das prueft echtes furi_hal intern per furi_check() und
// bricht bei Verletzung hart ab (Crash/Reboot). assert() hier steht 1:1 fuer
// dieses furi_check().
#include <assert.h>
#include <stdio.h>

typedef enum { DEV_IDLE, DEV_ASYNC_TX } DevState;
typedef enum { RF_IDLE, RF_JAMMING } RfOp;

static DevState g_dev = DEV_IDLE;

// Modell von subghz_devices_stop_async_tx(): fordert state==AsyncTx (furi_check).
static void mock_stop_async_tx(void) {
    assert(g_dev == DEV_ASYNC_TX && "stop_async_tx auf nicht-AsyncTx-Geraet (furi_check-Abbruch)");
    g_dev = DEV_IDLE;
}

// Modell von subghz_devices_start_async_tx(): fordert state==Idle (furi_check).
static void mock_start_async_tx(void) {
    assert(g_dev == DEV_IDLE && "start_async_tx auf nicht-Idle-Geraet (furi_check-Abbruch)");
    g_dev = DEV_ASYNC_TX;
}

// Modell von rf_stop_current() (rollforge_rf.c): stoppt nur passend zu rf_op.
static void mock_rf_stop_current(RfOp rf_op) {
    if(rf_op == RF_JAMMING) mock_stop_async_tx();
}

// Modell von rollforge_rf_start_tx() (rollforge_rf.c).
static void mock_rollforge_rf_start_tx(RfOp rf_op) {
    mock_rf_stop_current(rf_op);
    mock_start_async_tx();
}

// Ausgangszustand fuer beide Varianten: Jammer laeuft bereits (JM_JAMMING),
// Geraet ist im Treiber tatsaechlich in AsyncTx -- Zustand direkt vor einem
// Tastendruck InputKeyUp/InputKeyDown zum Frequenzwechsel.
static void reset_to_jamming(void) {
    g_dev = DEV_IDLE;
    mock_start_async_tx(); // -> DEV_ASYNC_TX, entspricht laufendem Jamming
}

// Vorher: jm_input() InputKeyUp/Down, JM_JAMMING-Zweig ruft zusaetzlich zum
// internen Stop in rollforge_rf_start_tx() selbst noch direkt
// subghz_devices_stop_async_tx() auf -- doppelter Stop.
static void old_freq_switch(RfOp* rf_op) {
    mock_stop_async_tx();               // direkter Stop-Aufruf im Input-Handler
    mock_rollforge_rf_start_tx(*rf_op); // BUG: rf_stop_current() stoppt hier ein zweites Mal,
                                         // obwohl *rf_op noch RF_JAMMING ist und das Geraet
                                         // bereits Idle -- furi_check-Verletzung.
    *rf_op = RF_JAMMING;
}

// Nachher: der direkte Stop-Aufruf im Input-Handler entfaellt; der einzige
// Stop passiert innerhalb von rollforge_rf_start_tx() (rf_stop_current()),
// exakt einmal, waehrend das Geraet noch AsyncTx ist.
static void new_freq_switch(RfOp* rf_op) {
    mock_rollforge_rf_start_tx(*rf_op);
    *rf_op = RF_JAMMING;
}

int main(int argc, char** argv) {
    int use_fix = (argc > 1 && argv[1][0] == 'f');
    RfOp rf_op = RF_JAMMING;

    reset_to_jamming();
    printf("Start: Geraet=%s, rf_op=%s, Variante=%s\n",
           g_dev == DEV_ASYNC_TX ? "AsyncTx" : "Idle",
           rf_op == RF_JAMMING ? "JAMMING" : "IDLE",
           use_fix ? "nachher (Fix)" : "vorher (Bug)");

    if(use_fix)
        new_freq_switch(&rf_op);
    else
        old_freq_switch(&rf_op);

    printf("Nach Frequenzwechsel: Geraet=%s, rf_op=%s -- kein Abbruch.\n",
           g_dev == DEV_ASYNC_TX ? "AsyncTx" : "Idle",
           rf_op == RF_JAMMING ? "JAMMING" : "IDLE");
    return 0;
}
