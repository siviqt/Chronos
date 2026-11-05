#define _POSIX_C_SOURCE 200809L
#define sleep test_sleep
#include "../../components/chronos_app/src/chronos_app.c"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static char sd_directory[128];
static unsigned audio_fail_mask;
static unsigned scan_fail_mask;
static char audio_load_directories[SOUND_DIRECTORY_COUNT][128];
static bool internal_mounted;
static unsigned audio_load_count;
static bool mounted, radio, saved_ntp, preowned_controller;
static unsigned mount_calls, ntp_starts, scenario;
static uint8_t hardware_volume;
static bool exit_shown, exit_waited, display_closed;
unsigned int test_sleep(unsigned int seconds) {
    assert(exit_shown && !display_closed && seconds == 3);
    exit_waited = true;
    return 0;
}
static chronos_settings_t fixture;
const char* esp_err_to_name(esp_err_t error) { (void)error; return "test error"; }
esp_err_t platform_board_init(void) { return ESP_OK; }
void platform_return_to_launcher(void) { assert(exit_waited && display_closed); }
uint64_t platform_monotonic_us(void) { return 1000000; }
esp_err_t platform_display_init(void) { return ESP_OK; }
pax_buf_t* platform_display_buffer(void) { return (pax_buf_t*)&display_closed; }
esp_err_t platform_display_present(void) { return ESP_OK; }
void platform_display_shutdown(void) { assert(exit_waited); display_closed = true; }
void chronos_ui_render(pax_buf_t* b, const chronos_ui_t* u, const chronos_ui_view_t* v) { (void)b; (void)u; (void)v; }
void chronos_ui_splash(pax_buf_t* b, const char* text) { (void)b; if (!strcmp(text, "CHRONOS signing off...")) exit_shown = true; }
esp_err_t platform_illumination(uint8_t k, uint8_t d) { (void)k; (void)d; return ESP_OK; }
esp_err_t platform_audio_init(void) { hardware_volume = 70; return ESP_OK; }
static unsigned sound_directory_index(const char* directory) {
    assert(directory);
    if (!strcmp(directory, sd_directory)) return 0;
    if (!strcmp(directory, "/sd/apps/com.github.siviqt.chronos")) return 1;
    if (!strcmp(directory, "/sd/apps/chronos")) return 2;
    if (!strcmp(directory, "/int/apps/com.github.siviqt.chronos")) return 3;
    assert(!strcmp(directory, "/int/apps/chronos"));
    return 4;
}
esp_err_t platform_audio_load(const char* d, const char* f) {
    if (f && f[0]) {
        assert(audio_load_count < SOUND_DIRECTORY_COUNT);
        snprintf(audio_load_directories[audio_load_count++], sizeof(audio_load_directories[0]), "%s", d);
        if (audio_fail_mask & (1u << sound_directory_index(d))) return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
esp_err_t platform_audio_volume_step(int step) { hardware_volume += step; return ESP_OK; }
uint8_t platform_audio_volume(void) { return hardware_volume; }
void platform_audio_loop(bool on) { (void)on; }
void platform_audio_pacer_active(bool enabled) { (void)enabled; }
void platform_audio_pulse(void) {}
esp_err_t platform_audio_error(void) { return ESP_OK; }
esp_err_t platform_audio_shutdown(void) { return ESP_OK; }
esp_err_t platform_battery_percent(uint8_t* p) { *p = 75; return ESP_OK; }
esp_err_t platform_leds_brightness(uint8_t p) { assert(p <= 100); return ESP_OK; }
esp_err_t platform_leds_init(void) { return ESP_OK; }
esp_err_t platform_leds_write(uint32_t a, uint32_t b) { (void)a; (void)b; return ESP_OK; }
esp_err_t platform_leds_shutdown(void) { return ESP_OK; }
esp_err_t platform_clock_radio_shutdown(void) { assert(!mounted); radio = false; return ESP_OK; }
esp_err_t platform_clock_radio_init(void) { radio = true; return ESP_OK; }
bool platform_clock_shared_controller(void) { return radio || preowned_controller; }
esp_err_t platform_clock_start(const char* server) {
    assert(settings.ntp_enabled);
    if (scenario != 3 && scenario != 5) assert(!strcmp(server, fixture.ntp_server));
    ++ntp_starts; return ESP_OK;
}
esp_err_t platform_clock_select_source(unsigned source) { (void)source; return ESP_OK; }
esp_err_t platform_clock_set_manual(int64_t seconds) { (void)seconds; return ESP_OK; }
esp_err_t platform_audio_next_file(const char* d, const char* c, char next[65]) {
    if (!d || !c || !next) return ESP_ERR_INVALID_ARG;
    unsigned index = sound_directory_index(d);
    if (scan_fail_mask & (1u << index)) return ESP_FAIL;
    /* A duplicate across package locations must not repeat in the picker. */
    const char* files[SOUND_DIRECTORY_COUNT][3] = {
        {"B.wav", NULL, NULL}, {"A.wav", "C.wav", NULL}, {"A.wav", "E.wav", NULL},
        {"A.wav", "D.wav", NULL}, {"D.wav", "F.wav", NULL}};
    next[0] = 0;
    for (size_t i = 0; i < 3 && files[index][i]; ++i) {
        if (strcmp(files[index][i], c) > 0) { strcpy(next, files[index][i]); break; }
    }
    return ESP_OK;
}
esp_err_t platform_internal_mount(void) { internal_mounted = true; return ESP_OK; }
esp_err_t platform_internal_unmount(void) { internal_mounted = false; return ESP_OK; }
void platform_clock_stop(void) {}
void platform_clock_request_sync(void) {}
const char* platform_clock_status(void) { return "test clock"; }
esp_err_t platform_clock_seconds(int64_t* seconds) { *seconds = 1704067200; return ESP_OK; }
const char* platform_sd_directory(void) { return mounted ? sd_directory : NULL; }
const char* platform_sd_status(void) { return "SD mount: ESP_ERR_TIMEOUT. Changes are session-only."; }
esp_err_t platform_sd_mount(const char* name, bool shared) {
    assert(!strcmp(name, "chronos"));
    ++mount_calls;
    assert(shared == (radio || preowned_controller));
    if ((mount_calls == 1 && (scenario == 0 || scenario == 2 || scenario == 3)) ||
        (mount_calls == 2 && (scenario == 1 || scenario == 3))) return ESP_ERR_TIMEOUT;
    mounted = true; return ESP_OK;
}
esp_err_t platform_sd_unmount(void) { mounted = false; return ESP_OK; }
bool platform_input_poll(platform_input_t* input, uint32_t wait) {
    (void)wait;
    assert(internal_mounted);
    if (scenario == 0 || scenario == 2 || scenario == 3 || scenario == 5) {
        assert(settings.volume_percent == 70);
    } else {
        assert(chronos_settings_equal(&settings, &fixture));
        assert(hardware_volume == 35);
        assert(settings.ntp_enabled == saved_ntp);
    }
    if (preowned_controller) assert(mount_calls == 1);
    assert(ntp_starts == ((scenario != 0 && scenario != 2 && scenario != 3 && scenario != 5 && saved_ntp) ? 1u : 0u));
    if (scenario == 0 || scenario == 2) assert(!persisted_valid && !mounted && !radio);
    if (scenario == 1 || scenario == 3) assert(!mounted);
    *input = (platform_input_t){.type = PLATFORM_INPUT_F1};
    return true;
}
int main(void) {
    strcpy(sd_directory, "/tmp/CHRONOS-audio");
    mounted = true;
    strcpy(settings.alarm_sound, "wake.wav");
    /* Each source takes priority over later sources; unavailable ones fall through. */
    for (unsigned source = 0; source < SOUND_DIRECTORY_COUNT; ++source) {
        audio_fail_mask = (1u << source) - 1;
        audio_load_count = 0;
        load_sound();
        assert(audio_load_count == source + 1);
        assert(sound_directory_index(audio_load_directories[source]) == source);
    }
    char next[65];
    assert(next_sound_file("", next) == ESP_OK && !strcmp(next, "A.wav"));
    assert(next_sound_file("A.wav", next) == ESP_OK && !strcmp(next, "B.wav"));
    assert(next_sound_file("B.wav", next) == ESP_OK && !strcmp(next, "C.wav"));
    assert(next_sound_file("C.wav", next) == ESP_OK && !strcmp(next, "D.wav"));
    assert(next_sound_file("D.wav", next) == ESP_OK && !strcmp(next, "E.wav"));
    assert(next_sound_file("E.wav", next) == ESP_OK && !strcmp(next, "F.wav"));
    assert(next_sound_file("F.wav", next) == ESP_OK && !next[0]);
    /* Internal assets remain usable without any SD card. */
    mounted = false;
    audio_fail_mask = 0;
    audio_load_count = 0;
    load_sound();
    assert(audio_load_count == 1 && sound_directory_index(audio_load_directories[0]) == 3);
    audio_load_count = 0;
    audio_fail_mask = 1u << 3;
    load_sound();
    assert(audio_load_count == 2 && sound_directory_index(audio_load_directories[1]) == 4);
    assert(next_sound_file("A.wav", next) == ESP_OK && !strcmp(next, "D.wav"));
    scan_fail_mask = (1u << 3) | (1u << 4);
    assert(next_sound_file("", next) != ESP_OK);
    mounted = true;
    assert(next_sound_file("B.wav", next) == ESP_OK && !strcmp(next, "C.wav"));
    audio_fail_mask = scan_fail_mask = 0;
    puts("sound lookup: real SD/internal paths, priority, merged choices and missing storage passed");

    for (scenario = 0; scenario < 8; ++scenario) {
        strcpy(sd_directory, "/tmp/CHRONOS-startup-XXXXXX");
        assert(mkdtemp(sd_directory));
        mounted = radio = false; preowned_controller = scenario >= 6; mount_calls = ntp_starts = 0;
        chronos_settings_defaults(&fixture);
        fixture.volume_percent = 35;
        fixture.ntp_enabled = saved_ntp = scenario != 2 && scenario != 4 && scenario != 7;
        fixture.wifi_enabled = saved_ntp;
        strcpy(fixture.ntp_server, "time.example.org");
        strcpy(fixture.timezone, "JST-9");
        if (scenario != 5) assert(chronos_settings_save(sd_directory, &fixture) == SETTINGS_OK);
        exit_shown = exit_waited = display_closed = false;
        chronos_app_run();
        assert(!internal_mounted);
        chronos_settings_t loaded;
        assert(chronos_settings_load(sd_directory, &loaded) == SETTINGS_OK);
        if (scenario != 5) assert(chronos_settings_equal(&loaded, &fixture));
        else assert(loaded.volume_percent == 70);
        char path[256]; snprintf(path, sizeof(path), "%s/%s", sd_directory, CHRONOS_SETTINGS_FILENAME);
        assert(unlink(path) == 0); assert(rmdir(sd_directory) == 0);
    }
    puts("app startup: Wi-Fi opt-in, SD failures and saved settings; all tests passed");
}

void external_display_submit(const external_config_t* c, const external_frame_t* f, bool retry) {
    (void)f; (void)retry; assert(!c->enabled);
}
external_status_t external_display_status(void) { return EXT_DISABLED; }
void external_display_stop(void) {}
