#pragma once
#include "esp_err.h"
#define BSP_DISPLAY_COLOR_FORMAT_24_888RGB 1
typedef struct { struct { int requested_color_format; int num_fbs; } display; } bsp_configuration_t;
esp_err_t bsp_device_initialize(const bsp_configuration_t* config);
void bsp_device_restart_to_launcher(void);
