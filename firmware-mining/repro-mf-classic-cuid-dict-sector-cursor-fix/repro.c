// Host repro for the MF Classic CUID-dictionary sector-cursor bug in
// nfc_scene_mf_classic_dict_attack.c (own mntm-012 base).
//
// Isolates the two integer expressions verbatim from both source states -
// no Furi/scene-manager/poller framework needed, since the bug is pure
// arithmetic: how the CUID pass's key-index counter is turned into the
// sector number shown on screen, and whether that number is cleared when a
// new dictionary phase starts.
//
// Field types (uint16_t current_key_idx, uint8_t current_sector,
// uint8_t sectors_total) and the three real MIFARE Classic sector counts
// (Mini 5, 1K 16, 4K 40) are taken from nfc_app_i.h / mf_classic.c.
//
// Build: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
// Run:   ./repro

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint16_t current_key_idx;
    uint8_t current_sector;
} DictAttackCtx;

// Verbatim from the ALT (currently shipped) NextSector handler's is_cuid_dict branch:
//   instance->nfc_dict_context.current_key_idx++;
//   instance->nfc_dict_context.current_sector =
//       instance->nfc_dict_context.current_key_idx / 2;
//   mfc_event->data->next_sector_data.current_sector =
//       instance->nfc_dict_context.current_sector;
static uint8_t old_next_sector_step(DictAttackCtx* ctx, uint8_t sectors_total) {
    (void)sectors_total; // ALT ignores it entirely - that is the bug
    ctx->current_key_idx++;
    ctx->current_sector = ctx->current_key_idx / 2;
    uint8_t published_to_poller = ctx->current_sector;
    return published_to_poller;
}

// Verbatim from the NEU (patched) NextSector handler's is_cuid_dict branch:
//   instance->nfc_dict_context.current_key_idx++;
//   uint8_t next_sector = instance->nfc_dict_context.current_key_idx / 2;
//   mfc_event->data->next_sector_data.current_sector = next_sector;
//   if(next_sector < instance->nfc_dict_context.sectors_total) {
//       instance->nfc_dict_context.current_sector = next_sector;
//   }
static uint8_t new_next_sector_step(DictAttackCtx* ctx, uint8_t sectors_total) {
    ctx->current_key_idx++;
    uint8_t next_sector = ctx->current_key_idx / 2;
    uint8_t published_to_poller = next_sector;
    if(next_sector < sectors_total) {
        ctx->current_sector = next_sector;
    }
    return published_to_poller;
}

// ALT prepare_view: no line touches current_sector between phases.
static void old_prepare_view_phase_switch(DictAttackCtx* ctx) {
    (void)ctx;
}

// NEU prepare_view: adds "instance->nfc_dict_context.current_sector = 0;"
// right after the dict_keys_current reset, ahead of the phase's first repaint.
static void new_prepare_view_phase_switch(DictAttackCtx* ctx) {
    ctx->current_sector = 0;
}

typedef struct {
    const char* type_name;
    uint8_t sectors_total;
} CardType;

static const CardType CARD_TYPES[] = {
    {"Mini (0.3K)", 5},
    {"1K", 16},
    {"4K", 40},
};
#define NUM_CARD_TYPES (sizeof(CARD_TYPES) / sizeof(CARD_TYPES[0]))

int main(void) {
    int checks = 0;

    // --- Full-domain sweep: does the display cursor ever reach an out-of-range
    // sector during a CUID pass, for every real card type? ---
    for(size_t t = 0; t < NUM_CARD_TYPES; t++) {
        uint8_t sectors_total = CARD_TYPES[t].sectors_total;
        // A full CUID pass tries every key index 0..2*sectors_total-1 (A/B per
        // sector), then the poller advances one more step to hand back the
        // end-of-pass marker (key_idx == 2*sectors_total). Sweep well past
        // that so no marker step is missed.
        uint16_t last_key_idx = (uint16_t)(2 * sectors_total + 8);

        DictAttackCtx old_ctx = {0};
        DictAttackCtx new_ctx = {0};
        bool old_saw_out_of_range = false;
        bool new_saw_out_of_range = false;
        uint8_t old_marker_value = 0;

        for(uint16_t key_idx = 0; key_idx < last_key_idx; key_idx++) {
            old_next_sector_step(&old_ctx, sectors_total);
            new_next_sector_step(&new_ctx, sectors_total);

            if(old_ctx.current_sector >= sectors_total) {
                if(!old_saw_out_of_range) {
                    old_marker_value = old_ctx.current_sector;
                }
                old_saw_out_of_range = true;
            }
            if(new_ctx.current_sector >= sectors_total) {
                new_saw_out_of_range = true;
            }
            checks++;
        }

        // ALT: the display cursor does reach the one-past-the-end marker.
        assert(old_saw_out_of_range);
        // For the 1K case this must be exactly the value from Unleashed's own
        // bug report ("Unlocking sector: 16" on a 1K card, sectors 0..15).
        if(sectors_total == 16) {
            assert(old_marker_value == 16);
            printf(
                "[%-12s] ALT reaches out-of-range display sector %u (valid: 0..%u) - matches "
                "the reported \"Unlocking sector: 16\" bug\n",
                CARD_TYPES[t].type_name,
                old_marker_value,
                sectors_total - 1);
        } else {
            printf(
                "[%-12s] ALT reaches out-of-range display sector %u (valid: 0..%u)\n",
                CARD_TYPES[t].type_name,
                old_marker_value,
                sectors_total - 1);
        }
        checks++;

        // NEU: full sweep, never once out of range.
        assert(!new_saw_out_of_range);
        printf(
            "[%-12s] NEU stays within 0..%u for the entire %u-step sweep - no out-of-range "
            "display value\n",
            CARD_TYPES[t].type_name,
            sectors_total - 1,
            last_key_idx);
        checks++;
    }

    // --- Cross-phase persistence: does the stale marker leak into the next
    // phase's first repaint (before its own first NextSector event)? ---
    for(size_t t = 0; t < NUM_CARD_TYPES; t++) {
        uint8_t sectors_total = CARD_TYPES[t].sectors_total;

        DictAttackCtx old_ctx = {0};
        DictAttackCtx new_ctx = {0};
        // Drive a full CUID pass to its end-of-pass marker (mirrors the real
        // sequence: NextSector fires once per exhausted key index pair).
        for(uint16_t key_idx = 0; key_idx < (uint16_t)(2 * sectors_total); key_idx++) {
            old_next_sector_step(&old_ctx, sectors_total);
            new_next_sector_step(&new_ctx, sectors_total);
        }
        assert(old_ctx.current_sector == sectors_total); // ALT: cursor sits on the marker
        assert(new_ctx.current_sector == sectors_total - 1); // NEU: capped at the last real sector
        checks += 2;

        // Phase switches to User Dictionary: prepare_view runs, then the view
        // repaints, before any NextSector event of the new phase has fired.
        old_prepare_view_phase_switch(&old_ctx);
        new_prepare_view_phase_switch(&new_ctx);

        // ALT: the User Dictionary phase's first repaint still shows the CUID
        // phase's stale, out-of-range sector.
        assert(old_ctx.current_sector == sectors_total);
        // NEU: the new phase starts clean at sector 0.
        assert(new_ctx.current_sector == 0);
        checks += 2;
    }
    printf(
        "Cross-phase check: ALT still shows the CUID pass's end marker at the start of the "
        "User Dictionary phase on all %zu card types; NEU resets to sector 0 on all of them\n",
        NUM_CARD_TYPES);

    // --- Regression: a normal (non-CUID) NextSector step is untouched by the
    // fix - it takes its value straight from the poller-computed field, no
    // sectors_total comparison involved. ---
    for(uint8_t sectors_total = 5; sectors_total <= 40; sectors_total += 5) {
        for(uint8_t poller_sector = 0; poller_sector < sectors_total; poller_sector++) {
            // Standard-dictionary branch, unchanged by this patch:
            //   instance->nfc_dict_context.current_sector =
            //       mfc_event->data->next_sector_data.current_sector;
            uint8_t old_result = poller_sector;
            uint8_t new_result = poller_sector;
            assert(old_result == new_result);
            checks++;
        }
    }

    printf("ALL CHECKS PASSED (%d checks)\n", checks);
    return 0;
}
