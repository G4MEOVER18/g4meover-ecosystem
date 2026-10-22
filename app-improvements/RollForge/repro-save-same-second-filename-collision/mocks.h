// Minimale Mocks fuer die echte rollforge_storage.c -- kein Flipper/furi
// verfuegbar, nur die hier tatsaechlich genutzten Storage-/RTC-Symbole.
// Deklarationen hier, Definitionen in mocks.c (eine Uebersetzungseinheit,
// damit ALT.c/NEU.c/repro_main.c dasselbe virtuelle Dateisystem teilen).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { int dummy; } Storage;
typedef struct { char path[256]; char data[8192]; size_t len; bool open; } File;

typedef enum { FSAM_READ = 1, FSAM_WRITE = 2 } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING = 1, FSOM_CREATE_ALWAYS = 4 } FS_OpenMode;

#define RECORD_STORAGE "storage"

#define MAX_FILES 32
typedef struct { char path[256]; char data[8192]; size_t len; bool used; } FsEntry;
extern FsEntry g_fs[MAX_FILES];
extern int g_fs_count;

Storage* furi_record_open(const char* name);
void furi_record_close(const char* name);
void storage_simply_mkdir(Storage* s, const char* path);
bool storage_file_exists(Storage* s, const char* path);
File* storage_file_alloc(Storage* s);
void storage_file_free(File* f);
bool storage_file_open(File* f, const char* path, FS_AccessMode am, FS_OpenMode om);
int storage_file_write(File* f, const void* buf, size_t size);
void storage_file_close(File* f);

typedef struct { uint8_t hour, minute, second; } DateTime;
extern DateTime g_mock_now;
void furi_hal_rtc_get_datetime(DateTime* dt);

void fs_reset(void);
int fs_count_with_prefix(const char* prefix);
