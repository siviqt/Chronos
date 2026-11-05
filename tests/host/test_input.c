#include <assert.h>
#include <stdio.h>
#include "bsp/device.h"
#include "bsp/input.h"
#include "tanmatsu_platform.h"

static bsp_input_event_t queued;
static bool available;
static esp_err_t board_error, queue_error;
static unsigned queue_requests;
int xQueueReceive(QueueHandle_t queue, void* event, uint32_t ticks) {
    (void)queue; (void)ticks;
    if (!available) return 0;
    *(bsp_input_event_t*)event = queued;
    available = false;
    return 1;
}
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t bsp_device_initialize(const bsp_configuration_t* config) { (void)config; return board_error; }
void bsp_device_restart_to_launcher(void) {}
esp_err_t bsp_input_get_queue(QueueHandle_t* queue) { ++queue_requests; *queue = &queued; return queue_error; }
esp_err_t bsp_input_read_action(bsp_input_action_type_t action, bool* state) {
    (void)action; *state = false; return ESP_OK;
}
esp_err_t bsp_rtc_get_time(uint32_t* seconds) { *seconds = 0; return ESP_OK; }
int64_t esp_timer_get_time(void) { return 0; }
esp_err_t platform_audio_volume_step(int step) { (void)step; return ESP_OK; }
void platform_audio_headphones(bool inserted) { (void)inserted; }
static platform_input_t dispatch(bsp_input_event_t input) {
    queued = input; available = true;
    platform_input_t result;
    assert(platform_input_poll(&result, 0));
    return result;
}
/* Replay the BSP's overlapping raw/navigation/text streams for one physical press. */
static void press(unsigned scan, char text, platform_input_type_t expected) {
    platform_input_t result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_SCANCODE,
        .args_scancode.scancode = scan});
    assert(result.type == PLATFORM_INPUT_NONE);
    if (text == '\t' || text == '\b') {
        result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_NAVIGATION,
            .args_navigation = {.key = text == '\t' ? BSP_INPUT_NAVIGATION_KEY_TAB : BSP_INPUT_NAVIGATION_KEY_BACKSPACE,
                                .state = true}});
        assert(result.type == PLATFORM_INPUT_NONE);
    }
    result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_KEYBOARD, .args_keyboard.ascii = text});
    assert(result.type == expected && result.character == (unsigned char)text);
    result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_SCANCODE,
        .args_scancode.scancode = scan | BSP_INPUT_SCANCODE_RELEASE_MODIFIER});
    assert(result.type == PLATFORM_INPUT_NONE);
}
int main(void) {
    /* BSP now continues initialization after a subsystem error. Do not consume
     * an input queue when the aggregate board initialization failed. */
    board_error = ESP_FAIL;
    assert(platform_board_init() == ESP_FAIL && queue_requests == 0);
    board_error = ESP_OK;
    queue_error = ESP_ERR_INVALID_STATE;
    assert(platform_board_init() == ESP_ERR_INVALID_STATE && queue_requests == 1);
    queue_error = ESP_OK;
    assert(platform_board_init() == ESP_OK);
    press(BSP_INPUT_SCANCODE_2, '2', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_3, '3', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_4, '4', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_1, '1', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_A, 'a', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_A, 'a', PLATFORM_INPUT_TEXT); /* Deliberate repeated letter. */
    press(BSP_INPUT_SCANCODE_A, 'A', PLATFORM_INPUT_TEXT); /* Shift uses BSP text. */
    press(BSP_INPUT_SCANCODE_SPACE, ' ', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_BACKSPACE, '\b', PLATFORM_INPUT_TEXT);
    press(BSP_INPUT_SCANCODE_TAB, '\t', PLATFORM_INPUT_TAB);
    for (unsigned i = 0; i < 2; ++i) { /* BSP-generated held-key repeats are retained. */
        platform_input_t result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_KEYBOARD,
            .args_keyboard.ascii = 'a'});
        assert(result.type == PLATFORM_INPUT_TEXT && result.character == 'a');
    }
    platform_input_t result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation = {.key = BSP_INPUT_NAVIGATION_KEY_F1, .state = true}});
    assert(result.type == PLATFORM_INPUT_F1);
    result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation = {.key = BSP_INPUT_NAVIGATION_KEY_F1, .state = false}});
    assert(result.type == PLATFORM_INPUT_NONE);
    result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation = {.key = BSP_INPUT_NAVIGATION_KEY_F6, .state = true}});
    assert(result.type == PLATFORM_INPUT_F6);
    result = dispatch((bsp_input_event_t){.type = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation = {.key = BSP_INPUT_NAVIGATION_KEY_F6, .state = false}});
    assert(result.type == PLATFORM_INPUT_NONE);
    puts("input: all tests passed");
}
