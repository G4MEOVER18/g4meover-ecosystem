/*
 * Host repro for the NFC Canary "Sound from" default-tier bug.
 *
 * Reduces nfc_canary's tier_enabled() gate (alert.c) plus its shipped
 * ModeSentinel detection ceiling (nfc_canary.c worker_thread only ever
 * reaches TierWarn -- ModeDecoy/TierAlarm is source-present but explicitly
 * not wired in, per the app's own settings_menu.c comment) down to the
 * values that matter, and walks every tier a real (non-test) detection can
 * reach with OLD vs NEW default settings.
 *
 * Build:  gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 * Run:    ./repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    TierNone = 0,
    TierInfo,
    TierWarn,
    TierAlarm, /* only reachable via alert_test() audition in the shipped build */
} ThreatTier;

/* Verbatim from alert.c */
static bool tier_enabled(uint8_t min_tier, ThreatTier tier) {
    return tier != TierNone && (uint8_t)tier >= min_tier;
}

static const char* tier_name(ThreatTier t) {
    switch(t) {
    case TierNone: return "None";
    case TierInfo: return "Info";
    case TierWarn: return "Warn";
    case TierAlarm: return "Alarm";
    }
    return "?";
}

int main(void) {
    /* Tiers a REAL detection can reach in the shipped 0.2 build (ModeSentinel
     * only; worker_thread never sets TierAlarm). TierAlarm is included
     * separately below to show what alert_test() (the "OK" audition in the
     * settings menu) always uses, regardless of min_tier_sound. */
    ThreatTier real_tiers[] = {TierInfo, TierWarn};
    const int n_real = 2;

    uint8_t old_min_tier_sound = TierAlarm; /* settings.c default, as shipped */
    uint8_t new_min_tier_sound = TierWarn;  /* proposed fix */

    printf("=== OLD default (min_tier_sound = Alarm) ===\n");
    int old_real_hits = 0;
    for(int i = 0; i < n_real; i++) {
        bool fires = tier_enabled(old_min_tier_sound, real_tiers[i]);
        printf("  real detection reaches %-5s -> sound fires: %s\n",
               tier_name(real_tiers[i]), fires ? "yes" : "no");
        if(fires) old_real_hits++;
    }
    bool old_test_fires = tier_enabled(old_min_tier_sound, TierAlarm);
    printf("  settings-menu audition (forces Alarm) -> sound fires: %s\n",
           old_test_fires ? "yes" : "no");
    printf("  real-detection sound hits: %d/%d\n\n", old_real_hits, n_real);

    printf("=== NEW default (min_tier_sound = Warn) ===\n");
    int new_real_hits = 0;
    for(int i = 0; i < n_real; i++) {
        bool fires = tier_enabled(new_min_tier_sound, real_tiers[i]);
        printf("  real detection reaches %-5s -> sound fires: %s\n",
               tier_name(real_tiers[i]), fires ? "yes" : "no");
        if(fires) new_real_hits++;
    }
    bool new_test_fires = tier_enabled(new_min_tier_sound, TierAlarm);
    printf("  settings-menu audition (forces Alarm) -> sound fires: %s\n",
           new_test_fires ? "yes" : "no");
    printf("  real-detection sound hits: %d/%d\n\n", new_real_hits, n_real);

    /* The bug: with the shipped default, the audition looks identical
     * (fires) whether or not any real detection could ever reach it, because
     * alert_test() bypasses the gate by injecting TierAlarm directly. A user
     * pressing OK in Settings hears the pattern working and has no way to
     * tell that live detections can never reach it. */
    if(old_real_hits == 0 && old_test_fires) {
        printf("CONFIRMED: shipped default makes Sound unreachable by any real "
               "detection (0/%d), while the audition still reports success --\n"
               "the audition cannot distinguish a live-reachable threshold from\n"
               "a dead one.\n",
               n_real);
    }
    if(new_real_hits > 0 && new_real_hits == old_real_hits + 1) {
        printf("CONFIRMED: fix restores Sound to the same trigger point as "
               "Vibrate (Warn), matching the only tier ceiling the shipped\n"
               "ModeSentinel-only build can actually reach.\n");
    }

    return (old_real_hits == 0 && new_real_hits > 0) ? 0 : 1;
}
