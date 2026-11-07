// Host repro: lora-ukfe scenes/scene_menu.c menu_callback() dispatch, before and
// after wiring a top-level "Einstellungen" entry to UkfeSceneSettings.
//
// Transcribed 1:1 from scenes/scene_menu.c (G4MEOVER18/lora-ukfe, 0dba410a) with
// the GUI/scene-manager calls replaced by recorded events (no Flipper headers
// needed): submenu_add_item() -> record an (id,label) pair; scene_manager_next_scene()
// -> record the target scene name; s_cat stays the real int. CATALOG_COUNT is the
// real value (8) from command_catalog.h so the index-collision check below is exact,
// not a stand-in.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CATALOG_COUNT 8
#define IDX_LOG      200
#define IDX_SETTINGS 202
#define IDX_BACK     201

typedef enum { TARGET_NONE, TARGET_LOG, TARGET_SETTINGS, TARGET_CATEGORY, TARGET_SEND_AND_LOG } Target;

typedef struct {
    int s_cat;
    Target last_target;
    int    last_category; // valid when last_target == TARGET_CATEGORY
    int    wire_settings;  // 0 = ALT (pre-patch), 1 = NEU (post-patch)
} Model;

// menu_callback(), post-patch when wire_settings, pre-patch otherwise (ALT simply
// never receives an IDX_SETTINGS event, since build_menu() never emits that item --
// modelled below by the caller only firing it when wire_settings is set).
static void menu_callback(Model* m, int index) {
    if(index == IDX_LOG) { m->last_target = TARGET_LOG; return; }
    if(m->wire_settings && index == IDX_SETTINGS) { m->last_target = TARGET_SETTINGS; return; }
    if(index == IDX_BACK) { m->s_cat = -1; m->last_target = TARGET_NONE; return; }
    if(m->s_cat < 0) {
        if(index < CATALOG_COUNT) { m->s_cat = index; m->last_target = TARGET_CATEGORY; m->last_category = index; }
    } else {
        if(index < CATALOG_COUNT /* stand-in bound for CATALOG[s_cat].count, irrelevant here */) {
            m->last_target = TARGET_SEND_AND_LOG;
        }
    }
}

int main(void) {
    int checks = 0;

    // 1) ALT: top-level submenu never contains IDX_SETTINGS (build_menu() doesn't
    //    add it), so UkfeSceneSettings is provably unreachable through the menu --
    //    matches the "keine Aufrufstelle zu UkfeSceneSettings" grep finding.
    {
        Model m = {.s_cat = -1, .wire_settings = 0};
        // Even if something else dispatched index 202 directly (it can't: no
        // submenu item exists for it), the ALT dispatcher has no IDX_SETTINGS
        // branch at all, so it would fall through to the s_cat<0 range check
        // and be silently ignored (202 >= CATALOG_COUNT).
        menu_callback(&m, IDX_SETTINGS);
        assert(m.last_target == TARGET_NONE);
        checks++;
    }

    // 2) NEU: selecting the new top-level "Einstellungen" entry navigates to
    //    Settings, independent of s_cat, and does not disturb the category cursor.
    {
        Model m = {.s_cat = -1, .wire_settings = 1};
        menu_callback(&m, IDX_SETTINGS);
        assert(m.last_target == TARGET_SETTINGS);
        assert(m.s_cat == -1);
        checks++;
    }

    // 3) NEU: Log entry still navigates to Log, unaffected by the new branch
    //    being inserted before the IDX_BACK check.
    {
        Model m = {.s_cat = -1, .wire_settings = 1};
        menu_callback(&m, IDX_LOG);
        assert(m.last_target == TARGET_LOG);
        checks++;
    }

    // 4) NEU: a normal category selection (index 0..CATALOG_COUNT-1) still drills
    //    down exactly as before -- the new branch only triggers on the literal
    //    IDX_SETTINGS=202 value.
    {
        Model m = {.s_cat = -1, .wire_settings = 1};
        menu_callback(&m, 3 /* "LoRaWAN/TTN" */);
        assert(m.last_target == TARGET_CATEGORY && m.last_category == 3);
        checks++;
    }

    // 5) NEU: "< zurueck" (IDX_BACK) still resets to the category level.
    {
        Model m = {.s_cat = 2, .wire_settings = 1};
        menu_callback(&m, IDX_BACK);
        assert(m.s_cat == -1);
        checks++;
    }

    // 6) Index-collision check: every real submenu id used at the top level
    //    (0..CATALOG_COUNT-1 for categories, IDX_LOG=200, IDX_SETTINGS=202,
    //    IDX_BACK=201) is pairwise distinct -- CATALOG_COUNT=8 leaves no room to
    //    collide with the 200/201/202 sentinels, so the new constant does not
    //    need its own reserved range beyond "greater than CATALOG_COUNT".
    {
        int ids[11];
        int n = 0;
        for(int i = 0; i < CATALOG_COUNT; i++) ids[n++] = i;
        ids[n++] = IDX_LOG;
        ids[n++] = IDX_SETTINGS;
        ids[n++] = IDX_BACK;
        for(int i = 0; i < n; i++)
            for(int j = i + 1; j < n; j++)
                assert(ids[i] != ids[j]);
        checks++;
    }

    printf("%d/%d Pruefungen bestanden\n", checks, checks);
    printf("ALLE CHECKS PASSED\n");
    return 0;
}
