/*
 * Host-Repro: FuriString-Leck in flipper-zero-evil-portal (bigbrodude6119)
 *
 * Nachbildung der realen FuriString-Semantik, so wie sie in
 * G4MEOVER-FW/furi/core/string.c tatsächlich implementiert ist:
 *
 *   FuriString* furi_string_alloc(void) {
 *       FuriString* string = malloc(sizeof(FuriString));
 *       string_init(string->string);
 *       return string;
 *   }
 *   void furi_string_free(FuriString* s) {
 *       string_clear(s->string);   // gibt einen ggf. separat allozierten
 *                                  // Inhaltspuffer frei, sobald der Inhalt
 *                                  // über die kleine Inline-/SSO-Kapazität
 *                                  // des m-string-Backends hinausgeht
 *       free(s);
 *   }
 *
 * Stand-in-Typ unten bildet exakt dieses Verhalten nach: kurze Inhalte
 * bleiben inline im Handle, längere Inhalte bekommen einen separat
 * allozierten Heap-Puffer, den nur furi_string_free() (über string_clear())
 * wieder freigibt. Die zwei geprüften Aufrufstellen
 * (evil_portal_uart.c:71 "setap="+AP-Name, evil_portal_scene_console_output.c:117
 * "sethtml="+komplettes index.html) rufen stattdessen direkt free() auf dem
 * FuriString-Handle auf -- das gibt nur den Handle selbst frei, nicht den
 * separat allozierten Inhaltspuffer.
 *
 * ALT (Original-App): free(out_data)       -> Leck des Inhaltspuffers
 * NEU (Fix):           furi_string_free(out_data) -> kein Leck
 *
 * Bau: gcc -std=c17 -Wall -Wextra -fsanitize=address,leak,undefined \
 *          -fno-sanitize-recover=undefined -O0 repro.c -o repro_o0 && ./repro_o0
 *      (ebenso bei -O1)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Inline-Kapazität bewusst klein gehalten (wie das reale m-string-Backend,
 * das kurze Inhalte ohne Zusatzallokation im Handle selbst hält). */
#define INLINE_CAP 7

typedef struct {
    char inline_buf[INLINE_CAP + 1];
    char *heap_buf;   /* != NULL, sobald der Inhalt INLINE_CAP überschreitet */
    size_t len;
} FuriString;

static FuriString *my_furi_string_alloc(void) {
    FuriString *s = malloc(sizeof(FuriString));
    assert(s);
    s->inline_buf[0] = '\0';
    s->heap_buf = NULL;
    s->len = 0;
    return s;
}

static const char *my_furi_string_get_cstr(const FuriString *s) {
    return s->heap_buf ? s->heap_buf : s->inline_buf;
}

/* Entspricht furi_string_cat_str() (bzw. dem furi_string_cat()-Makro, das
 * bei einem char*-Argument per _Generic auf furi_string_cat_str umleitet). */
static void my_furi_string_cat_str(FuriString *s, const char *add) {
    size_t old_len = s->len;
    size_t add_len = strlen(add);
    size_t new_len = old_len + add_len;

    if (s->heap_buf == NULL && new_len <= INLINE_CAP) {
        memcpy(s->inline_buf + old_len, add, add_len + 1);
    } else {
        char *grown = malloc(new_len + 1);
        assert(grown);
        memcpy(grown, my_furi_string_get_cstr(s), old_len);
        memcpy(grown + old_len, add, add_len + 1);
        free(s->heap_buf); /* beim Wachsen alten Zwischenpuffer ablösen */
        s->heap_buf = grown;
    }
    s->len = new_len;
}

/* Korrektes Pendant zu furi_string_free(): gibt Handle UND separat
 * allozierten Inhaltspuffer frei. */
static void my_furi_string_free_FIXED(FuriString *s) {
    free(s->heap_buf);
    free(s);
}

/* ====================================================================
 * Szenario 1: evil_portal_uart.c, uart_worker(), SET_AP_CMD-Zweig
 *   FuriString *out_data = furi_string_alloc();
 *   furi_string_cat(out_data, "setap=");
 *   furi_string_cat(out_data, (char *)uart->app->ap_name);
 *   ...
 *   free(out_data);              <-- ALT: Bug
 *   free(uart->app->ap_name);    <-- separat allozierter Klartext-Puffer, kein FuriString: korrekt
 * ==================================================================== */
static size_t scenario_set_ap_cmd(int use_fix, const char *ap_name) {
    char *ap_name_buf = strdup(ap_name); /* entspricht storage-gelesenem app->ap_name */

    FuriString *out_data = my_furi_string_alloc();
    my_furi_string_cat_str(out_data, "setap=");
    my_furi_string_cat_str(out_data, ap_name_buf);

    size_t heap_buf_leaked = out_data->heap_buf ? out_data->len + 1 : 0;

    if (use_fix) {
        my_furi_string_free_FIXED(out_data);
    } else {
        free(out_data); /* ALT: Original-Verhalten, lässt heap_buf haengen */
    }
    free(ap_name_buf); /* kein FuriString, dieser free() ist im Original korrekt */

    return use_fix ? 0 : heap_buf_leaked;
}

/* ====================================================================
 * Szenario 2: evil_portal_scene_console_output.c, SET_HTML_CMD-Zweig
 *   FuriString *data = furi_string_alloc();
 *   furi_string_cat(data, "sethtml=");
 *   furi_string_cat(data, (char *)app->index_html);
 *   ...
 *   free(data);              <-- ALT: Bug
 *   free(app->index_html);   <-- separat allozierter Dateipuffer, kein FuriString: korrekt
 * ==================================================================== */
static size_t scenario_set_html_cmd(int use_fix, const char *html) {
    char *index_html_buf = strdup(html); /* entspricht storage-gelesenem app->index_html */

    FuriString *data = my_furi_string_alloc();
    my_furi_string_cat_str(data, "sethtml=");
    my_furi_string_cat_str(data, index_html_buf);

    size_t heap_buf_leaked = data->heap_buf ? data->len + 1 : 0;

    if (use_fix) {
        my_furi_string_free_FIXED(data);
    } else {
        free(data); /* ALT */
    }
    free(index_html_buf);

    return use_fix ? 0 : heap_buf_leaked;
}

int main(int argc, char **argv) {
    const char *ap_name = "Free_Airport_WiFi"; /* realistische SSID, 18 Zeichen */
    const char *html =
        "<html><body><form method=POST action=/login>"
        "<input name=user><input name=pass type=password>"
        "<input type=submit></form></body></html>"; /* 118 Zeichen, realistische Mindestgroesse */

    /* Modus getrennt waehlbar, damit LeakSanitizer ALT und NEU in zwei
     * getrennten Prozessen beurteilt (sonst wuerden NEU-Laeufe im selben
     * Prozess die ALT-Lecks desselben Laufs verschleiern). */
    int use_fix;
    if (argc == 2 && strcmp(argv[1], "fix") == 0) {
        use_fix = 1;
    } else if (argc == 2 && strcmp(argv[1], "alt") == 0) {
        use_fix = 0;
    } else {
        fprintf(stderr, "Usage: %s alt|fix\n", argv[0]);
        return 2;
    }

    /* Regressionsnachweis fuer das Stand-in-Modell selbst (nicht fuer die
     * beiden realen Aufrufstellen, die immer ein Praefix wie "setap="/
     * "sethtml=" voranstellen und die kleine Inline-Kapazitaet dadurch so
     * gut wie immer ueberschreiten): ein isolierter, wirklich kurzer Inhalt
     * ohne Praefix bleibt inline und erzeugt ueberhaupt keinen separat
     * allozierten Puffer -- der Fund greift also gezielt erst, sobald ein
     * Inhalt die Inline-Kapazitaet des echten m-string-Backends
     * ueberschreitet, genau wie bei "setap="+SSID bzw. "sethtml="+HTML. */
    {
        FuriString *s = my_furi_string_alloc();
        my_furi_string_cat_str(s, "AB");
        int stayed_inline = (s->heap_buf == NULL);
        my_furi_string_free_FIXED(s);
        if (!stayed_inline) {
            fprintf(stderr, "REGRESSION: kurzer Inhalt ohne Praefix sollte inline bleiben\n");
            return 1;
        }
        printf("Regression OK: kurzer Inhalt ohne Praefix bleibt inline, kein Heap-Puffer\n");
    }

    /* 50 simulierte "Start Portal"-Zyklen, SET_AP_CMD-Zweig. */
    {
        size_t total_leaked = 0;
        for (int i = 0; i < 50; i++) {
            total_leaked += scenario_set_ap_cmd(use_fix, ap_name);
        }
        printf("SET_AP_CMD (%s): 50 Zyklen \"%s\" -> %zu Byte als geleckt gebucht\n",
               use_fix ? "NEU/Fix" : "ALT", ap_name, total_leaked);
    }

    /* 50 simulierte "Start Portal"-Zyklen, SET_HTML_CMD-Zweig (die
     * eigentliche Captive-Portal-HTML-Seite -- in der Praxis der groessere
     * der beiden Lecks, da index.html typischerweise mehrere hundert Byte
     * bis mehrere KB gross ist). */
    {
        size_t total_leaked = 0;
        for (int i = 0; i < 50; i++) {
            total_leaked += scenario_set_html_cmd(use_fix, html);
        }
        printf("SET_HTML_CMD (%s): 50 Zyklen index.html (%zu Byte) -> %zu Byte als geleckt gebucht\n",
               use_fix ? "NEU/Fix" : "ALT", strlen(html), total_leaked);
    }

    printf("Alle Pruefungen bestanden (LeakSanitizer entscheidet beim Prozessende).\n");
    return 0;
}
