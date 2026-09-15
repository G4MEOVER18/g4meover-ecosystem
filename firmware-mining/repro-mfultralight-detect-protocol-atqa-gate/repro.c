// Host repro for two combined findings around mf_ultralight_detect_protocol():
//
// 1. mf_ultralight_detect_protocol() itself (detect_protocol_c.patch): compiles the actual,
//    unmodified patched mf_ultralight.c/.h (only host-only include swap, see mf_ultralight.h)
//    and exercises the real function against the base's strict atqa[0]==0x44 check vs. the
//    patched, looser atqa[1]==0x00 && sak==0x00 check. "old_detect_protocol()" below is a
//    literal, unmodified copy of the pre-patch predicate (see mf_ultralight.c.orig / my base
//    main branch) kept purely for this ALT-vs-NEU comparison; it is not part of the patch.
//
// 2. The mf_ultralight_poller_detect() gate (poller_gate.patch): the real function lives in
//    mf_ultralight_poller.c, which pulls in the full ISO14443-3A polling state machine, FuriHal
//    and the NFC generic-event plumbing -- not realistically host-compilable. Group 2 below
//    instead models its exact control flow (same structure as the real function: on
//    Iso14443_3aPollerEventTypeReady, optionally probe-read page 0, always halt afterwards) with
//    a mocked probe-read and halt call counter, and plugs the REAL, compiled
//    mf_ultralight_detect_protocol() in as the gate condition -- so the interaction between the
//    two patches is exercised with real code, not just modeled twice.
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I . -I stubs repro.c
//        mf_ultralight.c -o repro && ./repro
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "mf_ultralight.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if(!(cond)) {                                                        \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            g_failures++;                                                    \
        }                                                                    \
    } while(0)

// ---- Group 1: mf_ultralight_detect_protocol() itself, base (pre-patch) vs. patched -----------

// Literal copy of my base main-branch predicate, kept only as the ALT side of this comparison.
static bool old_detect_protocol(uint8_t atqa0, uint8_t atqa1, uint8_t sak) {
    return (atqa0 == 0x44) && (atqa1 == 0x00) && (sak == 0x00);
}

static bool new_detect_protocol(uint8_t atqa0, uint8_t atqa1, uint8_t sak) {
    Iso14443_3aData d = {0};
    d.atqa[0] = atqa0;
    d.atqa[1] = atqa1;
    d.sak = sak;
    return mf_ultralight_detect_protocol(&d);
}

static void test_detect_protocol_atqa_variants(void) {
    // Standard Ultralight/NTAG: 7-byte UID, ATQA 0x0044, SAK 0x00 -- both old and new agree.
    CHECK(old_detect_protocol(0x44, 0x00, 0x00), "std UL: base must detect");
    CHECK(new_detect_protocol(0x44, 0x00, 0x00), "std UL: patched must detect");

    // Single-size-UID (4-byte) Ultralight-compatible magic/clone: ATQA 0x0004, SAK 0x00. This is
    // the actual gap the patch closes -- base silently drops it, patched accepts it.
    CHECK(!old_detect_protocol(0x04, 0x00, 0x00), "4-byte-UID UL: base must NOT detect (the bug)");
    CHECK(new_detect_protocol(0x04, 0x00, 0x00), "4-byte-UID UL: patched must detect (the fix)");

    // Any other ATQA low byte with ATQA high byte 0x00 and SAK 0x00 (not a real-world value, but
    // proves the low byte is genuinely unchecked, not just an extra allow-listed constant).
    CHECK(new_detect_protocol(0x00, 0x00, 0x00), "atqa low=0x00: patched must still detect");

    // ATQA high byte non-zero: never Ultralight/NTAG regardless of low byte, on both sides.
    CHECK(!old_detect_protocol(0x44, 0x01, 0x00), "atqa hi != 0: base must reject");
    CHECK(!new_detect_protocol(0x44, 0x01, 0x00), "atqa hi != 0: patched must reject");

    // Non-zero SAK (MIFARE Classic 1K/4K and their magic clones use 0x08/0x18/0x28/...): rejected
    // on both sides regardless of ATQA -- the SAK term is unchanged by this patch.
    CHECK(!old_detect_protocol(0x44, 0x00, 0x08), "classic SAK: base must reject");
    CHECK(!new_detect_protocol(0x44, 0x00, 0x08), "classic SAK: patched must reject");
    CHECK(!new_detect_protocol(0x04, 0x00, 0x08), "classic SAK + UL-like atqa low: patched must reject");
}

// ---- Group 2: mf_ultralight_poller_detect() control flow, ungated vs. gated -------------------

typedef struct {
    int probe_calls;
    int halt_calls;
    bool probe_result;
} MockPollerState;

static bool mock_probe_read(MockPollerState* s) {
    s->probe_calls++;
    return s->probe_result;
}

static void mock_halt(MockPollerState* s) {
    s->halt_calls++;
}

typedef bool (*DetectGateFn)(uint8_t atqa0, uint8_t atqa1, uint8_t sak);

// Mirrors my base main-branch mf_ultralight_poller_detect(): on Ready, unconditionally probes,
// then always halts. No SAK/ATQA gate at all.
static bool poller_detect_ungated(MockPollerState* s) {
    bool detected = mock_probe_read(s);
    mock_halt(s);
    return detected;
}

// Mirrors the patched mf_ultralight_poller_detect(): on Ready, probes only if the gate accepts
// the tag's SAK/ATQA; always halts afterwards regardless.
static bool poller_detect_gated(
    MockPollerState* s,
    DetectGateFn gate,
    uint8_t atqa0,
    uint8_t atqa1,
    uint8_t sak) {
    bool detected = false;
    if(gate(atqa0, atqa1, sak)) {
        detected = mock_probe_read(s);
    }
    mock_halt(s);
    return detected;
}

static void test_poller_gate_classic_clone(void) {
    // A magic MIFARE Classic 1K clone: real Classic SAK/ATQA (0x08 / 0x0004), but it happens to
    // ack an unauthenticated READ 0x30 for block 0 the same way an Ultralight would -- this is
    // exactly the misdetection scenario the poller_gate.patch comment describes. Modeled here by
    // simply setting probe_result = true (it "answers" the probe read successfully).
    MockPollerState s = {.probe_calls = 0, .halt_calls = 0, .probe_result = true};

    bool ungated_result = poller_detect_ungated(&s);
    CHECK(ungated_result, "ungated (base): magic Classic clone gets misdetected as Ultralight");
    CHECK(s.probe_calls == 1, "ungated: probe always runs, even against a Classic clone");
    CHECK(s.halt_calls == 1, "ungated: halt still runs exactly once");

    s = (MockPollerState){.probe_calls = 0, .halt_calls = 0, .probe_result = true};
    bool gated_result = poller_detect_gated(&s, new_detect_protocol, 0x04, 0x00, 0x08);
    CHECK(!gated_result, "gated (patched): magic Classic clone correctly rejected before probing");
    CHECK(s.probe_calls == 0, "gated: probe must NOT run against a non-UL SAK/ATQA");
    CHECK(s.halt_calls == 1, "gated: halt still runs exactly once even when the gate rejects");
}

static void test_poller_gate_real_ultralight(void) {
    // A genuine Ultralight (ATQA 0x0044, SAK 0x00): both ungated and gated must still detect it,
    // and both must still probe exactly once.
    MockPollerState s = {.probe_calls = 0, .halt_calls = 0, .probe_result = true};
    CHECK(poller_detect_ungated(&s), "ungated: real UL still detected");
    CHECK(s.probe_calls == 1 && s.halt_calls == 1, "ungated: real UL probes once, halts once");

    s = (MockPollerState){.probe_calls = 0, .halt_calls = 0, .probe_result = true};
    CHECK(
        poller_detect_gated(&s, new_detect_protocol, 0x44, 0x00, 0x00),
        "gated: real UL still detected");
    CHECK(s.probe_calls == 1 && s.halt_calls == 1, "gated: real UL probes once, halts once");
}

static void test_gate_needs_the_loosened_predicate(void) {
    // Cross-check that the two patches are genuinely complementary, not independently sufficient:
    // a 4-byte-UID magic/clone Ultralight (ATQA 0x0004, SAK 0x00) used to work under the base,
    // ungated poller purely because it never checked SAK/ATQA at all. Gating the probe on the
    // STILL-STRICT base predicate (old_detect_protocol) would silently turn that pre-existing
    // working case into a regression; gating on the patched, loosened predicate keeps it working.
    MockPollerState s = {.probe_calls = 0, .halt_calls = 0, .probe_result = true};
    bool with_old_predicate = poller_detect_gated(&s, old_detect_protocol, 0x04, 0x00, 0x00);
    CHECK(
        !with_old_predicate,
        "gate + base predicate: 4-byte-UID UL clone would regress if detect_protocol weren't "
        "loosened too");
    CHECK(s.probe_calls == 0, "gate + base predicate: probe skipped, confirming the regression");

    s = (MockPollerState){.probe_calls = 0, .halt_calls = 0, .probe_result = true};
    bool with_new_predicate = poller_detect_gated(&s, new_detect_protocol, 0x04, 0x00, 0x00);
    CHECK(with_new_predicate, "gate + patched predicate: 4-byte-UID UL clone still detected");
    CHECK(s.probe_calls == 1, "gate + patched predicate: probe actually runs");
}

// ---- Deliberately-broken variants, to confirm the tests above are not vacuous -----------------

static bool broken_detect_protocol_ignores_sak(uint8_t atqa0, uint8_t atqa1, uint8_t sak) {
    (void)sak;
    (void)atqa0;
    return atqa1 == 0x00;
}

static void test_broken_variant_is_caught(void) {
    // A gate that forgets the SAK term would let the magic Classic clone from
    // test_poller_gate_classic_clone() through -- confirms that test actually exercises the SAK
    // check rather than passing regardless of the gate's logic.
    MockPollerState s = {.probe_calls = 0, .halt_calls = 0, .probe_result = true};
    bool result = poller_detect_gated(&s, broken_detect_protocol_ignores_sak, 0x04, 0x00, 0x08);
    CHECK(result, "sanity: SAK-less gate wrongly lets the Classic clone through (expected here)");
    CHECK(s.probe_calls == 1, "sanity: SAK-less gate wrongly probes the Classic clone (expected)");
}

int main(void) {
    test_detect_protocol_atqa_variants();
    test_poller_gate_classic_clone();
    test_poller_gate_real_ultralight();
    test_gate_needs_the_loosened_predicate();
    test_broken_variant_is_caught();

    if(g_failures == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        fprintf(stderr, "%d CHECK(S) FAILED\n", g_failures);
        return 1;
    }
}
