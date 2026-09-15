#pragma once
// Host stand-in for mbedtls's 3DES API: mf_ultralight.c's mf_ultralight_3des_encrypt/decrypt
// (pre-existing, unrelated to this UL-AES struct/serialization patch) call these, so the linker
// needs symbols, but no test in this repro exercises the UL-C 3DES auth path -- so this is a
// non-cryptographic pass-through stand-in, not a real 3DES implementation.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MBEDTLS_DES_ENCRYPT 1
#define MBEDTLS_DES_DECRYPT 0

typedef struct {
    int dummy;
} mbedtls_des3_context;

static inline void mbedtls_des3_set2key_enc(mbedtls_des3_context* ctx, const uint8_t* key) {
    (void)ctx;
    (void)key;
}

static inline void mbedtls_des3_set2key_dec(mbedtls_des3_context* ctx, const uint8_t* key) {
    (void)ctx;
    (void)key;
}

static inline int mbedtls_des3_crypt_cbc(
    mbedtls_des3_context* ctx,
    int mode,
    size_t length,
    uint8_t* iv,
    const uint8_t* input,
    uint8_t* output) {
    (void)ctx;
    (void)mode;
    (void)iv;
    memcpy(output, input, length);
    return 0;
}
