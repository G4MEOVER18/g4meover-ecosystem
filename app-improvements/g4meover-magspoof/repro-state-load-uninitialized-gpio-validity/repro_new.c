// Host repro of mag_state.c :: mag_state_load(), NEU (fixed): same as repro_old.c but with
// `MagState state = {0};` at declaration, so mag_state_gpio_is_valid() always reads defined
// (zero) values, never uninitialized stack memory, regardless of which branch runs below.
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

bool mag_state_gpio_is_valid(MagState* state) {
    return (state->pin_input != state->pin_output) && (state->pin_input != state->pin_enable) &&
           (state->pin_enable != state->pin_output);
}

void mag_state_gpio_reset(MagState* state) {
    state->pin_input = MAG_STATE_DEFAULT_PIN_INPUT;
    state->pin_output = MAG_STATE_DEFAULT_PIN_OUTPUT;
    state->pin_enable = MAG_STATE_DEFAULT_PIN_ENABLE;
}

bool mag_state_load_neu(MagState* out_state, bool file_exists) {
    MagState state = {0}; // <-- FIX: zero-initialized, no undefined read below

    bool loaded_from_file = false;
    if(file_exists) {
        loaded_from_file = true;
        state.pin_input = MAG_STATE_DEFAULT_PIN_INPUT;
        state.pin_output = MAG_STATE_DEFAULT_PIN_OUTPUT;
        state.pin_enable = MAG_STATE_DEFAULT_PIN_ENABLE;
        state.allow_uart = MAG_STATE_DEFAULT_ALLOW_UART;
        state.n_repeats = MAG_STATE_DEFAULT_N_REPEATS;
        state.repeat_mode = MAG_STATE_DEFAULT_REPEAT_MODE;
    }

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
    bool loaded = mag_state_load_neu(&out, /*file_exists=*/false);
    printf(
        "NEU: loaded_from_file=%d pin_input=%d pin_output=%d pin_enable=%d\n",
        loaded,
        out.pin_input,
        out.pin_output,
        out.pin_enable);
    return 0;
}
