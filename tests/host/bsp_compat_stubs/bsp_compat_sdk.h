#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef int gpio_num_t;
typedef int lcd_color_format_t;
typedef void* esp_lcd_panel_handle_t;
typedef void* esp_lcd_panel_io_handle_t;
typedef void* esp_lcd_dsi_bus_handle_t;
enum { LCD_COLOR_FMT_RGB565 = 16, LCD_COLOR_FMT_RGB888 = 24,
       MIPI_DSI_DPI_CLK_SRC_DEFAULT = 0, LCD_RGB_ELEMENT_ORDER_RGB = 0,
       LCD_CMD_SLPOUT = 0x11, LCD_CMD_NORON = 0x13, LCD_CMD_DISPON = 0x29, LCD_CMD_MADCTL = 0x36,
       LCD_CMD_COLMOD = 0x3a, LCD_CMD_RDDID = 0x04,
       BSP_DISPLAY_COLOR_FORMAT_24_888RGB = 24 };
#define BSP_LCD_RESET_PIN 14
typedef struct { int requested_color_format; size_t num_fbs; } bsp_display_configuration_t;
typedef struct { int bus_id, num_data_lanes, lane_bit_rate_mbps; } esp_lcd_dsi_bus_config_t;
typedef struct { int virtual_channel, lcd_cmd_bits, lcd_param_bits; } esp_lcd_dbi_io_config_t;
typedef struct {
    int virtual_channel, dpi_clk_src, dpi_clock_freq_mhz, in_color_format;
    size_t num_fbs;
    struct { int h_size, v_size, hsync_back_porch, hsync_pulse_width, hsync_front_porch,
                 vsync_back_porch, vsync_pulse_width, vsync_front_porch; } video_timing;
} esp_lcd_dpi_panel_config_t;
typedef struct { int cmd; const void* data; size_t data_bytes; unsigned delay_ms; } st7701_lcd_init_cmd_t;
typedef struct {
    const st7701_lcd_init_cmd_t* init_cmds;
    size_t init_cmds_size;
    struct { esp_lcd_dsi_bus_handle_t dsi_bus; const esp_lcd_dpi_panel_config_t* dpi_config; } mipi_config;
    struct { bool use_mipi_interface; } flags;
} st7701_vendor_config_t;
typedef struct {
    int reset_gpio_num, rgb_ele_order, bits_per_pixel;
    const st7701_vendor_config_t* vendor_config;
} esp_lcd_panel_dev_config_t;

esp_err_t esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t*, esp_lcd_dsi_bus_handle_t*);
esp_err_t esp_lcd_del_dsi_bus(esp_lcd_dsi_bus_handle_t);
esp_err_t esp_lcd_new_panel_io_dbi(esp_lcd_dsi_bus_handle_t, const esp_lcd_dbi_io_config_t*, esp_lcd_panel_io_handle_t*);
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t);
esp_err_t esp_lcd_panel_io_rx_param(esp_lcd_panel_io_handle_t, int, void*, size_t);
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t, int, const void*, size_t);
esp_err_t esp_lcd_new_panel_st7701(esp_lcd_panel_io_handle_t, const esp_lcd_panel_dev_config_t*, esp_lcd_panel_handle_t*);
esp_err_t esp_lcd_dpi_panel_enable_dma2d(esp_lcd_panel_handle_t);
esp_err_t esp_lcd_dpi_panel_disable_dma2d(esp_lcd_panel_handle_t);
esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t);
esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t);
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t);
bool mipi_dsi_hal_host_gen_read_timed_out(void);
void vTaskDelay(unsigned);
void test_bsp_log(const char*, ...);
const char* esp_err_to_name(esp_err_t);
#define CONFIG_MIPI_DSI_ABSTRACTION_USE_VENDORED_ESP_HAL_LCD 1
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGI(tag, ...) do { (void)(tag); test_bsp_log(__VA_ARGS__); } while (0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { esp_err_t result_ = (expr); if (result_ != ESP_OK) return result_; } while (0)
