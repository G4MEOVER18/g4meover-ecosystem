/* Byte-for-byte copy of ProtoView's own ProtoViewModulations[] table
 * (app_subghz.c), minus the display-name strings (irrelevant to the
 * control flow under test). Four marker byte arrays stand in for the
 * real custom_presets.h register tables -- their content is never
 * inspected by radio_begin(), only their pointer identity. */
#include "modulations.h"

const uint8_t tpms1_regs[4] = {0x11, 0x11, 0x11, 0x11};
const uint8_t tpms2_regs[4] = {0x22, 0x22, 0x22, 0x22};
const uint8_t tpms3_regs[4] = {0x33, 0x33, 0x33, 0x33};
const uint8_t ook40k_regs[4] = {0x44, 0x44, 0x44, 0x44};
const uint8_t fsk40k_regs[4] = {0x55, 0x55, 0x55, 0x55};

const TestModulation ProtoViewModulations[9] = {
    {FuriHalSubGhzPresetOok650Async, NULL},
    {FuriHalSubGhzPresetOok270Async, NULL},
    {FuriHalSubGhzPreset2FSKDev238Async, NULL},
    {FuriHalSubGhzPreset2FSKDev476Async, NULL},
    {0, tpms1_regs},
    {0, tpms2_regs},
    {0, tpms3_regs},
    {0, ook40k_regs},
    {0, fsk40k_regs},
};
