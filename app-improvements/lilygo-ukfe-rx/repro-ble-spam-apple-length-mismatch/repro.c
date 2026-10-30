// Host-Repro: ble-spam-apple-length-mismatch-fix
//
// Bildet ausschliesslich den Byte-Aufbau von adv_apple() in
// lilygo-ukfe-rx/src/ble_spam.cpp (ALT und NEU) nach. Kein Arduino/ESP32/NimBLE-
// Abhaengigkeit: die eigentliche Pruefung ist reine Struktur-Arithmetik
// (TLV-Invariante "Laenge-Feld == Anzahl der danach folgenden Bytes"), die
// unabhaengig von Funkverhalten gilt.
//
// ALT und NEU sind Zeile-fuer-Zeile aus der jeweiligen Fassung von
// adv_apple() uebernommen (NEU = die hier vorgeschlagene Fix-Zeile).

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t  d[17];
    uint16_t model;      // zur Doku mitgefuehrt, geht nicht in die Pruefung ein
} AppleAdv;

// ALT: unveraenderte Fassung aus G4MEOVER18/lilygo-ukfe-rx, main, Commit aa8a285.
static AppleAdv build_old(uint16_t model, uint8_t r0, uint8_t r1, uint8_t r2) {
    AppleAdv a;
    a.model = model;
    uint8_t d[17] = {
        0x4C, 0x00,             // Apple Company ID
        0x07, 0x0F,             // Typ 0x07 (Proximity Pairing), Laenge
        0x05, 0xC1,             // Sub-Typ + Prefix
        (uint8_t)(model >> 8), (uint8_t)(model & 0xFF),
        0x00,                   // Status
        0x00, 0x00, 0x00,       // Batterie/Flags
        0x00,
        r0, r1, r2,
        0x00,
    };
    memcpy(a.d, d, sizeof(d));
    return a;
}

// NEU: vorgeschlagener Fix -- einzige Aenderung ist Index 3 (0x0F -> 0x0D).
static AppleAdv build_new(uint16_t model, uint8_t r0, uint8_t r1, uint8_t r2) {
    AppleAdv a;
    a.model = model;
    uint8_t d[17] = {
        0x4C, 0x00,             // Apple Company ID
        0x07, 0x0D,             // Typ 0x07 (Proximity Pairing), Laenge (13 Bytes nach diesem Feld)
        0x05, 0xC1,             // Sub-Typ + Prefix
        (uint8_t)(model >> 8), (uint8_t)(model & 0xFF),
        0x00,                   // Status
        0x00, 0x00, 0x00,       // Batterie/Flags
        0x00,
        r0, r1, r2,
        0x00,
    };
    memcpy(a.d, d, sizeof(d));
    return a;
}

static int checks_run = 0, checks_failed = 0;

static void check(const char* name, int ok) {
    checks_run++;
    if(!ok) { checks_failed++; printf("FAIL: %s\n", name); }
    else    { printf("ok:   %s\n", name); }
}

// TLV-Grundregel (bestaetigt gegen die oeffentlich bekannte ECTO-1A/AppleJuice-
// Referenzstruktur: Company-ID(2) Typ(1) Laenge(1) Payload(Laenge Bytes); dort
// ist Laenge=0x19=25 und genau 25 Bytes folgen danach -- derselbe Aufbau wie
// hier, nur mit einer kuerzeren Payload-Variante): das Laenge-Feld (Index 3)
// muss exakt der Anzahl der Bytes entsprechen, die im Array NACH Index 3 noch
// folgen.
static uint8_t declared_length(const uint8_t d[17]) { return d[3]; }
static uint8_t actual_trailing_bytes(size_t total) { return (uint8_t)(total - 4); } // -CompanyID(2) -Typ(1) -Laenge(1)

int main(void) {
    // Drei repraesentative Zufalls-Byte-Kombinationen (Position/Wert der
    // Zufallsbytes beeinflusst die Laengen-Invariante ohnehin nicht -- sie
    // sind reiner Fuellinhalt zwischen Index 13 und 15).
    const uint8_t combos[][3] = { {0x00,0x00,0x00}, {0xAB,0xCD,0xEF}, {0xFF,0x01,0x7F} };
    const uint16_t models[] = { 0x0220, 0x0e20, 0x1420 }; // je ein Modell aus APPLE_MODELS

    uint8_t actual = actual_trailing_bytes(sizeof(((AppleAdv*)0)->d));
    check("Payload nach Laenge-Feld ist 13 Bytes (sizeof(d)-4)", actual == 13);

    for(int i = 0; i < 3; i++) {
        AppleAdv o = build_old(models[i], combos[i][0], combos[i][1], combos[i][2]);
        AppleAdv n = build_new(models[i], combos[i][0], combos[i][1], combos[i][2]);

        char label[96];
        snprintf(label, sizeof(label), "ALT: Laenge-Feld (0x%02X) != tatsaechliche %u Folgebytes [combo %d]",
                 declared_length(o.d), actual, i);
        check(label, declared_length(o.d) != actual);   // ALT ist bewusst falsch -> Erwartung: Mismatch

        snprintf(label, sizeof(label), "NEU: Laenge-Feld (0x%02X) == tatsaechliche %u Folgebytes [combo %d]",
                 declared_length(n.d), actual, i);
        check(label, declared_length(n.d) == actual);   // NEU muss stimmen

        // Regression: ausser Index 3 duerfen sich ALT und NEU in keinem
        // einzigen Byte unterscheiden -- der Fix darf Modell-/Zufalls-/
        // Status-/Batterie-Bytes nicht antasten.
        int only_index3_differs = 1;
        for(int k = 0; k < 17; k++) {
            if(k == 3) continue;
            if(o.d[k] != n.d[k]) { only_index3_differs = 0; break; }
        }
        snprintf(label, sizeof(label), "Regression: ALT/NEU unterscheiden sich ausschliesslich in Index 3 [combo %d]", i);
        check(label, only_index3_differs);

        snprintf(label, sizeof(label), "Regression: Array-Groesse bleibt 17 Bytes [combo %d]", i);
        check(label, sizeof(o.d) == 17 && sizeof(n.d) == 17);

        snprintf(label, sizeof(label), "Regression: Company-ID/Typ unveraendert (0x4C,0x00,0x07) [combo %d]", i);
        check(label, n.d[0] == 0x4C && n.d[1] == 0x00 && n.d[2] == 0x07);

        snprintf(label, sizeof(label), "Regression: Modell-Bytes unveraendert [combo %d]", i);
        check(label, n.d[6] == (uint8_t)(models[i] >> 8) && n.d[7] == (uint8_t)(models[i] & 0xFF));

        snprintf(label, sizeof(label), "Regression: Zufallsbytes an Index 13..15 unveraendert [combo %d]", i);
        check(label, n.d[13] == combos[i][0] && n.d[14] == combos[i][1] && n.d[15] == combos[i][2]);
    }

    // Exakter Wert des korrigierten Laenge-Feldes (nicht nur "stimmt mit
    // Payload ueberein", sondern konkret der erwartete Literalwert 0x0D).
    AppleAdv n0 = build_new(models[0], 0, 0, 0);
    check("NEU: Laenge-Feld ist exakt 0x0D (13)", n0.d[3] == 0x0D);

    printf("\n%s (%d total)\n", checks_failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED",
           checks_run);
    return checks_failed == 0 ? 0 : 1;
}
