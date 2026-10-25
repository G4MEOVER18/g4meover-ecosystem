/* Link-time stubs for the parts of generic.c this repro never exercises
   (subghz_block_generic_serialize/_deserialize/_get_preset_name). The repro
   only calls the four pure override/reset functions, which touch none of
   these -- these stubs exist purely so the unmodified generic.c object
   links; none of them is called at runtime (checked via the call counter
   below, asserted zero at the end of main in repro.c). */
#include <stdlib.h>
#include <string.h>
#include "furi.h"
#include "lib/flipper_format/flipper_format.h"

int g_unexpected_stub_calls = 0;

struct FuriString {
    char* buf;
};

static char* dup_cstr(const char* src) {
    size_t n = strlen(src) + 1;
    char* out = malloc(n);
    memcpy(out, src, n);
    return out;
}

FuriString* furi_string_alloc(void) {
    g_unexpected_stub_calls++;
    FuriString* s = malloc(sizeof(FuriString));
    s->buf = dup_cstr("");
    return s;
}

void furi_string_free(FuriString* s) {
    g_unexpected_stub_calls++;
    if(s) {
        free(s->buf);
        free(s);
    }
}

void furi_string_set(FuriString* s, const char* src) {
    g_unexpected_stub_calls++;
    free(s->buf);
    s->buf = dup_cstr(src);
}

void furi_string_set_str(FuriString* s, const char* src) {
    g_unexpected_stub_calls++;
    free(s->buf);
    s->buf = dup_cstr(src);
}

const char* furi_string_get_cstr(const FuriString* s) {
    g_unexpected_stub_calls++;
    return s->buf;
}

void stream_clean(Stream* stream) {
    g_unexpected_stub_calls++;
    (void)stream;
}

Stream* flipper_format_get_raw_stream(FlipperFormat* flipper_format) {
    g_unexpected_stub_calls++;
    (void)flipper_format;
    return NULL;
}

bool flipper_format_write_header_cstr(FlipperFormat* f, const char* h, const uint32_t v) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)h;
    (void)v;
    return false;
}

bool flipper_format_write_uint32(
    FlipperFormat* f,
    const char* key,
    const uint32_t* data,
    const uint16_t data_size) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)key;
    (void)data;
    (void)data_size;
    return false;
}

bool flipper_format_write_string_cstr(FlipperFormat* f, const char* key, const char* s) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)key;
    (void)s;
    return false;
}

bool flipper_format_write_hex(
    FlipperFormat* f,
    const char* key,
    const uint8_t* data,
    const uint16_t data_size) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)key;
    (void)data;
    (void)data_size;
    return false;
}

bool flipper_format_write_float(
    FlipperFormat* f,
    const char* key,
    const float* data,
    const uint16_t data_size) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)key;
    (void)data;
    (void)data_size;
    return false;
}

bool flipper_format_rewind(FlipperFormat* f) {
    g_unexpected_stub_calls++;
    (void)f;
    return false;
}

bool flipper_format_read_uint32(
    FlipperFormat* f,
    const char* key,
    uint32_t* data,
    const uint16_t data_size) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)key;
    (void)data;
    (void)data_size;
    return false;
}

bool flipper_format_read_hex(
    FlipperFormat* f,
    const char* key,
    uint8_t* data,
    const uint16_t data_size) {
    g_unexpected_stub_calls++;
    (void)f;
    (void)key;
    (void)data;
    (void)data_size;
    return false;
}
