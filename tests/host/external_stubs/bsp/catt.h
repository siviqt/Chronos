#pragma once
#include "driver/i2c_master.h"
esp_err_t bsp_catt_i2c_bus_get_handle(i2c_master_bus_handle_t* handle);
esp_err_t bsp_catt_set_i2c_enabled(bool enabled);
