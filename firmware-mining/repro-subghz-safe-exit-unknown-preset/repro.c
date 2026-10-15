// Host-Repro fuer firmware-mining/subghz-safe-exit-unknown-preset.
//
// Bildet den Kernmechanismus des Fixes nach: subghz_setting_get_inx_preset_by_name()
// und subghz_setting_get_preset_data_by_name() (lib/subghz/subghz_setting.c) sowie einen
// stellvertretenden Aufrufer (subghz_txrx_rx_start()-Muster aus
// applications/main/subghz/helpers/subghz_txrx.c), ALT (unveraendert, mntm-012) und NEU
// (mit dem portierten Fix) nebeneinander. furi_crash() wird exakt wie im Flipper-SDK-Stub
// durch einen sofortigen, erkennbaren Prozessabbruch nachgebildet (hier: _exit(42)) statt
// eines echten Hardware-Halts -- fuer den Testzweck aequivalent: beide beenden den
// laufenden Vorgang unkontrolliert, nur der Exit-Code unterscheidet sich von einer echten
// Rueckkehr.
//
// Aufruf: ./repro <known|unknown> <alt|neu>
//   known:   preset_name verweist auf einen tatsaechlich geladenen Preset -- beide
//            Versionen liefern denselben, korrekten Datenzeiger, kein Unterschied.
//   unknown: preset_name verweist auf keinen geladenen Preset (z. B. ein per
//            "Custom_preset_data" im .sub geladener, danach wieder entfernter Preset, oder
//            ein per RPC/mobiler App gesetzter Name, der nie existierte) --
//            ALT: furi_crash() in subghz_setting_get_inx_preset_by_name(), Exit-Code 42.
//            NEU: Fehler geloggt, subghz_setting_get_preset_data_by_name() liefert NULL,
//                 der Aufrufer bricht den RX-Start kontrolliert ab (return), Exit-Code 0.
//
// Build:
//   gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -fsanitize=address,undefined
//   -fno-sanitize-recover=undefined -o repro repro.c
//
// Erwartung (gegen zwei frische, unabhaengige Zweitkopien von G4MEOVER-FW
// patch-verifiziert, unter -O0 und -O1):
//   ./repro known   alt   -> Exit 0, "RX gestartet mit Preset 'Ook650'"
//   ./repro known   neu   -> Exit 0, "RX gestartet mit Preset 'Ook650'"  (identisch)
//   ./repro unknown alt   -> Exit 42 (furi_crash ausgeloest, kein weiterer Output moeglich)
//   ./repro unknown neu   -> Exit 0, "RX NICHT gestartet -- unbekannter Preset 'Ook650'",
//                            Prozess bleibt am Leben

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// --- Stellvertreter fuer SubGhzSettingCustomPresetItemArray / SubGhzSetting ---

typedef struct {
    const char* name;
    uint8_t* data;
} PresetItem;

static uint8_t ook650_data[] = {0x08, 0x32, 0x00};

static PresetItem g_presets[] = {
    {"AM270", NULL},
    {"AM650", NULL},
    {"FM238", NULL},
    {"FM476", NULL},
    {"Ook650", ook650_data},
};
#define PRESET_COUNT (sizeof(g_presets) / sizeof(g_presets[0]))

// --- furi_crash()-Stub: wie im echten Build ein nicht fortsetzbarer Halt, hier als
// erkennbarer Sofort-Exit statt eines echten Hardware-Resets. ---
static void furi_crash(const char* message) {
    fprintf(stderr, "furi_crash(): %s\n", message);
    _exit(42);
}

// --- subghz_setting_get_inx_preset_by_name() ---

static int get_inx_preset_by_name_alt(const char* preset_name) {
    for(size_t idx = 0; idx < PRESET_COUNT; idx++) {
        if(strcmp(g_presets[idx].name, preset_name) == 0) {
            return (int)idx;
        }
    }
    furi_crash("SubGhz: No name preset."); // <-- ALT: haelt hier an, kehrt nie zurueck
    return -1;
}

static int get_inx_preset_by_name_neu(const char* preset_name) {
    for(size_t idx = 0; idx < PRESET_COUNT; idx++) {
        if(strcmp(g_presets[idx].name, preset_name) == 0) {
            return (int)idx;
        }
    }
    fprintf(stderr, "[SubGhzSetting] No preset with name \"%s\"\n", preset_name);
    return -1;
}

// --- subghz_setting_get_preset_data_by_name() ---

static uint8_t* get_preset_data_by_name_alt(const char* preset_name) {
    int idx = get_inx_preset_by_name_alt(preset_name); // crasht hier bereits bei Unbekanntem
    return g_presets[idx].data;
}

static uint8_t* get_preset_data_by_name_neu(const char* preset_name) {
    int idx = get_inx_preset_by_name_neu(preset_name);
    if(idx < 0) {
        return NULL;
    }
    return g_presets[idx].data;
}

// --- subghz_txrx_rx_start()-Stellvertreter ---

static void subghz_txrx_begin_stub(uint8_t* preset_data) {
    (void)preset_data;
}

static void subghz_txrx_rx_stub(void) {
    // Stellvertreter fuer den tatsaechlichen Funkempfang -- im Repro nur Markierung,
    // dass der RX-Start-Pfad tatsaechlich erreicht wurde.
}

static void subghz_txrx_rx_start_alt(const char* preset_name) {
    subghz_txrx_begin_stub(get_preset_data_by_name_alt(preset_name)); // crasht bei Unbekanntem
    subghz_txrx_rx_stub();
    printf("RX gestartet mit Preset '%s'\n", preset_name);
}

static void subghz_txrx_rx_start_neu(const char* preset_name) {
    uint8_t* preset_data = get_preset_data_by_name_neu(preset_name);
    if(preset_data == NULL) {
        printf("RX NICHT gestartet -- unbekannter Preset '%s'\n", preset_name);
        return;
    }
    subghz_txrx_begin_stub(preset_data);
    subghz_txrx_rx_stub();
    printf("RX gestartet mit Preset '%s'\n", preset_name);
}

int main(int argc, char** argv) {
    if(argc != 3) {
        fprintf(stderr, "Aufruf: %s <known|unknown> <alt|neu>\n", argv[0]);
        return 2;
    }
    const char* scenario = argv[1];
    const char* variant = argv[2];

    const char* preset_name;
    if(strcmp(scenario, "known") == 0) {
        preset_name = "Ook650"; // existiert in g_presets
    } else if(strcmp(scenario, "unknown") == 0) {
        preset_name = "Ook650_stale"; // existiert nicht (z. B. geloeschter Custom-Preset)
    } else {
        fprintf(stderr, "Unbekanntes Szenario '%s'\n", scenario);
        return 2;
    }

    if(strcmp(variant, "alt") == 0) {
        subghz_txrx_rx_start_alt(preset_name);
    } else if(strcmp(variant, "neu") == 0) {
        subghz_txrx_rx_start_neu(preset_name);
    } else {
        fprintf(stderr, "Unbekannte Variante '%s'\n", variant);
        return 2;
    }

    printf("Prozess am Leben, Exit 0.\n");
    return 0;
}
