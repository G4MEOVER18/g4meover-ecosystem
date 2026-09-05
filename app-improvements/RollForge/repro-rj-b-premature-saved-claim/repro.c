// Host-kompilierbares Modell des RollJam-Status-/Speicher-Bugs (module_rolljam.c,
// Uebergang RJ_P2_RX -> RJ_REPLAY in rj_tick()).
//
// Bildet nur den fuer den Bug relevanten Ausschnitt nach: den Moment, in dem
// Signal B fertig erfasst ist (m->sig_b.ready) und die Statuszeile
// "Replaying A... (B saved)" gesetzt wird, sowie den direkt danach moeglichen
// Abbruch-Pfad (Taste Back waehrend RJ_REPLAY -> mod_leave() -> rj_exit() ->
// rj_rf_clean(), die KEINEN Speicheraufruf enthaelt). rollforge_save_sig()
// wird hier durch mock_save_B() ersetzt (setzt/liest ein "Datei auf SD-Karte
// vorhanden"-Flag statt echtem storage_file_*), der Rest der Kontrollfluss-
// Reihenfolge ist 1:1 aus module_rolljam.c uebernommen.
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef enum { RJ_P2_RX, RJ_REPLAY, RJ_DONE, RJ_IDLE } RjPhase;

// Modell der SD-Karte: true == "rj_B_*.sub" liegt tatsaechlich im Dateisystem.
static bool disk_has_B = false;
static char last_status[64];

// Modell von rollforge_save_sig(&m->sig_b, ..., "rj_B"). sd_ok simuliert eine
// vorhandene (true) bzw. fehlende/vollgeschriebene (false) SD-Karte.
static bool mock_save_B(bool sd_ok) {
    if(sd_ok) disk_has_B = true;
    return sd_ok;
}

// Modell von rj_exit()/rj_rf_clean(): stoppt nur RF, schreibt nichts auf die
// SD-Karte. Entspricht dem Code, der bei [Back] waehrend RJ_REPLAY laeuft.
static RjPhase on_back_abort(void) {
    return RJ_IDLE;
}

// Vorher: Uebergang RJ_P2_RX -> RJ_REPLAY setzt die Statuszeile
// "Replaying A... (B saved)", ohne dass zu diesem Zeitpunkt irgendein
// rollforge_save_sig()-Aufruf fuer sig_b stattgefunden hat -- die einzige
// Stelle, die B tatsaechlich speichert, ist der optionale [v]-Tastendruck in
// RJ_DONE bzw. RJ_CAPTURED (rj_input(), InputKeyDown), den der Nutzer erst
// nach Abschluss von RJ_REPLAY erreichen kann.
static RjPhase old_on_p2rx_ready(void) {
    snprintf(last_status, sizeof(last_status), "Replaying A... (B saved)");
    return RJ_REPLAY;
}

// Nachher: sig_b wird exakt an dieser Stelle (P2_RX -> REPLAY, noch vor jeder
// Moeglichkeit fuer den Nutzer, [Back] zu druecken) tatsaechlich gespeichert;
// die Statuszeile spiegelt das reale Ergebnis wider statt es zu unterstellen.
static RjPhase new_on_p2rx_ready(bool sd_ok) {
    bool ok = mock_save_B(sd_ok);
    snprintf(last_status, sizeof(last_status),
        ok ? "Replaying A... (B saved)" : "Replaying A... (B SAVE FAILED!)");
    return RJ_REPLAY;
}

int main(void) {
    int failures = 0;

    // Szenario 1: SD-Karte vorhanden, Nutzer bricht sofort nach Erreichen von
    // RJ_REPLAY ab (druckt [Back], noch waehrend "Replaying A..." angezeigt wird).
    {
        disk_has_B = false;
        RjPhase phase = old_on_p2rx_ready();
        printf("ALT  (SD ok, sofortiger Abbruch): status=\"%s\"", last_status);
        phase = on_back_abort();
        (void)phase;
        printf(", disk_has_B=%d (Status behauptete \"B saved\")\n", disk_has_B);
        if(disk_has_B) {
            printf("  UNERWARTET: ALT haette B nicht speichern duerfen.\n");
            failures++;
        }
    }

    // Dasselbe Szenario mit dem Fix: Speicherung passiert VOR dem Statustext
    // und vor jeder Moeglichkeit zum Abbruch.
    {
        disk_has_B = false;
        RjPhase phase = new_on_p2rx_ready(true);
        printf("NEU  (SD ok, sofortiger Abbruch): status=\"%s\"", last_status);
        phase = on_back_abort();
        (void)phase;
        printf(", disk_has_B=%d\n", disk_has_B);
        if(!disk_has_B) {
            printf("  FEHLER: NEU haette B bereits gespeichert haben muessen.\n");
            failures++;
        }
    }

    // Regressionsfall: Nutzer wartet den Replay ab (kein fruehzeitiger Abbruch,
    // RJ_DONE wird erreicht) -- Fix darf normalen Ablauf nicht veraendern.
    {
        disk_has_B = false;
        RjPhase phase = new_on_p2rx_ready(true);
        assert(phase == RJ_REPLAY);
        phase = RJ_DONE; // Replay laeuft vollstaendig durch, siehe rj_tick() RJ_REPLAY-Zweig
        printf("NEU  (SD ok, kein Abbruch, RJ_DONE erreicht): disk_has_B=%d\n", disk_has_B);
        if(!disk_has_B) {
            printf("  FEHLER: Regressionsfall gebrochen.\n");
            failures++;
        }
    }

    // Fehlerfall: SD-Karte fehlt/voll -- Fix meldet das ehrlich statt "B saved"
    // zu behaupten (ALT haette in diesem Fall exakt denselben falschen Text
    // gezeigt wie im SD-ok-Fall, da ALT den Ruckgabewert gar nicht kennt).
    {
        disk_has_B = false;
        new_on_p2rx_ready(false);
        printf("NEU  (SD fehlt): status=\"%s\", disk_has_B=%d\n", last_status, disk_has_B);
        if(disk_has_B) {
            printf("  FEHLER: disk_has_B haette false bleiben muessen.\n");
            failures++;
        }
        bool says_failed = (strstr(last_status, "FAILED") != NULL);
        if(!says_failed) {
            printf("  FEHLER: Statustext haette den SD-Fehler melden muessen.\n");
            failures++;
        }
    }

    printf(failures == 0 ? "\nAlle Faelle wie erwartet.\n" : "\n%d Faelle abweichend!\n", failures);
    return failures == 0 ? 0 : 1;
}
