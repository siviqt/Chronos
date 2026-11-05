#include <assert.h>
#include <stdio.h>
#include "../../components/external_display/src/worker.c"
static request_t mailbox;
static unsigned  creates, tasks, messages, opens, closes, presents;
static esp_err_t open_result, present_result;
static bool      allocation_failure;
QueueHandle_t    xQueueCreate(unsigned n, unsigned size) {
    ++creates;
    assert(n == 1 && size == sizeof(request_t));
    return allocation_failure ? NULL : &mailbox;
}
void vQueueDelete(QueueHandle_t q) {
    (void)q;
}
int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* arg, unsigned priority, void* h) {
    (void)fn;
    (void)name;
    (void)stack;
    (void)arg;
    (void)priority;
    (void)h;
    ++tasks;
    return pdPASS;
}
int xQueueOverwrite(QueueHandle_t q, const void* data) {
    assert(q == &mailbox);
    memcpy(&mailbox, data, sizeof(mailbox));
    ++messages;
    return pdTRUE;
}
int xQueueReceive(QueueHandle_t q, void* data, TickType_t wait) {
    (void)q;
    (void)data;
    (void)wait;
    return 0;
}
static void drain(void) {
    apply_request(&mailbox);
    atomic_store(&completed_generation, mailbox.generation);
}
void vTaskDelay(TickType_t delay) {
    (void)delay;
    drain();
}
static esp_err_t mock_open(const external_config_t* c) {
    assert(c->enabled);
    ++opens;
    return open_result;
}
static esp_err_t mock_present(const external_frame_t* f) {
    (void)f;
    ++presents;
    return present_result;
}
static void mock_close(void) {
    ++closes;
}
const external_driver_t external_nokia_driver = {mock_open, mock_present, mock_close, EXT_NOKIA_READY};
const external_driver_t external_vfd_driver   = {mock_open, mock_present, mock_close, EXT_VFD_READY};
int                     main(void) {
    external_config_t c = EXTERNAL_CONFIG_DEFAULT;
    external_frame_t  f = {.source = EXT_CLOCK, .value = "12:00:00"};
    for (unsigned i = 0; i < 100; ++i) external_display_submit(&c, &f, true);
    assert(!creates && !tasks && !messages && !opens);
    c.enabled          = true;
    allocation_failure = true;
    external_display_submit(&c, &f, false);
    assert(creates == 1 && external_display_status() == EXT_INIT_ERROR);
    external_display_submit(&c, &f, false);
    assert(creates == 1);
    c.enabled = false;
    external_display_submit(&c, &f, false);
    assert(external_display_status() == EXT_DISABLED);
    c.enabled          = true;
    allocation_failure = false;
    open_result        = ESP_ERR_NOT_FOUND;
    external_display_submit(&c, &f, false);
    drain();
    assert(tasks == 1 && opens == 1 && external_display_status() == EXT_NOT_FOUND);
    unsigned count = messages;
    strcpy(f.value, "12:00:01");
    for (unsigned i = 0; i < 100; ++i) external_display_submit(&c, &f, false);
    assert(messages == count && opens == 1);
    open_result = ESP_OK;
    external_display_submit(&c, &f, true);
    drain();
    assert(opens == 2 && presents == 1 && external_display_status() == EXT_VFD_READY);
    count = messages;
    external_display_submit(&c, &f, false);
    assert(messages == count);
    /* Content changes never reopen a healthy device. */
    c.content = EXT_STOPWATCH;
    f.source  = EXT_STOPWATCH;
    strcpy(f.value, "00:00.0");
    external_display_submit(&c, &f, false);
    drain();
    assert(opens == 2);
    /* Latest snapshot wins; no queue backlog. */
    strcpy(f.value, "00:01.0");
    external_display_submit(&c, &f, false);
    strcpy(f.value, "00:02.0");
    external_display_submit(&c, &f, false);
    assert(!strcmp(mailbox.frame.value, "00:02.0"));
    drain();
    present_result = ESP_FAIL;
    strcpy(f.value, "00:03.0");
    external_display_submit(&c, &f, false);
    drain();
    assert(external_display_status() == EXT_IO_ERROR);
    count = presents;
    strcpy(f.value, "00:04.0");
    external_display_submit(&c, &f, false);
    assert(presents == count);
    c.driver = EXT_DRIVER_NOKIA;
    c.clk    = c.din;
    external_display_submit(&c, &f, false);
    drain();
    assert(external_display_status() == EXT_CONFIG_ERROR && opens == 2);
    c.clk          = 2;
    present_result = ESP_OK;
    external_display_submit(&c, &f, false);
    drain();
    assert(external_display_status() == EXT_NOKIA_READY && opens == 3);
    external_display_stop();
    assert(external_display_status() == EXT_DISABLED && closes == 3);
    count = messages;
    external_display_submit(&submitted.config, &submitted.frame, false);
    assert(messages == count);
    puts(
        "external worker: disabled, allocation failure, bounded retry, coalescing, reconfiguration, I/O failure and "
                            "shutdown passed");
}
