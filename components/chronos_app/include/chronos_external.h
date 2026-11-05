#pragma once
#include "chronos_runtime.h"
#include "external_display.h"
/* No tab focus participates in either selection or information projection. */
unsigned chronos_external_source(const external_config_t* config, const chronos_runtime_t* runtime);
void     chronos_external_frame(external_frame_t* frame, const chronos_settings_t* settings,
                                const chronos_runtime_t* runtime, const chronos_local_time_t* local,
                                const chronos_local_time_t* world, uint64_t now);
/* Called by the application owner; handles timezone conversion before dispatch. */
void     chronos_external_update(const chronos_settings_t* settings, const chronos_runtime_t* runtime,
                                 const chronos_local_time_t* local, int64_t epoch, uint64_t now, bool retry);
