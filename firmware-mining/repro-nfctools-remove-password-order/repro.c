// Host-Repro: Schreibreihenfolge in nfc_tools_scene_remove_password_write.c
//
// Modelliert den NTAG21x-Tag-Zustand (AUTH0-Schutz an/aus, PWD-Seiteninhalt) und die beiden
// Schreibschritte aus nfc_tools_remove_password_write_cb() (CONFIG dann PASSWORD, ALT) gegenueber
// der vorgeschlagenen Reihenfolge (PASSWORD dann CONFIG, NEU). Fuer jede moegliche Abbruchstelle
// (RF-Verbindungsabbruch nach 0, 1 oder 2 erfolgreichen Schreibschritten) wird verglichen, was die
// App dem Benutzer meldet (Erfolg/Fehler) gegen den tatsaechlichen Tag-Endzustand.
//
// Kein NFC-/Flipper-Bezug -- reines Zustandsmodell der beiden Schreibaufrufe und der
// Erfolgs-/Fehlermeldung, 1:1 aus dem gelesenen Quelltext uebernommen.
//
// gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PWD_DEFAULT 0xFFFFFFFFu
#define PWD_SECRET 0xAABBCCDDu /* beliebiger, vom Nutzer zuvor gesetzter Passwort-Wert */

typedef struct {
    bool protection_enabled; /* AUTH0 < 0xFF */
    uint32_t pwd; /* Inhalt der PASSWORD-Seite */
} TagState;

typedef enum { STEP_CONFIG, STEP_PWD } StepKind;

/* Wendet die beiden Schreibschritte in der gegebenen Reihenfolge an; bricht nach genau
   `steps_completed` erfolgreich ausgefuehrten Schritten ab (RF-Fehler danach). Gibt zurueck,
   ob die App am Ende "Erfolg" meldet (nur wenn beide Schritte durchliefen, exakt wie
   ctx->success in nfc_tools_remove_password_write_cb()). */
static bool apply(TagState* tag, const StepKind* order, int steps_completed) {
    for(int i = 0; i < steps_completed; i++) {
        if(order[i] == STEP_CONFIG) {
            tag->protection_enabled = false; /* AUTH0 <- 0xFF */
        } else {
            tag->pwd = PWD_DEFAULT; /* PASSWORD <- FF FF FF FF */
        }
    }
    return steps_completed == 2; /* ctx->success wird erst nach Schritt 2 gesetzt */
}

static void run_case(const char* label, const StepKind* order, int steps_completed) {
    TagState tag = {.protection_enabled = true, .pwd = PWD_SECRET};
    bool reported_success = apply(&tag, order, steps_completed);

    /* Der fuer den Nutzer relevante Sicherheitszustand: kann JEDER (ohne Passwort) den Tag
       jetzt beschreiben? Das ist der Fall, sobald AUTH0 = 0xFF ist, unabhaengig vom
       PASSWORD-Seiteninhalt. */
    bool tag_actually_open = !tag.protection_enabled;

    const char* mismatch = (reported_success != tag_actually_open) ? "  <-- WIDERSPRUCH" : "";
    printf(
        "%-32s | nach %d/2 Schritten | App meldet: %-7s | Tag tatsaechlich offen: %-5s%s\n",
        label,
        steps_completed,
        reported_success ? "Erfolg" : "Fehler",
        tag_actually_open ? "ja" : "nein",
        mismatch);
}

int main(void) {
    const StepKind old_order[2] = {STEP_CONFIG, STEP_PWD}; /* ALT: wie ausgeliefert */
    const StepKind new_order[2] = {STEP_PWD, STEP_CONFIG}; /* NEU: Vorschlag */

    printf("== ALT (CONFIG vor PASSWORD, wie ausgeliefert) ==\n");
    run_case("ALT, Abbruch vor Schritt 1", old_order, 0);
    run_case("ALT, Abbruch nach Schritt 1", old_order, 1);
    run_case("ALT, beide Schritte (Regression)", old_order, 2);

    printf("\n== NEU (PASSWORD vor CONFIG) ==\n");
    run_case("NEU, Abbruch vor Schritt 1", new_order, 0);
    run_case("NEU, Abbruch nach Schritt 1", new_order, 1);
    run_case("NEU, beide Schritte (Regression)", new_order, 2);

    /* Kernaussage: ALT hat genau einen Fall, in dem "App meldet Fehler" und "Tag tatsaechlich
       offen" auseinanderfallen (Abbruch nach Schritt 1) -- NEU hat in keinem der drei Faelle
       einen Widerspruch. */
    TagState alt_tag = {.protection_enabled = true, .pwd = PWD_SECRET};
    bool alt_reported = apply(&alt_tag, old_order, 1);
    bool alt_open = !alt_tag.protection_enabled;

    TagState new_tag = {.protection_enabled = true, .pwd = PWD_SECRET};
    bool new_reported = apply(&new_tag, new_order, 1);
    bool new_open = !new_tag.protection_enabled;

    printf("\nErgebnis:\n");
    printf(
        "  ALT nach Schritt 1: App meldet %s, Tag ist %s -> %s\n",
        alt_reported ? "Erfolg" : "Fehler",
        alt_open ? "offen" : "geschuetzt",
        (alt_reported != alt_open) ? "WIDERSPRUCH bestaetigt" : "kein Widerspruch");
    printf(
        "  NEU nach Schritt 1: App meldet %s, Tag ist %s -> %s\n",
        new_reported ? "Erfolg" : "Fehler",
        new_open ? "offen" : "geschuetzt",
        (new_reported != new_open) ? "WIDERSPRUCH" : "kein Widerspruch (erwartet)");

    if(alt_reported == true || alt_open == false) {
        fprintf(stderr, "unerwartetes ALT-Ergebnis, Modellannahme verletzt\n");
        return 1;
    }
    if(new_reported != new_open) {
        fprintf(stderr, "NEU sollte in diesem Modell nie widerspruechlich sein\n");
        return 1;
    }

    printf("\nBestaetigt: ALT widerspricht sich in 1 von 3 Faellen, NEU in 0 von 3 Faellen.\n");
    return 0;
}
