    // if we change counter/button in the signal settings menu, force the standard/OFEX branch
    // below to run this pass even in counter_mode 1/2/3, so the override actually takes effect
    if(subghz_block_generic_global.cnt_need_override || subghz_block_generic_global.btn_need_override)
        bypass = true;

    if(came_atomo_counter_mode == 0 || bypass) {
        // Check for OFEX (overflow experimental) mode
        if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF || bypass) {
            bypass = false;
            // standard counter mode: pull from subghz_block_generic_global first
            if(!subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
                // no override pending: increase by the standard mult value as before
                if((instance->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) >
                   0xFFFF) {
                    instance->generic.cnt = 0;
                } else {
                    instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
                }
            }
        } else {
            // OFEX mode
            if((instance->generic.cnt + 0x1) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
                instance->generic.cnt = 0xFFFE;
            } else {
                instance->generic.cnt++;
            }
        }
    } else if(came_atomo_counter_mode == 1) {
        // Mode 1
        // 0000 / 0001 / FFFE / FFFF
        if((instance->generic.cnt + 0x1) > 0xFFFF) {
            instance->generic.cnt = 0;
        } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
            instance->generic.cnt = 0xFFFE;
        } else {
            instance->generic.cnt++;
        }
    } else if(came_atomo_counter_mode == 2) {
        // Mode 2
        // 0x807B / 0x807C / 0x007B / 0x007C
        if(instance->generic.cnt != 0x807B && instance->generic.cnt != 0x807C &&
           instance->generic.cnt != 0x007B) {
            instance->generic.cnt = 0x807B;
        } else if(instance->generic.cnt == 0x807C) {
            instance->generic.cnt = 0x007B;
        } else {
            instance->generic.cnt++;
        }
    } else {
        // Mode 3 - Freeze counter
    }
