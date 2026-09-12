// Host-Repro: physische Back-Taste beendet in 7 von 9 nicht-Wurzel-Szenen der UHF-RFID-App die
// gesamte Anwendung statt eine Ebene zurueckzunavigieren.
//
// Modelliert exakt das SceneManagerEvent-Feldpaar {type, event} aus scene_manager.h
// (SceneManagerEventTypeCustom=0, SceneManagerEventTypeBack=1, SceneManagerEventTypeTick=2) sowie
// GuiButtonType (Left=0, Center=1, Right=2) und die projektspezifischen UHFCustomEvent-Werte
// (>= 100) 1:1 aus den gelesenen Quelldateien. Jede der sieben betroffenen on_event()-Funktionen
// wird als ALT (wortgetreu aus dem Quelltext) und NEU (vorgeschlagener Fix) nachgebaut; alle
// auswirkenden Aufrufe (scene_manager_previous_scene(), _search_and_switch_to_previous_scene(),
// _next_scene(), Worker-State-Zuweisung) werden statt echter Seiteneffekte protokolliert.
//
// Kein NFC-/Flipper-Bezug -- reines Kontrollfluss-/Zustandsmodell der on_event()-Dispatcher, ohne
// Speicherzugriffe; unter ASan/UBSan nur zur Konsistenzpruefung mitgebaut.
//
// gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- 1:1 aus scene_manager.h / widget_element.h / uhf_app_i.h ---- */
enum { SceneManagerEventTypeCustom = 0, SceneManagerEventTypeBack = 1, SceneManagerEventTypeTick = 2 };
enum { GuiButtonTypeLeft = 0, GuiButtonTypeCenter = 1, GuiButtonTypeRight = 2 };
enum {
    UHFCustomEventReserved = 100,
    UHFCustomEventVerifyDone,
    UHFCustomEventViewExit,
    UHFCustomEventWorkerExit,
};

typedef struct {
    int type;
    uint32_t event;
} SceneManagerEvent;

/* Protokollierte Wirkung statt echtem Seiteneffekt */
typedef enum {
    ACT_NONE, /* kein Zweig getroffen, reines Rueckfallverhalten */
    ACT_WORKER_STATE_STOP, /* uhf_app->worker->state = UHFWorkerStateStop (ALT, wirkungslos s.u.) */
    ACT_PREV_SCENE, /* scene_manager_previous_scene() */
    ACT_SEARCH_PREV_START, /* scene_manager_search_and_switch_to_previous_scene(..., UHFSceneStart) */
    ACT_NEXT_TAGMENU, /* scene_manager_next_scene(..., UHFSceneTagMenu) */
} Action;

typedef struct {
    bool consumed;
    Action action;
} Result;

/* ============================== uhf_scene_read_tag.c ============================== */
static Result read_tag_alt(SceneManagerEvent e) {
    Result r = {false, ACT_NONE};
    if(e.event == UHFCustomEventWorkerExit) {
        r.action = ACT_NEXT_TAGMENU; /* eigtl. UHFSceneReadTagSuccess, hier nur als "Uebergang" markiert */
        r.consumed = true;
    }
    return r;
}
static Result read_tag_neu(SceneManagerEvent e) {
    Result r = read_tag_alt(e);
    if(!r.consumed && e.type == SceneManagerEventTypeBack) {
        r.action = ACT_PREV_SCENE;
        r.consumed = true;
    }
    return r;
}

/* ============================== uhf_scene_write_tag.c ============================= */
/* Strukturell identisch zu read_tag.c (eigene Kopie im Quelltext) */
static Result write_tag_alt(SceneManagerEvent e) {
    return read_tag_alt(e);
}
static Result write_tag_neu(SceneManagerEvent e) {
    return read_tag_neu(e);
}

/* ========================= uhf_scene_read_tag_success.c =========================== */
static Result read_tag_success_alt(SceneManagerEvent e) {
    Result r = {false, ACT_NONE};
    if(e.event == SceneManagerEventTypeBack) { /* BUG: vergleicht event.event statt event.type */
        r.action = ACT_WORKER_STATE_STOP;
        /* kein consumed = true; im Original */
    }
    if(e.type == SceneManagerEventTypeCustom) {
        if(e.event == GuiButtonTypeLeft) {
            r.action = ACT_SEARCH_PREV_START;
            r.consumed = true;
        } else if(e.event == GuiButtonTypeRight) {
            r.action = ACT_NEXT_TAGMENU;
            r.consumed = true;
        }
        /* GuiButtonTypeCenter: auskommentiert im Original, kein Effekt */
    }
    return r;
}
static Result read_tag_success_neu(SceneManagerEvent e) {
    Result r = {false, ACT_NONE};
    if(e.type == SceneManagerEventTypeCustom) {
        if(e.event == GuiButtonTypeLeft) {
            r.action = ACT_SEARCH_PREV_START;
            r.consumed = true;
        } else if(e.event == GuiButtonTypeRight) {
            r.action = ACT_NEXT_TAGMENU;
            r.consumed = true;
        }
    } else if(e.type == SceneManagerEventTypeBack) { /* FIX: richtiges Feld, spiegelt Left-Taste */
        r.action = ACT_SEARCH_PREV_START;
        r.consumed = true;
    }
    return r;
}

/* ========================= uhf_scene_write_tag_success.c =========================== */
/* Strukturell identisch zu read_tag_success.c (eigene Kopie im Quelltext) */
static Result write_tag_success_alt(SceneManagerEvent e) {
    return read_tag_success_alt(e);
}
static Result write_tag_success_neu(SceneManagerEvent e) {
    return read_tag_success_neu(e);
}

/* ============================== uhf_scene_delete.c ================================= */
static Result delete_alt(SceneManagerEvent e) {
    Result r = {false, ACT_NONE};
    if(e.type == SceneManagerEventTypeCustom) {
        if(e.event == GuiButtonTypeLeft) {
            r.action = ACT_PREV_SCENE;
            r.consumed = true; /* Original: return scene_manager_previous_scene(...); */
        } else if(e.event == GuiButtonTypeRight) {
            r.consumed = true; /* Delete-Ausfuehrung, hier ohne Belang fuer Back-Frage */
        }
    }
    return r;
}
static Result delete_neu(SceneManagerEvent e) {
    Result r = delete_alt(e);
    if(!r.consumed && e.type == SceneManagerEventTypeBack) { /* FIX: spiegelt Left="Back" */
        r.action = ACT_PREV_SCENE;
        r.consumed = true;
    }
    return r;
}

/* ============================== uhf_scene_saved_menu.c ============================= */
enum { SubmenuIndexDelete = 0, SubmenuIndexInfo, SubmenuIndexWrite };
static Result saved_menu_alt(SceneManagerEvent e) {
    Result r = {false, ACT_NONE};
    if(e.type == SceneManagerEventTypeCustom) {
        if(e.event == SubmenuIndexDelete || e.event == SubmenuIndexInfo ||
           e.event == SubmenuIndexWrite) {
            r.consumed = true; /* jeweils next_scene(...), fuer Back-Frage ohne Belang */
        }
    }
    return r;
}
static Result saved_menu_neu(SceneManagerEvent e) {
    Result r = saved_menu_alt(e);
    if(!r.consumed && e.type == SceneManagerEventTypeBack) {
        /* FIX: analog zum SaveSuccess-Fallback in derselben App -- ueberspringt FileSelect, das
           sonst beim einfachen previous_scene() sofort den Dateibrowser erneut oeffnen wuerde */
        r.action = ACT_SEARCH_PREV_START;
        r.consumed = true;
    }
    return r;
}

/* ============================== uhf_scene_settings.c ================================ */
static Result settings_alt(SceneManagerEvent e) {
    (void)e;
    Result r = {false, ACT_NONE}; /* Original: UNUSED(event); return false; -- immer */
    return r;
}
static Result settings_neu(SceneManagerEvent e) {
    Result r = {false, ACT_NONE};
    if(e.type == SceneManagerEventTypeBack) {
        r.action = ACT_SEARCH_PREV_START; /* FIX: analog zu tag_menu.c im selben App-Bestand */
        r.consumed = true;
    }
    return r;
}

/* =========================== Test-Infrastruktur =========================== */
typedef Result (*Handler)(SceneManagerEvent);

typedef struct {
    const char* scene_name;
    Handler alt;
    Handler neu;
} SceneUnderTest;

static const char* action_name(Action a) {
    switch(a) {
    case ACT_NONE: return "NONE";
    case ACT_WORKER_STATE_STOP: return "WORKER_STATE_STOP(wirkungslos)";
    case ACT_PREV_SCENE: return "PREV_SCENE";
    case ACT_SEARCH_PREV_START: return "SEARCH_PREV(Start)";
    case ACT_NEXT_TAGMENU: return "NEXT(TagMenu)";
    }
    return "?";
}

static int g_failures = 0;

static void check(bool cond, const char* label) {
    if(!cond) {
        printf("  [FAIL] %s\n", label);
        g_failures++;
    }
}

int main(void) {
    const SceneUnderTest scenes[] = {
        {"read_tag", read_tag_alt, read_tag_neu},
        {"write_tag", write_tag_alt, write_tag_neu},
        {"read_tag_success", read_tag_success_alt, read_tag_success_neu},
        {"write_tag_success", write_tag_success_alt, write_tag_success_neu},
        {"delete", delete_alt, delete_neu},
        {"saved_menu", saved_menu_alt, saved_menu_neu},
        {"settings", settings_alt, settings_neu},
    };
    const size_t n_scenes = sizeof(scenes) / sizeof(scenes[0]);

    const SceneManagerEvent real_back = {SceneManagerEventTypeBack, 0};

    printf("=== Szenario A: echtes physisches Back (type=Back, event=0) ===\n");
    for(size_t i = 0; i < n_scenes; i++) {
        Result alt = scenes[i].alt(real_back);
        Result neu = scenes[i].neu(real_back);
        printf(
            "%-18s ALT: consumed=%-5s action=%-32s | NEU: consumed=%-5s action=%s\n",
            scenes[i].scene_name,
            alt.consumed ? "true" : "false",
            action_name(alt.action),
            neu.consumed ? "true" : "false",
            action_name(neu.action));
        check(!alt.consumed, "ALT haette bereits konsumieren duerfen (Annahme verletzt)");
        check(neu.consumed, "NEU konsumiert das echte Back-Event nicht");
    }
    printf(
        "-> Bei allen %zu Szenen: ALT liefert consumed=false fuer ein echtes Back-Event. Das "
        "bedeutet konkret (view_dispatcher.c: navigation_event_callback liefert false -> "
        "view_dispatcher_stop()): die physische Back-Taste beendet in jeder dieser Szenen die "
        "GESAMTE App, obwohl fuenf von ihnen (delete/read_tag_success/write_tag_success/settings/"
        "saved_menu) von Start aus per Vorwaertsnavigation erreicht wurden und mindestens eine "
        "Ebene zum Zurueckgehen haben.\n\n",
        n_scenes);

    printf("=== Szenario B: Regressionscheck ueber alle bereits gueltigen Events ===\n");
    const SceneManagerEvent regression_events[] = {
        {SceneManagerEventTypeCustom, GuiButtonTypeLeft},
        {SceneManagerEventTypeCustom, GuiButtonTypeCenter},
        {SceneManagerEventTypeCustom, GuiButtonTypeRight},
        {SceneManagerEventTypeCustom, SubmenuIndexDelete},
        {SceneManagerEventTypeCustom, SubmenuIndexInfo},
        {SceneManagerEventTypeCustom, SubmenuIndexWrite},
        {SceneManagerEventTypeCustom, UHFCustomEventWorkerExit},
        {SceneManagerEventTypeCustom, UHFCustomEventVerifyDone},
        {SceneManagerEventTypeCustom, UHFCustomEventViewExit},
        {SceneManagerEventTypeTick, 0},
    };
    const size_t n_events = sizeof(regression_events) / sizeof(regression_events[0]);
    int regression_mismatches = 0;
    int known_dead_branch_diffs = 0;
    for(size_t i = 0; i < n_scenes; i++) {
        for(size_t j = 0; j < n_events; j++) {
            Result alt = scenes[i].alt(regression_events[j]);
            Result neu = scenes[i].neu(regression_events[j]);
            /* Nach aussen sichtbar (view_dispatcher.c) ist ausschliesslich `consumed` -- der von
               ALT protokollierte ACT_WORKER_STATE_STOP-Zweig ist wie im Hauptdokument belegt
               wirkungslos (verify_module_connected()/setup_and_send_rx() pollen den Worker-State
               nie), daher zaehlt eine reine Action-Abweichung ohne consumed-Unterschied nicht als
               Regression -- betrifft hier ausschliesslich den bereits im Original
               auskommentierten GuiButtonTypeCenter-Zweig von read/write_tag_success. */
            if(alt.consumed != neu.consumed) {
                regression_mismatches++;
                printf(
                    "  [REGRESSION] %s bei event{type=%d,event=%u}: ALT(consumed=%d,%s) != "
                    "NEU(consumed=%d,%s)\n",
                    scenes[i].scene_name,
                    regression_events[j].type,
                    regression_events[j].event,
                    alt.consumed,
                    action_name(alt.action),
                    neu.consumed,
                    action_name(neu.action));
            } else if(alt.action != neu.action) {
                known_dead_branch_diffs++;
            }
        }
    }
    check(regression_mismatches == 0, "NEU veraendert das nach aussen sichtbare consumed-Ergebnis fuer ein bereits gueltiges Event");
    printf(
        "-> %zu Szenen x %zu bereits gueltige Ereignisse geprueft: 0 consumed-Abweichungen, %d "
        "reine Action-Unterschiede (ausschliesslich der bereits im Original auskommentierte, nie "
        "ueber einen echten Knopf erreichbare GuiButtonTypeCenter-Zweig -- ALT fuehrt dort "
        "zusaetzlich die belegt wirkungslose Worker-State-Zuweisung aus). Der Fix fuegt sonst "
        "ausschliesslich einen neuen, bisher unerreichbaren Zweig hinzu (SceneManagerEventTypeBack "
        "kollidiert mit keinem tatsaechlich benutzten Custom-Event-Code dieser Szenen).\n\n",
        n_scenes,
        n_events,
        known_dead_branch_diffs);

    printf("=== Szenario C: NEU spiegelt fuer read/write_tag_success exakt die eigene Exit-Taste ===\n");
    {
        SceneManagerEvent left = {SceneManagerEventTypeCustom, GuiButtonTypeLeft};
        Result via_exit_button = read_tag_success_neu(left);
        Result via_back = read_tag_success_neu(real_back);
        check(
            via_exit_button.action == via_back.action,
            "read_tag_success: Back-Ergebnis weicht von der eigenen Exit-Taste ab");
        via_exit_button = write_tag_success_neu(left);
        via_back = write_tag_success_neu(real_back);
        check(
            via_exit_button.action == via_back.action,
            "write_tag_success: Back-Ergebnis weicht von der eigenen Exit-Taste ab");
        SceneManagerEvent delete_left = {SceneManagerEventTypeCustom, GuiButtonTypeLeft};
        via_exit_button = delete_neu(delete_left);
        via_back = delete_neu(real_back);
        check(
            via_exit_button.action == via_back.action,
            "delete: Back-Ergebnis weicht von der eigenen Back-Taste ab");
    }
    printf(
        "-> Fuer alle drei Szenen mit eigenem Abbrechen-Button liefert die physische Back-Taste "
        "nach dem Fix bitgenau dieselbe Aktion wie der bereits vorhandene Bildschirm-Button.\n\n");

    if(g_failures == 0) {
        printf("ALLE PRUEFUNGEN BESTANDEN (0 Fehlschlaege)\n");
        return 0;
    } else {
        printf("%d PRUEFUNG(EN) FEHLGESCHLAGEN\n", g_failures);
        return 1;
    }
}
