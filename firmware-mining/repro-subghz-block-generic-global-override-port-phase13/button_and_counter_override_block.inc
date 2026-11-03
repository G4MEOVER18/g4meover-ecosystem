    btn = subghz_protocol_nice_flor_s_get_btn_code();

    // override button if we change it with signal settings button editor
    if(subghz_block_generic_global_button_override_get(&btn)) {
        bypass = true;
        FURI_LOG_D(TAG, "Button sucessfully changed to 0x%X", btn);
    }

    // if we change counter/button in SignalSettings menu then we must bypass counter_modes, just gen and save signal file.
    if(subghz_block_generic_global.cnt_need_override) bypass = true;

    if(nice_flors_counter_mode == 0 || bypass) {
        // Check for OFEX (overflow experimental) mode
        if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF || bypass) {
            bypass = false;
            // standart counter mode. PULL data from subghz_block_generic_global variables
            if(!subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
                // if counter_override_get return FALSE then counter was not changed and we increase counter by standart mult value
                if((instance->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) > 0xFFFF) {
                    instance->generic.cnt = 0;
                } else {
                    instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
                }
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
    } else if(nice_flors_counter_mode == 1) {
        // Mode 1 (floxi2r)
        // 0001 / FFFE
        if(instance->generic.cnt == 0xFFFE) {
            instance->generic.cnt = 0x0001;
        } else {
            instance->generic.cnt = 0xFFFE;
        }
    } else {
        // Mode 2 (ox2)
        // 0x0000 / 0x0001
        if(instance->generic.cnt >= 0x0001) {
            instance->generic.cnt = 0;
        } else {
            instance->generic.cnt++;
        }
    }
