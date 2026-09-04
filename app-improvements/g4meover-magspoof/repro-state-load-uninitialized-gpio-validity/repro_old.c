// Host repro of mag_state.c :: mag_state_load() control flow (ALT / current upstream).
// Faithful re-implementation of the pin fields + mag_state_gpio_is_valid/reset logic from
// helpers/mag_types.h and mag_state.c, with the Flipper-specific storage/flipper_format calls
// replaced by a `file_exists` flag that selects the same branch the real code would take when
// the state file is missing (first app run / fresh SD card) or its header is unreadable.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    MagPinA7,
    MagPinA6,
    MagPinA4,
    MagPinB3,
    MagPinB2,
    MagPinC3,
    MagPinC1,
    MagPinC0,
} MagPin;

#define MAG_STATE_DEFAULT_PIN_INPUT MagPinA7
#define MAG_STATE_DEFAULT_PIN_OUTPUT MagPinA6
#define MAG_STATE_DEFAULT_PIN_ENABLE MagPinA4
#define MAG_STATE_DEFAULT_ALLOW_UART false
#define MAG_STATE_DEFAULT_N_REPEATS 3
#define MAG_STATE_DEFAULT_REPEAT_MODE true

typedef struct {
    MagPin pin_input;
    MagPin pin_output;
    MagPin pin_enable;
    bool allow_uart;
    uint8_t n_repeats;
    bool repeat_mode;
} MagState;

// verbatim from mag_state.c
bool mag_state_gpio_is_valid(MagState* state) {
    return (state->pin_input != state->pin_output) && (state->pin_input != state->pin_enable) &&
           (state->pin_enable != state->pin_output);
}

// verbatim from mag_state.c
void mag_state_gpio_reset(MagState* state) {
    state->pin_input = MAG_STATE_DEFAULT_PIN_INPUT;
    state->pin_output = MAG_STATE_DEFAULT_PIN_OUTPUT;
    state->pin_enable = MAG_STATE_DEFAULT_PIN_ENABLE;
}

// ALT: mirrors mag_state_load() exactly, `file_exists` stands in for
// storage_file_exists(storage, MAG_STATE_PATH); when it is false, the whole
// `if(storage_file_exists(...)) { ... }` block (the do-while that assigns every field) is
// skipped entirely, exactly as in the real function.
bool mag_state_load_alt(MagState* out_state, bool file_exists) {
    MagState state; // <-- BUG: declared without an initializer

    bool loaded_from_file = false;
    if(file_exists) {
        // (do-while field-read loop omitted: irrelevant to this repro, since we drive
        // the file_exists=false path that skips it entirely, same as a fresh SD card)
        loaded_from_file = true;
        state.pin_input = MAG_STATE_DEFAULT_PIN_INPUT;
        state.pin_output = MAG_STATE_DEFAULT_PIN_OUTPUT;
        state.pin_enable = MAG_STATE_DEFAULT_PIN_ENABLE;
        state.allow_uart = MAG_STATE_DEFAULT_ALLOW_UART;
        state.n_repeats = MAG_STATE_DEFAULT_N_REPEATS;
        state.repeat_mode = MAG_STATE_DEFAULT_REPEAT_MODE;
    }

    // If file's GPIO config is invalid (pins overlap), reset to defaults.
    // <-- reads state.pin_input/output/enable BEFORE they are ever assigned when
    //     file_exists == false (first run / missing state file).
    if(!mag_state_gpio_is_valid(&state)) {
        mag_state_gpio_reset(&state);
    }

    if(!loaded_from_file) {
        mag_state_gpio_reset(&state);
        state.allow_uart = MAG_STATE_DEFAULT_ALLOW_UART;
        state.n_repeats = MAG_STATE_DEFAULT_N_REPEATS;
        state.repeat_mode = MAG_STATE_DEFAULT_REPEAT_MODE;
    }

    memcpy(out_state, &state, sizeof(state));
    return loaded_from_file;
}

int main(void) {
    MagState out;
    bool loaded = mag_state_load_alt(&out, /*file_exists=*/false);
    printf(
        "ALT: loaded_from_file=%d pin_input=%d pin_output=%d pin_enable=%d\n",
        loaded,
        out.pin_input,
        out.pin_output,
        out.pin_enable);
    return 0;
}
