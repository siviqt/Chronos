#include <stdio.h>
#include "chronos_clock.h"
#include "chronos_external.h"
unsigned chronos_external_source(const external_config_t* c, const chronos_runtime_t* r) {
    if (c->content) return c->content;
    if (chronos_alarms_status(&r->alarms).ringing_mask) return EXT_ALARMS;
    /* Timed completion notification outranks running sessions, as in audio.
     * Concurrent running sessions use Timer > Stopwatch > Pacer. Paused
     * sessions are inactive in Automatic, but stay visible in fixed mode. */
    if (r->countdown.session.state == TIMING_RUNNING || r->completion_sounding) return EXT_COUNTDOWN;
    if (r->stopwatch.session.state == TIMING_RUNNING) return EXT_STOPWATCH;
    if (r->pacer.session.state == TIMING_RUNNING) return EXT_PACER;
    return EXT_CLOCK;
}
static const char* state(timing_state_t s) {
    const char* names[] = {"Ready", "Running", "Paused", "Complete", "Stopped"};
    return (unsigned)s < 5 ? names[s] : "Unavailable";
}
static void duration(char* out, size_t size, uint64_t us) {
    uint64_t tenths = us / 100000;
    if (tenths / 600 > 9999)
        snprintf(out, size, "9999m+");
    else
        snprintf(out, size, "%02llu:%02llu.%llu", (unsigned long long)(tenths / 600),
                 (unsigned long long)(tenths / 10 % 60), (unsigned long long)(tenths % 10));
}
static void civil(external_frame_t* f, const chronos_local_time_t* t, bool h24) {
    if (!t) {
        snprintf(f->value, sizeof(f->value), "No data");
        return;
    }
    snprintf(f->value, sizeof(f->value), "%02u:%02u:%02u%s", h24 ? t->hour : (t->hour % 12 ? t->hour % 12 : 12),
             t->minute, t->second,
             h24            ? ""
             : t->hour < 12 ? " AM"
                            : " PM");
    snprintf(f->detail, sizeof(f->detail), "%04u-%02u-%02u", t->date.year, t->date.month, t->date.day);
}
void chronos_external_frame(external_frame_t* f, const chronos_settings_t* s, const chronos_runtime_t* r,
                            const chronos_local_time_t* local, const chronos_local_time_t* world, uint64_t now) {
    *f = (external_frame_t){.source = chronos_external_source(&s->external, r)};
    switch (f->source) {
        case EXT_CLOCK:
            snprintf(f->title, sizeof(f->title), "Clock");
            civil(f, local, s->use_24_hour);
            break;
        case EXT_WORLD:
            snprintf(f->title, sizeof(f->title), "World Time");
            if (s->external.world_city >= s->city_count) {
                snprintf(f->value, sizeof(f->value), "%s", s->city_count ? "Unavailable" : "No cities");
                break;
            }
            snprintf(f->title, sizeof(f->title), "%.31s", s->cities[s->external.world_city].name);
            civil(f, world, s->use_24_hour);
            break;
        case EXT_ALARMS: {
            snprintf(f->title, sizeof(f->title), "Alarms");
            int selected = -1, rank = -1;
            for (unsigned i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
                int score = r->alarms.runtime[i].phase == CHRONOS_ALARM_RINGING   ? 3
                            : r->alarms.runtime[i].phase == CHRONOS_ALARM_SNOOZED ? 2
                            : s->alarms[i].enabled                                ? 1
                                                                                  : 0;
                if (score > rank) {
                    selected = (int)i;
                    rank     = score;
                }
            }
            if (rank == 0) {
                snprintf(f->value, sizeof(f->value), "All off");
                break;
            }
            const chronos_alarm_config_t* a = &s->alarms[selected];
            snprintf(f->value, sizeof(f->value), "%02u:%02u %s", a->hour, a->minute,
                     rank == 3   ? "RING"
                     : rank == 2 ? "SNOZ"
                                 : "ON");
            snprintf(f->detail, sizeof(f->detail), "%.47s", a->label);
            snprintf(f->extra, sizeof(f->extra), "Alarm %u", (unsigned)selected + 1);
            break;
        }
        case EXT_STOPWATCH:
            snprintf(f->title, sizeof(f->title), "Stopwatch");
            duration(f->value, sizeof(f->value), timing_session_elapsed(&r->stopwatch.session, now));
            snprintf(f->detail, sizeof(f->detail), "%s", state(r->stopwatch.session.state));
            snprintf(f->extra, sizeof(f->extra), "Laps: %u", (unsigned)r->stopwatch.lap_count);
            break;
        case EXT_COUNTDOWN: {
            snprintf(f->title, sizeof(f->title), "Countdown");
            timing_countdown_t timer     = r->countdown;  // Snapshot: projection never changes runtime.
            uint64_t           remaining = timer.session.state == TIMING_IDLE ? (uint64_t)s->countdown_seconds * 1000000
                                                                              : timing_countdown_remaining(&timer, now);
            duration(f->value, sizeof(f->value), remaining);
            snprintf(f->detail, sizeof(f->detail), "%s", state(timer.session.state));
            break;
        }
        case EXT_PACER: {
            snprintf(f->title, sizeof(f->title), "Pacer");
            uint64_t period =
                r->pacer.session.state == TIMING_IDLE ? (uint64_t)s->pacer_period_ms * 1000 : r->pacer.period_us;
            snprintf(f->value, sizeof(f->value), "%llu.%03llus", (unsigned long long)(period / 1000000),
                     (unsigned long long)(period / 1000 % 1000));
            snprintf(f->detail, sizeof(f->detail), "%s", state(r->pacer.session.state));
            snprintf(f->extra, sizeof(f->extra), "Beats: %llu", (unsigned long long)r->pacer.delivered_beats);
            break;
        }
        case EXT_ACTIVITY:
            snprintf(f->title, sizeof(f->title), "Activity");
            snprintf(f->value, sizeof(f->value), "S:%c T:%c P:%c", "IRPCS"[r->stopwatch.session.state],
                     "IRPCS"[r->countdown.session.state], "IRPCS"[r->pacer.session.state]);
            snprintf(f->detail, sizeof(f->detail), "Ringing: %s",
                     chronos_alarms_status(&r->alarms).ringing_mask ? "Yes" : "No");
            snprintf(f->extra, sizeof(f->extra), "R=run P=pause");
            break;
        default:
            snprintf(f->title, sizeof(f->title), "Source %u", f->source);
            snprintf(f->value, sizeof(f->value), "Unavailable");
            break;
    }
}
void chronos_external_update(const chronos_settings_t* s, const chronos_runtime_t* r, const chronos_local_time_t* local,
                             int64_t epoch, uint64_t now, bool retry) {
    static external_config_t previous;
    static bool              enabled;
    static uint64_t          next;
    bool                     changed = !external_config_equal(&previous, &s->external);
    if (!s->external.enabled) {
        if (enabled) {
            external_frame_t empty = {0};
            external_display_submit(&s->external, &empty, false);
        }
        enabled  = false;
        previous = s->external;
        return;
    }
    if (!changed && !retry && enabled && now < next) return;
    external_status_t status = external_display_status();
    if (!changed && !retry && enabled && (status == EXT_NOT_FOUND || status >= EXT_CONFIG_ERROR)) return;
    /* Keep software SPI below 5 Hz; VFD tenths have a 10 Hz ceiling. */
    next     = now + (s->external.driver == EXT_DRIVER_NOKIA ? 200000 : 100000);
    enabled  = true;
    previous = s->external;
    chronos_local_time_t world, restored;
    bool                 dst, world_valid = false;
    if (s->external.content == EXT_WORLD && local && s->external.world_city < s->city_count) {
        world_valid = chronos_clock_local(epoch, s->cities[s->external.world_city].timezone, &world, &dst);
        (void)chronos_clock_local(epoch, s->timezone, &restored, &dst);
    }
    external_frame_t f;
    chronos_external_frame(&f, s, r, local, world_valid ? &world : NULL, now);
    external_display_submit(&s->external, &f, retry);
}
