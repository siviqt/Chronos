#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef struct { int unused; } wifi_ap_record_t;
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t* ap);
typedef struct { bool nvs_enable; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){.nvs_enable = true})
esp_err_t esp_wifi_init(const wifi_init_config_t* config);
esp_err_t esp_wifi_deinit(void);
