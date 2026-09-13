// Host-only model of the MF Ultralight/NTAG capacity gate in
// nfc_tools_scene_write_scan.c (nfctools-fz, wakdev). No Flipper/NFC
// dependency: models only the two things that matter for this bug —
// whether the CC (page 3) read succeeds, and whether the caller retries it
// the same way it already retries every page write in the same function.
//
// Bug: ALT never retries the CC read and silently substitutes a fixed
// fallback capacity (48) whenever it fails for *any* reason, including a
// transient communication error identical in kind to the ones the write
// path right below it explicitly retries. That fallback is then reported
// to the user as if it were the tag's real capacity, via a "Tag too
// small!" message — conflating "I couldn't read the tag" with "the tag is
// too small", and skipping the retry the surrounding code uses everywhere
// else for exactly this class of failure.
//
// NEU applies the same "one silent retry after a delay" pattern already
// used for every page write in the same worker to the CC read as well, and
// only ever reports "Tag too small" when a *successful* read reported a
// real capacity smaller than what's needed.

#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char* label;
    bool        attempt1_ok;
    bool        attempt2_ok; // only consulted if attempt1 fails
    int         real_capacity;
    int         ndef_size;
} Scenario;

typedef struct {
    bool        write_attempted;
    bool        reported_too_small;
    int         reported_capacity; // meaningful only if reported_too_small
    bool        reported_comm_error;
} Outcome;

#define FALLBACK_CAPACITY 48

static Outcome run_alt(const Scenario* s) {
    Outcome o = {0};
    bool    cc_ok    = s->attempt1_ok; // ALT: no retry on the CC read
    int     capacity = cc_ok ? s->real_capacity : FALLBACK_CAPACITY;

    if(s->ndef_size > capacity) {
        o.reported_too_small = true;
        o.reported_capacity  = capacity;
        return o;
    }
    o.write_attempted = true;
    return o;
}

static Outcome run_neu(const Scenario* s) {
    Outcome o = {0};
    bool    cc_ok = s->attempt1_ok;
    if(!cc_ok) {
        // same silent one-retry-after-delay pattern as every page write
        // below it in the same function
        cc_ok = s->attempt2_ok;
    }

    if(!cc_ok) {
        // honest: this is a communication failure, not a size verdict
        o.reported_comm_error = true;
        return o;
    }

    if(s->ndef_size > s->real_capacity) {
        o.reported_too_small = true;
        o.reported_capacity  = s->real_capacity;
        return o;
    }
    o.write_attempted = true;
    return o;
}

int main(void) {
    Scenario scenarios[] = {
        // label, attempt1_ok, attempt2_ok, real_capacity, ndef_size
        {"transient CC-read blip, tag has room (144B, writing 100B)", false, true, 144, 100},
        {"permanent CC-read failure (tag pulled away, writing 100B)", false, false, 144, 100},
        {"immediate success, genuinely tiny tag (32B, writing 100B)", true, true, 32, 100},
        {"immediate success, tag has room (144B, writing 100B)", true, true, 144, 100},
    };
    int n = (int)(sizeof(scenarios) / sizeof(scenarios[0]));

    int alt_wrong = 0, neu_wrong = 0;

    printf("== ALT (CC-Read ohne Retry, Fallback als echte Kapazitaet gemeldet) ==\n");
    for(int i = 0; i < n; i++) {
        Outcome o = run_alt(&scenarios[i]);
        bool    is_wrong = false;
        if(scenarios[i].attempt1_ok == false && o.reported_too_small &&
           scenarios[i].real_capacity >= scenarios[i].ndef_size) {
            // read failed, but tag would actually have had room -> "too small" is a lie
            is_wrong = true;
        }
        if(is_wrong) alt_wrong++;
        printf(
            "ALT %-58s | write=%d too_small=%d (meldet %dB) comm_err=%d %s\n",
            scenarios[i].label,
            o.write_attempted,
            o.reported_too_small,
            o.reported_capacity,
            o.reported_comm_error,
            is_wrong ? "<-- FALSCH: Kommunikationsfehler als 'zu klein' gemeldet" : "");
    }

    printf("\n== NEU (CC-Read mit demselben Retry wie die Seiten-Writes) ==\n");
    for(int i = 0; i < n; i++) {
        Outcome o = run_neu(&scenarios[i]);
        bool    is_wrong = false;
        if((scenarios[i].attempt1_ok || scenarios[i].attempt2_ok) == false && o.reported_too_small) {
            is_wrong = true; // fabricated a size verdict despite never reading the tag
        }
        if((scenarios[i].attempt1_ok || scenarios[i].attempt2_ok) && o.reported_comm_error) {
            is_wrong = true; // reported comm error despite a successful read
        }
        if(is_wrong) neu_wrong++;
        printf(
            "NEU %-58s | write=%d too_small=%d (meldet %dB) comm_err=%d\n",
            scenarios[i].label,
            o.write_attempted,
            o.reported_too_small,
            o.reported_capacity,
            o.reported_comm_error);
    }

    printf(
        "\nBestaetigt: ALT meldet in %d von %d Faellen eine falsche 'Tag too small'-Ursache, "
        "NEU in %d von %d.\n",
        alt_wrong,
        n,
        neu_wrong,
        n);

    return (alt_wrong > 0 && neu_wrong == 0) ? 0 : 1;
}
