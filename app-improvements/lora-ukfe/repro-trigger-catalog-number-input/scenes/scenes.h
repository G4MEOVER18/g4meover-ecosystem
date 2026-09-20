// Mock-Ersatz fuer scenes/scenes.h -- nur der fuer scene_trigger_confirm.c
// (verbatim kopiert) benoetigte Ausschnitt: die Scene-ID, die es referenziert,
// sowie seine eigenen Handler-Deklarationen.
#pragma once
#include "../lora_ukfe.h"

enum { UkfeSceneLog = 5 };

void scene_trigger_confirm_on_enter(void* ctx);
bool scene_trigger_confirm_on_event(void* ctx, SceneManagerEvent event);
void scene_trigger_confirm_on_exit(void* ctx);
