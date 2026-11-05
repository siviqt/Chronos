#pragma once
#include "esp_err.h"
#define BSP_POWER_RADIO_STATE_OFF 0
esp_err_t bsp_power_set_radio_state(int state);
