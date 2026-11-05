#ifndef CHRONOS_SIGNALS_H
#define CHRONOS_SIGNALS_H
#include "chronos_alarms.h"
#include "timing_core.h"
typedef struct {
    uint32_t alarm_rgb, timing_rgb;
} chronos_led_frame_t;
/* Pure LED policy. Pulse should be true for 80 ms after a pacer beat. Colors
 * are full-range RGB; the hardware adapter applies a low brightness cap. */
chronos_led_frame_t chronos_signals_leds(chronos_alarm_led_t alarm, timing_state_t stopwatch, timing_state_t countdown,
                                         timing_state_t pacer, bool pulse, uint64_t now_us);
#endif
