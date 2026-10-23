/* ALT: ProtoView's current upstream radio_begin() preset-selection logic
 * (app_subghz.c, verbatim control flow), unmodified. Builds fine against
 * real/older Flipper firmware headers that still provide
 * furi_hal_subghz_load_preset(), but fails against my own G4MEOVER-FW
 * (b359f6f) headers, which don't -- see build.sh "ALT" step. */
#include "modulations.h"

RadioCallLog g_log;

void radio_begin(int modulation) {
    if(ProtoViewModulations[modulation].custom == NULL) {
        furi_hal_subghz_load_preset(ProtoViewModulations[modulation].preset);
    } else {
        furi_hal_subghz_load_custom_preset(ProtoViewModulations[modulation].custom);
    }
}
