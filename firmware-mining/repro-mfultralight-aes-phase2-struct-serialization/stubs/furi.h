#pragma once
// Minimal host stand-in for furi.h: only the pieces mf_plus_i.c's SL3 section and the
// touched mf_plus.c hooks actually use (assert-style checks, a crash helper, two macros).
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define furi_check(x) assert(x)
#define furi_assert(x) assert(x)

#define UNUSED(x) ((void)(x))
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

#define FURI_LOG_W(tag, fmt, ...) ((void)0)
#define FURI_LOG_D(tag, fmt, ...) ((void)0)

static inline void furi_crash(const char* msg) {
    fprintf(stderr, "furi_crash: %s\n", msg ? msg : "(null)");
    abort();
}
