/* Accessors for the recordings host_shim.c makes while the real, unmodified
 * hitagmicro.c runs against mocks/. repro_main.c uses these to reconstruct what
 * hitagmicro_write() actually put on the wire, without changing a single line of
 * hitagmicro.c itself. */
#pragma once
#include <stdint.h>
#include <stddef.h>

void host_shim_reset(void);

size_t host_shim_delay_count(void);
uint32_t host_shim_delay_at(size_t index);

unsigned host_shim_field_on_count(void);
unsigned host_shim_field_off_count(void);
