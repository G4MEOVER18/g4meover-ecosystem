    // override button if we change it with the signal settings button editor
    if(subghz_block_generic_global_button_override_get(&instance->generic.btn)) {
        FURI_LOG_D(TAG, "Button successfully changed to 0x%X", instance->generic.btn);
    }

    // faac slh protocol have 20-bit counter so we take only 20 bits from mult (by AND 0xFFFFF)
    if(subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
        // override pending: use the value verbatim, skip the standard increment/OFEX
        // logic below entirely for this pass (one-shot, consumed by the call above)
    } else if(allow_zero_seed || (instance->generic.seed != 0x0)) {
        // check OFEX mode
        if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {
            if((instance->generic.cnt + (furi_hal_subghz_get_rolling_counter_mult() & 0xFFFFF)) >
               0xFFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt += (furi_hal_subghz_get_rolling_counter_mult() & 0xFFFFF);
            }

        } else {
            // OFEX mode
            if(instance->generic.cnt < 0xFFFFF) {
                if((instance->generic.cnt + 0xFFFFF) > 0xFFFFF) {
                    instance->generic.cnt = 0;
                } else {
                    instance->generic.cnt += 0xFFFFF;
                }
            } else if(
                (instance->generic.cnt >= 0xFFFFF) &&
                (furi_hal_subghz_get_rolling_counter_mult() != 0)) {
                instance->generic.cnt = 0;
            }
        }
    }
