// Host-Repro: ruft die echte (unveraenderte, nur umbenannte) Funktion
// rollforge_save_sig() aus rollforge_storage.c zweimal mit identischer
// Systemzeit auf -- genau das Szenario "Save"-Taste zweimal innerhalb
// derselben Sekunde gedrueckt (RJ_CAPTURED/RJ_DONE in module_rolljam.c,
// RL_ANALYZE_VIEW/RL_REPLAY_READY in module_rolllab.c). ALT = Stand vor
// dem Fix, NEU = Stand danach. Keine Flipper-Hardware/kein furi noetig --
// nur die hier tatsaechlich benutzten Storage-/RTC-Symbole sind gemockt
// (siehe mocks.h).
//
// Bauen: gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined
//   -fno-sanitize-recover=undefined -I. -o repro
//   mocks.c rollforge_storage_ALT.c rollforge_storage_NEU.c repro_main.c
#include "mocks.h"
#include "rollforge_storage_ALT.h"
#include "rollforge_storage_NEU.h"
#include <assert.h>
#include <stdio.h>

static RfSig make_sig(uint32_t n_edges, uint32_t base_us) {
    RfSig s = {0};
    for(uint32_t i = 0; i < n_edges; i++) {
        s.buf[i] = (i % 2 == 0 ? 0x80000000UL : 0UL) | (base_us + i);
    }
    s.count = n_edges;
    s.ready = true;
    return s;
}

int main(void) {
    // Szenario 1: zwei verschiedene Aufnahmen (sig1 != sig2), "Save" beide
    // Mal innerhalb derselben Sekunde gedrueckt (gleiche Stunde/Minute/
    // Sekunde, gleicher Hint "rl_ref" -- wie module_rolllab.c es tatsaechlich
    // tut: RL_ANALYZE_VIEW und RL_REPLAY_READY rufen beide mit "rl_ref" auf).
    {
        fs_reset();
        g_mock_now = (DateTime){.hour = 14, .minute = 7, .second = 22};

        RfSig sig1 = make_sig(20, 1000);  // erste Aufnahme, 20 Flanken, erkennbar an Basis 1000
        RfSig sig2 = make_sig(40, 9000);  // zweite, spaeter gespeicherte Aufnahme, Basis 9000

        bool ok1 = rollforge_save_sig_ALT(&sig1, 433920000U, "rl_ref");
        bool ok2 = rollforge_save_sig_ALT(&sig2, 433920000U, "rl_ref");
        int alt_files_after = g_fs_count;
        bool alt_first_still_present = false;
        for(int i = 0; i < g_fs_count; i++) {
            if(g_fs[i].len > 0 && strstr(g_fs[i].data, "1000 -1001 1002")) alt_first_still_present = true;
        }

        fs_reset();
        g_mock_now = (DateTime){.hour = 14, .minute = 7, .second = 22};
        bool n_ok1 = rollforge_save_sig_NEU(&sig1, 433920000U, "rl_ref");
        bool n_ok2 = rollforge_save_sig_NEU(&sig2, 433920000U, "rl_ref");
        int neu_files_after = g_fs_count;
        bool neu_both_present = fs_count_with_prefix("/ext/subghz/rollforge/rl_ref_140722") == 2;

        printf("Szenario 1 (zwei Saves, exakt dieselbe Sekunde, gleicher Hint):\n");
        printf("  ALT: ok1=%d ok2=%d Dateien danach=%d (1 heisst: zweite hat erste ueberschrieben)\n",
            ok1, ok2, alt_files_after);
        printf("  NEU: ok1=%d ok2=%d Dateien danach=%d, beide vorhanden=%d\n",
            n_ok1, n_ok2, neu_files_after, neu_both_present);

        assert(ok1 && ok2);
        assert(alt_files_after == 1);           // Bug: nur noch eine Datei auf der SD-Karte
        assert(alt_first_still_present == false); // Bug: Inhalt der ersten Aufnahme ist weg

        assert(n_ok1 && n_ok2);
        assert(neu_files_after == 2);            // Fix: beide Aufnahmen tatsaechlich erhalten
        assert(neu_both_present == true);
    }

    // Szenario 2 (Regression): nur ein Save in dieser Sekunde -- Pfad bleibt
    // exakt wie vorher (kein unnoetiger "_1"-Suffix fuer den Normalfall).
    {
        fs_reset();
        g_mock_now = (DateTime){.hour = 9, .minute = 3, .second = 5};
        RfSig sig = make_sig(16, 400);

        bool alt_ok = rollforge_save_sig_ALT(&sig, 433920000U, "rj_A");
        char alt_path[256];
        strncpy(alt_path, g_fs[0].path, sizeof(alt_path));

        fs_reset();
        bool neu_ok = rollforge_save_sig_NEU(&sig, 433920000U, "rj_A");
        char neu_path[256];
        strncpy(neu_path, g_fs[0].path, sizeof(neu_path));

        printf("Szenario 2 (Regression, nur ein Save -> unveraenderter Pfad):\n");
        printf("  ALT-Pfad: %s\n  NEU-Pfad: %s\n", alt_path, neu_path);

        assert(alt_ok && neu_ok);
        assert(strcmp(alt_path, "/ext/subghz/rollforge/rj_A_090305.sub") == 0);
        assert(strcmp(neu_path, alt_path) == 0); // identisch zum Normalfall-Pfad vor dem Fix
    }

    // Szenario 3: drei Saves in derselben Sekunde (z.B. "Save" dreimal
    // gedrueckt) -- NEU muss alle drei getrennt behalten, nicht nur zwei.
    {
        fs_reset();
        g_mock_now = (DateTime){.hour = 18, .minute = 40, .second = 59};
        RfSig a = make_sig(16, 500), b = make_sig(17, 600), c = make_sig(18, 700);

        rollforge_save_sig_NEU(&a, 433920000U, "rj_B");
        rollforge_save_sig_NEU(&b, 433920000U, "rj_B");
        rollforge_save_sig_NEU(&c, 433920000U, "rj_B");
        int n = fs_count_with_prefix("/ext/subghz/rollforge/rj_B_184059");

        printf("Szenario 3 (drei Saves, dieselbe Sekunde): Dateien=%d\n", n);
        assert(n == 3);
    }

    // Szenario 4 (Regression): verschiedene Hints kollidieren nicht
    // (sig_a->"rj_A", sig_b->"rj_B" -- wie module_rolljam.c es nutzt).
    {
        fs_reset();
        g_mock_now = (DateTime){.hour = 12, .minute = 0, .second = 0};
        RfSig a = make_sig(16, 800), b = make_sig(16, 850);
        rollforge_save_sig_NEU(&a, 433920000U, "rj_A");
        rollforge_save_sig_NEU(&b, 433920000U, "rj_B");
        printf("Szenario 4 (Regression, unterschiedliche Hints): Dateien=%d\n", g_fs_count);
        assert(g_fs_count == 2);
    }

    printf("\nALL CHECKS PASSED (0 Fehlschlaege)\n");
    return 0;
}
