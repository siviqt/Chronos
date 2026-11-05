#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include "esp_err.h"
typedef int gpio_num_t;
enum {
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT_OD
};
enum {
    GPIO_NUM_39 = 39,
    GPIO_NUM_40,
    GPIO_NUM_41,
    GPIO_NUM_42,
    GPIO_NUM_43,
    GPIO_NUM_44
};
typedef struct {
    uint64_t pin_bit_mask;
    int      mode;
} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t* config);
esp_err_t gpio_set_level(gpio_num_t pin, int level);
#define pdMS_TO_TICKS(value) (value)
void vTaskDelay(unsigned int ticks);
#define MALLOC_CAP_DMA      1
#define MALLOC_CAP_INTERNAL 2
void*         heap_caps_malloc(size_t size, int capabilities);
typedef void* sd_pwr_ctrl_handle_t;
typedef struct {
    int ldo_chan_id;
} sd_pwr_ctrl_ldo_config_t;
esp_err_t sd_pwr_ctrl_new_on_chip_ldo(const sd_pwr_ctrl_ldo_config_t* config, sd_pwr_ctrl_handle_t* handle);
esp_err_t sd_pwr_ctrl_del_on_chip_ldo(sd_pwr_ctrl_handle_t handle);
esp_err_t sd_pwr_ctrl_set_io_voltage(sd_pwr_ctrl_handle_t handle, int voltage);
typedef struct {
    int marker;
} sdmmc_card_t;
typedef struct {
    int   slot, max_freq_khz, flags;
    void* pwr_ctrl_handle;
    void* dma_aligned_buffer;
    esp_err_t (*init)(void);
    esp_err_t (*deinit_p)(int slot);
} sdmmc_host_t;
esp_err_t sdmmc_host_init(void);
esp_err_t sdmmc_host_deinit_slot(int slot);
typedef struct {
    int opcode, arg, flags, timeout_ms;
} sdmmc_command_t;
esp_err_t sdmmc_host_do_transaction(int slot, sdmmc_command_t* command);
#define SDMMC_HOST_FLAG_DEINIT_ARG 1
#define SDMMC_HOST_SLOT_0          0
#define SDMMC_FREQ_DEFAULT         20000
#define SDMMC_FREQ_PROBING         400
#define SDMMC_HOST_DEFAULT() \
    {.flags = SDMMC_HOST_FLAG_DEINIT_ARG, .init = sdmmc_host_init, .deinit_p = sdmmc_host_deinit_slot}
typedef struct {
    int clk, cmd, d0, d1, d2, d3, width;
} sdmmc_slot_config_t;
#define SDMMC_SLOT_CONFIG_DEFAULT() {0}
typedef struct {
    bool format_if_mount_failed;
    int  max_files, allocation_unit_size;
} esp_vfs_fat_sdmmc_mount_config_t;
esp_err_t esp_vfs_fat_sdmmc_mount(const char*, const sdmmc_host_t*, const sdmmc_slot_config_t*,
                                  const esp_vfs_fat_sdmmc_mount_config_t*, sdmmc_card_t**);
esp_err_t esp_vfs_fat_sdcard_unmount(const char*, sdmmc_card_t*);
esp_err_t bsp_led_get_count(uint32_t* count);
esp_err_t bsp_led_get_mode(bool* automatic);
esp_err_t bsp_led_get_brightness(uint8_t* brightness);
esp_err_t bsp_led_set_mode(bool automatic);
esp_err_t bsp_led_set_brightness(uint8_t brightness);
esp_err_t bsp_led_set_pixel(uint32_t index, uint32_t rgb);
esp_err_t bsp_led_send(void);
