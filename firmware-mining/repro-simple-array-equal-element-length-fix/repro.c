// Host repro for the SimpleArray equality element-length fix
// (firmware-mining/simple-array-equal-element-length-fix.md).
//
// Models lib/toolbox/simple_array.c's data/count/config layout and its is_equal function in both
// forms (OLD: my unchanged mntm-012 code, NEW: the same function after applying
// simple_array_equal.patch), plus the two FeliCa/DESFire-shaped wrapper patterns the patch adds
// on top for element types that own a nested SimpleArray of their own
// (felica_system_array_is_equal / mf_desfire_application_array_is_equal /
// mf_desfire_file_data_array_is_equal in the real patch). The array bookkeeping itself (malloc,
// counts, config->type_size) is copied verbatim in spirit from simple_array.c; only furi_check
// calls are dropped (replaced with plain asserts) so this compiles standalone.
//
// Tested:
//  1) Plain byte elements (type_size == 1, as iso15693_3's block_data/block_security and
//     type_4_tag's ndef_data already use): OLD and NEW must agree on every case, since
//     other->count * type_size == other->count when type_size is 1 -- this is the regression
//     check that the fix does not touch the one element type it was already correct for.
//  2) Wide scalar elements (type_size == 8, no nested pointers, e.g. shaped like FelicaArea/
//     FelicaPublicBlock): OLD's memcmp only ever compares `count` bytes regardless of
//     type_size, so for N elements it inspects at most N bytes out of the N*8 that actually
//     make up the array -- swept over many N and many single-byte differences placed at every
//     offset up to N*8, OLD misses (reports "equal" for) every difference that falls outside its
//     truncated N-byte window, NEW (which compares count*type_size bytes) catches all of them.
//  3) Owning elements (a struct holding one int field plus a pointer to its own heap-allocated
//     nested SimpleArray, shaped like FelicaSystem/MfDesfireApplication/MfDesfireFileData):
//     3a. Two independently built copies with identical int field and identical nested-array
//         *content* (but distinct heap pointers, as after any two separate loads/copies) --
//         NEW's plain byte-for-byte compare (correct element length, still no structural
//         awareness) reports them as DIFFERENT in every trial, because it compares the raw
//         pointer bytes, not what they point to. This is the "two copies of one card can never
//         be equal" / spurious shadow-file-write failure mode the entry-wise wrapper exists to
//         fix -- confirms the byte-count fix alone is insufficient for this element type.
//     3b. The same two copies through an entry-wise wrapper (mirrors
//         felica_system_array_is_equal/mf_desfire_application_array_is_equal: same top-level
//         count check, then per-element field compare that recurses into simple_array_is_equal
//         on the nested array instead of memcmp-ing the owning struct) correctly report them as
//         EQUAL in every trial.
//     3c. Negative control: when the nested array's content is actually changed in one copy, or
//         the plain int field differs, or the element counts differ, the entry-wise wrapper
//         correctly reports DIFFERENT in every trial -- it is not vacuously true.
//  4) File-data-shaped elements (a struct holding only a pointer to a nested byte SimpleArray,
//     shaped like MfDesfireFileData): same 3a/3b/3c pattern one level simpler, matching the
//     dedicated mf_desfire_file_data_array_is_equal helper the patch adds.
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long g_checks = 0;
#define CHECK(cond)                    \
    do {                               \
        assert(cond);                  \
        g_checks++;                    \
    } while(0)

// ---- minimal SimpleArray model, mirrors lib/toolbox/simple_array.{c,h}'s layout ----

typedef struct {
    size_t type_size;
} ArrayConfig;

typedef struct {
    const ArrayConfig* config;
    uint8_t* data;
    uint32_t count;
} SimpleArray;

static SimpleArray* arr_alloc(const ArrayConfig* config) {
    SimpleArray* a = calloc(1, sizeof(SimpleArray));
    a->config = config;
    return a;
}

static void arr_init(SimpleArray* a, uint32_t count) {
    a->data = calloc(count, a->config->type_size);
    a->count = count;
}

static void arr_free(SimpleArray* a) {
    free(a->data);
    free(a);
}

static void* arr_get(SimpleArray* a, uint32_t i) {
    return a->data + (size_t)i * a->config->type_size;
}

static const void* arr_cget(const SimpleArray* a, uint32_t i) {
    return a->data + (size_t)i * a->config->type_size;
}

// ---- OLD: simple_array_is_equal(), pre-patch (lib/toolbox/simple_array.c) ----
static bool arr_is_equal_old(const SimpleArray* instance, const SimpleArray* other) {
    if(instance == other) return true;
    return (instance->config == other->config) && (instance->count == other->count) &&
           ((instance->data == other->data) || (instance->data == NULL) || (other->data == NULL) ||
            (memcmp(instance->data, other->data, other->count) == 0));
}

// ---- NEW: simple_array_is_equal(), from simple_array_equal.patch ----
static bool arr_is_equal_new(const SimpleArray* instance, const SimpleArray* other) {
    if(instance == other) return true;
    return (instance->config == other->config) && (instance->count == other->count) &&
           ((instance->data == other->data) || (instance->data == NULL) || (other->data == NULL) ||
            (memcmp(instance->data, other->data, (size_t)other->count * instance->config->type_size) ==
             0));
}

// ==================== Test 1: plain byte elements (type_size == 1) ====================

static const ArrayConfig cfg_u8 = {.type_size = 1};

static void test_byte_elements_regression(void) {
    for(uint32_t n = 0; n <= 12; n++) {
        for(int trial = 0; trial < 20; trial++) {
            SimpleArray* a = arr_alloc(&cfg_u8);
            SimpleArray* b = arr_alloc(&cfg_u8);
            arr_init(a, n ? n : 1);
            arr_init(b, n ? n : 1);
            a->count = b->count = n;
            for(uint32_t i = 0; i < n; i++) {
                ((uint8_t*)a->data)[i] = (uint8_t)(trial * 7 + i);
                ((uint8_t*)b->data)[i] = (uint8_t)(trial * 7 + i);
            }
            bool old_eq = arr_is_equal_old(a, b);
            bool new_eq = arr_is_equal_new(a, b);
            CHECK(old_eq == true);
            CHECK(new_eq == true);

            if(n > 0) {
                // Flip one byte at every offset; OLD and NEW must still agree (both "not equal")
                for(uint32_t off = 0; off < n; off++) {
                    ((uint8_t*)b->data)[off] ^= 0xFF;
                    CHECK(arr_is_equal_old(a, b) == false);
                    CHECK(arr_is_equal_new(a, b) == false);
                    ((uint8_t*)b->data)[off] ^= 0xFF;
                }
            }
            arr_free(a);
            arr_free(b);
        }
    }
}

// ==================== Test 2: wide scalar elements (type_size == 8) ====================

typedef struct {
    uint32_t lo;
    uint32_t hi;
} WideElement; // 8 bytes, no nested pointers -- shaped like FelicaArea/FelicaPublicBlock

static const ArrayConfig cfg_wide = {.type_size = sizeof(WideElement)};

static void test_wide_elements_undercount(void) {
    for(uint32_t n = 1; n <= 10; n++) {
        SimpleArray* a = arr_alloc(&cfg_wide);
        SimpleArray* b = arr_alloc(&cfg_wide);
        arr_init(a, n);
        arr_init(b, n);
        for(uint32_t i = 0; i < n; i++) {
            WideElement e = {.lo = 0x11111111u * (i + 1), .hi = 0x22222222u * (i + 1)};
            memcpy(arr_get(a, i), &e, sizeof(e));
            memcpy(arr_get(b, i), &e, sizeof(e));
        }
        CHECK(arr_is_equal_old(a, b) == true);
        CHECK(arr_is_equal_new(a, b) == true);

        size_t total_bytes = (size_t)n * sizeof(WideElement);
        for(size_t off = 0; off < total_bytes; off++) {
            uint8_t saved = a->data[off];
            a->data[off] ^= 0xFF;

            bool old_eq = arr_is_equal_old(a, b);
            bool new_eq = arr_is_equal_new(a, b);

            // OLD only inspects the first `n` bytes (its byte count is the element count, not
            // element_count * type_size) -- a difference at or past offset n is invisible to it.
            if(off < n) {
                CHECK(old_eq == false);
            } else {
                CHECK(old_eq == true); // confirmed miss: real difference reported as "equal"
            }
            // NEW inspects all n * type_size bytes, so it must catch every difference.
            CHECK(new_eq == false);

            a->data[off] = saved;
        }
        arr_free(a);
        arr_free(b);
    }
}

// ==================== Test 3: owning elements (struct + pointer to nested array) ====================

typedef struct {
    int tag;
    SimpleArray* nested; // owned: allocated/freed with the element, like FelicaSystem's arrays
} OwningElement;

static const ArrayConfig cfg_owning = {.type_size = sizeof(OwningElement)};

static OwningElement make_owning(int tag, const uint8_t* bytes, uint32_t n) {
    OwningElement e;
    e.tag = tag;
    e.nested = arr_alloc(&cfg_u8);
    arr_init(e.nested, n ? n : 1);
    e.nested->count = n;
    memcpy(e.nested->data, bytes, n);
    return e;
}

static void free_owning(OwningElement* e) {
    arr_free(e->nested);
}

// mirrors the real per-element helper (felica_system_is_equal / mf_desfire_application_is_equal)
static bool owning_element_is_equal(const OwningElement* x, const OwningElement* y) {
    return x->tag == y->tag && arr_is_equal_new(x->nested, y->nested);
}

// mirrors felica_system_array_is_equal / mf_desfire_application_array_is_equal
static bool owning_array_is_equal(const SimpleArray* instance, const SimpleArray* other) {
    if(instance->count != other->count) return false;
    for(uint32_t i = 0; i < instance->count; i++) {
        const OwningElement* x = arr_cget(instance, i);
        const OwningElement* y = arr_cget(other, i);
        if(!owning_element_is_equal(x, y)) return false;
    }
    return true;
}

static SimpleArray* build_owning_array(const int* tags, const uint8_t nested_bytes[][4], uint32_t n) {
    SimpleArray* a = arr_alloc(&cfg_owning);
    arr_init(a, n);
    for(uint32_t i = 0; i < n; i++) {
        OwningElement e = make_owning(tags[i], nested_bytes[i], 4);
        memcpy(arr_get(a, i), &e, sizeof(e));
    }
    return a;
}

static void free_owning_array(SimpleArray* a) {
    for(uint32_t i = 0; i < a->count; i++) {
        free_owning(arr_get(a, i));
    }
    arr_free(a);
}

static void test_owning_elements(void) {
    int tags[3] = {1, 2, 3};
    uint8_t nested[3][4] = {{0xAA, 0xBB, 0xCC, 0xDD}, {1, 2, 3, 4}, {9, 8, 7, 6}};

    // Two independently built copies, same tags and same nested content, distinct heap pointers
    // -- exactly the "loaded twice" / "current state vs. freshly re-read" scenario that made
    // FeliCa emulation's shadow-file check never see two identical cards as equal.
    for(int trial = 0; trial < 50; trial++) {
        SimpleArray* a = build_owning_array(tags, nested, 3);
        SimpleArray* b = build_owning_array(tags, nested, 3);

        // 3a: fixed-length but structurally-blind full-struct compare -- WRONG on purpose here,
        // demonstrating why felica_is_equal/mf_desfire_is_equal do NOT call arr_is_equal_new()
        // directly on an owning array.
        CHECK(arr_is_equal_new(a, b) == false);

        // 3b: entry-wise wrapper -- correct.
        CHECK(owning_array_is_equal(a, b) == true);

        free_owning_array(a);
        free_owning_array(b);
    }

    // 3c: negative controls -- entry-wise wrapper must still detect real differences.
    {
        SimpleArray* a = build_owning_array(tags, nested, 3);
        uint8_t nested_diff[3][4] = {{0xAA, 0xBB, 0xCC, 0xDD}, {1, 2, 3, 4}, {9, 8, 7, 5}};
        SimpleArray* b = build_owning_array(tags, nested_diff, 3);
        CHECK(owning_array_is_equal(a, b) == false); // nested content differs
        free_owning_array(a);
        free_owning_array(b);
    }
    {
        int tags_diff[3] = {1, 2, 4};
        SimpleArray* a = build_owning_array(tags, nested, 3);
        SimpleArray* b = build_owning_array(tags_diff, nested, 3);
        CHECK(owning_array_is_equal(a, b) == false); // scalar field differs
        free_owning_array(a);
        free_owning_array(b);
    }
    {
        SimpleArray* a = build_owning_array(tags, nested, 3);
        SimpleArray* b = build_owning_array(tags, nested, 2);
        CHECK(owning_array_is_equal(a, b) == false); // element count differs
        free_owning_array(a);
        free_owning_array(b);
    }
}

// ==================== Test 4: file-data-shaped elements (pointer only, no scalar field) ====================

typedef struct {
    SimpleArray* data; // shaped like MfDesfireFileData { SimpleArray* data; }
} FileDataElement;

static const ArrayConfig cfg_filedata = {.type_size = sizeof(FileDataElement)};

static bool file_data_array_is_equal(const SimpleArray* instance, const SimpleArray* other) {
    if(instance->count != other->count) return false;
    for(uint32_t i = 0; i < instance->count; i++) {
        const FileDataElement* x = arr_cget(instance, i);
        const FileDataElement* y = arr_cget(other, i);
        if(!arr_is_equal_new(x->data, y->data)) return false;
    }
    return true;
}

static SimpleArray* build_filedata_array(const uint8_t contents[][4], uint32_t n) {
    SimpleArray* a = arr_alloc(&cfg_filedata);
    arr_init(a, n);
    for(uint32_t i = 0; i < n; i++) {
        FileDataElement e;
        e.data = arr_alloc(&cfg_u8);
        arr_init(e.data, 4);
        memcpy(e.data->data, contents[i], 4);
        memcpy(arr_get(a, i), &e, sizeof(e));
    }
    return a;
}

static void free_filedata_array(SimpleArray* a) {
    for(uint32_t i = 0; i < a->count; i++) {
        FileDataElement* e = arr_get(a, i);
        arr_free(e->data);
    }
    arr_free(a);
}

static void test_file_data_elements(void) {
    uint8_t contents[2][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}};

    for(int trial = 0; trial < 50; trial++) {
        SimpleArray* a = build_filedata_array(contents, 2);
        SimpleArray* b = build_filedata_array(contents, 2);

        CHECK(arr_is_equal_new(a, b) == false); // pointer-only struct: raw compare is wrong
        CHECK(file_data_array_is_equal(a, b) == true); // entry-wise: correct

        free_filedata_array(a);
        free_filedata_array(b);
    }

    uint8_t contents_diff[2][4] = {{1, 2, 3, 4}, {5, 6, 7, 9}};
    SimpleArray* a = build_filedata_array(contents, 2);
    SimpleArray* b = build_filedata_array(contents_diff, 2);
    CHECK(file_data_array_is_equal(a, b) == false);
    free_filedata_array(a);
    free_filedata_array(b);
}

int main(void) {
    test_byte_elements_regression();
    test_wide_elements_undercount();
    test_owning_elements();
    test_file_data_elements();
    printf("%ld/%ld checks passed\n", g_checks, g_checks);
    return 0;
}
