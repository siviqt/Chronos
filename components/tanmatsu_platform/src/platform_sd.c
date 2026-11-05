#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"
#include "tanmatsu_platform.h"

static sdmmc_card_t*        card;
static sd_pwr_ctrl_handle_t power;
static void*                dma_buffer;
static char                 directory[96];
static const char           mount_point[] = "/sd";
static char status[160] = "SD not mounted. Changes are session-only.";

const char* platform_sd_status(void) { return status; }
static esp_err_t failed(const char* stage, esp_err_t error) {
    snprintf(status, sizeof(status), "SD %s: %s. Changes are session-only.", stage, esp_err_to_name(error));
    return error;
}

/* When ESP-Hosted already initialized the controller, initialize only SD slot 0. */
static esp_err_t shared_host_init(void) {
    return ESP_OK;
}
/* The IDF ISR remembers the last slot used. Point it at live hosted slot 1
 * before removing slot 0, including IDF's failed-mount cleanup. Only shared
 * mounts install this callback; standalone mounts must never access the radio. */
static esp_err_t shared_host_deinit_slot(int slot) {
#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)
    if (slot != CONFIG_ESP_HOSTED_SDIO_SLOT && platform_clock_shared_controller()) {
        sdmmc_command_t command = {
            .opcode = SD_IO_RW_DIRECT,
            .arg = 0,
            .flags = SCF_CMD_AC | SCF_RSP_R5,
            .timeout_ms = 1000,
        };
        esp_err_t error = sdmmc_host_do_transaction(CONFIG_ESP_HOSTED_SDIO_SLOT, &command);
        if (error != ESP_OK)
            ESP_LOGW("chronos_sd", "Hosted slot handoff failed: %s", esp_err_to_name(error));
    }
#endif
    return sdmmc_host_deinit_slot(slot);
}
/* Validate a single application directory component before touching hardware. */
static bool directory_name_valid(const char* name) {
    if (!name || !name[0]) return false;
    for (size_t i = 0; name[i]; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (i >= 64 ||
            !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    }
    return true;
}
/* Power-cycle slot 0 using the board's LDO4 and GPIO39–44 wiring. */
static esp_err_t power_cycle(void) {
    gpio_config_t pins  = {.pin_bit_mask = UINT64_C(0x3f) << 39, .mode = GPIO_MODE_OUTPUT_OD};
    esp_err_t     error = gpio_config(&pins);
    if (error != ESP_OK) return error;
    for (int pin = 39; pin <= 44; ++pin) {
        error = gpio_set_level((gpio_num_t)pin, 0);
        if (error != ESP_OK) return error;
    }
    error = sd_pwr_ctrl_set_io_voltage(power, 0);
    if (error != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(150));
    pins.mode = GPIO_MODE_INPUT;
    error     = gpio_config(&pins);
    if (error != ESP_OK) return error;
    error = sd_pwr_ctrl_set_io_voltage(power, 3300);
    if (error != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(150));
    return ESP_OK;
}
/* Release only application-owned DMA and LDO resources after the slot is detached. */
static void release_resources(void) {
    free(dma_buffer);
    dma_buffer = NULL;
    if (power) {
        (void)sd_pwr_ctrl_del_on_chip_ldo(power);
        power = NULL;
    }
}
/* Mount existing FAT data, never format, then create the app directory on that volume. */
esp_err_t platform_sd_mount(const char* subdirectory, bool shared_controller) {
    if (!directory_name_valid(subdirectory)) return ESP_ERR_INVALID_ARG;
    char requested[sizeof(directory)];
    snprintf(requested, sizeof(requested), "%s/%s", mount_point, subdirectory);
    if (card) return !strcmp(requested, directory) ? ESP_OK : ESP_ERR_INVALID_STATE;
    bool      present = false;
    esp_err_t error   = platform_sd_present(&present);
    if (error != ESP_OK) return failed("detection", error);
    if (!present) {
        snprintf(status, sizeof(status), "SD card not detected. Changes are session-only.");
        return ESP_ERR_NOT_FOUND;
    }
    sd_pwr_ctrl_ldo_config_t ldo = {.ldo_chan_id = 4};
    error                        = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &power);
    if (error != ESP_OK) return failed("power setup", error);
    dma_buffer = heap_caps_malloc(2048, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!dma_buffer) {
        release_resources();
        return failed("DMA allocation", ESP_ERR_NO_MEM);
    }
    error = power_cycle();
    if (error != ESP_OK) {
        release_resources();
        return failed("power cycle", error);
    }
    sdmmc_host_t host       = SDMMC_HOST_DEFAULT();
    host.slot               = SDMMC_HOST_SLOT_0;
    host.max_freq_khz       = SDMMC_FREQ_DEFAULT;
    host.pwr_ctrl_handle    = power;
    host.dma_aligned_buffer = dma_buffer;
    if (shared_controller) {
        host.init = shared_host_init;
        host.deinit_p = shared_host_deinit_slot;
    }
    /* Retain slot-specific deinit from SDMMC_HOST_DEFAULT to protect radio slot 1. */
    sdmmc_slot_config_t slot                      = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk                                      = GPIO_NUM_43;
    slot.cmd                                      = GPIO_NUM_44;
    slot.d0                                       = GPIO_NUM_39;
    slot.d1                                       = GPIO_NUM_40;
    slot.d2                                       = GPIO_NUM_41;
    slot.d3                                       = GPIO_NUM_42;
    slot.width                                    = 4;
    const esp_vfs_fat_sdmmc_mount_config_t config = {
        .format_if_mount_failed = false, .max_files = 6, .allocation_unit_size = 16384};
    error = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot, &config, &card);
    /* Retry transient card/bus errors once using only D0 at the probing clock.
     * IDF already detached the failed slot; shared slot 1 remains owned by WiFi. */
    if (error == ESP_ERR_TIMEOUT || error == ESP_ERR_INVALID_RESPONSE || error == ESP_ERR_INVALID_CRC) {
        error = power_cycle();
        if (error == ESP_OK) {
            host.max_freq_khz = SDMMC_FREQ_PROBING;
            slot.width = 1;
            error = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot, &config, &card);
        }
    }
    if (error != ESP_OK) {
        card = NULL;
        release_resources();
        return failed("mount", error);
    }
    struct stat info;
    int path_error = 0;
    if (mkdir(requested, 0755) != 0 && errno != EEXIST) path_error = errno;
    else if (stat(requested, &info) != 0) path_error = errno;
    else if (!S_ISDIR(info.st_mode)) path_error = ENOTDIR;
    if (path_error) {
        (void)platform_sd_unmount();
        snprintf(status, sizeof(status), "SD directory: %s. Changes are session-only.", strerror(path_error));
        return ESP_FAIL;
    }
    strcpy(directory, requested);
    snprintf(status, sizeof(status), "SD mounted at %s", directory);
    return ESP_OK;
}
/* Detach only after all files close. IDF consumes the card/context even when
 * final VFS unregistration reports an error, so never reuse that handle. */
esp_err_t platform_sd_unmount(void) {
    if (!card) return ESP_OK;
    snprintf(status, sizeof(status), "SD not mounted. Changes are session-only.");
    directory[0]    = '\0';
    esp_err_t error = esp_vfs_fat_sdcard_unmount(mount_point, card);
    card            = NULL;
    release_resources();
    return error;
}
/* Expose the path only after both mounting and directory validation succeed. */
const char* platform_sd_directory(void) {
    return card && directory[0] ? directory : NULL;
}
