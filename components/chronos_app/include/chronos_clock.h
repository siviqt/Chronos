#pragma once
#include "chronos_settings.h"
#include "chronos_ui.h"
/* Called only by the application owner: POSIX TZ is process-global. Always
 * restore the configured local zone before alarm conversion resumes. */
bool chronos_clock_local(int64_t seconds, const char* zone, chronos_local_time_t* local, bool* dst);
void chronos_clock_view(chronos_ui_view_t* view, const chronos_ui_t* ui,
                        const chronos_settings_t* settings, bool valid, int64_t seconds, const char* status);
#define CHRONOS_CLOCK_SYNC 20

/* Parse local Gregorian YYYY-MM-DD HH:MM:SS, rejecting normalized DST gaps. */
bool chronos_clock_parse_manual(const char* text, const char* zone, int64_t* seconds);
