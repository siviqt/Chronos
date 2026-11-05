#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_test_sdk.h"
#include "tanmatsu_platform.h"
#include "bsp/power.h"
#include <math.h>
#include "sd_protocol_defs.h"

static esp_err_t battery_error;
static bsp_power_battery_information_t battery;
esp_err_t bsp_power_get_battery_information(bsp_power_battery_information_t* out) { *out = battery; return battery_error; }

static bool         present, fail_directory, fail_allocation;
static esp_err_t    mount_error, unmount_error;
static bool fail_first_mount;
static bool hosted_live, shared_mount;
static unsigned handoffs;
static esp_err_t handoff_error;
static bool handoff_pending;
static unsigned int mounts, unmounts, ldo_count, host_inits, host_deinits;
static sdmmc_card_t card = {123};
static sdmmc_host_t mounted_host;
static bool         auto_mode  = false;
static uint8_t      brightness = 50;
static uint32_t     pixels[6];
static unsigned int sends;
static bool         fail_send, fail_brightness;
static unsigned brightness_writes;

const char* esp_err_to_name(esp_err_t error) {
    switch (error) {
        case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        default: return "ESP_FAIL";
    }
}
/* Simulate card detection independently of mount behavior. */
esp_err_t platform_sd_present(bool* output) {
    *output = present;
    return ESP_OK;
}
/* Validate the physical power-cycle pin mask against known Tanmatsu wiring. */
esp_err_t gpio_config(const gpio_config_t* config) {
    assert(config->pin_bit_mask == (UINT64_C(0x3f) << 39));
    return ESP_OK;
}
/* Ensure reset touches only the SD bus. */
esp_err_t gpio_set_level(gpio_num_t pin, int level) {
    assert(pin >= 39 && pin <= 44 && level == 0);
    return ESP_OK;
}
/* Skip hardware stabilization delays in host tests. */
void vTaskDelay(unsigned int ticks) {
    assert(ticks == 150);
}
/* Inject a DMA allocation failure before any filesystem operations. */
void* heap_caps_malloc(size_t size, int caps) {
    assert(caps == 3 && size == 2048);
    return fail_allocation ? NULL : malloc(size);
}
/* Track application ownership of the SD LDO. */
esp_err_t sd_pwr_ctrl_new_on_chip_ldo(const sd_pwr_ctrl_ldo_config_t* config, sd_pwr_ctrl_handle_t* handle) {
    assert(config->ldo_chan_id == 4);
    ++ldo_count;
    *handle = &card;
    return ESP_OK;
}
/* Verify each acquired LDO is released exactly once. */
esp_err_t sd_pwr_ctrl_del_on_chip_ldo(sd_pwr_ctrl_handle_t handle) {
    assert(handle == &card && ldo_count == 1);
    --ldo_count;
    return ESP_OK;
}
/* Check voltage sequencing stays within the referenced board reset procedure. */
esp_err_t sd_pwr_ctrl_set_io_voltage(sd_pwr_ctrl_handle_t handle, int voltage) {
    assert(handle == &card && (voltage == 0 || voltage == 3300));
    return ESP_OK;
}
/* Count exclusive-controller initialization attempts. */
esp_err_t sdmmc_host_init(void) {
    ++host_inits;
    return ESP_OK;
}
/* Expose runtime radio ownership, including shutdown before standalone remount. */
bool platform_clock_shared_controller(void) { return hosted_live; }
/* Verify the ISR handoff happens before every shared detach, even after errors. */
esp_err_t sdmmc_host_do_transaction(int slot, sdmmc_command_t* command) {
    assert(hosted_live && slot == 1);
    assert(command->opcode == SD_IO_RW_DIRECT && command->arg == 0);
    assert(command->flags == (SCF_CMD_AC | SCF_RSP_R5) && command->timeout_ms == 1000);
    ++handoffs;
    handoff_pending = true;
    return handoff_error;
}
/* Ensure unmount deinitializes SD slot zero rather than the entire controller. */
esp_err_t sdmmc_host_deinit_slot(int slot) {
    assert(slot == 0);
    assert(handoff_pending == (shared_mount && hosted_live));
    handoff_pending = false;
    ++host_deinits;
    return ESP_OK;
}
/* Validate the actual mount configuration passed by the production adapter. */
esp_err_t esp_vfs_fat_sdmmc_mount(const char* path, const sdmmc_host_t* host, const sdmmc_slot_config_t* slot,
                                  const esp_vfs_fat_sdmmc_mount_config_t* config, sdmmc_card_t** output) {
    assert(!strcmp(path, "/sd") && !config->format_if_mount_failed);
    assert(host->slot == 0 && host->dma_aligned_buffer && host->pwr_ctrl_handle);
    assert(host->flags & SDMMC_HOST_FLAG_DEINIT_ARG);
    shared_mount = host->deinit_p != sdmmc_host_deinit_slot;
    assert(shared_mount == hosted_live);
    assert((slot->width == 4 || slot->width == 1) && slot->clk == 43 && slot->cmd == 44 && slot->d0 == 39 && slot->d1 == 40 &&
           slot->d2 == 41 && slot->d3 == 42);
    ++mounts;
    mounted_host = *host;
    assert(host->init() == ESP_OK);
    if (fail_first_mount && slot->width == 4) {
        assert(host->deinit_p(0) == ESP_OK);
        return ESP_ERR_TIMEOUT;
    }
    if (slot->width == 1) assert(host->max_freq_khz == 400);
    if (mount_error) {
        assert(host->deinit_p(0) == ESP_OK);
        return mount_error;
    }
    *output = &card;
    return ESP_OK;
}
/* Mimic IDF consuming its card/context even when final unregistration fails. */
esp_err_t esp_vfs_fat_sdcard_unmount(const char* path, sdmmc_card_t* handle) {
    assert(!strcmp(path, "/sd") && handle == &card);
    ++unmounts;
    assert(mounted_host.deinit_p(0) == ESP_OK);
    return unmount_error;
}
/* Prevent real /sd writes and assert mkdir runs only after mounting. */
int __wrap_mkdir(const char* path, mode_t mode) {
    assert(!strcmp(path, "/sd/chronos") && mode == 0755 && mounts > unmounts);
    if (fail_directory) {
        errno = EACCES;
        return -1;
    }
    return 0;
}
/* Supply a real-directory result to the adapter without touching the host root. */
int __wrap_stat(const char* path, struct stat* info) {
    assert(!strcmp(path, "/sd/chronos"));
    memset(info, 0, sizeof(*info));
    info->st_mode = S_IFDIR;
    return 0;
}
/* Expose six RGB LEDs as on the board. */
esp_err_t bsp_led_get_count(uint32_t* count) {
    *count = 6;
    return ESP_OK;
}
/* Expose saved system mode for restoration tests. */
esp_err_t bsp_led_get_mode(bool* automatic) {
    *automatic = auto_mode;
    return ESP_OK;
}
/* Expose saved global brightness for restoration tests. */
esp_err_t bsp_led_get_brightness(uint8_t* value) {
    *value = brightness;
    return ESP_OK;
}
/* Track system-mode transitions. */
esp_err_t bsp_led_set_mode(bool automatic) {
    auto_mode = automatic;
    return ESP_OK;
}
/* Track brightness restoration and the initially-disabled case. */
esp_err_t bsp_led_set_brightness(uint8_t value) {
    ++brightness_writes;
    if (fail_brightness) return ESP_FAIL;
    brightness = value;
    return ESP_OK;
}
/* Fail the test if application code ever addresses a system LED. */
esp_err_t bsp_led_set_pixel(uint32_t index, uint32_t rgb) {
    assert(index == 4 || index == 5);
    pixels[index] = rgb;
    return ESP_OK;
}
/* Inject an LED send failure to verify dirty-frame retries. */
esp_err_t bsp_led_send(void) {
    ++sends;
    return fail_send ? ESP_FAIL : ESP_OK;
}

/* Cover missing media, invalid names, failed setup, remount, sharing, and consumed error handles. */
static void test_sd(void) {
    assert(platform_sd_directory() == NULL);
    assert(platform_sd_mount("../bad", false) == ESP_ERR_INVALID_ARG);
    assert(platform_sd_mount("chronos", false) == ESP_ERR_NOT_FOUND && mounts == 0);
    present         = true;
    fail_allocation = true;
    assert(platform_sd_mount("chronos", false) == ESP_ERR_NO_MEM && ldo_count == 0);
    fail_allocation = false;
    mount_error     = ESP_FAIL;
    assert(platform_sd_mount("chronos", false) == ESP_FAIL && ldo_count == 0);
    assert(platform_sd_directory() == NULL);
    mount_error    = ESP_OK;
    fail_directory = true;
    assert(platform_sd_mount("chronos", false) == ESP_FAIL && ldo_count == 0);
    fail_directory = false;
    assert(platform_sd_mount("chronos", false) == ESP_OK);
    assert(!strcmp(platform_sd_directory(), "/sd/chronos"));
    unsigned int count = mounts;
    assert(platform_sd_mount("chronos", false) == ESP_OK && mounts == count);
    assert(platform_sd_mount("different", false) == ESP_ERR_INVALID_STATE);
    unmount_error = ESP_FAIL;
    assert(platform_sd_unmount() == ESP_FAIL && ldo_count == 0 && !platform_sd_directory());
    count = unmounts;
    assert(platform_sd_unmount() == ESP_OK && unmounts == count);
    unmount_error = ESP_OK;
    count         = host_inits;
    hosted_live = true;
    assert(platform_sd_mount("chronos", true) == ESP_OK && host_inits == count);
    assert(platform_sd_unmount() == ESP_OK && ldo_count == 0);
    assert(host_deinits == mounts);
    fail_first_mount = true;
    count = mounts;
    assert(platform_sd_mount("chronos", true) == ESP_OK && mounts == count + 2);
    assert(strstr(platform_sd_status(), "mounted at"));
    assert(platform_sd_unmount() == ESP_OK);
    fail_first_mount = false;
    assert(handoffs == 3); /* Shared unmount, failed 4-bit mount, retry unmount. */
    handoff_error = ESP_ERR_TIMEOUT;
    assert(platform_sd_mount("chronos", true) == ESP_OK);
    assert(platform_sd_unmount() == ESP_OK && handoffs == 4);
    handoff_error = ESP_OK;
    hosted_live = false;
    mount_error = ESP_ERR_TIMEOUT;
    count = mounts;
    assert(platform_sd_mount("chronos", false) == ESP_ERR_TIMEOUT && mounts == count + 2);
    assert(strstr(platform_sd_status(), "SD mount: ESP_ERR_TIMEOUT"));
    mount_error = ESP_OK;
    assert(handoffs == 4); /* Powered-off standalone cleanup never accesses slot 1. */
}
/* Verify A/B mapping, dim colors, cached writes, retry, and restored system state. */
static void test_leds(void) {
    assert(platform_leds_init() == ESP_OK && auto_mode && brightness == 50);
    assert(platform_leds_write(0xff8000, 0x00ffff) == ESP_OK);
    assert(pixels[4] == 0x804000 && pixels[5] == 0x008080);
    unsigned int count = sends;
    assert(platform_leds_write(0xff8000, 0x00ffff) == ESP_OK && sends == count);
    fail_send = true;
    assert(platform_leds_write(0xff0000, 0) == ESP_FAIL);
    fail_send = false;
    assert(platform_leds_write(0xff0000, 0) == ESP_OK && sends == count + 2);
    /* Returning to a previous color after a failed write must still send a frame. */
    fail_send = true;
    assert(platform_leds_write(0x00ff00, 0) == ESP_FAIL);
    count     = sends;
    fail_send = false;
    assert(platform_leds_write(0xff0000, 0) == ESP_OK && sends == count + 1);
    assert(platform_leds_brightness(101) == ESP_ERR_INVALID_ARG && brightness == 50);
    assert(platform_leds_brightness(0) == ESP_OK && brightness == 0);
    assert(platform_leds_write(0xff0000, 0) == ESP_OK && brightness == 0);
    unsigned writes = brightness_writes;
    assert(platform_leds_brightness(0) == ESP_OK && brightness_writes == writes);
    fail_brightness = true;
    assert(platform_leds_brightness(100) == ESP_FAIL && brightness == 0);
    fail_brightness = false;
    assert(platform_leds_brightness(100) == ESP_OK && brightness == 100);
    assert(brightness_writes == writes + 2);
    assert(platform_leds_shutdown() == ESP_OK && !auto_mode && brightness == 50);
    assert(!pixels[4] && !pixels[5]);
    brightness = 3; /* Coprocessor truncation formerly made 16 * 7 / 255 black. */
    assert(platform_leds_init() == ESP_OK && brightness == 10);
    assert(platform_leds_write(0, 0x0000ff) == ESP_OK);
    assert((pixels[5] & 255) * (brightness * 255 / 100) / 255 >= 12);
    assert(platform_leds_shutdown() == ESP_OK && brightness == 3);
    brightness = 0;
    assert(platform_leds_init() == ESP_OK && brightness == 10);
    assert(platform_leds_shutdown() == ESP_OK && brightness == 0);
}
/* Run production SD/LED adapters against controllable SDK boundaries. */
static void test_battery(void) {
    uint8_t percent = 77;
    assert(platform_battery_percent(NULL) == ESP_ERR_INVALID_ARG);
    assert(platform_battery_percent(&percent) == ESP_ERR_NOT_FOUND && percent == 77);
    battery.battery_available = true;
    battery.remaining_percentage = 20.1;
    assert(platform_battery_percent(&percent) == ESP_OK && percent == 21);
    battery.remaining_percentage = 80;
    assert(platform_battery_percent(&percent) == ESP_OK && percent == 80);
    battery.remaining_percentage = 110;
    assert(platform_battery_percent(&percent) == ESP_OK && percent == 100);
    battery.remaining_percentage = -1;
    assert(platform_battery_percent(&percent) == ESP_OK && percent == 0);
    battery.remaining_percentage = NAN;
    assert(platform_battery_percent(&percent) == ESP_ERR_INVALID_STATE);
    battery.remaining_percentage = 50; battery_error = ESP_FAIL;
    assert(platform_battery_percent(&percent) == ESP_FAIL && percent == 0);
}
int main(void) {
    test_battery();
    test_sd();
    test_leds();
    puts("platform SD/LED: all tests passed");
    return 0;
}
