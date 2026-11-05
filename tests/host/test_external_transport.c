#include <assert.h>
#include <stdio.h>
#include "../../components/external_display/src/vfd_transport.c"
static int       catt, internal, dev, mutex;
static unsigned  probes, enabled, removed, takes, gives, transfers;
static bool      busy, catt_missing, internal_missing;
static esp_err_t catt_result = ESP_ERR_NOT_FOUND, internal_result, transfer_result, add_result, enable_result;
int              xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
    assert(s == &mutex && wait == 20);
    ++takes;
    return !busy;
}
int xSemaphoreGive(SemaphoreHandle_t s) {
    assert(s == &mutex);
    ++gives;
    return 1;
}
esp_err_t bsp_i2c_primary_bus_get_handle(i2c_master_bus_handle_t* h) {
    *h = internal_missing ? NULL : &internal;
    return ESP_OK;
}
esp_err_t bsp_i2c_primary_bus_get_semaphore(SemaphoreHandle_t* h) {
    *h = &mutex;
    return ESP_OK;
}
/* Model a BSP-owned bus and an explicit retry after startup failure. */
esp_err_t bsp_catt_i2c_bus_get_handle(i2c_master_bus_handle_t* h) {
    *h = catt_missing ? NULL : &catt;
    return catt_missing ? ESP_ERR_INVALID_STATE : ESP_OK;
}
esp_err_t bsp_catt_set_i2c_enabled(bool enable) {
    assert(enable);
    ++enabled;
    if (enable_result == ESP_OK) catt_missing = false;
    return enable_result;
}
/* A borrowed BSP bus must never be removed by the transport. */
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t h) {
    (void)h;
    assert(!"transport deleted BSP bus");
    return ESP_FAIL;
}
esp_err_t i2c_master_probe(i2c_master_bus_handle_t h, unsigned address, int timeout) {
    assert(timeout == 20 && (address == 0x13 || (h == &catt && address == 0x10)));
    ++probes;
    return h == &catt ? catt_result : internal_result;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t h, const i2c_device_config_t* c,
                                    i2c_master_dev_handle_t* d) {
    assert(h && c->scl_speed_hz == 100000 && c->dev_addr_length == I2C_ADDR_BIT_LEN_7);
    if (add_result != ESP_OK) return add_result;
    *d = &dev;
    return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d) {
    assert(d == &dev);
    ++removed;
    return ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t* b, unsigned n, int timeout) {
    assert(d == &dev && b && n && timeout == 50);
    ++transfers;
    return transfer_result;
}
int main(void) {
    external_config_t c = EXTERNAL_CONFIG_DEFAULT;
    assert(external_vfd_transport_open(&c) == ESP_OK);
    assert(probes == 2 && enabled == 0 && takes == 1 && gives == 1);
    uint8_t data[] = {0, 1};
    assert(external_vfd_transport_write(data, 2) == ESP_OK);
    assert(takes == 2 && gives == 2 && transfers == 1);
    transfer_result = ESP_ERR_TIMEOUT;
    assert(external_vfd_transport_write(data, 2) == ESP_ERR_TIMEOUT);
    assert(takes == gives);
    external_vfd_transport_close();
    assert(removed == 1 && enabled == 0);
    c.vfd_bus   = EXT_BUS_CATT;
    catt_result = ESP_OK;
    assert(external_vfd_transport_open(&c) == ESP_OK);
    external_vfd_transport_close();
    assert(removed == 2 && enabled == 0);
    catt_missing = true;
    enable_result = ESP_ERR_INVALID_STATE;
    unsigned before = probes;
    assert(external_vfd_transport_open(&c) == ESP_ERR_INVALID_STATE);
    assert(enabled == 1 && probes == before && !bus);
    enable_result = ESP_OK;
    assert(external_vfd_transport_open(&c) == ESP_OK);
    assert(enabled == 2 && !catt_missing);
    external_vfd_transport_close();
    c.vfd_bus       = EXT_BUS_INTERNAL;
    busy            = true;
    before = probes;
    assert(external_vfd_transport_open(&c) == ESP_ERR_TIMEOUT);
    assert(probes == before);
    busy             = false;
    internal_missing = true;
    assert(external_vfd_transport_open(&c) == ESP_ERR_INVALID_STATE);
    internal_missing = false;
    c.vfd_bus        = EXT_BUS_AUTO;
    c.vfd_address    = 0x10;
    catt_result      = ESP_ERR_NOT_FOUND;
    before           = probes;
    assert(external_vfd_transport_open(&c) == ESP_ERR_NOT_FOUND);
    assert(probes == before + 1);
    c.vfd_bus     = EXT_BUS_CATT;
    c.vfd_address = 0x13;
    catt_result   = ESP_OK;
    add_result    = ESP_ERR_NO_MEM;
    assert(external_vfd_transport_open(&c) == ESP_ERR_NO_MEM && !bus && !device);
    assert(enabled == 2);
    puts("VFD transport: board wiring, bounded address-only probes, shared locking, BSP ownership, retry and cleanup passed");
}
