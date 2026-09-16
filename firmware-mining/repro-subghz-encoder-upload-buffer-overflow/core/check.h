#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define bit_read(value, bit) (((value) >> (bit)) & 0x01)
#define bit_write(value, bit, bitvalue) \
    ((bitvalue) ? ((value) |= (1UL << (bit))) : ((value) &= ~(1UL << (bit))))

#define furi_check(expr) ((void)(expr))

void furi_crash(const char* message);
