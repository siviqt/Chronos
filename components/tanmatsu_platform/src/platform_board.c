#include "bsp/device.h"
#include "bsp/input.h"
#include "bsp/rtc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "tanmatsu_platform.h"

static QueueHandle_t input_queue;

/* Initialize the template BSP without erasing shared NVS on an initialization error. */
esp_err_t platform_board_init(void) {
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) return error;
    const bsp_configuration_t config = {
        .display = {.requested_color_format = BSP_DISPLAY_COLOR_FORMAT_24_888RGB, .num_fbs = 1}};
    error = bsp_device_initialize(&config);
    if (error != ESP_OK) return error;
    return bsp_input_get_queue(&input_queue);
}
/* Use the BSP's application return path after application-owned resources stop. */
void platform_return_to_launcher(void) {
    bsp_device_restart_to_launcher();
}
/* Expose elapsed time independently from wall-clock adjustments. */
uint64_t platform_monotonic_us(void) {
    return (uint64_t)esp_timer_get_time();
}
/* Read RTC seconds without modifying RTC or its hardware alarm registers. */
esp_err_t platform_rtc_seconds(uint32_t* seconds) {
    return bsp_rtc_get_time(seconds);
}
/* Read card detection without changing storage state. */
esp_err_t platform_sd_present(bool* present) {
    return bsp_input_read_action(BSP_INPUT_ACTION_TYPE_SD_CARD, present);
}
/* The BSP emits scancode AND text events for printable keys, Backspace and Tab.
 * Text is authoritative (including Shift and key repeat); ignore raw scancodes.
 * Navigation events own arrows, Enter, Escape, function and volume keys. */
bool platform_input_poll(platform_input_t* event, uint32_t wait_ms) {
    *event = (platform_input_t){0};
    bsp_input_event_t input;
    if (!input_queue || xQueueReceive(input_queue, &input, pdMS_TO_TICKS(wait_ms)) != pdTRUE) return false;
    if (input.type == INPUT_EVENT_TYPE_KEYBOARD) {
        event->type      = input.args_keyboard.ascii == '\t' ? PLATFORM_INPUT_TAB : PLATFORM_INPUT_TEXT;
        event->character = (unsigned char)input.args_keyboard.ascii;
    } else if (input.type == INPUT_EVENT_TYPE_NAVIGATION && input.args_navigation.state) {
        switch (input.args_navigation.key) {
            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                event->type = PLATFORM_INPUT_LEFT;
                break;
            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                event->type = PLATFORM_INPUT_RIGHT;
                break;
            case BSP_INPUT_NAVIGATION_KEY_UP:
                event->type = PLATFORM_INPUT_UP;
                break;
            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                event->type = PLATFORM_INPUT_DOWN;
                break;
            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                event->type = PLATFORM_INPUT_ENTER;
                break;
            case BSP_INPUT_NAVIGATION_KEY_F1:
                event->type = PLATFORM_INPUT_F1;
                break;
            case BSP_INPUT_NAVIGATION_KEY_F2:
                event->type = PLATFORM_INPUT_F2;
                break;
            case BSP_INPUT_NAVIGATION_KEY_F3:
                event->type = PLATFORM_INPUT_F3;
                break;
            case BSP_INPUT_NAVIGATION_KEY_F4:
                event->type = PLATFORM_INPUT_F4;
                break;
            case BSP_INPUT_NAVIGATION_KEY_F5:
                event->type = PLATFORM_INPUT_F5;
                break;
            case BSP_INPUT_NAVIGATION_KEY_F6:
                event->type = PLATFORM_INPUT_F6;
                break;
            case BSP_INPUT_NAVIGATION_KEY_ESC:
                event->type = PLATFORM_INPUT_ESCAPE;
                break;
            case BSP_INPUT_NAVIGATION_KEY_VOLUME_UP:
                (void)platform_audio_volume_step(5);
                break;
            case BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN:
                (void)platform_audio_volume_step(-5);
                break;
            default:
                break;
        }
    } else if (input.type == INPUT_EVENT_TYPE_ACTION) {
        if (input.args_action.type == BSP_INPUT_ACTION_TYPE_SD_CARD)
            event->type = input.args_action.state ? PLATFORM_INPUT_SD_INSERTED : PLATFORM_INPUT_SD_REMOVED;
        else if (input.args_action.type == BSP_INPUT_ACTION_TYPE_AUDIO_JACK)
            platform_audio_headphones(input.args_action.state);
    }
    return true;
}
