/* Host repro, ALT side only: confirms radio_begin_ALT.c's unmodified
 * upstream control flow is a faithful extraction of app_subghz.c (it
 * calls the old enum-based furi_hal_subghz_load_preset() for the four
 * standard presets, and the unchanged furi_hal_subghz_load_custom_preset()
 * for the five custom ones) -- only buildable here against mocks.h with
 * PROTOVIEW_REPRO_PROVIDE_REMOVED_SYMBOL=1, i.e. an older firmware that
 * still has the removed function. Against my own firmware headers
 * (PROVIDE_REMOVED_SYMBOL=0) this same radio_begin_ALT.c fails to build
 * at all -- see build.sh. */
#include "modulations.h"
#include <stdio.h>
#include <assert.h>

void radio_begin(int modulation);

int main(void) {
    int checks = 0;

    for(size_t m = 0; m < MODULATIONS_COUNT; m++) {
        g_log = (RadioCallLog){0};
        radio_begin((int)m);

        if(ProtoViewModulations[m].custom == NULL) {
            assert(g_log.old_load_preset_calls == 1);
            assert(g_log.custom_preset_calls == 0);
            assert(g_log.last_old_preset == ProtoViewModulations[m].preset);
            checks += 3;
        } else {
            assert(g_log.custom_preset_calls == 1);
            assert(g_log.old_load_preset_calls == 0);
            assert(g_log.last_custom_preset == ProtoViewModulations[m].custom);
            checks += 3;
        }
    }

    printf(
        "ALT ALL CHECKS PASSED (%d assertions over %u modulations, old-firmware headers only)\n",
        checks,
        MODULATIONS_COUNT);
    return 0;
}
