#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "chronos_alarms.h"

/* Construct a valid enabled alarm for each scheduling scenario. */
static chronos_alarm_config_t alarm_at(chronos_alarm_repeat_t repeat, unsigned int hour, unsigned int minute) {
    chronos_alarm_config_t config;
    chronos_alarm_defaults(&config);
    config.enabled = true;
    config.repeat  = repeat;
    config.hour    = (uint8_t)hour;
    config.minute  = (uint8_t)minute;
    strcpy(config.label, "Wake up");
    return config;
}

/* Exercise full validation, including real leap days and Unicode label boundaries. */
static void test_validation(void) {
    chronos_alarm_config_t config = alarm_at(CHRONOS_ALARM_ONCE, 7, 30);
    assert(chronos_alarm_config_valid(&config));
    assert(chronos_date_valid((chronos_date_t){2000, 2, 29}));
    assert(!chronos_date_valid((chronos_date_t){2100, 2, 29}));
    assert(!chronos_date_valid((chronos_date_t){2026, 4, 31}));
    assert(!chronos_date_valid((chronos_date_t){1969, 12, 31}));
    config.hour = 24;
    assert(!chronos_alarm_config_valid(&config));
    config.hour   = 7;
    config.minute = 60;
    assert(!chronos_alarm_config_valid(&config));
    config.minute         = 30;
    for (unsigned minutes = 1; minutes <= 60; ++minutes) {
        config.snooze_minutes = minutes;
        assert(chronos_alarm_config_valid(&config));
    }
    config.snooze_minutes = 0;
    assert(!chronos_alarm_config_valid(&config));
    config.snooze_minutes = 61;
    assert(!chronos_alarm_config_valid(&config));
    config.snooze_minutes = 5;
    config.ring_seconds   = 4;
    assert(!chronos_alarm_config_valid(&config));
    config.ring_seconds = 61;
    assert(!chronos_alarm_config_valid(&config));
    config.ring_seconds = 15;
    config.repeat       = (chronos_alarm_repeat_t)3;
    assert(!chronos_alarm_config_valid(&config));
    config.repeat = CHRONOS_ALARM_ONCE;
    memset(config.label, 'a', 31);
    config.label[31] = 0;
    assert(!chronos_alarm_config_valid(&config));
    config.label[30] = 0;
    assert(chronos_alarm_config_valid(&config));
    for (size_t i = 0; i < 30; ++i) memcpy(config.label + i * 4, "\xf0\x9f\x94\x94", 4);
    config.label[120] = 0;
    assert(chronos_alarm_config_valid(&config));
    assert(!chronos_text_valid("\xc0\x80", 3, 30));
    assert(!chronos_text_valid("\xed\xa0\x80", 4, 30));
    assert(!chronos_text_valid("\xf4\x90\x80\x80", 5, 30));
    assert(!chronos_text_valid("\xe2\x82", 3, 30));
    assert(!chronos_text_valid("\n", 2, 30));
    char unterminated[] = {'a', 'b'};
    assert(!chronos_text_valid(unterminated, sizeof(unterminated), 30));
}

/* Verify simultaneous rings, input consumption, missed flags, and LED priorities. */
static void test_controls(void) {
    chronos_alarms_t alarms;
    chronos_alarms_init(&alarms);
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_OFF);
    chronos_alarm_config_t config = alarm_at(CHRONOS_ALARM_DAILY, 7, 30);
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) assert(chronos_alarms_set(&alarms, i, &config));
    assert(!chronos_alarms_set(&alarms, 5, &config));
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_ENABLED);
    assert(!chronos_alarms_key(&alarms, 'a', 0));
    chronos_local_time_t local = {{2026, 9, 9}, 7, 29, 59};
    assert(!chronos_alarms_tick(&alarms, &local, 0).ringing_mask);
    local.minute                  = 30;
    local.second                  = 0;
    chronos_alarm_events_t events = chronos_alarms_tick(&alarms, &local, 1000000);
    assert(events.started_mask == 31 && events.ringing_mask == 31);
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_RINGING);
    assert(!chronos_alarms_key(&alarms, '1', 1000000));
    assert(chronos_alarms_key(&alarms, 'Z', 1000000));
    assert(chronos_alarms_status(&alarms).snoozed_mask == 31);
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_SNOOZED);
    assert(!chronos_alarms_key(&alarms, ' ', 2000000));
    /* Civil time may vanish or jump backward while monotonic snooze continues. */
    assert(!chronos_alarms_tick(&alarms, NULL, 300999999).started_mask);
    events = chronos_alarms_tick(&alarms, NULL, 301000000);
    assert(events.started_mask == 31 && events.ringing_mask == 31);
    assert(chronos_alarms_tick(&alarms, NULL, 315999999).ringing_mask == 31);
    events = chronos_alarms_tick(&alarms, NULL, 316000000);
    assert(events.missed_mask == 31 && !events.ringing_mask);
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_MISSED);
    local.date.day = 10;
    events         = chronos_alarms_tick(&alarms, &local, 400000000);
    assert(events.started_mask == 31 && events.missed_mask == 31);
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_RINGING);
    assert(chronos_alarms_key(&alarms, 'a', 400000000));
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_MISSED);
    assert(chronos_alarms_key(&alarms, ' ', 400000000));
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_SNOOZED);
    chronos_alarms_stop(&alarms);
    assert(chronos_alarms_led(&alarms) == CHRONOS_ALARM_LED_OFF);
    local.date.day = 11;
    assert(!chronos_alarms_tick(&alarms, &local, 900000000).started_mask);
}

/* Verify skipped spring times trigger, repeated autumn times cannot ring twice. */
static void test_clock_transitions(void) {
    chronos_alarms_t alarms;
    chronos_alarms_init(&alarms);
    chronos_alarm_config_t config = alarm_at(CHRONOS_ALARM_DAILY, 3, 30);
    assert(chronos_alarms_set(&alarms, 0, &config));
    chronos_local_time_t local = {{2026, 3, 29}, 2, 59, 59};
    assert(!chronos_alarms_tick(&alarms, &local, 0).started_mask);
    local.hour   = 4;
    local.minute = 0;
    local.second = 0;
    assert(chronos_alarms_tick(&alarms, &local, 1000000).started_mask == 1);
    assert(chronos_alarms_key(&alarms, ' ', 1000000));
    local = (chronos_local_time_t){{2026, 10, 25}, 3, 29, 59};
    /* A long forward jump coalesces overdue days into the latest occurrence. */
    assert(chronos_alarms_tick(&alarms, &local, 2000000).started_mask == 1);
    assert(chronos_alarms_key(&alarms, ' ', 2000000));
    local.minute = 30;
    local.second = 0;
    assert(chronos_alarms_tick(&alarms, &local, 3000000).started_mask == 1);
    assert(chronos_alarms_key(&alarms, ' ', 3000000));
    local.minute = 0;
    assert(!chronos_alarms_tick(&alarms, &local, 4000000).started_mask);
    local.minute = 30;
    assert(!chronos_alarms_tick(&alarms, &local, 5000000).started_mask);
    local.date.day = 26;
    assert(chronos_alarms_tick(&alarms, &local, 6000000).started_mask == 1);
}

/* Verify one-shot consumption, weekday scheduling, startup policy, and invalid edit isolation. */
static void test_schedule(void) {
    chronos_alarms_t alarms;
    chronos_alarms_init(&alarms);
    chronos_alarm_config_t config = alarm_at(CHRONOS_ALARM_ONCE, 7, 30);
    assert(chronos_alarms_set(&alarms, 0, &config));
    chronos_local_time_t local = {{2026, 9, 9}, 7, 30, 45};
    assert(chronos_alarms_tick(&alarms, &local, 0).started_mask == 1);
    config.ring_seconds = 1;
    assert(!chronos_alarms_set(&alarms, 0, &config));
    assert(chronos_alarms_status(&alarms).ringing_mask == 1);
    assert(chronos_alarms_key(&alarms, ' ', 0));
    local.date.day = 10;
    assert(!chronos_alarms_tick(&alarms, &local, 1000000).started_mask);
    config.ring_seconds = 15;
    assert(chronos_alarms_set(&alarms, 0, &config));
    assert(chronos_alarms_tick(&alarms, &local, 2000000).started_mask == 1);
    config.repeat = CHRONOS_ALARM_WEEKDAYS;
    assert(chronos_alarms_set(&alarms, 0, &config));
    local = (chronos_local_time_t){{2026, 9, 11}, 7, 30, 0}; /* Friday */
    assert(chronos_alarms_tick(&alarms, &local, 3000000).started_mask == 1);
    assert(chronos_alarms_key(&alarms, ' ', 3000000));
    local.date.day = 12;
    assert(!chronos_alarms_tick(&alarms, &local, 4000000).started_mask);
    local.date.day = 13;
    assert(!chronos_alarms_tick(&alarms, &local, 5000000).started_mask);
    local.date.day = 14;
    assert(chronos_alarms_tick(&alarms, &local, 6000000).started_mask == 1);
    config.enabled = false;
    assert(chronos_alarms_set(&alarms, 0, &config));
    assert(!chronos_alarms_status(&alarms).ringing_mask);
}

/* Verify each snooze option and both ring-duration endpoints at exact deadlines. */
static void test_deadlines(void) {
    const uint8_t snoozes[] = {1, 5, 10, 15};
    const uint8_t rings[]   = {5, 15, 60};
    for (size_t i = 0; i < sizeof(snoozes); ++i) {
        for (size_t j = 0; j < sizeof(rings); ++j) {
            chronos_alarms_t alarms;
            chronos_alarms_init(&alarms);
            chronos_alarm_config_t config = alarm_at(CHRONOS_ALARM_ONCE, 0, 0);
            config.snooze_minutes         = snoozes[i];
            config.ring_seconds           = rings[j];
            assert(chronos_alarms_set(&alarms, 0, &config));
            chronos_local_time_t local = {{2026, 9, 9}, 0, 0, 0};
            assert(chronos_alarms_tick(&alarms, &local, 100).started_mask == 1);
            assert(chronos_alarms_key(&alarms, 'b', 100));
            uint64_t due    = 100 + (uint64_t)snoozes[i] * 60000000;
            local.date.year = 2000;
            assert(!chronos_alarms_tick(&alarms, &local, due - 1).started_mask);
            assert(chronos_alarms_tick(&alarms, &local, due).started_mask == 1);
            due += (uint64_t)rings[j] * 1000000;
            assert(!chronos_alarms_tick(&alarms, NULL, due - 1).missed_mask);
            assert(chronos_alarms_tick(&alarms, NULL, due).missed_mask == 1);
        }
    }
}

/* Run the alarm regression suite without a clock service, UI, or board. */
int main(void) {
    chronos_alarms_t selected;
    chronos_alarms_init(&selected);
    chronos_alarm_config_t config;
    chronos_alarm_defaults(&config);
    config.enabled = true; config.repeat = CHRONOS_ALARM_WEEKDAYS;
    config.weekdays = 1u << 5; config.hour = 9;
    assert(chronos_alarms_set(&selected, 0, &config));
    chronos_local_time_t friday = {{2026, 9, 11}, 9, 0, 0};
    assert(!chronos_alarms_tick(&selected, &friday, 0).started_mask);
    chronos_local_time_t saturday = {{2026, 9, 12}, 9, 0, 0};
    assert(chronos_alarms_tick(&selected, &saturday, 1000000).started_mask == 1);
    config.weekdays = 0;
    assert(!chronos_alarm_config_valid(&config));
    test_validation();
    test_controls();
    test_clock_transitions();
    test_schedule();
    test_deadlines();
    puts("alarms: all tests passed");
    return 0;
}
