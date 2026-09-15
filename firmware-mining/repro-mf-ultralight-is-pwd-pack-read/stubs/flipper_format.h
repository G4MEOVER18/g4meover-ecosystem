#pragma once
// Minimal host stand-in for FlipperFormat: an ordered, in-memory key->value-string store that
// mimics the on-disk ".nfc" text format closely enough to round-trip the SL3 payload
// (hex-encoded byte fields, decimal integers, plain strings). Comments are accepted and
// discarded, exactly as a real load ignores them.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "furi_string.h"

typedef struct {
    char* key;
    char* value; // raw stored text, format depends on which write_* function wrote it
} FFEntry;

// Avoid strdup() here: whether it is declared depends on which feature-test macro was in effect
// the first time <string.h> was included by any translation unit, which is fragile to get right
// across every file that transitively pulls this header in.
static inline char* ff_strdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = malloc(n);
    memcpy(p, s, n);
    return p;
}

typedef struct FlipperFormat {
    FFEntry* entries;
    size_t count;
    size_t cap;
} FlipperFormat;

static inline FlipperFormat* flipper_format_alloc(void) {
    FlipperFormat* ff = malloc(sizeof(FlipperFormat));
    ff->entries = NULL;
    ff->count = 0;
    ff->cap = 0;
    return ff;
}

static inline void flipper_format_free(FlipperFormat* ff) {
    for(size_t i = 0; i < ff->count; i++) {
        free(ff->entries[i].key);
        free(ff->entries[i].value);
    }
    free(ff->entries);
    free(ff);
}

static inline FFEntry* ff_find(FlipperFormat* ff, const char* key) {
    for(size_t i = 0; i < ff->count; i++) {
        if(strcmp(ff->entries[i].key, key) == 0) return &ff->entries[i];
    }
    return NULL;
}

static inline void ff_set(FlipperFormat* ff, const char* key, const char* value) {
    FFEntry* e = ff_find(ff, key);
    if(e) {
        free(e->value);
        e->value = ff_strdup(value);
        return;
    }
    if(ff->count == ff->cap) {
        ff->cap = ff->cap ? ff->cap * 2 : 64;
        ff->entries = realloc(ff->entries, ff->cap * sizeof(FFEntry));
    }
    ff->entries[ff->count].key = ff_strdup(key);
    ff->entries[ff->count].value = ff_strdup(value);
    ff->count++;
}

static inline bool flipper_format_write_comment_cstr(FlipperFormat* ff, const char* comment) {
    (void)ff;
    (void)comment;
    return true;
}

static inline bool flipper_format_write_string_cstr(
    FlipperFormat* ff,
    const char* key,
    const char* value) {
    ff_set(ff, key, value);
    return true;
}

// Ordered-append store never needs an explicit read cursor reset for read-by-key lookups; the
// real API's rewind() is only meaningful for its sequential/streaming read mode, which this stub
// (and every call site under test) doesn't use.
static inline bool flipper_format_rewind(FlipperFormat* ff) {
    (void)ff;
    return true;
}

static inline bool
    flipper_format_write_string(FlipperFormat* ff, const char* key, FuriString* value) {
    ff_set(ff, key, furi_string_get_cstr(value));
    return true;
}

static inline bool
    flipper_format_read_string(FlipperFormat* ff, const char* key, FuriString* value) {
    FFEntry* e = ff_find(ff, key);
    if(!e) return false;
    furi_string_set_str(value, e->value);
    return true;
}

static inline bool flipper_format_write_hex(
    FlipperFormat* ff,
    const char* key,
    const uint8_t* data,
    size_t data_size) {
    FuriString* s = furi_string_alloc();
    for(size_t i = 0; i < data_size; i++) {
        furi_string_cat_printf(s, i ? " %02X" : "%02X", data[i]);
    }
    ff_set(ff, key, furi_string_get_cstr(s));
    furi_string_free(s);
    return true;
}

static inline bool
    flipper_format_read_hex(FlipperFormat* ff, const char* key, uint8_t* data, size_t data_size) {
    FFEntry* e = ff_find(ff, key);
    if(!e) return false;
    const char* p = e->value;
    for(size_t i = 0; i < data_size; i++) {
        unsigned int byte;
        if(sscanf(p, "%2x", &byte) != 1) return false;
        data[i] = (uint8_t)byte;
        p += 2;
        if(*p == ' ') p++;
    }
    return true;
}

static inline bool flipper_format_write_uint32(
    FlipperFormat* ff,
    const char* key,
    const uint32_t* data,
    size_t data_size) {
    FuriString* s = furi_string_alloc();
    for(size_t i = 0; i < data_size; i++) {
        furi_string_cat_printf(s, i ? " %lu" : "%lu", (unsigned long)data[i]);
    }
    ff_set(ff, key, furi_string_get_cstr(s));
    furi_string_free(s);
    return true;
}

static inline bool flipper_format_read_uint32(
    FlipperFormat* ff,
    const char* key,
    uint32_t* data,
    size_t data_size) {
    FFEntry* e = ff_find(ff, key);
    if(!e) return false;
    const char* p = e->value;
    for(size_t i = 0; i < data_size; i++) {
        unsigned long v;
        int consumed = 0;
        if(sscanf(p, "%lu%n", &v, &consumed) != 1) return false;
        data[i] = (uint32_t)v;
        p += consumed;
        while(*p == ' ') p++;
    }
    return true;
}
