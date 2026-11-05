// Adapted from external_display_driver/I2C_VFD/main.c (MIT).
#include "driver.h"
static char      last[13];
static bool      presented, opened;
static esp_err_t reg(uint8_t address, uint8_t value) {
    uint8_t bytes[] = {address, value};
    return external_vfd_transport_write(bytes, sizeof(bytes));
}
static esp_err_t open_display(const external_config_t* c) {
    presented   = false;
    esp_err_t e = external_vfd_transport_open(c);
    if (e != ESP_OK) return e;
    opened = true;
    e      = reg(0, 1);              // System control: enable (not test or LED).
    if (e == ESP_OK) e = reg(3, 0);  // Disable any scrolling left by another application.
    if (e == ESP_OK) e = reg(1, 0);  // Reset display offset.
    if (e == ESP_OK) e = reg(6, c->vfd_brightness);
    return e;
}
static esp_err_t present(const external_frame_t* f) {
    char text[13];
    external_vfd_format(f, text);
    if (presented && !memcmp(text, last, 12)) return ESP_OK;
    uint8_t bytes[13] = {10};
    memcpy(bytes + 1, text, 12);
    esp_err_t e = external_vfd_transport_write(bytes, sizeof(bytes));
    if (e == ESP_OK) {
        memcpy(last, text, sizeof(last));
        presented = true;
    }
    return e;
}
static void close_display(void) {
    if (opened) {
        (void)reg(0, 0);
        external_vfd_transport_close();
    }
    opened = presented = false;
}
const external_driver_t external_vfd_driver = {open_display, present, close_display, EXT_VFD_READY};
