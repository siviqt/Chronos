#include "chronos_signals.h"

/* Apply the approved LED priorities independently of tab focus and backlights. */
chronos_led_frame_t chronos_signals_leds(chronos_alarm_led_t alarm, timing_state_t stopwatch, timing_state_t countdown,
                                         timing_state_t pacer, bool pulse, uint64_t now_us) {
    const uint32_t      cyan = 0x00ffff, blue = 0x0000ff, amber = 0xff8000, red = 0xff0000;
    bool                slow_on = now_us % 1000000 < 500000;
    bool                fast_on = now_us % 250000 < 125000;
    chronos_led_frame_t frame   = {0};
    switch (alarm) {
        case CHRONOS_ALARM_LED_ENABLED:
            frame.alarm_rgb = 0x00ff00;
            break;
        case CHRONOS_ALARM_LED_SNOOZED:
            frame.alarm_rgb = slow_on ? amber : 0;
            break;
        case CHRONOS_ALARM_LED_MISSED:
            frame.alarm_rgb = slow_on ? red : 0;
            break;
        case CHRONOS_ALARM_LED_RINGING:
            frame.alarm_rgb = fast_on ? red : 0;
            break;
        default:
            break;
    }
    if (countdown == TIMING_COMPLETE)
        frame.timing_rgb = fast_on ? blue : 0;
    else if (pacer == TIMING_RUNNING && pulse)
        frame.timing_rgb = 0xff00ff;
    else if (stopwatch == TIMING_RUNNING && countdown == TIMING_RUNNING)
        frame.timing_rgb = (now_us / 1000000) % 2 ? blue : cyan;
    else if (countdown == TIMING_RUNNING)
        frame.timing_rgb = blue;
    else if (stopwatch == TIMING_RUNNING)
        frame.timing_rgb = cyan;
    else if (pacer != TIMING_RUNNING &&
             (stopwatch == TIMING_PAUSED || countdown == TIMING_PAUSED || pacer == TIMING_PAUSED))
        frame.timing_rgb = amber;
    return frame;
}
