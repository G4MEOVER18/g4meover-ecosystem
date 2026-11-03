#pragma once
/* Minimal host mock -- only the FlipperFormat functions generic.c calls. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "furi.h"
#include "lib/toolbox/stream/stream.h"

typedef struct FlipperFormat FlipperFormat;

Stream* flipper_format_get_raw_stream(FlipperFormat* flipper_format);
bool flipper_format_write_header_cstr(
    FlipperFormat* flipper_format,
    const char* header,
    const uint32_t version);
bool flipper_format_write_uint32(
    FlipperFormat* flipper_format,
    const char* key,
    const uint32_t* data,
    const uint16_t data_size);
bool flipper_format_write_string_cstr(
    FlipperFormat* flipper_format,
    const char* key,
    const char* string_cstr);
bool flipper_format_write_hex(
    FlipperFormat* flipper_format,
    const char* key,
    const uint8_t* data,
    const uint16_t data_size);
bool flipper_format_write_float(
    FlipperFormat* flipper_format,
    const char* key,
    const float* data,
    const uint16_t data_size);
bool flipper_format_rewind(FlipperFormat* flipper_format);
bool flipper_format_read_uint32(
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t* data,
    const uint16_t data_size);
bool flipper_format_read_hex(
    FlipperFormat* flipper_format,
    const char* key,
    uint8_t* data,
    const uint16_t data_size);
