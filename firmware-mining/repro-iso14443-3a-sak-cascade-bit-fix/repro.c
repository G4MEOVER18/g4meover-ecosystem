// Host-Repro: iso14443_3a_poller_i.c, Kollisionsaufloesungsschleife
// (Iso14443_3aPollerColResStateStateSelectCascade-Zweig). Bei mir (mntm-012) entscheidet die
// Schleife, ob eine weitere Kaskadenstufe folgt, ausschliesslich anhand von
// "sel_req.nfcid[0] == 0x88" (dem Kaskadierungs-Tag-Byte aus der SDD-Antwort). ISO14443-3 sieht
// dafuer eigentlich das Kaskadenbit im SAK vor (sel_resp.sak & 0x04) -- ein Tag mit einer
// *echten* 4-Byte-UID, deren erstes Byte zufaellig 0x88 ist (nicht spezifikationskonform, aber
// auf realer Hardware existent, siehe Unleashed-Commit unten), wird bei mir faelschlich als
// unvollstaendig behandelt: die Schleife fragt eine zweite, nicht vorhandene Kaskadenstufe ab,
// die die Karte nicht mehr sinnvoll beantwortet -- der Lesevorgang schlaegt fehl bzw. liefert
// eine falsche UID, obwohl die Karte selbst korrekt geantwortet hat.
//
// ALT ist 1:1 die Entscheidung aus iso14443_3a_poller_i.c (nfcid[0] == 0x88). NEU ist die
// Unleashed-Fassung (sak & ISO14443_3A_POLLER_SAK_CASCADE_BIT), portiert aus Commit 38ba133f
// ("[NFC] Fix reading 4-byte UIDs that start with 0x88 (cascade tag)", #1075/#1074,
// hardware-verifiziert von @mishamyte).
//
// Der Test vergleicht ALT- und NEU-Entscheidung ("noch eine Kaskadenstufe?") ueber eine Reihe
// realistischer (nfcid[0], sak)-Antwortpaare: fuer jede *echte* Kaskadenfortsetzung setzt eine
// spezifikationskonforme Karte nfcid[0] UND das SAK-Kaskadenbit gemeinsam -- ALT und NEU stimmen
// dort immer ueberein. Nur im einen nicht-konformen, aber real vorkommenden Fall (finale 4-Byte-
// UID, die zufaellig mit 0x88 beginnt, SAK-Kaskadenbit korrekt geloescht) weichen sie voneinander
// ab -- exakt der von Unleashed gefixte Fall.
//
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ISO14443_3A_POLLER_SDD_CL          (0x88U)
#define ISO14443_3A_POLLER_SAK_CASCADE_BIT (0x04U)

// ALT: iso14443_3a_poller_i.c bei mir (mntm-012), unveraendert.
static bool old_should_continue_cascade(uint8_t nfcid0, uint8_t sak) {
    (void)sak;
    return nfcid0 == ISO14443_3A_POLLER_SDD_CL;
}

// NEU: Unleashed-Fassung ab Commit 38ba133f -- Entscheidung ueber das SAK-Kaskadenbit statt
// ueber das erste NFCID-Byte.
static bool new_should_continue_cascade(uint8_t nfcid0, uint8_t sak) {
    (void)nfcid0;
    return (sak & ISO14443_3A_POLLER_SAK_CASCADE_BIT) != 0;
}

typedef struct {
    const char* name;
    uint8_t nfcid0;
    uint8_t sak;
    bool real_continuation; // Ground Truth: ist das tatsaechlich eine Kaskadenfortsetzung?
} Sample;

static const Sample SAMPLES[] = {
    // Normalfall: 4-Byte-UID, nfcid[0] != 0x88 (Regelfall, kein Kaskadenbyte-Konflikt).
    {"MIFARE Classic 1K, normale 4-Byte-UID", 0x12, 0x08, false},
    // Normalfall: 7-Byte-UID, Kaskadenstufe 0 -- Karte setzt spezifikationskonform BEIDES:
    // nfcid[0]=0x88 UND SAK-Kaskadenbit.
    {"MIFARE Ultralight, 7-Byte-UID Stufe 0", 0x88, 0x04, true},
    // Normalfall: 10-Byte-UID, Kaskadenstufe 0 und 1 -- ebenfalls beides gesetzt.
    {"DESFire, 10-Byte-UID Stufe 0", 0x88, 0x04, true},
    {"DESFire, 10-Byte-UID Stufe 1", 0x88, 0x04, true},
    // Normalfall: 7-Byte-UID, finale Stufe -- kein Kaskadierungsbyte, kein Kaskadenbit mehr.
    {"MIFARE Ultralight, 7-Byte-UID finale Stufe", 0x51, 0x00, false},
    // Pathologisch, aber real (Unleashed #1074): echte 4-Byte-UID, die zufaellig mit 0x88
    // beginnt (manche MIFARE-Classic-/Magic-Karten) -- SAK ist final, Kaskadenbit korrekt
    // geloescht.
    {"MIFARE Classic (Magic), 4-Byte-UID beginnt mit 0x88", 0x88, 0x08, false},
};

int main(void) {
    int old_wrong_count = 0;
    int n = (int)(sizeof(SAMPLES) / sizeof(SAMPLES[0]));

    for(int i = 0; i < n; i++) {
        const Sample* s = &SAMPLES[i];
        bool old_decision = old_should_continue_cascade(s->nfcid0, s->sak);
        bool new_decision = new_should_continue_cascade(s->nfcid0, s->sak);
        bool old_correct = (old_decision == s->real_continuation);
        bool new_correct = (new_decision == s->real_continuation);

        printf(
            "%-48s nfcid0=0x%02X sak=0x%02X  ALT=%s(%s)  NEU=%s(%s)\n",
            s->name,
            s->nfcid0,
            s->sak,
            old_decision ? "continue" : "final  ",
            old_correct ? "richtig" : "FALSCH",
            new_decision ? "continue" : "final  ",
            new_correct ? "richtig" : "FALSCH");

        if(!new_correct) {
            fprintf(stderr, "FEHLER: NEU (SAK-Kaskadenbit) muesste in jedem Fall richtig liegen\n");
            return 1;
        }
        if(!old_correct) {
            old_wrong_count++;
        }
    }

    if(old_wrong_count != 1) {
        fprintf(
            stderr,
            "FEHLER: erwartet genau 1 Divergenzfall (die 0x88-Magic-Karte), gefunden %d\n",
            old_wrong_count);
        return 1;
    }

    printf(
        "\nOK: NEU (SAK-Kaskadenbit) entscheidet in allen %d Faellen richtig. ALT (nfcid[0]) "
        "irrt sich in genau 1 von %d Faellen -- der reale, hardware-bestaetigte Unleashed-"
        "Fund (#1074): eine echte, finale 4-Byte-UID, die zufaellig mit 0x88 beginnt, wird "
        "faelschlich als unvollstaendig behandelt.\n",
        n,
        n);
    return 0;
}
