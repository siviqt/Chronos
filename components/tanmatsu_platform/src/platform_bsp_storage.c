#include "esp_err.h"
#include "esp_log.h"

/* BSP 2.0.0 eagerly reserves LDO4. Leave storage initialization to platform_sd
 * so radio power-off, standalone remounts and slow-card recovery retain one owner.
 * Deliberately do not call the BSP storage API elsewhere in the application. */
esp_err_t __wrap_bsp_storage_initialize(void) {
    ESP_LOGI("chronos_storage", "Application owns SD/LDO4; BSP storage initialization skipped");
    return ESP_OK;
}
