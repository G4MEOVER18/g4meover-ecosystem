#include "mocks.h"

FsEntry g_fs[MAX_FILES];
int g_fs_count = 0;
DateTime g_mock_now;

Storage* furi_record_open(const char* name) { (void)name; return (Storage*)1; }
void furi_record_close(const char* name) { (void)name; }
void storage_simply_mkdir(Storage* s, const char* path) { (void)s; (void)path; }

bool storage_file_exists(Storage* s, const char* path) {
    (void)s;
    for(int i = 0; i < g_fs_count; i++)
        if(g_fs[i].used && strcmp(g_fs[i].path, path) == 0) return true;
    return false;
}

File* storage_file_alloc(Storage* s) {
    (void)s;
    return (File*)calloc(1, sizeof(File));
}
void storage_file_free(File* f) { free(f); }

bool storage_file_open(File* f, const char* path, FS_AccessMode am, FS_OpenMode om) {
    (void)am; (void)om;
    strncpy(f->path, path, sizeof(f->path) - 1);
    f->len = 0;
    f->open = true;
    return true;
}
int storage_file_write(File* f, const void* buf, size_t size) {
    memcpy(f->data + f->len, buf, size);
    f->len += size;
    return (int)size;
}
void storage_file_close(File* f) {
    // "echtes" Schreiben ans virtuelle FS -- simuliert FSOM_CREATE_ALWAYS:
    // vorhandener Eintrag mit demselben Pfad wird kommentarlos ersetzt.
    int slot = -1;
    for(int i = 0; i < g_fs_count; i++)
        if(g_fs[i].used && strcmp(g_fs[i].path, f->path) == 0) { slot = i; break; }
    if(slot < 0) { slot = g_fs_count++; g_fs[slot].used = true; }
    strncpy(g_fs[slot].path, f->path, sizeof(g_fs[slot].path) - 1);
    memcpy(g_fs[slot].data, f->data, f->len);
    g_fs[slot].len = f->len;
    f->open = false;
}

void furi_hal_rtc_get_datetime(DateTime* dt) { *dt = g_mock_now; }

void fs_reset(void) {
    g_fs_count = 0;
    memset(g_fs, 0, sizeof(g_fs));
}
int fs_count_with_prefix(const char* prefix) {
    int n = 0;
    for(int i = 0; i < g_fs_count; i++)
        if(g_fs[i].used && strncmp(g_fs[i].path, prefix, strlen(prefix)) == 0) n++;
    return n;
}
