#include <assert.h>
#include <stdio.h>
#include "chronos_runtime.h"

/* Verify exact blink boundaries, simultaneous timing, and priority overlays. */
static void test_leds(void) {
    chronos_led_frame_t frame =
        chronos_signals_leds(CHRONOS_ALARM_LED_RINGING, TIMING_RUNNING, TIMING_RUNNING, TIMING_RUNNING, false, 0);
    assert(frame.alarm_rgb == 0xff0000 && frame.timing_rgb == 0x00ffff);
    frame = chronos_signals_leds(CHRONOS_ALARM_LED_RINGING, TIMING_RUNNING, TIMING_RUNNING, TIMING_IDLE, false, 125000);
    assert(frame.alarm_rgb == 0);
    frame = chronos_signals_leds(CHRONOS_ALARM_LED_MISSED, TIMING_RUNNING, TIMING_RUNNING, TIMING_IDLE, false, 1000000);
    assert(frame.alarm_rgb == 0xff0000 && frame.timing_rgb == 0x0000ff);
    frame =
        chronos_signals_leds(CHRONOS_ALARM_LED_SNOOZED, TIMING_RUNNING, TIMING_RUNNING, TIMING_RUNNING, true, 1000000);
    assert(frame.alarm_rgb == 0xff8000 && frame.timing_rgb == 0xff00ff);
    frame =
        chronos_signals_leds(CHRONOS_ALARM_LED_SNOOZED, TIMING_RUNNING, TIMING_COMPLETE, TIMING_RUNNING, true, 500000);
    assert(frame.alarm_rgb == 0 && frame.timing_rgb == 0x0000ff);
    frame = chronos_signals_leds(CHRONOS_ALARM_LED_OFF, TIMING_PAUSED, TIMING_IDLE, TIMING_PAUSED, false, 0);
    assert(frame.alarm_rgb == 0 && frame.timing_rgb == 0xff8000);
    frame = chronos_signals_leds(CHRONOS_ALARM_LED_ENABLED, TIMING_PAUSED, TIMING_IDLE, TIMING_RUNNING, false, 0);
    assert(frame.alarm_rgb == 0x00ff00 && frame.timing_rgb == 0);
    frame = chronos_signals_leds(CHRONOS_ALARM_LED_OFF, TIMING_IDLE, TIMING_RUNNING, TIMING_IDLE, false, 0);
    assert(frame.timing_rgb == 0x0000ff);
}
/* Verify real alarm/timer sessions produce correct audio requests and latched indications. */
static void test_runtime(void) {
    chronos_settings_t settings;
    chronos_settings_defaults(&settings);
    settings.alarms[0].enabled = true;
    settings.alarms[0].repeat  = CHRONOS_ALARM_DAILY;
    settings.alarms[0].hour    = 7;
    chronos_runtime_t runtime;
    assert(chronos_runtime_init(&runtime, &settings));
    timing_stopwatch_start(&runtime.stopwatch, 0);
    assert(timing_pacer_start(&runtime.pacer, 1000000, 0));
    assert(timing_countdown_start(&runtime.countdown, 2000000, 0));
    chronos_local_time_t     local  = {{2026, 9, 10}, 6, 59, 59};
    chronos_runtime_output_t output = chronos_runtime_tick(&runtime, &local, 0);
    assert(!output.audio_loop && !output.pacer_beat);
    uint8_t keyboard, display;
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(keyboard == settings.keyboard_percent && display == settings.display_percent);
    chronos_runtime_toggle_backlights(&runtime);
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(keyboard == 0 && display == 0);
    local.hour   = 7;
    local.minute = local.second = 0;
    output                      = chronos_runtime_tick(&runtime, &local, 1000000);
    assert(output.audio_loop && output.pacer_beat && output.alarms.started_mask == 1);
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(keyboard == 0 && display == settings.display_percent);
    assert(runtime.stopwatch.session.state == TIMING_RUNNING);
    assert(runtime.countdown.session.state == TIMING_RUNNING);
    assert(runtime.pacer.session.state == TIMING_RUNNING);
    settings.display_percent = 0;
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(display == 80 && settings.display_percent == 0);
    chronos_runtime_toggle_backlights(&runtime);
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(display == 0 && keyboard == 0);
    settings.display_percent = 35;
    settings.keyboard_percent = 15;
    chronos_runtime_toggle_backlights(&runtime);
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(display == 35 && keyboard == 15);
    chronos_runtime_toggle_backlights(&runtime);
    assert(chronos_runtime_key(&runtime, 'a', 1000000));
    output = chronos_runtime_tick(&runtime, &local, 1000001);
    assert(!output.audio_loop && output.alarms.snoozed_mask == 1);
    output = chronos_runtime_tick(&runtime, &local, 2000000);
    assert(output.audio_loop && output.leds.timing_rgb == 0x0000ff);
    output = chronos_runtime_tick(&runtime, &local, 16999999);
    assert(output.audio_loop);
    output = chronos_runtime_tick(&runtime, &local, 17000000);
    assert(!output.audio_loop && runtime.countdown.session.state == TIMING_COMPLETE);
    assert(chronos_runtime_key(&runtime, ' ', 17000000));
    assert(runtime.countdown.session.state == TIMING_IDLE);
    runtime.countdown_buzzer = false;
    assert(timing_countdown_start(&runtime.countdown, 1, 17000000));
    output = chronos_runtime_tick(&runtime, &local, 17000001);
    assert(!output.audio_loop && runtime.countdown.session.state == TIMING_COMPLETE);
    output = chronos_runtime_tick(&runtime, NULL, 301000000);
    assert(output.audio_loop && output.alarms.started_mask == 1);
    chronos_runtime_brightness(&runtime, &settings, &keyboard, &display);
    assert(display == 35 && keyboard == 0); /* Snooze expiry wakes again. */
    chronos_runtime_stop(&runtime);
    output = chronos_runtime_tick(&runtime, &local, 400000000);
    assert(!output.audio_loop && !output.pacer_beat && !output.leds.alarm_rgb && !output.leds.timing_rgb);
    assert(settings.alarms[0].enabled); /* Stop never mutates persistent configuration. */
}
/* Run pure output-policy and coordination regressions. */
int main(void) {
    test_leds();
    test_runtime();
    puts("signals/runtime: all tests passed");
    return 0;
}
