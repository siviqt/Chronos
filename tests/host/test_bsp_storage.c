#include <assert.h>
#include <stdio.h>
#include "esp_err.h"

esp_err_t bsp_storage_initialize(void);

/* Exercise the same linker interception as firmware, not a direct wrapper call. */
int main(void) {
    assert(bsp_storage_initialize() == ESP_OK);
    puts("BSP storage: automatic LDO4 initialization intercepted by application");
    return 0;
}
