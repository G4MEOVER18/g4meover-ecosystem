/* NEU: same preset-selection logic after the port fix (app_subghz.c),
 * routed through the still-present subghz_devices_load_preset()/
 * subghz_devices_get_by_name() device-abstraction API instead of the
 * removed furi_hal_subghz_load_preset(). */
#include "modulations.h"

RadioCallLog g_log;

void radio_begin(int modulation) {
    if(ProtoViewModulations[modulation].custom == NULL) {
        subghz_devices_load_preset(
            subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME),
            ProtoViewModulations[modulation].preset,
            NULL);
    } else {
        furi_hal_subghz_load_custom_preset(ProtoViewModulations[modulation].custom);
    }
}
