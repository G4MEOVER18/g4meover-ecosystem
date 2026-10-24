    int32_t multicntr = (furi_hal_subghz_get_rolling_counter_mult() & 0xFFFFFFF);
    // Adjust for negative multiplier
    if(furi_hal_subghz_get_rolling_counter_mult() < 0) {
        multicntr = furi_hal_subghz_get_rolling_counter_mult();
    }
    if(multicntr == 1) {
        multicntr = 2; // to keep old behaviour when mult = 1
    }
    // Check for OFEX (overflow experimental) mode
    if(subghz_block_generic_global_counter_override_get(&rolling)) {
        // override pending: use the value verbatim, skip the standard increment/OFEX
        // logic below entirely for this pass (one-shot, consumed by the call above)
    } else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {
        if((furi_hal_subghz_get_rolling_counter_mult() == (int32_t)0xFFFFFFF) & (rolling != 0)) {
            rolling = 0;
        } else {
            // if cnt was reset to 0 on previous step and user want new Cnt then set it to 0xE6000000 or 0xFFFFFFFF or new user value
            if(rolling == 0) {
                if((furi_hal_subghz_get_rolling_counter_mult()) < (int32_t)0x6000000) {
                    rolling = 0xE6000000;
                } else {
                    if((furi_hal_subghz_get_rolling_counter_mult()) >= (int32_t)0xFFFFFFF) {
                        rolling = 0xFFFFFFFF;
                    } else {
                        rolling = 0xE0000000;
                        rolling += multicntr;
                    }
                }
            } else {
                // if we have not special cases - so work as standart mode
                if((rolling + multicntr) > 0xFFFFFFFF) {
                    rolling = 0xE6000000;
                } else {
                    rolling += multicntr;
                }
            }
        }
    } else {
        // OFEX (overflow experimental) mode
        if((rolling + 0x1) > 0xFFFFFFFF) {
            rolling = 0xE6000000;
        } else if(rolling >= 0xE6000000 && rolling != 0xFFFFFFFE) {
            rolling = 0xFFFFFFFE;
        } else {
            rolling++;
        }
    }
