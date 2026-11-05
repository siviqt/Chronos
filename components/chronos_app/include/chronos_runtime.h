#ifndef CHRONOS_RUNTIME_H
#define CHRONOS_RUNTIME_H
#include "chronos_settings.h"
#include "chronos_signals.h"
typedef struct {
    chronos_alarms_t   alarms;
    timing_stopwatch_t stopwatch;
    timing_countdown_t countdown;
    timing_pacer_t     pacer;
    bool               countdown_buzzer, completion_seen, completion_sounding, beat_seen;
    uint64_t           completion_us, beat_us;
    bool               backlights_off, display_awake;
} chronos_runtime_t;
typedef struct {
    chronos_alarm_events_t alarms;
    chronos_led_frame_t    leds;
    bool                   audio_loop, pacer_beat;
} chronos_runtime_output_t;
/* Initialize volatile sessions from validated settings; invalid input leaves state unchanged. */
bool                     chronos_runtime_init(chronos_runtime_t* runtime, const chronos_settings_t* settings);
chronos_runtime_output_t chronos_runtime_tick(chronos_runtime_t* runtime, const chronos_local_time_t* local,
                                              uint64_t now_us);
bool                     chronos_runtime_key(chronos_runtime_t* runtime, unsigned int key, uint64_t now_us);
void chronos_runtime_toggle_backlights(chronos_runtime_t* runtime);
void chronos_runtime_brightness(const chronos_runtime_t* runtime, const chronos_settings_t* settings,
                                uint8_t* keyboard, uint8_t* display);
void                     chronos_runtime_stop(chronos_runtime_t* runtime);
#endif
