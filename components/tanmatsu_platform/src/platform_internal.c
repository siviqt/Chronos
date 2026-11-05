#include "esp_vfs_fat.h"
#include "tanmatsu_platform.h"
#include "wear_levelling.h"

static wl_handle_t internal_handle = WL_INVALID_HANDLE;

/* Match the launcher's existing locfd filesystem; never format on failure. */
esp_err_t platform_internal_mount(void) {
    if (internal_handle != WL_INVALID_HANDLE) return ESP_OK;
    const esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };
    wl_handle_t handle = WL_INVALID_HANDLE;
    esp_err_t error = esp_vfs_fat_spiflash_mount_rw_wl("/int", "locfd", &config, &handle);
    if (error == ESP_OK) internal_handle = handle;
    else if (handle != WL_INVALID_HANDLE) (void)wl_unmount(handle);
    return error;
}

esp_err_t platform_internal_unmount(void) {
    if (internal_handle == WL_INVALID_HANDLE) return ESP_OK;
    esp_err_t error = esp_vfs_fat_spiflash_unmount_rw_wl("/int", internal_handle);
    internal_handle = WL_INVALID_HANDLE;
    return error;
}
