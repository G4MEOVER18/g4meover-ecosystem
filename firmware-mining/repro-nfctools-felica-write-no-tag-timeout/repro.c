// Host-only model of the FeliCa write worker's tag-presence timeout in
// nfc_tools_scene_felica_write.c (nfctools-fz, wakdev). No Flipper/NFC
// dependency: models only what matters for this bug — whether a FeliCa
// poller event ever fires within the 8 s wait, and what the popup ends up
// showing afterwards.
//
// Bug: nfc_tools_felica_write_worker() starts an NfcPoller and waits on
// wctx.done with a single furi_event_flag_wait(..., 8000), but never looks
// at the wait's return value. app->info_str is set only from inside the
// poller callback (nfc_tools_felica_write_cb) — Ready/Incomplete/Error, all
// of which also signal wctx.done. If no FeliCa tag is presented within the
// 8 s window, the callback never runs, wctx.done is never signaled, and
// wctx.success stays at its initialized value (false). The worker still
// falls into the "else" branch (sequence_error + WriteFail) and displays
// app->info_str — which was never touched since on_enter's
// furi_string_reset(), i.e. an empty string. Result: header "Failed" with a
// blank body.
//
// Every other poller/write worker in the same app (nfc_tools_scene_write_
// scan.c, _set_password_write.c, _remove_password_write.c,
// _nfc_commands_run.c, _lock_tag.c) runs an explicit NfcScanner detection
// phase first and reports NTS_ERR_NO_TAG ("No tag detected") on that
// phase's timeout before ever starting the actual poller. FeliCa write is
// the only write path with no detection phase at all — the single 8 s
// poller wait is the sole presence check, and its timeout is silently
// swallowed.
//
// NEU checks the wait's return value; only on timeout does it set
// info_str to NTS_ERR_NO_TAG before falling through to the existing
// success/failure branch. Every path where the callback did fire (success,
// denied, bad response, activation error) is untouched, since those all
// already set a real info_str and the wait returns the signaled flag (1u),
// not a timeout.

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define WAIT_OK      1u        // furi_event_flag_wait: flag 1u was signaled
#define WAIT_TIMEOUT 0xFFFFFFFEu // stand-in for (uint32_t)FuriFlagErrorTimeout
#define NTS_ERR_NO_TAG "No tag detected"

typedef enum {
    OutcomeNoTag,       // poller callback never ran (no FeliCa tag presented)
    OutcomeSuccess,     // Ready + SF1==0 && SF2==0
    OutcomeDenied,      // Ready + SF1/SF2 nonzero
    OutcomeBadResponse, // Ready but response too short / wrong code
    OutcomeActivateErr  // FelicaPollerEventTypeError
} Outcome;

typedef struct {
    const char* label;
    Outcome     outcome;
} Scenario;

typedef struct {
    const char* header; // "OK" or "Failed"
    char        body[96];
} Result;

// Simulates what nfc_tools_felica_write_cb() would have written into
// info_str for each outcome where the callback actually runs.
static void callback_info_str(Outcome o, char* out, size_t out_size) {
    switch(o) {
    case OutcomeSuccess:
        snprintf(out, out_size, "Block S06 written\nOK");
        break;
    case OutcomeDenied:
        snprintf(out, out_size, "Write denied\nBlock S06\nSF1=01 SF2=A6");
        break;
    case OutcomeBadResponse:
        snprintf(out, out_size, "Bad response\nlen=4 code=00");
        break;
    case OutcomeActivateErr:
        snprintf(out, out_size, "Tag error\nCould not\nactivate FeliCa");
        break;
    case OutcomeNoTag:
    default:
        out[0] = '\0'; // callback never runs for this outcome
        break;
    }
}

static Result run_alt(const Scenario* s) {
    Result r = {0};
    char   info_str[96] = ""; // furi_string_reset() at on_enter

    bool success = (s->outcome == OutcomeSuccess);
    if(s->outcome != OutcomeNoTag) {
        callback_info_str(s->outcome, info_str, sizeof(info_str));
    }
    // ALT: furi_event_flag_wait(...)'s return value is discarded entirely.

    if(success) {
        r.header = "OK";
    } else {
        r.header = "Failed";
    }
    snprintf(r.body, sizeof(r.body), "%s", info_str);
    return r;
}

static Result run_neu(const Scenario* s) {
    Result   r = {0};
    char     info_str[96] = ""; // furi_string_reset() at on_enter
    uint32_t wait_result;

    bool success = (s->outcome == OutcomeSuccess);
    if(s->outcome != OutcomeNoTag) {
        callback_info_str(s->outcome, info_str, sizeof(info_str));
        wait_result = WAIT_OK;
    } else {
        wait_result = WAIT_TIMEOUT;
    }

    // NEU: only override info_str when the wait actually timed out, i.e.
    // the callback never ran and info_str was never set by it.
    if(wait_result != WAIT_OK) {
        snprintf(info_str, sizeof(info_str), "%s", NTS_ERR_NO_TAG);
    }

    if(success) {
        r.header = "OK";
    } else {
        r.header = "Failed";
    }
    snprintf(r.body, sizeof(r.body), "%s", info_str);
    return r;
}

int main(void) {
    Scenario scenarios[] = {
        {"no FeliCa tag presented within 8s (timeout)", OutcomeNoTag},
        {"tag presented, write succeeds",                OutcomeSuccess},
        {"tag presented, write denied (SF1/SF2 set)",    OutcomeDenied},
        {"tag presented, malformed/short response",      OutcomeBadResponse},
        {"tag presented, FeliCa activation failed",      OutcomeActivateErr},
    };
    size_t n = sizeof(scenarios) / sizeof(scenarios[0]);

    int blank_alt = 0, blank_neu = 0;
    int mismatch  = 0;

    printf("== ALT (wait() return value discarded) ==\n");
    for(size_t i = 0; i < n; i++) {
        Result r = run_alt(&scenarios[i]);
        bool   blank = (r.body[0] == '\0');
        if(blank) blank_alt++;
        printf(
            "ALT %-45s | header=%-6s body=\"%s\"%s\n",
            scenarios[i].label,
            r.header,
            r.body,
            blank ? "  <-- BLANK BODY" : "");
    }

    printf("\n== NEU (timeout sets NTS_ERR_NO_TAG) ==\n");
    for(size_t i = 0; i < n; i++) {
        Result r = run_neu(&scenarios[i]);
        bool   blank = (r.body[0] == '\0');
        if(blank) blank_neu++;
        printf(
            "NEU %-45s | header=%-6s body=\"%s\"%s\n",
            scenarios[i].label,
            r.header,
            r.body,
            blank ? "  <-- BLANK BODY" : "");
    }

    // Regression check: for every scenario where the callback actually ran
    // (i.e. not the no-tag/timeout case), ALT and NEU must produce an
    // identical result.
    printf("\n== Regression check (all non-timeout scenarios identical ALT vs NEU) ==\n");
    for(size_t i = 0; i < n; i++) {
        if(scenarios[i].outcome == OutcomeNoTag) continue;
        Result a = run_alt(&scenarios[i]);
        Result b = run_neu(&scenarios[i]);
        bool   same = (strcmp(a.header, b.header) == 0) && (strcmp(a.body, b.body) == 0);
        if(!same) mismatch++;
        printf(
            "%-45s | %s\n",
            scenarios[i].label,
            same ? "identical" : "MISMATCH");
    }

    printf(
        "\nBestaetigt: ALT zeigt in %d von %d Faellen eine leere Fehlermeldung, "
        "NEU in %d von %d. Regressionsabweichungen: %d.\n",
        blank_alt, (int)n, blank_neu, (int)n, mismatch);

    return (blank_alt == 1 && blank_neu == 0 && mismatch == 0) ? 0 : 1;
}
