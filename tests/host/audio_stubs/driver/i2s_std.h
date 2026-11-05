#pragma once
#include <stddef.h>
#include <stdint.h>
#include "i2s_types.h"
#include "esp_err.h"
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
esp_err_t i2s_channel_disable(i2s_chan_handle_t);
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, const void*, size_t, size_t*);
esp_err_t i2s_channel_write(i2s_chan_handle_t, const void*, size_t, size_t*, uint32_t);
