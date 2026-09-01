// Host-Repro: Timing-Tuner-Namensvergleich (protopirate_get_protocol_timing())
// aus protocols/protocol_items.c, ALT-vs-NEU, ohne Flipper-/Hardware-Abhaengigkeiten.
//
// Bildet exakt die Such-Logik der Originalfunktion nach (erst strcmp/strstr ueber
// die Timing-Tabelle in Deklarationsreihenfolge, danach ein Alias-Fallback), einmal
// mit dem ALT-Tabelleneintrag (.name = "VW") und einmal mit dem NEU-Eintrag
// (.name = VAG_PROTOCOL_NAME, "VAG"). Die real registrierten Decoder-Namen sind
// wortgetreu aus den *_PROTOCOL_NAME-Konstanten aller protocols/*.h/*.c uebernommen.
//
// Build: gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
// Run:   ./repro

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    const char* name;
    unsigned te_short;
    unsigned te_long;
    unsigned te_delta;
    unsigned min_count_bit;
} Timing;

// Tabelle 1:1 aus protocol_items.c uebernommen, einzig der VW/VAG-Eintrag variiert
// zwischen ALT und NEU (Rest identisch, um zu zeigen, dass der Fix keinen der
// anderen 23 Eintraege veraendert).
#define VW_OR_VAG_ENTRY(n) \
    {n, 500, 1000, 120, 80}

static const Timing table_alt[] = {
    {"Honda Static", 63, 700, 120, 64},     {"Honda V1", 1000, 2000, 400, 64},
    {"Kia V0", 250, 500, 100, 61},          {"Kia V1", 800, 1600, 200, 56},
    {"Kia V2", 500, 1000, 150, 51},         {"Kia V3/V4", 400, 800, 150, 64},
    {"Kia V5", 400, 800, 150, 64},          {"Kia V6", 200, 400, 100, 144},
    {"Kia V7", 250, 500, 100, 64},          {"Ford V0", 250, 500, 100, 64},
    {"Chrysler V0", 300, 3700, 400, 80},    {"Ford V1", 65, 130, 39, 136},
    {"Ford V2", 200, 400, 260, 104},        {"Ford V3", 240, 480, 60, 104},
    {"Fiat V0", 200, 400, 100, 64},         {"Fiat V1", 260, 520, 80, 80},
    {"Mazda V0", 250, 500, 100, 64},        {"Land Rover V0", 250, 500, 100, 81},
    {"Porsche Touareg", 1680, 3370, 500, 64}, {"Subaru", 800, 1600, 200, 64},
    VW_OR_VAG_ENTRY("VW"), // <- ALT: does not match the registered decoder name "VAG"
    {"Scher-Khan", 750, 1100, 180, 35},     {"Star Line", 250, 500, 120, 64},
    {"PSA", 250, 500, 100, 128},
};

static const Timing table_neu[] = {
    {"Honda Static", 63, 700, 120, 64},     {"Honda V1", 1000, 2000, 400, 64},
    {"Kia V0", 250, 500, 100, 61},          {"Kia V1", 800, 1600, 200, 56},
    {"Kia V2", 500, 1000, 150, 51},         {"Kia V3/V4", 400, 800, 150, 64},
    {"Kia V5", 400, 800, 150, 64},          {"Kia V6", 200, 400, 100, 144},
    {"Kia V7", 250, 500, 100, 64},          {"Ford V0", 250, 500, 100, 64},
    {"Chrysler V0", 300, 3700, 400, 80},    {"Ford V1", 65, 130, 39, 136},
    {"Ford V2", 200, 400, 260, 104},        {"Ford V3", 240, 480, 60, 104},
    {"Fiat V0", 200, 400, 100, 64},         {"Fiat V1", 260, 520, 80, 80},
    {"Mazda V0", 250, 500, 100, 64},        {"Land Rover V0", 250, 500, 100, 81},
    {"Porsche Touareg", 1680, 3370, 500, 64}, {"Subaru", 800, 1600, 200, 64},
    VW_OR_VAG_ENTRY("VAG"), // <- NEU: matches the registered decoder name "VAG"
    {"Scher-Khan", 750, 1100, 180, 35},     {"Star Line", 250, 500, 120, 64},
    {"PSA", 250, 500, 100, 128},
};

typedef struct {
    const char* alias;
    const char* canonical;
} Alias;

static const Alias aliases[] = {
    {"Honda V0", "Kia V0"},
    {"Suzuki", "Kia V0"},
    {"V3", "Kia V3/V4"},
    {"V4", "Kia V3/V4"},
};

static const Timing* lookup(const Timing* table, size_t count, const char* protocol_name) {
    if(!protocol_name) return NULL;

    for(size_t i = 0; i < count; i++) {
        if(strcmp(protocol_name, table[i].name) == 0 || strstr(protocol_name, table[i].name) != NULL) {
            return &table[i];
        }
    }

    for(size_t a = 0; a < sizeof(aliases) / sizeof(aliases[0]); a++) {
        if(strstr(protocol_name, aliases[a].alias) == NULL) continue;
        for(size_t i = 0; i < count; i++) {
            if(strstr(table[i].name, aliases[a].canonical) != NULL) {
                return &table[i];
            }
        }
    }

    return NULL;
}

// Alle real bei mir registrierten Decoder-Namen (protocols/*.h *_PROTOCOL_NAME-Konstanten,
// bzw. die woertlichen .name-Strings der SubGhzProtocol-Structs, wo es keine eigene
// _PROTOCOL_NAME-Konstante gibt).
static const char* real_decoder_names[] = {
    "Chrysler V0", "Fiat V0",       "Fiat V1",  "Ford V0",         "Ford V1",
    "Ford V2",     "Ford V3",       "Honda Static", "Honda V1",    "Kia V0",
    "Kia V1",      "Kia V2",        "Kia V3/V4", "Kia V5",         "Kia V6",
    "Kia V7",      "Land Rover V0", "Mazda V0", "Porsche Touareg", "PSA",
    "Scher-Khan",  "Star Line",     "Subaru",   "VAG",
    // "Mitsubishi V0" ist auch in der Tabelle nicht enthalten (keine Regression, war nie
    // erreichbar) -- bewusst NICHT Teil dieses Fixes, siehe Report/Backlog-Eintrag.
};

int main(void) {
    size_t count_alt = sizeof(table_alt) / sizeof(table_alt[0]);
    size_t count_neu = sizeof(table_neu) / sizeof(table_neu[0]);
    size_t n = sizeof(real_decoder_names) / sizeof(real_decoder_names[0]);

    int failures = 0;

    printf("=== ALT (.name = \"VW\") ===\n");
    for(size_t i = 0; i < n; i++) {
        const Timing* t = lookup(table_alt, count_alt, real_decoder_names[i]);
        printf("  %-16s -> %s\n", real_decoder_names[i], t ? t->name : "NOT FOUND");
        if(strcmp(real_decoder_names[i], "VAG") == 0) {
            // The bug: VAG must NOT be found under ALT.
            if(t != NULL) {
                printf("    UNEXPECTED: VAG resolved under ALT (bug reproduction failed)\n");
                failures++;
            }
        } else {
            if(t == NULL) {
                printf("    UNEXPECTED: %s not found under ALT (should already work)\n", real_decoder_names[i]);
                failures++;
            }
        }
    }

    printf("\n=== NEU (.name = \"VAG\") ===\n");
    for(size_t i = 0; i < n; i++) {
        const Timing* t = lookup(table_neu, count_neu, real_decoder_names[i]);
        printf("  %-16s -> %s", real_decoder_names[i], t ? t->name : "NOT FOUND");
        if(t) {
            printf(" (short=%u long=%u delta=%u bits=%u)", t->te_short, t->te_long, t->te_delta, t->min_count_bit);
        }
        printf("\n");
        if(t == NULL) {
            printf("    UNEXPECTED: %s not found under NEU\n", real_decoder_names[i]);
            failures++;
        }
        if(strcmp(real_decoder_names[i], "VAG") == 0 && t) {
            if(t->te_short != 500 || t->te_long != 1000 || t->te_delta != 120 || t->min_count_bit != 80) {
                printf("    UNEXPECTED: VAG resolved to wrong timing values under NEU\n");
                failures++;
            }
        }
    }

    // Regression check: every other entry's resolved values are identical between ALT and NEU.
    printf("\n=== Regression: ALT vs. NEU identical for all names except VAG ===\n");
    for(size_t i = 0; i < n; i++) {
        if(strcmp(real_decoder_names[i], "VAG") == 0) continue;
        const Timing* a = lookup(table_alt, count_alt, real_decoder_names[i]);
        const Timing* b = lookup(table_neu, count_neu, real_decoder_names[i]);
        int same = (a && b && a->te_short == b->te_short && a->te_long == b->te_long &&
                    a->te_delta == b->te_delta && a->min_count_bit == b->min_count_bit &&
                    strcmp(a->name, b->name) == 0);
        printf("  %-16s -> %s\n", real_decoder_names[i], same ? "identical (OK)" : "DIFFERS (unexpected)");
        if(!same) failures++;
    }

    printf("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
