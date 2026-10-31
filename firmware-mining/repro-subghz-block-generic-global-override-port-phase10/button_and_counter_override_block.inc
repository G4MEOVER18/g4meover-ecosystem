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

    instance->generic.btn = (fix >> 17) & 0x0F;
    instance->generic.serial = ((fix >> 5) & 0xFFFF0000) | (fix & 0xFFFF);

    uint32_t data = (decrypt & 0xFFFF0000) | instance->generic.cnt;

    // override button if we change it with the signal settings button editor; both the
    // fixed part (fix, carries btn at bits [20:17]) and the hop's cleartext upper 16
    // bits (decrypt's top byte, carries btn at bits [31:28]) have to be rebuilt from the
    // new button for the override to actually reach the transmitted signal -- gen_data()
    // otherwise never rebuilds either field from instance->generic.btn once a key/fix was
    // loaded, it only ever re-derives btn FROM fix, never the other way around
    if(subghz_block_generic_global_button_override_get(&instance->generic.btn)) {
        FURI_LOG_D(TAG, "Button successfully changed to 0x%X", instance->generic.btn);
        fix = ((uint64_t)((instance->generic.serial >> 16) & 0xFFFF) << 21) |
              ((uint64_t)instance->generic.btn << 17) | ((uint64_t)1 << 16) |
              (instance->generic.serial & 0xFFFF);
        instance->generic.data = subghz_protocol_blocks_reverse_key(fix, 53);

        data = ((uint32_t)instance->generic.btn << 28) | (((uint32_t)0x0C) << 24) |
               ((instance->generic.serial & 0xFF) << 16) | instance->generic.cnt;
    }
