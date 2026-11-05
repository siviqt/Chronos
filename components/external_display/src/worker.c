#include <stdatomic.h>
#include "driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
/* Latest-value mailbox bounds backlog even if a display is slow. No pointers
 * to application state or process-global timezone operations cross tasks. */
typedef struct {
    external_config_t config;
    external_frame_t  frame;
    uint32_t          generation;
} request_t;
static QueueHandle_t            queue;
static atomic_int               status = EXT_DISABLED;
static request_t                submitted;
static bool                     have_submission;
static uint32_t                 generation;
static atomic_uint              completed_generation;
/* Worker-owned lifecycle; failures leave driver NULL until a new generation. */
static const external_driver_t* driver;
static uint32_t                 applied;
static void                     apply_request(const request_t* request) {
    if (request->generation != applied) {
        if (driver) {
            driver->close();
            driver = NULL;
        }
        applied = request->generation;
        if (!request->config.enabled) {
            atomic_store(&status, EXT_DISABLED);
            return;
        }
        if (!external_config_valid(&request->config)) {
            atomic_store(&status, EXT_CONFIG_ERROR);
            return;
        }
        atomic_store(&status, EXT_STARTING);
        driver = request->config.driver == EXT_DRIVER_NOKIA ? &external_nokia_driver : &external_vfd_driver;
        esp_err_t e = driver->open(&request->config);
        if (e != ESP_OK) {
            driver->close();
            driver = NULL;
            atomic_store(&status, e == ESP_ERR_NOT_FOUND ? EXT_NOT_FOUND : EXT_INIT_ERROR);
            return;
        }
        atomic_store(&status, driver->ready);
    }
    if (driver && driver->present(&request->frame) != ESP_OK) {
        driver->close();
        driver = NULL;
        atomic_store(&status, EXT_IO_ERROR);
    }
}
static void worker(void* unused) {
    (void)unused;
    request_t request;
    for (;;)
        if (xQueueReceive(queue, &request, portMAX_DELAY) == pdTRUE) {
            apply_request(&request);
            atomic_store(&completed_generation, request.generation);
        }
}
external_status_t external_display_status(void) {
    return atomic_load(&status);
}
void external_display_submit(const external_config_t* c, const external_frame_t* f, bool retry) {
    if (!queue && !c->enabled) {
        submitted.config = *c;
        have_submission  = true;
        atomic_store(&status, EXT_DISABLED);
        return;
    }
    bool changed = !have_submission || !external_config_equal(c, &submitted.config);
    if (!queue) {
        /* An allocation failure is also latched until explicit retry/change. */
        if (have_submission && !changed && !retry) return;
        queue = xQueueCreate(1, sizeof(request_t));
        if (queue && xTaskCreate(worker, "external-display", 4096, NULL, 1, NULL) != pdPASS) {
            vQueueDelete(queue);
            queue = NULL;
        }
        if (!queue) {
            submitted.config = *c;
            have_submission  = true;
            atomic_store(&status, EXT_INIT_ERROR);
            return;
        }
    }
    external_status_t current = external_display_status();
    bool              failed  = current == EXT_NOT_FOUND || current >= EXT_CONFIG_ERROR;
    if (!changed && !retry && (failed || !memcmp(f, &submitted.frame, sizeof(*f)))) return;
    external_config_t hardware = *c;
    hardware.content           = submitted.config.content;
    hardware.world_city        = submitted.config.world_city;
    if (!have_submission || !external_config_equal(&hardware, &submitted.config) || retry || (changed && failed))
        ++generation;
    submitted       = (request_t){.config = *c, .frame = *f, .generation = generation};
    have_submission = true;
    xQueueOverwrite(queue, &submitted);
}
void external_display_stop(void) {
    if (!queue) return;
    external_config_t c = submitted.config;
    c.enabled           = false;
    external_frame_t f  = {0};
    external_display_submit(&c, &f, true);
    /* Bounded graceful drain; never force-delete a task holding a bus lock. */
    for (unsigned i = 0; i < 50 && atomic_load(&completed_generation) != generation; ++i) vTaskDelay(pdMS_TO_TICKS(10));
}
