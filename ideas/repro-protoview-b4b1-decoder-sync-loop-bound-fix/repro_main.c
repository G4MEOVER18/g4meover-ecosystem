/* Host repro for the ProtoView B4B1 (PT2262/SC5262) decoder sync-search
 * loop-bound finding (protocols/b4b1.c, antirez/protoview, commit
 * fd2f6edd43cb813c37a7d912c7dd88f9732d298f).
 *
 * sync_patterns[] holds 6 entries (30/31/32 te gap, each followed by a
 * trailing data bit of 1 or 0 -- added by upstream commit cc32c5e "b4b1:
 * more robust message detection"), but the search loop was left at
 * "for (j = 0; j < 3; j++)": only indices 0,1,2 (30-gap+one, 30-gap+zero,
 * 31-gap+one) are ever tried. Indices 3,4,5 (31-gap+zero, 32-gap+one,
 * 32-gap+zero) are unreachable dead code.
 *
 * This repro builds, for each of the 6 declared patterns, a bitmap that
 * contains exactly that pattern and nothing else, and runs both the
 * unmodified (ALT) and fixed (NEU) sync-search extraction against it. */
#include <stdio.h>
#include <string.h>
#include "mocks.h"
#include "b4b1_seek_sync.h"

static const char *sync_patterns[6] = {
    "100000000000000000000000000000011101", /* idx0: 30 times gap + one. */
    "100000000000000000000000000000010001", /* idx1: 30 times gap + zero. */
    "1000000000000000000000000000000011101", /* idx2: 31 times gap + one. */
    "1000000000000000000000000000000010001", /* idx3: 31 times gap + zero. */
    "10000000000000000000000000000000011101", /* idx4: 32 times gap + one. */
    "10000000000000000000000000000000010001", /* idx5: 32 times gap + zero. */
};

static int total = 0, passed = 0;

static void check(const char *label, bool got, bool expected) {
    total++;
    if (got == expected) {
        passed++;
    } else {
        printf("FAIL: %s (got %d, expected %d)\n", label, got, expected);
    }
}

int main(void) {
    const uint32_t PREFIX = 2; /* small offset, proves seek actually searches
                                   forward instead of only checking pos 0 */
    uint8_t buf[8];
    char label[128];

    /* For each of the 6 declared patterns: ALT only finds idx 0,1,2 (the
     * bug being reproduced), NEU finds all 6 (the fix). */
    for (int idx = 0; idx < 6; idx++) {
        memset(buf, 0, sizeof(buf));
        bitmap_set_pattern(buf, sizeof(buf), PREFIX, sync_patterns[idx]);

        uint32_t off_alt = 0, off_neu = 0;
        int j_alt = -1, j_neu = -1;
        bool found_alt = b4b1_seek_sync_ALT(buf, sizeof(buf), 64, &off_alt, &j_alt);
        bool found_neu = b4b1_seek_sync_NEU(buf, sizeof(buf), 64, &off_neu, &j_neu);

        bool alt_should_find = (idx < 3); /* bug: only the first 3 are reachable */

        snprintf(label, sizeof(label), "ALT finds idx%d", idx);
        check(label, found_alt, alt_should_find);

        snprintf(label, sizeof(label), "NEU finds idx%d", idx);
        check(label, found_neu, true);

        if (found_neu) {
            snprintf(label, sizeof(label), "NEU idx%d: off correct", idx);
            check(label, off_neu == PREFIX, true);
            snprintf(label, sizeof(label), "NEU idx%d: matched pattern index correct", idx);
            check(label, j_neu == idx, true);
        }
        if (found_alt) {
            snprintf(label, sizeof(label), "ALT idx%d: off correct", idx);
            check(label, off_alt == PREFIX, true);
            snprintf(label, sizeof(label), "ALT idx%d: matched pattern index correct", idx);
            check(label, j_alt == idx, true);
        }
    }

    /* Baseline: an all-zero buffer (no sync pattern present at all) must be
     * rejected by both ALT and NEU -- the fix must not turn into a
     * false-positive machine. */
    {
        memset(buf, 0, sizeof(buf));
        uint32_t off = 0;
        int j = -1;
        check("ALT rejects all-zero buffer", b4b1_seek_sync_ALT(buf, sizeof(buf), 64, &off, &j), false);
        check("NEU rejects all-zero buffer", b4b1_seek_sync_NEU(buf, sizeof(buf), 64, &off, &j), false);
    }

    /* numbits < 30 guard must still reject everything for both variants. */
    {
        memset(buf, 0, sizeof(buf));
        bitmap_set_pattern(buf, sizeof(buf), 0, sync_patterns[0]);
        uint32_t off = 0;
        int j = -1;
        check("ALT rejects numbits<30", b4b1_seek_sync_ALT(buf, sizeof(buf), 29, &off, &j), false);
        check("NEU rejects numbits<30", b4b1_seek_sync_NEU(buf, sizeof(buf), 29, &off, &j), false);
    }

    printf("%d/%d Pruefungen bestanden\n", passed, total);
    if (passed != total) {
        printf("ALL CHECKS FAILED\n");
        return 1;
    }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
