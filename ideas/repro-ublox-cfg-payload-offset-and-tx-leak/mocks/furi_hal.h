#pragma once
/* Minimaler Stand-in fuer furi_hal.h / furi_hal_i2c.h, nur die vier von
 * ublox_device.c genutzten Symbole. Signaturen 1:1 aus
 * G4MEOVER-FW/targets/furi_hal_include/furi_hal_i2c.h uebernommen
 * (furi_hal_i2c_acquire/release werden von ublox_device.c nicht genutzt,
 * hier daher bewusst weggelassen). */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct { int unused; } FuriHalI2cBusHandle;
extern const FuriHalI2cBusHandle furi_hal_i2c_handle_external;

bool furi_hal_i2c_is_device_ready(const FuriHalI2cBusHandle* handle, uint8_t i2c_addr, uint32_t timeout);
bool furi_hal_i2c_tx(const FuriHalI2cBusHandle* handle, uint8_t address, const uint8_t* data, size_t size, uint32_t timeout);
bool furi_hal_i2c_rx(const FuriHalI2cBusHandle* handle, uint8_t address, uint8_t* data, size_t size, uint32_t timeout);
