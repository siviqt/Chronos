#include <assert.h>
#include <stdio.h>
#include "../../components/tanmatsu_platform/src/platform_display.c"
static int pixel, semaphore;
static bool in_flight, stall, destroyed, joined;
static esp_lcd_dpi_panel_event_callbacks_t callbacks;
esp_err_t bsp_display_get_parameters(size_t* w, size_t* h, int* f, int* e) { *w=480; *h=800; *f=24; *e=0; return ESP_OK; }
int bsp_display_get_default_rotation(void) { return BSP_DISPLAY_ROTATION_270; }
esp_err_t bsp_display_get_panel(void** p) { *p=&pixel; return ESP_OK; }
esp_err_t bsp_input_set_backlight_brightness(uint8_t p) { assert(p<=100); return ESP_OK; }
esp_err_t bsp_display_set_backlight_brightness(uint8_t p) { assert(p<=100); return ESP_OK; }
void* xSemaphoreCreateBinary(void) { return &semaphore; }
int xSemaphoreGiveFromISR(void* s, int* w) { assert(s==&semaphore); *w=1; return 1; }
int xSemaphoreTake(void* s, uint32_t timeout) {
    assert(s==&semaphore && timeout==1000);
    if (stall) return 0;
    assert(in_flight && !destroyed);
    assert(!platform_display_buffer()); /* Rendering is blocked during DMA. */
    in_flight=false;
    assert(callbacks.on_color_trans_done(panel, NULL, NULL));
    return 1;
}
void vSemaphoreDelete(void* s) { assert(s==&semaphore && !in_flight); }
esp_err_t esp_lcd_dpi_panel_register_event_callbacks(void* p, const esp_lcd_dpi_panel_event_callbacks_t* c, void* ctx) {
    (void)p; (void)ctx; callbacks=*c; return ESP_OK;
}
esp_err_t esp_lcd_panel_draw_bitmap(void* p, int x, int y, int w, int h, const void* pixels) {
    (void)p; assert(!in_flight && joined && x==0 && y==0 && w==480 && h==800 && pixels==&pixel);
    in_flight=true; return ESP_OK;
}
void pax_buf_init(pax_buf_t* b, void* p, size_t w, size_t h, int f) { (void)p;(void)w;(void)h;(void)f;b->pixels=&pixel;destroyed=false; }
void* pax_buf_get_pixels(pax_buf_t* b) { return b->pixels; }
void pax_buf_reversed(pax_buf_t* b, bool reverse) { (void)b;(void)reverse; }
void pax_buf_set_orientation(pax_buf_t* b, int o) { (void)b;(void)o; }
void pax_buf_destroy(pax_buf_t* b) { (void)b;assert(!in_flight);destroyed=true; }
void pax_join(void) { joined=true; }
int main(void) {
    assert(platform_display_init()==ESP_OK);
    assert(platform_display_buffer());
    assert(platform_display_present()==ESP_OK && !in_flight);
    assert(platform_illumination(100,100)==ESP_OK);
    assert(platform_illumination(0,0)==ESP_OK); /* F6 remains valid. */
    assert(platform_illumination(101,100)==ESP_ERR_INVALID_ARG);
    platform_display_shutdown(); assert(destroyed);
    assert(platform_display_init()==ESP_OK);
    stall=true;
    assert(platform_display_present()==ESP_ERR_TIMEOUT);
    assert(!platform_display_buffer());
    platform_display_shutdown(); assert(!destroyed); /* Never free live DMA source. */
    puts("display: completion fencing and timeout ownership passed");
}
