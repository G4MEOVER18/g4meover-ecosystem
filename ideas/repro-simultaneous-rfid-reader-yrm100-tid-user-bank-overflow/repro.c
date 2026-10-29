#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ---- verbatim from yrm100x_tag.h (haffnerriley/Simultaneous-UHF-RFID-FlipperZero, commit 1dfefed) ---- */
#define MAX_BANK_SIZE 200
typedef enum { ReservedBank, EPCBank, TIDBank, UserBank, KillPwd, AccessPwd, FileZero} BankType;

typedef struct {
    uint8_t kill_password[4];
    uint8_t access_password[4];
} ReservedMemoryBank;

typedef struct {
    size_t size;
    uint8_t data[MAX_BANK_SIZE];
    uint16_t pc;
    uint16_t crc;
} EPCMemoryBank;

typedef struct {
    size_t size;
    uint8_t data[MAX_BANK_SIZE];
} TIDMemoryBank;

typedef struct {
    size_t size;
    uint8_t data[MAX_BANK_SIZE];
} UserMemoryBank;

typedef struct {
    ReservedMemoryBank* reserved;
    EPCMemoryBank* epc;
    TIDMemoryBank* tid;
    UserMemoryBank* user;
} UHFTag;

/* ---- verbatim from yrm100x_tag.c ---- */
UHFTag* uhf_tag_alloc() {
    UHFTag* uhf_tag = (UHFTag*)malloc(sizeof(UHFTag));
    uhf_tag->reserved = (ReservedMemoryBank*)malloc(sizeof(ReservedMemoryBank));
    uhf_tag->epc = (EPCMemoryBank*)malloc(sizeof(EPCMemoryBank));
    uhf_tag->tid = (TIDMemoryBank*)malloc(sizeof(TIDMemoryBank));
    uhf_tag->user = (UserMemoryBank*)malloc(sizeof(UserMemoryBank));
    return uhf_tag;
}

void uhf_tag_free(UHFTag* uhf_tag) {
    if(uhf_tag == NULL) return;
    free(uhf_tag->reserved);
    free(uhf_tag->epc);
    free(uhf_tag->tid);
    free(uhf_tag->user);
    free(uhf_tag);
}

void uhf_tag_set_epc(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    memcpy(uhf_tag->epc->data, data_in, size);
    uhf_tag->epc->size = size;
}

size_t uhf_tag_get_epc_size(UHFTag* uhf_tag) {
    return uhf_tag->epc->size;
}

#ifndef FIXED
/* ALT: unchanged upstream -- no bound check against MAX_BANK_SIZE */
void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    memcpy(uhf_tag->tid->data, data_in, size);
    uhf_tag->tid->size = size;
}

void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    memcpy(uhf_tag->user->data, data_in, size);
    uhf_tag->user->size = size;
}
#else
/* NEU: additive clamp to the fixed backing array size, same pattern style
   as the rest of this codebase's bank-size-bounded structs */
void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > MAX_BANK_SIZE) size = MAX_BANK_SIZE;
    memcpy(uhf_tag->tid->data, data_in, size);
    uhf_tag->tid->size = size;
}

void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > MAX_BANK_SIZE) size = MAX_BANK_SIZE;
    memcpy(uhf_tag->user->data, data_in, size);
    uhf_tag->user->size = size;
}
#endif

/* ---- verbatim response-parsing fragment of
   m100_read_label_data_storage() in yrm100x_module.c (lines 256-277),
   extracted into a standalone function taking the already-validated
   response frame as input (setup_and_send_rx()'s FRAME_START/FRAME_END/
   checksum checks happened before this point in the real function and
   are not re-tested here -- this isolates exactly the length arithmetic
   and the memory-bank write that follows it) ---- */
typedef enum {
    M100SuccessResponse,
    M100EmptyResponse,
    M100ValidationFail,
    M100ChecksumFail,
    M100NoTagResponse,
    M100MemoryOverrun,
    M100APWrong
} M100ResponseType;

M100ResponseType apply_bank_response(UHFTag* uhf_tag, BankType bank, uint8_t* data) {
    uint8_t rtn_command = data[2];
    uint16_t payload_len = data[3];
    payload_len = (payload_len << 8) + data[4];

    if(rtn_command == 0xFF) {
        if(payload_len == 0x01) return M100NoTagResponse;
        return M100MemoryOverrun;
    }

    size_t ptr_offset = 5 + uhf_tag_get_epc_size(uhf_tag) + 3;
    size_t bank_data_length = payload_len - (ptr_offset - 5);

    if(bank == TIDBank) {
        uhf_tag_set_tid(uhf_tag, data + ptr_offset, bank_data_length);
    } else if(bank == UserBank) {
        uhf_tag_set_user(uhf_tag, data + ptr_offset, bank_data_length);
    }
    return M100SuccessResponse;
}

/* ---- host repro harness ---- */
static int checks_total = 0;
static int checks_failed = 0;

static void check(const char* name, bool cond) {
    checks_total++;
    if(!cond) {
        checks_failed++;
        printf("FAIL: %s\n", name);
    } else {
        printf("PASS: %s\n", name);
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Case 1: regression -- a normal, self-imposed 64-word (128-byte)
       TID read request answered with exactly matching data length.
       Response header: [0xBB][0x01][rtn][len_hi][len_lo][ptr...] */
    {
        UHFTag* tag = uhf_tag_alloc();
        uint8_t epc8[8] = {1,2,3,4,5,6,7,8};
        uhf_tag_set_epc(tag, epc8, 8); /* epc_size = 8 -> ptr_offset = 5+8+3 = 16 */
        size_t ptr_offset = 16;
        uint16_t payload_len = (ptr_offset - 5) + 128; /* honest 128-byte TID payload */
        size_t total = ptr_offset + 128 + 2;
        uint8_t* data = (uint8_t*)calloc(total, 1);
        data[2] = 0x00; /* not an error response */
        data[3] = (payload_len >> 8) & 0xFF;
        data[4] = payload_len & 0xFF;
        for(size_t i = 0; i < 128; i++) data[ptr_offset + i] = (uint8_t)(i + 1);
        apply_bank_response(tag, TIDBank, data);
        bool ok = true;
        for(size_t i = 0; i < 128; i++) if(tag->tid->data[i] != (uint8_t)(i+1)) ok = false;
        check("case1 regression: honest 128-byte TID response stored correctly", ok && tag->tid->size == 128);
        free(data);
        uhf_tag_free(tag);
    }

    /* Case 2: the fund -- a reader-module response claims a TID payload
       larger than MAX_BANK_SIZE (200), e.g. because the connected tag
       sent back more data than the app ever requested. ALT: heap
       buffer overflow write past tag->tid->data[200]. NEU: clamped,
       data[] stores exactly the first 200 bytes, no overflow. */
    {
        UHFTag* tag = uhf_tag_alloc();
        uint8_t epc8[8] = {1,2,3,4,5,6,7,8};
        uhf_tag_set_epc(tag, epc8, 8);
        size_t ptr_offset = 16;
        size_t oversized = 300; /* > MAX_BANK_SIZE */
        uint16_t payload_len = (uint16_t)((ptr_offset - 5) + oversized);
        size_t total = ptr_offset + oversized + 2;
        uint8_t* data = (uint8_t*)calloc(total, 1);
        data[2] = 0x00;
        data[3] = (payload_len >> 8) & 0xFF;
        data[4] = payload_len & 0xFF;
        for(size_t i = 0; i < oversized; i++) data[ptr_offset + i] = 0x41;
        apply_bank_response(tag, TIDBank, data);
#ifdef FIXED
        check("case2 fund: oversized 300-byte TID response clamped to 200, no overflow",
              tag->tid->size == MAX_BANK_SIZE);
#else
        /* If we get here under ASan without the fund triggering, the clamp
           is missing and the preceding memcpy already overflowed the heap
           allocation (ASan aborts the process before this line runs). */
        check("case2 fund: oversized 300-byte TID response (ASan should abort before this line)",
              false);
#endif
        free(data);
        uhf_tag_free(tag);
    }

    /* Case 3: same fund via the UserBank path, confirming it's not
       TID-specific. */
    {
        UHFTag* tag = uhf_tag_alloc();
        uint8_t epc8[8] = {1,2,3,4,5,6,7,8};
        uhf_tag_set_epc(tag, epc8, 8);
        size_t ptr_offset = 16;
        size_t oversized = 512;
        uint16_t payload_len = (uint16_t)((ptr_offset - 5) + oversized);
        size_t total = ptr_offset + oversized + 2;
        uint8_t* data = (uint8_t*)calloc(total, 1);
        data[2] = 0x00;
        data[3] = (payload_len >> 8) & 0xFF;
        data[4] = payload_len & 0xFF;
        apply_bank_response(tag, UserBank, data);
#ifdef FIXED
        check("case3 fund (UserBank): oversized 512-byte response clamped to 200, no overflow",
              tag->user->size == MAX_BANK_SIZE);
#else
        check("case3 fund (UserBank): oversized 512-byte response (ASan should abort before this line)",
              false);
#endif
        free(data);
        uhf_tag_free(tag);
    }

    printf("\n%s (%d total, %d failed)\n",
        checks_failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED",
        checks_total, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
