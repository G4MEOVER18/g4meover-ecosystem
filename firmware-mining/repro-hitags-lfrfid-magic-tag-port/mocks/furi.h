/* Minimal host stand-in for furi.h, just enough to compile the real, unmodified
 * lib/lfrfid/tools/hitags.c on the host and run its own hitags_selftest().
 * hitags_selftest() never reaches furi_hal_rfid or FURI_CRITICAL, so those are
 * trivial no-op stubs; only furi_check/log/UNUSED/COUNT_OF/MIN/delay/FuriString
 * need to behave like the real thing for the self-test to mean anything. */
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

#define furi_check(cond)         \
    do {                         \
        if(!(cond)) host_fail(#cond); \
    } while(0)

#define furi_crash(msg) host_fail(msg)

#define FURI_CRITICAL_ENTER() \
    do {                      \
    } while(0)
#define FURI_CRITICAL_EXIT() \
    do {                     \
    } while(0)

#define __DMB() \
    do {        \
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

static inline void furi_delay_us(uint32_t us) {
    UNUSED(us);
}
static inline void furi_delay_ms(uint32_t ms) {
    UNUSED(ms);
}

/* Just enough of FuriString for hitags_log_capture(), which selftest never calls. */
typedef struct {
    char buf[256];
} FuriString;

static inline FuriString* furi_string_alloc_printf(const char* fmt, ...) {
    FuriString* s = malloc(sizeof(FuriString));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->buf, sizeof(s->buf), fmt, ap);
    va_end(ap);
    return s;
}
static inline void furi_string_cat_printf(FuriString* s, const char* fmt, ...) {
    size_t len = strlen(s->buf);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->buf + len, sizeof(s->buf) - len, fmt, ap);
    va_end(ap);
}
static inline const char* furi_string_get_cstr(FuriString* s) {
    return s->buf;
}
static inline void furi_string_free(FuriString* s) {
    free(s);
}
