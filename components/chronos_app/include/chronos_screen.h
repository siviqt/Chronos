#pragma once
#include "chronos_runtime.h"
#include "chronos_ui.h"
void chronos_screen_view(chronos_ui_view_t* view, const chronos_ui_t* ui, chronos_runtime_t* runtime,
                         const chronos_settings_t* settings, const chronos_local_time_t* local, uint64_t now,
                         const char* notice, bool storage);
void chronos_screen_command(int command, chronos_ui_t* ui, chronos_runtime_t* runtime, chronos_settings_t* settings,
                            uint64_t now);

/* Runtime alarm shortcuts are dispatched before editor text. */
void chronos_screen_key(chronos_ui_key_t key, chronos_ui_t* ui, const chronos_ui_view_t* view,
                        chronos_runtime_t* runtime, chronos_settings_t* settings, uint64_t now);
void chronos_screen_text(unsigned key, chronos_ui_t* ui);
