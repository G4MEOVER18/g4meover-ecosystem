    if(btn == 0x1) {
        btn = 0x0;
    } else if(btn == 0x2) {
        btn = 0x2;
    } else if(btn == 0x3) {
        btn = 0x4;
    } else if(btn == 0x4) {
        btn = 0x6;
    }

    // override button if we change it with the signal settings button editor
    if(subghz_block_generic_global_button_override_get(&btn)) {
        FURI_LOG_D(TAG, "Button successfully changed to 0x%X", btn);
    }
