#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <strings.h>
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "bsp/audio.h"
#include "bsp/input.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "platform_pcm.h"
#include "tanmatsu_platform.h"

#define AUDIO_FRAMES 256
static i2s_chan_handle_t channel;
static SemaphoreHandle_t lock, stopped;
static platform_pcm_t    clip;
static bool              loop_enabled, pacer_active, quit, headphones;
static uint32_t          pulse_left;
static uint64_t          position, pulse_position;
static uint8_t           volume_percent = 70;
static esp_err_t         last_error;

/* Retain the first hardware failure for the application to report without log flooding. */
static void record_error(esp_err_t error) {
    if (error == ESP_OK) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (last_error == ESP_OK) last_error = error;
    xSemaphoreGive(lock);
}
/* A muted/failed playback may leave nonzero DMA samples. Clear every descriptor
 * through the IDF preload API before restarting the clock or speaker. */
static esp_err_t clear_dma(void) {
    static const int16_t silence[AUDIO_FRAMES * 2] = {0};
    size_t loaded;
    do {
        esp_err_t error = i2s_channel_preload_data(channel, silence, sizeof(silence), &loaded);
        if (error != ESP_OK) return error;
    } while (loaded == sizeof(silence));
    return ESP_OK;
}
/* A timeout may follow a partial copy. Resume that same block instead of
 * dropping its tail and cycling the speaker. Bound retries so shutdown cannot
 * hang on a failed device; mute/shutdown still interrupt immediately. */
static esp_err_t write_block(const int16_t* samples, size_t bytes) {
    size_t offset = 0;
    for (unsigned attempt = 0; offset < bytes && attempt < 4; ++attempt) {
        xSemaphoreTake(lock, portMAX_DELAY);
        bool cancelled = quit || !volume_percent;
        xSemaphoreGive(lock);
        if (cancelled) return ESP_OK;
        size_t written = 0;
        esp_err_t error = i2s_channel_write(channel, (const uint8_t*)samples + offset,
                                            bytes - offset, &written, 40);
        if (written > bytes - offset) return ESP_FAIL;
        offset += written;
        if (error != ESP_OK && error != ESP_ERR_TIMEOUT) return error;
    }
    return offset == bytes ? ESP_OK : ESP_ERR_TIMEOUT;
}
/* Render small stereo blocks while holding clip ownership, then write outside the lock. */
static void audio_worker(void* unused) {
    (void)unused;
    int16_t  samples[AUDIO_FRAMES * 2];
    bool     amplifier_on    = false;
    bool     channel_on = false;
    unsigned silent_blocks = 0;
    for (;;) {
        xSemaphoreTake(lock, portMAX_DELAY);
        bool     exiting = quit;
        bool     active  = loop_enabled || pulse_left > 0;
        bool audible = active && volume_percent > 0 && !exiting;
        if (audible) silent_blocks = 0;
        bool want_channel = !exiting && volume_percent > 0 && (audible || pacer_active || (channel_on && silent_blocks < 8));
        bool want_amplifier = want_channel && !headphones;
        for (size_t i = 0; i < AUDIO_FRAMES; ++i) {
            int16_t left = 0, right = 0;
            if (!exiting && loop_enabled) {
                if (clip.data) {
                    size_t frame = (size_t)(position % clip.frames);
                    left         = platform_pcm_sample(&clip, frame, 0);
                    right        = platform_pcm_sample(&clip, frame, 1);
                } else
                    left = right = platform_pcm_tone(position, false);
                ++position;
            } else if (!exiting && pulse_left) {
                left = right = platform_pcm_tone(pulse_position++, true);
                --pulse_left;
            }
            samples[i * 2]     = volume_percent ? left : 0;
            samples[i * 2 + 1] = volume_percent ? right : 0;
        }
        xSemaphoreGive(lock);
        if (want_channel && !channel_on) {
            esp_err_t error = clear_dma();
            if (error == ESP_OK) error = i2s_channel_enable(channel);
            record_error(error);
            if (error != ESP_OK) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
            channel_on = true;
        }
        if (want_amplifier != amplifier_on) {
            esp_err_t error = bsp_audio_set_amplifier(want_amplifier);
            record_error(error);
            if (error == ESP_OK) {
                amplifier_on = want_amplifier;
                /* Coprocessor applies speaker enable on its 50 ms input scan.
                 * Keep the preloaded silence playing until that scan and the
                 * amplifier settle; otherwise a short beat can be mostly lost. */
                if (amplifier_on) vTaskDelay(pdMS_TO_TICKS(60));
            }
        }
        if (!want_channel && channel_on) {
            esp_err_t error = i2s_channel_disable(channel);
            record_error(error);
            if (error == ESP_OK) channel_on = false;
        }
        if (exiting) break;
        if (!want_channel) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        esp_err_t error = write_block(samples, sizeof(samples));
        if (error == ESP_OK && !audible && silent_blocks < 8) ++silent_blocks;
        if (error != ESP_OK) {
            record_error(error);
            esp_err_t mute_error = bsp_audio_set_amplifier(false);
            record_error(mute_error);
            if (mute_error == ESP_OK) amplifier_on = false;
            esp_err_t stop_error = i2s_channel_disable(channel);
            record_error(stop_error);
            if (stop_error == ESP_OK) channel_on = false;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    record_error(bsp_audio_set_amplifier(false));
    if (channel_on) record_error(i2s_channel_disable(channel));
    xSemaphoreGive(stopped);
    vTaskDelete(NULL);
}
/* BSP owns codec initialization; leave its I2S channel disabled until playback. */
esp_err_t platform_audio_init(void) {
    if (lock) return ESP_ERR_INVALID_STATE;
    esp_err_t error = bsp_audio_get_i2s_handle(&channel);
    if (error != ESP_OK || !channel) return error == ESP_OK ? ESP_ERR_INVALID_STATE : error;
    lock    = xSemaphoreCreateMutex();
    stopped = xSemaphoreCreateBinary();
    if (!lock || !stopped) {
        error = ESP_ERR_NO_MEM;
        goto failed;
    }
    loop_enabled = pacer_active = quit = false;
    pulse_left          = 0;
    position = pulse_position = 0;
    last_error                = ESP_OK;
    headphones                = false;
    (void)bsp_input_read_action(BSP_INPUT_ACTION_TYPE_AUDIO_JACK, &headphones);
    error = bsp_audio_set_amplifier_force(false);
    if (error != ESP_OK) goto failed;
    error = bsp_audio_set_amplifier(false);
    if (error != ESP_OK) goto failed;
    error = i2s_channel_disable(channel);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) goto failed;
    error = bsp_audio_set_rate(PLATFORM_AUDIO_RATE);
    if (error != ESP_OK) goto failed;
    volume_percent = 70;
    error          = bsp_audio_set_volume(volume_percent);
    if (error != ESP_OK) goto failed;
    if (xTaskCreate(audio_worker, "chronos_audio", 4096, NULL, 5, NULL) != pdPASS) {
        error = ESP_ERR_NO_MEM;
        goto failed;
    }
    return ESP_OK;
failed:
    (void)bsp_audio_set_amplifier(false);
    if (channel) (void)i2s_channel_disable(channel);
    if (lock) vSemaphoreDelete(lock);
    if (stopped) vSemaphoreDelete(stopped);
    lock = stopped = NULL;
    return error;
}
/* Reject traversal and allow only a plain filename inside the supplied app directory. */
static bool sound_name_valid(const char* name) {
    if (!strcmp(name, ".") || !strcmp(name, "..")) return false;
    for (size_t i = 0; name[i]; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (i >= 64 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                         c == '_' || c == '-'))
            return false;
    }
    return true;
}
/* Preload custom sound in the owner task; atomically swap to it or to fallback. */
esp_err_t platform_audio_load(const char* directory, const char* filename) {
    if (!lock) return ESP_ERR_INVALID_STATE;
    platform_pcm_t next  = {0};
    esp_err_t      error = ESP_OK;
    if (filename && filename[0]) {
        if (!directory || !sound_name_valid(filename))
            error = ESP_ERR_INVALID_ARG;
        else {
            char path[256];
            int  count = snprintf(path, sizeof(path), "%s/%s", directory, filename);
            if (count < 0 || (size_t)count >= sizeof(path))
                error = ESP_ERR_INVALID_SIZE;
            else {
                FILE* file = fopen(path, "rb");
                if (!file) {
                    ESP_LOGW("chronos_audio", "Cannot open %s: %s", path, strerror(errno));
                    error = ESP_ERR_NOT_FOUND;
                }
                else {
                    if (!platform_pcm_load(file, &next)) {
                        ESP_LOGW("chronos_audio", "%s: invalid/truncated WAV, allocation failure, or unsupported format; require PCM16 mono/stereo 8000-48000 Hz, <=2 MiB input and converted audio", path);
                        error = ESP_ERR_NOT_SUPPORTED;
                    } else ESP_LOGI("chronos_audio", "Loaded %s: %u frames at 44100 Hz, %u channels", path, (unsigned)next.frames, next.channels);
                    if (fclose(file)) {
                        platform_pcm_free(&next);
                        error = ESP_FAIL;
                    }
                }
            }
        }
    }
    if (error != ESP_OK) ESP_LOGW("chronos_audio", "Sound '%s' in '%s': %s; using built-in alarm", filename ? filename : "", directory ? directory : "SD unavailable", esp_err_to_name(error));
    xSemaphoreTake(lock, portMAX_DELAY);
    platform_pcm_t previous = clip;
    clip                    = next;
    position                = 0;
    xSemaphoreGive(lock);
    platform_pcm_free(&previous);
    return error;
}
/* Start/stop continuous alert audio without restarting an already-running clip. */
void platform_audio_loop(bool enabled) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (enabled != loop_enabled) {
        position   = 0;
        pulse_left = 0;
    }
    loop_enabled = enabled;
    xSemaphoreGive(lock);
}
/* Keep DMA fed with silence between beats while the pacer is running. This
 * avoids repeated asynchronous speaker/codec startups within a cadence. On
 * pause/stop, finish the current pulse and normal drain before powering down. */
void platform_audio_pacer_active(bool enabled) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    pacer_active = enabled;
    xSemaphoreGive(lock);
}
/* Coalesce cadence requests to one 100 ms generated tone; continuous alerts win. */
void platform_audio_pulse(void) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (!loop_enabled && !pulse_left) {
        pulse_left     = PLATFORM_PACER_FRAMES;
        pulse_position = 0;
    }
    xSemaphoreGive(lock);
}
/* Apply bounded side-button volume changes once per press, without writing system NVS. */
esp_err_t platform_audio_volume_step(int step_percent) {
    if (!lock) return ESP_ERR_INVALID_STATE;
    if (step_percent < -100 || step_percent > 100) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(lock, portMAX_DELAY);
    int next = volume_percent + step_percent;
    if (next < 0) next = 0;
    if (next > 100) next = 100;
    esp_err_t error = bsp_audio_set_volume((float)next);
    if (error == ESP_OK) volume_percent = (uint8_t)next;
    xSemaphoreGive(lock);
    record_error(error);
    return error;
}
/* Report the commanded session volume, independent of BSP codec scaling/rounding. */
uint8_t platform_audio_volume(void) {
    if (!lock) return volume_percent;
    xSemaphoreTake(lock, portMAX_DELAY);
    uint8_t result = volume_percent;
    xSemaphoreGive(lock);
    return result;
}
/* Update routing; the worker mutes the speaker whenever headphones are inserted. */
void platform_audio_headphones(bool inserted) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    headphones = inserted;
    xSemaphoreGive(lock);
}
/* Return any latched hardware fault under the worker's shared lock. */
esp_err_t platform_audio_error(void) {
    if (!lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(lock, portMAX_DELAY);
    esp_err_t error = last_error;
    xSemaphoreGive(lock);
    return error;
}
/* Join the worker before releasing clip memory or allowing storage teardown. */
esp_err_t platform_audio_shutdown(void) {
    if (!lock) return ESP_OK;
    xSemaphoreTake(lock, portMAX_DELAY);
    quit = true;
    xSemaphoreGive(lock);
    if (xSemaphoreTake(stopped, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t error = platform_audio_error();
    platform_pcm_free(&clip);
    vSemaphoreDelete(stopped);
    vSemaphoreDelete(lock);
    stopped = lock = NULL;
    return error;
}

/* Owner-only directory enumeration. Empty selection is the built-in choice;
 * lexical iteration is bounded in memory and preserves actual filename case. */
esp_err_t platform_audio_next_file(const char* directory, const char* current, char next[65]) {
    if (!directory || !current || !next) return ESP_ERR_INVALID_ARG;
    DIR* dir = opendir(directory);
    if (!dir) { ESP_LOGW("chronos_audio", "Cannot enumerate %s: %s", directory, strerror(errno)); return ESP_FAIL; }
    next[0] = 0;
    struct dirent* entry;
    errno = 0;
    while ((entry = readdir(dir))) {
        const char* name = entry->d_name;
        size_t n = strlen(name);
        if (n < 5 || !sound_name_valid(name) || strcasecmp(name + n - 4, ".wav")) continue;
        if (strcmp(name, current) > 0 && (!next[0] || strcmp(name, next) < 0)) strcpy(next, name);
    }
    int failure = errno;
    if (closedir(dir)) failure = errno;
    if (failure) { ESP_LOGW("chronos_audio", "Enumeration %s: %s", directory, strerror(failure)); return ESP_FAIL; }
    return ESP_OK;
}
