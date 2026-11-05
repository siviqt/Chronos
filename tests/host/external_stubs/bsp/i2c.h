#pragma once
#include "driver/i2c_master.h"
#include "freertos/semphr.h"
esp_err_t bsp_i2c_primary_bus_get_handle(i2c_master_bus_handle_t* handle);
esp_err_t bsp_i2c_primary_bus_get_semaphore(SemaphoreHandle_t* handle);
