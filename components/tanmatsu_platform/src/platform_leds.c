#include "bsp/led.h"
#include "esp_log.h"
#include "tanmatsu_platform.h"

#define LED_A 4
#define LED_B 5
static bool     initialized, saved_automatic, frame_valid;
static uint8_t  saved_brightness, active_brightness;
static uint32_t previous_a, previous_b;

/* Use half-scale colors; a 10% global floor prevents integer dimming to black. */
static uint32_t dim(uint32_t rgb) {
    uint32_t result = 0;
    for (unsigned int shift = 0; shift <= 16; shift += 8)
        result |= ((((rgb >> shift) & 255) * 128 + 127) / 255) << shift;
    return result;
}
/* Preserve system LED automatic control and remember settings for clean shutdown. */
esp_err_t platform_leds_init(void) {
    if (initialized) return ESP_OK;
    uint32_t  count = 0;
    esp_err_t error = bsp_led_get_count(&count);
    if (error != ESP_OK) return error;
    if (count <= LED_B) return ESP_ERR_NOT_SUPPORTED;
    error = bsp_led_get_mode(&saved_automatic);
    if (error != ESP_OK) return error;
    error = bsp_led_get_brightness(&saved_brightness);
    if (error != ESP_OK) return error;
    initialized = true;
    frame_valid = false;
    error       = bsp_led_set_mode(true);
    if (error == ESP_OK && saved_brightness < 10) error = bsp_led_set_brightness(10);
    active_brightness = saved_brightness < 10 ? 10 : saved_brightness;
    if (error == ESP_OK) error = platform_leds_write(0, 0);
    if (error != ESP_OK) (void)platform_leds_shutdown();
    else ESP_LOGI("chronos_leds", "A/B ready: global brightness %u%% -> %u%%; RGB cap 128/255, automatic system LEDs",
                  saved_brightness, saved_brightness < 10 ? 10 : saved_brightness);
    return error;
}
/* Explicit user brightness includes zero; the initialization floor is not reapplied. */
esp_err_t platform_leds_brightness(uint8_t percent) {
    if (!initialized) return ESP_ERR_INVALID_STATE;
    if (percent > 100) return ESP_ERR_INVALID_ARG;
    if (percent == active_brightness) return ESP_OK;
    esp_err_t error = bsp_led_set_brightness(percent);
    if (error == ESP_OK) active_brightness = percent;
    return error;
}
/* Send only changed frames; failed sends remain dirty and are retried by the next tick. */
esp_err_t platform_leds_write(uint32_t alarm_rgb, uint32_t timing_rgb) {
    if (!initialized) return ESP_ERR_INVALID_STATE;
    uint32_t a = dim(alarm_rgb), b = dim(timing_rgb);
    if (frame_valid && a == previous_a && b == previous_b) return ESP_OK;
    frame_valid     = false;
    esp_err_t error = bsp_led_set_pixel(LED_A, a);
    if (error == ESP_OK) error = bsp_led_set_pixel(LED_B, b);
    if (error == ESP_OK) error = bsp_led_send();
    if (error == ESP_OK) {
        previous_a  = a;
        previous_b  = b;
        frame_valid = true;
    }
    return error;
}
/* Clear application LEDs and restore the pre-session global LED settings. */
esp_err_t platform_leds_shutdown(void) {
    if (!initialized) return ESP_OK;
    frame_valid      = false;
    esp_err_t result = platform_leds_write(0, 0);
    esp_err_t error  = bsp_led_set_brightness(saved_brightness);
    if (result == ESP_OK) result = error;
    error = bsp_led_set_mode(saved_automatic);
    if (result == ESP_OK) result = error;
    if (result == ESP_OK) initialized = false;
    return result;
}
