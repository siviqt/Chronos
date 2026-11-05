#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
/* Run the real worker deterministically; driver hooks model DMA and idle wakeups. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../components/tanmatsu_platform/src/platform_audio.c"
static bool hw_channel, hw_amplifier, jack, fail_enable;
static unsigned writes, silent_writes, enables, amp_enables, preload_bytes, idle_calls;
static int scenario;
static bool speaker_settled;
static size_t pulse_checked;
static unsigned pulses_completed;
static void (*worker)(void*);
static int mutex_token, stopped_token;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &mutex_token; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return &stopped_token; }
int xSemaphoreTake(SemaphoreHandle_t s, uint32_t wait) { (void)s; (void)wait; return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* arg, unsigned priority, void* task) {
    (void)name; (void)stack; (void)arg; (void)priority; (void)task; worker = fn; return pdPASS;
}
void vTaskDelete(void* task) { (void)task; }
void vTaskDelay(uint32_t ticks) {
    if (ticks == 60 && hw_amplifier) { speaker_settled = true; return; }
    ++idle_calls;
    assert(idle_calls < 10);
    assert(!hw_amplifier);
    if (scenario == 5 && idle_calls == 1) {
        assert(!hw_channel);
        assert(platform_audio_volume_step(70) == ESP_OK);
        return;
    }
    quit = true;
}
esp_err_t bsp_audio_get_i2s_handle(i2s_chan_handle_t* out) { *out = &mutex_token; return ESP_OK; }
esp_err_t bsp_audio_set_rate(uint32_t rate) { assert(!hw_channel && rate == PLATFORM_AUDIO_RATE); return ESP_OK; }
esp_err_t bsp_audio_set_volume(float percent) { assert(percent >= 0 && percent <= 100); return ESP_OK; }
esp_err_t bsp_audio_set_amplifier_force(bool force) { assert(!force); return ESP_OK; }
esp_err_t bsp_audio_set_amplifier(bool on) {
    if (on) { assert(hw_channel && !jack); ++amp_enables; }
    speaker_settled = false;
    hw_amplifier = on; return ESP_OK;
}
esp_err_t bsp_input_read_action(int action, bool* state) { (void)action; *state = jack; return ESP_OK; }
esp_err_t i2s_channel_enable(i2s_chan_handle_t c) {
    (void)c; assert(!hw_channel && preload_bytes == 5760);
    if (fail_enable) return ESP_FAIL;
    hw_channel = true; ++enables; return ESP_OK;
}
esp_err_t i2s_channel_disable(i2s_chan_handle_t c) {
    (void)c; assert(!hw_amplifier);
    bool was_on = hw_channel; hw_channel = false; preload_bytes = 0;
    return was_on ? ESP_OK : ESP_ERR_INVALID_STATE;
}
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t c, const void* data, size_t size, size_t* loaded) {
    (void)c; assert(!hw_channel);
    const uint8_t* bytes = data;
    for (size_t i = 0; i < size; ++i) assert(!bytes[i]);
    *loaded = size < 5760 - preload_bytes ? size : 5760 - preload_bytes;
    preload_bytes += (unsigned)*loaded; return ESP_OK;
}
esp_err_t i2s_channel_write(i2s_chan_handle_t c, const void* data, size_t size, size_t* written, uint32_t timeout) {
    (void)c; (void)timeout; assert(hw_channel);
    if (hw_amplifier) assert(speaker_settled);
    const int16_t* samples = data; bool silent = true;
    for (size_t i = 0; i < size / 2; ++i) if (samples[i]) silent = false;
    ++writes; if (silent) ++silent_writes;
    assert(writes < 100);
    *written = size;
    if (scenario == 11) { *written = 0; return ESP_ERR_TIMEOUT; }
    if (scenario == 10 && writes == 1) { *written = 0; return ESP_ERR_TIMEOUT; }
    bool partial = scenario == 9 && writes == 1;
    if (partial) *written = 128;
    if (scenario == 9 || scenario == 10 || scenario == 12) {
        /* Compare every accepted stereo sample: neither a retry nor stop may
         * skip, duplicate, truncate, or restart any part of the pulse. */
        assert(*written % 4 == 0);
        for (size_t i = 0; i < *written / 4; ++i) {
            int16_t expected = pulse_checked < PLATFORM_PACER_FRAMES ?
                platform_pcm_tone(pulse_checked, true) : 0;
            assert(samples[2 * i] == expected && samples[2 * i + 1] == expected);
            if (pulse_checked < PLATFORM_PACER_FRAMES && ++pulse_checked == PLATFORM_PACER_FRAMES)
                ++pulses_completed;
        }
        if (scenario == 12 && writes == 35) {
            assert(pulses_completed == 1 && hw_amplifier && enables == 1);
            platform_audio_pulse(); pulse_checked = 0;
        }
        if (scenario == 12 && writes == 38) platform_audio_pacer_active(false);
    }
    if (partial) return ESP_ERR_TIMEOUT;
    if (scenario == 7) { *written = 0; return ESP_FAIL; }
    if ((scenario == 1 || scenario == 3) && writes == 3) platform_audio_loop(false);
    if (scenario == 5 && writes == 2) assert(platform_audio_volume_step(-70) == ESP_OK);
    if (scenario == 5 && writes == 4) platform_audio_loop(false);
    return ESP_OK;
}
static void run(int mode) {
    pulse_checked = pulses_completed = 0;
    scenario = mode; writes = silent_writes = enables = amp_enables = preload_bytes = idle_calls = 0;
    hw_channel = true; hw_amplifier = false; jack = mode == 3; fail_enable = mode == 6;
    assert(platform_audio_init() == ESP_OK);
    assert(!hw_channel && !hw_amplifier); /* Never enable merely to initialize. */
    if (mode == 1 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7) platform_audio_loop(true);
    if (mode == 2 || mode == 8) {
        platform_audio_pulse();
        pulse_left -= 100; pulse_position = 100;
        platform_audio_pulse();
        assert(pulse_left == PLATFORM_PACER_FRAMES - 100 && pulse_position == 100);
    }
    if (mode == 4) assert(platform_audio_volume_step(-100) == ESP_OK);
    if (mode == 8) assert(platform_audio_volume_step(30) == ESP_OK && volume_percent == 100);
    if (mode >= 9 && mode <= 12) platform_audio_pulse();
    if (mode == 12 || mode == 13) platform_audio_pacer_active(true);
    if (mode == 13) assert(platform_audio_volume_step(-100) == ESP_OK);
    worker(NULL);
    assert(!hw_channel && !hw_amplifier);
    if (mode == 0 || mode == 4 || mode == 6 || mode == 13) assert(writes == 0 && amp_enables == 0);
    if (mode == 1 || mode == 2 || mode == 3 || mode == 5 || mode == 8) assert(silent_writes == 8);
    if (mode == 3) assert(amp_enables == 0 && enables == 1);
    if (mode == 5) assert(enables == 2 && amp_enables == 2);
    if (mode == 9 || mode == 10 || mode == 12) {
        assert(pulses_completed == (mode == 12 ? 2u : 1u));
        assert(enables == 1 && amp_enables == 1);
    }
    if (mode == 11) assert(writes == 4);
    assert(platform_audio_shutdown() == (mode == 11 ? ESP_ERR_TIMEOUT : mode == 6 || mode == 7 ? ESP_FAIL : ESP_OK));
}
static void test_sd_sound(void) {
    char directory[] = "/tmp/chronos-wav-XXXXXX";
    assert(mkdtemp(directory));
    char path[256], next[65];
    const char* names[] = {"a.wav", "B.WAV", "ignore.txt"};
    for (unsigned i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        FILE* f = fopen(path, "wb"); assert(f); fputs("invalid", f); fclose(f);
    }
    assert(platform_audio_next_file(directory, "", next) == ESP_OK && !strcmp(next, "B.WAV"));
    assert(platform_audio_next_file(directory, "B.WAV", next) == ESP_OK && !strcmp(next, "a.wav"));
    assert(platform_audio_next_file(directory, "a.wav", next) == ESP_OK && !next[0]);
    lock = &mutex_token;
    FILE* sample = fopen("assets/sounds/TF048.WAV", "rb");
    if (sample) {
        fclose(sample);
        assert(platform_audio_load("assets/sounds", "TF048.WAV") == ESP_OK && clip.data);
        assert(clip.frames == (136861ULL * 44100 + 31999) / 32000);
    }
    assert(platform_audio_load(directory, "a.wav") == ESP_ERR_NOT_SUPPORTED && !clip.data);
    assert(platform_audio_load(directory, "missing.wav") == ESP_ERR_NOT_FOUND && !clip.data);
    assert(platform_audio_load(directory, "../a.wav") == ESP_ERR_INVALID_ARG && !clip.data);
    assert(platform_audio_load(NULL, "TF048.WAV") == ESP_ERR_INVALID_ARG && !clip.data);
    assert(platform_audio_load(NULL, "") == ESP_OK && !clip.data);
    lock = NULL;
    for (unsigned i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]); assert(unlink(path) == 0);
    }
    assert(rmdir(directory) == 0);
}
int main(void) {
    for (int mode = 0; mode <= 13; ++mode) run(mode);
    test_sd_sound();
    puts("audio lifecycle: all tests passed");
    return 0;
}
