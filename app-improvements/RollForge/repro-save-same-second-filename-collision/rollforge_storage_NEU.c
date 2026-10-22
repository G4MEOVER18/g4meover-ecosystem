#include "rollforge_storage_NEU.h"
#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <stdio.h>
#include <string.h>

bool rollforge_save_sig_NEU(const RfSig* sig, uint32_t frequency, const char* filename_hint) {
    if(!sig || sig->count == 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/subghz");
    storage_simply_mkdir(storage, RF_SAVE_DIR);

    // Dateiname: rollforge/<hint>_HHMMSS.sub
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    const char* hint = filename_hint ? filename_hint : "capture";
    char path[128];
    snprintf(path, sizeof(path), "%s/%s_%02u%02u%02u.sub",
        RF_SAVE_DIR, hint, dt.hour, dt.minute, dt.second);

    // Zwei Saves derselben Sekunde (z. B. "Save" zweimal gedrueckt) treffen
    // sonst denselben Pfad -- FSOM_CREATE_ALWAYS unten wuerde die erste
    // Aufnahme stillschweigend ueberschreiben, ohne dass "Gespeichert" das
    // irgendwie von einem echten Neuanlegen unterscheidet. Bei Kollision
    // einen laufenden Suffix anhaengen, bis ein freier Pfad gefunden ist.
    if(storage_file_exists(storage, path)) {
        for(unsigned i = 1; i <= 99; i++) {
            char alt[128];
            snprintf(alt, sizeof(alt), "%s/%s_%02u%02u%02u_%u.sub",
                RF_SAVE_DIR, hint, dt.hour, dt.minute, dt.second, i);
            if(!storage_file_exists(storage, alt)) {
                strncpy(path, alt, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
                break;
            }
        }
    }

    File* f = storage_file_alloc(storage);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        // Flipper SubGhz RAW Format
        char hdr[256];
        int n = snprintf(hdr, sizeof(hdr),
            "Filetype: Flipper SubGhz RAW File\n"
            "Version: 1\n"
            "Frequency: %lu\n"
            "Preset: FuriHalSubGhzPresetOok650Async\n"
            "Protocol: RAW\n"
            "RAW_Data: ",
            (unsigned long)frequency);
        storage_file_write(f, hdr, (size_t)n);

        // RAW Data: positive=HIGH(level=1), negative=LOW. Werte als ASCII.
        // Format: "350 -300 700 -650 ..."
        char buf[24];
        size_t count = sig->count;
        for(size_t i = 0; i < count; i++) {
            uint32_t packed = sig->buf[i];
            bool     level  = (packed & 0x80000000UL) != 0;
            int32_t  dur    = (int32_t)(packed & 0x7FFFFFFFUL);
            if(!level) dur = -dur;
            n = snprintf(buf, sizeof(buf), (i == 0) ? "%ld" : " %ld", (long)dur);
            storage_file_write(f, buf, (size_t)n);
        }
        storage_file_write(f, "\n", 1);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
