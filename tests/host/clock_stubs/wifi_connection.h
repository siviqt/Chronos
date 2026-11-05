#pragma once
#include <stdbool.h>
#include "esp_err.h"
esp_err_t wifi_connection_init_stack(void);
bool wifi_connection_is_connected(void);
esp_err_t wifi_connection_connect(uint16_t index, uint8_t retries);
void wifi_connection_disconnect(void);
