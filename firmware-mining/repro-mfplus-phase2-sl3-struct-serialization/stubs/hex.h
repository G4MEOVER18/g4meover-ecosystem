#pragma once
// Minimal host stand-in for toolbox/hex.h's hex_char_to_uint8 (the only symbol mf_plus_i.c's SL3
// section pulls from it).
#include <stdbool.h>
#include <stdint.h>

static inline bool hex_digit_to_uint8(char c, uint8_t* out) {
    if(c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }
    if(c >= 'A' && c <= 'F') {
        *out = (uint8_t)(c - 'A' + 10);
        return true;
    }
    if(c >= 'a' && c <= 'f') {
        *out = (uint8_t)(c - 'a' + 10);
        return true;
    }
    return false;
}

static inline bool hex_char_to_uint8(char hi, char lo, uint8_t* out) {
    uint8_t h, l;
    if(!hex_digit_to_uint8(hi, &h) || !hex_digit_to_uint8(lo, &l)) return false;
    *out = (uint8_t)((h << 4) | l);
    return true;
}
