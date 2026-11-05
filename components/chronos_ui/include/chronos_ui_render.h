#pragma once
#include "chronos_ui.h"
#include "pax_gfx.h"
void chronos_ui_render(pax_buf_t* buffer, const chronos_ui_t* ui, const chronos_ui_view_t* view);
/* Full-screen startup page; status describes the service currently loading. */
void chronos_ui_splash(pax_buf_t* buffer, const char* status);
