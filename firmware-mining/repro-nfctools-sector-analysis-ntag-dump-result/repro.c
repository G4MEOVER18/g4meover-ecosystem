// Host repro: nfc_tools_sector_worker() discards the bool return value of
// nfc_tools_ntag_dump() for the MF Ultralight / NTAG branch of "Sector
// Analysis", unlike every other branch in the same switch (ICODE, DESFire,
// FeliCa), which all check their dump/read helper's result and short-circuit
// to the error path on failure.
//
// scenes/other/nfc_tools_scene_sector_analysis.c, nfc_tools_sector_worker():
//
//   } else if(app->detected_protocol == NfcProtocolMfUltralight) {
//       nfc_tools_ntag_dump(app);                    // <-- bool return value discarded
//
//   } else if(app->detected_protocol == NfcProtocolIso15693_3 || ...) {
//       bool ok = nfc_tools_icode_dump(app);          // <-- checked
//       if(!ok) {
//           notification_message(app->notifications, &sequence_error);
//           view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
//           return 0;
//       }
//   ... (DESFire/FeliCa poller paths check ctx.success the same way)
//
//   notification_message(app->notifications, &sequence_success); // unconditional after the if/else chain
//   view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
//
// core/nfc_tools_ntag.c, nfc_tools_ntag_dump(): on a hard read failure
// (mf_ultralight_poller_sync_read_card() != MfUltralightErrorNone -- tag
// pulled from the field mid-read, or a communication error) it appends
// "Read error\ncode: N" to app->info_str, leaves app->ndef_str untouched
// (still empty from the furi_string_reset() at the top of the worker), and
// returns false *before* the page loop -- no page data, no pages_read/total
// summary line.
//
// Because the caller never looks at that false, the worker falls through to
// the unconditional success branch: it plays the SUCCESS notification
// (sound+LED) instead of &sequence_error, and still dispatches
// NfcToolsEventAnalysisDone as if the analysis had completed normally. The
// scene's on_event handler then picks the display path by
// furi_string_size(app->ndef_str) > 0 -- since ndef_str is empty in the
// failure case, it shows the plain TextBox with info_str's "Read error..."
// text, so the *visible* text is technically not wrong, but the audible/
// haptic feedback the user gets while approaching the tag contradicts the
// on-screen result, exactly the fault class already documented for the
// FeliCa Write / second-wait-timeout / Remove-Password-order finds in this
// same app.
//
// This model reproduces only the two functions' externally observable
// decisions (which notification sequence is played, whether
// NfcToolsEventAnalysisDone still fires, and what ends up in
// info_str/ndef_str) -- not the Flipper NFC/notification APIs themselves.
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef enum { NOTIF_NONE, NOTIF_SUCCESS, NOTIF_ERROR } Notif;

typedef struct {
    char info[256];
    char ndef[256];
} Strings;

typedef enum {
    ScenarioReadOk,    // mf_ultralight_poller_sync_read_card() succeeds
    ScenarioReadFails, // tag pulled mid-read / comms error
} Scenario;

// Models core/nfc_tools_ntag.c :: nfc_tools_ntag_dump() (unchanged by the fix
// -- only its caller changes). Returns the real function's bool result.
static bool ntag_dump(Scenario s, Strings* out) {
    if(s == ScenarioReadFails) {
        snprintf(out->info, sizeof(out->info), "Read error\ncode: 3\n");
        // out->ndef left empty, matching furi_string_reset() at worker start.
        return false;
    }
    snprintf(out->info, sizeof(out->info), "16/16 pages (4B)\n\n[00] 04 12 34 56\n...\n");
    snprintf(out->ndef, sizeof(out->ndef), "16/16 pages - ASCII\n\n[00] .4VE\n...\n");
    return true;
}

// ALT: as shipped -- nfc_tools_sector_worker() ignores ntag_dump()'s result.
static void worker_alt(Scenario s, Notif* notif_out, bool* analysis_done_out, Strings* out) {
    memset(out, 0, sizeof(*out));
    ntag_dump(s, out); // return value discarded, exactly as in the shipped file
    *notif_out = NOTIF_SUCCESS;
    *analysis_done_out = true;
}

// NEU: check the result exactly like the sibling ICODE branch already does.
static void worker_fixed(Scenario s, Notif* notif_out, bool* analysis_done_out, Strings* out) {
    memset(out, 0, sizeof(*out));
    bool ok = ntag_dump(s, out);
    if(!ok) {
        *notif_out = NOTIF_ERROR;
        *analysis_done_out = true; // scene still needs the event to leave the "Analyzing..." popup
        return;
    }
    *notif_out = NOTIF_SUCCESS;
    *analysis_done_out = true;
}

static const char* notif_name(Notif n) {
    switch(n) {
    case NOTIF_SUCCESS: return "success";
    case NOTIF_ERROR:   return "error";
    default:            return "none";
    }
}

int main(void) {
    struct {
        const char* name;
        Scenario    s;
        Notif       expect_alt;
        Notif       expect_fixed;
    } cases[] = {
        {"Read succeeds (regression)", ScenarioReadOk,    NOTIF_SUCCESS, NOTIF_SUCCESS},
        {"Read fails (tag pulled)",    ScenarioReadFails, NOTIF_SUCCESS, NOTIF_ERROR},
    };

    int unexpected_alt = 0, unexpected_fixed = 0, contradictions_alt = 0, contradictions_fixed = 0;

    printf("== ALT (nfc_tools_ntag_dump() return value discarded, as shipped) ==\n");
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Notif notif; bool done; Strings out;
        worker_alt(cases[i].s, &notif, &done, &out);
        bool contradiction = (cases[i].s == ScenarioReadFails && notif == NOTIF_SUCCESS);
        if(contradiction) contradictions_alt++;
        printf("%-28s | notification: %-7s | AnalysisDone: %s%s\n",
            cases[i].name, notif_name(notif), done ? "yes" : "no",
            contradiction ? "  <-- CONTRADICTION (success sound on a failed read)" : "");
        if(notif != cases[i].expect_alt) unexpected_alt++;
    }

    printf("\n== NEU (result checked, mirrors the existing ICODE branch) ==\n");
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Notif notif; bool done; Strings out;
        worker_fixed(cases[i].s, &notif, &done, &out);
        bool contradiction = (cases[i].s == ScenarioReadFails && notif == NOTIF_SUCCESS);
        if(contradiction) contradictions_fixed++;
        printf("%-28s | notification: %-7s | AnalysisDone: %s\n",
            cases[i].name, notif_name(notif), done ? "yes" : "no");
        if(notif != cases[i].expect_fixed) unexpected_fixed++;
    }

    printf("\nBestaetigt: ALT meldet in %d von 2 Faellen die falsche Benachrichtigung "
        "(Erfolg statt Fehler bei fehlgeschlagenem Lesevorgang), NEU in %d von 2.\n",
        contradictions_alt, contradictions_fixed);
    printf("Regressionsfall (Lesevorgang erfolgreich) fuer ALT und NEU identisch: %s\n",
        (contradictions_alt == 1 && contradictions_fixed == 0) ? "bestaetigt" : "ABWEICHUNG");

    if(unexpected_alt != 0 || unexpected_fixed != 0 ||
       contradictions_alt != 1 || contradictions_fixed != 0) {
        fprintf(stderr, "UNEXPECTED RESULT\n");
        return 1;
    }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
