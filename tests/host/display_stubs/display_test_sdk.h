#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#define IRAM_ATTR
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define PAX_BUF_24_888RGB 24
#define PAX_O_ROT_CCW 1
#define PAX_O_ROT_CW 2
#define PAX_O_ROT_HALF 3
#define PAX_O_UPRIGHT 0
#define BSP_DISPLAY_COLOR_FORMAT_24_888RGB 24
#define BSP_DISPLAY_ENDIAN_BIG 1
#define BSP_DISPLAY_ROTATION_90 1
#define BSP_DISPLAY_ROTATION_270 2
#define BSP_DISPLAY_ROTATION_180 3
typedef int BaseType_t;
typedef int bsp_display_color_format_t;
typedef int bsp_display_endianness_t;
typedef int bsp_display_rotation_t;
typedef void* SemaphoreHandle_t;
typedef void* esp_lcd_panel_handle_t;
typedef struct { int unused; } esp_lcd_dpi_panel_event_data_t;
typedef struct pax_buf { void* pixels; } pax_buf_t;
typedef struct {
    bool (*on_color_trans_done)(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void*);
} esp_lcd_dpi_panel_event_callbacks_t;
esp_err_t bsp_display_get_parameters(size_t*, size_t*, bsp_display_color_format_t*, bsp_display_endianness_t*);
bsp_display_rotation_t bsp_display_get_default_rotation(void);
esp_err_t bsp_display_get_panel(esp_lcd_panel_handle_t*);
esp_err_t bsp_input_set_backlight_brightness(uint8_t);
esp_err_t bsp_display_set_backlight_brightness(uint8_t);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t*);
int xSemaphoreTake(SemaphoreHandle_t, uint32_t);
void vSemaphoreDelete(SemaphoreHandle_t);
esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t, const esp_lcd_dpi_panel_event_callbacks_t*, void*);
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t, int, int, int, int, const void*);
void pax_buf_init(pax_buf_t*, void*, size_t, size_t, int);
void* pax_buf_get_pixels(pax_buf_t*);
void pax_buf_reversed(pax_buf_t*, bool);
void pax_buf_set_orientation(pax_buf_t*, int);
void pax_buf_destroy(pax_buf_t*);
void pax_join(void);
