// Host-Repro: Cursor-Reset-Logik aus archive_browser.c::archive_list_item_cb()
//
// Bildet ausschließlich den Kontrollfluss ab, der sich zwischen ALT und NEU unterscheidet:
// die Platzierung von "if(model->item_idx < 0) model->item_idx = 0;" relativ zum
// "if(model->item_cnt <= BROWSER_SORT_THRESHOLD)"-Block. Die Sortier-/Pfadsuch-Schleife davor
// ist für diesen Vergleich irrelevant, weil sie item_idx nur verändert, wenn item_idx bereits
// >= 0 war (dann bleibt item_idx in beiden Varianten identisch) -- der Fund betrifft exakt den
// Fall item_idx < 0, in dem "selected" NULL bleibt und die Schleife nichts tut.
//
// BROWSER_SORT_THRESHOLD = 220 (applications/services/gui/modules/file_browser_worker.h,
// Next-Flip/Momentum-Firmware, unverändert seit mntm-012).

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define BROWSER_SORT_THRESHOLD 220

// ALT: mntm-012-Stand -- Reset nur INNERHALB des Sortier-Blocks.
static int32_t cursor_reset_old(int32_t item_idx, uint32_t item_cnt) {
    if(item_cnt <= BROWSER_SORT_THRESHOLD) {
        // (Sortier-/Pfadsuch-Schleife hier ausgelassen -- siehe Kommentar oben)
        if(item_idx < 0) {
            item_idx = 0;
        }
    }
    return item_idx;
}

// NEU: Reset AUSSERHALB des Sortier-Blocks -- läuft immer, unabhängig von item_cnt.
static int32_t cursor_reset_new(int32_t item_idx, uint32_t item_cnt) {
    if(item_cnt <= BROWSER_SORT_THRESHOLD) {
        // (Sortier-/Pfadsuch-Schleife hier ausgelassen -- siehe Kommentar oben)
    }
    if(item_idx < 0) {
        item_idx = 0;
    }
    return item_idx;
}

int main(void) {
    long checks = 0;
    long bug_reproduced = 0;

    for(int32_t item_idx = -5; item_idx <= 5; item_idx++) {
        for(uint32_t item_cnt = 0; item_cnt <= 500; item_cnt++) {
            int32_t old_result = cursor_reset_old(item_idx, item_cnt);
            int32_t new_result = cursor_reset_new(item_idx, item_cnt);
            checks++;

            if(item_cnt <= BROWSER_SORT_THRESHOLD) {
                // Regressionsfall: bei <= 220 Einträgen muss NEU sich exakt wie ALT verhalten.
                assert(old_result == new_result);
            } else {
                // Fundfall: bei > 220 Einträgen.
                if(item_idx < 0) {
                    // ALT: Bug reproduziert -- Cursor bleibt negativ (item_idx unverändert).
                    assert(old_result == item_idx);
                    assert(old_result < 0);
                    // NEU: Fix greift -- Cursor wird auf 0 zurückgesetzt.
                    assert(new_result == 0);
                    bug_reproduced++;
                } else {
                    // Kein Regressionsfall für bereits gültige (nicht-negative) Cursor-Werte:
                    // ALT und NEU verhalten sich identisch (Reset greift ohnehin nicht).
                    assert(old_result == new_result);
                    assert(old_result == item_idx);
                }
            }
        }
    }

    printf(
        "checks=%ld bug_reproduced=%ld (item_cnt>%d && item_idx<0) -- ALLE CHECKS BESTANDEN\n",
        checks,
        bug_reproduced,
        BROWSER_SORT_THRESHOLD);
    return 0;
}
