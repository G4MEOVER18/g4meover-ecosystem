/* Host stub matching targets/f7/furi_hal/furi_hal_rfid.h in G4MEOVER-FW (mntm-012,
 * tag b359f6f) byte-for-byte on every signature hitags.c actually calls. None of
 * these are reached by hitags_selftest(); they only need to link. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

static inline void furi_hal_rfid_pins_reset(void) {}
static inline void furi_hal_rfid_tim_read_start(float freq, float duty_cycle) {
    (void)freq;
    (void)duty_cycle;
}
static inline void furi_hal_rfid_tim_read_pause(void) {}
static inline void furi_hal_rfid_tim_read_continue(void) {}
static inline void furi_hal_rfid_tim_read_stop(void) {}

typedef void (*FuriHalRfidReadCaptureCallback)(bool level, uint32_t duration, void* context);
static inline void
    furi_hal_rfid_tim_read_capture_start(FuriHalRfidReadCaptureCallback callback, void* context) {
    (void)callback;
    (void)context;
}
static inline void furi_hal_rfid_tim_read_capture_stop(void) {}
