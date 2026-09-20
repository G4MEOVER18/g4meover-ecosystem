// Mock-Ersatz fuer lora_ukfe.h -- stellt nur die Typen/Funktionssignaturen
// bereit, die scenes/scene_payload_run_confirm.c (verbatim aus lora-ukfe
// kopiert) tatsaechlich braucht. Die echte lora_ukfe.h haengt an
// <furi.h>/<gui/...>, die auf dem Host nicht verfuegbar sind -- ukfe_rf.h
// dagegen ist reine Protokolllogik ohne furi-Abhaengigkeit und wird deshalb
// unveraendert eingebunden. 1:1 identisch zum Mock in
// repro-trigger-catalog-number-input/ (dieselbe App-Struktur, dieselben
// GUI-Bausteine).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "rf/ukfe_rf.h"

#define UNUSED(x) (void)(x)

// ---- NumberInput (gui/modules/number_input.h in der echten Firmware) ----
typedef struct NumberInput NumberInput;
typedef void (*NumberInputCallback)(void* context, int32_t number);
void number_input_set_header_text(NumberInput* number_input, const char* text);
void number_input_set_result_callback(
    NumberInput* number_input,
    NumberInputCallback callback,
    void* callback_context,
    int32_t current_number,
    int32_t min_value,
    int32_t max_value);

// ---- SceneManager / ViewDispatcher (gui/scene_manager.h, gui/view_dispatcher.h) ----
typedef struct SceneManager SceneManager;
void scene_manager_next_scene(SceneManager* scene_manager, uint32_t next_scene_id);

typedef struct ViewDispatcher ViewDispatcher;
void view_dispatcher_switch_to_view(ViewDispatcher* view_dispatcher, uint32_t view_id);

typedef enum {
    SceneManagerEventTypeCustom,
    SceneManagerEventTypeBack,
    SceneManagerEventTypeTick,
} SceneManagerEventType;

typedef struct {
    SceneManagerEventType type;
    uint32_t              event;
} SceneManagerEvent;

// ---- Views (UkfeView-Ausschnitt aus der echten lora_ukfe.h) ----
enum { UkfeViewNumberInput = 5 };

// ---- App-State (nur die von scene_payload_run_confirm.c genutzten Felder) ----
typedef struct {
    NumberInput*    number_input;
    SceneManager*   scene_manager;
    ViewDispatcher* view_dispatcher;
} LораUkfeApp;

void ukfe_uart_send_rf(LораUkfeApp* app, const UkfeRfMessage* msg);
