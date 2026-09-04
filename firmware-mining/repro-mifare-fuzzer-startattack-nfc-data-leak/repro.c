// Host repro: heap leak of the Iso14443_3aData allocated at the top of the
// MifareFuzzerEventStartAttack handler in
// scenes/mifare_fuzzer_scene_emulator.c.
//
// The real Iso14443_3aData/iso14443_3a_alloc()/iso14443_3a_free() come from
// the Flipper NFC library and are not available on a host build, so this
// mirrors them with real malloc()/free() wrappers around a dummy struct of
// the same role, and reproduces the exact call sequence the handler makes
// once per MifareFuzzerEventStartAttack (fired once per manual "Start" press
// AND once per automatic UID-cycle tick while an attack is running -- see
// the SceneManagerEventTypeTick handler in the same file).
//
// Build/run under ASan with leak detection to make the missing free()
// observable directly instead of only by code-reading:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
// ALT (unpatched): reports N reachable-at-exit blocks (one per simulated
// StartAttack cycle) via LeakSanitizer.
// NEU (patched): exits clean, zero leaked blocks.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char uid[10];
    unsigned char uid_len;
    unsigned char atqa[2];
    unsigned char sak;
} Iso14443_3aDataMock;

static Iso14443_3aDataMock* iso14443_3a_alloc_mock(void) {
    return calloc(1, sizeof(Iso14443_3aDataMock));
}
static void iso14443_3a_free_mock(Iso14443_3aDataMock* d) {
    free(d);
}

// Mirrors mifare_fuzzer_worker_set_nfc_data(): takes the struct BY VALUE,
// makes its OWN internal copy, and correctly frees that internal copy again
// -- this part of the app already gets the alloc/free pairing right.
static void worker_set_nfc_data_mock(Iso14443_3aDataMock nfc_data) {
    Iso14443_3aDataMock* internal_copy = iso14443_3a_alloc_mock();
    memcpy(internal_copy, &nfc_data, sizeof(nfc_data));
    iso14443_3a_free_mock(internal_copy);
}

// Mirrors mifare_fuzzer_emulator_set_nfc_data(): also takes the struct BY
// VALUE and stores it into a view model -- no heap allocation at all.
static Iso14443_3aDataMock g_emulator_model_nfc_data;
static void emulator_set_nfc_data_mock(Iso14443_3aDataMock nfc_data) {
    g_emulator_model_nfc_data = nfc_data;
}

// --- ALT: verbatim shape of the shipped MifareFuzzerEventStartAttack handler ---
static void start_attack_cycle_ALT(void) {
    Iso14443_3aDataMock* nfc_data = iso14443_3a_alloc_mock();
    nfc_data->uid_len = 4;
    nfc_data->uid[0] = 0x11;

    emulator_set_nfc_data_mock(*nfc_data);
    worker_set_nfc_data_mock(*nfc_data);
    // <-- missing iso14443_3a_free(nfc_data) here: leaked every call.
}

// --- NEU: same shape, with the missing free added at the same spot ---
static void start_attack_cycle_NEU(void) {
    Iso14443_3aDataMock* nfc_data = iso14443_3a_alloc_mock();
    nfc_data->uid_len = 4;
    nfc_data->uid[0] = 0x11;

    emulator_set_nfc_data_mock(*nfc_data);
    worker_set_nfc_data_mock(*nfc_data);
    iso14443_3a_free_mock(nfc_data);
}

int main(int argc, char** argv) {
    // Simulate a short unattended "Random Values" attack: one StartAttack
    // call for the initial "Start" press, plus N automatic UID-cycle ticks
    // -- exactly how the app is meant to be used, no malformed input needed.
    int cycles = (argc > 1) ? atoi(argv[1]) : 50;
    int use_neu = (argc > 2) && (strcmp(argv[2], "neu") == 0);

    for(int i = 0; i < cycles; i++) {
        if(use_neu) {
            start_attack_cycle_NEU();
        } else {
            start_attack_cycle_ALT();
        }
    }

    printf(
        "Simulated %d MifareFuzzerEventStartAttack cycles (%s).\n",
        cycles,
        use_neu ? "NEU, fixed" : "ALT, unpatched");
    printf("Exiting now -- ASan LeakSanitizer will report anything still allocated.\n");
    return 0;
}
