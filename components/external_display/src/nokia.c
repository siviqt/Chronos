#include "driver.h"
#include "nokia5110_espidf.h"
static nokia5110_espidf_t lcd;
static nokia5110_canvas_t last;
static bool               presented;
static esp_err_t          open_display(const external_config_t* c) {
    presented             = false;
    nokia5110_pins_t pins = {.clk = c->clk, .din = c->din, .ce = c->ce, .dc = c->dc, .rst = c->rst};
    return nokia5110_espidf_open(&lcd, pins, c->contrast) ? ESP_FAIL : ESP_OK;
}
static esp_err_t present(const external_frame_t* f) {
    nokia5110_canvas_t canvas;
    nokia5110_clear(&canvas, false);
    /* Four independently clipped lines, 14 characters each. */
    const char* lines[] = {f->title, f->value, f->detail, f->extra};
    for (unsigned i = 0; i < 4; ++i) {
        char   line[15];
        size_t n = strlen(lines[i]);
        if (n > 14) n = 14;
        memcpy(line, lines[i], n);
        line[n] = 0;
        nokia5110_text(&canvas, 0, (int)i * 12, line, true);
    }
    if (presented && !memcmp(&canvas, &last, sizeof(canvas))) return ESP_OK;
    if (nokia5110_flush(&lcd.display, &canvas)) return ESP_FAIL;
    last      = canvas;
    presented = true;
    return ESP_OK;
}
static void close_display(void) {
    (void)nokia5110_espidf_close(&lcd);
    presented = false;
}
const external_driver_t external_nokia_driver = {open_display, present, close_display, EXT_NOKIA_READY};
