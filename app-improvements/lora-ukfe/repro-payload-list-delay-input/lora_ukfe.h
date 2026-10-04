// Mock-Ersatz fuer lora_ukfe.h -- stellt nur die Typen/Funktionssignaturen
// bereit, die scenes/scene_payload_list.c (verbatim aus lora-ukfe kopiert,
// nach Anwendung von trigger_catalog_number_input.patch UND
// payload_list_delay_input.patch) tatsaechlich braucht. Die echte
// lora_ukfe.h haengt an <furi.h>/<gui/...>, die auf dem Host nicht verfuegbar
// sind. 1:1 nach demselben Muster wie in repro-trigger-catalog-number-input/
// und repro-payload-run-catalog-number-input/ (dieselbe App-Struktur,
// dieselben GUI-Bausteine), hier um Submenu/Payload-Liste ergaenzt.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h> // snprintf -- von build_list() in scene_payload_list.c genutzt;
                   // in der echten Firmware stellt <furi.h> das transitiv bereit.

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

// ---- Submenu (gui/modules/submenu.h in der echten Firmware) ----
typedef struct Submenu Submenu;
typedef void (*SubmenuItemCallback)(void* context, uint32_t index);
void submenu_reset(Submenu* submenu);
void submenu_set_header(Submenu* submenu, const char* header);
void submenu_add_item(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallback callback,
    void* callback_context);

// ---- SceneManager / ViewDispatcher (gui/scene_manager.h, gui/view_dispatcher.h) ----
typedef struct SceneManager SceneManager;
void scene_manager_previous_scene(SceneManager* scene_manager);

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
enum {
    UkfeViewMenu = 0,
    UkfeViewNumberInput = 5,
};

// ---- Payload-Liste (UkfePayloadEntry/UKFE_MAX_PAYLOADS aus der echten lora_ukfe.h) ----
typedef struct {
    uint8_t id;
    char    name[48];
} UkfePayloadEntry;

#define UKFE_MAX_PAYLOADS 32

// ---- App-State (nur die von scene_payload_list.c genutzten Felder) ----
typedef struct {
    Submenu*         submenu;
    NumberInput*     number_input;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    UkfePayloadEntry payloads[UKFE_MAX_PAYLOADS];
    uint8_t          payload_count;
} LораUkfeApp;

// uart_comm.c (unveraendert) -- hier als aufzeichnender Stub nachgebildet,
// exakt wie ukfe_uart_send_rf() es in den vorherigen zwei lora-ukfe-Repros
// dieses Verzeichnisses war: die echte Implementierung haengt an
// furi_hal_serial_tx()/ukfe_log_append() (furi-Abhaengigkeit), ihr JSON-Format
// ist bereits an anderer Stelle dokumentiert
// (app-improvements/lora-ukfe/trigger-catalog-number-input-port.md) und wird
// hier nicht erneut nachgebaut -- getestet wird ausschliesslich, dass
// scene_payload_list.c sie mit der richtigen Payload-ID und dem richtigen
// delay_ms aufruft.
void ukfe_uart_send_trigger(LораUkfeApp* app, uint8_t id, uint32_t delay_ms);
void ukfe_uart_send_payload_list(LораUkfeApp* app);
