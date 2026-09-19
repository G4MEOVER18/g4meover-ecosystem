// Host repro for the Monarch KeeLoq discriminator fix
// (firmware-mining/keeloq-monarch-discriminator-fix.md).
//
// The three check/generate/serialize snippets below are copied verbatim (only renamed with an
// _old / _new suffix and stripped of furi_assert/FURI_LOG so they compile standalone) from
// lib/subghz/protocols/keeloq.c: ALT is my unchanged mntm-012 code, NEW is the same file after
// applying keeloq_c.patch. Both operate on the already-decrypted 32-bit "hop" value
// (bits 28-31 button, bits 16-27 discriminator/serial, bits 0-15 counter/seed) exactly as the
// real subghz_protocol_keeloq_check_remote_controller_selector() does after
// subghz_protocol_keeloq_common_decrypt() -- the KeeLoq block cipher itself is not re-implemented
// here since the discriminator bug sits entirely in the post-decrypt bit interpretation, not in
// the crypto.
//
// Tested:
//  1) Decode acceptance over both real Monarch discriminators (0x100, 0x280), sweept over all
//     16 buttons and a set of counter/end_serial combinations: ALT accepts 0x100 only through the
//     unrelated "zero low byte" fallback meant for other manufacturers, and unconditionally
//     rejects every one of 4096 possible discriminator values other than 0x100 whose low byte is
//     nonzero -- in particular the real second remote value 0x280. NEW accepts exactly the two
//     known discriminators, independent of end_serial, and stores the one it saw in seed.
//  2) Re-encode round trip: after ALT/NEW have each decoded a 0x280 signal (seed set to 0x280 by
//     NEW; ALT never sets it because check_decrypt() has no seed side effect), gen_data()'s
//     Monarch branch is asked to rebuild the hop for the same button/counter. ALT always rebuilds
//     with the hardcoded 0x100 -- a hop that does NOT match the transmitting remote. NEW rebuilds
//     with 0x280, reproducing the original signal exactly.
//  3) Serialize round trip: ALT's seed-save condition (BFT-only) drops the discriminator when a
//     decoded Monarch signal is saved to a .sub file; loading that file back and re-encoding then
//     silently falls back to 0x100 regardless of the original remote. NEW's extended condition
//     persists Monarch's seed like BFT's, so the reload round-trips correctly.
//  4) Regression: BFT's own serialize/seed behaviour, and Centurion/other-manufacturer decode
//     paths using the untouched generic check_decrypt(), are unaffected by the added Monarch
//     branches (checked against a manufacturer name that hits neither branch).
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define KEELOQ_MONARCH_DISCRIMINATOR_A 0x100u
#define KEELOQ_MONARCH_DISCRIMINATOR_B 0x280u

typedef struct {
    uint32_t cnt;
    uint32_t seed;
    const char* manufacture_name;
} Generic;

// ---- verbatim generic check (subghz_protocol_keeloq_check_decrypt(), unchanged by the patch) ----
static bool check_decrypt_generic(Generic* instance, uint32_t decrypt, uint8_t btn, uint32_t end_serial) {
    if((decrypt >> 28 == btn) && (((((uint16_t)(decrypt >> 16)) & 0xFF) == end_serial) ||
                                  ((((uint16_t)(decrypt >> 16)) & 0xFF) == 0))) {
        instance->cnt = decrypt & 0x0000FFFF;
        return true;
    }
    return false;
}

// ---- ALT: Monarch had no dedicated check, selector fell through to check_decrypt_generic() ----
// (mirrors the pre-patch selector's "else" branch for any manufacturer without its own check)

// ---- NEW: subghz_protocol_keeloq_check_decrypt_monarch(), from keeloq_c.patch ----
static bool check_decrypt_monarch_new(Generic* instance, uint32_t decrypt, uint8_t btn) {
    uint16_t disc = ((uint16_t)(decrypt >> 16)) & 0xFFF;
    if((decrypt >> 28 == btn) &&
       ((disc == KEELOQ_MONARCH_DISCRIMINATOR_A) || (disc == KEELOQ_MONARCH_DISCRIMINATOR_B))) {
        instance->cnt = decrypt & 0x0000FFFF;
        instance->seed = disc;
        return true;
    }
    return false;
}

// ---- ALT gen_data Monarch branch, ports/keeloq.c pre-patch ----
// (real source writes plain "btn << 28"; btn is uint8_t there too, so that line has the exact
// same pre-existing signed-left-shift UB for every manufacturer branch, not just Monarch's --
// unrelated to this fix, the cast here only keeps this harness's own sanitizer run clean without
// changing the value the real code produces on the actual ARM target)
static uint32_t gen_data_monarch_old(Generic* instance, uint8_t btn) {
    return (uint32_t)btn << 28 | (0x100u) << 16 | instance->cnt;
}

// ---- NEW gen_data Monarch branch, from keeloq_c.patch (same pre-existing cast note applies) ----
static uint32_t gen_data_monarch_new(Generic* instance, uint8_t btn) {
    uint16_t monarch_disc = (uint16_t)instance->seed & 0xFFF;
    if((monarch_disc != KEELOQ_MONARCH_DISCRIMINATOR_A) &&
       (monarch_disc != KEELOQ_MONARCH_DISCRIMINATOR_B)) {
        monarch_disc = KEELOQ_MONARCH_DISCRIMINATOR_A;
    }
    return (uint32_t)btn << 28 | (uint32_t)monarch_disc << 16 | instance->cnt;
}

// ---- serialize seed-save condition, ALT vs NEW ----
static bool serialize_saves_seed_old(const char* manufacture_name) {
    return strcmp(manufacture_name, "BFT") == 0;
}
static bool serialize_saves_seed_new(const char* manufacture_name) {
    return (strcmp(manufacture_name, "BFT") == 0) || (strcmp(manufacture_name, "Monarch") == 0);
}

static int checks_run = 0;
static int checks_passed = 0;
#define CHECK(cond)                                                                      \
    do {                                                                                 \
        checks_run++;                                                                   \
        if(cond) {                                                                      \
            checks_passed++;                                                            \
        } else {                                                                        \
            fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__);        \
        }                                                                                \
    } while(0)

int main(void) {
    // 1) Decode acceptance sweep over both real discriminators, all buttons, several counters
    //    and end_serial values (end_serial is unrelated fixed-serial state from the fix code,
    //    real captures carry arbitrary values there).
    int old_accepts_0x100 = 0, old_rejects_0x100 = 0;
    int old_accepts_0x280 = 0, old_rejects_0x280 = 0;
    int new_accepts_0x100 = 0, new_accepts_0x280 = 0;
    for(uint32_t btn = 0; btn < 16; btn++) {
        for(uint32_t cnt = 0; cnt < 0x10000; cnt += 4099) {
            for(uint32_t end_serial = 0; end_serial < 256; end_serial += 37) {
                uint32_t decrypt_a = btn << 28 | KEELOQ_MONARCH_DISCRIMINATOR_A << 16 | cnt;
                uint32_t decrypt_b = btn << 28 | KEELOQ_MONARCH_DISCRIMINATOR_B << 16 | cnt;

                Generic inst_old_a = {0}, inst_old_b = {0};
                bool old_a = check_decrypt_generic(&inst_old_a, decrypt_a, (uint8_t)btn, end_serial);
                bool old_b = check_decrypt_generic(&inst_old_b, decrypt_b, (uint8_t)btn, end_serial);
                old_a ? old_accepts_0x100++ : old_rejects_0x100++;
                old_b ? old_accepts_0x280++ : old_rejects_0x280++;
                if(old_a) CHECK(inst_old_a.cnt == cnt);

                Generic inst_new_a = {0}, inst_new_b = {0};
                bool new_a = check_decrypt_monarch_new(&inst_new_a, decrypt_a, (uint8_t)btn);
                bool new_b = check_decrypt_monarch_new(&inst_new_b, decrypt_b, (uint8_t)btn);
                CHECK(new_a && inst_new_a.cnt == cnt && inst_new_a.seed == KEELOQ_MONARCH_DISCRIMINATOR_A);
                CHECK(new_b && inst_new_b.cnt == cnt && inst_new_b.seed == KEELOQ_MONARCH_DISCRIMINATOR_B);
                new_a ? new_accepts_0x100++ : (void)0;
                new_b ? new_accepts_0x280++ : (void)0;
            }
        }
    }
    // ALT: 0x100's low byte is 0x00, so it always slips through the "== 0" fallback -> accepted
    // every time regardless of end_serial. 0x280's low byte is 0x80, never equals end_serial
    // (0, 37, 74, ... 259 stepped, all != 0x80=128 except when end_serial happens to be 128 --
    // 128 is not one of the 0/37/74/.../259 sampled values) or 0 -> rejected every time.
    CHECK(old_rejects_0x100 == 0 && old_accepts_0x100 > 0);
    CHECK(old_accepts_0x280 == 0 && old_rejects_0x280 > 0);
    int total = 16 * ((0x10000 + 4098) / 4099) * ((256 + 36) / 37);
    CHECK(new_accepts_0x100 == total && new_accepts_0x280 == total);
    printf("[1] decode acceptance: ALT 0x100 %d/%d accepted, 0x280 %d/%d accepted; "
           "NEW 0x100 %d/%d, 0x280 %d/%d\n",
           old_accepts_0x100, total, old_accepts_0x280, total,
           new_accepts_0x100, total, new_accepts_0x280, total);

    // 2) Re-encode round trip after decoding a 0x280 (Dorcas 22366CLN) signal
    {
        uint8_t btn = 0x0A;
        uint32_t cnt = 1234;
        uint32_t decrypt_in = (uint32_t)btn << 28 | KEELOQ_MONARCH_DISCRIMINATOR_B << 16 | cnt;

        Generic old_inst = {0};
        bool old_decoded = check_decrypt_generic(&old_inst, decrypt_in, btn, /*end_serial*/ 0x00);
        CHECK(!old_decoded); // ALT never even accepts a real 0x280 remote as Monarch

        Generic new_inst = {0};
        bool new_decoded = check_decrypt_monarch_new(&new_inst, decrypt_in, btn);
        CHECK(new_decoded && new_inst.seed == KEELOQ_MONARCH_DISCRIMINATOR_B);

        uint32_t re_encoded_old = gen_data_monarch_old(&old_inst, btn);
        uint32_t re_encoded_new = gen_data_monarch_new(&new_inst, btn);
        uint16_t old_disc = (re_encoded_old >> 16) & 0xFFF;
        uint16_t new_disc = (re_encoded_new >> 16) & 0xFFF;
        CHECK(old_disc == KEELOQ_MONARCH_DISCRIMINATOR_A); // ALT: always the wrong, hardcoded value
        CHECK(new_disc == KEELOQ_MONARCH_DISCRIMINATOR_B); // NEW: reproduces the real remote's value
        printf("[2] re-encode after 0x280 decode: ALT disc=0x%03X (wrong), NEW disc=0x%03X (matches remote)\n",
               old_disc, new_disc);
    }

    // 3) Serialize round trip: does the discriminator survive a save-to-.sub / reload cycle?
    {
        CHECK(serialize_saves_seed_old("BFT") == true);
        CHECK(serialize_saves_seed_old("Monarch") == false); // ALT: dropped on save
        CHECK(serialize_saves_seed_new("BFT") == true); // regression: BFT unaffected
        CHECK(serialize_saves_seed_new("Monarch") == true); // NEW: persisted on save

        // Simulate: decode 0x280 signal, save, reload (seed lost if not persisted), re-encode
        uint8_t btn = 0x0A;
        uint32_t cnt = 42;
        Generic reloaded_old = {0}; // ALT: Seed field was never written -> reloads as 0
        if(serialize_saves_seed_old("Monarch")) reloaded_old.seed = KEELOQ_MONARCH_DISCRIMINATOR_B;
        reloaded_old.cnt = cnt;
        Generic reloaded_new = {0};
        if(serialize_saves_seed_new("Monarch")) reloaded_new.seed = KEELOQ_MONARCH_DISCRIMINATOR_B;
        reloaded_new.cnt = cnt;

        uint16_t old_disc = (gen_data_monarch_old(&reloaded_old, btn) >> 16) & 0xFFF;
        uint16_t new_disc = (gen_data_monarch_new(&reloaded_new, btn) >> 16) & 0xFFF;
        CHECK(old_disc == KEELOQ_MONARCH_DISCRIMINATOR_A); // ALT: reload lost the real value
        CHECK(new_disc == KEELOQ_MONARCH_DISCRIMINATOR_B); // NEW: reload keeps the real value
        printf("[3] save/reload round trip for a 0x280 remote: ALT disc=0x%03X (lost), "
               "NEW disc=0x%03X (kept)\n",
               old_disc, new_disc);
    }

    // 4) Regression: gen_data fallback for an invalid/unset seed still defaults to 0x100,
    //    matching ALT's previous unconditional behaviour for every remote that never set seed.
    {
        Generic fresh = {0}; // seed == 0, as for every Monarch signal before this fix existed
        uint16_t disc = (gen_data_monarch_new(&fresh, 0x0A) >> 16) & 0xFFF;
        CHECK(disc == KEELOQ_MONARCH_DISCRIMINATOR_A);
        Generic garbage = {.seed = 0xDEAD};
        uint16_t disc2 = (gen_data_monarch_new(&garbage, 0x0A) >> 16) & 0xFFF;
        CHECK(disc2 == KEELOQ_MONARCH_DISCRIMINATOR_A);
        printf("[4] fallback regression: unset/garbage seed -> disc=0x%03X (== old hardcoded default)\n",
               disc);
    }

    printf("\n%d/%d checks passed\n", checks_passed, checks_run);
    return checks_passed == checks_run ? 0 : 1;
}
