#pragma once
/* generic.h includes <furi_hal.h> but generic.c/.h never call any furi_hal_*
   symbol. The extracted keeloq.c counter block (counter_block.inc) calls
   exactly one: furi_hal_subghz_get_rolling_counter_mult(). Host-side test
   control for that single function lives in repro.c (g_rolling_counter_mult),
   declared here so both repro.c and the extracted .inc fragment see the same
   symbol. */
#include <stdint.h>

extern int32_t g_rolling_counter_mult;
static inline int32_t furi_hal_subghz_get_rolling_counter_mult(void) {
    return g_rolling_counter_mult;
}
