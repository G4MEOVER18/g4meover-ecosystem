#pragma once
// mf_ultralight.c calls exactly one bit_lib function, from mf_ultralight_is_all_data_read()
// (unmodified by this patch, but compiled in as part of the same translation unit, so the
// linker still needs a real definition even though this port's own tests don't call that
// function directly): a plain big-endian byte-array-to-integer read, real implementation (not a
// no-op stand-in) so any test that does exercise that path gets a correct result.
#include <stddef.h>
#include <stdint.h>

static inline uint32_t bit_lib_bytes_to_num_be(const uint8_t* bytes, size_t length) {
    uint32_t value = 0;
    for(size_t i = 0; i < length; i++) {
        value = (value << 8) | bytes[i];
    }
    return value;
}
