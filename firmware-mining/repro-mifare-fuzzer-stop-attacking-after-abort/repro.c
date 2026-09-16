// Host repro: Mifare Fuzzer "Load UIDs from file" abort branch, follow-up
// to the already-shipped hang fix in
// firmware-mining/mifare-fuzzer-app-port/mifare_fuzzer_scene_emulator_c.patch.
//
// That patch stops the busy-loop hang when a file has no usable UID line,
// but it never resets MifareFuzzerEmulator::is_attacking (a plain field
// read directly by mifare_fuzzer_scene_emulator_on_event(), see
// SceneManagerEventTypeTick and the Increment/DecrementTicks branches) nor
// the view model's own is_attacking (private to
// views/mifare_fuzzer_emulator.c, only ever cleared via the OK-key toggle
// in mifare_fuzzer_emulator_input_callback() or on view (re-)entry in
// mifare_fuzzer_emulator_enter_callback() -- never from the scene handler).
//
// This models both fields plus the tick handler's re-trigger logic
// (SceneManagerEventTypeTick -> if(emulator->is_attacking) { ... enqueue
// another MifareFuzzerEventStartAttack once tick_counter reaches
// ticks_between_cards ... }) to show:
//   ALT: emulator->is_attacking stays true forever after the abort, so the
//        tick handler keeps re-enqueuing MifareFuzzerEventStartAttack, which
//        immediately re-hits the same abort branch every
//        ticks_between_cards ticks -- and the view model's is_attacking
//        stays true throughout, so the screen keeps showing "Stop" plus the
//        progress bar although nothing is actually running.
//   NEU: the abort branch also calls the new
//        mifare_fuzzer_emulator_stop_attacking() (added in
//        views/mifare_fuzzer_emulator.c by
//        mifare_fuzzer_emulator_c.patch/mifare_fuzzer_emulator_h.patch),
//        which clears both fields the same way
//        mifare_fuzzer_emulator_enter_callback() already does -- the tick
//        handler's guard then prevents any further auto re-trigger, and the
//        view model correctly reports "not attacking" again.
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    bool is_attacking; // MifareFuzzerEmulator::is_attacking
    unsigned ticks_between_cards;
} Emulator;

typedef struct {
    bool is_attacking; // MifareFuzzerEmulatorModel::is_attacking (view-local)
} Model;

// Mirrors mifare_fuzzer_emulator_stop_attacking() (NEU only).
static void emulator_stop_attacking(Emulator* emulator, Model* model) {
    emulator->is_attacking = false;
    model->is_attacking = false;
}

// Mirrors the file-mode abort branch inside
// mifare_fuzzer_scene_emulator_on_event(), case MifareFuzzerEventStartAttack,
// attack == MifareFuzzerAttackLoadUidsFromFile, "!found_valid_line" path.
// Always represents a file with zero usable lines (comment-only/empty/
// wrong-length-only), so this is reached on every StartAttack attempt.
// Returns true if the abort branch actually ran (for call counting).
static bool abort_branch(Emulator* emulator, Model* model, bool apply_followup_fix) {
    // iso14443_3a_free(nfc_data); notification_message(...) omitted: no
    // observable effect on is_attacking state, not relevant to this repro.
    if(apply_followup_fix) {
        emulator_stop_attacking(emulator, model);
    }
    return true;
}

// Mirrors SceneManagerEventTypeTick in mifare_fuzzer_scene_emulator_on_event():
// increments a per-tick counter while emulator->is_attacking, and once it
// reaches ticks_between_cards, "enqueues" (here: directly re-invokes)
// MifareFuzzerEventStartAttack, which -- for our all-invalid-file scenario --
// always lands back in abort_branch().
static unsigned simulate_ticks(
    Emulator* emulator,
    Model* model,
    bool apply_followup_fix,
    unsigned tick_windows) {
    unsigned abort_hits = 0;
    unsigned tick_counter = 0;

    for(unsigned t = 0; t < tick_windows; t++) {
        if(!emulator->is_attacking) continue; // tick handler's own guard
        tick_counter++;
        if(tick_counter >= emulator->ticks_between_cards) {
            tick_counter = 0;
            if(abort_branch(emulator, model, apply_followup_fix)) abort_hits++;
        }
    }
    return abort_hits;
}

static void run_case(const char* label, bool apply_followup_fix) {
    Emulator emulator = {.is_attacking = true, .ticks_between_cards = 5};
    Model model = {.is_attacking = true};

    // Initial manual "Start" attempt (OK key already set both is_attacking
    // fields true and sent MifareFuzzerEventStartAttack before the scene
    // handler runs) immediately hits the all-invalid-file abort branch once.
    unsigned abort_hits = abort_branch(&emulator, &model, apply_followup_fix) ? 1 : 0;

    // 47 simulated ticks at ticks_between_cards=5 -> 9 further windows where
    // the tick handler would re-fire MifareFuzzerEventStartAttack if it still
    // thinks an attack is running.
    abort_hits += simulate_ticks(&emulator, &model, apply_followup_fix, 47);

    printf(
        "[%s] abort branch hit %u time(s) over 47 ticks; "
        "emulator.is_attacking=%s model.is_attacking=%s (UI shows %s)\n",
        label,
        abort_hits,
        emulator.is_attacking ? "true" : "false",
        model.is_attacking ? "true" : "false",
        model.is_attacking ? "\"Stop\"+progress bar" : "\"Start\"/t-1/t+1");
}

int main(void) {
    run_case("ALT (hang fix only, no follow-up)", false);
    run_case("NEU (with mifare_fuzzer_emulator_stop_attacking())", true);
    return 0;
}
