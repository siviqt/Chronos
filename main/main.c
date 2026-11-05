#include "chronos_app.h"

/** ESP-IDF entry point; delegate application lifecycle to the CHRONOS module. */
void app_main(void) {
    chronos_app_run();
}
