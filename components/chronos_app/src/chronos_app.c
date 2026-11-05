#define _POSIX_C_SOURCE 200809L
#include "chronos_app.h"
#include "chronos_clock.h"
#include "chronos_external.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "chronos_runtime.h"
#include "chronos_screen.h"
#include "chronos_ui_render.h"
#include "esp_log.h"
#include "tanmatsu_platform.h"

static const char*        TAG = "chronos";
static chronos_settings_t settings;
static chronos_runtime_t  runtime;
static chronos_ui_view_t  view;
/* View row targets point into the stable view or static labels. Keep snapshots
 * off the task stack; changed inline text is included in the comparison. */
static chronos_ui_view_t  presented_view;
static chronos_ui_t       presented_ui;

static chronos_settings_t observed_settings, persisted_settings;
static bool settings_dirty, persistence_ready, persisted_valid;
static uint64_t save_after;

/* Observe committed values, not editor drafts or struct padding. */
static void observe_settings(uint64_t now) {
    if (chronos_settings_equal(&settings, &observed_settings)) return;
    if (settings.volume_percent != observed_settings.volume_percent)
        ESP_LOGI(TAG, "Volume: %u%%%s", settings.volume_percent, settings.volume_percent ? "" : " (muted)");
    if (strcmp(settings.timezone, observed_settings.timezone))
        ESP_LOGI(TAG, "Local timezone: %s", settings.timezone);
    if (settings.city_count != observed_settings.city_count)
        ESP_LOGI(TAG, "World Clock locations: %u", settings.city_count);
    for (size_t i = 0; i < CHRONOS_CITY_LIMIT; ++i)
        if (strcmp(settings.cities[i].name, observed_settings.cities[i].name) ||
            strcmp(settings.cities[i].timezone, observed_settings.cities[i].timezone))
            ESP_LOGI(TAG, "World Clock %u: %s (%s)", (unsigned)i + 1, settings.cities[i].name, settings.cities[i].timezone);
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        const chronos_alarm_config_t* a = &settings.alarms[i];
        const chronos_alarm_config_t* b = &observed_settings.alarms[i];
        if (a->enabled != b->enabled || a->repeat != b->repeat || a->hour != b->hour || a->minute != b->minute ||
            a->weekdays != b->weekdays || a->ring_seconds != b->ring_seconds ||
            a->snooze_minutes != b->snooze_minutes || strcmp(a->label, b->label))
            ESP_LOGI(TAG, "Alarm %u configured: %s, %02u:%02u, %s", (unsigned)i + 1, a->label,
                     a->hour, a->minute, a->enabled ? "enabled" : "disabled");
    }
    ESP_LOGI(TAG, "Configuration changed; automatic save pending%s", platform_sd_directory() ? "" : " (SD unavailable)");
    observed_settings = settings;
    settings_dirty = !persisted_valid || !chronos_settings_equal(&settings, &persisted_settings);
    save_after = now + 1000000;
}
static settings_result_t save_settings(uint64_t now) {
    settings_result_t result = platform_sd_directory() ? chronos_settings_save(platform_sd_directory(), &settings) : SETTINGS_UNAVAILABLE;
    if (result == SETTINGS_OK) {
        settings_dirty = false;
        persisted_settings = settings;
        persisted_valid = true;
        ESP_LOGI(TAG, "Saved %s/%s", platform_sd_directory(), CHRONOS_SETTINGS_FILENAME);
    } else ESP_LOGW(TAG, "Settings save: %s", chronos_settings_notice(result));
    save_after = now + 30000000;
    return result;
}
/* Log logical transitions, never blink frames or individual pacer beats. */
static void observe_runtime(void) {
    /* Persist consumed one-shot alarms; live snooze/ring state stays in runtime. */
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i)
        if (settings.alarms[i].repeat == CHRONOS_ALARM_ONCE)
            settings.alarms[i].enabled = runtime.alarms.config[i].enabled;
    static timing_state_t previous[3];
    static chronos_alarm_phase_t phases[CHRONOS_ALARM_LIMIT];
    static int led_mode = -1;
    static bool audio_loop;
    const timing_state_t current[] = {runtime.stopwatch.session.state, runtime.countdown.session.state, runtime.pacer.session.state};
    const char* names[] = {"Stopwatch", "Countdown", "Pacer"};
    const char* states[] = {"idle/reset", "running", "paused", "complete", "stopped/cancelled"};
    for (unsigned i = 0; i < 3; ++i) if (current[i] != previous[i]) {
        ESP_LOGI(TAG, "%s: %s (LED B updated)", names[i], states[current[i]]);
        previous[i] = current[i];
    }
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) if (phases[i] != runtime.alarms.runtime[i].phase) {
        phases[i] = runtime.alarms.runtime[i].phase;
        ESP_LOGI(TAG, "Alarm %u %s: %s", (unsigned)i + 1, settings.alarms[i].label,
                 phases[i] == CHRONOS_ALARM_RINGING ? "ringing/playback requested" :
                 phases[i] == CHRONOS_ALARM_SNOOZED ? "snoozed" : "dismissed/finished");
    }
    int next_led = chronos_alarms_led(&runtime.alarms);
    if (next_led != led_mode) {
        const char* modes[] = {"off", "enabled (green)", "snoozed (amber blink)", "missed (red blink)", "ringing (fast red blink)"};
        ESP_LOGI(TAG, "LED A: %s", modes[next_led]);
        led_mode = next_led;
    }
    bool next_audio = chronos_alarms_status(&runtime.alarms).ringing_mask ||
                      (runtime.countdown_buzzer && runtime.completion_sounding);
    if (audio_loop != next_audio) { ESP_LOGI(TAG, "Alert playback %s", next_audio ? "started" : "stopped"); audio_loop = next_audio; }
}

/* Report hardware errors while keeping independent subsystems available. */
static void report(const char* operation, esp_err_t error) {
    if (error != ESP_OK) ESP_LOGW(TAG, "%s: %s", operation, esp_err_to_name(error));
}
enum { SOUND_DIRECTORY_COUNT = 5 };
/* Canonical AppFS slug plus the original manually copied package name. */
static void sound_directories(const char* directories[SOUND_DIRECTORY_COUNT]) {
    directories[0] = platform_sd_directory();
    directories[1] = directories[0] ? "/sd/apps/com.github.siviqt.chronos" : NULL;
    directories[2] = directories[0] ? "/sd/apps/chronos" : NULL;
    directories[3] = "/int/apps/com.github.siviqt.chronos";
    directories[4] = "/int/apps/chronos";
}

/* Prefer user sounds, then package assets on SD, then internal package assets. */
static void load_sound(void) {
    ESP_LOGI(TAG, "Loading alarm sound: %s", settings.alarm_sound[0] ? settings.alarm_sound : "built-in tone");
    if (!settings.alarm_sound[0]) {
        report("Alarm sound", platform_audio_load(NULL, NULL));
        return;
    }
    const char* directories[SOUND_DIRECTORY_COUNT];
    sound_directories(directories);
    esp_err_t error = ESP_ERR_NOT_FOUND;
    for (size_t i = 0; i < SOUND_DIRECTORY_COUNT; ++i) {
        if (!directories[i]) continue;
        error = platform_audio_load(directories[i], settings.alarm_sound);
        if (error == ESP_OK) break;
    }
    report("Alarm sound; fallback selected on failure", error);
}

/* Merge successful scans; duplicate filenames appear only once in the picker. */
static esp_err_t next_sound_file(const char* current, char next[65]) {
    const char* directories[SOUND_DIRECTORY_COUNT];
    sound_directories(directories);
    esp_err_t result = ESP_ERR_NOT_FOUND;
    next[0] = 0;
    for (size_t i = 0; i < SOUND_DIRECTORY_COUNT; ++i) {
        if (!directories[i]) continue;
        char candidate[65] = {0};
        esp_err_t error = platform_audio_next_file(directories[i], current, candidate);
        if (error != ESP_OK) continue;
        result = ESP_OK;
        if (candidate[0] && (!next[0] || strcmp(candidate, next) < 0)) strcpy(next, candidate);
    }
    return result;
}
static bool loading(bool display_ready, const char* status) {
    if (!display_ready || !platform_display_buffer()) return false;
    chronos_ui_splash(platform_display_buffer(), status);
    esp_err_t error = platform_display_present();
    report("Splash display", error);
    return error == ESP_OK;
}
/* Serialize input, rendering, timing and hardware services in one owning task. */
void chronos_app_run(void) {
    ESP_LOGI(TAG, "CHRONOS starting");
    esp_err_t error = platform_board_init();
    if (error != ESP_OK) {
        report("Board initialization", error);
        platform_return_to_launcher();
        return;
    }
    bool display_ready = (error = platform_display_init()) == ESP_OK;
    report("Display initialization", error);
    display_ready = loading(display_ready, "Loading audio...");
    report("Startup illumination", platform_illumination(0, 70));
    chronos_ui_t ui          = {0};
    uint64_t     next_frame  = 0;
    bool         frame_presented = false;
    bool         audio_ready = (error = platform_audio_init()) == ESP_OK;
    report("Audio initialization", error);
    bool leds_ready = (error = platform_leds_init()) == ESP_OK;
    report("LED initialization", error);
    chronos_settings_defaults(&settings);
    display_ready = loading(display_ready, "Loading settings...");
    report("Internal asset storage", platform_internal_mount());
    settings_result_t storage = SETTINGS_UNAVAILABLE;
    error = platform_sd_mount("chronos", platform_clock_shared_controller());
    if (error == ESP_OK)
        storage = chronos_settings_load(platform_sd_directory(), &settings);
    else
        report("SD mount; using defaults", error);
    report("Establish time source", platform_clock_select_source(settings.time_source));
    if (settings.wifi_enabled) {
        display_ready = loading(display_ready, "Starting network...");
        /* ESP-Hosted normally owns slot 1 before app_main. Keep the already
         * mounted card; only non-shared builds need a controller handover. */
        if (!platform_clock_shared_controller()) report("SD bootstrap unmount", platform_sd_unmount());
        report("Radio initialization", platform_clock_radio_init());
        if (!platform_sd_directory())
            report("SD shared mount", platform_sd_mount("chronos", platform_clock_shared_controller()));
        /* A failed bootstrap mount must not suppress settings loading after a
         * successful shared-controller mount. Apply recovered settings before NTP. */
        if (platform_sd_directory()) {
            if (storage != SETTINGS_OK && storage != SETTINGS_RECOVERED)
                storage = chronos_settings_load(platform_sd_directory(), &settings);
        } else storage = SETTINGS_UNAVAILABLE;
        if (settings.wifi_enabled) report("Wi-Fi service", platform_clock_start(settings.ntp_enabled && settings.time_source == CHRONOS_TIME_NTP ? settings.ntp_server : ""));
    }
    ESP_LOGI(TAG, "Settings load result: %d; %s", storage, CHRONOS_SETTINGS_FILENAME);
    if (audio_ready) report("Restore volume", platform_audio_volume_step((int)settings.volume_percent - platform_audio_volume()));
    observed_settings = settings;
    persistence_ready = true;
    persisted_settings = settings;
    persisted_valid = storage == SETTINGS_OK || storage == SETTINGS_RECOVERED;
    settings_dirty = storage == SETTINGS_MISSING;
    save_after = platform_monotonic_us() + 1000000;
    ESP_LOGI(TAG, "Local timezone %s; %u World Clock locations", settings.timezone, settings.city_count);
    for (size_t i = 0; i < settings.city_count; ++i)
        ESP_LOGI(TAG, "World Clock %u: %s (%s)", (unsigned)i + 1, settings.cities[i].name, settings.cities[i].timezone);
    error = platform_illumination(settings.keyboard_percent, settings.display_percent);
    report("Illumination", error);
    uint8_t keyboard = error == ESP_OK ? settings.keyboard_percent : UINT8_MAX;
    uint8_t display = error == ESP_OK ? settings.display_percent : UINT8_MAX;
    const char* notice = platform_sd_directory() ? chronos_settings_notice(storage) : platform_sd_status();
    if (notice) ESP_LOGW(TAG, "%s", notice);
    if (setenv("TZ", settings.timezone, 1) != 0) {
        ESP_LOGE(TAG, "Cannot apply timezone; stopping rather than scheduling in the wrong zone");
        goto cleanup;
    }
    tzset();
    (void)chronos_runtime_init(&runtime, &settings);
    display_ready = loading(display_ready, "Preparing clock...");
    if (audio_ready) load_sound();
    bool splash_visible = display_ready;
    bool external_was_enabled = false;
    display_ready = loading(display_ready, "Press Space to continue");
    ESP_LOGI(TAG, "Services running; audio %u%%. F1 returns to AppFS. GUI ready.",
             platform_audio_volume());
    uint8_t              previous_missed    = 0;
    bool                 led_error_reported = false, audio_error_reported = false;
    uint64_t             next_civil_read = 0, next_illumination = 0, next_battery_read = 0;
    chronos_local_time_t local;
    bool                 local_valid = false;
    int64_t epoch = 0;
    bool dst = false;
    for (;;) {
        uint64_t now = platform_monotonic_us();
        if (now >= next_battery_read) {
            uint8_t percent = 0;
            ui.battery_valid = platform_battery_percent(&percent) == ESP_OK;
            ui.battery_percent = percent;
            next_battery_read = now + 5000000;
        }
        if (now >= next_civil_read) {
            local_valid = platform_clock_seconds(&epoch) == ESP_OK &&
                          chronos_clock_local(epoch, settings.timezone, &local, &dst);
            next_civil_read = now + 1000000;
        }
        chronos_runtime_output_t output = chronos_runtime_tick(&runtime, local_valid ? &local : NULL, now);
        observe_runtime();
        platform_input_t input;
        /* Bound each burst so held keys cannot starve alarms and outputs.
         * Rebuild availability before every key: earlier keys may change tabs. */
        for (unsigned pending = 0; pending < 16 && platform_input_poll(&input, pending ? 0 : 10); ++pending) {
            if (audio_ready) settings.volume_percent = platform_audio_volume();
            observe_settings(platform_monotonic_us());
            if (input.type == PLATFORM_INPUT_NONE) continue;
            if (input.type == PLATFORM_INPUT_F1 && splash_visible) goto cleanup;
            if (splash_visible) {
                if (input.type == PLATFORM_INPUT_TEXT && input.character == ' ') {
                    splash_visible = false;
                    next_frame = 0;
                    continue;
                }
                /* Keep hardware events and backlight controls available while
                 * waiting, without dispatching keys to the hidden screen. */
                if (input.type != PLATFORM_INPUT_SD_INSERTED && input.type != PLATFORM_INPUT_SD_REMOVED &&
                    input.type != PLATFORM_INPUT_F6) continue;
            }
            now = platform_monotonic_us();
            chronos_screen_view(&view, &ui, &runtime, &settings, local_valid ? &local : NULL, now, platform_sd_directory() ? notice : platform_sd_status(),
                                platform_sd_directory() != NULL);
            chronos_clock_view(&view, &ui, &settings, local_valid, epoch, platform_clock_status());
            if (input.type == PLATFORM_INPUT_F6) chronos_runtime_toggle_backlights(&runtime);
            chronos_ui_key_t key;
            bool navigation = true;
            switch (input.type) {
                case PLATFORM_INPUT_F1: key = UI_F1; break;
                case PLATFORM_INPUT_ESCAPE: key = UI_ESCAPE; break;
                case PLATFORM_INPUT_LEFT: key = UI_LEFT; break;
                case PLATFORM_INPUT_RIGHT: key = UI_RIGHT; break;
                case PLATFORM_INPUT_UP: key = UI_UP; break;
                case PLATFORM_INPUT_DOWN: key = UI_DOWN; break;
                case PLATFORM_INPUT_ENTER: key = UI_ENTER; break;
                case PLATFORM_INPUT_F2: key = UI_F2; break;
                case PLATFORM_INPUT_F3: key = UI_F3; break;
                case PLATFORM_INPUT_F4: key = UI_F4; break;
                case PLATFORM_INPUT_F5: key = UI_F5; break;
                case PLATFORM_INPUT_TAB: key = UI_TAB; break;
                default: navigation = false; break;
            }
            if (navigation) {
                chronos_screen_key(key, &ui, &view, &runtime, &settings, now);
                if (ui.exit_requested) goto cleanup;
                if (ui.sync_requested) {
                    if (settings.wifi_enabled && settings.ntp_enabled && settings.time_source == CHRONOS_TIME_NTP)
                        platform_clock_request_sync();
                    else notice = "NTP sync requires Wi-Fi On and NTP source enabled";
                    ui.sync_requested = false;
                }
                if (ui.clock_reconfigure_requested) {
                    platform_clock_stop();
                    error = platform_clock_select_source(settings.time_source);
                    report("Apply time source", error);
                    if (error != ESP_OK) notice = "Time source failed; check hardware logs";
                    if (ui.manual_requested) {
                        error = platform_clock_set_manual(ui.manual_seconds);
                        report("Set manual system time", error);
                        notice = error == ESP_OK ? "Manual time applied; RTC unchanged" : "Manual time failed; check logs";
                        ui.manual_requested = false;
                    }
                    if (settings.wifi_enabled) {
                        if (!platform_clock_shared_controller()) report("SD unmount for radio", platform_sd_unmount());
                        report("Radio initialization", platform_clock_radio_init());
                        if (!platform_sd_directory()) {
                            report("SD shared mount", platform_sd_mount("chronos", platform_clock_shared_controller()));
                            if (audio_ready) load_sound();
                        }
                        report("Wi-Fi service", platform_clock_start(settings.ntp_enabled && settings.time_source == CHRONOS_TIME_NTP ? settings.ntp_server : ""));
                    }
                    if (!settings.wifi_enabled && observed_settings.wifi_enabled) {
                        error = platform_sd_unmount();
                        report("SD unmount before radio power-off", error);
                        if (error == ESP_OK) error = platform_clock_radio_shutdown();
                        report("Radio power-off", error);
                        if (error != ESP_OK) notice = "Radio power-off failed; check logs";
                        report("SD remount after radio power-off", platform_sd_mount("chronos", platform_clock_shared_controller()));
                        /* Audio is preloaded in RAM; do not interrupt a ringing clip. */
                    }
                    ui.clock_reconfigure_requested = false;
                    next_civil_read = 0;
                }
                if (ui.sound_scan_requested) {
                    char next[65];
                    error = next_sound_file(ui.clock_server, next);
                    report("Discover alarm WAV", error);
                    if (error == ESP_OK) strcpy(ui.clock_server, next);
                    else notice = "Cannot list alarm sounds; check storage and logs";
                    ui.sound_scan_requested = false;
                }
                if (ui.sound_reload_requested) {
                    if (audio_ready) load_sound();
                    ui.sound_reload_requested = false;
                }
                if (ui.settings_save_requested) {
                    observe_settings(now);
                    settings_result_t saved = save_settings(now);
                    notice = saved == SETTINGS_OK ? "Settings saved" : chronos_settings_notice(saved);
                    ui.settings_save_requested = false;
                }
                next_frame = 0;
            }
            if (input.type == PLATFORM_INPUT_TEXT) {
                if (!chronos_runtime_key(&runtime, input.character, platform_monotonic_us()))
                    chronos_screen_text(input.character, &ui);
                next_frame = 0;
                bool    beat    = output.pacer_beat;
                uint8_t started = output.alarms.started_mask;
                output          = chronos_runtime_tick(&runtime, local_valid ? &local : NULL, platform_monotonic_us());
                output.pacer_beat          |= beat;
                output.alarms.started_mask |= started;
            } else if (input.type == PLATFORM_INPUT_SD_REMOVED) {
                if (audio_ready) (void)platform_audio_load(NULL, NULL);
                report("SD unmount", platform_sd_unmount());
                if (audio_ready) load_sound();
                notice = platform_sd_status();
                ESP_LOGW(TAG, "%s", notice);
            } else if (input.type == PLATFORM_INPUT_SD_INSERTED) {
                error = platform_sd_mount("chronos", platform_clock_shared_controller());
                report("SD insertion", error);
                if (error == ESP_OK) {
                    notice = "SD available; session settings retained";
                    ESP_LOGI(TAG, "SD available; retaining current session settings");
                    if (audio_ready) load_sound();
                }
            }
            if (strcmp(settings.timezone, observed_settings.timezone)) {
                local_valid = platform_clock_seconds(&epoch) == ESP_OK &&
                              chronos_clock_local(epoch, settings.timezone, &local, &dst);
                next_civil_read = 0;
            }
            observe_settings(platform_monotonic_us());
            /* Apply navigation changes to outputs in this same iteration. */
            bool beat = output.pacer_beat;
            uint8_t started = output.alarms.started_mask;
            output = chronos_runtime_tick(&runtime, local_valid ? &local : NULL, platform_monotonic_us());
            output.pacer_beat |= beat;
            output.alarms.started_mask |= started;
            observe_runtime();
        }
        now = platform_monotonic_us();
        observe_runtime();
        observe_settings(now);
        if (settings.external.enabled || external_was_enabled)
            chronos_external_update(&settings, &runtime, local_valid ? &local : NULL, epoch, now, ui.external_retry_requested);
        if (settings.external.enabled || external_was_enabled || ui.external_status != EXT_DISABLED)
            ui.external_status = external_display_status();
        external_was_enabled = settings.external.enabled;
        ui.external_retry_requested = false;
        if (settings_dirty && platform_sd_directory() && now >= save_after) {
            settings_result_t saved = save_settings(now);
            notice = saved == SETTINGS_OK ? "Settings saved automatically" : chronos_settings_notice(saved);
        }
        /* A new alarm wins over an F6 press in the same iteration. */
        if (output.alarms.started_mask) runtime.display_awake = true;
        uint8_t desired_keyboard, desired_display;
        chronos_runtime_brightness(&runtime, &settings, &desired_keyboard, &desired_display);
        if ((keyboard != desired_keyboard || display != desired_display) && now >= next_illumination) {
            error = platform_illumination(desired_keyboard, desired_display);
            report("Illumination", error);
            if (error != ESP_OK) notice = "Illumination could not be applied to hardware.";
            if (error == ESP_OK) {
                keyboard = desired_keyboard; display = desired_display;
                ESP_LOGI(TAG, "Illumination: keyboard %u%%, display %u%%", keyboard, display);
            }
            next_illumination = error == ESP_OK ? 0 : now + 1000000;
        }
        if (display_ready && !splash_visible && now >= next_frame) {
            chronos_screen_view(&view, &ui, &runtime, &settings, local_valid ? &local : NULL, now, platform_sd_directory() ? notice : platform_sd_status(),
                                platform_sd_directory() != NULL);
            chronos_clock_view(&view, &ui, &settings, local_valid, epoch, platform_clock_status());
            /* Poll animation/time changes at 10 Hz, but do not spend a full
             * software render and display transfer on an identical screen.
             * Padding differences can only cause an extra redraw. */
            if (!frame_presented || memcmp(&ui, &presented_ui, sizeof(ui)) ||
                memcmp(&view, &presented_view, sizeof(view))) {
                chronos_ui_render(platform_display_buffer(), &ui, &view);
                error = platform_display_present();
                if (error != ESP_OK) {
                    report("Display update", error);
                    display_ready = false;
                } else {
                    presented_ui = ui;
                    presented_view = view;
                    frame_presented = true;
                }
            }
            /* Rendering time must not make the next frame overdue already. */
            next_frame = platform_monotonic_us() + 100000;
        }
        if (audio_ready) {
            platform_audio_pacer_active(runtime.pacer.session.state == TIMING_RUNNING);
            platform_audio_loop(output.audio_loop);
            if (output.pacer_beat) platform_audio_pulse();
            error = platform_audio_error();
            if (error != ESP_OK && !audio_error_reported) {
                report("Audio output", error);
                audio_error_reported = true;
            }
        }
        if (leds_ready) {
            error = platform_leds_brightness(settings.led_percent);
            if (error == ESP_OK) error = platform_leds_write(output.leds.alarm_rgb, output.leds.timing_rgb);
            if (error != ESP_OK && !led_error_reported) report("LED output", error);
            led_error_reported = error != ESP_OK;
        }
        if (output.alarms.missed_mask != previous_missed) {
            for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i)
                if (output.alarms.missed_mask & (1u << i)) ESP_LOGW(TAG, "Missed alarm: %s", settings.alarms[i].label);
            previous_missed = output.alarms.missed_mask;
        }
    }
cleanup:
    ESP_LOGI(TAG, "CHRONOS shutting down; clearing audio and A/B LEDs");
    if (persistence_ready) {
        if (audio_ready) settings.volume_percent = platform_audio_volume();
        observe_settings(platform_monotonic_us());
        if (settings_dirty) (void)save_settings(platform_monotonic_us());
    }
    external_display_stop();
    platform_clock_stop();
    chronos_runtime_stop(&runtime);
    report("Audio shutdown", platform_audio_shutdown());
    report("LED restoration", platform_leds_shutdown());
    report("SD shutdown", platform_sd_unmount());
    report("Internal storage shutdown", platform_internal_unmount());
    if (display_ready) {
        report("Exit illumination", platform_illumination(settings.keyboard_percent, settings.display_percent ? settings.display_percent : 70));
        loading(true, "CHRONOS signing off...");
        sleep(3);
    }
    platform_display_shutdown();
    platform_return_to_launcher();
}
