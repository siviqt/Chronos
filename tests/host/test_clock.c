#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chronos_clock.h"

int main(void) {
    int64_t manual;
    assert(chronos_clock_parse_manual("2024-02-29 23:59:59", "UTC0", &manual));
    assert(manual == 1709251199);
    const char* invalid[] = {"2023-02-29 12:00:00", "2024-04-31 00:00:00", "2024-01-01 24:00:00", "2024-01-01 00:60:00", "2024-01-01 00:00:60", "2024-1-01 00:00:00", "1999-12-31 23:59:59", "2024-01-01 00:00:00x"};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
        assert(!chronos_clock_parse_manual(invalid[i], "UTC0", &manual));
    assert(!chronos_clock_parse_manual("2024-03-31 01:30:00", "GMT0BST,M3.5.0/1,M10.5.0/2", &manual));

    chronos_settings_t s;
    chronos_settings_defaults(&s);
    assert(s.city_count == 5 && chronos_settings_valid(&s));
    chronos_local_time_t t;
    bool dst;
    /* Europe spring jump and autumn repeated hour, 2024. */
    const char* london = "GMT0BST,M3.5.0/1,M10.5.0/2";
    assert(chronos_clock_local(1711846799, london, &t, &dst));
    assert(t.hour == 0 && t.minute == 59 && !dst);
    assert(chronos_clock_local(1711846800, london, &t, &dst));
    assert(t.hour == 2 && t.minute == 0 && dst);
    assert(chronos_clock_local(1729989000, london, &t, &dst));
    assert(t.hour == 1 && t.minute == 30 && dst);
    assert(chronos_clock_local(1729992600, london, &t, &dst));
    assert(t.hour == 1 && t.minute == 30 && !dst);
    /* Southern hemisphere DST and non-integral offset, Jan 1 2024 UTC. */
    assert(chronos_clock_local(1704067200, s.cities[3].timezone, &t, &dst));
    assert(t.hour == 11 && dst);
    assert(chronos_clock_local(1704067200, "IST-5:30", &t, &dst));
    assert(t.hour == 5 && t.minute == 30 && !dst);
    assert(!chronos_clock_local(0, "UTC0", &t, &dst));
    s.wifi_enabled = true;
    chronos_ui_t ui = {0};
    chronos_ui_view_t v = {0};
    chronos_clock_view(&v, &ui, &s, true, 1704067200, "Offline - RTC");
    assert(v.count == 6 && strstr(v.detail[1], "2023-12-31"));
    assert(strstr(v.heading, "00:00:00") && strstr(v.heading, "DST off"));
    assert(!strcmp(getenv("TZ"), s.timezone));
    assert(v.rows[5].command == CHRONOS_CLOCK_SYNC && v.rows[5].verb == UI_ACTIVATE);
    s.use_24_hour = false;
    chronos_clock_view(&v, &ui, &s, true, 1704067200, "NTP synchronized");
    assert(strstr(v.heading, "12:00:00 AM"));
    chronos_clock_view(&v, &ui, &s, true, 1704110400, "NTP synchronized");
    assert(strstr(v.heading, "12:00:00 PM"));
    s.ntp_enabled = false;
    chronos_clock_view(&v, &ui, &s, false, 0, "Offline - RTC");
    assert(strstr(v.detail[0], "unavailable") && v.rows[5].verb == UI_UNAVAILABLE);
    s.city_count = 0;
    chronos_clock_view(&v, &ui, &s, true, 1704067200, "Offline - RTC");
    assert(v.count == 1);
    /* Selected-city differences include dates, fractional zones and live DST. */
    const struct { const char *home, *city, *expected; int64_t epoch; } cases[] = {
        {"UTC0", "EST5", "-05:00 vs local", 1704067200},
        {"UTC0", "NPT-5:45", "+05:45 vs local", 1704067200},
        {"UTC0", "IST-5:30", "+05:30 vs local", 1704067200},
        {"UTC0", "UTC0", "Same as local", 1704067200},
        {"XXX-14", "YYY12", "-26:00 vs local", 1704067200},
        {"YYY12", "XXX-14", "+26:00 vs local", 1704067200},
        {"UTC0", "GMT0BST,M3.5.0/1,M10.5.0/2", "Same as local", 1711846799},
        {"UTC0", "GMT0BST,M3.5.0/1,M10.5.0/2", "+01:00 vs local", 1711846800},
    };
    s.city_count = 1; ui.content = true; ui.row = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        strcpy(s.timezone, cases[i].home);
        strcpy(s.cities[0].timezone, cases[i].city);
        for (unsigned hour24 = 0; hour24 < 2; ++hour24) {
            s.use_24_hour = hour24;
            chronos_clock_view(&v, &ui, &s, true, cases[i].epoch, "Offline");
            assert(!strcmp(v.world_difference, cases[i].expected));
            assert(!strcmp(getenv("TZ"), s.timezone));
        }
    }
    ui.row = 1; /* Sync row has no city difference. */
    chronos_clock_view(&v, &ui, &s, true, 1711846800, "Offline");
    assert(!v.world_difference[0]);
    ui.row = 0; ui.content = false;
    chronos_clock_view(&v, &ui, &s, true, 1711846800, "Offline");
    assert(!v.world_difference[0]);
    ui.content = true;
    chronos_clock_view(&v, &ui, &s, false, 0, "Offline");
    assert(!v.world_difference[0]);
    puts("clock: all tests passed");
}
