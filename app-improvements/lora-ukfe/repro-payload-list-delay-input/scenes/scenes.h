// Mock-Ersatz fuer scenes/scenes.h -- nur der fuer scene_payload_list.c
// (verbatim kopiert) benoetigte Ausschnitt: seine eigenen
// Handler-Deklarationen. scene_payload_list.c navigiert ausschliesslich ueber
// scene_manager_previous_scene() (kein next_scene zu einer anderen
// Scene-ID), daher sind hier keine weiteren Scene-IDs noetig.
#pragma once
#include "../lora_ukfe.h"

void scene_payload_list_on_enter(void* ctx);
bool scene_payload_list_on_event(void* ctx, SceneManagerEvent event);
void scene_payload_list_on_exit(void* ctx);
