// Host repro: unobserved second furi_event_flag_wait() return value in the
// four NFC Tools sibling scenes (set_password_write, remove_password_write,
// nfc_commands_run, lock_tag).
//
// Shared pattern in all four scenes (Phase 4 / Phase 2 for nfc_commands_run):
//
//   <SomeCtx> ctx = {};                       // success=false, err[0]='\0'
//   ...
//   NfcPoller* poller = nfc_poller_alloc(...);
//   nfc_poller_start[_ex](poller, cb, &ctx);
//   furi_event_flag_wait(ctx.done, 1u, FuriFlagWaitAny, 8000);   // <-- return value discarded
//   nfc_poller_stop(poller);
//   nfc_poller_free(poller);
//   furi_event_flag_free(ctx.done);
//   ...
//   if(ctx.success) { ... } else { furi_string_set(app->info_str, ctx.err); ... }
//
// The callback is the only place that sets ctx.err and signals ctx.done. If
// the callback never runs within the 8s window -- the scanner phase already
// confirmed a tag, but it is pulled away (or coupling drops) before the
// second, poller-driven RF session's first event -- furi_event_flag_wait()
// returns a timeout status that is never inspected. ctx.success stays at its
// zero-init value (false) and ctx.err stays at its zero-init value (empty
// string), so the popup shows header "Failed" with a completely blank body
// instead of any cause. This is the same fault class already documented for
// nfc_tools_scene_felica_write.c, here reproduced for the four scenes that
// still have it.
//
// This model uses one generic ctx shape (success + err[80]) that is
// structurally identical across all four affected files, and exercises the
// union of callback outcomes actually present in them: tag contact error,
// incompatible/unsupported tag, a step-specific write/auth error, success,
// and the never-fires timeout case.
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    bool success;
    char err[80]; // zero-initialized -> empty string until a callback writes it
} Ctx;

typedef enum {
    ScenarioTagContactError,
    ScenarioIncompatible,
    ScenarioStepError,
    ScenarioSuccess,
    ScenarioNeverFires, // scanner found a tag, but the poller callback never runs within 8s
} Scenario;

// Models the callback actually writing ctx.err/ctx.success, exactly as in
// nfc_tools_set_password_write_cb() / nfc_tools_remove_password_write_cb() /
// nfc_commands_iso3a_cb()/nfc_commands_iso4a_cb() / nfc_tools_lock_tag_cb().
static void run_callback_if_fired(Scenario s, Ctx* ctx) {
    switch(s) {
    case ScenarioTagContactError:
        strncpy(ctx->err, "Tag contact error", sizeof(ctx->err) - 1);
        break;
    case ScenarioIncompatible:
        strncpy(ctx->err, "Incompatible tag\nOnly NTAG213/215/216", sizeof(ctx->err) - 1);
        break;
    case ScenarioStepError:
        snprintf(ctx->err, sizeof(ctx->err), "Config error\npage 0x29 (err:3)");
        break;
    case ScenarioSuccess:
        ctx->success = true;
        break;
    case ScenarioNeverFires:
        // Callback never runs -> ctx stays at its zero-initialized values.
        break;
    }
}

// ALT: as shipped -- wait() return value discarded.
static void worker_alt(Scenario s, char* header_out, char* body_out, size_t body_cap) {
    Ctx ctx = {0};

    // furi_event_flag_wait(ctx.done, 1u, FuriFlagWaitAny, 8000); // return ignored
    if(s != ScenarioNeverFires) {
        run_callback_if_fired(s, &ctx);
    }
    // else: timeout, callback never ran, ctx is still {false, ""}

    if(ctx.success) {
        strcpy(header_out, "OK");
        snprintf(body_out, body_cap, "Success");
    } else {
        strcpy(header_out, "Failed");
        snprintf(body_out, body_cap, "%s", ctx.err);
    }
}

// NEU: wait() return value checked; on timeout, ctx.err is set explicitly
// before the success/fail branch, using the already-existing
// NTS_ERR_TAG_CONTACT string ("Tag contact error") -- the same constant the
// callbacks themselves already use for "poller ran but never reached Ready".
// A wait timeout after a confirmed scanner detection is the same underlying
// situation (RF session never completed), so no new string is introduced.
static void worker_neu(Scenario s, char* header_out, char* body_out, size_t body_cap) {
    Ctx ctx = {0};
    bool timed_out = false;

    // uint32_t wait_result = furi_event_flag_wait(ctx.done, 1u, FuriFlagWaitAny, 8000);
    if(s != ScenarioNeverFires) {
        run_callback_if_fired(s, &ctx);
        // wait_result == 1u (flag observed)
    } else {
        timed_out = true; // wait_result would be FuriFlagErrorTimeout
    }

    if(timed_out) {
        strncpy(ctx.err, "Tag contact error", sizeof(ctx.err) - 1); // NTS_ERR_TAG_CONTACT
    }

    if(ctx.success) {
        strcpy(header_out, "OK");
        snprintf(body_out, body_cap, "Success");
    } else {
        strcpy(header_out, "Failed");
        snprintf(body_out, body_cap, "%s", ctx.err);
    }
}

static const char* scenario_name(Scenario s) {
    switch(s) {
    case ScenarioTagContactError: return "tag contact error (Ready check fails)";
    case ScenarioIncompatible:    return "incompatible tag type";
    case ScenarioStepError:       return "step-specific write/auth error";
    case ScenarioSuccess:         return "success";
    case ScenarioNeverFires:      return "no callback event within 8s (tag lost after scan)";
    }
    return "?";
}

int main(void) {
    Scenario all[] = {
        ScenarioTagContactError,
        ScenarioIncompatible,
        ScenarioStepError,
        ScenarioSuccess,
        ScenarioNeverFires,
    };
    int n = (int)(sizeof(all) / sizeof(all[0]));

    int blank_alt = 0, blank_neu = 0, regressions = 0;

    printf("== ALT (wait() return value discarded) ==\n");
    for(int i = 0; i < n; i++) {
        char header[16], body[128];
        worker_alt(all[i], header, body, sizeof(body));
        int blank = (header[0] != '\0' && body[0] == '\0');
        if(blank) blank_alt++;
        printf("ALT %-50s | header=%-6s body=\"%s\"%s\n",
               scenario_name(all[i]), header, body, blank ? "  <-- BLANK BODY" : "");
    }

    printf("\n== NEU (timeout sets NTS_ERR_TAG_CONTACT) ==\n");
    for(int i = 0; i < n; i++) {
        char header[16], body[128];
        worker_neu(all[i], header, body, sizeof(body));
        int blank = (header[0] != '\0' && body[0] == '\0');
        if(blank) blank_neu++;
        printf("NEU %-50s | header=%-6s body=\"%s\"%s\n",
               scenario_name(all[i]), header, body, blank ? "  <-- BLANK BODY" : "");
    }

    printf("\n== Regression check (all non-timeout scenarios identical ALT vs NEU) ==\n");
    for(int i = 0; i < n; i++) {
        if(all[i] == ScenarioNeverFires) continue;
        char h_alt[16], b_alt[128], h_neu[16], b_neu[128];
        worker_alt(all[i], h_alt, b_alt, sizeof(b_alt));
        worker_neu(all[i], h_neu, b_neu, sizeof(b_neu));
        int same = (strcmp(h_alt, h_neu) == 0) && (strcmp(b_alt, b_neu) == 0);
        if(!same) regressions++;
        printf("%-50s | %s\n", scenario_name(all[i]), same ? "identical" : "DIFFERS");
    }

    printf(
        "\nBestaetigt: ALT %d von %d Faellen mit leerer Fehlermeldung, NEU %d von %d. "
        "Regressionsabweichungen: %d.\n",
        blank_alt, n, blank_neu, n, regressions);

    return (blank_alt > 0 && blank_neu == 0 && regressions == 0) ? 0 : 1;
}
