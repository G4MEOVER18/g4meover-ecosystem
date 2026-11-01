    // override button if we change it with signal settings button editor
    if(subghz_block_generic_global_button_override_get(&instance->generic.btn)) {
        FURI_LOG_D(TAG, "Button sucessfully changed to 0x%X", instance->generic.btn);
    }

    // Experemental case - we dont know counter size exactly, so just will be think that it is in range of 0xE500000 - 0xFFFFFFF
    // one case when we have mult = 0xFFFFFFFF  - its when we reset counter before applying new cnt value
    // so at first step we reset cnt to 0 and now we sure here will be second step (set new cnt value);
    // at second step check what user set for new Cnt (and correct it if cnt less than 0xE500000 or more than 0xFFFFFFF)
    int32_t multicntr = (furi_hal_subghz_get_rolling_counter_mult() & 0xFFFFFFF);
    // Adjust for negative multiplier
    if(furi_hal_subghz_get_rolling_counter_mult() < 0) {
        multicntr = furi_hal_subghz_get_rolling_counter_mult();
    }
    // Check for OFEX (overflow experimental) mode
    if(subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
        // override pending: use the value verbatim, skip the standard increment/OFEX
        // logic below entirely for this pass (one-shot, consumed by the call above)
    } else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {
        if((furi_hal_subghz_get_rolling_counter_mult() == (int32_t)0xFFFFFFF) &
           (instance->generic.cnt != 0)) {
            instance->generic.cnt = 0;
        } else {
            // if cnt was reset to 0 on previous step and user want new Cnt then set it to 0xE500000 or 0xFFFFFFF or new user value
            if(instance->generic.cnt == 0) {
                if(furi_hal_subghz_get_rolling_counter_mult() < (int32_t)0xE500000) {
                    instance->generic.cnt = 0xE500000;
                } else {
                    if(furi_hal_subghz_get_rolling_counter_mult() >= (int32_t)0xFFFFFFF) {
                        instance->generic.cnt = 0xFFFFFFF;
                    } else {
                        instance->generic.cnt += multicntr;
                    }
                }
            } else {
                // if we have not special cases - so work as standart mode
                if((instance->generic.cnt + multicntr) > 0xFFFFFFF) {
                    instance->generic.cnt = 0xE500000;
                } else {
                    instance->generic.cnt += multicntr;
                }
            }
        }
    } else {
        // OFEX (overflow experimental) mode
        if((instance->generic.cnt + 0x1) > 0xFFFFFFF) {
            instance->generic.cnt = 0xE500000;
        } else if(instance->generic.cnt >= 0xE500000 && instance->generic.cnt != 0xFFFFFFE) {
            instance->generic.cnt = 0xFFFFFFE;
        } else {
            instance->generic.cnt++;
        }
    }
