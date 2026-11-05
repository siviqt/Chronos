#pragma once
#include "esp_err.h"
#include "external_display.h"
/* Lifecycle is owned exclusively by the display worker. */
typedef struct {
    esp_err_t (*open)(const external_config_t* config);
    esp_err_t (*present)(const external_frame_t* frame);
    void (*close)(void);
    external_status_t ready;
} external_driver_t;
extern const external_driver_t external_nokia_driver, external_vfd_driver;
/* VFD transport owns only its device and (when created here) CATT bus. */
esp_err_t                      external_vfd_transport_open(const external_config_t* config);
esp_err_t                      external_vfd_transport_write(const uint8_t* bytes, unsigned length);
void                           external_vfd_transport_close(void);
