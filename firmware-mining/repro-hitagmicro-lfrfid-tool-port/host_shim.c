/* Implements the real furi_delay_us() hitagmicro.c links against (declared in
 * mocks/furi.h) and the field on/off counters mocks/furi_hal_rfid.h calls into.
 * One shared definition per process, so recordings made while hitagmicro.c runs
 * are visible to repro_main.c afterwards. */
#include "host_shim.h"
#include <stdint.h>

#define HOST_SHIM_MAX_DELAYS 4096

static uint32_t g_delays[HOST_SHIM_MAX_DELAYS];
static size_t g_delay_count = 0;
static unsigned g_field_on_count = 0;
static unsigned g_field_off_count = 0;

void host_shim_reset(void) {
    g_delay_count = 0;
    g_field_on_count = 0;
    g_field_off_count = 0;
}

void furi_delay_us(uint32_t us) {
    if(g_delay_count < HOST_SHIM_MAX_DELAYS) {
        g_delays[g_delay_count] = us;
    }
    g_delay_count++;
}

size_t host_shim_delay_count(void) {
    return g_delay_count;
}

uint32_t host_shim_delay_at(size_t index) {
    if(index >= g_delay_count || index >= HOST_SHIM_MAX_DELAYS) return 0xFFFFFFFFu;
    return g_delays[index];
}

void host_record_field_on(void) {
    g_field_on_count++;
}

void host_record_field_off(void) {
    g_field_off_count++;
}

unsigned host_shim_field_on_count(void) {
    return g_field_on_count;
}

unsigned host_shim_field_off_count(void) {
    return g_field_off_count;
}
