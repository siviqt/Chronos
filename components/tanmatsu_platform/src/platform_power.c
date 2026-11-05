#include <math.h>
#include "bsp/power.h"
#include "tanmatsu_platform.h"

esp_err_t platform_battery_percent(uint8_t* percent) {
    if (!percent) return ESP_ERR_INVALID_ARG;
    bsp_power_battery_information_t information = {0};
    esp_err_t error = bsp_power_get_battery_information(&information);
    if (error != ESP_OK) return error;
    if (!information.battery_available) return ESP_ERR_NOT_FOUND;
    double value = information.remaining_percentage;
    if (!isfinite(value)) return ESP_ERR_INVALID_STATE;
    /* Round upward so each nonzero capacity belongs to its containing 20% band. */
    *percent = value <= 0 ? 0 : value >= 100 ? 100 : (uint8_t)ceil(value);
    return ESP_OK;
}
