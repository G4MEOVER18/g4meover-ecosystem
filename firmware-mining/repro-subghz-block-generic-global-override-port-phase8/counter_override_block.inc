    // if we change counter/button in SignalSettings menu then we must bypass counter_modes,
    // just gen and save signal file.
    if(subghz_block_generic_global.cnt_need_override) bypass = true;

    if((alutech_at4n_counter_mode == 0) || bypass) {
        // Check for OFEX (overflow experimental) mode
        if((furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) || bypass) {
            bypass = false;
            // standard counter mode. PULL data from subghz_block_generic_global variables
            if(!subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
                // if counter_override_get return FALSE then counter was not changed and we
                // increase counter by standard mult value
                if((instance->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) >
                   0xFFFF) {
                    instance->generic.cnt = 0;
                } else {
                    instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
                }
            }
        } else {
            //OFEX mode
            if((instance->generic.cnt + 0x1) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
                instance->generic.cnt = 0xFFFE;
            } else {
                instance->generic.cnt++;
            }
        }
    } else if(alutech_at4n_counter_mode == 1) {
        // Mode 1
        // 0000 / 0001 / FFFE / FFFF
        if((instance->generic.cnt + 0x1) > 0xFFFF) {
            instance->generic.cnt = 0;
        } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
            instance->generic.cnt = 0xFFFE;
        } else {
            instance->generic.cnt++;
        }
    } else {
        // Mode 2
        // 0x0000 / 0x0001 / 0x0002 / 0x0003 / 0x0004 / 0x0005
        if(instance->generic.cnt >= 0x0005) {
            instance->generic.cnt = 0;
        } else {
            instance->generic.cnt++;
        }
    }
