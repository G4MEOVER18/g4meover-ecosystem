// repro_old.c -- reproduces the ALT (current) mag_device_load_data() naming
// path in mag_device.c: strncpy(dev->dev_name, filename_stem, MAG_DEV_NAME_MAX_LEN).
// Host-only, no Furi/Flipper types needed -- pure string/heap logic.
//
// Build: gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_old repro_old.c
// Run:   ./repro_old
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAG_DEV_NAME_MAX_LEN 22

// Mirrors MagDevice (mag_device.h): dev_name is the field this repro
// targets; the real struct has more members after it in the heap
// allocation, which is exactly what a NUL-less dev_name would read into.
typedef struct {
    char dev_name[MAG_DEV_NAME_MAX_LEN + 1];
} MagDeviceOld;

// Mirrors mag_device_load_data() in mag_device.c: after
// path_extract_filename(path, filename, true) strips the ".mag"
// extension, the remaining stem is copied with a bare strncpy().
static void mag_device_load_data_old(MagDeviceOld* dev, const char* filename_stem) {
    strncpy(dev->dev_name, filename_stem, MAG_DEV_NAME_MAX_LEN);
}

int main(void) {
    // A 22-character filename stem -- e.g. a card saved under a name of
    // that length, or any file dropped into the app folder with a long
    // enough name. strncpy(dst, src, 22) with strlen(src) >= 22 copies
    // exactly 22 bytes and appends NO terminator.
    char filename_stem[MAG_DEV_NAME_MAX_LEN + 1];
    memset(filename_stem, 'F', MAG_DEV_NAME_MAX_LEN);
    filename_stem[MAG_DEV_NAME_MAX_LEN] = '\0';

    // malloc(), not calloc() -- matches mag_device_alloc()'s
    // malloc(sizeof(MagDevice)); dev_name is never explicitly initialized.
    MagDeviceOld* dev = malloc(sizeof(MagDeviceOld));
    if(!dev) return 1;
    // Poison the whole allocation with a non-NUL byte pattern to model
    // uninitialized heap memory that happens to contain no NUL -- the
    // case this bug depends on, and no different from real heap garbage.
    memset(dev, 'A', sizeof(*dev));

    mag_device_load_data_old(dev, filename_stem);

    // Mirrors the CLI-arg / "favorites" launch path in mag.c
    // (mag_device_load_data(mag->mag_dev, mag->args, true);), which has
    // NO subsequent mag_device_set_name() call to re-terminate dev_name --
    // unlike the interactive mag_file_select() path. Any later %s read of
    // dev_name (mag_scene_emulate.c, mag_scene_delete_confirm.c,
    // mag_scene_saved_info.c, mag_device_delete()'s fallback path) behaves
    // exactly like this strlen() call.
    size_t len = strlen(dev->dev_name); // OOB read: scans past the 23-byte allocation
    printf("ALT: dev_name read as C string, len=%zu (should have trapped under ASan first)\n", len);

    free(dev);
    return 0;
}
