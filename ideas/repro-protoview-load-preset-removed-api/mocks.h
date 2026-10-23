#pragma once
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- Minimal stand-ins for the real Flipper subghz HAL/device API, built
 * strictly from the symbol lists confirmed present/absent against my own
 * G4MEOVER-FW (b359f6f) firmware headers:
 *   targets/f7/furi_hal/furi_hal_subghz.h  -- no furi_hal_subghz_load_preset()
 *   lib/subghz/devices/devices.h           -- subghz_devices_load_preset()
 *   lib/subghz/devices/preset.h            -- FuriHalSubGhzPreset enum (kept)
 * --------------------------------------------------------------------- */

typedef enum {
    FuriHalSubGhzPresetIDLE,
    FuriHalSubGhzPresetOok270Async,
    FuriHalSubGhzPresetOok650Async,
    FuriHalSubGhzPreset2FSKDev238Async,
    FuriHalSubGhzPreset2FSKDev476Async,
    FuriHalSubGhzPresetMSK99_97KbAsync,
    FuriHalSubGhzPresetGFSK9_99KbAsync,
    FuriHalSubGhzPresetCustom,
} FuriHalSubGhzPreset;

typedef struct SubGhzDevice SubGhzDevice;
struct SubGhzDevice {
    const char* name;
};

extern SubGhzDevice g_cc1101_int_device; /* defined once in mocks.c */
#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"

/* Records what the mocked radio layer was actually asked to do, so the
 * test can assert on it without any real CC1101/SPI involved. */
typedef struct {
    int custom_preset_calls;
    const uint8_t* last_custom_preset;
    int device_preset_calls;
    const SubGhzDevice* last_device;
    FuriHalSubGhzPreset last_device_preset;
    const uint8_t* last_device_preset_data;
    int old_load_preset_calls; /* only used by the ALT/old-firmware build */
    FuriHalSubGhzPreset last_old_preset;
} RadioCallLog;

extern RadioCallLog g_log;

static inline void furi_hal_subghz_load_custom_preset(const uint8_t* preset_data) {
    g_log.custom_preset_calls++;
    g_log.last_custom_preset = preset_data;
}

/* NEU: the real, still-present modern API. */
static inline const SubGhzDevice* subghz_devices_get_by_name(const char* name) {
    if(name && g_cc1101_int_device.name && !strcmp(name, g_cc1101_int_device.name))
        return &g_cc1101_int_device;
    return NULL;
}

static inline void subghz_devices_load_preset(
    const SubGhzDevice* device,
    FuriHalSubGhzPreset preset,
    uint8_t* preset_data) {
    g_log.device_preset_calls++;
    g_log.last_device = device;
    g_log.last_device_preset = preset;
    g_log.last_device_preset_data = preset_data;
}

/* ALT path only: deliberately NOT defined here, exactly like my own
 * furi_hal_subghz.h -- see alt_radio_begin.c, which fails to build when
 * this header is used instead of providing a stub for it. */
#if PROTOVIEW_REPRO_PROVIDE_REMOVED_SYMBOL
static inline void furi_hal_subghz_load_preset(FuriHalSubGhzPreset preset) {
    g_log.old_load_preset_calls++;
    g_log.last_old_preset = preset;
}
#endif
