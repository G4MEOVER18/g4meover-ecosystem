    // Allow button edit from the signal settings button editor; all three branches below
    // transmit the same 4-bit button field, so this is unconditional.
    subghz_block_generic_global.btn_is_available = true;
    subghz_block_generic_global.current_btn = instance->generic.btn;
    subghz_block_generic_global.btn_length_bit = 4;
