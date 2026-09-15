#pragma once
// Minimal host stand-in for FuriString. Only ASCII content is ever stored in the code under
// test (hex digits, decimal digits, "??", key names), so utf8_length == byte length here.
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FuriString {
    char* buf; // always NUL-terminated
    size_t len; // strlen(buf)
    size_t cap;
} FuriString;

static inline void furi_string_grow(FuriString* s, size_t min_cap) {
    if(s->cap >= min_cap) return;
    size_t new_cap = s->cap ? s->cap * 2 : 16;
    while(new_cap < min_cap) new_cap *= 2;
    s->buf = realloc(s->buf, new_cap);
    s->cap = new_cap;
}

static inline FuriString* furi_string_alloc(void) {
    FuriString* s = malloc(sizeof(FuriString));
    s->cap = 16;
    s->buf = malloc(s->cap);
    s->buf[0] = '\0';
    s->len = 0;
    return s;
}

static inline void furi_string_free(FuriString* s) {
    if(!s) return;
    free(s->buf);
    free(s);
}

static inline void furi_string_reset(FuriString* s) {
    s->buf[0] = '\0';
    s->len = 0;
}

static inline void furi_string_set_str(FuriString* s, const char* str) {
    size_t l = strlen(str);
    furi_string_grow(s, l + 1);
    memcpy(s->buf, str, l + 1);
    s->len = l;
}

static inline void furi_string_cat(FuriString* s, const char* str) {
    size_t l = strlen(str);
    furi_string_grow(s, s->len + l + 1);
    memcpy(s->buf + s->len, str, l + 1);
    s->len += l;
}

static inline void furi_string_cat_printf(FuriString* s, const char* fmt, ...) {
    char tmp[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    furi_string_cat(s, tmp);
}

static inline void furi_string_printf(FuriString* s, const char* fmt, ...) {
    char tmp[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    furi_string_set_str(s, tmp);
}

static inline bool furi_string_equal_str(const FuriString* s, const char* str) {
    return strcmp(s->buf, str) == 0;
}

static inline const char* furi_string_get_cstr(const FuriString* s) {
    return s->buf;
}

static inline char furi_string_get_char(const FuriString* s, size_t idx) {
    assert(idx < s->len);
    return s->buf[idx];
}

static inline size_t furi_string_size(const FuriString* s) {
    return s->len;
}

static inline size_t furi_string_utf8_length(const FuriString* s) {
    return s->len; // ASCII-only content in the tested code paths
}

static inline void furi_string_trim(FuriString* s) {
    size_t start = 0;
    while(start < s->len && s->buf[start] == ' ') start++;
    size_t end = s->len;
    while(end > start && s->buf[end - 1] == ' ') end--;
    size_t new_len = end - start;
    memmove(s->buf, s->buf + start, new_len);
    s->buf[new_len] = '\0';
    s->len = new_len;
}
