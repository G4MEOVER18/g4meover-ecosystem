#pragma once
/* Minimal host mock -- generic.c only calls stream_clean() once, on the
   stream obtained from flipper_format_get_raw_stream(). */
typedef struct Stream Stream;

void stream_clean(Stream* stream);
