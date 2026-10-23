    if(counter_up && prog_mode == PROG_MODE_OFF) {
        // Counter increment conditions. PULL data from subghz_block_generic_global first --
        // if the counter was overriden via the signal settings counter editor, use that value
        // as-is and skip the standard increment below entirely.
        if(subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
            // counter_override_get() already wrote the new value into instance->generic.cnt
        } else if(keeloq_counter_mode == 0) {
            // Check for OFEX (overflow experimental) mode
            if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {
                if((instance->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) > 0xFFFF) {
                    instance->generic.cnt = 0;
                } else {
                    instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
                }
            } else {
                if((instance->generic.cnt + 0x1) > 0xFFFF) {
                    instance->generic.cnt = 0;
                } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
                    instance->generic.cnt = 0xFFFE;
                } else {
                    instance->generic.cnt++;
                }
            }
        } else if(keeloq_counter_mode == 1) {
            // Mode 1
            // 0000 / 0001 / FFFE / FFFF
            if((instance->generic.cnt + 0x1) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
                instance->generic.cnt = 0xFFFE;
            } else {
                instance->generic.cnt++;
            }
        } else if(keeloq_counter_mode == 2) {
            // Mode 2
            // + 0x3333 each time
            if((instance->generic.cnt + 0x3333) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt += 0x3333;
            }
        } else if(keeloq_counter_mode == 3) {
            // Mode 3
            // 0x8006 / 0x8007 / 0x0006 / 0x0007
            if(instance->generic.cnt != 0x8006 && instance->generic.cnt != 0x8007 &&
               instance->generic.cnt != 0x0006) {
                instance->generic.cnt = 0x8006;
            } else if(instance->generic.cnt == 0x8007) {
                instance->generic.cnt = 0x0006;
            } else {
                instance->generic.cnt++;
            }

        } else if(keeloq_counter_mode == 4) {
            // Mode 4
            // 0x807B / 0x807C / 0x007B / 0x007C
            if(instance->generic.cnt != 0x807B && instance->generic.cnt != 0x807C &&
               instance->generic.cnt != 0x007B) {
                instance->generic.cnt = 0x807B;
            } else if(instance->generic.cnt == 0x807C) {
                instance->generic.cnt = 0x007B;
            } else {
                instance->generic.cnt++;
            }
        } else if(keeloq_counter_mode == 5) {
            // Mode 5
            // 0000 / FFFF
            if((instance->generic.cnt + 0x1) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt = 0xFFFF;
            }
        } else {
            // Mode 6 - Freeze counter
        }
    }
