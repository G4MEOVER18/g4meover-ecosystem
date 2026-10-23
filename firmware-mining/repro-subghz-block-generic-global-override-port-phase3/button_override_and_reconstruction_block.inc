    // override button if we change it with the signal settings button editor
    if(subghz_block_generic_global_button_override_get(&instance->generic.btn)) {
        FURI_LOG_D(TAG, "Button successfully changed to 0x%X", instance->generic.btn);
    }

    // Reconstruction of the data
    // If we have 8bit button code move serial to left by 8 bits (and 4 if 4 bits)
    if(instance->generic.btn == 0x30 || instance->generic.btn == 0xC0) {
        instance->generic.data =
            ((uint64_t)instance->generic.serial << 8 | (uint64_t)instance->generic.btn);
    } else if(instance->generic.btn == 0xF3 || instance->generic.btn == 0xFC) {
        instance->generic.data =
            ((uint64_t)instance->generic.serial << 8 | (uint64_t)(instance->generic.btn & 0xF));
    } else {
        instance->generic.data =
            ((uint64_t)instance->generic.serial << 4 | (uint64_t)instance->generic.btn);
    }
