// Host-Repro: rollforge_rf.c rf_power_on()/rf_power_off() Ownership-Logik.
// Bildet ausschließlich die OTG-Ownership-Buchhaltung nach (kein CC1101/keine
// Hardware), mit furi_hal_power_is_otg_enabled/enable_otg/disable_otg als
// simple globale Mocks -- genau der Teil, den ein Flipper-Firmware-Host-Build
// nicht ohne Weiteres isoliert bauen laesst.
//
// Baubar mit: gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

// ---- Mock: BQ25896-OTG-Boost, EIN globaler Hardwarezustand (wie real) -----
static bool g_otg_hw = false;
static int  g_enable_calls = 0;

static bool mock_is_otg_enabled(void) { return g_otg_hw; }
static void mock_enable_otg(void)  { g_otg_hw = true;  g_enable_calls++; }
static void mock_disable_otg(void) { g_otg_hw = false; }

// ---- ALT: 1:1 aus rollforge_rf.c uebernommen -------------------------------
static bool s_otg_by_app_ALT = false;

static void rf_power_on_ALT(void) {
    unsigned attempts = 0;
    while(!mock_is_otg_enabled() && attempts++ < 5) {
        mock_enable_otg();
    }
    if(mock_is_otg_enabled()) s_otg_by_app_ALT = true;
}
static void rf_power_off_ALT(void) {
    if(s_otg_by_app_ALT && mock_is_otg_enabled()) {
        mock_disable_otg();
        s_otg_by_app_ALT = false;
    }
}

// ---- NEU: bereits von aussen aktiviertes OTG wird nicht als eigenes -------
// ---- Eigentum beansprucht --------------------------------------------------
static bool s_otg_by_app_NEU = false;

static void rf_power_on_NEU(void) {
    if(mock_is_otg_enabled()) {
        // Schon von aussen an (Settings > Power "5V on GPIO" oder eine
        // andere App/ein anderes Modul) -- nicht meine Zustaendigkeit.
        return;
    }
    unsigned attempts = 0;
    while(!mock_is_otg_enabled() && attempts++ < 5) {
        mock_enable_otg();
    }
    if(mock_is_otg_enabled()) s_otg_by_app_NEU = true;
}
static void rf_power_off_NEU(void) {
    if(s_otg_by_app_NEU && mock_is_otg_enabled()) {
        mock_disable_otg();
        s_otg_by_app_NEU = false;
    }
}

int main(void) {
    // Szenario 1: OTG bereits von aussen an (z. B. Settings > Power
    // "5V on GPIO"), kein/kein erreichbares EXT-CC1101-Board angeschlossen.
    // rollforge_rf_init() ruft rf_power_on() so oder so unconditional auf.
    {
        g_otg_hw = true; g_enable_calls = 0;
        s_otg_by_app_ALT = false; s_otg_by_app_NEU = false;

        rf_power_on_ALT();
        bool alt_claimed_ownership = s_otg_by_app_ALT;
        // ... kein EXT gefunden -> rollforge_rf_init() ruft sofort rf_power_off() ...
        rf_power_off_ALT();
        bool alt_otg_after_immediate_off = g_otg_hw;

        g_otg_hw = true; // Ausgangszustand neu herstellen
        rf_power_on_NEU();
        bool neu_claimed_ownership = s_otg_by_app_NEU;
        rf_power_off_NEU();
        bool neu_otg_after_immediate_off = g_otg_hw;

        printf("Szenario 1 (OTG vorher extern an, kein EXT gefunden):\n");
        printf("  ALT: Ownership faelschlich beansprucht=%d, OTG danach an=%d\n",
            alt_claimed_ownership, alt_otg_after_immediate_off);
        printf("  NEU: Ownership beansprucht=%d,            OTG danach an=%d\n",
            neu_claimed_ownership, neu_otg_after_immediate_off);

        assert(alt_claimed_ownership == true);   // Bug: ALT beansprucht faelschlich
        assert(alt_otg_after_immediate_off == false); // Bug: schaltet fremde 5V-Versorgung ab
        assert(neu_claimed_ownership == false);  // Fix: keine Beanspruchung
        assert(neu_otg_after_immediate_off == true);  // Fix: fremde Versorgung bleibt an
    }

    // Szenario 2 (Regression): OTG vorher aus, RollForge findet ein echtes
    // EXT-CC1101-Board -> muss weiterhin selbst ein-/nach Nutzung ausschalten.
    {
        g_otg_hw = false; g_enable_calls = 0;
        s_otg_by_app_ALT = false; s_otg_by_app_NEU = false;

        rf_power_on_ALT();
        bool alt_on_after_on = g_otg_hw;
        rf_power_off_ALT(); // App-Ende
        bool alt_off_after_deinit = g_otg_hw;

        g_otg_hw = false;
        rf_power_on_NEU();
        bool neu_on_after_on = g_otg_hw;
        rf_power_off_NEU();
        bool neu_off_after_deinit = g_otg_hw;

        printf("Szenario 2 (OTG vorher aus, echtes EXT-Board):\n");
        printf("  ALT: an nach power_on=%d, aus nach deinit=%d\n",
            alt_on_after_on, alt_off_after_deinit);
        printf("  NEU: an nach power_on=%d, aus nach deinit=%d\n",
            neu_on_after_on, neu_off_after_deinit);

        assert(alt_on_after_on == true && alt_off_after_deinit == false);
        assert(neu_on_after_on == true && neu_off_after_deinit == false);
    }

    // Szenario 3 (Regression): OTG vorher aus, kein EXT-Board vorhanden ->
    // enable_otg() schlaegt alle 5 Versuche fehl (z. B. defekter Laderegler).
    {
        g_otg_hw = false; g_enable_calls = 0;
        s_otg_by_app_ALT = false; s_otg_by_app_NEU = false;

        // enable_otg() greift hardwareseitig nicht (z. B. defekter
        // Laderegler/Kurzschluss) -- jeder Versuch faellt sofort zurueck auf
        // aus, is_otg_enabled() bleibt dauerhaft false.
        // ALT
        {
            unsigned attempts = 0;
            while(!mock_is_otg_enabled() && attempts++ < 5) {
                g_otg_hw = true; g_otg_hw = false; // enable "greift" hardwareseitig nicht
            }
            if(mock_is_otg_enabled()) s_otg_by_app_ALT = true;
        }
        bool alt_claimed_on_failure = s_otg_by_app_ALT;
        // NEU
        {
            if(!mock_is_otg_enabled()) {
                unsigned attempts = 0;
                while(!mock_is_otg_enabled() && attempts++ < 5) {
                    g_otg_hw = true; g_otg_hw = false;
                }
                if(mock_is_otg_enabled()) s_otg_by_app_NEU = true;
            }
        }
        bool neu_claimed_on_failure = s_otg_by_app_NEU;

        printf("Szenario 3 (OTG bleibt trotz Versuchen aus):\n");
        printf("  ALT: Ownership beansprucht=%d\n", alt_claimed_on_failure);
        printf("  NEU: Ownership beansprucht=%d\n", neu_claimed_on_failure);

        assert(alt_claimed_on_failure == false);
        assert(neu_claimed_on_failure == false);
    }

    printf("\nAlle Faelle wie erwartet.\n");
    return 0;
}
