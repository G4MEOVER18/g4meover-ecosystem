/* Host repro: FlipperJammer scene-manager navigation stack model.
 *
 * Covers ALL ten "Run" scenes reachable through the shared Duration-picker
 * scene (SubGhz, Nrf24, Ble, Wifi, Ir, UsbFuzzer, RfidJam, NfcJam,
 * RfidFuzzer, NfcFuzzer). The push/pop/search-and-switch algorithms below
 * are transcribed 1:1 from
 * applications/services/gui/scene_manager.c (flipperdevices/flipperzero-firmware,
 * functions scene_manager_next_scene / scene_manager_previous_scene /
 * scene_manager_search_and_switch_to_previous_scene) with the m-lib deque
 * replaced by a fixed-size array (identical push_back/pop_back/search
 * semantics, verified against the upstream source read for this session). */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#define MAX_STACK 16

typedef struct {
    uint32_t items[MAX_STACK];
    int size;
} Stack;

static void stack_push(Stack* s, uint32_t id) {
    assert(s->size < MAX_STACK);
    s->items[s->size++] = id;
}

/* scene_manager_next_scene */
static void sm_next_scene(Stack* s, uint32_t next_id) {
    stack_push(s, next_id);
}

/* scene_manager_previous_scene */
static bool sm_previous_scene(Stack* s) {
    if(s->size == 0) return false;
    s->size--;
    if(s->size == 0) return false;
    return true;
}

/* scene_manager_search_and_switch_to_previous_scene */
static bool sm_search_and_switch_to_previous_scene(Stack* s, uint32_t scene_id) {
    if(s->size == 0) return false;
    bool found = false;
    int found_at = -1;
    for(int i = s->size - 2; i >= 0; i--) {
        if(s->items[i] == scene_id) {
            found = true;
            found_at = i;
            break;
        }
    }
    if(!found) return false;
    s->size = found_at + 1;
    return true;
}

static uint32_t stack_top(const Stack* s) {
    assert(s->size > 0);
    return s->items[s->size - 1];
}

/* ------------------------------------------------------------------ */
enum {
    SceneMainMenu = 1,
    SceneRfidMenu,
    SceneSubGhzConfig,
    SceneSubGhzRun,
    SceneNrf24Config,
    SceneNrf24Run,
    SceneBleConfig,
    SceneBleRun,
    SceneWifiConfig,
    SceneWifiRun,
    SceneIrConfig,
    SceneIrRun,
    SceneUsbFuzzerConfig,
    SceneUsbFuzzerRun,
    SceneRfidJamRun,
    SceneNfcJamRun,
    SceneRfidFuzzerRun,
    SceneNfcFuzzerRun,
    SceneDuration,
};

typedef struct {
    const char* name;
    uint32_t    run_scene;
    uint32_t    parent_scene;  /* the scene that owns this Run scene */
    uint32_t    menu_scene;    /* MainMenu, or RfidMenu for the RFID/NFC family */
    bool        has_direct_start; /* does a Start-without-Duration path exist? */
} Module;

static const Module MODULES[] = {
    {"SubGhz",      SceneSubGhzRun,     SceneSubGhzConfig,     SceneMainMenu, true},
    {"Nrf24",       SceneNrf24Run,      SceneNrf24Config,      SceneMainMenu, true},
    {"Ble",         SceneBleRun,        SceneBleConfig,        SceneMainMenu, true},
    {"Wifi",        SceneWifiRun,       SceneWifiConfig,       SceneMainMenu, true},
    {"Ir",          SceneIrRun,         SceneIrConfig,         SceneMainMenu, true},
    {"UsbFuzzer",   SceneUsbFuzzerRun,  SceneUsbFuzzerConfig,  SceneMainMenu, true},
    {"RfidJam",     SceneRfidJamRun,    SceneRfidMenu,         SceneRfidMenu, false},
    {"NfcJam",      SceneNfcJamRun,     SceneRfidMenu,         SceneRfidMenu, false},
    {"RfidFuzzer",  SceneRfidFuzzerRun, SceneRfidMenu,         SceneRfidMenu, false},
    {"NfcFuzzer",   SceneNfcFuzzerRun,  SceneRfidMenu,         SceneRfidMenu, false},
};

/* Builds the navigation stack as the real app would, for one module,
 * one entry path ("direct Start" or "via Duration"). */
static void build_stack(Stack* s, const Module* m, bool via_duration) {
    memset(s, 0, sizeof(*s));
    sm_next_scene(s, SceneMainMenu);
    if(m->menu_scene != SceneMainMenu) {
        sm_next_scene(s, m->menu_scene); /* e.g. RfidMenu, for the RFID/NFC family */
    }
    if(m->parent_scene != m->menu_scene) {
        sm_next_scene(s, m->parent_scene); /* e.g. SubGhzConfig */
    }
    if(via_duration) {
        sm_next_scene(s, SceneDuration);
    }
    sm_next_scene(s, m->run_scene);
}

int main(void) {
    int checks = 0, fails = 0;
    size_t n = sizeof(MODULES) / sizeof(MODULES[0]);

    for(size_t i = 0; i < n; i++) {
        const Module* m = &MODULES[i];

        /* Path A: via Duration (always possible, and the ONLY path for the
         * four RFID/NFC modules). ALT must reproduce the bug; NEW must not. */
        {
            Stack s_old, s_new;
            build_stack(&s_old, m, true);
            build_stack(&s_new, m, true);

            sm_previous_scene(&s_old); /* ALT: plain pop */
            sm_search_and_switch_to_previous_scene(&s_new, m->parent_scene); /* NEW */

            uint32_t top_old = stack_top(&s_old);
            uint32_t top_new = stack_top(&s_new);

            printf("[%-11s via Duration] ALT top=%2u (parent=%2u, Duration=%2u) %s | NEW top=%2u %s\n",
                m->name, top_old, m->parent_scene, (uint32_t)SceneDuration,
                top_old == SceneDuration ? "<-- stuck on Duration picker" : "??",
                top_new,
                top_new == m->parent_scene ? "OK (parent)" : "FAIL");

            checks++;
            if(top_old != SceneDuration) { printf("  FAIL: expected ALT to reproduce the bug (land on Duration)\n"); fails++; }
            checks++;
            if(top_new != m->parent_scene) { printf("  FAIL: expected NEW to land on the real parent scene\n"); fails++; }
        }

        /* Path B: direct Start (only exists for modules with a Start item
         * that skips Duration). Here ALT is already accidentally correct;
         * NEW must not regress it. */
        if(m->has_direct_start) {
            Stack s_old, s_new;
            build_stack(&s_old, m, false);
            build_stack(&s_new, m, false);

            sm_previous_scene(&s_old);
            sm_search_and_switch_to_previous_scene(&s_new, m->parent_scene);

            uint32_t top_old = stack_top(&s_old);
            uint32_t top_new = stack_top(&s_new);

            printf("[%-11s direct Start ] ALT top=%2u %s | NEW top=%2u %s\n",
                m->name, top_old, top_old == m->parent_scene ? "OK" : "FAIL",
                top_new, top_new == m->parent_scene ? "OK (no regression)" : "FAIL");

            checks++;
            if(top_old != m->parent_scene) { printf("  FAIL: ALT unexpectedly wrong on the direct-Start path\n"); fails++; }
            checks++;
            if(top_new != m->parent_scene) { printf("  FAIL: NEW regressed the direct-Start path\n"); fails++; }
        }
    }

    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
