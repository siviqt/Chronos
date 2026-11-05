#include <stdatomic.h>
#include "sdkconfig.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_hosted.h"
#include "bsp/power.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tanmatsu_platform.h"
#include "wifi_connection.h"
#include "wifi_remote.h"
#include "wifi_settings.h"

/* Hosted owns SDMMC slot 1 while initialized. Full radio power-off requires
 * the owner to unmount SD slot 0 before deinitializing the shared controller. */
static TaskHandle_t worker;
static atomic_bool stopping, online, synced, busy, requested;
static atomic_uint sync_generation;
static bool radio_ready, stack_ready, driver_ready;
#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)
static bool hosted_live = true; /* SDK constructor owns the controller. */
#else
static bool hosted_live;
#endif
static unsigned time_source;
static atomic_bool time_valid;
static char server[254];
static void synchronized(struct timeval* value) {
    (void)value;
    atomic_fetch_add(&sync_generation, 1);
    atomic_store(&time_valid, true);
    atomic_store(&synced, true); /* SNTP updates system RAM time only, never BSP RTC. */
}
/* The pinned manager retains WIFI_CONNECTED_BIT after disconnect. Verify the
 * station association as well so restarting Wi-Fi cannot reuse that stale bit. */
static bool connected_now(void) {
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK && wifi_connection_is_connected();
}
/* The SDK's wifi_connection_await() can continue indefinitely on timeout.
 * Poll instead, checking cancellation and bounding each saved-network attempt. */
static bool connect_saved(void) {
    static uint16_t next_network;
    uint64_t deadline = platform_monotonic_us() + UINT64_C(15000000);
    for (unsigned tried = 0; tried < WIFI_SETTINGS_MAX && !atomic_load(&stopping) &&
         platform_monotonic_us() < deadline; ++tried) {
        uint16_t index = next_network;
        next_network = (next_network + 1) % WIFI_SETTINGS_MAX;
        if (wifi_connection_connect(index, 1) != ESP_OK) continue;
        for (unsigned i = 0; i < 50 && !atomic_load(&stopping) && platform_monotonic_us() < deadline; ++i) {
            if (connected_now()) return true;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    return connected_now();
}
static void clock_worker(void* argument) {
    (void)argument;
    if (!stack_ready) {
        stack_ready = wifi_connection_init_stack() == ESP_OK;
        driver_ready = stack_ready;
    } else if (!driver_ready) {
        /* Keep the manager's netif/event handlers; recreate only the powered-off
         * remote Wi-Fi driver. Re-running init_stack would duplicate netifs. */
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        config.nvs_enable = false;
        driver_ready = esp_wifi_init(&config) == ESP_OK;
    }
    if (!driver_ready) ESP_LOGW("chronos_clock", "Wi-Fi stack initialization failed");
    uint64_t next_attempt = 0;
    bool previous_online = false, attempted = false;
    while (!atomic_load(&stopping)) {
        bool connected = driver_ready && attempted && connected_now();
        atomic_store(&online, connected);
        bool disconnected = !connected && previous_online;
        bool reconnected = connected && !previous_online;
        previous_online = connected;
        uint64_t now = platform_monotonic_us();
        if (disconnected) next_attempt = now;
        bool manual = atomic_exchange(&requested, false);
        if (driver_ready && (manual || reconnected || now >= next_attempt)) {
            next_attempt = now + UINT64_C(3600000000);
            atomic_store(&busy, true);
            if (!connected) { connected = connect_saved(); attempted = true; }
            atomic_store(&online, connected);
            previous_online = connected;
            if (!connected) {
                ESP_LOGW("chronos_clock", "No saved Wi-Fi network connected; retry in 60 seconds");
                next_attempt = now + UINT64_C(60000000);
            }
            if (connected && server[0] && !atomic_load(&stopping)) {
                esp_sntp_stop();
                esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                esp_sntp_setservername(0, server);
                esp_sntp_set_time_sync_notification_cb(synchronized);
                esp_sntp_set_sync_interval(3600000);
                unsigned generation = atomic_load(&sync_generation);
                esp_sntp_init();
                /* Bound each attempt; the UI, RTC and monotonic timers keep running. */
                for (unsigned i = 0; i < 150 && !atomic_load(&stopping) && atomic_load(&sync_generation) == generation; ++i) {
                    if (!connected_now()) {
                        atomic_store(&online, false);
                        previous_online = false;
                        next_attempt = platform_monotonic_us() + UINT64_C(60000000);
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                esp_sntp_stop();
                if (atomic_load(&sync_generation) == generation && !atomic_load(&stopping))
                    ESP_LOGW("chronos_clock", "NTP attempt timed out or disconnected; retaining system time");
            }
            atomic_store(&busy, false);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    esp_sntp_stop();
    if (driver_ready) wifi_connection_disconnect();
    atomic_store(&online, false);
    atomic_store(&busy, false);
    atomic_store(&stopping, false); /* Acknowledge shutdown; no shared state follows. */
    vTaskDelete(NULL);
}
esp_err_t platform_clock_radio_init(void) {
    esp_err_t error = wifi_remote_initialize();
    /* wifi_remote_initialize caches its first initialization. The explicit SDK
     * init is idempotent and restores transport after a complete power-off. */
    if (error == ESP_OK) error = esp_hosted_init();
    if (error == ESP_OK) hosted_live = true;
    radio_ready = error == ESP_OK;
    return error;
}
bool platform_clock_shared_controller(void) { return hosted_live; }
esp_err_t platform_clock_radio_shutdown(void) {
    if (!radio_ready) return ESP_OK;
    /* Hosted deinit tears down the whole SDMMC host. The owner must unmount
     * slot 0 first, then remount it as an independent controller owner. */
    if (worker || platform_sd_directory()) return ESP_ERR_INVALID_STATE;
    esp_err_t error;
    if (driver_ready) {
        error = esp_wifi_deinit();
        if (error != ESP_OK) return error;
        driver_ready = false;
    }
    if (hosted_live) {
        error = esp_hosted_deinit();
        if (error != ESP_OK) return error;
        hosted_live = false;
    }
    error = bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
    if (error == ESP_OK) {
        radio_ready = false;
        ESP_LOGI("chronos_clock", "Radio powered off; automatic radio LED follows hardware state");
    }
    return error;
}
esp_err_t platform_clock_start(const char* hostname) {
    if (!radio_ready || worker) return ESP_ERR_INVALID_STATE;
    if (!hostname || strlen(hostname) >= sizeof(server)) return ESP_ERR_INVALID_ARG;
    strcpy(server, hostname);
    atomic_store(&stopping, false);
    atomic_store(&requested, false);
    return xTaskCreate(clock_worker, "clock_network", 6144, NULL, 3, &worker) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
void platform_clock_request_sync(void) { if (worker && server[0]) atomic_store(&requested, true); }
esp_err_t platform_clock_select_source(unsigned source) {
    if (source > 2 || worker) return ESP_ERR_INVALID_STATE;
    if (source == 1 || (source == 0 && !atomic_load(&time_valid))) {
        uint32_t rtc;
        esp_err_t error = platform_rtc_seconds(&rtc);
        if (error != ESP_OK) return error;
        struct timeval value = {.tv_sec = rtc};
        if (settimeofday(&value, NULL)) return ESP_FAIL;
        atomic_store(&time_valid, true);
        atomic_store(&synced, false);
    }
    time_source = source;
    return ESP_OK;
}
esp_err_t platform_clock_set_manual(int64_t seconds) {
    if (worker && server[0]) return ESP_ERR_INVALID_STATE;
    struct timeval value = {.tv_sec = (time_t)seconds};
    if ((int64_t)value.tv_sec != seconds || seconds < 946684800 || seconds > 4102444799LL) return ESP_ERR_INVALID_ARG;
    if (settimeofday(&value, NULL)) return ESP_FAIL;
    time_source = 2;
    atomic_store(&time_valid, true);
    atomic_store(&synced, false);
    return ESP_OK;
}
const char* platform_clock_status(void) {
    if (time_source == 2) return atomic_load(&time_valid) ? "Manual system time" : "Manual - enter date/time";
    if (time_source == 1) return "RTC sampled into system time";
    if (!worker || !server[0]) return "NTP paused - Wi-Fi off / NTP disabled";
    if (atomic_load(&busy)) return "Synchronizing NTP...";
    if (atomic_load(&synced)) return "NTP synchronized (system clock retained)";
    return "NTP waiting - RTC startup time";
}
esp_err_t platform_clock_seconds(int64_t* seconds) {
    if (!seconds) return ESP_ERR_INVALID_ARG;
    if (!atomic_load(&time_valid)) return ESP_ERR_INVALID_STATE;
    *seconds = (int64_t)time(NULL);
    return ESP_OK;
}
void platform_clock_stop(void) {
    if (!worker) return;
    atomic_store(&stopping, true);
    while (atomic_load(&stopping)) vTaskDelay(pdMS_TO_TICKS(10));
    worker = NULL;
}
