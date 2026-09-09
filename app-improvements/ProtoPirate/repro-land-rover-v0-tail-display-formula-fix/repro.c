// Host repro: Land Rover V0 decoder get_string() "Tail:" display formula.
//
// Isolates the display expression from subghz_protocol_decoder_land_rover_v0_get_string()
// (protocols/land_rover_v0.c) without pulling in FuriString/SubGhz headers. Compares the
// current (ALT) formula, which reconstructs a value purely from bit 15 of `tail` and
// silently assumes extra_bit == true, against the fixed (NEU) formula, which prints the
// actual captured/loaded 17-bit tail region (16-bit tail field + trailing extra bit).
//
// Build/run:
//   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// ALT: subghz_protocol_decoder_land_rover_v0_get_string(), current "Tail:" argument.
static unsigned long alt_tail_display(uint16_t tail, bool extra_bit) {
    (void)extra_bit; // ALT never looks at extra_bit at all
    return ((tail >> 15) & 1U) ? 0x1FFFFUL : 0x0FFFFUL;
}

// NEU: directly reflects the real captured/loaded state (tail bits + trailing extra bit),
// same value the RF frame / saved file actually carries.
static unsigned long neu_tail_display(uint16_t tail, bool extra_bit) {
    return ((unsigned long)tail << 1) | (extra_bit ? 1UL : 0UL);
}

typedef struct {
    const char* name;
    uint16_t tail;
    bool extra_bit;
    unsigned long expect_neu;
    bool expect_alt_matches_neu;
} TestCase;

int main(void) {
    const TestCase cases[] = {
        // Canonical live-decode outcomes (tail_ok can only be true for these two,
        // always with extra_bit == true) -- ALT must keep matching NEU here.
        {"canonical tail=0xFFFF, extra_bit=1 (Unlock/valid)", 0xFFFF, true, 0x1FFFFUL, true},
        {"canonical tail=0x7FFF, extra_bit=1 (Lock/valid)", 0x7FFF, true, 0x0FFFFUL, true},

        // Loaded from a file whose Tail field is canonical but ExtraBit is 0 (tail_ok
        // shows BAD) -- ALT still claims extra_bit=1 and hides the discrepancy.
        {"canonical tail=0xFFFF, extra_bit=0 (tail_ok BAD)", 0xFFFF, false, 0x1FFFEUL, false},
        {"canonical tail=0x7FFF, extra_bit=0 (tail_ok BAD)", 0x7FFF, false, 0x0FFFEUL, false},

        // Loaded from a file with an off-canonical Tail field (hand-edited / corrupted /
        // produced by another tool) -- ALT collapses any such value to one of only two
        // outputs based solely on bit 15, NEU reflects what was actually loaded.
        {"off-canonical tail=0x1234, extra_bit=1", 0x1234, true, 0x2469UL, false},
        {"off-canonical tail=0x8000, extra_bit=1", 0x8000, true, 0x10001UL, false},
        {"off-canonical tail=0x0000, extra_bit=0", 0x0000, false, 0x0000UL, false},
    };
    const size_t n = sizeof(cases) / sizeof(cases[0]);

    int failures = 0;
    for(size_t i = 0; i < n; i++) {
        const TestCase* tc = &cases[i];
        unsigned long alt = alt_tail_display(tc->tail, tc->extra_bit);
        unsigned long neu = neu_tail_display(tc->tail, tc->extra_bit);
        bool alt_matches_neu = (alt == neu);

        printf(
            "[%s] tail=0x%04X extra_bit=%d -> ALT=0x%05lX NEU=0x%05lX (alt==neu: %s, expected: %s)\n",
            tc->name,
            tc->tail,
            tc->extra_bit,
            alt,
            neu,
            alt_matches_neu ? "yes" : "no",
            tc->expect_alt_matches_neu ? "yes" : "no");

        if(neu != tc->expect_neu) {
            printf("  FAIL: NEU formula mismatch, expected 0x%05lX\n", tc->expect_neu);
            failures++;
        }
        if(alt_matches_neu != tc->expect_alt_matches_neu) {
            printf("  FAIL: ALT vs NEU agreement mismatch\n");
            failures++;
        }
    }

    if(failures) {
        printf("\n%d check(s) failed.\n", failures);
        return 1;
    }

    printf("\nAll checks passed: ALT matches NEU only for the two canonical, extra_bit=1 "
           "cases; for a BAD extra_bit or an off-canonical Tail field (both possible on "
           "load, since deserialize() applies the file's Tail/ExtraBit fields regardless "
           "of validate_frame()'s result), ALT hides the real value behind one of two "
           "hardcoded outputs while NEU shows what was actually captured/loaded.\n");
    return 0;
}
