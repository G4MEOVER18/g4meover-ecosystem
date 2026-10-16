/* Host stub matching targets/f7/furi_hal/furi_hal_rfid.h in G4MEOVER-FW (mntm-012,
 * tag b359f6f) byte-for-byte on every signature hitagmicro.c actually calls.
 * tim_read_start/_stop are counted (host_shim.c) so the repro can confirm the
 * real code power-cycles the field exactly as many times as the module's own
 * comments claim (3 write sessions + 6 latch cycles = 9). hitagmicro.c never
 * reads a response (open-loop), so it never calls tim_read_capture_start/_stop
 * or pause/continue in a way that needs anything beyond a plain no-op; they
 * still have to exist and link because hitagmicro_gap() calls them. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void host_record_field_on(void);
void host_record_field_off(void);

static inline void furi_hal_rfid_pins_reset(void) {}
static inline void furi_hal_rfid_pin_pull_release(void) {}
static inline void furi_hal_rfid_tim_read_start(float freq, float duty_cycle) {
    (void)freq;
    (void)duty_cycle;
    host_record_field_on();
}
static inline void furi_hal_rfid_tim_read_pause(void) {}
static inline void furi_hal_rfid_tim_read_continue(void) {}
static inline void furi_hal_rfid_tim_read_stop(void) {
    host_record_field_off();
}

typedef void (*FuriHalRfidReadCaptureCallback)(bool level, uint32_t duration, void* context);
static inline void
    furi_hal_rfid_tim_read_capture_start(FuriHalRfidReadCaptureCallback callback, void* context) {
    (void)callback;
    (void)context;
}
static inline void furi_hal_rfid_tim_read_capture_stop(void) {}
