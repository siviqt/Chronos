#include <string.h>
#include "chronos_alarms.h"

/* Apply Gregorian century rules. */
static bool leap(unsigned int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

/* Reject invalid dates before civil-time arithmetic. */
bool chronos_date_valid(chronos_date_t date) {
    static const uint8_t lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (date.year < 1970 || date.year > 9999 || date.month < 1 || date.month > 12) return false;
    unsigned int limit = lengths[date.month - 1] + (date.month == 2 && leap(date.year));
    return date.day >= 1 && date.day <= limit;
}

/* Validate bounded UTF-8 and count scalar values without accepting overlong encodings. */
bool chronos_text_valid(const char* text, size_t capacity, size_t max_characters) {
    size_t i = 0, count = 0;
    while (i < capacity && text[i]) {
        uint32_t     cp = (unsigned char)text[i++];
        unsigned int extra;
        uint32_t     minimum;
        if (cp < 0x80) {
            extra   = 0;
            minimum = 0;
        } else if (cp >= 0xc2 && cp <= 0xdf) {
            cp      &= 0x1f;
            extra    = 1;
            minimum  = 0x80;
        } else if (cp >= 0xe0 && cp <= 0xef) {
            cp      &= 0x0f;
            extra    = 2;
            minimum  = 0x800;
        } else if (cp >= 0xf0 && cp <= 0xf4) {
            cp      &= 7;
            extra    = 3;
            minimum  = 0x10000;
        } else
            return false;
        while (extra--) {
            if (i >= capacity) return false;
            unsigned char next = (unsigned char)text[i++];
            if ((next & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (next & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || cp < 0x20 ||
            (cp >= 0x7f && cp <= 0x9f) || ++count > max_characters)
            return false;
    }
    return i < capacity;
}

/* Supply a disabled alarm with the required ring and snooze defaults. */
void chronos_alarm_defaults(chronos_alarm_config_t* config) {
    *config = (chronos_alarm_config_t){.weekdays = 0x1f, .snooze_minutes = 5, .ring_seconds = 15};
}

/* Enforce all alarm editor and persistence limits, including disabled slots. */
bool chronos_alarm_config_valid(const chronos_alarm_config_t* config) {
    return config->repeat >= CHRONOS_ALARM_ONCE && config->repeat <= CHRONOS_ALARM_WEEKDAYS &&
           config->weekdays <= 0x7f && (config->repeat != CHRONOS_ALARM_WEEKDAYS || config->weekdays != 0) &&
           config->hour < 24 && config->minute < 60 &&
           (config->snooze_minutes >= 1 && config->snooze_minutes <= 60) &&
           config->ring_seconds >= 5 && config->ring_seconds <= 60 &&
           chronos_text_valid(config->label, sizeof(config->label), 30);
}

/* Convert a validated Gregorian date into days since 1970 without libc timezone state. */
static int64_t day_number(chronos_date_t date) {
    static const unsigned int before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t                   y              = date.year - 1;
    return 365 * (date.year - 1970) + y / 4 - y / 100 + y / 400 - 477 + before_month[date.month - 1] +
           (date.month > 2 && leap(date.year)) + date.day - 1;
}

/* Reset occurrence tracking as well as any active notification. */
static void clear_runtime(chronos_alarm_runtime_t* runtime) {
    *runtime = (chronos_alarm_runtime_t){.last_occurrence = -1};
}

/* Initialize all five slots to safe disabled defaults. */
void chronos_alarms_init(chronos_alarms_t* alarms) {
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        chronos_alarm_defaults(&alarms->config[i]);
        clear_runtime(&alarms->runtime[i]);
    }
}

/* Commit a validated slot edit and discard that slot's previous live state. */
bool chronos_alarms_set(chronos_alarms_t* alarms, size_t slot, const chronos_alarm_config_t* config) {
    if (slot >= CHRONOS_ALARM_LIMIT || !chronos_alarm_config_valid(config)) return false;
    alarms->config[slot] = *config;
    clear_runtime(&alarms->runtime[slot]);
    return true;
}

/* Locate the latest scheduled civil occurrence at or before the current instant. */
static int64_t latest_occurrence(const chronos_alarm_config_t* config, int64_t current) {
    int64_t offset = config->hour * 3600 + config->minute * 60;
    int64_t day = current / 86400;
    if (current % 86400 < offset) --day;
    if (config->repeat == CHRONOS_ALARM_WEEKDAYS) {
        /* 1970-01-01 was Thursday; Monday is weekday zero. */
        while (day >= 0 && !(config->weekdays & (1u << ((day + 3) % 7)))) --day;
    }
    return day < 0 ? -1 : day * 86400 + offset;
}

/* Enter ringing without clearing an older unacknowledged missed notification. */
static void start_ring(chronos_alarm_runtime_t* runtime, uint64_t now_us) {
    runtime->phase            = CHRONOS_ALARM_RINGING;
    runtime->phase_started_us = now_us;
}

/* Collect independent notification masks for audio and banners. */
chronos_alarm_events_t chronos_alarms_status(const chronos_alarms_t* alarms) {
    chronos_alarm_events_t status = {0};
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if (alarms->runtime[i].phase == CHRONOS_ALARM_RINGING) status.ringing_mask |= bit;
        if (alarms->runtime[i].phase == CHRONOS_ALARM_SNOOZED) status.snoozed_mask |= bit;
        if (alarms->runtime[i].missed) status.missed_mask |= bit;
    }
    return status;
}

/* Advance civil scheduling and monotonic deadlines independently, even without RTC time. */
chronos_alarm_events_t chronos_alarms_tick(chronos_alarms_t* alarms, const chronos_local_time_t* local,
                                           uint64_t now_us) {
    bool civil_valid =
        local && chronos_date_valid(local->date) && local->hour < 24 && local->minute < 60 && local->second < 60;
    int64_t current =
        civil_valid ? day_number(local->date) * 86400 + local->hour * 3600 + local->minute * 60 + local->second : 0;
    uint8_t started = 0;
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        const chronos_alarm_config_t* config  = &alarms->config[i];
        chronos_alarm_runtime_t*      runtime = &alarms->runtime[i];
        if (!config->enabled && runtime->phase == CHRONOS_ALARM_QUIET) continue;
        uint64_t elapsed = now_us >= runtime->phase_started_us ? now_us - runtime->phase_started_us : 0;
        if (runtime->phase == CHRONOS_ALARM_RINGING && elapsed >= (uint64_t)config->ring_seconds * 1000000) {
            runtime->phase  = CHRONOS_ALARM_QUIET;
            runtime->missed = true;
        } else if (runtime->phase == CHRONOS_ALARM_SNOOZED && elapsed >= (uint64_t)config->snooze_minutes * 60000000) {
            start_ring(runtime, now_us);
            started |= (uint8_t)(1u << i);
        }
        if (!civil_valid || !config->enabled) continue;
        /* First observation checks this minute only; startup never replays old alarms. */
        int64_t previous   = runtime->has_previous ? runtime->previous_local : current - current % 60 - 1;
        int64_t occurrence = latest_occurrence(config, current);
        if (occurrence >= 0 && occurrence <= current && occurrence > previous &&
            occurrence > runtime->last_occurrence) {
            if (config->repeat == CHRONOS_ALARM_ONCE) alarms->config[i].enabled = false;
            runtime->last_occurrence = occurrence;
            start_ring(runtime, now_us);
            started |= (uint8_t)(1u << i);
        }
        runtime->previous_local = current;
        runtime->has_previous   = true;
    }
    chronos_alarm_events_t result = chronos_alarms_status(alarms);
    result.started_mask           = started;
    return result;
}

/* Consume alarm shortcuts before the caller routes ordinary text entry. */
bool chronos_alarms_key(chronos_alarms_t* alarms, unsigned int key, uint64_t now_us) {
    bool letter = (key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z');
    if (!letter && key != ' ') return false;
    bool consumed = false;
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        chronos_alarm_runtime_t* runtime = &alarms->runtime[i];
        if (runtime->phase == CHRONOS_ALARM_RINGING) {
            runtime->phase            = letter ? CHRONOS_ALARM_SNOOZED : CHRONOS_ALARM_QUIET;
            runtime->phase_started_us = now_us;
            consumed                  = true;
        }
        if (key == ' ' && runtime->missed) {
            runtime->missed = false;
            consumed        = true;
        }
    }
    return consumed;
}

/* Select approved LED A priority without hiding lower-priority missed banners. */
chronos_alarm_led_t chronos_alarms_led(const chronos_alarms_t* alarms) {
    chronos_alarm_events_t status = chronos_alarms_status(alarms);
    if (status.ringing_mask) return CHRONOS_ALARM_LED_RINGING;
    if (status.missed_mask) return CHRONOS_ALARM_LED_MISSED;
    if (status.snoozed_mask) return CHRONOS_ALARM_LED_SNOOZED;
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i)
        if (alarms->config[i].enabled) return CHRONOS_ALARM_LED_ENABLED;
    return CHRONOS_ALARM_LED_OFF;
}

/* Shut down scheduling and all transient state without changing persisted configuration. */
void chronos_alarms_stop(chronos_alarms_t* alarms) {
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        alarms->config[i].enabled = false;
        clear_runtime(&alarms->runtime[i]);
    }
}
