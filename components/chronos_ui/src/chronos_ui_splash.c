#include "chronos_ui_render.h"
#include "chronos_typography.h"
#include <string.h>

extern const uint8_t chronos_splash_pixels[420 * 420];
extern const pax_col_t chronos_splash_palette[256];

void chronos_ui_splash(pax_buf_t* buffer, const char* status) {
    if (!buffer) return;
    pax_background(buffer, 0xff000000);
    /* Flash-backed indexed artwork avoids a second full RGB framebuffer. */
    pax_buf_t logo;
    pax_buf_init(&logo, (void*)chronos_splash_pixels, 420, 420, PAX_BUF_8_PAL);
    logo.palette = chronos_splash_palette;
    logo.palette_size = 256;
    pax_draw_image(buffer, &logo, 190, 4);
    if (status) {
        pax_vec2f size = pax_text_size(CHRONOS_FONT_BODY, chronos_font_size(CHRONOS_FONT_BODY), status);
        pax_draw_text(buffer, 0xff78dce8, CHRONOS_FONT_BODY, chronos_font_size(CHRONOS_FONT_BODY),
                      (800 - size.x) / 2, 444, status);
    }
    if (status && !strcmp(status, "Press Space to continue")) {
        const char* keys = "F1 Exit app     F6 Lights     Side +/- Volume";
        pax_vec2f size = pax_text_size(CHRONOS_FONT_SECONDARY, chronos_font_size(CHRONOS_FONT_SECONDARY), keys);
        pax_draw_text(buffer, 0xff9bafb7, CHRONOS_FONT_SECONDARY, chronos_font_size(CHRONOS_FONT_SECONDARY),
                      (800 - size.x) / 2, 416, keys);
    }
    /* Finish reads before the stack-backed image descriptor goes away. */
    pax_join();
    pax_buf_destroy(&logo);
}
