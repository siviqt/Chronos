#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "chronos_clock.h"

bool chronos_clock_local(int64_t seconds, const char* zone, chronos_local_time_t* local, bool* dst) {
    if (!zone || !local || !dst || setenv("TZ", zone, 1)) return false;
    tzset();
    time_t epoch = (time_t)seconds;
    struct tm civil;
    if ((int64_t)epoch != seconds || !localtime_r(&epoch, &civil)) return false;
    if (civil.tm_year < 100 || civil.tm_year > 199) return false;
    *local = (chronos_local_time_t){.date = {civil.tm_year + 1900, civil.tm_mon + 1, civil.tm_mday},
        .hour = civil.tm_hour, .minute = civil.tm_min, .second = civil.tm_sec};
    *dst = civil.tm_isdst > 0;
    return chronos_date_valid(local->date);
}
/* Compare full civil dates, so midnight/year rollover is not mistaken for
 * a short offset in the opposite direction. No process-global TZ changes. */
static int64_t civil_minutes(const chronos_local_time_t* t) {
    static const unsigned before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t y = t->date.year - 1;
    bool leap = t->date.year % 4 == 0 && (t->date.year % 100 != 0 || t->date.year % 400 == 0);
    int64_t days = 365 * y + y / 4 - y / 100 + y / 400 + before_month[t->date.month - 1] +
                   (t->date.month > 2 && leap) + t->date.day - 1;
    return days * 1440 + t->hour * 60 + t->minute;
}
static void format(char* out, size_t size, const char* name, const chronos_local_time_t* t, bool dst, bool hour24) {
    snprintf(out, size, "%s\t%02u:%02u:%02u%s\t%04u-%02u-%02u\tDST %s", name,
             hour24 ? t->hour : (t->hour % 12 ? t->hour % 12 : 12), t->minute, t->second,
             hour24 ? "" : (t->hour < 12 ? " AM" : " PM"), t->date.year, t->date.month, t->date.day,
             dst ? "on" : "off");
}
void chronos_clock_view(chronos_ui_view_t* v, const chronos_ui_t* ui,
                        const chronos_settings_t* s, bool valid, int64_t seconds, const char* status) {
    if (ui->editing || (ui->active != 0 && ui->active != 5)) return;
    chronos_local_time_t local, home;
    bool dst;
    v->world_difference[0] = 0;
    bool home_valid = valid && chronos_clock_local(seconds, s->timezone, &home, &dst);
    if (home_valid)
        format(v->heading, sizeof(v->heading), "Local", &home, dst, s->use_24_hour);
    if (ui->active == 0) {
        v->count = 0;
        for (size_t i = 0; i < s->city_count; ++i) {
            if (valid && chronos_clock_local(seconds, s->cities[i].timezone, &local, &dst)) {
                format(v->detail[i], sizeof(v->detail[i]), s->cities[i].name, &local, dst, s->use_24_hour);
                if (home_valid && ui->content && ui->row == i) {
                    int difference = (int)(civil_minutes(&local) - civil_minutes(&home));
                    unsigned magnitude = (unsigned)abs(difference);
                    if (!difference) snprintf(v->world_difference, sizeof(v->world_difference), "Same as local");
                    else snprintf(v->world_difference, sizeof(v->world_difference), "%c%02u:%02u vs local",
                                  difference < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
                }
            }
            else snprintf(v->detail[i], sizeof(v->detail[i]), "%s - time unavailable", s->cities[i].name);
            v->rows[i] = (chronos_ui_action_t){UI_NONE, v->detail[i], 0};
            ++v->count;
        }
        size_t row = v->count++;
        snprintf(v->detail[row], sizeof(v->detail[row]), "Sync now - %s", status);
        v->rows[row] = (chronos_ui_action_t){s->ntp_enabled && s->wifi_enabled && s->time_source == CHRONOS_TIME_NTP ? UI_ACTIVATE : UI_UNAVAILABLE, "Sync now", CHRONOS_CLOCK_SYNC};
    }
    /* Every world-city conversion must leave alarms in the local timezone. */
    if (setenv("TZ", s->timezone, 1) == 0) tzset();
}

bool chronos_clock_parse_manual(const char* text, const char* zone, int64_t* seconds) {
    if (!text || !zone || !seconds || strlen(text) != 19) return false;
    for (unsigned i = 0; i < 19; ++i) {
        char separator = i == 4 || i == 7 ? '-' : i == 10 ? ' ' : i == 13 || i == 16 ? ':' : 0;
        if (separator ? text[i] != separator : text[i] < '0' || text[i] > '9') return false;
    }
    unsigned y, m, d, h, min, sec;
    if (sscanf(text, "%4u-%2u-%2u %2u:%2u:%2u", &y, &m, &d, &h, &min, &sec) != 6 ||
        y < 2000 || y > 2099 || !chronos_date_valid((chronos_date_t){y,m,d}) || h > 23 || min > 59 || sec > 59) return false;
    if (setenv("TZ", zone, 1)) return false;
    tzset();
    struct tm civil = {.tm_year = y - 1900, .tm_mon = m - 1, .tm_mday = d,
                       .tm_hour = h, .tm_min = min, .tm_sec = sec, .tm_isdst = -1};
    time_t epoch = mktime(&civil);
    struct tm check;
    if (epoch == (time_t)-1 || !localtime_r(&epoch, &check) || check.tm_year != (int)y - 1900 ||
        check.tm_mon != (int)m - 1 || check.tm_mday != (int)d || check.tm_hour != (int)h ||
        check.tm_min != (int)min || check.tm_sec != (int)sec) return false;
    *seconds = epoch;
    return true;
}
