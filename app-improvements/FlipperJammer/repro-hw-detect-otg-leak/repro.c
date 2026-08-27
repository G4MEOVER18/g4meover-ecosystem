// Host-Modell des OTG-Ownership-Bugs zwischen jammer_hw_detect() (jammer_app.c)
// und den vier OTG-verwaltenden Stellen (nrf24_jam.c, ble_jam.c, subghz_jam.c,
// scene_nrf24_diag.c). Bildet nur die fuer den Bug relevanten Zustandsuebergaenge
// nach: eine globale otg_enabled-Flagge (steht fuer
// furi_hal_power_is_otg_enabled()/enable_otg()/disable_otg()) plus die "nur
// abschalten, was ich selbst angeschaltet habe"-Eigentuemer-Logik, die
// nrf24_jam_power_on()/off() im echten Code genauso verwendet.
//
// Nachgestelltes Szenario: ein NRF24-Multiboard, das den 5V-OTG-Boost braucht
// (im echten Code selbst als unterstuetzter Fall benannt: "E01C-ML01SP4 und
// PA+LNA Module brauchen mehr als die 3.3V vom GPIO-Pin"). jammer_hw_detect()
// erkennt so ein Board nur im zweiten Versuch, MIT eingeschaltetem OTG, und
// laesst OTG danach absichtlich an (Kommentar im echten Code: "Versuch 2: mit
// OTG-Strom"). Kein Aufrufer merkt sich aber, dass hw_detect selbst
// eingeschaltet hat.
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool g_otg = false;

/* --- nrf24_jam.c Modell (unveraendert gegenueber dem echten Code) --- */
static bool s_otg_by_jam = false;

static void nrf24_jam_power_on(void) {
    if(!g_otg) {
        g_otg          = true;
        s_otg_by_jam   = true;
    }
}

static void nrf24_jam_power_off(void) {
    if(s_otg_by_jam && g_otg) {
        g_otg        = false;
        s_otg_by_jam = false;
    }
}

/* --- jammer_hw_detect() Modell, NRF24-"Versuch 2"-Zweig ---
   ALT: kein App-Flag haelt fest, dass hw_detect selbst eingeschaltet hat. */
static void hw_detect_old(bool nrf24_needs_boost) {
    bool detected = !nrf24_needs_boost; /* Versuch 1 ohne OTG (OTG ist zu Beginn aus) */
    if(!detected) {
        bool otg2_was_on = g_otg; /* false */
        if(!otg2_was_on) g_otg = true; /* Versuch 2: OTG einschalten */
        detected = true; /* jetzt erkannt -- der Boost war das fehlende Stueck */
        if(!detected && !otg2_was_on) g_otg = false; /* wird NICHT erreicht (detected==true) */
    }
    /* g_otg bleibt an -- aber nichts merkt sich, dass hw_detect es eingeschaltet hat */
}

/* NEU: zusaetzliches App-Flag, ausgewertet in jammer_app_free(). */
static bool app_hw_detect_left_otg_on = false;

static void hw_detect_new(bool nrf24_needs_boost) {
    bool otg_was_off_at_entry = !g_otg;
    bool detected = !nrf24_needs_boost;
    if(!detected) {
        bool otg2_was_on = g_otg;
        if(!otg2_was_on) g_otg = true;
        detected = true;
    }
    app_hw_detect_left_otg_on = otg_was_off_at_entry && g_otg;
}

static void jammer_app_free_old(void) {
    /* ALT: keine OTG-Aufraeumlogik beim Beenden der App. */
}

static void jammer_app_free_new(void) {
    if(app_hw_detect_left_otg_on && g_otg) {
        g_otg = false;
    }
}

int main(int argc, char** argv) {
    int use_fix = (argc > 1 && strcmp(argv[1], "fix") == 0);

    g_otg                       = false;
    s_otg_by_jam                = false;
    app_hw_detect_left_otg_on   = false;

    /* Schritt 1: App startet, jammer_hw_detect() laeuft. */
    if(use_fix) hw_detect_new(true); else hw_detect_old(true);
    printf("Nach hw_detect:      OTG=%s\n", g_otg ? "AN" : "AUS");
    fflush(stdout);

    /* Schritt 2: Nutzer startet NRF24-Jamming -- findet OTG schon an vor,
       beansprucht es deshalb NICHT als eigenes (s_otg_by_jam bleibt false). */
    nrf24_jam_power_on();
    printf("Nach nrf24_jam_start: OTG=%s (s_otg_by_jam=%s)\n",
           g_otg ? "AN" : "AUS", s_otg_by_jam ? "true" : "false");
    fflush(stdout);

    /* Schritt 3: Nutzer stoppt das Jamming wieder. */
    nrf24_jam_power_off();
    printf("Nach nrf24_jam_stop:  OTG=%s\n", g_otg ? "AN" : "AUS");
    fflush(stdout);

    /* Schritt 4: App wird beendet. */
    if(use_fix) jammer_app_free_new(); else jammer_app_free_old();
    printf("Nach App-Ende:        OTG=%s\n", g_otg ? "AN" : "AUS");
    fflush(stdout);

    assert(!g_otg && "OTG-5V-Boost bleibt nach Beenden der App aktiv (Batterie-Drain)");
    printf("OK: OTG korrekt aus.\n");
    return 0;
}
