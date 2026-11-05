// Host-Repro: Back-Button-Navigation in lora-ukfe/scenes/scene_settings.c
// gegen ein 1:1-Modell der Flipper-SceneManager-Stack-Semantik, wie sie bereits
// in anderen Funden dieses Repos (z. B. FlipperJammer
// scene-run-exit-lands-on-duration-picker-fix) nachgebaut wurde:
//
//   scene_manager_handle_back_event(mgr):
//     consumed = current_scene->on_event(ctx, { type = Back })
//     if(!consumed) {
//         // Default-Aktion: eine Ebene vom Navigationsstapel poppen
//         if(stack_size > 1) { pop(); return true; }
//         else                { return false; }   // Stapel leer -> App verlaesst sich
//     }
//     return true;   // Scene hat selbst navigiert (oder bewusst verschluckt)
//
// Reihenfolge (aus lora_ukfe.c): Menu -> Settings waere die geplante Navigation,
// sobald ein Menuepunkt dafuer verdrahtet ist (aktuell noch offen, siehe .md).
// Fuer den Back-Fund selbst ist nur relevant, WIE scene_settings_on_event() auf
// das Back-Event reagiert -- unabhaengig davon, wie der Nutzer dorthin kam.
//
// Zeigt:
//   1) BUG (ALT): scene_settings_on_event() setzt bei Back nur consumed=true,
//      ruft aber nie scene_manager_previous_scene() -- der SceneManager denkt,
//      die Scene habe die Navigation selbst erledigt, tut deshalb nichts, der
//      Stapel bleibt unveraendert. Das Back-Druecken hat sichtbar keine Wirkung.
//   2) Vergleich mit den vier Geschwister-Scenes im selben Repo
//      (scene_status.c, scene_log.c, scene_payload_list.c -- alle drei rufen
//      "scene_manager_previous_scene(app->scene_manager); consumed = true;" --
//      sowie scene_menu.c, das fuer die Kategorie-Ebene denselben Aufruf
//      durchfuehrt): scene_settings.c ist die einzige Scene, die vom
//      etablierten Muster abweicht.
//   3) FIX (NEU): scene_settings_on_event() uebernimmt exakt denselben Aufruf
//      wie die drei Geschwister-Scenes -- Stapel wird korrekt gepoppt.
//
// Kompilieren/Ausfuehren:
//   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0 -o repro repro.c && ./repro
//   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O1 -o repro_o1 repro.c && ./repro_o1

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef enum { EvBack, EvOther } EventType;
typedef struct { EventType type; } SceneManagerEvent;

typedef enum { SceneMenu, SceneSettings, SceneCount } SceneId;
static const char* scene_name(SceneId s) {
    switch(s) {
    case SceneMenu: return "Menu";
    case SceneSettings: return "Settings";
    default: return "?";
    }
}

#define STACK_MAX 8
typedef struct {
    SceneId stack[STACK_MAX];
    int     top; // Index des aktuell sichtbaren Eintrags
} SceneManager;

static void sm_push(SceneManager* m, SceneId s) {
    assert(m->top + 1 < STACK_MAX);
    m->stack[++m->top] = s;
}

// Entspricht scene_manager_previous_scene(): poppt genau eine Ebene.
static bool sm_previous_scene(SceneManager* m) {
    if(m->top <= 0) return false; // bereits an der Wurzel
    m->top--;
    return true;
}

static SceneId sm_current(const SceneManager* m) { return m->stack[m->top]; }

// ---- Die beiden scene_settings_on_event()-Varianten, 1:1 aus dem Patch ----

// ALT: Bug-Zustand (aktueller Code vor diesem Fund)
static bool scene_settings_on_event_OLD(SceneManager* m, SceneManagerEvent ev) {
    (void)m; // entspricht UNUSED(ctx) im Original -- ctx/app->scene_manager wird nie benutzt
    bool consumed = false;
    if(ev.type == EvBack) {
        consumed = true;
    }
    return consumed;
}

// NEU: wie die drei Geschwister-Scenes (scene_status.c/scene_log.c/scene_payload_list.c)
static bool scene_settings_on_event_NEW(SceneManager* m, SceneManagerEvent ev) {
    bool consumed = false;
    if(ev.type == EvBack) {
        sm_previous_scene(m);
        consumed = true;
    }
    return consumed;
}

// Modell des echten scene_manager_handle_back_event(): ruft den Scene-Handler,
// wendet bei consumed==false die Standardaktion (ein Pop) an.
static bool handle_back_event(SceneManager* m, bool (*on_event)(SceneManager*, SceneManagerEvent)) {
    SceneManagerEvent ev = { .type = EvBack };
    bool consumed = on_event(m, ev);
    if(!consumed) {
        return sm_previous_scene(m);
    }
    return true;
}

int main(void) {
    int checks = 0, failed = 0;
#define CHECK(cond) do { checks++; if(!(cond)) { failed++; \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); } } while(0)

    // --- Szenario 1: ALT -- Nutzer ist in Settings (Stapel: Menu, Settings) ---
    {
        SceneManager m = { .stack = {SceneMenu}, .top = 0 };
        sm_push(&m, SceneSettings);
        CHECK(sm_current(&m) == SceneSettings);

        bool handled = handle_back_event(&m, scene_settings_on_event_OLD);
        printf("ALT:  vor Back: %s | nach Back: %s (handled=%d)\n",
               "Settings", scene_name(sm_current(&m)), handled);

        // BUG: Stapel unveraendert -- Back hat keine sichtbare Wirkung, der
        // Nutzer bleibt in Settings gefangen (handled==true verschleiert das
        // zusaetzlich: der SceneManager glaubt, die Navigation sei erledigt).
        CHECK(sm_current(&m) == SceneSettings);
        CHECK(handled == true);

        // Mehrfaches Back-Druecken aendert nichts -- kein Ausweg ueber diese Taste.
        handle_back_event(&m, scene_settings_on_event_OLD);
        handle_back_event(&m, scene_settings_on_event_OLD);
        CHECK(sm_current(&m) == SceneSettings);
    }

    // --- Szenario 2: NEU -- derselbe Ausgangszustand ---
    {
        SceneManager m = { .stack = {SceneMenu}, .top = 0 };
        sm_push(&m, SceneSettings);
        CHECK(sm_current(&m) == SceneSettings);

        bool handled = handle_back_event(&m, scene_settings_on_event_NEW);
        printf("NEU:  vor Back: %s | nach Back: %s (handled=%d)\n",
               "Settings", scene_name(sm_current(&m)), handled);

        // FIX: Stapel korrekt gepoppt, Nutzer landet wieder im Menu -- exakt
        // wie bei scene_status.c/scene_log.c/scene_payload_list.c.
        CHECK(sm_current(&m) == SceneMenu);
        CHECK(handled == true);
    }

    // --- Szenario 3: Geschwister-Scene als Referenzverhalten (kein eigener
    //     Fund hier, nur zum Abgleich -- identische Pop-Semantik wie NEU) ---
    {
        SceneManager m_old = { .stack = {SceneMenu}, .top = 0 };
        SceneManager m_new = { .stack = {SceneMenu}, .top = 0 };
        sm_push(&m_old, SceneSettings);
        sm_push(&m_new, SceneSettings);
        handle_back_event(&m_old, scene_settings_on_event_OLD);
        handle_back_event(&m_new, scene_settings_on_event_NEW);
        // NEU verhaelt sich identisch zu den bereits korrekten Geschwister-Scenes
        // (deren on_event ebenfalls nur "previous_scene(); consumed=true;" tut);
        // ALT weicht als einzige Scene im Repo davon ab.
        CHECK(sm_current(&m_new) == SceneMenu);
        CHECK(sm_current(&m_old) != sm_current(&m_new));
    }

    printf("%d/%d Pruefungen bestanden\n", checks - failed, checks);
    if(failed) { printf("ES GAB FEHLSCHLAEGE\n"); return 1; }
    printf("ALLE CHECKS PASSED\n");
    return 0;
}
