#include <stdlib.h>
#include "esp_err.h"

/* A missing link flag would execute the BSP's conflicting LDO4 acquisition. */
esp_err_t bsp_storage_initialize(void) {
    abort();
}
