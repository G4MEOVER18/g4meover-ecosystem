#include "furi_mock.h"
#include <stdlib.h>
#include <string.h>

FuriString* furi_string_alloc(void) {
    FuriString* s = malloc(sizeof(FuriString));
    s->buf = malloc(1);
    s->buf[0] = '\0';
    s->len = 0;
    return s;
}

void furi_string_free(FuriString* s) {
    free(s->buf);
    free(s);
}

void furi_string_set_strn(FuriString* s, const char* data, size_t n) {
    free(s->buf);
    s->buf = malloc(n + 1);
    memcpy(s->buf, data, n);
    s->buf[n] = '\0';
    s->len = n;
}

void furi_string_push_back(FuriString* s, char c) {
    s->buf = realloc(s->buf, s->len + 2);
    s->buf[s->len] = c;
    s->buf[s->len + 1] = '\0';
    s->len++;
}

size_t furi_string_size(const FuriString* s) {
    return s->len;
}

const char* furi_string_data(const FuriString* s) {
    return s->buf;
}
