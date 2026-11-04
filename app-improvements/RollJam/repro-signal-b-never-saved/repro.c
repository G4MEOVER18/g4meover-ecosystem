// Host-Repro fuer app-improvements/RollJam/signal-b-never-saved-fix.md
//
// Teil A: transkribiert die reine Byte-Erzeugungslogik aus
// rolljam_rf_save_signal() (Header-String + "RAW_Data:"-Zeile), 1:1 aus dem
// Patch uebernommen, storage_file_write()/furi_hal_rtc_get_datetime() durch
// einen wachsenden Puffer bzw. ein injiziertes DateTime ersetzt -- fuer den
// Fund irrelevant sind ausschliesslich Storage-/RTC-Hardwarezugriffe selbst,
// nicht die Byte-Erzeugung, die hier Zeile fuer Zeile erhalten bleibt.
// Das Format ist absichtlich identisch zum bereits produktiv genutzten
// rollforge_save_sig() der Schwester-App RollForge (gleiches gepacktes
// Puffer-Layout: bit31=level, bits 0-30=duration in us) -- dieser Host-Repro
// prueft, dass die hierher uebertragene Implementierung exakt dieselben
// Bytes fuer dieselben Eingaben erzeugt wie von Hand berechnet.
//
// Teil B: die neue Aufrufstellen-Logik in ROLLJAM_PHASE2_JAMMING (ALT: immer
// "Signal B still valid" ohne jede Sicherung: Signal B existiert nur im RAM
// dieses App-Laufs. NEU: Speicherversuch bei captured==true, app->signal_b_saved
// spiegelt den tatsaechlichen Rueckgabewert, Statuszeile passend dazu) als
// reine Zustands-/Kontrollfluss-Transkription, Speicherfunktion durch einen
// Stub mit einstellbarem Rueckgabewert ersetzt (fuer den Fund irrelevant ist
// die tatsaechliche Datei-E/A, nicht das Verzweigen auf ihr Ergebnis).
//
// Build: gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_o0
//        gcc -std=c17 -Wall -Wextra -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_o1
// Run:   ./repro_o0 && ./repro_o1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Minimaler Ersatz fuer RollJamSignal (identisches gepacktes Layout)
// ---------------------------------------------------------------------------
#define ROLLJAM_SIGNAL_BUF_SIZE 2048U

typedef struct {
    uint32_t buf[ROLLJAM_SIGNAL_BUF_SIZE];
    size_t   count;
    bool     ready;
} SigStub;

typedef struct {
    unsigned hour, minute, second;
} DateTimeStub;

// Wachsender Puffer als Ersatz fuer storage_file_write()/storage_file_open().
typedef struct {
    char*  data;
    size_t len;
    size_t cap;
} ByteBuf;

static void bb_init(ByteBuf* b) {
    b->cap  = 256;
    b->data = malloc(b->cap);
    b->len  = 0;
    b->data[0] = '\0';
}
static void bb_free(ByteBuf* b) { free(b->data); b->data = NULL; }
static void bb_write(ByteBuf* b, const char* s, size_t n) {
    if(b->len + n + 1 > b->cap) {
        while(b->len + n + 1 > b->cap) b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

// ---------------------------------------------------------------------------
// Teil A: transkribierte Byte-Erzeugung aus rolljam_rf_save_signal()
// (Patch: app-improvements/RollJam/signal-b-never-saved-fix/rolljam_rf_c.patch)
// Rueckgabe: true bei Erfolg (sig!=NULL && count>0), Inhalt landet in *out.
// dt/path_out nur zur zusaetzlichen Pruefung des Dateinamens-Formats.
// ---------------------------------------------------------------------------
static bool save_signal_new(
    const SigStub* sig,
    uint32_t       frequency,
    const char*    filename_hint,
    DateTimeStub   dt,
    ByteBuf*       out,
    char*          path_out,
    size_t         path_out_sz) {
    if(!sig || sig->count == 0) return false;

    snprintf(path_out, path_out_sz, "%s/%s_%02u%02u%02u.sub",
        "/ext/subghz/rolljam",
        filename_hint ? filename_hint : "capture",
        dt.hour, dt.minute, dt.second);

    bb_init(out);

    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "Filetype: Flipper SubGhz RAW File\n"
        "Version: 1\n"
        "Frequency: %lu\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n"
        "RAW_Data: ",
        (unsigned long)frequency);
    bb_write(out, hdr, (size_t)n);

    char buf[24];
    size_t count = sig->count;
    for(size_t i = 0; i < count; i++) {
        uint32_t packed = sig->buf[i];
        bool     level  = (packed & 0x80000000UL) != 0;
        int32_t  dur    = (int32_t)(packed & 0x7FFFFFFFUL);
        if(!level) dur = -dur;
        n = snprintf(buf, sizeof(buf), (i == 0) ? "%ld" : " %ld", (long)dur);
        bb_write(out, buf, (size_t)n);
    }
    bb_write(out, "\n", 1);
    return true;
}

// ALT: es gibt in der unveraenderten App ueberhaupt keine Speicherfunktion --
// Signal B existiert ausschliesslich im RAM von app->signal_b. Als Stub hier
// bewusst eine Funktion, die IMMER false liefert (kein Pfad existiert, der
// je true liefern koennte), um Teil B unten symmetrisch gegen ALT/NEU zu
// pruefen.
static bool save_signal_old_never_exists(const SigStub* sig, uint32_t f, const char* hint) {
    (void)sig; (void)f; (void)hint;
    return false; // es gibt in rolljam_rf.c/.h schlicht keine solche Funktion
}

static int failures = 0;
static void check(bool cond, const char* label) {
    printf("%-78s %s\n", label, cond ? "OK" : "FAIL");
    if(!cond) failures++;
}

// ---------------------------------------------------------------------------
// Teil A Tests: Byte-Erzeugung
// ---------------------------------------------------------------------------
static void test_part_a(void) {
    printf("== Teil A: rolljam_rf_save_signal() Byte-Erzeugung ==\n");

    // A1: leeres Signal wird abgelehnt (count==0), kein Inhalt erzeugt.
    {
        SigStub sig = {.count = 0, .ready = false};
        ByteBuf out; char path[128];
        bool ok = save_signal_new(&sig, 433920000UL, "signal_b", (DateTimeStub){1,2,3}, &out, path, sizeof(path));
        check(!ok, "A1: count==0 -> Ablehnung (kein Schreibversuch)");
    }

    // A2: NULL-Signal wird abgelehnt.
    {
        ByteBuf out; char path[128];
        bool ok = save_signal_new(NULL, 433920000UL, "signal_b", (DateTimeStub){0,0,0}, &out, path, sizeof(path));
        check(!ok, "A2: sig==NULL -> Ablehnung");
    }

    // A3: vier Flanken (HIGH 350, LOW 300, HIGH 700, LOW 650) -> exakt
    //     erwarteter Header + RAW_Data-String, Dateiname-Format.
    {
        SigStub sig = {.count = 4, .ready = true};
        sig.buf[0] = 0x80000000UL | 350u;
        sig.buf[1] = 300u;              // level=0 -> LOW
        sig.buf[2] = 0x80000000UL | 700u;
        sig.buf[3] = 650u;

        ByteBuf out; char path[128];
        bool ok = save_signal_new(&sig, 433920000UL, "signal_b", (DateTimeStub){14,7,9}, &out, path, sizeof(path));

        const char* expected =
            "Filetype: Flipper SubGhz RAW File\n"
            "Version: 1\n"
            "Frequency: 433920000\n"
            "Preset: FuriHalSubGhzPresetOok650Async\n"
            "Protocol: RAW\n"
            "RAW_Data: 350 -300 700 -650\n";

        check(ok, "A3: vier Flanken -> Speicherung akzeptiert");
        check(strcmp(out.data, expected) == 0, "A3: RAW_Data-Zeile exakt wie erwartet (Vorzeichen=Level)");
        check(strcmp(path, "/ext/subghz/rolljam/signal_b_140709.sub") == 0,
              "A3: Dateiname-Format \"<dir>/<hint>_HHMMSS.sub\" exakt");
        bb_free(&out);
    }

    // A4: ein einzelner Eintrag an der 31-Bit-Maskierungsgrenze
    //     (0x7FFFFFFF, HIGH) -- darf nicht in UB/Ueberlauf laufen (passt
    //     exakt in int32_t, groesster erlaubter Wert).
    {
        SigStub sig = {.count = 1, .ready = true};
        sig.buf[0] = 0x80000000UL | 0x7FFFFFFFUL;
        ByteBuf out; char path[128];
        bool ok = save_signal_new(&sig, 868350000UL, "signal_b", (DateTimeStub){23,59,59}, &out, path, sizeof(path));
        check(ok, "A4: Grenzwert 0x7FFFFFFF akzeptiert");
        check(strstr(out.data, "RAW_Data: 2147483647\n") != NULL,
              "A4: Grenzwert korrekt als 2147483647 formatiert (kein UB/Overflow)");
        bb_free(&out);
    }

    // A5: volle Puffergroesse (2048 Eintraege) -- Schleife muss exakt
    //     ROLLJAM_SIGNAL_BUF_SIZE Werte emittieren, kein Abschneiden.
    {
        SigStub sig; sig.count = ROLLJAM_SIGNAL_BUF_SIZE; sig.ready = true;
        for(size_t i = 0; i < ROLLJAM_SIGNAL_BUF_SIZE; i++) {
            sig.buf[i] = (i % 2 == 0) ? (0x80000000UL | 100u) : 100u;
        }
        ByteBuf out; char path[128];
        bool ok = save_signal_new(&sig, 433920000UL, "signal_b", (DateTimeStub){0,0,0}, &out, path, sizeof(path));
        check(ok, "A5: volles 2048-Element-Signal akzeptiert");

        size_t tokens = 0;
        const char* p = strstr(out.data, "RAW_Data: ");
        check(p != NULL, "A5: RAW_Data-Marker vorhanden");
        if(p) {
            p += strlen("RAW_Data: ");
            while(*p && *p != '\n') { if(*p == ' ') tokens++; p++; }
            tokens += 1; // n Werte -> n-1 Trenner, +1 fuer den ersten Wert
        }
        check(tokens == ROLLJAM_SIGNAL_BUF_SIZE,
              "A5: genau 2048 RAW-Werte in der Ausgabe (kein Abschneiden/Ueberzaehlen)");
        bb_free(&out);
    }
}

// ---------------------------------------------------------------------------
// Teil B: Aufrufstellen-Logik in ROLLJAM_PHASE2_JAMMING
// ---------------------------------------------------------------------------
typedef struct {
    bool signal_b_saved;
    char status[48];
} AppStub;

// ALT: 1:1 aus dem unveraenderten rolljam_app.c (Zeilen vor diesem Patch) --
// es existiert keine Speicherfunktion, "B skipped" nur bei !captured, in
// JEDEM anderen Fall (captured==true) bleibt die DONE-Behauptung
// "Signal B still valid" unconditional stehen (hier als Statuswert direkt
// nachgebildet, da der DONE-Bildschirm selbst keine Variable abfragt).
static void phase2_old(bool captured, AppStub* app) {
    if(!captured) {
        snprintf(app->status, sizeof(app->status), "B skipped - replaying A");
    }
    // sonst: nichts. app->signal_b_saved existiert in ALT nicht einmal als
    // Feld -- hier zur Vergleichbarkeit konstant false gehalten, DAMIT
    // sichtbar wird, dass "still valid" in ALT unabhaengig vom tatsaechlichen
    // (nicht vorhandenen) Speicherzustand behauptet wuerde.
    app->signal_b_saved = false;
}

// NEU: 1:1 aus dem Patch (rolljam_app.c, ROLLJAM_PHASE2_JAMMING-Fall).
// save_fn als Stub fuer rolljam_rf_save_signal().
static void phase2_new(bool captured, bool save_fn_result, AppStub* app) {
    if(!captured) {
        app->signal_b_saved = false;
        snprintf(app->status, sizeof(app->status), "B skipped - replaying A");
    } else {
        app->signal_b_saved = save_fn_result; // == Rueckgabewert von save_signal_new()
        snprintf(
            app->status,
            sizeof(app->status),
            app->signal_b_saved ? "B saved - replaying A" : "B save FAILED - replaying A");
    }
}

static void test_part_b(void) {
    printf("\n== Teil B: ROLLJAM_PHASE2_JAMMING Aufrufstellen-Logik ==\n");

    // B1: captured==true, Speicherung gelingt (Normalfall auf SD-Karte mit Platz).
    {
        AppStub old_app = {0}, new_app = {0};
        phase2_old(true, &old_app);
        phase2_new(true, true, &new_app);
        check(old_app.signal_b_saved == false,
              "B1: ALT -- 'signal_b_saved' existiert nicht einmal, DONE behauptet trotzdem 'still valid'");
        check(new_app.signal_b_saved == true && strcmp(new_app.status, "B saved - replaying A") == 0,
              "B1: NEU -- captured+gespeichert -> signal_b_saved=true, Status spiegelt Erfolg");
    }

    // B2: captured==true, Speicherung schlaegt fehl (z.B. SD voll/entfernt) --
    //     DER FUND: in ALT gibt es diesen Fall gar nicht (keine Speicherung
    //     versucht), die DONE-Behauptung "still valid" waere trotzdem falsch,
    //     weil Signal B nie auf SD landet und mit App-Ende verloren ist.
    {
        AppStub old_app = {0}, new_app = {0};
        bool old_saved_ever_possible = save_signal_old_never_exists((void*)1, 433920000UL, "signal_b");
        phase2_old(true, &old_app);
        phase2_new(true, false, &new_app);
        check(old_saved_ever_possible == false,
              "B2: ALT -- es gibt ueberhaupt keinen Codepfad, der Signal B je persistiert");
        check(new_app.signal_b_saved == false && strcmp(new_app.status, "B save FAILED - replaying A") == 0,
              "B2: NEU -- Speicherung fehlgeschlagen -> signal_b_saved=false, Status warnt statt zu behaupten");
    }

    // B3: Regression -- captured==false (Phase 2 timeout) bleibt in beiden
    //     Versionen gleich: kein Speicherversuch, "B skipped" Status.
    {
        AppStub old_app = {0}, new_app = {0};
        phase2_old(false, &old_app);
        phase2_new(false, true /* darf nicht aufgerufen werden, s.u. */, &new_app);
        check(strcmp(old_app.status, "B skipped - replaying A") == 0 &&
              strcmp(new_app.status, "B skipped - replaying A") == 0,
              "B3: Regression -- !captured -> beide Versionen identischer 'skip'-Status");
        check(new_app.signal_b_saved == false,
              "B3: Regression -- !captured -> signal_b_saved bleibt false (kein Speicherversuch auf leeres Signal)");
    }
}

int main(void) {
    test_part_a();
    test_part_b();
    printf("\n%s (%d Fehlschlaege)\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
