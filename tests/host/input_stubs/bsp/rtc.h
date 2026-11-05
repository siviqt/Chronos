#pragma once
#include <stdint.h>
#include "esp_err.h"
esp_err_t bsp_rtc_get_time(uint32_t* seconds);
