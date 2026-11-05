#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "chronos_external.h"
#include "chronos_screen.h"
static unsigned         submissions;
static external_frame_t received;
void                    external_display_submit(const external_config_t* c, const external_frame_t* f, bool retry) {
    (void)c;
    (void)retry;
    ++submissions;
    received = *f;
}
external_status_t external_display_status(void) {
    return EXT_DISABLED;
}
void external_display_stop(void) {
}
int main(void) {
    chronos_settings_t s, decoded;
    chronos_settings_defaults(&s);
    chronos_runtime_t r;
    assert(chronos_runtime_init(&r, &s));
    chronos_local_time_t local = {.date = {2026, 9, 17}, .hour = 12, .minute = 34, .second = 56};
    external_frame_t     f;
    for (unsigned i = 0; i < 100; ++i) chronos_external_update(&s, &r, &local, 0, i * 100000, false);
    assert(!submissions);
    assert(chronos_external_source(&s.external, &r) == EXT_CLOCK);
    timing_pacer_start(&r.pacer, 1000000, 0);
    assert(chronos_external_source(&s.external, &r) == EXT_PACER);
    timing_stopwatch_start(&r.stopwatch, 0);
    assert(chronos_external_source(&s.external, &r) == EXT_STOPWATCH);
    timing_countdown_start(&r.countdown, 10000000, 0);
    assert(chronos_external_source(&s.external, &r) == EXT_COUNTDOWN);
    r.alarms.runtime[0].phase = CHRONOS_ALARM_RINGING;
    assert(chronos_external_source(&s.external, &r) == EXT_ALARMS);
    r.alarms.runtime[0].phase = CHRONOS_ALARM_SNOOZED;
    assert(chronos_external_source(&s.external, &r) == EXT_COUNTDOWN);
    timing_session_pause(&r.countdown.session, 1000000);
    assert(chronos_external_source(&s.external, &r) == EXT_STOPWATCH);
    r.completion_sounding = true;
    assert(chronos_external_source(&s.external, &r) == EXT_COUNTDOWN);
    r.completion_sounding = false;
    timing_session_pause(&r.stopwatch.session, 1000000);
    assert(chronos_external_source(&s.external, &r) == EXT_PACER);
    timing_session_reset(&r.pacer.session);
    assert(chronos_external_source(&s.external, &r) == EXT_CLOCK);
    chronos_runtime_init(&r, &s);
    for (unsigned source = EXT_CLOCK; source <= EXT_WORLD; ++source) {
        s.external.content = source;
        chronos_external_frame(&f, &s, &r, &local, &local, 0);
        assert(f.source == source && f.value[0]);
        if (source == EXT_STOPWATCH) assert(!strcmp(f.value, "00:00.0"));
        if (source == EXT_COUNTDOWN) assert(!strcmp(f.value, "05:00.0"));
        if (source == EXT_PACER) assert(!strcmp(f.value, "1.000s"));
        /* Even a ringing alarm does not rewrite fixed content. */
        r.alarms.runtime[0].phase = CHRONOS_ALARM_RINGING;
        assert(chronos_external_source(&s.external, &r) == source);
        r.alarms.runtime[0].phase = CHRONOS_ALARM_QUIET;
    }
    s.external.content = 222;
    chronos_external_frame(&f, &s, &r, &local, NULL, 0);
    assert(f.source == 222 && !strcmp(f.value, "Unavailable"));
    s.external.enabled  = true;
    s.external.driver   = EXT_DRIVER_NOKIA;
    s.external.contrast = 78;
    uint8_t bytes[16384];
    size_t  size;
    assert(chronos_settings_text_schema.encode(&s, bytes, sizeof(bytes), &size));
    assert(chronos_settings_text_schema.decode(&decoded, bytes, size));
    assert(chronos_settings_equal(&s, &decoded) && decoded.external.content == 222);
    s.external.clk = s.external.din;
    assert(!external_config_valid(&s.external));
    /* Bad hardware data is preserved without discarding unrelated alarms/settings. */
    assert(chronos_settings_text_schema.encode(&s, bytes, sizeof(bytes), &size));
    assert(chronos_settings_text_schema.decode(&decoded, bytes, size));
    assert(!external_config_valid(&decoded.external));
    s.external         = EXTERNAL_CONFIG_DEFAULT;
    s.external.enabled = true;
    s.external.driver  = EXT_DRIVER_NOKIA;
    s.external.clk     = 9;
    assert(!external_config_valid(&s.external));
    s.external.clk      = 2;
    s.external.contrast = 128;
    assert(!external_config_valid(&s.external));
    s.external             = EXTERNAL_CONFIG_DEFAULT;
    s.external.vfd_bus     = EXT_BUS_INTERNAL;
    s.external.vfd_address = 0x10;
    assert(!external_config_valid(&s.external));
    s.external.vfd_address = 0x13;
    assert(external_config_valid(&s.external));
    s.external.vfd_brightness = 111;
    assert(!external_config_valid(&s.external));
    s.external           = EXTERNAL_CONFIG_DEFAULT;
    s.external.enabled   = true;
    s.external.content   = EXT_STOPWATCH;
    chronos_ui_t      ui = {.active = 6, .content = true};
    chronos_ui_view_t v;
    chronos_screen_command(700, &ui, &r, &s, 0);
    chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
    assert(ui.editor_slot == 9 && v.count <= CHRONOS_UI_ROWS);
    chronos_screen_command(703, &ui, &r, &s, 0);
    assert(s.external.content == EXT_STOPWATCH);
    chronos_screen_key(UI_ESCAPE, &ui, &v, &r, &s, 0);
    assert(s.external.content == EXT_STOPWATCH);
    chronos_screen_command(700, &ui, &r, &s, 0);
    chronos_screen_command(701, &ui, &r, &s, 0);
    chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
    assert(v.count == 3);
    chronos_screen_key(UI_F4, &ui, &v, &r, &s, 0);
    assert(!s.external.enabled);
    chronos_screen_command(700, &ui, &r, &s, 0);
    chronos_screen_command(701, &ui, &r, &s, 0);
    chronos_screen_command(706, &ui, &r, &s, 0);
    assert(s.external.enabled && ui.external_retry_requested);
    for (unsigned tab = 0; tab < CHRONOS_UI_TABS; ++tab) {
        ui.active = tab;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        chronos_external_frame(&f, &s, &r, &local, NULL, 0);
        assert(f.source == EXT_STOPWATCH);
    }
    chronos_external_update(&s, &r, &local, 1789646400, 20000000, false);
    assert(received.source == EXT_STOPWATCH);
    s.external.content    = EXT_WORLD;
    s.external.world_city = 2;
    chronos_external_update(&s, &r, &local, 1789646400, 20100000, false);
    assert(received.source == EXT_WORLD);
    s.city_count = 0;
    chronos_external_frame(&f, &s, &r, &local, NULL, 0);
    assert(!strcmp(f.value, "No cities"));
    s.external.content = EXT_CLOCK;
    chronos_external_frame(&f, &s, &r, NULL, NULL, 0);
    assert(!strcmp(f.value, "No data"));
    char text[13];
    external_vfd_format(&f, text);
    assert(strlen(text) == 12 && !strncmp(text, "No data", 7));
    f = (external_frame_t){.source = EXT_STOPWATCH, .value = "00:12.3"};
    external_vfd_format(&f, text);
    assert(!strcmp(text, "SW 00M12S   "));
    strcpy(f.detail, "Paused");
    external_vfd_format(&f, text);
    assert(text[11] == 'P');
    f = (external_frame_t){.source = EXT_WORLD, .title = "Tokyo", .value = "12:34:56 PM"};
    external_vfd_format(&f, text);
    assert(!strcmp(text, "Tok 12 34 PM"));
    strcpy(f.value, "Unavailable");
    external_vfd_format(&f, text);
    assert(!strncmp(text, "Unavailable", 11));
    puts(
        "external policy/settings/UI: priority, restoration, fixed idle content, unavailable sources, persistence and "
        "validation passed");
}
