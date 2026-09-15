// Host repro for the mf_ultralight_is_pwd_pack_read() helper port: compiles the actual,
// unmodified (bar the host-only include swaps already established by the earlier UL-AES
// Phase 2 repro) patched mf_ultralight.c/.h and exercises the new function against every
// relevant PWD/PACK-page state, plus a direct comparison against the pre-existing, unchanged
// mf_ultralight_is_all_data_read() to demonstrate the robustness gain described in the port
// doc (real PWD=0/PACK=0 content).
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I stubs repro.c
//        mf_ultralight.c -o repro && ./repro
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mf_ultralight.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if(!(cond)) {                                                        \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            g_failures++;                                                    \
        }                                                                    \
    } while(0)

static MfUltralightData* alloc_poisoned(uint8_t poison_byte) {
    // Simulate mf_ultralight_alloc()'s real, non-zeroing malloc(): fill the whole struct with a
    // fixed non-zero pattern, matching the earlier UL-AES Phase 2 repro's approach, so a missed
    // field read would show up as a poison-dependent result instead of silently passing on a
    // zero-initialized host artifact that the real allocator never produces.
    MfUltralightData* data = malloc(sizeof(MfUltralightData));
    memset(data, poison_byte, sizeof(*data));
    data->iso14443_3a_data = iso14443_3a_alloc();
    return data;
}

static void free_data(MfUltralightData* data) {
    iso14443_3a_free(data->iso14443_3a_data);
    free(data);
}

// --- Group 1: types without a config/PWD page never report PWD+PACK read -------------------

static void test_no_pwd_page(void) {
    // MfUltralightTypeOrigin (config_page == 0, so mf_ultralight_get_pwd_page_num() == 0) has
    // no password feature at all -- the function must reject it regardless of pages_read,
    // including the edge case pages_read == pages_total (a fully read card of this type).
    MfUltralightData* data = alloc_poisoned(0xAA);
    data->type = MfUltralightTypeOrigin;
    CHECK(mf_ultralight_get_pwd_page_num(data->type) == 0, "precondition: Origin has no pwd page");

    data->pages_read = 0;
    CHECK(!mf_ultralight_is_pwd_pack_read(data), "no-pwd-page: pages_read=0 must be false");

    data->pages_read = mf_ultralight_get_pages_total(data->type);
    CHECK(
        !mf_ultralight_is_pwd_pack_read(data),
        "no-pwd-page: fully read card of a non-password type must still be false");

    free_data(data);
}

// --- Group 2: password-capable type, the three pages_read states that matter ---------------

static void check_pwd_pack_states(MfUltralightType type, const char* label) {
    uint8_t pwd_page = mf_ultralight_get_pwd_page_num(type);
    CHECK(pwd_page != 0, "precondition: type under test must support PWD/PACK");
    uint16_t pages_total = mf_ultralight_get_pages_total(type);
    // Standard UL/NTAG layout: PWD and PACK are the last two pages (see mf_ultralight.c's own
    // config-page table), so a full read always ends exactly on pwd_page + 2.
    CHECK(pwd_page + 2 == pages_total, "precondition: PWD/PACK expected to be the final 2 pages");

    MfUltralightData* data = alloc_poisoned(0x55);
    data->type = type;

    // Case A: default state before auth is attempted at all (pages_read == 0, as after alloc()
    // + reset(), before the poller runs). Must not claim PWD/PACK were captured.
    data->pages_read = 0;
    CHECK(
        !mf_ultralight_is_pwd_pack_read(data),
        "pwd-pack: pages_read=0 (not started) must be false");

    // Case B: try_default_pass() rolled pages_read back by 2 after a failed default-password
    // attempt on a fully-read card (see mf_ultralight_poller.c,
    // mf_ultralight_poller_handler_try_default_pass(): "instance->data->pages_read -= 2").
    // These two pages hold the poller's own masked zero fill, not the real PWD/PACK -- the
    // one case this helper exists to distinguish from a naive "pages_read == pages_total"
    // check, which the pre-existing mf_ultralight_is_all_data_read() already gets right for
    // this exact case but by a different, non-reusable route (it needs live config-page
    // content, this helper only needs the persisted page count).
    data->pages_read = (uint16_t)(pwd_page);
    CHECK(
        !mf_ultralight_is_pwd_pack_read(data),
        "pwd-pack: pages_read rolled back after failed default-pass must be false");

    // Case C: exactly one page short of PWD/PACK (a read that stopped one page early, e.g. an
    // interrupted/partial read) -- still false, since PACK was never reached.
    data->pages_read = (uint16_t)(pwd_page + 1);
    CHECK(
        !mf_ultralight_is_pwd_pack_read(data),
        "pwd-pack: pages_read == pwd_page+1 (PACK not yet reached) must be false");

    // Case D: full read reaching pages_total, i.e. both PWD and PACK were actually returned by
    // the tag (successful default-password auth, or an unprotected card with AUTH0 disabled).
    data->pages_read = pages_total;
    CHECK(
        mf_ultralight_is_pwd_pack_read(data),
        "pwd-pack: pages_read == pages_total (PWD+PACK captured) must be true");

    fprintf(stderr, "  %s: pwd_page=%u pages_total=%u -- all four states OK\n",
        label,
        (unsigned)pwd_page,
        (unsigned)pages_total);

    free_data(data);
}

static void test_pwd_pack_states(void) {
    // Two independent password-capable types with different config-page geometry (UL11's
    // 20-page layout vs. NTAG213's 45-page layout), to rule out an off-by-one tied to one
    // specific total_pages value.
    check_pwd_pack_states(MfUltralightTypeUL11, "UL11");
    check_pwd_pack_states(MfUltralightTypeNTAG213, "NTAG213");
}

// --- Group 3: persisted-dump path (loaded from a saved file, not a fresh poller read) ------

static void test_loaded_dump(void) {
    // pages_read is a persisted field (see mf_ultralight_load()'s
    // MF_ULTRALIGHT_PAGES_READ_KEY), so a dump loaded from disk with pages_read == pages_total
    // must report PWD/PACK as read too, with no live poller/auth context involved at all.
    MfUltralightData* data = alloc_poisoned(0x11);
    data->type = MfUltralightTypeNTAG213;
    data->pages_read = mf_ultralight_get_pages_total(data->type);
    CHECK(
        mf_ultralight_is_pwd_pack_read(data),
        "loaded-dump: fully-read persisted dump must report PWD/PACK read");
    free_data(data);
}

// --- Group 4: robustness vs. the pre-existing mf_ultralight_is_all_data_read() heuristic ---

static void test_more_robust_than_is_all_data_read(void) {
    // mf_ultralight_is_all_data_read()'s PasswordAuth branch (pre-existing, unchanged by this
    // patch) infers a successful capture from the config page's *content*
    // ((pass != 0) || (pack != 0)), which assumes a real password/pack is never exactly zero.
    // A card whose owner deliberately set PWD=0x00000000/PACK=0x0000 breaks that assumption:
    // the content look identical to the poller's own masked-failure fill. is_pwd_pack_read()
    // only looks at the persisted page count, so it gets this case right where the existing
    // heuristic does not -- demonstrated here directly against the real, unmodified
    // mf_ultralight_is_all_data_read().
    MfUltralightData* data = alloc_poisoned(0x77);
    data->type = MfUltralightTypeNTAG213;
    data->pages_total = mf_ultralight_get_pages_total(data->type);
    data->pages_read = data->pages_total;

    // Set up through the same MfUltralightConfigPages* view mf_ultralight_is_all_data_read()
    // itself reads from (mf_ultralight_get_config_page()), so the test writes to exactly the
    // bytes the function under test reads, independent of this struct's real on-wire packing.
    MfUltralightConfigPages* config = NULL;
    CHECK(mf_ultralight_get_config_page(data, &config), "precondition: NTAG213 has a config page");
    memset(&config->password, 0, sizeof(config->password));
    memset(&config->pack, 0, sizeof(config->pack));

    CHECK(
        !mf_ultralight_is_all_data_read(data),
        "is_all_data_read: real PWD=0/PACK=0 is (mis)read as not fully captured");
    CHECK(
        mf_ultralight_is_pwd_pack_read(data),
        "is_pwd_pack_read: correctly unaffected by PWD/PACK content, only pages_read");

    free_data(data);
}

int main(void) {
    test_no_pwd_page();
    test_pwd_pack_states();
    test_loaded_dump();
    test_more_robust_than_is_all_data_read();

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", g_failures);
        return 1;
    }
}
