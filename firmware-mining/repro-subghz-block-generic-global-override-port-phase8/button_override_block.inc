    // override button if we change it with signal settings button editor
    if(subghz_block_generic_global_button_override_get(&btn)) {
        bypass = true;
        FURI_LOG_D(TAG, "Button sucessfully changed to 0x%X", btn);
    }
