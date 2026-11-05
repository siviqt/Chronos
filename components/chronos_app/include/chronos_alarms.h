#ifndef CHRONOS_ALARMS_H
#define CHRONOS_ALARMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHRONOS_ALARM_LIMIT       5
/* Thirty Unicode scalar values, each requiring at most four UTF-8 bytes. */
#define CHRONOS_ALARM_LABEL_BYTES 121

typedef struct {
    uint16_t year;
    uint8_t  month, day;
} chronos_date_t;
typedef struct {
    chronos_date_t date;
    uint8_t        hour, minute, second;
} chronos_local_time_t;
typedef enum {
    CHRONOS_ALARM_ONCE,
    CHRONOS_ALARM_DAILY,
    CHRONOS_ALARM_WEEKDAYS
} chronos_alarm_repeat_t;
typedef struct {
    bool                   enabled;
    chronos_alarm_repeat_t repeat;
    uint8_t                hour, minute;
    uint8_t                weekdays; /* Bits 0..6: Monday..Sunday. */
    uint8_t                snooze_minutes, ring_seconds;
    char                   label[CHRONOS_ALARM_LABEL_BYTES];
} chronos_alarm_config_t;
typedef enum {
    CHRONOS_ALARM_QUIET,
    CHRONOS_ALARM_RINGING,
    CHRONOS_ALARM_SNOOZED
} chronos_alarm_phase_t;
typedef struct {
    chronos_alarm_phase_t phase;
    bool                  missed, has_previous;
    int64_t               previous_local, last_occurrence;
    uint64_t              phase_started_us;
} chronos_alarm_runtime_t;
typedef struct {
    chronos_alarm_config_t  config[CHRONOS_ALARM_LIMIT];
    chronos_alarm_runtime_t runtime[CHRONOS_ALARM_LIMIT];
} chronos_alarms_t;
typedef struct {
    uint8_t started_mask; /* Illuminate the display whenever nonzero. */
    uint8_t ringing_mask, snoozed_mask, missed_mask;
} chronos_alarm_events_t;
typedef enum {
    CHRONOS_ALARM_LED_OFF,
    CHRONOS_ALARM_LED_ENABLED,
    CHRONOS_ALARM_LED_SNOOZED,
    CHRONOS_ALARM_LED_MISSED,
    CHRONOS_ALARM_LED_RINGING
} chronos_alarm_led_t;

/* Pure validation: dates are Gregorian, 1970 through 9999; labels are UTF-8. */
bool                   chronos_date_valid(chronos_date_t date);
bool                   chronos_text_valid(const char* text, size_t capacity, size_t max_characters);
void                   chronos_alarm_defaults(chronos_alarm_config_t* config);
bool                   chronos_alarm_config_valid(const chronos_alarm_config_t* config);
void                   chronos_alarms_init(chronos_alarms_t* alarms);
/* Replacing a slot cancels its activity and notification; invalid edits do nothing. */
bool                   chronos_alarms_set(chronos_alarms_t* alarms, size_t slot, const chronos_alarm_config_t* config);
/* Supply converted local time, or NULL when civil time is unavailable. Monotonic
 * microseconds control ringing and snooze independently of civil corrections.
 * Poll before dispatching input. See ARCHITECTURE.md for clock-jump policy. */
chronos_alarm_events_t chronos_alarms_tick(chronos_alarms_t* alarms, const chronos_local_time_t* local,
                                           uint64_t now_us);
/* Returns true when consumed. Letter keys snooze all ringing slots; Space
 * dismisses all ringing slots and clears missed flags, leaving snoozes intact. */
bool                   chronos_alarms_key(chronos_alarms_t* alarms, unsigned int key, uint64_t now_us);
chronos_alarm_events_t chronos_alarms_status(const chronos_alarms_t* alarms);
chronos_alarm_led_t    chronos_alarms_led(const chronos_alarms_t* alarms);
/* Clear all live activity and disable scheduling on application exit. */
void                   chronos_alarms_stop(chronos_alarms_t* alarms);

#endif
