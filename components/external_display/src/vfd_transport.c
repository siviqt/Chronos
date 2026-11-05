// Reference: I2C_VFD/main.c and BSP 1.6.2 CATT API (MIT).
#include "bsp/catt.h"
#include "bsp/i2c.h"
#include "driver.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static SemaphoreHandle_t       lock;
static bool                    claim(void) {
    return !lock || xSemaphoreTake(lock, pdMS_TO_TICKS(20)) == pdTRUE;
}
static void release(void) {
    if (lock) xSemaphoreGive(lock);
}
void external_vfd_transport_close(void) {
    if (device) {
        i2c_master_bus_rm_device(device);
        device = NULL;
    }
    bus     = NULL;
    lock    = NULL;
}
esp_err_t external_vfd_transport_open(const external_config_t* c) {
    esp_err_t result = ESP_ERR_NOT_FOUND;
    for (unsigned candidate = EXT_BUS_CATT; candidate <= EXT_BUS_INTERNAL; ++candidate) {
        if (c->vfd_bus && c->vfd_bus != candidate) continue;
        if (candidate == EXT_BUS_INTERNAL && c->vfd_address == 0x10) continue;
        if (candidate == EXT_BUS_CATT) {
            /* BSP owns bus 1. Recover a failed startup enable only on this
             * explicit open/retry, and never delete the shared bus on close. */
            result = bsp_catt_i2c_bus_get_handle(&bus);
            if (result != ESP_OK) {
                result = bsp_catt_set_i2c_enabled(true);
                if (result == ESP_OK) result = bsp_catt_i2c_bus_get_handle(&bus);
            }
            if (result != ESP_OK || !bus) {
                bus = NULL;
                if (result == ESP_OK) result = ESP_ERR_INVALID_STATE;
                continue;
            }
        } else {
            result = bsp_i2c_primary_bus_get_handle(&bus);
            if (result == ESP_OK) result = bsp_i2c_primary_bus_get_semaphore(&lock);
            if (result != ESP_OK || !bus || !lock) {
                bus    = NULL;
                lock   = NULL;
                result = ESP_ERR_INVALID_STATE;
                continue;
            }
        }
        if (!claim()) {
            external_vfd_transport_close();
            result = ESP_ERR_TIMEOUT;
            continue;
        }
        result = i2c_master_probe(bus, c->vfd_address, 20);
        release();
        if (result == ESP_OK) {
            i2c_device_config_t cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = c->vfd_address, .scl_speed_hz = 100000};
            result = i2c_master_bus_add_device(bus, &cfg, &device);
            if (result == ESP_OK) return ESP_OK;
        }
        external_vfd_transport_close();
    }
    return result;
}
esp_err_t external_vfd_transport_write(const uint8_t* bytes, unsigned length) {
    if (!device) return ESP_ERR_INVALID_STATE;
    if (!claim()) return ESP_ERR_TIMEOUT;
    esp_err_t e = i2c_master_transmit(device, bytes, length, 50);
    release();
    return e;
}
