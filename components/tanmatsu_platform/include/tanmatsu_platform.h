#ifndef TANMATSU_PLATFORM_H
#define TANMATSU_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* One application owner calls these APIs after board initialization. The audio
 * worker alone writes I2S; it never accesses storage. No functions write RTC. */
esp_err_t platform_board_init(void);
void      platform_return_to_launcher(void);
uint64_t  platform_monotonic_us(void);
esp_err_t platform_rtc_seconds(uint32_t* seconds);

/* SDIO builds initially own the shared controller before app_main via ESP-Hosted.
 * Ownership changes on full radio shutdown/restart; query it before SD mounting.
 * The worker uses AppFS WiFi credentials; clock reads never write hardware RTC. */
esp_err_t platform_clock_radio_init(void);
/* Call after stopping clock worker and unmounting SD; remount SD afterwards. */
esp_err_t platform_clock_radio_shutdown(void);
bool platform_clock_shared_controller(void);
esp_err_t platform_clock_start(const char* hostname);
/* Source values match Chronos NTP=0, RTC=1, Manual=2. Call with worker stopped.
 * RTC is sampled into system RAM now. Manual awaits entry on fresh startup. */
esp_err_t platform_clock_select_source(unsigned source);
esp_err_t platform_clock_set_manual(int64_t seconds);
void platform_clock_request_sync(void);
const char* platform_clock_status(void);
esp_err_t platform_clock_seconds(int64_t* seconds);
void platform_clock_stop(void);

/* SD slot 0 is mounted without formatting. subdirectory is a plain name, created
 * only after successful mounting. shared_controller=true requires an already
 * initialized ESP-Hosted slot 1; keep that owner alive until SD is unmounted. */
esp_err_t   platform_sd_mount(const char* subdirectory, bool shared_controller);
esp_err_t   platform_sd_unmount(void);
const char* platform_sd_directory(void); /* NULL unless mounted and directory usable. */
const char* platform_sd_status(void); /* Last mount stage/result for GUI diagnostics. */
esp_err_t   platform_sd_present(bool* present);

/* Mount existing internal FAT assets at /int without formatting. */
esp_err_t platform_internal_mount(void);
esp_err_t platform_internal_unmount(void);

/* Initialize at 70% volume. WAV files must be PCM16, 8–48 kHz (resampled to 44.1 kHz), mono/stereo,
 * <=2 MiB source and converted audio data. A missing/invalid file selects generated fallback audio.
 * NULL/empty filename selects fallback without an error. Load only from owner. */
esp_err_t platform_audio_next_file(const char* directory, const char* current, char next[65]);
esp_err_t platform_audio_init(void);
esp_err_t platform_audio_load(const char* directory, const char* filename);
void      platform_audio_loop(bool enabled);
/* Keep the output ready between beats; disabling lets the current beat finish. */
void      platform_audio_pacer_active(bool enabled);
void      platform_audio_pulse(void);
esp_err_t platform_audio_volume_step(int step_percent);
uint8_t   platform_audio_volume(void);
void      platform_audio_headphones(bool inserted);
esp_err_t platform_audio_error(void);
esp_err_t platform_audio_shutdown(void);

/* Control user LEDs 4/5 (A/B), with low per-channel brightness, retaining
 * automatic system LEDs. Initialize snapshots mode/brightness for cleanup. */
esp_err_t platform_leds_init(void);
esp_err_t platform_leds_brightness(uint8_t percent);
/* Read-only capacity; errors / absent battery leave the output unchanged. */
esp_err_t platform_battery_percent(uint8_t* percent);
esp_err_t platform_leds_write(uint32_t alarm_rgb, uint32_t timing_rgb);
esp_err_t platform_leds_shutdown(void);

typedef enum {
    PLATFORM_INPUT_NONE,
    PLATFORM_INPUT_TEXT,
    PLATFORM_INPUT_ESCAPE,
    PLATFORM_INPUT_LEFT,
    PLATFORM_INPUT_RIGHT,
    PLATFORM_INPUT_UP,
    PLATFORM_INPUT_DOWN,
    PLATFORM_INPUT_ENTER,
    PLATFORM_INPUT_F5,
    PLATFORM_INPUT_F6,
    PLATFORM_INPUT_F1,
    PLATFORM_INPUT_F2,
    PLATFORM_INPUT_F3,
    PLATFORM_INPUT_F4,
    PLATFORM_INPUT_TAB,
    PLATFORM_INPUT_SD_INSERTED,
    PLATFORM_INPUT_SD_REMOVED
} platform_input_type_t;
typedef struct {
    platform_input_type_t type;
    unsigned int          character;
} platform_input_t;
/* Dispatches side volume and headphone events. Remaining input will be extended
 * with the GUI; SD events are returned so owner can close audio before unmount. */
bool                   platform_input_poll(platform_input_t* event, uint32_t wait_ms);
typedef struct pax_buf pax_buf_t;
esp_err_t              platform_display_init(void);
pax_buf_t*             platform_display_buffer(void);
esp_err_t              platform_display_present(void);
esp_err_t platform_illumination(uint8_t keyboard, uint8_t display);
void                   platform_display_shutdown(void);
#endif
