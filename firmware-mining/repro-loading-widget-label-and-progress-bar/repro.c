// Host-Repro fuer den Loading-Widget-Port (loading_set_text/_set_progress/_reset_progress,
// applications/services/gui/modules/loading.c).
//
// Ziel: belegen, dass (a) der neue Code fuer den bisherigen Default-Zustand (kein Text, keine
// Progress-Bar) exakt dieselbe Icon-Position berechnet wie der pristine ALT-Stand, und (b) die
// drei neuen Setter-Funktionen ihre "changed"-Rueckgabe (steuert bei mir mit_view_model() das
// Redraw) korrekt aus dem Modellzustand ableiten. Reine Arithmetik/Zustandslogik, 1:1 aus der
// echten Funktion transkribiert -- kein Canvas/Icon/Furi-Stack noetig, deshalb ohne Hardware
// verifizierbar. Die eigentliche Pixeldarstellung (canvas_draw_icon/elements_progress_bar) ist
// NICHT Teil dieses Repros, siehe Risiko-Abschnitt in der zugehoerigen .md.
//
// Kompilieren: gcc -O0 -g -fsanitize=address,undefined -Wall -Wextra -o repro repro.c && ./repro
// (zusaetzlich mit -O1 wiederholt)

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOADING_ICON_SIZE  24
#define LOADING_BAR_WIDTH  64
#define LOADING_BAR_HEIGHT 9
#define LOADING_BAR_GAP    4
#define LOADING_BAR_BLOCK  (LOADING_ICON_SIZE + LOADING_BAR_GAP + LOADING_BAR_HEIGHT)

// CLAMP(x, upper, lower) == MIN(upper, MAX(x, lower)) -- 1:1 aus furi/core/core_defines.h (bei
// mir unveraendert seit dem mntm-012-Basisstand vorhanden, siehe .md-Quelle-Abschnitt).
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, upper, lower) (MIN(upper, MAX(x, lower)))

// ---- ALT: pristine Berechnung (mntm-012-Basisstand, vor diesem Port) ----
static void alt_default_xy(uint8_t canvas_width, uint8_t canvas_height, uint8_t* x, uint8_t* y) {
    *x = (uint8_t)(canvas_width / 2 - LOADING_ICON_SIZE / 2);
    *y = (uint8_t)(canvas_height / 2 - LOADING_ICON_SIZE / 2);
}

// ---- NEU: Ausschnitt aus loading_draw_callback() (Default-Zweig, kein Label) ----
static void
    neu_default_xy(uint8_t canvas_width, uint8_t canvas_height, bool progress_shown, uint8_t* x, uint8_t* y) {
    *x = (uint8_t)(canvas_width / 2 - LOADING_ICON_SIZE / 2);
    const uint8_t block = progress_shown ? LOADING_BAR_BLOCK : LOADING_ICON_SIZE;
    *y = (uint8_t)(canvas_height / 2 - block / 2);
}

// ---- NEU: Dispatch-Bedingung aus loading_draw_callback() ----
// "!furi_string_empty(model->text)" -- furi_string_empty() ist laut string.h ein reiner
// strlen()==0-Test; hier direkt als strlen(text)==0 auf dem C-String nachgebaut.
static bool neu_takes_label_path(const char* text) {
    return strlen(text) != 0;
}

// ---- Minimalmodell fuer die drei Setter, 1:1 aus LoadingModel plus den drei Funktionskoerpern.
// FuriString-Semantik (furi_string_empty/_equal_str/_set) hier durch die entsprechenden
// C-String-Operationen ersetzt (strlen==0 / strcmp==0 / strcpy-in-heap-Puffer) -- reine
// Bytevergleiche laut m_string/mlib-Wrapper-Doku, keine Naeherung mit abweichender Semantik.
typedef struct {
    char* text; // wie FuriString: niemals NULL, startet als ""
    bool progress_shown;
    float progress;
} Model;

static Model model_alloc(void) {
    Model m;
    m.text = strdup("");
    m.progress_shown = false;
    m.progress = 0.0f;
    return m;
}

static void model_free(Model* m) {
    free(m->text);
}

// ---- NEU: loading_set_progress() ----
static bool neu_set_progress(Model* model, float progress) {
    const float clamped = CLAMP(progress, 1.0f, 0.0f);
    bool changed = !model->progress_shown || (model->progress != clamped);
    model->progress_shown = true;
    model->progress = clamped;
    return changed;
}

// ---- NEU: loading_reset_progress() ----
static bool neu_reset_progress(Model* model) {
    bool changed = model->progress_shown;
    model->progress_shown = false;
    model->progress = 0.0f;
    return changed;
}

// ---- NEU: loading_set_text() ----
static bool neu_set_text(Model* model, const char* text) {
    if(!text) text = "";
    bool changed = (strcmp(model->text, text) != 0);
    free(model->text);
    model->text = strdup(text);
    return changed;
}

int main(void) {
    long checks = 0;

    // 1) Regression: Default-Zustand (progress_shown=false, Text leer) liefert bei JEDER
    //    Canvas-Groesse exakt dieselbe (x,y) wie der pristine ALT-Stand -- die reine
    //    Layout-Erweiterung aendert am unveraenderten Aufrufpfad nichts.
    for(int cw = 0; cw <= 255; cw++) {
        for(int ch = 0; ch <= 255; ch++) {
            uint8_t ax, ay, nx, ny;
            alt_default_xy((uint8_t)cw, (uint8_t)ch, &ax, &ay);
            neu_default_xy((uint8_t)cw, (uint8_t)ch, /*progress_shown=*/false, &nx, &ny);
            assert(ax == nx && ay == ny);
            checks++;
        }
    }
    printf("[OK] Default-Icon-Position ALT==NEU ueber vollen 256x256 Canvas-Groessenraum (%ld Pruefungen)\n",
           checks);

    // 2) Regression: bei leerem Text nimmt der Draw-Callback nie den neuen Label-Zweig.
    assert(neu_takes_label_path("") == false);
    checks++;
    // ... aber bei jedem nicht-leeren Text schon (das ist der beabsichtigte neue Zweig).
    const char* nonempty[] = {"a", "Reading UID", "  ", "\n", "x"};
    for(size_t i = 0; i < sizeof(nonempty) / sizeof(nonempty[0]); i++) {
        assert(neu_takes_label_path(nonempty[i]) == true);
        checks++;
    }
    printf("[OK] Text-leer -> alter Zweig, Text-nichtleer -> neuer Label-Zweig, korrekt getrennt\n");

    // 3) loading_set_progress(): Clamp-Verhalten ueber Regel-, Rand- und Ausreisserwerte.
    {
        struct {
            float in, want;
        } cases[] = {
            {-100.0f, 0.0f}, {-0.0001f, 0.0f}, {0.0f, 0.0f}, {0.3333f, 0.3333f},
            {1.0f, 1.0f},    {1.0001f, 1.0f},  {100.0f, 1.0f},
        };
        for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            Model m = model_alloc();
            neu_set_progress(&m, cases[i].in);
            assert(m.progress == cases[i].want);
            model_free(&m);
            checks++;
        }
    }
    printf("[OK] loading_set_progress() clamped korrekt auf [0.0, 1.0] (Rand-/Ausreisserwerte)\n");

    // 4) loading_set_progress()/loading_reset_progress(): "changed"-Rueckgabe steuert bei mir das
    //    Redraw (with_view_model()-Param) -- muss beim allerersten Aufruf true sein (Uebergang
    //    "keine Bar" -> "Bar"), bei identischem Folgewert false (Kommentar im echten Code: Aufrufer
    //    melden oft denselben Wert erneut, das soll keine neue Zeichnung ausloesen), bei neuem Wert
    //    wieder true, und beim Reset einer bereits sichtbaren Bar true / einer bereits verborgenen
    //    Bar false.
    {
        Model m = model_alloc();
        assert(neu_set_progress(&m, 0.1f) == true); // erster Aufruf: Bar erscheint
        assert(neu_set_progress(&m, 0.1f) == false); // derselbe Wert erneut: kein Redraw
        assert(neu_set_progress(&m, 0.2f) == true); // neuer Wert: Redraw
        assert(neu_reset_progress(&m) == true); // Bar war sichtbar: Redraw
        assert(neu_reset_progress(&m) == false); // Bar schon verborgen: kein Redraw
        assert(m.progress_shown == false && m.progress == 0.0f);
        model_free(&m);
        checks += 5;
    }
    printf("[OK] Redraw-Unterdrueckung bei loading_set_progress()/loading_reset_progress() korrekt\n");

    // 5) loading_set_text(): "changed" nur bei tatsaechlicher Aenderung, NULL wird wie "" behandelt,
    //    mehrfaches Setzen desselben Textes loest kein wiederholtes Redraw aus.
    {
        Model m = model_alloc();
        assert(neu_set_text(&m, "Reading UID") == true);
        assert(strcmp(m.text, "Reading UID") == 0);
        assert(neu_set_text(&m, "Reading UID") == false); // identischer Text: kein Redraw
        assert(neu_set_text(&m, "Writing UID") == true); // anderer Text: Redraw
        assert(neu_set_text(&m, NULL) == true); // NULL -> "": Aenderung, da vorher nicht leer
        assert(strcmp(m.text, "") == 0);
        assert(neu_set_text(&m, NULL) == false); // wieder NULL: bereits leer, kein Redraw
        model_free(&m);
        checks += 6;
    }
    printf("[OK] loading_set_text() Change-Erkennung inkl. NULL-Behandlung korrekt\n");

    printf("\nALLE CHECKS BESTANDEN (%ld Einzelpruefungen)\n", checks);
    return 0;
}
