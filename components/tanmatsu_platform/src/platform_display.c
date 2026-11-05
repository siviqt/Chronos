#include "bsp/display.h"
#include "bsp/input.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/semphr.h"
#include "pax_gfx.h"
#include "tanmatsu_platform.h"
static pax_buf_t frame;
static esp_lcd_panel_handle_t panel;
static SemaphoreHandle_t flushed;
static bool pending;
static bool IRAM_ATTR transfer_done(esp_lcd_panel_handle_t p, esp_lcd_dpi_panel_event_data_t* e, void* ctx) {
    (void)p; (void)e; (void)ctx;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(flushed, &wake);
    return wake == pdTRUE;
}
static bool      ready;
static size_t    width, height;
esp_err_t        platform_display_init(void) {
    bsp_display_color_format_t format;
    bsp_display_endianness_t   endian;
    esp_err_t                  error = bsp_display_get_parameters(&width, &height, &format, &endian);
    if (error != ESP_OK) return error;
    bsp_display_rotation_t rotation = bsp_display_get_default_rotation();
    bool                   rotated = rotation == BSP_DISPLAY_ROTATION_90 || rotation == BSP_DISPLAY_ROTATION_270;
    if ((rotated ? height : width) != 800 || (rotated ? width : height) != 480 ||
        format != BSP_DISPLAY_COLOR_FORMAT_24_888RGB)
        return ESP_ERR_NOT_SUPPORTED;
    pax_buf_init(&frame, NULL, width, height, PAX_BUF_24_888RGB);
    if (!pax_buf_get_pixels(&frame)) return ESP_ERR_NO_MEM;
    pax_buf_reversed(&frame, endian == BSP_DISPLAY_ENDIAN_BIG);
    pax_buf_set_orientation(&frame, rotation == BSP_DISPLAY_ROTATION_90    ? PAX_O_ROT_CCW
                                           : rotation == BSP_DISPLAY_ROTATION_270 ? PAX_O_ROT_CW
                                           : rotation == BSP_DISPLAY_ROTATION_180 ? PAX_O_ROT_HALF
                                                                                  : PAX_O_UPRIGHT);
    error = bsp_display_get_panel(&panel);
    if (error != ESP_OK) { pax_buf_destroy(&frame); return error; }
    flushed = xSemaphoreCreateBinary();
    if (!flushed) { pax_buf_destroy(&frame); return ESP_ERR_NO_MEM; }
    /* Chronos exclusively owns presentation. The BSP waits BEFORE submitting
     * DMA, which does not protect the source from the next software render.
     * Own the completion callback and submit via the same IDF panel API. */
    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {.on_color_trans_done = transfer_done};
    error = esp_lcd_dpi_panel_register_event_callbacks(panel, &callbacks, NULL);
    if (error != ESP_OK) { vSemaphoreDelete(flushed); flushed = NULL; pax_buf_destroy(&frame); return error; }
    ready = true;
    return ESP_OK;
}
pax_buf_t* platform_display_buffer(void) {
    return ready && !pending ? &frame : NULL;
}
esp_err_t platform_display_present(void) {
    if (!ready || pending) return ESP_ERR_INVALID_STATE;
    pax_join();
    pending = true;
    esp_err_t error = esp_lcd_panel_draw_bitmap(panel, 0, 0, width, height, pax_buf_get_pixels(&frame));
    if (error != ESP_OK) { pending = false; return error; }
    if (xSemaphoreTake(flushed, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    pending = false;
    return ESP_OK;
}
void platform_display_shutdown(void) {
    /* On a failed DMA completion retain its source and callback until reboot. */
    if (pending && xSemaphoreTake(flushed, pdMS_TO_TICKS(1000)) != pdTRUE) { ready = false; return; }
    pending = false;
    if (ready) {
        const esp_lcd_dpi_panel_event_callbacks_t callbacks = {0};
        esp_lcd_dpi_panel_register_event_callbacks(panel, &callbacks, NULL);
        pax_buf_destroy(&frame);
        vSemaphoreDelete(flushed); flushed = NULL;
    }
    ready = false;
}

/* Keep illumination policy separate from status LEDs. */
esp_err_t platform_illumination(uint8_t keyboard, uint8_t display) {
    if (keyboard > 100 || display > 100) return ESP_ERR_INVALID_ARG;
    esp_err_t error = bsp_input_set_backlight_brightness(keyboard);
    esp_err_t screen = bsp_display_set_backlight_brightness(display);
    return error == ESP_OK ? screen : error;
}
