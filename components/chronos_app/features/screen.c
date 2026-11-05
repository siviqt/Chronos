#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "chronos_screen.h"
#include "chronos_clock.h"
enum {
    START = 1,
    PAUSE,
    RESUME,
    RESET,
    SPLIT,
    FORMAT,
    HOUR12,
    HOUR24, CONFIGURE, COMMIT, STOP, ALARM_BASE = 100
};
static const char* state(timing_state_t s) {
    static const char* names[] = {"Idle", "Running", "Paused", "Complete", "Stopped"};
    return names[s];
}
static void row(chronos_ui_view_t* v, const char* label, chronos_ui_verb_t verb, int command) {
    size_t i = v->count++;
    snprintf(v->detail[i], sizeof(v->detail[i]), "%.159s", label);
    v->rows[i] = (chronos_ui_action_t){verb, label, command};
}
/* External settings use a draft, the same Enter/F4 apply and Esc cancel
 * conventions as the other Settings editors. Unknown source IDs survive. */
static const char* external_content_name(unsigned source) {
    if (source == EXT_AUTO) return "Automatic";
    for (unsigned i = 0; i < CHRONOS_UI_TABS; ++i)
        if (chronos_ui_tab_sources[i] == source) return chronos_ui_tabs[i];
    if (source == EXT_WORLD) return "World Time";
    return "Unavailable (stored source)";
}
static void external_row(chronos_ui_view_t* v, const char* text, int command) {
    row(v, text, command == 705 || command == 706 ? UI_APPLY : command == 707 ? UI_BACK : command == 704 ? UI_OPEN : command == 701 ? UI_TOGGLE : (command == 715 || command == 717 || command == 718) ? UI_EDIT : command ? UI_CHOOSE : UI_NONE, command);
    v->rows[v->count - 1].target = v->detail[v->count - 1];
}
static void external_editor_view(chronos_ui_view_t* v, const chronos_ui_t* ui, const chronos_settings_t* s) {
    const external_config_t* c = &ui->external_draft;
    char                     text[160];
    snprintf(v->heading, sizeof(v->heading), "External Display");
    if (c->enabled && !external_config_valid(c))
        snprintf(v->heading, sizeof(v->heading), "Configuration error - check pins, bus, address and values");
    if (ui->external_page && c->enabled) {
        if (c->driver == EXT_DRIVER_NOKIA) {
            const char* names[] = {"CLK", "DIN", "CE", "DC", "RST"};
            unsigned    pins[]  = {c->clk, c->din, c->ce, c->dc, c->rst};
            for (unsigned i = 0; i < 5; ++i) {
                snprintf(text, sizeof(text), "%s GPIO: %u", names[i], pins[i]);
                external_row(v, text, 710 + (int)i);
            }
            snprintf(text, sizeof(text), "Contrast: %u (0-127)", c->contrast);
            external_row(v, text, 715);
            external_row(v, "Software SPI: 2 us phases; no SPI host", 0);
            external_row(v, "84 x 48; bias 4; TC 0; LIGHT disconnected", 0);
            external_row(v, "Pins 2-5 require external JTAG disconnected", 0);
        } else {
            const char* buses[] = {"Automatic (CATT, then internal)", "CATT (SDA 12 / SCL 13)",
                                   "Internal (SDA 9 / SCL 10)"};
            snprintf(text, sizeof(text), "Bus: %s", c->vfd_bus < 3 ? buses[c->vfd_bus] : "Invalid");
            external_row(v, text, 716);
            snprintf(text, sizeof(text), "I2C address: 0x%02X (match module jumpers)", c->vfd_address);
            external_row(v, text, 717);
            snprintf(text, sizeof(text), "Brightness / filament current: %u (0-110)", c->vfd_brightness);
            external_row(v, text, 718);
            external_row(v, "NE-HCS12SS59T-R1: 12 ASCII characters, 100 kHz", 0);
            external_row(v, "Current setting requires module firmware v2", 0);
            external_row(v, "0x10 is never probed on the internal bus", 0);
        }
        external_row(v, "Back to External Display", 707);
        external_row(v, "Apply External Display", 705);
        return;
    }
    snprintf(text, sizeof(text), "Status: %s", external_status_text(ui->external_status));
    external_row(v, text, 0);
    snprintf(text, sizeof(text), "External Display Support: %s", c->enabled ? "Enabled" : "Disabled");
    external_row(v, text, 701);
    if (c->enabled) {
        const char* drivers[] = {"Automatic detection", "Nokia 5110 / PCD8544", "VFD"};
        snprintf(text, sizeof(text), "Driver: %s", c->driver < 3 ? drivers[c->driver] : "Invalid");
        external_row(v, text, 702);
        if (c->content <= EXT_WORLD)
            snprintf(text, sizeof(text), "Content: %s", external_content_name(c->content));
        else
            snprintf(text, sizeof(text), "Content: Unavailable (stored source %u)", c->content);
        external_row(v, text, 703);
        if (c->content == EXT_WORLD) {
            snprintf(text, sizeof(text), "World city: %s",
                     c->world_city < s->city_count ? s->cities[c->world_city].name : "Unavailable");
            external_row(v, text, 708);
        }
        external_row(v, "Hardware configuration", 704);
        external_row(v, "Automatic detects VFD; select write-only Nokia manually", 0);
    }
    external_row(v, "Apply External Display", 705);
    if (c->enabled) external_row(v, "Apply and Detect / Retry", 706);
}
static void external_adjust(chronos_ui_t* ui, int command, int direction) {
    external_config_t* c       = &ui->external_draft;
    uint8_t*           value   = NULL;
    unsigned           minimum = 0, maximum = 0;
    switch (command) {
        case 701:
            c->enabled = !c->enabled;
            return;
        case 702:
            value   = &c->driver;
            maximum = 2;
            break;
        case 703: {
            unsigned choices[CHRONOS_UI_TABS + 2] = {EXT_AUTO}, count = 1, index = 0;
            for (unsigned i = 0; i < CHRONOS_UI_TABS; ++i)
                if (chronos_ui_tab_sources[i]) choices[count++] = chronos_ui_tab_sources[i];
            choices[count++] = EXT_WORLD;
            while (index < count && choices[index] != c->content) ++index;
            c->content = choices[index == count ? 0 : (index + count + direction) % count];
            return;
        }
        case 708:
            value   = &c->world_city;
            maximum = CHRONOS_CITY_LIMIT - 1;
            break;
        case 710:
            value = &c->clk;
            break;
        case 711:
            value = &c->din;
            break;
        case 712:
            value = &c->ce;
            break;
        case 713:
            value = &c->dc;
            break;
        case 714:
            value = &c->rst;
            break;
        case 715:
            value   = &c->contrast;
            maximum = 127;
            break;
        case 716:
            value   = &c->vfd_bus;
            maximum = 2;
            break;
        case 717:
            value   = &c->vfd_address;
            minimum = 0x10;
            maximum = 0x2f;
            break;
        case 718:
            value   = &c->vfd_brightness;
            maximum = 110;
            break;
        default:
            return;
    }
    if (command >= 710 && command <= 714) {
        const uint8_t pins[] = {2, 3, 4, 5, 15, 34};
        unsigned      index  = 0;
        while (index < 6 && pins[index] != *value) ++index;
        *value = pins[index == 6 ? 0 : (index + 6 + direction) % 6];
        return;
    }
    if (command == 715 || command == 717 || command == 718) {
        if (direction < 0 && *value > minimum) --*value;
        if (direction > 0 && *value < maximum) ++*value;
        return;
    }
    if (*value < minimum || *value > maximum)
        *value = minimum;
    else
        *value = (uint8_t)(minimum + (*value - minimum + maximum - minimum + 1 + direction) % (maximum - minimum + 1));
}

static unsigned number_limit(unsigned command, bool maximum) {
    switch (command) {
        case 800: return maximum ? 60 : 1;
        case 801: return maximum ? 60 : 5;
        case 40: return maximum ? CHRONOS_CITY_LIMIT : 0;
        case 715: return maximum ? 127 : 0;
        case 717: return maximum ? 0x2f : 0x10;
        case 718: return maximum ? 110 : 0;
        default: return 0;
    }
}
static const char* number_name(unsigned command) {
    switch (command) {
        case 800: return "Snooze minutes";
        case 801: return "Ring seconds";
        case 40: return "World city count";
        case 715: return "Contrast";
        case 717: return "I2C address (hex)";
        default: return "Brightness";
    }
}
static void number_open(chronos_ui_t* ui, unsigned command) {
    ui->field_edit = 8; ui->field_command = command;
    ui->field_error = false; ui->field_text[0] = 0; ui->cursor = 0;
}
static void number_set(chronos_ui_t* ui, chronos_settings_t* s, unsigned value) {
    switch (ui->field_command) {
        case 800: ui->draft[7] = value; break;
        case 801: ui->draft[8] = value; break;
        case 40:
            s->city_count = value;
            for (unsigned i = 0; i < value; ++i)
                if (!s->cities[i].name[0]) s->cities[i] = *chronos_zone(0);
            break;
        case 715: ui->external_draft.contrast = value; break;
        case 717: ui->external_draft.vfd_address = value; break;
        case 718: ui->external_draft.vfd_brightness = value; break;
    }
}
/* Choice editors retain parent focus; Enter accepts, F4 accepts and applies the form. */
static unsigned choices(const chronos_ui_t* ui, const chronos_settings_t* s,
                        unsigned values[8], const char* labels[8]) {
    static const char* const repeat[] = {"Once (next occurrence)", "Daily", "Weekdays"};
    static const char* const drivers[] = {"Automatic detection", "Nokia 5110 / PCD8544", "VFD"};
    static const char* const buses[] = {"Automatic (CATT, then internal)", "CATT", "Internal"};
    static const char* const pins[] = {"GPIO 2", "GPIO 3", "GPIO 4", "GPIO 5", "GPIO 15", "GPIO 34"};
    unsigned command = ui->field_command, count = 0;
    if (command == 802 || command == 702 || command == 716) {
        for (unsigned i = 0; i < 3; ++i) {
            values[i] = i; labels[i] = command == 802 ? repeat[i] : command == 702 ? drivers[i] : buses[i];
        }
        return 3;
    }
    if (command >= 710 && command <= 714) {
        const unsigned gpio[] = {2, 3, 4, 5, 15, 34};
        for (unsigned i = 0; i < 6; ++i) { values[i] = gpio[i]; labels[i] = pins[i]; }
        return 6;
    }
    if (command == 708) {
        for (unsigned i = 0; i < CHRONOS_CITY_LIMIT; ++i) {
            values[i] = i; labels[i] = i < s->city_count ? s->cities[i].name : "Unavailable city slot";
        }
        return CHRONOS_CITY_LIMIT;
    }
    values[count] = EXT_AUTO; labels[count++] = external_content_name(EXT_AUTO);
    for (unsigned i = 0; i < CHRONOS_UI_TABS; ++i) if (chronos_ui_tab_sources[i]) {
        values[count] = chronos_ui_tab_sources[i]; labels[count++] = chronos_ui_tabs[i];
    }
    values[count] = EXT_WORLD; labels[count++] = external_content_name(EXT_WORLD);
    return count;
}
static uint8_t* choice_value(chronos_ui_t* ui, unsigned command) {
    external_config_t* c = &ui->external_draft;
    switch (command) {
        case 702: return &c->driver; case 703: return &c->content;
        case 708: return &c->world_city; case 710: return &c->clk;
        case 711: return &c->din; case 712: return &c->ce;
        case 713: return &c->dc; case 714: return &c->rst;
        default: return &c->vfd_bus;
    }
}
static void choice_open(chronos_ui_t* ui, const chronos_settings_t* s, unsigned command) {
    ui->field_edit = 9; ui->field_command = command; ui->field_parent = ui->row;
    unsigned values[8]; const char* labels[8];
    unsigned count = choices(ui, s, values, labels);
    unsigned value = command == 802 ? ui->draft[3] : *choice_value(ui, command);
    ui->row = 0;
    for (unsigned i = 0; i < count; ++i) if (values[i] == value) ui->row = i;
}

/* Treat spaces and catalogue underscores alike, without locale-dependent matching. */
static bool contains(const char* text, const char* query) {
    for (; *text; ++text) {
        size_t i = 0;
        while (query[i] && text[i] &&
               tolower((unsigned char)(text[i] == '_' ? ' ' : text[i])) ==
               tolower((unsigned char)(query[i] == '_' ? ' ' : query[i]))) ++i;
        if (!query[i]) return true;
    }
    return !*query;
}
static bool zone_matches(size_t i, const chronos_ui_t* ui) {
    return contains(chronos_zone(i)->name, ui->zone_search) ||
           contains(chronos_zone(i)->timezone, ui->zone_search);
}
static unsigned zone_matches_count(const chronos_ui_t* ui) {
    unsigned count = 0;
    for (size_t i = 0; i < chronos_zone_count(); ++i) count += zone_matches(i, ui);
    return count;
}
static chronos_alarm_config_t draft_alarm(const chronos_ui_t* ui) {
    chronos_alarm_config_t a = {.enabled = ui->draft[0], .hour = ui->draft[1], .minute = ui->draft[2],
        .weekdays = ui->draft[9], .repeat = ui->draft[3],
        .snooze_minutes = ui->draft[7], .ring_seconds = ui->draft[8]};
    snprintf(a.label, sizeof(a.label), "%s", ui->label);
    return a;
}
static bool draft_dirty(const chronos_ui_t* ui, const chronos_settings_t* s) {
    if (!ui->editing) return false;
    if (ui->field_edit == 8 && ui->field_text[0] && ui->field_command != 40) return true;
    if (ui->active == 1) {
        chronos_alarm_config_t a = draft_alarm(ui);
        const chronos_alarm_config_t* b = &s->alarms[ui->editor_slot];
        char time[8]; snprintf(time, sizeof(time), "%02u:%02u", a.hour, a.minute);
        return a.enabled != b->enabled || a.hour != b->hour || a.minute != b->minute ||
            a.repeat != b->repeat || a.weekdays != b->weekdays || a.snooze_minutes != b->snooze_minutes ||
            a.ring_seconds != b->ring_seconds || strcmp(a.label, b->label) ||
            (ui->field_edit == 1 && strcmp(time, ui->field_text)) ||
            (ui->field_edit == 2 && strcmp(ui->field_text, ui->label));
    }
    if (ui->active == 3 || ui->active == 4) {
        uint64_t value = ui->active == 3 ? (uint64_t)ui->draft[0] * 60 + ui->draft[3] :
                                         (uint64_t)ui->draft[0] * 1000 + ui->draft[3];
        return value != (ui->active == 3 ? s->countdown_seconds : s->pacer_period_ms) ||
            (ui->active == 3 && ui->draft[1] != s->countdown_buzzer) ||
            (ui->field_edit == 5 && ui->field_text[0]);
    }
    if (ui->active != 6) return false;
    switch (ui->editor_slot) {
        case 11: return ui->clock_server[0] != 0;
        case 3: return ui->draft[0] != s->ntp_enabled || strcmp(ui->clock_server, s->ntp_server);
        case 5: return ui->draft[0] != s->keyboard_percent || ui->draft[1] != s->display_percent || ui->draft[3] != s->led_percent ||
                       (ui->field_edit == 7 && ui->field_text[0]);
        case 6: return strcmp(ui->clock_server, s->alarm_sound) != 0;
        case 9: return !external_config_equal(&ui->external_draft, &s->external);
        default: return false;
    }
}

static const char* const weekdays[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
static void editor_view(chronos_ui_view_t* v, const chronos_ui_t* ui, uint64_t now) {
    if (ui->active == 1) {
        snprintf(v->heading, sizeof(v->heading), "Alarm configuration");
        if (ui->field_edit == 3) {
            snprintf(v->heading, sizeof(v->heading), "Weekdays");
            for (unsigned i = 0; i < 7; ++i) {
                row(v, "Toggle day", UI_TOGGLE, 0);
                snprintf(v->detail[i], sizeof(v->detail[i]), "[%c] %s", ui->draft[9] & (1u << i) ? 'x' : ' ', weekdays[i]);
            }
            row(v, "Done selecting days", UI_DONE, 0);
            if (!ui->draft[9]) snprintf(v->heading, sizeof(v->heading), "Select at least one weekday");
            return;
        }
        row(v, "Enabled", UI_TOGGLE, 0);
        snprintf(v->detail[0], sizeof(v->detail[0]), "Enabled: [%c]", ui->draft[0] ? 'x' : ' ');
        row(v, "Time", UI_EDIT, 0);
        snprintf(v->detail[1], sizeof(v->detail[1]), "Time: %02u:%02u", ui->draft[1], ui->draft[2]);
        row(v, "Repeat", UI_CHOOSE, 0);
        const char* repeats[] = {"Once (next occurrence)", "Daily", "Weekdays"};
        snprintf(v->detail[2], sizeof(v->detail[2]), "Repeat: %s", repeats[ui->draft[3]]);
        row(v, "Select weekdays", ui->draft[3] == CHRONOS_ALARM_WEEKDAYS ? UI_EDIT : UI_UNAVAILABLE, 0);
        snprintf(v->detail[3], sizeof(v->detail[3]), "Days: %s", ui->draft[3] == CHRONOS_ALARM_WEEKDAYS ? "Select days" : "Choose Weekdays repeat first");
        row(v, "Snooze minutes", UI_EDIT, 0);
        snprintf(v->detail[4], sizeof(v->detail[4]), "Snooze minutes: %u", ui->draft[7]);
        row(v, "Ring seconds", UI_EDIT, 0);
        snprintf(v->detail[5], sizeof(v->detail[5]), "Ring seconds: %u", ui->draft[8]);
        row(v, "Label", UI_EDIT, 0);
        snprintf(v->detail[6], sizeof(v->detail[6]), "Label: %s", ui->label);
        row(v, "Apply alarm", UI_APPLY, COMMIT);
        if (ui->field_edit == 1 || ui->field_edit == 2) {
            unsigned field_row = ui->field_edit == 1 ? 1 : 6;
            snprintf(v->heading, sizeof(v->heading), ui->field_edit == 1 ?
                     "Editing alarm time" : "Editing alarm label");
            snprintf(v->detail[field_row], sizeof(v->detail[field_row]), "%s%s", ui->field_edit == 1 ? "Time: " : "Label: ", ui->field_text);
            v->rows[field_row] = (chronos_ui_action_t){UI_DONE, ui->field_edit == 1 ? "Time" : "Label", 0};
            if (ui->field_edit == 1 &&
                (((ui->field_text[0] - '0') * 10 + ui->field_text[1] - '0') >= 24 ||
                 ((ui->field_text[3] - '0') * 10 + ui->field_text[4] - '0') >= 60))
                snprintf(v->heading, sizeof(v->heading), "Invalid time - use 00:00 through 23:59");
            v->cursor_visible = (now / 500000) % 2 == 0;
            v->cursor_row = field_row;
            v->cursor_column = (ui->field_edit == 1 ? 6 : 7) + ui->cursor;
        }
        if (ui->draft[3] == CHRONOS_ALARM_WEEKDAYS && !ui->draft[9])
            snprintf(v->heading, sizeof(v->heading), "Select at least one weekday");
        return;
    }
    snprintf(v->heading, sizeof(v->heading), "%s configuration", chronos_ui_tabs[ui->active]);
    row(v, ui->active == 3 ? "Minutes" : "Seconds", UI_EDIT, 0);
    snprintf(v->detail[0], sizeof(v->detail[0]), ui->active == 3 ?
             "Minutes: %u" : "Seconds: %u", ui->draft[0]);
    row(v, ui->active == 3 ? "Seconds" : "Milliseconds", UI_EDIT, 0);
    snprintf(v->detail[1], sizeof(v->detail[1]), ui->active == 3 ? "Seconds: %02u" : "Milliseconds: %03u", ui->draft[3]);
    if (ui->active == 4 && ((uint64_t)ui->draft[0] * 1000 + ui->draft[3] == 0 ||
                          (uint64_t)ui->draft[0] * 1000 + ui->draft[3] > UINT32_MAX))
        snprintf(v->heading, sizeof(v->heading), "Interval must be 0.001 through 4294967.295 seconds");
    if (ui->active == 3) {
        if (ui->draft[0] * 60 + ui->draft[3] == 0 || ui->draft[0] * 60 + ui->draft[3] > 3600)
            snprintf(v->heading, sizeof(v->heading), "Duration must be 00:01 through 60:00");
        row(v, "Completion sound", UI_TOGGLE, 0);
        snprintf(v->detail[2], sizeof(v->detail[1]), "Completion sound: %s", ui->draft[1] ? "On" : "Off");
    }
    row(v, "Apply configuration", UI_APPLY, COMMIT);
    if (ui->field_edit == 5) {
        const char* name = ui->active == 3 ? (ui->row ? "Seconds" : "Minutes") : (ui->row ? "Milliseconds" : "Seconds");
        unsigned maximum = ui->active == 3 ? (ui->row ? 59 : 60) : (ui->row ? 999 : UINT32_MAX / 1000);
        snprintf(v->heading, sizeof(v->heading), ui->draft[2] ? "Invalid %s - use 0-%u" : "Editing %s (0-%u)", name, maximum);
        snprintf(v->detail[ui->row], sizeof(v->detail[0]), "%s: %s", name, ui->field_text);
        v->rows[ui->row] = (chronos_ui_action_t){UI_DONE, "Value", 0};
        v->cursor_visible = (now / 500000) % 2 == 0;
        v->cursor_row = ui->row;
        v->cursor_column = strlen(name) + 2 + ui->cursor;
    }

}
void chronos_screen_view(chronos_ui_view_t* v, const chronos_ui_t* ui, chronos_runtime_t* r,
                         const chronos_settings_t* s, const chronos_local_time_t* local, uint64_t now,
                         const char* notice, bool storage) {
    *v = (chronos_ui_view_t){.storage_available = storage, .city_count = s->city_count, .history_available = r->stopwatch.lap_count > 0, .battery_valid = ui->battery_valid, .battery_percent = ui->battery_percent, .draft_dirty = draft_dirty(ui, s),
        .split_available = !ui->editing && r->stopwatch.session.state == TIMING_RUNNING, .theme = s->theme, .font_size = s->font_size, .volume_percent = s->volume_percent, .storage_notice = notice ? notice : storage ? NULL : chronos_settings_notice(SETTINGS_UNAVAILABLE)};
    if (v->storage_notice) {
        snprintf(v->storage_message, sizeof(v->storage_message), "%s", v->storage_notice);
        v->storage_notice = v->storage_message;
    }
    snprintf(v->local_time, sizeof(v->local_time), "--:--:--");
    if (local) {
        snprintf(v->local_time, sizeof(v->local_time), "%02u:%02u:%02u%s",
                 s->use_24_hour ? local->hour : (local->hour % 12 ? local->hour % 12 : 12),
                 local->minute, local->second, s->use_24_hour ? "" : local->hour < 12 ? " AM" : " PM");
        v->year = local->date.year; v->month = local->date.month; v->day = local->date.day;
        v->minute_of_day = local->hour * 60 + local->minute;
    }
    snprintf(v->heading, sizeof(v->heading), "%s", chronos_ui_tabs[ui->active]);
    if (ui->editing && ui->active >= 1 && ui->active <= 4) {
        editor_view(v, ui, now);
        if (ui->active == 1 && ui->draft[3] == CHRONOS_ALARM_ONCE && !v->year && !ui->field_edit)
            snprintf(v->heading, sizeof(v->heading), "Clock unavailable - cannot apply a once alarm");
    } else if (ui->active == 0 || ui->active == 5) {
        if (local)
            snprintf(v->heading, sizeof(v->heading), "Local\t%02u:%02u:%02u%s\t%04u-%02u-%02u",
                     s->use_24_hour ? local->hour : (local->hour % 12 ? local->hour % 12 : 12), local->minute,
                     local->second, s->use_24_hour ? "" : (local->hour < 12 ? " AM" : " PM"), local->date.year,
                     local->date.month, local->date.day);
        else
            snprintf(v->heading, sizeof(v->heading), "Clock unavailable - check Settings > Time");
        if (ui->active == 0) {
            row(v, "World cities", UI_UNAVAILABLE, 0);
            row(v, "Sync now", s->ntp_enabled && s->wifi_enabled && s->time_source == CHRONOS_TIME_NTP ? UI_ACTIVATE : UI_UNAVAILABLE, CHRONOS_CLOCK_SYNC);
        } else {
            snprintf(v->heading, sizeof(v->heading), "Activity");
            uint64_t values[] = {timing_session_elapsed(&r->stopwatch.session, now),
                                 timing_countdown_remaining(&r->countdown, now), r->pacer.period_us};
            const timing_session_t* sessions[] = {&r->stopwatch.session, &r->countdown.session, &r->pacer.session};
            for (size_t i = 0; i < 3; ++i) {
                row(v, "", UI_NONE, 0);
                snprintf(v->detail[i], sizeof(v->detail[i]), "%s\t%s", chronos_ui_tabs[i + 2], state(sessions[i]->state));
                if (i == 2 && sessions[i]->state != TIMING_IDLE) {
                    size_t used = strlen(v->detail[i]);
                    snprintf(v->detail[i] + used, sizeof(v->detail[i]) - used, " - %llu.%03llu s period",
                             (unsigned long long)(values[i] / 1000000),
                             (unsigned long long)(values[i] / 1000 % 1000));
                } else if (sessions[i]->state != TIMING_IDLE) {
                    size_t used = strlen(v->detail[i]);
                    snprintf(v->detail[i] + used, sizeof(v->detail[i]) - used, " - %02llu:%02llu%s",
                             (unsigned long long)(values[i] / 60000000),
                             (unsigned long long)(values[i] / 1000000 % 60), i == 2 ? " period" : "");
                }
            }
            unsigned enabled = 0, ringing = 0, snoozed = 0;
            for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
                enabled += s->alarms[i].enabled;
                ringing += r->alarms.runtime[i].phase == CHRONOS_ALARM_RINGING;
                snoozed += r->alarms.runtime[i].phase == CHRONOS_ALARM_SNOOZED;
            }
            row(v, "", UI_NONE, 0);
            snprintf(v->detail[3], sizeof(v->detail[3]), "Alarms: %u enabled / %u ringing / %u snoozed", enabled, ringing, snoozed);

        }
    } else if (ui->active == 1) {
        for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
            row(v, "Alarm editor", UI_EDIT, ALARM_BASE + (int)i);
            snprintf(v->detail[i], sizeof(v->detail[i]), "%u. %02u:%02u %s - %s", (unsigned)i + 1, s->alarms[i].hour, s->alarms[i].minute, s->alarms[i].label,
                     s->alarms[i].enabled ? "Enabled" : "Disabled");
            v->rows[i].target = v->detail[i];
        }
    } else if (ui->active >= 2 && ui->active <= 4) {
        timing_session_t* session = ui->active == 2   ? &r->stopwatch.session
                                    : ui->active == 3 ? &r->countdown.session
                                                      : &r->pacer.session;
        uint64_t          elapsed =
            ui->active == 3 ? (session->state == TIMING_IDLE ? (uint64_t)s->countdown_seconds * 1000000 :
                              timing_countdown_remaining(&r->countdown, now)) : timing_session_elapsed(session, now);
        v->timer_state = session->state;
        snprintf(v->heading, sizeof(v->heading), "%s\t%s\t%02llu:%02llu.%01llu", chronos_ui_tabs[ui->active],
                 state(session->state), (unsigned long long)(elapsed / 60000000),
                 (unsigned long long)(elapsed / 1000000 % 60), (unsigned long long)(elapsed / 100000 % 10));
        if (ui->active == 2) {
            for (size_t i = r->stopwatch.lap_count; i > 0; --i) {
                const timing_lap_t* lap = &r->stopwatch.laps[i - 1];
                uint64_t segment = lap->lap_us / 100000, total = lap->total_us / 100000;
                row(v, "", UI_NONE, 0);
                snprintf(v->detail[v->count - 1], sizeof(v->detail[0]),
                         "Segment %02llu:%02llu.%llu / Total %02llu:%02llu.%llu",
                         (unsigned long long)(segment / 600), (unsigned long long)(segment / 10 % 60),
                         (unsigned long long)(segment % 10), (unsigned long long)(total / 600),
                         (unsigned long long)(total / 10 % 60), (unsigned long long)(total % 10));
            }
        } else
            row(v, "Configuration", UI_OPEN, CONFIGURE);
    } else if (ui->editing && ui->editor_slot == 10) {
        const char* sources[] = {"NTP", "Tanmatsu RTC", "Manual"};
        snprintf(v->heading, sizeof(v->heading), "Time - selected source: %s", sources[s->time_source]);
        row(v, "Clock format", UI_OPEN, FORMAT);
        row(v, "Local timezone", UI_OPEN, 21);
        row(v, "NTP settings / select NTP", UI_OPEN, 23);
        row(v, s->wifi_enabled ? "NTP sync now" : "NTP sync unavailable - Wi-Fi is Off",
            s->wifi_enabled && s->ntp_enabled && s->time_source == CHRONOS_TIME_NTP ? UI_ACTIVATE : UI_UNAVAILABLE, CHRONOS_CLOCK_SYNC);
        row(v, "Use Tanmatsu RTC now", UI_ACTIVATE, 681);
        row(v, "Manual date and HH:MM:SS", UI_OPEN, 682);
        row(v, "Back to Settings", UI_BACK, 684);
    } else if (ui->editing && ui->editor_slot == 11) {
        snprintf(v->heading, sizeof(v->heading), "%s", ui->draft[1] ? "Invalid date/time (2000-2099); check DST gap" : "Manual time - hardware RTC is never changed");
        row(v, "Date/time", ui->field_edit == 4 ? UI_DONE : UI_EDIT, 51);
        snprintf(v->detail[0], sizeof(v->detail[0]), "Date/time: %.100s", ui->clock_server);
        v->cursor_visible = ui->field_edit == 4 && (now / 500000) % 2 == 0;
        v->cursor_column = 11 + ui->cursor;
        row(v, "Format: YYYY-MM-DD HH:MM:SS (local timezone)", UI_NONE, 0);
        row(v, "Apply Manual time", UI_APPLY, 683);
    } else if (ui->editing && ui->editor_slot == 9) {
        external_editor_view(v, ui, s);
    } else if (ui->editing && ui->editor_slot == 6) {
        snprintf(v->heading, sizeof(v->heading), "%s", ui->draft[1] ? "Invalid filename - use up to 64 letters, digits, dot, dash, underscore" : "Alarm sound - empty filename uses built-in audio");
        row(v, "Sound filename", ui->field_edit == 4 ? UI_DONE : UI_EDIT, 51);
        snprintf(v->detail[0], sizeof(v->detail[0]), "File: %.64s", ui->clock_server);
        v->cursor_visible = ui->field_edit == 4 && (now / 500000) % 2 == 0;
        v->cursor_column = 6 + ui->cursor;
        row(v, "Next WAV / built-in", UI_CHOOSE, 670);
        row(v, "Apply sound", UI_APPLY, 66);
    } else if (ui->editing && ui->editor_slot == 8) {
        snprintf(v->heading, sizeof(v->heading), "Font size");
        row(v, s->font_size == 0 ? "[x] Small" : "[ ] Small", UI_APPLY, 68);
        row(v, s->font_size == 1 ? "[x] Large (default)" : "[ ] Large (default)", UI_APPLY, 69);
    } else if (ui->editing && ui->editor_slot == 4) {
        snprintf(v->heading, sizeof(v->heading), "Theme");
        row(v, "Vector", UI_APPLY, 60);
        row(v, "Neon District", UI_APPLY, 61);
        row(v, "Neon Operator", UI_APPLY, 62);
    } else if (ui->editing && ui->editor_slot == 5) {
        snprintf(v->heading, sizeof(v->heading), "Illumination");
        row(v, "Brightness", UI_EDIT, 640);
        snprintf(v->detail[0], sizeof(v->detail[0]), "Keyboard: %u%% (5-100%%)", ui->draft[0]);
        row(v, "Brightness", UI_EDIT, 640);
        snprintf(v->detail[1], sizeof(v->detail[1]), "Display: %u%% (5-100%%)", ui->draft[1]);
        row(v, "Brightness", UI_EDIT, 640);
        snprintf(v->detail[2], sizeof(v->detail[2]), "LEDs: %u%% (0-100%%)", ui->draft[3]);
        row(v, "Apply illumination", UI_APPLY, 64);
        if (ui->field_edit == 7) {
            snprintf(v->detail[ui->row], sizeof(v->detail[0]), "%s: %s (%u-100%%)", ui->row == 2 ? "LEDs" : ui->row ? "Display" : "Keyboard", ui->field_text, ui->row == 2 ? 0 : 5);
            v->rows[ui->row].verb = UI_DONE;
            v->cursor_visible = (now / 500000) % 2 == 0;
            v->cursor_row = ui->row; v->cursor_column = (ui->row == 2 ? 6 : ui->row ? 9 : 10) + ui->cursor;
            if (ui->draft[2]) snprintf(v->heading, sizeof(v->heading), "Invalid brightness - use %u-100%%", ui->row == 2 ? 0 : 5);
        }
    } else if (ui->editing && ui->editor_slot == 3) {
        snprintf(v->heading, sizeof(v->heading), "%s", ui->draft[1] ? "Invalid NTP server - use a hostname or IPv4 address" :
                 ui->field_edit == 4 ? "Editing NTP server" : "NTP settings - Apply selects NTP source");
        row(v, "Enable NTP", UI_TOGGLE, 50);
        snprintf(v->detail[0], sizeof(v->detail[0]), "NTP: %s", ui->draft[0] ? "enabled" : "paused");
        row(v, "Server", ui->field_edit == 4 ? UI_DONE : UI_EDIT, 51);
        /* Keep the cursor and suffix visible even for a long hostname. */
        size_t offset = ui->field_edit == 4 && ui->cursor > 70 ? ui->cursor - 70 : 0;
        snprintf(v->detail[1], sizeof(v->detail[1]), "Server: %.145s", ui->clock_server + offset);
        v->cursor_visible = ui->field_edit == 4 && (now / 500000) % 2 == 0;
        v->cursor_row = 1; v->cursor_column = 8 + ui->cursor - offset;
        row(v, "Apply NTP", UI_APPLY, 52);
    } else if (ui->editing && (ui->editor_slot == 1 || ui->editor_slot == 7)) {
        unsigned matches = zone_matches_count(ui), pages = (matches + 9) / 10;
        v->zone_pages = pages;
        snprintf(v->heading, sizeof(v->heading), "%s zone - %u matches",
                 ui->editor_slot == 1 ? "Local" : "World", matches);
        row(v, "Search", ui->field_edit == 6 ? UI_DONE : UI_EDIT, 902);
        snprintf(v->detail[0], sizeof(v->detail[0]), "Search: %s", ui->zone_search);
        v->cursor_visible = ui->field_edit == 6 && (now / 500000) % 2 == 0;
        v->cursor_column = ui->field_edit == 6 ? 8 + ui->cursor : 0;
        unsigned match = 0;
        for (size_t i = 0; i < chronos_zone_count(); ++i) {
            if (!zone_matches(i, ui)) continue;
            unsigned position = match++;
            if (position / 10 != ui->zone_page) continue;
            row(v, chronos_zone(i)->name, UI_APPLY, 1000 + (int)i);
            bool selected = ui->editor_slot == 7 ?
                            (!strcmp(s->cities[ui->zone_city].name, chronos_zone(i)->name) ||
                             (!strchr(s->cities[ui->zone_city].name, '/') &&
                              !strcmp(s->cities[ui->zone_city].timezone, chronos_zone(i)->timezone))) :
                            !strcmp(s->timezone, chronos_zone(i)->timezone);
            snprintf(v->detail[v->count - 1], sizeof(v->detail[0]), "[%c] %s", selected ? 'x' : ' ', chronos_zone(i)->name);
        }
        if (!matches) row(v, "No time zones found", UI_NONE, 0);
        if (pages > 1) {
            row(v, "Previous page", UI_ACTIVATE, 900);
            row(v, "Next page", UI_ACTIVATE, 901);
        }

    } else if (ui->editing && ui->editor_slot == 2) {
        snprintf(v->heading, sizeof(v->heading), "World cities - changes apply immediately");
        row(v, "Number of cities", UI_EDIT, 40);
        snprintf(v->detail[0], sizeof(v->detail[0]), "Number of cities: %u", s->city_count);
        for (size_t i = 0; i < s->city_count; ++i) {
            row(v, "Change city", UI_OPEN, 41 + (int)i);
            snprintf(v->detail[i + 1], sizeof(v->detail[i + 1]), "%u. %s", (unsigned)i + 1, s->cities[i].name);
        }
    } else if (ui->editing) {
        snprintf(v->heading, sizeof(v->heading), "Clock format - changes save automatically");
        row(v, "12-hour", UI_APPLY, HOUR12);
        row(v, "24-hour", UI_APPLY, HOUR24);
    } else {
        row(v, "Time", UI_OPEN, 680);
        row(v, "Local timezone", UI_EDIT, 21);
        row(v, "World cities", UI_EDIT, 22);
        row(v, s->wifi_enabled ? "Wi-Fi: On" : "Wi-Fi: Off", UI_TOGGLE, 685);
        row(v, "Alarm sound", UI_EDIT, 65);
        row(v, "Theme", UI_EDIT, 25);
        row(v, "Illumination", UI_EDIT, 63);
        row(v, "Font size", UI_EDIT, 67);
        row(v, "External Display", UI_EDIT, 700);
        row(v, "Save to SD", storage ? UI_APPLY : UI_UNAVAILABLE, 24);
    }
    if (ui->field_edit == 8) {
        const char* name = number_name(ui->field_command);
        unsigned minimum = number_limit(ui->field_command, false), maximum = number_limit(ui->field_command, true);
        if (ui->field_command == 717)
            snprintf(v->heading, sizeof(v->heading), "%s %s (0x%02X-0x%02X)", ui->field_error ? "Invalid" : "Editing", name, minimum, maximum);
        else snprintf(v->heading, sizeof(v->heading), "%s %s (%u-%u)", ui->field_error ? "Invalid" : "Editing", name, minimum, maximum);
        snprintf(v->detail[ui->row], sizeof(v->detail[0]), "%s: %s", name, ui->field_text);
        v->rows[ui->row].verb = UI_DONE;
        v->cursor_visible = (now / 500000) % 2 == 0;
        v->cursor_row = ui->row; v->cursor_column = strlen(name) + 2 + ui->cursor;
    } else if (ui->field_edit == 9) {
        unsigned values[8]; const char* labels[8];
        unsigned count = choices(ui, s, values, labels);
        snprintf(v->heading, sizeof(v->heading), "%s", ui->field_command == 802 ? "Repeat" : "Choose value");
        v->count = 0;
        for (unsigned i = 0; i < count; ++i) row(v, labels[i], UI_DONE, 0);
    }
    size_t used = 0;
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        v->alarm_ringing |= r->alarms.runtime[i].phase == CHRONOS_ALARM_RINGING;
        const char* status = (r->alarms.runtime[i].phase == CHRONOS_ALARM_RINGING) ? "Ringing"
                             : r->alarms.runtime[i].missed
                                 ? (r->alarms.runtime[i].phase == CHRONOS_ALARM_SNOOZED ? "Missed / Snoozed" : "Missed")
                             : (r->alarms.runtime[i].phase == CHRONOS_ALARM_SNOOZED) ? "Snoozed"
                                                                                     : NULL;
        if (status && used < sizeof(v->notification)) {
            snprintf(v->notices[v->notice_count++], sizeof(v->notices[0]), "%s: %s", status, s->alarms[i].label);
            int n = snprintf(v->notification + used, sizeof(v->notification) - used, "%s%s: %s", used ? " | " : "",
                             status, s->alarms[i].label);
            if (n > 0) used += (size_t)n;
        }
    }
    if (r->countdown.session.state == TIMING_COMPLETE) {
        snprintf(v->notices[v->notice_count++], sizeof(v->notices[0]), "Countdown complete");
        if (used < sizeof(v->notification)) snprintf(v->notification + used, sizeof(v->notification) - used,
                                                     "%sCountdown complete", used ? " | " : "");
    }
    bool missed = false;
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) missed |= r->alarms.runtime[i].missed;
    if (v->alarm_ringing) snprintf(v->alert_hint, sizeof(v->alert_hint), "Any letter Snooze all ringing   Space Dismiss alerts");
    else if (missed || r->countdown.session.state == TIMING_COMPLETE)
        snprintf(v->alert_hint, sizeof(v->alert_hint), "Space Clear %s", missed && r->countdown.session.state == TIMING_COMPLETE ?
                 "missed / countdown" : missed ? "missed alarms" : "countdown");
    if (v->storage_notice) snprintf(v->notices[v->notice_count++], sizeof(v->notices[0]), "%s", v->storage_notice);
    if (!v->notification[0] && v->storage_notice) snprintf(v->notification, sizeof(v->notification), "%s", v->storage_notice);
    if (ui->editing) {
        if (!strncmp(v->heading, "Invalid", 7) || !strncmp(v->heading, "Interval must", 13) ||
            !strncmp(v->heading, "Duration must", 13) || !strncmp(v->heading, "Select at least", 15) ||
            !strncmp(v->heading, "Configuration error", 19) || !strncmp(v->heading, "Clock unavailable", 17)) {
            snprintf(v->error, sizeof(v->error), "%s", v->heading);
            snprintf(v->heading, sizeof(v->heading), "%s configuration", chronos_ui_tabs[ui->active]);
        }
        if (ui->active == 3)
            snprintf(v->context, sizeof(v->context), "Duration: %02u:%02u - used on next Start", ui->draft[0], ui->draft[3]);
        if (ui->active == 4)
            snprintf(v->context, sizeof(v->context), "Interval: %u.%03u s - used on next Start", ui->draft[0], ui->draft[3]);
    }

}
static void command_impl(int command, chronos_ui_t* ui, chronos_runtime_t* r, chronos_settings_t* s, uint64_t now) {
    if (command == 680) { ui->time_menu = true; ui->editing = true; ui->editor_slot = 10; ui->row = 0; return; }
    if (command == 684) { ui->time_menu = false; ui->editing = false; ui->row = 0; return; }
    if (command == 685) { s->wifi_enabled = !s->wifi_enabled; ui->clock_reconfigure_requested = true; return; }
    if (command == 681) { s->time_source = CHRONOS_TIME_RTC; ui->clock_reconfigure_requested = true; return; }
    if (command == 682) {
        ui->editing = true; ui->editor_slot = 11; ui->row = 0; ui->field_edit = 4;
        ui->clock_server[0] = ui->text_backup[0] = 0; ui->cursor = 0; ui->draft[1] = 0; return;
    }
    if (command == 683) {
        if (!chronos_clock_parse_manual(ui->clock_server, s->timezone, &ui->manual_seconds)) { ui->draft[1] = 1; return; }
        s->time_source = CHRONOS_TIME_MANUAL; ui->manual_requested = true;
        ui->clock_reconfigure_requested = true; ui->editing = false; return;
    }
    if (command == 700) {
        ui->external_draft=s->external; ui->external_page=0;
        ui->editing=true; ui->editor_slot=9; ui->row=1; return;
    }
    if (command>=701 && command<=718) {
        if (command == 704 || command == 707) {
            ui->external_page = command == 704;
            ui->row = command == 704 ? 0 : 4 + (ui->external_draft.content == EXT_WORLD);
            return;
        }
        if(command==705 || command==706){
            if(ui->external_draft.enabled && !external_config_valid(&ui->external_draft))return;
            s->external=ui->external_draft;
            ui->external_retry_requested=command==706;
            ui->editing=false;ui->row=8;return;
        }
        if (command == 701) external_adjust(ui, command, 1);
        else if (command == 715 || command == 717 || command == 718) number_open(ui, command);
        else choice_open(ui, s, command);
        return;
    }
    if (command == 67) {
        ui->editing = true; ui->editor_slot = 8; ui->row = s->font_size; return;
    }
    if (command == 68 || command == 69) {
        s->font_size = command - 68; ui->editing = false; ui->row = 7; return;
    }
    if (command == 902) { strcpy(ui->text_backup, ui->zone_search); ui->field_edit = 6; ui->cursor = strlen(ui->zone_search); ui->row = 0; return; }
    if (command == 670) { ui->sound_scan_requested = true; return; }
    if (command == 65) {
        ui->editing = true; ui->editor_slot = 6; ui->row = 0; ui->field_edit = 0;
        ui->draft[1] = 0; strcpy(ui->clock_server, s->alarm_sound); return;
    }
    if (command == 66) {
        chronos_settings_t candidate = *s;
        if (strlen(ui->clock_server) >= sizeof(candidate.alarm_sound)) { ui->draft[1] = 1; return; }
        strcpy(candidate.alarm_sound, ui->clock_server);
        if (!chronos_settings_valid(&candidate)) { ui->draft[1] = 1; return; }
        *s = candidate; ui->editing = false; ui->row = 4; ui->sound_reload_requested = true; return;
    }
    if (command == 25 || command == 63) {
        ui->editing = true; ui->editor_slot = command == 25 ? 4 : 5;
        ui->row = command == 25 ? (unsigned)s->theme : 0;
        ui->draft[0] = s->keyboard_percent; ui->draft[1] = s->display_percent; ui->draft[2] = 0; ui->draft[3] = s->led_percent;
        return;
    }
    if (command >= 60 && command <= 62) {
        s->theme = (chronos_theme_t)(command - 60);
        ui->editing = false; ui->row = 5; return;
    }
    if (command == 640) {
        ui->field_edit = 7; ui->field_text[0] = 0; ui->cursor = 0; ui->draft[2] = 0; return;
    }
    if (command == 64) {
        s->keyboard_percent = ui->draft[0]; s->display_percent = ui->draft[1]; s->led_percent = ui->draft[3];
        ui->editing = false; ui->row = 6; return;
    }
    if (command == 24) { ui->settings_save_requested = true; return; }
    if (command == 23) {
        ui->editing = true; ui->editor_slot = 3; ui->row = 0; ui->field_edit = 0;
        ui->draft[0] = s->ntp_enabled; ui->draft[1] = 0;
        strcpy(ui->clock_server, s->ntp_server); return;
    }
    if (command == 50) { ui->draft[0] = !ui->draft[0]; return; }
    if (command == 51) { strcpy(ui->text_backup, ui->clock_server); ui->field_edit = 4; ui->cursor = strlen(ui->clock_server); return; }
    if (command == 52) {
        chronos_settings_t candidate = *s;
        candidate.ntp_enabled = ui->draft[0];
        candidate.time_source = CHRONOS_TIME_NTP;
        strcpy(candidate.ntp_server, ui->clock_server);
        if (!chronos_settings_valid(&candidate)) { ui->draft[1] = 1; return; }
        *s = candidate; ui->clock_reconfigure_requested = true;
        ui->editing = false; ui->row = 0; return;
    }
    if (command == 21 || command == 22) {
        ui->editing = true; ui->editor_slot = command - 20; ui->zone_search[0] = 0; ui->field_edit = 0; ui->zone_page = 0; ui->row = 0; return;
    }
    if (command == 900 || command == 901) {
        unsigned pages = (zone_matches_count(ui) + 9) / 10;
        if (pages < 2) return;
        ui->zone_page = (ui->zone_page + pages + (command == 900 ? -1 : 1)) % pages;
        ui->row = 0; return;
    }
    if (command >= 1000 && (size_t)(command - 1000) < chronos_zone_count()) {
        const chronos_city_t* zone = chronos_zone((size_t)(command - 1000));
        if (ui->editor_slot == 7) {
            s->cities[ui->zone_city] = *zone;
            ui->editor_slot = 2; ui->row = ui->zone_city + 1;
        } else {
            strcpy(s->timezone, zone->timezone);
            ui->editing = false; ui->row = 0;
        }
        return;
    }
    if (command == 40) {
        number_open(ui, 40);
        return;
    }
    if (command >= 41 && command < 46) {
        ui->zone_city = (unsigned)(command - 41);
        ui->editor_slot = 7; ui->zone_search[0] = 0; ui->field_edit = 0; ui->zone_page = 0; ui->row = 0;
        for (size_t i = 0; i < chronos_zone_count(); ++i)
            if (!strcmp(s->cities[ui->zone_city].name, chronos_zone(i)->name)) {
                ui->zone_page = (unsigned)i / 10; ui->row = (unsigned)i % 10 + 1; break;
            }
        return;
    }
    if (command == CHRONOS_CLOCK_SYNC) { ui->sync_requested = true; return; }
    if (command >= ALARM_BASE && command < ALARM_BASE + CHRONOS_ALARM_LIMIT) {
        ui->field_edit = 0;
        ui->editor_slot = command - ALARM_BASE;
        const chronos_alarm_config_t* a = &s->alarms[ui->editor_slot];
        unsigned values[] = {a->enabled, a->hour, a->minute, a->repeat, 0, 0, 0, a->snooze_minutes, a->ring_seconds, a->weekdays};
        memcpy(ui->draft, values, sizeof(values));
        snprintf(ui->label, sizeof(ui->label), "%s", a->label);
        ui->editing = ui->content = true;
        ui->row = 0;
        return;
    }
    if (command == CONFIGURE) {
        ui->field_edit = 0; ui->draft[2] = 0;
        ui->draft[0] = ui->active == 3 ? s->countdown_seconds / 60 : s->pacer_period_ms / 1000;
        ui->draft[3] = ui->active == 3 ? s->countdown_seconds % 60 : s->pacer_period_ms % 1000;
        ui->draft[1] = s->countdown_buzzer;
        ui->editing = ui->content = true;
        ui->row = 0;
        return;
    }
    if (command == COMMIT) {
        if (ui->active == 1) {
            chronos_alarm_config_t a = draft_alarm(ui);
            if (!chronos_alarms_set(&r->alarms, ui->editor_slot, &a)) return;
            s->alarms[ui->editor_slot] = a;
        } else if (ui->active == 3) {
            unsigned duration = ui->draft[0] * 60 + ui->draft[3];
            if (!duration || duration > 3600) return;
            s->countdown_seconds = duration;
            if (r->countdown.session.state != TIMING_RUNNING && r->countdown.session.state != TIMING_PAUSED)
                timing_session_reset(&r->countdown.session);
            s->countdown_buzzer = r->countdown_buzzer = ui->draft[1];
        } else if (ui->active == 4) {
            uint64_t interval = (uint64_t)ui->draft[0] * 1000 + ui->draft[3];
            if (!interval || interval > UINT32_MAX) return;
            s->pacer_period_ms = (unsigned)interval;
        }
        ui->editing = false;
        ui->row = 0;
        return;
    }
    if (command == FORMAT) {
        ui->editor_slot = 0;
        ui->editing = true;
        ui->row     = s->use_24_hour ? 1 : 0;
        return;
    }
    if (command == HOUR12 || command == HOUR24) {
        s->use_24_hour = command == HOUR24;
        ui->editing    = false;
        ui->row        = 0;
        return;
    }
    timing_session_t* session = ui->active == 2   ? &r->stopwatch.session
                                : ui->active == 3 ? &r->countdown.session
                                                  : &r->pacer.session;
    if (command == STOP) {
        timing_session_pause(session, now);
        session->state = TIMING_STOPPED;
    }
    if (command == PAUSE) timing_session_pause(session, now);
    if (command == RESUME) timing_session_resume(session, now);
    if (command == RESET) {
        if (ui->active == 2)
            timing_stopwatch_reset(&r->stopwatch);
        else
            timing_session_reset(session);
    }
    if (command == SPLIT) (void)timing_stopwatch_split(&r->stopwatch, now);
    if (command == START) {
        if (ui->active == 2) timing_stopwatch_start(&r->stopwatch, now);
        if (ui->active == 3) (void)timing_countdown_start(&r->countdown, (uint64_t)s->countdown_seconds * 1000000, now);
        if (ui->active == 4) (void)timing_pacer_start(&r->pacer, (uint64_t)s->pacer_period_ms * 1000, now);
    }
}

void chronos_screen_command(int command, chronos_ui_t* ui, chronos_runtime_t* r, chronos_settings_t* s, uint64_t now) {
    bool editing = ui->editing;
    unsigned return_row = ui->row;
    command_impl(command, ui, r, s, now);
    if (!editing && ui->editing) {
        ui->return_row = return_row;
    } else if (editing && !ui->editing) {
        ui->field_edit = 0;
        ui->row = ui->return_row;
        if (ui->time_menu) { ui->editing = true; ui->editor_slot = 10; ui->row = 0; }
    }
}

static void leave(chronos_ui_t* ui, chronos_ui_key_t key, const chronos_ui_view_t* v) {
    ui->leave_prompt = ui->notifications = ui->time_menu = false;
    ui->field_edit = 0;
    if (key == UI_F1) ui->exit_requested = true;
    else (void)chronos_ui_key(ui, v, UI_F5);
}

void chronos_screen_key(chronos_ui_key_t key, chronos_ui_t* ui, const chronos_ui_view_t* v,
                        chronos_runtime_t* r, chronos_settings_t* s, uint64_t now) {
    if (ui->leave_prompt) {
        if (key == UI_UP && ui->leave_choice) --ui->leave_choice;
        if (key == UI_DOWN && ui->leave_choice < 2) ++ui->leave_choice;
        if (key == UI_ESCAPE || (key == UI_ENTER && ui->leave_choice == 2)) ui->leave_prompt = false;
        else if (key == UI_ENTER) {
            chronos_ui_key_t destination = (chronos_ui_key_t)ui->leave_key;
            ui->leave_prompt = false;
            if (!ui->leave_choice) {
                ui->notifications = false;
                chronos_screen_key(UI_F4, ui, v, r, s, now);
                if (ui->editing && !(ui->time_menu && ui->editor_slot == 10)) return; /* Invalid draft stays open. */
            }
            leave(ui, destination, v);
        }
        return;
    }
    if (key == UI_F1 || key == UI_F5) {
        if (draft_dirty(ui, s)) {
            ui->leave_prompt = true; ui->leave_key = key; ui->leave_choice = 2;
        } else leave(ui, key, v);
        return;
    }
    if (ui->notifications) {
        if (key == UI_ESCAPE) ui->notifications = false;
        if (key == UI_UP && ui->notice_row) --ui->notice_row;
        if (key == UI_DOWN && ui->notice_row + 1 < v->notice_count) ++ui->notice_row;
        return;
    }
    if (ui->field_edit == 9) {
        unsigned values[8]; const char* labels[8];
        unsigned count = choices(ui, s, values, labels);
        if (key == UI_UP && ui->row) --ui->row;
        if (key == UI_DOWN && ui->row + 1 < count) ++ui->row;
        if (key == UI_ENTER || key == UI_ESCAPE || key == UI_F4) {
            if (key != UI_ESCAPE) {
                if (ui->field_command == 802) ui->draft[3] = values[ui->row];
                else *choice_value(ui, ui->field_command) = values[ui->row];
            }
            ui->row = ui->field_parent; ui->field_edit = 0;
            if (key == UI_F4) chronos_screen_key(key, ui, v, r, s, now);
        }
        return;
    }
    if (ui->field_edit == 8) {
        if (key == UI_F4 && ui->field_command == 40) return;
        if (key == UI_ESCAPE) { ui->field_edit = 0; ui->field_error = false; }
        if (key == UI_ENTER || key == UI_F4) {
            unsigned long long value = strtoull(ui->field_text, NULL, ui->field_command == 717 ? 16 : 10);
            ui->field_error = !ui->field_text[0] || value < number_limit(ui->field_command, false) || value > number_limit(ui->field_command, true);
            if (!ui->field_error) {
                number_set(ui, s, (unsigned)value); ui->field_edit = 0;
                if (key == UI_F4 && ui->field_command != 40) chronos_screen_key(key, ui, v, r, s, now);
            }
        }
        if (key == UI_LEFT && ui->cursor) --ui->cursor;
        if (key == UI_RIGHT && ui->cursor < strlen(ui->field_text)) ++ui->cursor;
        return;
    }
    if (key == UI_DOWN && !ui->content && ui->highlighted != ui->active) {
        ui->active = ui->highlighted; ui->row = 0;
        ui->content = ui->active != 5 && (ui->active != 2 || r->stopwatch.lap_count > 0);
        return;
    }
    if (key == UI_ESCAPE && ui->editing) {
        if (ui->field_edit) {
            if (ui->field_edit == 3) { ui->draft[9] = ui->weekday_backup; ui->row = 3; }
            if (ui->field_edit == 4) { strcpy(ui->clock_server, ui->text_backup); ui->draft[1] = 0; }
            if (ui->field_edit == 6) { snprintf(ui->zone_search, sizeof(ui->zone_search), "%.64s", ui->text_backup); ui->zone_page = 0; }
            if (ui->field_edit == 5 || ui->field_edit == 7) ui->draft[2] = 0;
            ui->field_edit = 0;
        } else if (ui->active == 6 && ui->editor_slot == 7) {
            ui->editor_slot = 2; ui->row = ui->zone_city + 1;
        } else if (ui->active == 6 && ui->editor_slot == 9 && ui->external_page) {
            ui->external_page = 0; ui->row = 4 + (ui->external_draft.content == EXT_WORLD);
        } else if (ui->time_menu && ui->editor_slot != 10) {
            ui->editor_slot = 10; ui->row = 0;
        } else {
            ui->time_menu = false;
            ui->editing = false; ui->row = ui->return_row;
        }
        return;
    }
    chronos_ui_controls_t controls;
    chronos_ui_controls(ui, v, &controls);
    if ((key == UI_F2 && !controls.function[1]) || (key == UI_F3 && !controls.function[2]) ||
        (key == UI_F4 && !controls.function[3])) return;
    if (ui->active == 6 && ui->editing && ui->field_edit == 7) {
        if (key == UI_ENTER || key == UI_F4) {
            unsigned long value = strtoul(ui->field_text, NULL, 10);
            ui->draft[2] = !ui->field_text[0] || value > 100 || value < (ui->row == 2 ? 0u : 5u);
            if (!ui->draft[2]) { ui->draft[ui->row == 2 ? 3 : ui->row] = (unsigned)value; ui->field_edit = 0; }
            if (key == UI_F4 && !ui->field_edit) chronos_screen_command(64, ui, r, s, now);
        }
        if (key == UI_LEFT && ui->cursor) --ui->cursor;
        if (key == UI_RIGHT && ui->cursor < strlen(ui->field_text)) ++ui->cursor;
        return;
    }
    if (key == UI_F4 && ui->active == 6 && ui->editing && ui->editor_slot != 9) {
        ui->field_edit = 0;
        chronos_screen_command(ui->editor_slot == 11 ? 683 : ui->editor_slot == 3 ? 52 : ui->editor_slot == 5 ? 64 : 66, ui, r, s, now);
        return;
    }
    /* Information rows in hardware pages are readable, but not focus stops. */
    if (ui->active == 6 && ui->editing && ui->editor_slot == 9 && (key == UI_UP || key == UI_DOWN)) {
        int next = (int)ui->row + (key == UI_UP ? -1 : 1);
        while (next >= 0 && next < (int)v->count && v->rows[next].verb == UI_NONE)
            next += key == UI_UP ? -1 : 1;
        if (next >= 0 && next < (int)v->count) ui->row = (unsigned)next;
        return;
    }
    if (ui->active == 6 && !ui->editing && ui->content && (key == UI_UP || key == UI_DOWN)) {
        if (key == UI_UP) {
            if (ui->row % 5) --ui->row;
            else { ui->content = false; ui->highlighted = ui->active; }
        } else if (ui->row % 5 < 4 && ui->row + 1 < v->count) ++ui->row;
        return;
    }
    if (ui->active==6 && ui->editing && ui->editor_slot==9) {
        if(key==UI_F4){chronos_screen_command(705,ui,r,s,now);return;}
        if(key==UI_LEFT || key==UI_RIGHT){
            if(ui->row<v->count && v->rows[ui->row].verb != UI_TOGGLE)external_adjust(ui,v->rows[ui->row].command,key==UI_LEFT?-1:1);
            return;
        }
    }
    if (key == UI_F4 && ui->editing && (ui->active == 3 || ui->active == 4)) {
        if (ui->field_edit == 5) chronos_screen_key(UI_ENTER, ui, v, r, s, now);
        if (!ui->field_edit) chronos_screen_command(COMMIT, ui, r, s, now);
        return;
    }
    if (ui->active == 6 && ui->editing && ui->field_edit == 6 && key != UI_F5) {
        if (key == UI_ENTER || key == UI_DOWN) {
            ui->field_edit = 0;
            if (v->count > 1) ui->row = 1;
        }
        if (key == UI_LEFT && ui->cursor) --ui->cursor;
        if (key == UI_RIGHT && ui->cursor < strlen(ui->zone_search)) ++ui->cursor;
        return;
    }
    if (ui->active == 6 && ui->editing && (ui->editor_slot == 1 || ui->editor_slot == 7) &&
        (key == UI_LEFT || key == UI_RIGHT)) {
        chronos_screen_command(key == UI_LEFT ? 900 : 901, ui, r, s, now); return;
    }
    if (ui->active == 6 && ui->editing && ui->editor_slot == 5 && ui->row < 3 && (key == UI_LEFT || key == UI_RIGHT)) {
        unsigned max = 100, min = ui->row == 2 ? 0 : 5;
        unsigned* value = &ui->draft[ui->row == 2 ? 3 : ui->row];
        if (key == UI_LEFT) *value = *value < min + 5 ? min : *value - 5;
        else *value = *value + 5 > max ? max : *value + 5;
        return;
    }
    if (ui->active == 6 && ui->editing && (ui->editor_slot == 3 || ui->editor_slot == 6 || ui->editor_slot == 11) && ui->field_edit == 4 && key != UI_F5) {
        if (key == UI_ENTER) { ui->field_edit = 0; return; }
        if (key == UI_LEFT && ui->cursor) --ui->cursor;
        if (key == UI_RIGHT && ui->cursor < strlen(ui->clock_server)) ++ui->cursor;
        return;
    }
    if (key == UI_TAB) {
        if (!ui->editing && r->stopwatch.session.state == TIMING_RUNNING)
            (void)timing_stopwatch_split(&r->stopwatch, now);
        return;
    }
    if (ui->active == 6 && !ui->editing && ui->content && (key == UI_LEFT || key == UI_RIGHT)) {
        if (key == UI_LEFT && ui->row >= 5) ui->row -= 5;
        if (key == UI_RIGHT && ui->row < 5) ui->row = ui->row + 5 < v->count ? ui->row + 5 : v->count - 1;
        return;
    }
    if (key == UI_F4 && ui->active == 6 && !ui->editing) {
        for (size_t i = 0; i < v->count; ++i)
            if (v->rows[i].command == 24 && v->rows[i].verb == UI_APPLY)
                chronos_screen_command(24, ui, r, s, now);
        return;
    }
    if (key == UI_F4 && ui->active == 1 && ui->editing) {
        if (ui->field_edit == 1 || ui->field_edit == 2)
            chronos_screen_key(UI_ENTER, ui, v, r, s, now);
        if (ui->field_edit == 3 && ui->draft[9]) ui->field_edit = 0;
        if (ui->field_edit) return;
        ui->row = 7;
        chronos_screen_key(UI_ENTER, ui, v, r, s, now);
        return;
    }
    if (key == UI_F2 || key == UI_F3 || key == UI_F4) {
        if (!ui->editing && ui->active >= 2 && ui->active <= 4) {
            timing_session_t* session = ui->active == 2 ? &r->stopwatch.session :
                                       ui->active == 3 ? &r->countdown.session : &r->pacer.session;
            if (ui->active == 3) (void)timing_countdown_remaining(&r->countdown, now);
            bool active = session->state == TIMING_RUNNING || session->state == TIMING_PAUSED;
            int command = key == UI_F2 ? (session->state == TIMING_PAUSED ? RESUME : PAUSE) :
                          key == UI_F3 ? RESET : active ? STOP : START;
            chronos_screen_command(command, ui, r, s, now);
            if (key == UI_F3 || (key == UI_F4 && !active)) {
                ui->row = 0;
                if (ui->active == 2) ui->content = false;
            }
        }
        return;
    }
    if (ui->editing && ui->active == 1 && key != UI_F5) {
        if (ui->field_edit == 1 || ui->field_edit == 2) {
            if (key == UI_ENTER) {
                if (ui->field_edit == 1) {
                    unsigned hour = (ui->field_text[0] - '0') * 10 + ui->field_text[1] - '0';
                    unsigned minute = (ui->field_text[3] - '0') * 10 + ui->field_text[4] - '0';
                    if (hour >= 24 || minute >= 60) return;
                    ui->draft[1] = hour; ui->draft[2] = minute;
                } else snprintf(ui->label, sizeof(ui->label), "%s", ui->field_text);
                ui->field_edit = 0;
            } else if (key == UI_LEFT && ui->cursor) {
                do { --ui->cursor; } while (ui->cursor && ((unsigned char)ui->field_text[ui->cursor] & 0xc0) == 0x80);
                if (ui->field_edit == 1 && ui->cursor == 2) --ui->cursor;
            } else if (key == UI_RIGHT && ui->cursor < strlen(ui->field_text)) {
                do { ++ui->cursor; } while (((unsigned char)ui->field_text[ui->cursor] & 0xc0) == 0x80);
                if (ui->field_edit == 1 && ui->cursor == 2) ++ui->cursor;
            }
            return;
        }
        if (ui->field_edit == 3) {
            if (key == UI_UP && ui->row) --ui->row;
            if (key == UI_DOWN && ui->row < 7) ++ui->row;
            if (key == UI_ENTER && ui->row < 7) ui->draft[9] ^= 1u << ui->row;
            if (key == UI_ENTER && ui->row == 7 && ui->draft[9]) {
                ui->field_edit = 0; ui->row = 3;
            }
            return;
        }
        if (key == UI_ENTER && (ui->row == 1 || ui->row == 6)) {
            ui->field_edit = ui->row == 1 ? 1 : 2;
            if (ui->field_edit == 1) snprintf(ui->field_text, sizeof(ui->field_text), "%02u:%02u", ui->draft[1], ui->draft[2]);
            else snprintf(ui->field_text, sizeof(ui->field_text), "%s", ui->label);
            ui->cursor = ui->field_edit == 1 ? 0 : strlen(ui->field_text);
            return;
        }
        if (key == UI_ENTER || key == UI_LEFT || key == UI_RIGHT) {
            if (ui->row == 0 && key == UI_ENTER) ui->draft[0] = !ui->draft[0];
            if (ui->row == 2) {
                if (key == UI_ENTER) choice_open(ui, s, 802);
                else ui->draft[3] = (ui->draft[3] + (key == UI_LEFT ? 2 : 1)) % 3;
            } else if (ui->draft[3] == CHRONOS_ALARM_WEEKDAYS && ui->row == 3 && key == UI_ENTER) {
                ui->field_edit = 3; ui->weekday_backup = ui->draft[9]; ui->row = 0;
            } else if (ui->row == 4 || ui->row == 5) {
                unsigned command = ui->row == 4 ? 800 : 801;
                if (key == UI_ENTER) number_open(ui, command);
                else {
                    unsigned* value = &ui->draft[ui->row == 4 ? 7 : 8];
                    if (key == UI_LEFT && *value > number_limit(command, false)) --*value;
                    if (key == UI_RIGHT && *value < number_limit(command, true)) ++*value;
                }
            } else if (ui->row == 7 && key == UI_ENTER) {
                chronos_screen_command(COMMIT, ui, r, s, now);
            }
            return;
        }
    }
    if (ui->editing && (ui->active == 3 || ui->active == 4) && key != UI_F5) {
        if (ui->field_edit == 5) {
            if (key == UI_ENTER) {
                uint64_t value = 0;
                for (size_t i = 0; ui->field_text[i]; ++i) value = value * 10 + ui->field_text[i] - '0';
                uint64_t maximum = ui->active == 3 ? (ui->row == 0 ? 60 : 59) : (ui->row == 0 ? UINT32_MAX / 1000 : 999);
                ui->draft[2] = !ui->field_text[0] || value > maximum;
                if (!ui->draft[2]) { ui->draft[ui->row == 1 ? 3 : 0] = (unsigned)value; ui->field_edit = 0; }
            }
            if (key == UI_LEFT && ui->cursor) --ui->cursor;
            if (key == UI_RIGHT && ui->cursor < strlen(ui->field_text)) ++ui->cursor;
            return;
        }
        if (key == UI_ENTER && (ui->row == 0 || ui->row == 1)) {
            ui->field_edit = 5; ui->draft[2] = 0;
            ui->field_text[0] = 0; ui->cursor = 0;
            return;
        }
        if (ui->row == 2 && ui->active == 3 && key == UI_ENTER) {
            ui->draft[1] = !ui->draft[1];
            return;
        }
        if ((ui->row == 0 || ui->row == 1) && (key == UI_LEFT || key == UI_RIGHT)) {
            unsigned maximum = ui->active == 3 ? (ui->row == 0 ? 60 : 59) : (ui->row == 0 ? UINT32_MAX / 1000 : 999);
            unsigned minimum = 0;
            unsigned* value = &ui->draft[ui->row == 1 ? 3 : 0];
            if (key == UI_LEFT && *value > minimum) --*value;
            if (key == UI_RIGHT && *value < maximum) ++*value;
            return;
        }
    }
    int command = chronos_ui_key(ui, v, key);
    if (command) chronos_screen_command(command, ui, r, s, now);
}
void chronos_screen_text(unsigned key, chronos_ui_t* ui) {
    if (ui->leave_prompt) return;
    if (ui->field_edit == 9 && key != 8) return;
    if (key == 8 && (!ui->field_edit || ui->field_edit == 3 || ui->field_edit == 9)) { ui->notifications = !ui->notifications; ui->notice_row = 0; return; }
    if (ui->notifications) return;
    if (ui->active == 6 && ui->editing && ui->field_edit == 6) {
        size_t len = strlen(ui->zone_search);
        if (key == 8 && ui->cursor) {
            memmove(ui->zone_search + ui->cursor - 1, ui->zone_search + ui->cursor, len - ui->cursor + 1);
            --ui->cursor;
        } else if (key >= 32 && key <= 126 && len + 1 < sizeof(ui->zone_search)) {
            memmove(ui->zone_search + ui->cursor + 1, ui->zone_search + ui->cursor, len - ui->cursor + 1);
            ui->zone_search[ui->cursor++] = (char)key;
        }
        ui->zone_page = 0; ui->row = 0;
        return;
    }
    if (ui->editing && (ui->field_edit == 5 || ui->field_edit == 7 || ui->field_edit == 8)) {
        size_t len = strlen(ui->field_text);
        if (key == 8 && ui->cursor) {
            memmove(ui->field_text + ui->cursor - 1, ui->field_text + ui->cursor, len - ui->cursor + 1);
            --ui->cursor;
        } else if (((key >= '0' && key <= '9') || (ui->field_edit == 8 && ui->field_command == 717 && key < 128 && isxdigit((int)key))) && len < 10) {
            memmove(ui->field_text + ui->cursor + 1, ui->field_text + ui->cursor, len - ui->cursor + 1);
            ui->field_text[ui->cursor++] = (char)key;
        }
        if (ui->field_edit == 8) ui->field_error = false;
        else ui->draft[2] = 0;
        return;
    }
    if (ui->editing && ui->active == 6 && (ui->editor_slot == 3 || ui->editor_slot == 6 || ui->editor_slot == 11) && ui->field_edit == 4) {
        size_t len = strlen(ui->clock_server);
        if (key == 8 && ui->cursor) {
            memmove(ui->clock_server + ui->cursor - 1, ui->clock_server + ui->cursor, len - ui->cursor + 1);
            --ui->cursor;
        } else if (key >= (ui->editor_slot == 11 ? 32u : 33u) && key <= 126 && len + 1 < sizeof(ui->clock_server)) {
            memmove(ui->clock_server + ui->cursor + 1, ui->clock_server + ui->cursor, len - ui->cursor + 1);
            ui->clock_server[ui->cursor++] = (char)key;
        }
        ui->draft[1] = 0;
        return;
    }
    if (!ui->editing || ui->active != 1) return;
    if (ui->field_edit == 1) {
        if (key == 8 && ui->cursor) {
            --ui->cursor; if (ui->cursor == 2) --ui->cursor;
        } else if (key >= '0' && key <= '9' && ui->cursor < 5) {
            ui->field_text[ui->cursor++] = (char)key;
            if (ui->cursor == 2) ++ui->cursor;
        }
        return;
    }
    if (ui->field_edit != 2) return;
    size_t len = strlen(ui->field_text);
    if (key == 8 && ui->cursor) {
        unsigned previous = ui->cursor;
        do { --previous; } while (previous && ((unsigned char)ui->field_text[previous] & 0xc0) == 0x80);
        memmove(ui->field_text + previous, ui->field_text + ui->cursor, len - ui->cursor + 1);
        ui->cursor = previous;
    } else if (key >= 32 && key <= 126 && len + 1 < sizeof(ui->field_text)) {
        char candidate[121];
        memcpy(candidate, ui->field_text, ui->cursor);
        candidate[ui->cursor] = (char)key;
        memcpy(candidate + ui->cursor + 1, ui->field_text + ui->cursor, len - ui->cursor + 1);
        if (chronos_text_valid(candidate, sizeof(candidate), 30)) {
            memcpy(ui->field_text, candidate, len + 2); ++ui->cursor;
        }
    }
}
