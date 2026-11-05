#include "chronos_runtime.h"

/* Copy configuration into an independent volatile session, leaving persistence untouched. */
bool chronos_runtime_init(chronos_runtime_t* runtime, const chronos_settings_t* settings) {
    if (!chronos_settings_valid(settings)) return false;
    *runtime = (chronos_runtime_t){.countdown_buzzer = settings->countdown_buzzer};
    chronos_alarms_init(&runtime->alarms);
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i)
        (void)chronos_alarms_set(&runtime->alarms, i, &settings->alarms[i]);
    return true;
}
/* Advance every session independently of GUI focus, then resolve sound and LED priorities. */
chronos_runtime_output_t chronos_runtime_tick(chronos_runtime_t* runtime, const chronos_local_time_t* local,
                                              uint64_t now_us) {
    chronos_runtime_output_t output = {.alarms = chronos_alarms_tick(&runtime->alarms, local, now_us)};
    if (output.alarms.started_mask) runtime->display_awake = true;
    (void)timing_countdown_remaining(&runtime->countdown, now_us);
    bool complete = runtime->countdown.session.state == TIMING_COMPLETE;
    if (complete && !runtime->completion_seen) {
        runtime->completion_us       = now_us;
        runtime->completion_sounding = true;
    }
    runtime->completion_seen = complete;
    if (!complete || (now_us >= runtime->completion_us && now_us - runtime->completion_us >= 15000000))
        runtime->completion_sounding = false;
    output.pacer_beat = timing_pacer_poll(&runtime->pacer, now_us) != 0;
    if (output.pacer_beat) {
        runtime->beat_seen = true;
        runtime->beat_us   = now_us;
    }
    bool pulse        = runtime->beat_seen && now_us >= runtime->beat_us && now_us - runtime->beat_us < 80000;
    output.leds       = chronos_signals_leds(chronos_alarms_led(&runtime->alarms), runtime->stopwatch.session.state,
                                             runtime->countdown.session.state, runtime->pacer.session.state, pulse, now_us);
    output.audio_loop = output.alarms.ringing_mask || (runtime->countdown_buzzer && runtime->completion_sounding);
    return output;
}
/* Give alarm shortcuts first refusal; Space also acknowledges countdown completion. */
bool chronos_runtime_key(chronos_runtime_t* runtime, unsigned int key, uint64_t now_us) {
    bool consumed = chronos_alarms_key(&runtime->alarms, key, now_us);
    if (key == ' ' && runtime->countdown.session.state == TIMING_COMPLETE) {
        timing_session_reset(&runtime->countdown.session);
        runtime->completion_sounding = runtime->completion_seen = false;
        consumed                                                = true;
    }
    return consumed;
}
/* Stop all application-owned timing and output requests before hardware teardown. */
void chronos_runtime_stop(chronos_runtime_t* runtime) {
    chronos_alarms_stop(&runtime->alarms);
    timing_stopwatch_reset(&runtime->stopwatch);
    timing_session_reset(&runtime->countdown.session);
    timing_session_reset(&runtime->pacer.session);
    runtime->completion_sounding = runtime->completion_seen = runtime->beat_seen = false;
}

/* Backlight state never pauses sessions or changes saved brightness. */
void chronos_runtime_toggle_backlights(chronos_runtime_t* runtime) {
    bool lit = !runtime->backlights_off || runtime->display_awake;
    runtime->backlights_off = lit;
    runtime->display_awake = false;
}
void chronos_runtime_brightness(const chronos_runtime_t* runtime, const chronos_settings_t* settings,
                                uint8_t* keyboard, uint8_t* display) {
    *keyboard = runtime->backlights_off ? 0 : settings->keyboard_percent;
    *display = runtime->backlights_off ? 0 : settings->display_percent;
    if (runtime->display_awake)
        *display = settings->display_percent ? settings->display_percent : 80;
}
