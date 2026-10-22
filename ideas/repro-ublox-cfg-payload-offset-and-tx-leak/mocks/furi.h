#pragma once
/* Minimaler Stand-in fuer furi.h, nur was ublox_device.c tatsaechlich braucht. */
#include <stdio.h>
#include <stdint.h>

#define FURI_LOG_E(tag, fmt, ...) fprintf(stderr, "[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define FURI_LOG_I(tag, fmt, ...) fprintf(stderr, "[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define FURI_LOG_D(tag, fmt, ...) fprintf(stderr, "[D][%s] " fmt "\n", tag, ##__VA_ARGS__)

static inline uint32_t furi_ms_to_ticks(uint32_t ms) {
    return ms;
}

static inline void furi_delay_ms(uint32_t ms) {
    (void)ms;
}
