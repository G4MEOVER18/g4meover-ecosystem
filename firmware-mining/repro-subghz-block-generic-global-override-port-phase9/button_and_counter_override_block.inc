    // override button if we change it with the signal settings button editor
    if(subghz_block_generic_global_button_override_get(&instance->generic.btn)) {
        FURI_LOG_D(TAG, "Button successfully changed to 0x%X", instance->generic.btn);
    }

    // Check for OFEX (overflow experimental) mode
    if(subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {
        // override pending: use the value verbatim, skip the standard increment/OFEX
        // logic below entirely for this pass (one-shot, consumed by the call above)
    } else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {
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
