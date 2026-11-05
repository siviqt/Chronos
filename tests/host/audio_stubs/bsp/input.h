#pragma once
#include <stdbool.h>
#include "esp_err.h"
#define BSP_INPUT_ACTION_TYPE_AUDIO_JACK 1
esp_err_t bsp_input_read_action(int, bool*);
