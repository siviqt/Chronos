#define _DEFAULT_SOURCE
#define settimeofday fake_settimeofday
#define time fake_time
#include <assert.h>
#include <stdio.h>
/* Run the adapter's actual worker with a deterministic scheduler and fake SDK.
 * No BSP RTC setter exists in these stubs: introducing one fails the link. */
#include "../../components/tanmatsu_platform/src/platform_clock.c"
static time_t system_seconds;
int fake_settimeofday(const struct timeval* v, const struct timezone* zone) { (void)zone; system_seconds = v->tv_sec; return 0; }
time_t fake_time(time_t* out) { if (out) *out = system_seconds; return system_seconds; }
static uint64_t elapsed, end_at, manual_at, disconnect_at, reconnect_at;
static unsigned attempts, ntp_starts, rtc_reads;
static bool sd_mounted, radio_powered = true;
static unsigned driver_inits, driver_deinits, transport_stops;
const char* platform_sd_directory(void) { return sd_mounted ? "/sd/chronos" : NULL; }
int esp_hosted_init(void) { radio_powered = true; return ESP_OK; }
int esp_hosted_deinit(void) { assert(!sd_mounted); ++transport_stops; return ESP_OK; }
esp_err_t bsp_power_set_radio_state(int state) { assert(state == 0 && !hosted_live && !driver_ready); radio_powered = false; return ESP_OK; }
esp_err_t esp_wifi_init(const wifi_init_config_t* config) { assert(!config->nvs_enable && radio_powered); ++driver_inits; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { assert(!sd_mounted && !worker); ++driver_deinits; return ESP_OK; }
static bool network_available, respond, sntp_running, connect_pending;
static void (*sync_cb)(struct timeval*);
static void (*task_fn)(void*);
int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* arg, unsigned priority, TaskHandle_t* task) {
    (void)name; (void)stack; (void)arg; (void)priority;
    task_fn = fn; *task = (void*)1; return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { (void)task; }
void vTaskDelay(uint32_t ticks) {
    elapsed += (uint64_t)ticks * 1000;
    if (manual_at && elapsed >= manual_at) { manual_at = 0; platform_clock_request_sync(); }
    if (disconnect_at && elapsed >= disconnect_at) { disconnect_at = 0; network_available = false; }
    if (reconnect_at && elapsed >= reconnect_at) { reconnect_at = 0; network_available = true; }
    if (elapsed >= end_at) atomic_store(&stopping, true);
}
uint64_t platform_monotonic_us(void) { return elapsed; }
esp_err_t platform_rtc_seconds(uint32_t* seconds) { ++rtc_reads; *seconds = 1704067200; return ESP_OK; }
esp_err_t wifi_remote_initialize(void) { return ESP_OK; }
esp_err_t wifi_connection_init_stack(void) { return ESP_OK; }
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t* ap) { (void)ap; return network_available ? ESP_OK : ESP_FAIL; }
bool wifi_connection_is_connected(void) { return network_available; }
esp_err_t wifi_connection_connect(uint16_t index, uint8_t retries) {
    assert(retries == 1);
    if (index) return ESP_FAIL;
    ++attempts; return network_available || connect_pending ? ESP_OK : ESP_FAIL;
}
void wifi_connection_disconnect(void) {}
void esp_sntp_stop(void) { sntp_running = false; }
void esp_sntp_setoperatingmode(int mode) { assert(mode == SNTP_OPMODE_POLL); }
void esp_sntp_setservername(int index, const char* name) { assert(index == 0 && !strcmp(name, "pool.ntp.org")); }
void esp_sntp_set_time_sync_notification_cb(void (*cb)(struct timeval*)) { sync_cb = cb; }
void esp_sntp_set_sync_interval(uint32_t ms) { assert(ms == 3600000); }
void esp_sntp_init(void) {
    ++ntp_starts; sntp_running = true;
    if (respond) { struct timeval value = {0}; sync_cb(&value); }
}
static void run(bool available, bool reply, uint64_t duration) {
    elapsed = 0; end_at = duration; attempts = ntp_starts = 0;
    network_available = available; respond = reply;
    worker = NULL;
    atomic_store(&synced, false);
    assert(platform_clock_radio_init() == ESP_OK && platform_clock_shared_controller());
    assert(platform_clock_start("pool.ntp.org") == ESP_OK);
    task_fn(NULL);
    assert(!sntp_running && !atomic_load(&busy));
}
int main(void) {
    assert(platform_clock_shared_controller()); /* Constructor-owned, before radio API initialization. */
    int64_t seconds;
    assert(platform_clock_seconds(&seconds) == ESP_ERR_INVALID_STATE);
    assert(platform_clock_select_source(1) == ESP_OK);
    assert(platform_clock_seconds(&seconds) == ESP_OK && seconds == 1704067200);
    assert(strstr(platform_clock_status(), "RTC"));
    run(true, true, UINT64_C(3600100000));
    assert(ntp_starts == 2); /* Startup and 60-minute sync. */
    manual_at = 2000000;
    run(true, true, 3000000);
    assert(ntp_starts == 2); /* Keyboard request restarts immediately. */
    run(false, false, 61000000);
    assert(ntp_starts == 0 && attempts == 2); /* Offline retry, RTC remains usable. */
    assert(platform_clock_seconds(&seconds) == ESP_OK && seconds == 1704067200);
    reconnect_at = 2000000;
    run(false, true, 3000000);
    assert(ntp_starts == 1); /* Obtain time promptly when networking returns. */
    run(true, false, 16000000);
    assert(ntp_starts == 1 && !atomic_load(&synced)); /* Bounded failed request. */
    connect_pending = true;
    run(false, false, 6000000);
    assert(attempts == 1 && ntp_starts == 0); /* A silent AP cannot trap the worker in SDK await. */
    connect_pending = false;
    atomic_store(&online, true); atomic_store(&synced, true);
    unsigned reads = rtc_reads;
    assert(platform_clock_seconds(&seconds) == ESP_OK && rtc_reads == reads);
    atomic_store(&online, false);
    assert(platform_clock_seconds(&seconds) == ESP_OK && rtc_reads == reads);
    worker = NULL;
    assert(platform_clock_select_source(2) == ESP_OK);
    assert(platform_clock_set_manual(1800000000) == ESP_OK);
    assert(platform_clock_seconds(&seconds) == ESP_OK && seconds == 1800000000);
    assert(rtc_reads == reads);
    assert(platform_clock_select_source(1) == ESP_OK);
    assert(platform_clock_seconds(&seconds) == ESP_OK && seconds == 1704067200);
    assert(platform_clock_start("") == ESP_OK); /* Wi-Fi only must never start SNTP. */
    elapsed = 0; end_at = 2000000; ntp_starts = 0; network_available = true;
    task_fn(NULL); assert(ntp_starts == 0);
    worker = NULL;
    sd_mounted = true;
    assert(platform_clock_radio_shutdown() == ESP_ERR_INVALID_STATE);
    assert(!transport_stops && radio_powered);
    sd_mounted = false;
    assert(platform_clock_radio_shutdown() == ESP_OK);
    assert(!radio_powered && !platform_clock_shared_controller() && driver_deinits == 1);
    assert(platform_clock_radio_init() == ESP_OK);
    assert(platform_clock_shared_controller());
    assert(platform_clock_start("pool.ntp.org") == ESP_OK);
    elapsed = 0; end_at = 2000000; respond = true; ntp_starts = 0;
    task_fn(NULL);
    assert(driver_inits == 1 && ntp_starts == 1);
    worker = NULL;
    assert(platform_clock_radio_shutdown() == ESP_OK);
    assert(!radio_powered && transport_stops == 2 && driver_deinits == 2);
    puts("network clock: all tests passed");
}
