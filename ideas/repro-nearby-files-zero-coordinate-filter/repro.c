// Host repro: nearby_files_process_gps_coordinates() zero-coordinate filter
// (Stichoza/flipper-nearby-files "nearby_files", bundled in my own mntm-012
// firmware tree as applications/external/nearby_files/nearby_files.c,
// pinned Momentum-Apps commit 85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4,
// fap_version 1.5 -- identical condition also present in the upstream repo's
// current HEAD, fap_version 1.7).
//
// ALT = exact filter condition as shipped (nearby_files.c line 487):
//   if ((float)latitude != 0.0f && (float)longitude != 0.0f) -> keep as valid
// NEU = fix applied in ../nearby-files-zero-coordinate-filter-fix/nearby_files_c.patch:
//   if ((float)latitude != 0.0f || (float)longitude != 0.0f) -> keep as valid
//
// Everything below is test scaffolding, not app code.
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

static bool alt_is_valid(double latitude, double longitude) {
    return (float)latitude != 0.0f && (float)longitude != 0.0f;
}

static bool neu_is_valid(double latitude, double longitude) {
    return (float)latitude != 0.0f || (float)longitude != 0.0f;
}

typedef struct {
    const char* name;
    double lat;
    double lon;
    bool expect_alt_valid;
    bool expect_neu_valid;
} Case;

int main(void) {
    int checks = 0;

    Case cases[] = {
        // 1. Regression: ordinary nonzero coordinates, both filters must agree (valid).
        {"ordinary coordinates (Berlin)", 52.520008, 13.404954, true, true},
        {"ordinary coordinates (negative lon, negative lat)", -33.868820, -70.233893, true,
         true},

        // 2. True sentinel (0,0): both filters must agree (invalid) -- this is the
        //    case the AND/OR distinction was presumably meant to catch.
        {"true (0,0) sentinel", 0.0, 0.0, false, false},

        // 3. THE FUND: real capture sitting on the Greenwich meridian (lon == 0.0
        //    exactly) with a perfectly ordinary, nonzero latitude. ALT wrongly
        //    discards this file from the nearby list; NEU keeps it.
        {"real fix on Greenwich meridian (London)", 51.5074, 0.0, false /*bug*/, true},

        // 4. THE FUND, other axis: real capture sitting exactly on the equator
        //    (lat == 0.0 exactly) with a perfectly ordinary, nonzero longitude.
        {"real fix on the equator (Quito-ish)", 0.0, -78.5, false /*bug*/, true},

        // 5. Both axes individually nonzero but one coordinate numerically tiny
        //    (not the bug -- sanity check that near-zero-but-not-zero values are
        //    unaffected by either filter).
        {"near-zero but not exactly zero", 0.000001, 0.000001, true, true},
    };
    size_t n_cases = sizeof(cases) / sizeof(cases[0]);

    printf("=== Per-case ALT vs NEU ===\n");
    for(size_t i = 0; i < n_cases; i++) {
        Case* c = &cases[i];
        bool alt = alt_is_valid(c->lat, c->lon);
        bool neu = neu_is_valid(c->lat, c->lon);
        printf(
            "%-42s lat=%12.6f lon=%12.6f  ALT=%-5s NEU=%-5s\n",
            c->name,
            c->lat,
            c->lon,
            alt ? "keep" : "DROP",
            neu ? "keep" : "DROP");
        assert(alt == c->expect_alt_valid);
        checks++;
        assert(neu == c->expect_neu_valid);
        checks++;
    }

    // 6. Explicitly demonstrate the discrepancy exists exactly where claimed: ALT
    //    and NEU must disagree on cases 4 and 5 (index 3 and 4), and must agree
    //    (both true or both false) on every other case.
    for(size_t i = 0; i < n_cases; i++) {
        Case* c = &cases[i];
        bool alt = alt_is_valid(c->lat, c->lon);
        bool neu = neu_is_valid(c->lat, c->lon);
        bool is_single_axis_zero_case = (i == 3 || i == 4);
        if(is_single_axis_zero_case) {
            assert(alt != neu && alt == false && neu == true);
        } else {
            assert(alt == neu);
        }
        checks++;
    }

    // 7. Equivalence sweep: for a grid of coordinates where NEITHER axis is
    //    exactly zero, ALT and NEU must always agree (both filters behave
    //    identically once we are away from either zero-axis).
    for(int lat10 = -900; lat10 <= 900; lat10 += 37) { // skip-stride, avoids lat10==0
        for(int lon10 = -1800; lon10 <= 1800; lon10 += 53) { // skip-stride, avoids lon10==0
            if(lat10 == 0 || lon10 == 0) continue;
            double lat = lat10 / 10.0;
            double lon = lon10 / 10.0;
            bool alt = alt_is_valid(lat, lon);
            bool neu = neu_is_valid(lat, lon);
            assert(alt == neu);
            assert(alt == true);
            checks++;
        }
    }

    // 8. Gegenprobe: confirm the test itself can fail -- a deliberately
    //    re-broken "NEU" (AND instead of OR) must reproduce the same bug as ALT
    //    on the two single-axis-zero cases, proving cases 3/4 above actually
    //    exercise the AND-vs-OR difference and are not vacuously true.
    {
        bool broken_neu_london = alt_is_valid(51.5074, 0.0); // same formula as ALT
        bool broken_neu_equator = alt_is_valid(0.0, -78.5);
        assert(broken_neu_london == false);
        assert(broken_neu_equator == false);
        checks += 2;
    }

    printf("\n%d checks passed.\n", checks);
    return 0;
}
