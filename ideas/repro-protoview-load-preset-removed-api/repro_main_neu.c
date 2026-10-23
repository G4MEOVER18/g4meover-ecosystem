/* Host repro for the ProtoView "furi_hal_subghz_load_preset() removed"
 * port finding. Exercises radio_begin() for all nine
 * ProtoViewModulations[] entries and checks, for the NEU build, that:
 *   - the four standard presets (custom == NULL) go through
 *     subghz_devices_load_preset() with the correct device pointer and
 *     the correct FuriHalSubGhzPreset enum value, no preset_data,
 *   - the five custom presets (custom != NULL) still go through the
 *     unchanged furi_hal_subghz_load_custom_preset() with the exact
 *     same pointer as before the fix,
 * i.e. the fix changes *how* the standard presets reach the radio, not
 * *which* preset ends up selected for any of the nine modulations. */
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
            /* Standard preset: must have gone through the device API,
             * exactly once, with the real cc1101_int device and the
             * untouched preset_data). */
            assert(g_log.device_preset_calls == 1);
            assert(g_log.custom_preset_calls == 0);
            assert(g_log.last_device == &g_cc1101_int_device);
            assert(g_log.last_device_preset == ProtoViewModulations[m].preset);
            assert(g_log.last_device_preset_data == NULL);
            checks += 5;
        } else {
            /* Custom preset: unchanged call, same pointer as the table. */
            assert(g_log.custom_preset_calls == 1);
            assert(g_log.device_preset_calls == 0);
            assert(g_log.last_custom_preset == ProtoViewModulations[m].custom);
            checks += 3;
        }
    }

    /* subghz_devices_get_by_name() must resolve the real registered
     * device name and reject anything else (guards against a silent
     * typo in SUBGHZ_DEVICE_CC1101_INT_NAME breaking the lookup). */
    assert(subghz_devices_get_by_name("cc1101_int") == &g_cc1101_int_device);
    assert(subghz_devices_get_by_name("cc1101_ext") == NULL);
    checks += 2;

    printf("ALL CHECKS PASSED (%d assertions over %u modulations)\n", checks, MODULATIONS_COUNT);
    return 0;
}
