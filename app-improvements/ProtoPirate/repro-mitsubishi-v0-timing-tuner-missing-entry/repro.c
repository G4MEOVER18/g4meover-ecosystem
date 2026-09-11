// Host-Nachbau von protopirate_get_protocol_timing() (protocols/protocol_items.c) --
// reine Tabellensuche + Alias-Fallback, ohne Flipper-/SubGhz-Laufzeit.
//
// Zweck: zeigen, dass "Mitsubishi V0" (real registrierter Decoder-Name, siehe
// protocols/mitsubishi_v0.h MITSUBISHI_PROTOCOL_NAME, eingebunden ueber
// protocols/plugins/protopirate_fm_plugin.c) unter dem ALT-Tabellenstand (protocol_timings[]
// ohne Mitsubishi-Zeile) durchgehend NULL liefert, waehrend NEU (Zeile ergaenzt, Werte 1:1
// aus subghz_protocol_mitsubishi_const in protocols/mitsubishi_v0.c uebernommen) die korrekten
// Referenzwerte liefert -- und alle uebrigen 24 real registrierten Namen dabei unveraendert
// bleiben (Regressionsvergleich). "VW"/VAG ist ein separater, bereits dokumentierter Fund
// (timing-tuner-vag-name-mismatch-fix.md) und wird hier absichtlich unveraendert mitgefuehrt.
//
// Bauen/Ausfuehren:
//   gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* name;
    unsigned te_short;
    unsigned te_long;
    unsigned te_delta;
    unsigned min_count_bit;
} ProtoPirateProtocolTiming;

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

// ALT: protocol_timings[] wie im aktuell geklonten ProtoPirate-Stand (24 Eintraege, keine
// Mitsubishi-V0-Zeile). Werte 1:1 aus protocols/protocol_items.c uebernommen.
static const ProtoPirateProtocolTiming protocol_timings_alt[] = {
    {"Honda Static", 500, 1000, 150, 128},
    {"Honda V1", 400, 800, 150, 128},
    {"Kia V0", 250, 500, 100, 61},
    {"Kia V1", 800, 800, 200, 61},
    {"Kia V2", 250, 500, 100, 53},
    {"Kia V3/V4", 250, 500, 100, 61},
    {"Kia V5", 250, 500, 100, 64},
    {"Kia V6", 250, 500, 100, 64},
    {"Kia V7", 250, 500, 100, 64},
    {"Ford V0", 250, 500, 100, 64},
    {"Chrysler V0", 300, 3700, 400, 80},
    {"Ford V1", 65, 130, 39, 136},
    {"Ford V2", 200, 400, 260, 104},
    {"Ford V3", 240, 480, 60, 104},
    {"Fiat V0", 200, 400, 100, 64},
    {"Fiat V1", 260, 520, 80, 80},
    {"Mazda V0", 250, 500, 100, 64},
    {"Land Rover V0", 250, 500, 100, 81},
    {"Porsche Touareg", 1680, 3370, 500, 64},
    {"Subaru", 800, 1600, 200, 64},
    {"VW", 500, 1000, 120, 80}, // Bug (falscher Name statt "VAG"), separat dokumentiert
    {"Scher-Khan", 750, 1100, 180, 35},
    {"Star Line", 250, 500, 120, 64},
    {"PSA", 250, 500, 100, 128},
};

// NEU: dieselbe Tabelle plus die eine ergaenzte Zeile, an derselben Stelle wie im Patch
// (direkt nach "Mazda V0", passend zur Registrierungsreihenfolge im FM-Plugin). Werte 1:1
// aus subghz_protocol_mitsubishi_const in protocols/mitsubishi_v0.c (dieselben Werte, mit
// denen der Decoder echte Signale tatsaechlich demoduliert -- keine neue Recherche noetig).
static const ProtoPirateProtocolTiming protocol_timings_neu[] = {
    {"Honda Static", 500, 1000, 150, 128},
    {"Honda V1", 400, 800, 150, 128},
    {"Kia V0", 250, 500, 100, 61},
    {"Kia V1", 800, 800, 200, 61},
    {"Kia V2", 250, 500, 100, 53},
    {"Kia V3/V4", 250, 500, 100, 61},
    {"Kia V5", 250, 500, 100, 64},
    {"Kia V6", 250, 500, 100, 64},
    {"Kia V7", 250, 500, 100, 64},
    {"Ford V0", 250, 500, 100, 64},
    {"Chrysler V0", 300, 3700, 400, 80},
    {"Ford V1", 65, 130, 39, 136},
    {"Ford V2", 200, 400, 260, 104},
    {"Ford V3", 240, 480, 60, 104},
    {"Fiat V0", 200, 400, 100, 64},
    {"Fiat V1", 260, 520, 80, 80},
    {"Mazda V0", 250, 500, 100, 64},
    {"Mitsubishi V0", 250, 500, 100, 80}, // <-- ergaenzte Zeile
    {"Land Rover V0", 250, 500, 100, 81},
    {"Porsche Touareg", 1680, 3370, 500, 64},
    {"Subaru", 800, 1600, 200, 64},
    {"VW", 500, 1000, 120, 80}, // unveraendert -- separater Fund
    {"Scher-Khan", 750, 1100, 180, 35},
    {"Star Line", 250, 500, 120, 64},
    {"PSA", 250, 500, 100, 128},
};

// 1:1-Nachbau von protopirate_get_protocol_timing() (Tabellensuche + Alias-Fallback).
static const ProtoPirateProtocolTiming*
    lookup(const ProtoPirateProtocolTiming* table, size_t count, const char* protocol_name) {
    if(!protocol_name) return NULL;

    for(size_t i = 0; i < count; i++) {
        if(strcmp(protocol_name, table[i].name) == 0 ||
           strstr(protocol_name, table[i].name) != NULL) {
            return &table[i];
        }
    }

    static const struct {
        const char* alias;
        const char* canonical;
    } aliases[] = {
        {"Honda V0", "Kia V0"},
        {"Suzuki", "Kia V0"},
        {"V3", "Kia V3/V4"},
        {"V4", "Kia V3/V4"},
    };
    for(size_t a = 0; a < COUNT_OF(aliases); a++) {
        if(strstr(protocol_name, aliases[a].alias) == NULL) continue;
        for(size_t i = 0; i < count; i++) {
            if(strcmp(table[i].name, aliases[a].canonical) == 0) return &table[i];
        }
    }

    return NULL;
}

// Alle real registrierten Decoder-Namen (protocols/*.h *_PROTOCOL_NAME bzw. .name-Literal),
// aus beiden Registries (AM-Haupt-Registry protocol_items.c + FM-Plugin
// protocols/plugins/protopirate_fm_plugin.c) -- Mitsubishi V0 ist ausschliesslich im
// FM-Plugin registriert, wird von derselben protopirate_get_protocol_timing()-Suche aber
// identisch behandelt (scenes/protopirate_scene_timing_tuner.c liest nur
// decoder_base->protocol->name, unabhaengig von der Registry).
static const char* real_registered_names[] = {
    "Honda Static", "Honda V1",  "Kia V0",         "Kia V1",   "Kia V2",     "Kia V3/V4",
    "Kia V5",       "Kia V6",    "Kia V7",         "Ford V0",  "Chrysler V0", "Ford V1",
    "Ford V2",      "Ford V3",   "Fiat V0",        "Fiat V1",  "Mazda V0",   "Mitsubishi V0",
    "Land Rover V0", "Porsche Touareg", "Subaru",  "VAG",      "Scher-Khan", "Star Line",
    "PSA",
};

int main(void) {
    int fail = 0;
    size_t alt_count = COUNT_OF(protocol_timings_alt);
    size_t neu_count = COUNT_OF(protocol_timings_neu);

    printf("=== ALT (ohne Mitsubishi-V0-Zeile) ===\n");
    for(size_t i = 0; i < COUNT_OF(real_registered_names); i++) {
        const char* name = real_registered_names[i];
        const ProtoPirateProtocolTiming* t = lookup(protocol_timings_alt, alt_count, name);
        if(t) {
            printf("  %-16s -> FOUND  short=%u long=%u delta=%u bits=%u\n", name, t->te_short,
                   t->te_long, t->te_delta, t->min_count_bit);
        } else {
            printf("  %-16s -> NOT FOUND\n", name);
        }
        // Erwartung ALT: nur "Mitsubishi V0" fehlt; "VAG" bleibt ebenfalls NOT FOUND
        // (separater, hier bewusst unveraenderter Fund).
        if(strcmp(name, "Mitsubishi V0") == 0 || strcmp(name, "VAG") == 0) {
            if(t != NULL) {
                printf("    !! unerwartet gefunden (sollte unter ALT fehlen)\n");
                fail = 1;
            }
        } else if(t == NULL) {
            printf("    !! unerwartet NICHT gefunden (Regression in ALT-Tabelle)\n");
            fail = 1;
        }
    }

    printf("\n=== NEU (Mitsubishi-V0-Zeile ergaenzt) ===\n");
    for(size_t i = 0; i < COUNT_OF(real_registered_names); i++) {
        const char* name = real_registered_names[i];
        const ProtoPirateProtocolTiming* t = lookup(protocol_timings_neu, neu_count, name);
        if(t) {
            printf("  %-16s -> FOUND  short=%u long=%u delta=%u bits=%u\n", name, t->te_short,
                   t->te_long, t->te_delta, t->min_count_bit);
        } else {
            printf("  %-16s -> NOT FOUND\n", name);
        }
        if(strcmp(name, "VAG") == 0) {
            // Unveraendert weiterhin NOT FOUND -- separater, hier nicht behobener Fund.
            if(t != NULL) {
                printf("    !! unerwartet gefunden (VAG-Fund ist nicht Teil dieses Fixes)\n");
                fail = 1;
            }
            continue;
        }
        if(t == NULL) {
            printf("    !! unerwartet NICHT gefunden\n");
            fail = 1;
        } else if(strcmp(name, "Mitsubishi V0") == 0) {
            if(t->te_short != 250 || t->te_long != 500 || t->te_delta != 100 ||
               t->min_count_bit != 80) {
                printf("    !! Referenzwerte weichen von subghz_protocol_mitsubishi_const ab\n");
                fail = 1;
            }
        }
    }

    // Regressionsvergleich: alle 24 unveraenderten Zeilen liefern in ALT und NEU identische
    // Werte (nur die neue Zeile kommt hinzu, nichts Bestehendes wird verschoben/veraendert).
    printf("\n=== Regressionsvergleich ALT vs. NEU (23 unveraenderte Namen, VAG/Mitsubishi V0 "
           "ausgenommen) ===\n");
    for(size_t i = 0; i < COUNT_OF(real_registered_names); i++) {
        const char* name = real_registered_names[i];
        if(strcmp(name, "Mitsubishi V0") == 0 || strcmp(name, "VAG") == 0) continue;
        const ProtoPirateProtocolTiming* a = lookup(protocol_timings_alt, alt_count, name);
        const ProtoPirateProtocolTiming* n = lookup(protocol_timings_neu, neu_count, name);
        if(!a || !n || a->te_short != n->te_short || a->te_long != n->te_long ||
           a->te_delta != n->te_delta || a->min_count_bit != n->min_count_bit) {
            printf("  !! Abweichung bei %s\n", name);
            fail = 1;
        }
    }
    printf("  (keine Abweichung gefunden, wenn oben nichts ausgegeben wurde)\n");

    if(fail) {
        printf("\nFAIL\n");
        return 1;
    }
    printf("\nOK: ALT liefert fuer \"Mitsubishi V0\" NOT FOUND, NEU liefert die korrekten\n"
           "Referenzwerte (short=250 long=500 delta=100 bits=80); alle uebrigen 23 Namen\n"
           "(sowie der separat dokumentierte \"VAG\"/\"VW\"-Fund) bleiben in beiden Tabellen\n"
           "unveraendert.\n");
    return 0;
}
