// Host-Repro: NfcProtocol[]-memcpy mit Elementzahl statt Bytezahl
// (lib/nfc/nfc_scanner.c::nfc_scanner_filter_detected_protocols(),
//  applications/main/nfc/helpers/nfc_detected_protocols.c::nfc_detected_protocols_set()).
// Reine Host-Logik, keine furi_hal-Abhaengigkeit. ALT = eigene mntm-012-Basis,
// NEU = gebundene Fassung aus Unleashed-Commit 9ef0b565e / 2b4411f23.
//
// Kein Speicherfehler (beide Arrays sind NfcProtocolNum gross) -- reiner
// Korrektheitsfehler: zu wenige Bytes kopiert, Zielarray behaelt teilweise
// seinen alten Inhalt. Deshalb kein ASan/UBSan-Befund zu erwarten; die
// Pruefung erfolgt per Wertevergleich.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef enum {
    NfcProtocolIso14443_3a,
    NfcProtocolIso14443_3b,
    NfcProtocolIso14443_4a,
    NfcProtocolIso14443_4b,
    NfcProtocolIso15693_3,
    NfcProtocolFelica,
    NfcProtocolMfUltralight,
    NfcProtocolMfClassic,
    NfcProtocolMfPlus,
    NfcProtocolMfDesfire,
    NfcProtocolSlix,
    NfcProtocolSt25tb,
    NfcProtocolNtag4xx,
    NfcProtocolType4Tag,
    NfcProtocolEmv,
    NfcProtocolNum,
} NfcProtocol;

// --- Nachbildung von nfc_scanner_filter_detected_protocols() ---------------

static void filter_alt(
    NfcProtocol* detected_protocols,
    size_t* detected_protocols_num,
    const NfcProtocol* filtered_protocols,
    size_t filtered_protocols_num) {
    *detected_protocols_num = filtered_protocols_num;
    // ALT: bei mir (mntm-012) so -- kopiert filtered_protocols_num BYTES.
    memcpy(detected_protocols, filtered_protocols, filtered_protocols_num);
}

static void filter_neu(
    NfcProtocol* detected_protocols,
    size_t* detected_protocols_num,
    const NfcProtocol* filtered_protocols,
    size_t filtered_protocols_num) {
    *detected_protocols_num = filtered_protocols_num;
    // NEU: Unleashed-Commit 9ef0b565e -- kopiert filtered_protocols_num ELEMENTE.
    memcpy(
        detected_protocols,
        filtered_protocols,
        filtered_protocols_num * sizeof(NfcProtocol));
}

// --- Nachbildung von nfc_detected_protocols_set() ---------------------------

static void set_alt(NfcProtocol* protocols_detected, const NfcProtocol* types, uint32_t count) {
    // ALT: bei mir (mntm-012) so -- kopiert count BYTES.
    memcpy(protocols_detected, types, count);
}

static void set_neu(NfcProtocol* protocols_detected, const NfcProtocol* types, uint32_t count) {
    // NEU: Unleashed-Commit 2b4411f23 -- kopiert count ELEMENTE.
    memcpy(protocols_detected, types, count * sizeof(NfcProtocol));
}

static int fails = 0;

static void check(const char* label, int ok) {
    printf("  %s: %s\n", label, ok ? "ok" : "FEHLGESCHLAGEN");
    if(!ok) fails++;
}

int main(void) {
    printf("sizeof(NfcProtocol) = %zu\n\n", sizeof(NfcProtocol));

    // --- Fall 1 (Regressionsfall): genau ein verbleibendes Protokoll --------
    // Haeufigster Scanner-Fall. Jeder Slot von `detected_protocols` ist vorher
    // stets per vollem Enum-Assignment (4 Byte) beschrieben worden, die drei
    // hoeherwertigen Bytes sind also auf Little-Endian-Zielen (Flipper: Cortex-M4,
    // STM32WB55) bereits 0 -- die 1-Byte-Truncated-Kopie trifft deshalb in diesem
    // Einzelfall rein zufaellig noch den richtigen Wert. Kein Funktionsunterschied
    // ALT/NEU zu erwarten; absichtlich als Regressionsfall mitgefuehrt.
    printf(
        "== Fall 1 (Regression): nfc_scanner_filter_detected_protocols(), 1 "
        "verbleibendes Protokoll ==\n");
    {
        NfcProtocol detected[NfcProtocolNum] = {0};
        detected[0] = NfcProtocolIso14443_3a;
        size_t num = 0;
        NfcProtocol filtered[1] = {NfcProtocolMfPlus};
        filter_alt(detected, &num, filtered, 1);
        check(
            "ALT: Slot 0 zufaellig trotzdem korrekt (hoeherwertige Bytes waren bereits 0)",
            detected[0] == NfcProtocolMfPlus);
    }
    {
        NfcProtocol detected[NfcProtocolNum] = {0};
        detected[0] = NfcProtocolIso14443_3a;
        size_t num = 0;
        NfcProtocol filtered[1] = {NfcProtocolMfPlus};
        filter_neu(detected, &num, filtered, 1);
        check("NEU: Slot 0 zeigt korrekt MfPlus", detected[0] == NfcProtocolMfPlus);
    }

    // --- Fall 2: zwei verbleibende, voneinander unabhaengige Protokolle -----
    printf(
        "\n== Fall 2: nfc_scanner_filter_detected_protocols(), 2 verbleibende "
        "Protokolle ==\n");
    {
        NfcProtocol detected[NfcProtocolNum] = {0};
        detected[0] = NfcProtocolIso14443_3a;
        detected[1] = NfcProtocolIso14443_3b;
        size_t num = 0;
        NfcProtocol filtered[2] = {NfcProtocolMfClassic, NfcProtocolMfDesfire};
        filter_alt(detected, &num, filtered, 2);
        check(
            "ALT: Slot 1 bleibt vollstaendig unberuehrt (alter Wert statt MfDesfire)",
            detected[1] != NfcProtocolMfDesfire);
    }
    {
        NfcProtocol detected[NfcProtocolNum] = {0};
        detected[0] = NfcProtocolIso14443_3a;
        detected[1] = NfcProtocolIso14443_3b;
        size_t num = 0;
        NfcProtocol filtered[2] = {NfcProtocolMfClassic, NfcProtocolMfDesfire};
        filter_neu(detected, &num, filtered, 2);
        check(
            "NEU: Slot 0/1 zeigen korrekt MfClassic/MfDesfire",
            detected[0] == NfcProtocolMfClassic && detected[1] == NfcProtocolMfDesfire);
    }

    // --- Fall 3: nfc_detected_protocols_set(), gleiche Fehlerklasse ---------
    printf("\n== Fall 3: nfc_detected_protocols_set(), 2 erkannte Protokolle ==\n");
    {
        NfcProtocol protocols_detected[NfcProtocolNum] = {0};
        protocols_detected[0] = NfcProtocolFelica;
        protocols_detected[1] = NfcProtocolSlix;
        NfcProtocol types[2] = {NfcProtocolMfClassic, NfcProtocolMfPlus};
        set_alt(protocols_detected, types, 2);
        check(
            "ALT: zweites Protokoll in der Auswahlliste bleibt faelschlich Slix statt MfPlus",
            protocols_detected[1] != NfcProtocolMfPlus);
    }
    {
        NfcProtocol protocols_detected[NfcProtocolNum] = {0};
        protocols_detected[0] = NfcProtocolFelica;
        protocols_detected[1] = NfcProtocolSlix;
        NfcProtocol types[2] = {NfcProtocolMfClassic, NfcProtocolMfPlus};
        set_neu(protocols_detected, types, 2);
        check(
            "NEU: Auswahlliste zeigt korrekt MfClassic/MfPlus",
            protocols_detected[0] == NfcProtocolMfClassic &&
                protocols_detected[1] == NfcProtocolMfPlus);
    }

    // --- Regressionsfall 2: ALT und NEU identisch, wenn kein Protokoll erkannt ist (count == 0)
    printf("\n== Regressionsfall 2: count == 0 (kein Protokoll erkannt) ==\n");
    {
        NfcProtocol detected_alt[NfcProtocolNum] = {0};
        NfcProtocol detected_neu[NfcProtocolNum] = {0};
        size_t num_alt = 0, num_neu = 0;
        NfcProtocol filtered[1] = {NfcProtocolMfPlus}; // ungenutzt bei count == 0
        filter_alt(detected_alt, &num_alt, filtered, 0);
        filter_neu(detected_neu, &num_neu, filtered, 0);
        check(
            "ALT und NEU verhalten sich bei 0 Protokollen identisch (kein Funktionsverlust)",
            num_alt == 0 && num_neu == 0 &&
                memcmp(detected_alt, detected_neu, sizeof(detected_alt)) == 0);
    }

    printf("\n%s\n", fails == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED");
    return fails == 0 ? 0 : 1;
}
