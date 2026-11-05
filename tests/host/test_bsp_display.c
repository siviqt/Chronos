#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dsi_panel_nicolaielectronics_st7701.c"
#include "bsp_display_initialize_panel.inc"

static int bus_token, io_token, panel_token;
static bool bus_live, io_live, panel_live, dma_live, timed_out;
static int timeout_command, id_error, configured_vcom;
static uint8_t fixture_id[3];
static unsigned reads;
void test_bsp_log(const char* format, ...) { (void)format; }
const char* esp_err_to_name(esp_err_t e) { (void)e; return "injected"; }
void vTaskDelay(unsigned ms) { assert(ms == 100); }
bool mipi_dsi_hal_host_gen_read_timed_out(void) { return timed_out; }
esp_err_t esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t* c, void** out) {
    assert(!bus_live && c->bus_id == 0 && c->num_data_lanes == 2);
    bus_live = true; *out = &bus_token; return ESP_OK;
}
esp_err_t esp_lcd_new_panel_io_dbi(void* bus, const esp_lcd_dbi_io_config_t* c, void** out) {
    assert(bus_live && bus == &bus_token && !io_live && c->lcd_cmd_bits == 8);
    io_live = true; *out = &io_token; return ESP_OK;
}
esp_err_t esp_lcd_panel_io_rx_param(void* io, int command, void* out, size_t length) {
    assert(io == &io_token && io_live); ++reads;
    timed_out = command == timeout_command;
    if (command == LCD_CMD_RDDID) {
        assert(length == 3 && !panel_live);
        memcpy(out, fixture_id, 3);
        return id_error;
    }
    assert(command == LCD_CMD_ST7701_VCOMS && length == 1 && panel_live);
    *(uint8_t*)out = 0x46; return ESP_OK;
}
esp_err_t esp_lcd_panel_io_tx_param(void* io, int command, const void* data, size_t length) {
    assert(io_live && io == &io_token && data && length);
    (void)command; return ESP_OK;
}
esp_err_t esp_lcd_new_panel_st7701(void* io, const esp_lcd_panel_dev_config_t* c, void** out) {
    assert(io_live && io == &io_token && !panel_live && c->bits_per_pixel == 24);
    assert(c->reset_gpio_num == 14);
    const st7701_vendor_config_t* v = c->vendor_config;
    assert(v->mipi_config.dsi_bus == &bus_token && v->flags.use_mipi_interface);
    assert(v->mipi_config.dpi_config->num_fbs == 1);
    assert(v->mipi_config.dpi_config->video_timing.h_size == 480);
    assert(v->mipi_config.dpi_config->video_timing.v_size == 800);
    configured_vcom = -1;
    for (size_t i = 0; i < v->init_cmds_size; ++i)
        if (v->init_cmds[i].cmd == LCD_CMD_ST7701_VCOMS)
            configured_vcom = *(const uint8_t*)v->init_cmds[i].data;
    panel_live = true; *out = &panel_token; return ESP_OK;
}
esp_err_t esp_lcd_dpi_panel_enable_dma2d(void* p) { assert(panel_live && p == &panel_token); dma_live = true; return ESP_OK; }
esp_err_t esp_lcd_dpi_panel_disable_dma2d(void* p) { assert(dma_live && p == &panel_token); dma_live = false; return ESP_OK; }
esp_err_t esp_lcd_panel_del(void* p) { assert(panel_live && !dma_live && p == &panel_token); panel_live = false; return ESP_OK; }
esp_err_t esp_lcd_panel_io_del(void* io) { assert(!panel_live && io_live && io == &io_token); io_live = false; return ESP_OK; }
esp_err_t esp_lcd_del_dsi_bus(void* bus) { assert(!io_live && bus_live && bus == &bus_token); bus_live = false; return ESP_OK; }
esp_err_t esp_lcd_panel_reset(void* p) { assert(panel_live && p == &panel_token); return ESP_OK; }
esp_err_t esp_lcd_panel_init(void* p) { assert(panel_live && p == &panel_token); return ESP_OK; }

static void finish_success(void) {
    assert(panel_live && io_live && bus_live);
    esp_lcd_dpi_panel_disable_dma2d(mipi_dpi_panel);
    esp_lcd_panel_del(mipi_dpi_panel); mipi_dpi_panel = NULL;
    esp_lcd_panel_io_del(mipi_dbi_io); mipi_dbi_io = NULL;
    esp_lcd_del_dsi_bus(&bus_token);
}
int main(void) {
    const bsp_display_configuration_t config = {.requested_color_format = 24, .num_fbs = 1};
    uint8_t value, id[3];
    assert(st7701_initialize(NULL) == ESP_ERR_INVALID_ARG);
    timeout_command = LCD_CMD_RDDID;
    assert(bsp_display_initialize_panel(&config) == ESP_ERR_NOT_FOUND);
    assert(reads == 1 && !bus_live && !io_live && !panel_live);
    assert(st7701_get_panel_io() == NULL && st7701_get_id(id) == ESP_ERR_INVALID_STATE);
    timeout_command = -1;
    memcpy(fixture_id, (uint8_t[]){0x42, 0, 1}, 3);
    assert(bsp_display_initialize_panel(&config) == ESP_OK);
    assert(st7701_is_display_variant_2() && configured_vcom == 0x2e);
    assert(st7701_get_id(id) == ESP_OK && !memcmp(id, fixture_id, 3));
    assert(st7701_get_vcom_otp(&value) == ESP_OK && value == 0x46);
    finish_success();
    timeout_command = LCD_CMD_ST7701_VCOMS;
    assert(bsp_display_initialize_panel(&config) == ESP_ERR_NOT_FOUND);
    assert(!bus_live && !io_live && !panel_live && !dma_live);
    assert(!st7701_is_display_variant_2() && st7701_get_id(id) == ESP_ERR_INVALID_STATE);
    assert(st7701_get_vcom_otp(&value) == ESP_ERR_INVALID_STATE);
    timeout_command = -1;
    memset(fixture_id, 0x11, 3);
    assert(bsp_display_initialize_panel(&config) == ESP_OK);
    assert(!st7701_is_display_variant_2() && configured_vcom == 0x2d);
    finish_success();
    id_error = ESP_FAIL;
    assert(bsp_display_initialize_panel(&config) == ESP_OK);
    assert(st7701_get_id(id) == ESP_ERR_INVALID_STATE && configured_vcom == 0x2d);
    finish_success();
    puts("BSP display: ID/OTP timeout cleanup, error propagation, retry and both panel variants passed");
}
