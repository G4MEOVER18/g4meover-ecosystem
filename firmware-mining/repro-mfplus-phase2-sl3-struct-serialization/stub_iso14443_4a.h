#pragma once
// Host stand-in for everything mf_plus.c/mf_plus_i.c pull in from the real
// <lib/nfc/protocols/iso14443_4a/iso14443_4a.h> + <nfc/nfc_device_base.h> chain: just enough
// surface (an opaque Iso14443_4aData handle with a UID round-trip, NfcDeviceBase, NfcDeviceNameType)
// to compile and run mf_plus.c/.h and mf_plus_i.c/.h unmodified. Nothing here is exercised by the
// SL3 struct/serialization code under test; it only has to make the surrounding file compile and
// let a full mf_plus_load()/mf_plus_save() round-trip carry the (unrelated, pre-existing) base
// card identity along for a realistic end-to-end test.
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "furi.h"
#include "furi_string.h"
#include "flipper_format.h"

typedef enum {
    NfcDeviceNameTypeFull,
    NfcDeviceNameTypeShort,
} NfcDeviceNameType;

#define ISO14443_4A_STUB_UID_MAX 10

typedef struct Iso14443_4aData {
    uint8_t uid[ISO14443_4A_STUB_UID_MAX];
    size_t uid_len;
} Iso14443_4aData;

static inline Iso14443_4aData* iso14443_4a_alloc(void) {
    return calloc(1, sizeof(Iso14443_4aData));
}

static inline void iso14443_4a_free(Iso14443_4aData* d) {
    free(d);
}

static inline void iso14443_4a_reset(Iso14443_4aData* d) {
    memset(d, 0, sizeof(*d));
}

static inline void iso14443_4a_copy(Iso14443_4aData* d, const Iso14443_4aData* o) {
    *d = *o;
}

static inline bool iso14443_4a_is_equal(const Iso14443_4aData* a, const Iso14443_4aData* b) {
    return a->uid_len == b->uid_len && memcmp(a->uid, b->uid, a->uid_len) == 0;
}

static inline const uint8_t* iso14443_4a_get_uid(const Iso14443_4aData* d, size_t* uid_len) {
    *uid_len = d->uid_len;
    return d->uid;
}

static inline bool
    iso14443_4a_set_uid(Iso14443_4aData* d, const uint8_t* uid, size_t uid_len) {
    if(uid_len > ISO14443_4A_STUB_UID_MAX) return false;
    memcpy(d->uid, uid, uid_len);
    d->uid_len = uid_len;
    return true;
}

static inline bool iso14443_4a_save(const Iso14443_4aData* d, FlipperFormat* ff) {
    uint32_t len32 = (uint32_t)d->uid_len;
    if(!flipper_format_write_uint32(ff, "Iso4Stub UID Len", &len32, 1)) return false;
    return flipper_format_write_hex(ff, "Iso4Stub UID", d->uid, d->uid_len);
}

static inline bool iso14443_4a_load(Iso14443_4aData* d, FlipperFormat* ff, uint32_t version) {
    (void)version;
    uint32_t len32 = 0;
    if(!flipper_format_read_uint32(ff, "Iso4Stub UID Len", &len32, 1)) return false;
    if(len32 > ISO14443_4A_STUB_UID_MAX) return false;
    d->uid_len = len32;
    return flipper_format_read_hex(ff, "Iso4Stub UID", d->uid, d->uid_len);
}

typedef void* (*NfcDeviceAlloc)(void);
typedef void (*NfcDeviceFree)(void*);
typedef void (*NfcDeviceReset)(void*);
typedef void (*NfcDeviceCopy)(void*, const void*);
typedef bool (*NfcDeviceVerify)(void*, const FuriString*);
typedef bool (*NfcDeviceLoad)(void*, FlipperFormat*, uint32_t);
typedef bool (*NfcDeviceSave)(const void*, FlipperFormat*);
typedef bool (*NfcDeviceEqual)(const void*, const void*);
typedef const char* (*NfcDeviceGetName)(const void*, NfcDeviceNameType);
typedef const uint8_t* (*NfcDeviceGetUid)(const void*, size_t*);
typedef bool (*NfcDeviceSetUid)(void*, const uint8_t*, size_t);
typedef void* (*NfcDeviceGetBaseData)(const void*);

typedef struct {
    const char* protocol_name;
    NfcDeviceAlloc alloc;
    NfcDeviceFree free;
    NfcDeviceReset reset;
    NfcDeviceCopy copy;
    NfcDeviceVerify verify;
    NfcDeviceLoad load;
    NfcDeviceSave save;
    NfcDeviceEqual is_equal;
    NfcDeviceGetName get_name;
    NfcDeviceGetUid get_uid;
    NfcDeviceSetUid set_uid;
    NfcDeviceGetBaseData get_base_data;
} NfcDeviceBase;
