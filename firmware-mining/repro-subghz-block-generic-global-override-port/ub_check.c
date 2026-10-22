/* Separate, deliberately-UB probe: calls
   subghz_block_generic_global_counter_override_get() with cnt_length_bit
   still at its post-reset default of 0 -- the state before ANY real
   protocol's get_string()/encoder path has run. Every real call site in
   Unleashed pairs the override _get() with an earlier, unconditional write
   of cnt_length_bit/btn_length_bit (see the port-proposal doc's "Risiko"
   section for the full list of call sites checked), so this state is
   unreachable through the real protocol contract today -- this probe only
   demonstrates what happens if that implicit contract is ever broken by a
   future caller, under UBSan. Expected: the process aborts with a
   "shift exponent 32 is too large" diagnostic; this binary is run from the
   shell and its exit status is checked, it is not part of the main
   repro.c "all checks passed" count. */
#include <stdint.h>
#include <string.h>

#include "lib/subghz/blocks/generic.h"

int main(void) {
    memset(&subghz_block_generic_global, 0, sizeof(subghz_block_generic_global));
    /* cnt_length_bit left at 0 -- contract violation */
    subghz_block_generic_global_counter_override_set(0xFFFFFFFFu);
    uint32_t cnt = 0;
    subghz_block_generic_global_counter_override_get(&cnt);
    return 0;
}
