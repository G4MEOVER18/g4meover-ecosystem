// Host-Mini-Repro: nfc_sniffer_callback() (applications/external/nfc_sniffer/ui/log_scene.c,
// RogueMaster-Bundle, Quelle kdiller713/public-FlipperApps) baut fuer jedes empfangene,
// nicht-leere NFC-Kommando einen Hex-String in einem Stack-Array, dessen Groesse exakt fuer
// "1 Zeilenumbruch + 2*N Hex-Zeichen + 1 NUL-Terminator" bemessen ist -- schreibt aber VOR der
// eigentlichen Hex-Umwandlung einen zusaetzlichen NUL-Terminator eine Position hinter das Ende
// dieses Arrays (gueltige Indizes: 0 .. 2*buffer_length+1, geschrieben wird auf
// 2*buffer_length+2). Die nachfolgende Schleife ist korrekt bemessen und schreibt ihren eigenen
// Terminator bereits an der richtigen Stelle -- die vorab ausgefuehrte Zeile ist damit sowohl
// ueberfluessig als auch ein Ein-Byte-Stack-Overwrite, ausgeloest bei jedem einzelnen,
// nicht-leeren Kommando waehrend des Sniffens (Normalbetrieb der App, kein Sonderfall).
//
// Dieses Repro bildet die Array-Groessen-/Index-Rechnung 1:1 aus dem Original nach
// (Flipper-spezifische Typen BitBuffer/bit_buffer_get_byte() durch ein einfaches uint8_t-Array
// ersetzt, kein echtes Furi/NFC-HAL noetig). ALT enthaelt die fehlerhafte Zeile, NEU (Compile
// mit -DFIXED) hat sie entfernt -- exakt der im Port-Vorschlag beschriebene Fix.
#include <stdint.h>
#include <stdio.h>

#ifdef FIXED
static void append_hex_fixed(size_t buffer_length, const uint8_t* data, char* out) {
    char output[2 * buffer_length + 2]; // gueltige Indizes: 0 .. 2*buffer_length+1
    output[0] = '\n';
    // (entfernt: output[2 * buffer_length + 2] = '\0'; -- ueberfluessig und OOB)

    for(size_t i = 0; i < buffer_length; i++) {
        snprintf(&output[i * 2 + 1], 3, "%02X", data[i]);
    }
    for(size_t i = 0; i < 2 * buffer_length + 2; i++) out[i] = output[i];
}
#else
static void append_hex_old(size_t buffer_length, const uint8_t* data, char* out) {
    char output[2 * buffer_length + 2]; // gueltige Indizes: 0 .. 2*buffer_length+1
    output[0] = '\n';
    output[2 * buffer_length + 2] = '\0'; // <- 1 Byte HINTER dem Array

    for(size_t i = 0; i < buffer_length; i++) {
        snprintf(&output[i * 2 + 1], 3, "%02X", data[i]);
    }
    for(size_t i = 0; i < 2 * buffer_length + 2; i++) out[i] = output[i];
}
#endif

int main(void) {
    // Steht fuer ein 4-Byte NFC-Kommando (z. B. ein ISO14443-3A REQA/Anticollision-Frame).
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    char out[16] = {0};

#ifdef FIXED
    append_hex_fixed(4, data, out);
#else
    append_hex_old(4, data, out);
#endif

    printf("output=%s\n", out);
    return 0;
}

// Ergebnis (gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined):
//
// ALT (ohne -DFIXED):
//   ==...==ERROR: AddressSanitizer: dynamic-stack-buffer-overflow ...
//   WRITE of size 1 at ... thread T0
//       #0 ... in append_hex_old repro.c:34
//       #1 ... in main repro.c:51
//   SUMMARY: AddressSanitizer: dynamic-stack-buffer-overflow repro.c:34 in append_hex_old
//   Exit-Code: 1 (Abbruch)
//
// NEU (mit -DFIXED):
//   output=
//   DEADBEEF
//   Exit-Code: 0 (sauberer Lauf, korrekte Ausgabe)
