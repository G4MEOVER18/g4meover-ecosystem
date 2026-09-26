// Host repro: HID Mouse Jiggler (Stealth) movement can be (0, dy) or (dx, 0) -- a jiggle
// timer tick that reaches the host as a no-op HID report on one or both axes.
//
// Mirrors the exact movement-generation logic from
// hid_mouse_jiggler_stealth_timer_callback() in
// applications/system/hid_app/views/hid_mouse_jiggler_stealth.c, ALT (my base with the
// already-proposed, not-yet-applied hid-jiggler-stealth-move-bounds-fix applied --
// int8_t move_x/move_y = (rand() % (2*INT8_MAX+1)) - INT8_MAX, i.e. -127..127) vs. NEU
// (this fix's hid_mouse_jiggler_stealth_random_move() helper, verbatim from Unleashed
// commit 8fdb70dd3, PR #1112, "Stealth movement is never zero on an axis" -- the one
// piece of that commit ported here; its timer/lock restructuring and BLE-wait addition
// are separate, already-filed or still-open items).
//
// The View/Hid/Furi machinery (with_view_model, Hid*, FuriTimer, canvas drawing) carries
// no part of the defect and is not reproduced, matching the established lighter-weight
// repro style already used in this repo (see
// firmware-mining/repro-hid-jiggler-stealth-move-bounds-fix/repro.c).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// --- ALT: verbatim expression from my base after hid-jiggler-stealth-move-bounds-fix ---
// int8_t move_x = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;
static int8_t alt_move(int raw) { // raw in [0, 2*INT8_MAX], verbatim range of rand() % (2*INT8_MAX+1)
    return (int8_t)(raw - INT8_MAX);
}

// --- NEU: verbatim from Unleashed 8fdb70dd3, hid_mouse_jiggler_stealth_random_move() ---
// const int8_t delta = (int8_t)(rand() % (2 * INT8_MAX) - INT8_MAX);
// return delta >= 0 ? delta + 1 : delta;
static int8_t neu_move(int raw) { // raw in [0, 2*INT8_MAX - 1], verbatim range of rand() % (2*INT8_MAX)
    const int8_t delta = (int8_t)(raw - INT8_MAX);
    return delta >= 0 ? (int8_t)(delta + 1) : delta;
}

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                                             \
    do {                                                                             \
        checks_run++;                                                                \
        if(!(cond)) {                                                                \
            checks_failed++;                                                         \
            fprintf(stderr, "CHECK FAILED (%s:%d): %s\n", __FILE__, __LINE__, msg);  \
        }                                                                             \
    } while(0)

int main(void) {
    // --- Test group 1: ALT's full single-axis domain contains exactly one raw draw
    //     that yields 0 -- confirms a single axis really can go silent, not just in
    //     theory. ---
    {
        int zero_draws = 0;
        int total = 0;
        for(int raw = 0; raw <= 2 * INT8_MAX; raw++) {
            total++;
            if(alt_move(raw) == 0) zero_draws++;
        }
        CHECK(total == 255, "ALT single-axis domain must have exactly 255 possible draws");
        CHECK(zero_draws == 1, "ALT must be able to draw exactly 0 on a single axis (raw==INT8_MAX)");
        printf(
            "[group1] ALT: %d/%d single-axis draws (%.2f%%) are exactly 0\n",
            zero_draws,
            total,
            100.0 * (double)zero_draws / (double)total);
    }

    // --- Test group 2: ALT's full two-axis domain (move_x, move_y independent) contains
    //     the (0,0) "jiggle does nothing at all" case, and separately the "one axis
    //     silent" case on X or Y alone -- both reach the host as a HID report the OS
    //     idle/lock timer may not treat as activity. ---
    {
        long total = 0;
        long both_zero = 0;
        long one_axis_zero = 0;
        for(int rx = 0; rx <= 2 * INT8_MAX; rx++) {
            for(int ry = 0; ry <= 2 * INT8_MAX; ry++) {
                total++;
                int8_t mx = alt_move(rx);
                int8_t my = alt_move(ry);
                if(mx == 0 && my == 0)
                    both_zero++;
                else if(mx == 0 || my == 0)
                    one_axis_zero++;
            }
        }
        CHECK(total == 255L * 255L, "ALT two-axis domain must be 255*255");
        CHECK(both_zero == 1, "ALT must be able to draw (0,0) exactly once across the full domain");
        CHECK(
            one_axis_zero == 2 * 254,
            "ALT must be able to draw exactly one axis silent (the other nonzero) 2*254 times");
        printf(
            "[group2] ALT: (0,0) reachable (%ld/%ld draws), one-axis-silent reachable "
            "(%ld/%ld draws)\n",
            both_zero,
            total,
            one_axis_zero,
            total);
    }

    // --- Test group 3: NEU's full single-axis domain must never produce 0, and must
    //     stay within the documented +-INT8_MAX bound (no new out-of-range value
    //     introduced by the "+1" nudge). ---
    {
        int zero_draws = 0;
        int total = 0;
        int8_t min_seen = INT8_MAX, max_seen = -INT8_MAX;
        for(int raw = 0; raw <= 2 * INT8_MAX - 1; raw++) {
            total++;
            int8_t m = neu_move(raw);
            int m_wide = m; // widen before comparing: int8_t's own range makes the check tautological
            if(m == 0) zero_draws++;
            if(m < min_seen) min_seen = m;
            if(m > max_seen) max_seen = m;
            CHECK(m_wide >= -INT8_MAX && m_wide <= INT8_MAX, "NEU draw must stay within +-INT8_MAX");
        }
        CHECK(total == 2 * INT8_MAX, "NEU single-axis domain must have exactly 2*INT8_MAX possible draws");
        CHECK(zero_draws == 0, "NEU must never draw exactly 0 on a single axis");
        CHECK(min_seen == -INT8_MAX, "NEU minimum must still reach -INT8_MAX");
        CHECK(max_seen == INT8_MAX, "NEU maximum must still reach +INT8_MAX");
        printf(
            "[group3] NEU: %d/%d single-axis draws are 0, range [%d, %d]\n",
            zero_draws,
            total,
            min_seen,
            max_seen);
    }

    // --- Test group 4: NEU is a bijection on its input domain (no value produced twice,
    //     none skipped besides 0) -- confirms the "+1 nudge" does not bunch values up or
    //     introduce a second collision while removing the 0 case. ---
    {
        int seen[2 * 256 + 1] = {0}; // index by value + 256, generously sized
        for(int raw = 0; raw <= 2 * INT8_MAX - 1; raw++) {
            int8_t m = neu_move(raw);
            int idx = (int)m + 256;
            CHECK(seen[idx] == 0, "NEU must not produce the same output value twice (bijection)");
            seen[idx] = 1;
        }
        int distinct_nonzero_in_range = 0;
        for(int v = -INT8_MAX; v <= INT8_MAX; v++) {
            if(v == 0) continue;
            if(seen[v + 256]) distinct_nonzero_in_range++;
        }
        CHECK(
            distinct_nonzero_in_range == 2 * INT8_MAX,
            "NEU must cover every nonzero value in [-INT8_MAX, INT8_MAX] exactly once");
        printf(
            "[group4] NEU covers all %d nonzero values in [-127,127] exactly once (bijection "
            "confirmed)\n",
            distinct_nonzero_in_range);
    }

    // --- Test group 5: NEU's full two-axis domain must never produce (0,0) or a single
    //     silent axis -- the defect is removed entirely, not just made rarer. ---
    {
        long total = 0;
        long both_zero = 0;
        long one_axis_zero = 0;
        for(int rx = 0; rx <= 2 * INT8_MAX - 1; rx++) {
            for(int ry = 0; ry <= 2 * INT8_MAX - 1; ry++) {
                total++;
                int8_t mx = neu_move(rx);
                int8_t my = neu_move(ry);
                if(mx == 0 && my == 0)
                    both_zero++;
                else if(mx == 0 || my == 0)
                    one_axis_zero++;
            }
        }
        CHECK(both_zero == 0, "NEU must never draw (0,0) across its entire domain");
        CHECK(one_axis_zero == 0, "NEU must never draw a single silent axis across its entire domain");
        printf(
            "[group5] NEU: 0/%ld draws have any silent axis (both-zero and one-axis-zero both "
            "eliminated)\n",
            total);
    }

    if(checks_failed == 0) {
        printf("ALL CHECKS PASSED (%d checks)\n", checks_run);
        return 0;
    } else {
        printf("%d/%d CHECKS FAILED\n", checks_failed, checks_run);
        return 1;
    }
}
