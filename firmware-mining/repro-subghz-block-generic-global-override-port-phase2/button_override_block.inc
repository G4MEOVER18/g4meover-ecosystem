    // override button if we change it with the signal settings button editor
    if(subghz_block_generic_global_button_override_get(&btn)) {
        FURI_LOG_D(TAG, "Button successfully changed to 0x%X", btn);
    }

