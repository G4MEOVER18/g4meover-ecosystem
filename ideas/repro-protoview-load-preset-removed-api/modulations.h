#pragma once
#include "mocks.h"

/* Declarations only -- defined once in modulations.c, so radio_begin_*.c
 * and repro_main_*.c (separate translation units) see the exact same
 * array/byte-array addresses and pointer-identity checks are meaningful. */

typedef struct {
    FuriHalSubGhzPreset preset;
    const uint8_t* custom;
} TestModulation;

extern const uint8_t tpms1_regs[4];
extern const uint8_t tpms2_regs[4];
extern const uint8_t tpms3_regs[4];
extern const uint8_t ook40k_regs[4];
extern const uint8_t fsk40k_regs[4];

extern const TestModulation ProtoViewModulations[9];
#define MODULATIONS_COUNT (9u)
