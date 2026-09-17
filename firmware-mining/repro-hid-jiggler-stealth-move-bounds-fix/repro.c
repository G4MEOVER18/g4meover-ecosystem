// Host repro: HID Mouse Jiggler (Stealth) movement-delta narrowing defect.
//
// Mirrors the exact two computation lines (plus the call they feed) from
// hid_mouse_jiggler_stealth_timer_callback() in
// applications/system/hid_app/views/hid_mouse_jiggler_stealth.c, ALT (my
// unmodified mntm-012 base, byte-identical to the Unleashed unlshd-093
// pre-fix state for these lines) vs. NEU (Unleashed unlshd-093, commit
// c742dfe6c, PR #1111, applied 1:1). The real HID call target is
//   void hid_hal_mouse_move(Hid* instance, int8_t dx, int8_t dy);
// (applications/system/hid_app/hid.h) -- so capture_move() below reproduces
// that exact parameter type. This is not a reimplementation of the movement
// logic: the ALT/NEU expressions are copied verbatim from the two source
// states, and the narrowing that matters happens at the real call boundary
// (int argument into an int8_t parameter), which capture_move() reproduces
// faithfully because it is declared with the same int8_t parameter types as
// hid_hal_mouse_move() -- the compiler performs the identical conversion
// here as it would in the real firmware build.
//
// The View/Hid/Furi machinery around the two lines (with_view_model, Hid*,
// FuriTimer, canvas drawing) carries no part of the defect and is not
// reproduced, matching the established lighter-weight repro style already
// used in this repo for UI/scene-embedded logic bugs (see
// firmware-mining/repro-mifare-fuzzer-load-uids-no-valid-line-hang/repro.c).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// --- real call target signature (applications/system/hid_app/hid.h) ---
static int8_t g_last_dx, g_last_dy;
static void capture_move(int8_t dx, int8_t dy) {
    g_last_dx = dx;
    g_last_dy = dy;
}

// --- ALT: verbatim expressions from my unmodified base (== Unleashed pre-fix) ---
// int move_x = (rand() % 2001) - 1000; // Randomly between -1000 and 1000
// int move_y = (rand() % 2001) - 1000; // Randomly between -1000 and 1000
// hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
static void alt_move_call(int raw_x, int raw_y, int* intended_x, int* intended_y) {
    int move_x = raw_x - 1000; // raw_x in [0,2000], verbatim range of (rand() % 2001)
    int move_y = raw_y - 1000;
    *intended_x = move_x;
    *intended_y = move_y;
    capture_move(move_x, move_y); // <-- same narrowing the real call performs
}

// --- NEU: verbatim expressions from the Unleashed unlshd-093 fix (PR #1111) ---
// int8_t move_x = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;
// int8_t move_y = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;
// hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
static void neu_move_call(int raw_x, int raw_y, int* intended_x, int* intended_y) {
    int8_t move_x = raw_x - INT8_MAX; // raw_x in [0, 2*INT8_MAX], verbatim range of (rand() % (2*INT8_MAX+1))
    int8_t move_y = raw_y - INT8_MAX;
    *intended_x = move_x;
    *intended_y = move_y;
    capture_move(move_x, move_y);
}

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        checks_run++;                                                          \
        if(!(cond)) {                                                          \
            checks_failed++;                                                   \
            fprintf(stderr, "CHECK FAILED (%s:%d): %s\n", __FILE__, __LINE__, msg); \
        }                                                                       \
    } while(0)

int main(void) {
    // --- Test group 1: ALT over its full input domain (raw_x/raw_y in [0,2000],
    //     i.e. every value (rand() % 2001) can produce) mismatches "intended"
    //     movement whenever it falls outside int8_t range -- quantify exactly
    //     how much of the domain is affected. ---
    {
        long mismatches = 0;
        long total = 0;
        for(int raw_x = 0; raw_x <= 2000; raw_x++) {
            int intended_x, intended_y;
            alt_move_call(raw_x, raw_x, &intended_x, &intended_y); // dx==dy for this scan, fine
            total++;
            if((int)g_last_dx != intended_x) mismatches++;
        }
        // Safe sub-range is exactly where intended fits the full int8_t range
        // [-128,127] (256 values) -> raw_x in [1000-128, 1000+127] = [872,1127].
        long expected_safe = 256;
        long expected_mismatches = total - expected_safe;
        CHECK(
            mismatches == expected_mismatches,
            "ALT mismatch count does not match the analytically expected 2001-256=1745");
        CHECK(
            mismatches * 100 / total > 85,
            "ALT should silently corrupt movement in the large majority (~87.3%) of draws, "
            "not just at rare extremes");
        printf(
            "[group1] ALT: %ld/%ld draws (%.1f%%) deliver a movement value with no numeric "
            "relationship to the intended -1000..1000 delta\n",
            mismatches,
            total,
            100.0 * (double)mismatches / (double)total);
    }

    // --- Test group 2: concrete before/after examples an implementer would recognize,
    //     confirming the aliasing is silent (no crash, no diagnostic) and can flip sign
    //     or land far from the intended magnitude/direction. ---
    {
        int intended_x, intended_y;
        alt_move_call(2000, 2000, &intended_x, &intended_y); // intended +1000
        CHECK(intended_x == 1000, "sanity: raw 2000 should mean intended +1000");
        CHECK((int)g_last_dx != intended_x, "ALT must not deliver the intended +1000 as-is (impossible in int8_t)");
        int example_1000 = g_last_dx;

        alt_move_call(0, 0, &intended_x, &intended_y); // intended -1000
        CHECK(intended_x == -1000, "sanity: raw 0 should mean intended -1000");
        int example_neg1000 = g_last_dx;

        alt_move_call(1900, 1900, &intended_x, &intended_y); // intended +900, "large but plausible" jiggle
        CHECK(intended_x == 900, "sanity: raw 1900 should mean intended +900");
        int example_900 = g_last_dx;

        printf(
            "[group2] ALT examples: intended +1000 -> actual %d, intended -1000 -> actual %d, "
            "intended +900 -> actual %d (all silently aliased, no crash/diagnostic)\n",
            example_1000,
            example_neg1000,
            example_900);
    }

    // --- Test group 3: NEU over its full input domain (raw_x/raw_y in
    //     [0, 2*INT8_MAX], i.e. every value (rand() % (2*INT8_MAX+1)) can
    //     produce) must match "intended" for every single draw -- the fix
    //     removes the defect entirely, it does not merely narrow its window. ---
    {
        long mismatches = 0;
        long total = 0;
        for(int raw_x = 0; raw_x <= 2 * INT8_MAX; raw_x++) {
            int intended_x, intended_y;
            neu_move_call(raw_x, raw_x, &intended_x, &intended_y);
            total++;
            if((int)g_last_dx != intended_x) mismatches++;
            CHECK(intended_x >= -INT8_MAX && intended_x <= INT8_MAX, "NEU intended value must stay in bounds");
        }
        CHECK(mismatches == 0, "NEU must have zero mismatches across its entire domain");
        CHECK(total == 2 * INT8_MAX + 1, "NEU domain size must match (rand() % (2*INT8_MAX+1)) exactly");
        printf(
            "[group3] NEU: %ld/%ld draws (0%%) mismatch -- every possible draw is delivered exactly "
            "as intended\n",
            mismatches,
            total);
    }

    // --- Test group 4: NEU's extremes stay at the documented HID-safe bound
    //     (+-INT8_MAX == +-127, not +-128) -- confirms the fix does not just
    //     "usually" avoid INT8_MIN's asymmetry, it never reaches it. ---
    {
        int intended_x, intended_y;
        neu_move_call(2 * INT8_MAX, 2 * INT8_MAX, &intended_x, &intended_y);
        CHECK(intended_x == INT8_MAX, "NEU max draw must equal +INT8_MAX (127)");
        CHECK((int)g_last_dx == INT8_MAX, "NEU max draw delivered value must equal +INT8_MAX (127)");

        neu_move_call(0, 0, &intended_x, &intended_y);
        CHECK(intended_x == -INT8_MAX, "NEU min draw must equal -INT8_MAX (-127, not INT8_MIN -128)");
        CHECK((int)g_last_dx == -INT8_MAX, "NEU min draw delivered value must equal -INT8_MAX (-127)");
        printf("[group4] NEU bounds: min=%d max=%d (both within int8_t, symmetric around 0)\n",
               -INT8_MAX,
               INT8_MAX);
    }

    if(checks_failed == 0) {
        printf("ALL CHECKS PASSED (%d checks)\n", checks_run);
        return 0;
    } else {
        printf("%d/%d CHECKS FAILED\n", checks_failed, checks_run);
        return 1;
    }
}
