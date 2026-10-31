#pragma once
/* Minimal host mock of furi.h -- only what generic.c/.h actually reference:
   FuriString (opaque, backed by a real heap-allocated C string), furi_check,
   FURI_LOG_*, UNUSED. Not a reimplementation of furi.h's real behavior for
   anything generic.c does not touch. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) ((void)(x))

#define furi_check(expr) \
    do { \
        if(!(expr)) { \
            abort(); \
        } \
    } while(0)

#define FURI_LOG_E(tag, fmt, ...) ((void)0)
#define FURI_LOG_W(tag, fmt, ...) ((void)0)
#define FURI_LOG_I(tag, fmt, ...) ((void)0)
#define FURI_LOG_D(tag, fmt, ...) ((void)0)

typedef struct FuriString FuriString;

FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* s);
/* Real furi_string_set() is an m-string macro overloaded for a FuriString*
   or a const char* second argument; generic.c's one call site always
   passes a string literal, so this mock only needs that shape. */
void furi_string_set(FuriString* s, const char* src);
void furi_string_set_str(FuriString* s, const char* src);
const char* furi_string_get_cstr(const FuriString* s);
