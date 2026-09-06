// Host repro (no Flipper/NFC dependency): demonstrates that
// nfc_tools_scene_nfc_commands_run.c's protocol_is_iso4a() hand-lists only
// 3 of the 6 real ISO14443-4A-family protocols, and that the resulting
// routing function accepts every possible NfcProtocol value with zero
// rejections -- unlike every other worker in the same app (Set Password /
// Remove Password / Format Tag), which explicitly reject anything outside
// their supported list.
//
// The NfcProtocol enum, its parent/child tree, and nfc_protocol_has_parent()
// are copied verbatim (values + logic) from the firmware's own
// lib/nfc/protocols/nfc_protocol.{h,c} (RogueMaster branch 420, same
// mntm-012-lineage header used by every previous NFC-Tools session) --
// this is not a guess at the hierarchy, it is the real one.
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
//   ./repro

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

typedef enum {
    NfcProtocolIso14443_3a,
    NfcProtocolIso14443_3b,
    NfcProtocolIso14443_4a,
    NfcProtocolIso14443_4b,
    NfcProtocolIso15693_3,
    NfcProtocolFelica,
    NfcProtocolMfUltralight,
    NfcProtocolMfClassic,
    NfcProtocolMfPlus,
    NfcProtocolMfDesfire,
    NfcProtocolSlix,
    NfcProtocolSt25tb,
    NfcProtocolNtag4xx,
    NfcProtocolType4Tag,
    NfcProtocolEmv,
    NfcProtocolNum,
    NfcProtocolInvalid,
} NfcProtocol;

static const char* const protocol_names[NfcProtocolNum] = {
    [NfcProtocolIso14443_3a] = "Iso14443_3a",
    [NfcProtocolIso14443_3b] = "Iso14443_3b",
    [NfcProtocolIso14443_4a] = "Iso14443_4a",
    [NfcProtocolIso14443_4b] = "Iso14443_4b",
    [NfcProtocolIso15693_3] = "Iso15693_3",
    [NfcProtocolFelica] = "Felica",
    [NfcProtocolMfUltralight] = "MfUltralight",
    [NfcProtocolMfClassic] = "MfClassic",
    [NfcProtocolMfPlus] = "MfPlus",
    [NfcProtocolMfDesfire] = "MfDesfire",
    [NfcProtocolSlix] = "Slix",
    [NfcProtocolSt25tb] = "St25tb",
    [NfcProtocolNtag4xx] = "Ntag4xx",
    [NfcProtocolType4Tag] = "Type4Tag",
    [NfcProtocolEmv] = "Emv",
};

// Verbatim parent_protocol column of nfc_protocol_nodes[] in
// lib/nfc/protocols/nfc_protocol.c (children arrays omitted, only the
// parent links are needed to reproduce nfc_protocol_has_parent()).
static const NfcProtocol parent_of[NfcProtocolNum] = {
    [NfcProtocolIso14443_3a] = NfcProtocolInvalid,
    [NfcProtocolIso14443_3b] = NfcProtocolInvalid,
    [NfcProtocolIso14443_4a] = NfcProtocolIso14443_3a,
    [NfcProtocolIso14443_4b] = NfcProtocolIso14443_3b,
    [NfcProtocolIso15693_3] = NfcProtocolInvalid,
    [NfcProtocolFelica] = NfcProtocolInvalid,
    [NfcProtocolMfUltralight] = NfcProtocolIso14443_3a,
    [NfcProtocolMfClassic] = NfcProtocolIso14443_3a,
    [NfcProtocolMfPlus] = NfcProtocolIso14443_4a,
    [NfcProtocolMfDesfire] = NfcProtocolIso14443_4a,
    [NfcProtocolSlix] = NfcProtocolIso15693_3,
    [NfcProtocolSt25tb] = NfcProtocolInvalid,
    [NfcProtocolNtag4xx] = NfcProtocolIso14443_4a,
    [NfcProtocolType4Tag] = NfcProtocolIso14443_4a,
    [NfcProtocolEmv] = NfcProtocolIso14443_4a,
};

// Verbatim logic of nfc_protocol_has_parent() from nfc_protocol.c, ported to
// the parent_of[] table above instead of the full tree-node struct.
static bool nfc_protocol_has_parent(NfcProtocol protocol, NfcProtocol parent_protocol) {
    NfcProtocol iter = protocol;
    while(parent_of[iter] != NfcProtocolInvalid) {
        if(parent_of[iter] == parent_protocol) return true;
        iter = parent_of[iter];
    }
    return false;
}

// ── OLD: verbatim copy of protocol_is_iso4a() from
// applications/external/nfctools/scenes/other/nfc_tools_scene_nfc_commands_run.c ──
static bool protocol_is_iso4a_OLD(NfcProtocol p) {
    return p == NfcProtocolIso14443_4a || p == NfcProtocolMfDesfire || p == NfcProtocolMfPlus;
}

static const char* old_route(NfcProtocol p) {
    return protocol_is_iso4a_OLD(p) ? "4A poller (sent)" : "3A poller (sent)";
}

// ── NEW: routes by real protocol-tree family (same nfc_protocol_has_parent()
// API the firmware already exposes for exactly this purpose) instead of a
// hand-picked, incomplete list, and rejects anything outside both families --
// matching the "Incompatible tag" / "Unsupported tag: %s" pattern already
// used by nfc_tools_scene_set_password_write.c, _remove_password_write.c and
// nfc_tools_scene_format_tag.c in the very same app.
static const char* new_route(NfcProtocol p) {
    bool is4a = (p == NfcProtocolIso14443_4a) ||
                nfc_protocol_has_parent(p, NfcProtocolIso14443_4a);
    if(is4a) return "4A poller (sent)";

    bool is3a_only = (p == NfcProtocolIso14443_3a) || (p == NfcProtocolMfUltralight) ||
                     (p == NfcProtocolMfClassic);
    if(is3a_only) return "3A poller (sent)";

    return "REJECTED (\"Incompatible tag\")";
}

int main(void) {
    int mismatches = 0;

    printf(
        "%-14s | %-20s | %s\n", "protocol", "OLD (as-shipped)", "NEW (real protocol-tree family)");
    printf("--------------------------------------------------------------------------------\n");
    for(NfcProtocol p = 0; p < NfcProtocolNum; p++) {
        const char* old_r = old_route(p);
        const char* new_r = new_route(p);
        bool differs = (old_r[0] != new_r[0]); // '4'/'3'/'R' first-char tag is enough here
        if(differs) mismatches++;
        printf("%-14s | %-20s | %s%s\n", protocol_names[p], old_r, new_r, differs ? "   <-- differs" : "");
    }

    printf("\n%d / %d protocol values are routed differently by OLD vs. NEW.\n", mismatches, (int)NfcProtocolNum);
    printf("Two distinct failure modes hide behind that number:\n");
    printf(
        "  1. Real ISO14443-4A-family cards silently sent through the WRONG (3A) poller:\n"
        "     Ntag4xx (NTAG424 DNA and similar), Type4Tag and Emv (contactless payment cards)\n"
        "     -- all three are direct children of Iso14443_4a in nfc_protocol.c's own tree,\n"
        "     exactly like the MfDesfire/MfPlus siblings protocol_is_iso4a() DOES recognise.\n");
    printf(
        "  2. Genuinely unrelated tags (Felica, Iso15693_3/Slix, St25tb, Iso14443_3b/4b) get no\n"
        "     rejection at all and are still pushed into the 3A poller, instead of the clear\n"
        "     \"Incompatible tag\" message every sibling scene in this app already gives.\n");

    // 3 wrong-poller cases (Ntag4xx, Type4Tag, Emv) + 6 should-be-rejected cases
    // (Iso14443_3b, Iso14443_4b, Iso15693_3, Felica, Slix, St25tb).
    assert(mismatches == 9);
    return 0;
}
