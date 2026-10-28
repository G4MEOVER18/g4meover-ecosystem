/* Minimaler Host-Mock fuer die beiden von ring_buffer.c tatsaechlich genutzten FuriString-
 * Funktionen (furi_string_alloc/_free/_set_strn/_push_back). Kein Flipper-/Furi-Code, nur
 * genug, um das reale Verhalten (Laenge + kopierte Bytes) pruefbar zu machen. */
#ifndef FURI_MOCK_H
#define FURI_MOCK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    char* buf;
    size_t len;
} FuriString;

FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* s);
void furi_string_set_strn(FuriString* s, const char* data, size_t n);
void furi_string_push_back(FuriString* s, char c);
size_t furi_string_size(const FuriString* s);
const char* furi_string_data(const FuriString* s);

#endif
