/* Minimal host stand-in for furi.h, just enough to compile the real, unmodified
 * lib/lfrfid/tools/hitagmicro.c on the host. hitagmicro.c has no self-test of its
 * own (unlike hitags.c), so furi_delay_us() is NOT a no-op here: it is the one
 * real function the host shim provides (see host_shim.c) that records every
 * delay hitagmicro_write() asks for, in call order. That recording is the only
 * channel this repro needs: every bit hitagmicro_write() would put on the wire
 * passes through a furi_delay_us(96|160|288|64) call before anything else, so
 * replaying the recorded durations reconstructs the exact bitstream the real
 * modulation code computed - without touching hardware or the module's static
 * frame builders. */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define UNUSED(x) (void)(x)
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static inline void host_fail(const char* what) {
    fprintf(stderr, "furi_check/furi_crash failed: %s\n", what);
    exit(134);
}

#define furi_check(cond)             \
    do {                             \
        if(!(cond)) host_fail(#cond); \
    } while(0)

#define furi_crash(msg) host_fail(msg)

#define FURI_CRITICAL_ENTER() \
    do {                      \
    } while(0)
#define FURI_CRITICAL_EXIT() \
    do {                     \
    } while(0)

typedef enum {
    FuriLogLevelDefault,
    FuriLogLevelNone,
    FuriLogLevelError,
    FuriLogLevelWarn,
    FuriLogLevelInfo,
    FuriLogLevelDebug,
    FuriLogLevelTrace,
} FuriLogLevel;

static inline FuriLogLevel furi_log_get_level(void) {
    return FuriLogLevelNone;
}

#define FURI_LOG_D(tag, fmt, ...) ((void)0)
#define FURI_LOG_I(tag, fmt, ...) ((void)0)
#define FURI_LOG_W(tag, fmt, ...) ((void)0)

/* Real function, defined in host_shim.c: records every requested delay (in
 * call order) instead of discarding it. See host_shim.c/host_shim.h. */
void furi_delay_us(uint32_t us);
static inline void furi_delay_ms(uint32_t ms) {
    UNUSED(ms);
}
