#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chronos_ui_render.h"
#include "chronos_typography.h"
#include "chronos_screen.h"
#include "chronos_clock.h"

static void sync_status_fit_checks(void) {
    const char* statuses[] = {"RTC sampled into system time", "NTP paused - Wi-Fi off / NTP disabled",
        "NTP synchronized (system clock retained)", "NTP waiting - RTC startup time",
        "Manual - enter date/time", "Synchronizing NTP..."};
    const pax_font_t* sizes[] = {&ibm_plex_sans_regular_14, &ibm_plex_sans_regular_16};
    for (unsigned size = 0; size < 2; ++size) for (unsigned i = 0; i < sizeof(statuses)/sizeof(statuses[0]); ++i) {
        const pax_font_t* font = sizes[size];
        if (pax_text_size(font, chronos_font_size(font), statuses[i]).x > 364) font = &ibm_plex_sans_regular_12;
        assert(pax_text_size(font, chronos_font_size(font), statuses[i]).x <= 364);
        assert(302 + chronos_font_height(font) <= 336);
    }
}
static void typography_checks(void) {
    const pax_font_t* fonts[] = {
        CHRONOS_FONT_CAPTION, CHRONOS_FONT_SECONDARY, CHRONOS_FONT_BODY,
        CHRONOS_FONT_BUTTON, CHRONOS_FONT_SECTION, CHRONOS_FONT_DATE,
        CHRONOS_FONT_TITLE, CHRONOS_FONT_TIME_SMALL, CHRONOS_FONT_TIME_NORMAL,
        CHRONOS_FONT_TIME_MEDIUM, CHRONOS_FONT_TIME_LARGE, CHRONOS_FONT_CLOCK_MAIN
    };
    for (unsigned f = 0; f < sizeof(fonts) / sizeof(*fonts); ++f) {
        const pax_font_t* font = fonts[f];
        for (unsigned r = 0; r < font->n_ranges; ++r) {
            const pax_font_range_t* range = &font->ranges[r];
            assert(range->bitmap_var.bpp == 4);
            for (unsigned cp = range->start; cp <= range->end; ++cp) {
                const pax_bmpv_t* d = &range->bitmap_var.dims[cp - range->start];
                /* Native font line boxes must contain all supported glyphs. */
                assert(d->draw_y >= 0);
                assert(d->draw_y + d->draw_h <= chronos_font_height(font));
            }
        }
        if (f >= 7) {
            float advance = pax_text_size(font, font->default_size, "0").x;
            for (char digit = '1'; digit <= '9'; ++digit) {
                char glyph[] = {digit, 0};
                assert(pax_text_size(font, font->default_size, glyph).x == advance);
            }
        }
    }
    /* UINT64_MAX microseconds: maximum elapsed minutes and fractional time. */
    assert(pax_text_size(CHRONOS_FONT_TIME_LARGE, 56, "307445734561:49.5").x < 754);
    assert(pax_text_size(CHRONOS_FONT_TIME_NORMAL, 24, "23:59").x < 100);
    assert(pax_text_size(CHRONOS_FONT_TIME_MEDIUM, 28, "23:59").x <= 90);
    assert(pax_text_size(CHRONOS_FONT_BODY, 16, "Los Angeles").x <= 154);
    /* Large navigation and action labels still fit their fixed device regions. */
    float tabs_width = 16;
    for (unsigned i = 0; i < CHRONOS_UI_TABS; ++i)
        tabs_width += pax_text_size(&ibm_plex_sans_medium_20, 20, chronos_ui_tabs[i]).x + 22;
    assert(tabs_width < 784);
    assert(pax_text_size(&ibm_plex_sans_regular_16, 16, "Any letter: Snooze    Space: Stop").x < 760);
    assert(pax_text_size(&ibm_plex_sans_medium_20, 20, "Running - 307445734561:49").x < 344);
    assert(pax_text_size(&ibm_plex_sans_medium_20, 20, "Running - 4294967.295 s period").x < 344);
    const char* labels[] = {"Exit", "Resume", "Reset", "Start", "Next tab", "Lights"};
    for (unsigned i = 0; i < 6; ++i)
        {
            assert(pax_text_size(CHRONOS_FONT_BUTTON, 16, labels[i]).x <= 94);
            assert(pax_text_size(&ibm_plex_sans_medium_20, 20, labels[i]).x <= 94);
        }
}

/* Footer slots must not advertise inactive timer keys on unrelated tabs/editors. */
static void footer_checks(pax_buf_t* frame, unsigned font_size) {
    const char* states[] = {"Idle", "Running", "Paused", "Complete", "Stopped"};
    for (unsigned tab = 0; tab < CHRONOS_UI_TABS; ++tab)
        for (unsigned editing = 0; editing < 2; ++editing)
            for (unsigned state = 0; state < 5; ++state) {
                chronos_ui_t ui = {.active = tab, .highlighted = tab, .editing = editing != 0};
                chronos_ui_view_t view = {.font_size = font_size, .timer_state = state, .storage_available = true};
                snprintf(view.heading, sizeof(view.heading), "Timer\t%s\t00:00.0", states[state]);
                chronos_ui_render(frame, &ui, &view);
                pax_join();
                bool timer = tab >= 2 && tab <= 4 && !editing;
                bool expected[] = {true, timer && (state == 1 || state == 2), timer, timer || ((tab == 1 || tab == 3 || tab == 4) && editing) || (tab == 6 && !editing), true, true};
                uint32_t background = pax_get_pixel(frame, 0, 479);
                for (unsigned key = 0; key < 6; ++key) {
                    bool visible = false;
                    for (unsigned y = 448; y < 480; ++y)
                        for (unsigned x = 16 + key * 128; x < 16 + (key + 1) * 128; ++x)
                            visible |= pax_get_pixel(frame, x, y) != background;
                    assert(visible == expected[key]);
                }
            }
}

/* Removing each row must change its own content region, even without focus. */
static void layout_checks(pax_buf_t* frame) {
    uint32_t* baseline = malloc(800 * 480 * sizeof(*baseline));
    assert(baseline);
    const char* settings[] = {"Clock format", "Local timezone", "World cities", "NTP settings",
        "Alarm sound", "Theme", "Illumination", "Font size", "Save settings"};
    for (unsigned font = 0; font < 2; ++font) for (unsigned theme = 0; theme < 3; ++theme) {
        for (unsigned mode = 0; mode < 2; ++mode) {
            chronos_ui_t ui = {.active = mode ? 6 : 0};
            chronos_ui_view_t view = {.font_size = font, .theme = theme, .count = mode ? 9 : 5};
            strcpy(view.heading, mode ? "Settings" : "Local\t12:34:56\t2026-09-17\tDST off");
            for (size_t i = 0; i < view.count; ++i)
                snprintf(view.detail[i], sizeof(view.detail[i]), "%s", mode ? settings[i] : "Los Angeles\t11:11:11 PM\t2026-09-17\tDST on");
            chronos_ui_render(frame, &ui, &view); pax_join();
            for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x)
                baseline[y * 800 + x] = pax_get_pixel(frame, x, y);
            for (size_t row = 0; row < view.count; ++row) {
                char saved[160]; strcpy(saved, view.detail[row]); view.detail[row][0] = 0;
                chronos_ui_render(frame, &ui, &view); pax_join();
                unsigned changed = 0;
                for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x)
                    if (baseline[y * 800 + x] != pax_get_pixel(frame, x, y)) {
                        assert(y >= 112 && y < 342);
                        ++changed;
                    }
                assert(changed > 100);
                strcpy(view.detail[row], saved);
            }
        }
    }
    free(baseline);
}

/* Compare complete rendered frames, not string lengths: a seconds update may
 * change only the local seconds cells. World clocks, labels, dates and tabs
 * must remain unchanged, including in 12-hour format. */
static void dock_width(const chronos_ui_t* ui, const chronos_ui_view_t* view) {
    chronos_ui_controls_t c;
    chronos_ui_controls(ui, view, &c);
    const pax_font_t* font = view->font_size ? &ibm_plex_sans_regular_16 : &ibm_plex_sans_regular_14;
    const char* lines[] = {c.primary, c.secondary, view->alert_hint, view->error};
    for (unsigned i = 0; i < 4; ++i) {
        float width = pax_text_size(font, font->default_size, lines[i]).x;
        if (width > 744) fprintf(stderr, "Dock overflow %.0f: %s (tab %u, slot %u, row %u, field %u)\n",
                                width, lines[i], ui->active, ui->editor_slot, ui->row, ui->field_edit);
        assert(width <= 744);
    }
}
static void audit_frame(pax_buf_t* frame, const chronos_ui_t* ui, const chronos_ui_view_t* view, const char* name) {
    dock_width(ui, view);
    chronos_ui_render(frame, ui, view); pax_join();
    const char* directory = getenv("CHRONOS_AUDIT_PREVIEW");
    if (!directory || !name) return;
    char path[256]; snprintf(path, sizeof(path), "%s/%s-%u.ppm", directory, name, view->font_size);
    FILE* out = fopen(path, "wb"); assert(out);
    fprintf(out, "P6\n800 480\n255\n");
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x) {
        uint32_t p = pax_get_pixel(frame, x, y);
        unsigned char rgb[] = {p >> 16, p >> 8, p};
        assert(fwrite(rgb, 1, 3, out) == 3);
    }
    fclose(out);
}
/* Enumerate real form rows/modes, including long errors and simultaneous alerts. */
static void navigation_layout_checks(pax_buf_t* frame) {
    chronos_settings_t s;
    chronos_settings_defaults(&s);
    chronos_runtime_t r;
    chronos_runtime_init(&r, &s);
    chronos_local_time_t local = {.date = {2026, 9, 23}, .hour = 16, .minute = 30, .second = 25};
    chronos_ui_view_t v;
    chronos_ui_t ui;
    timing_stopwatch_start(&r.stopwatch, 0);
    for (unsigned font = 0; font < 2; ++font) {
        s.font_size = font;
        ui = (chronos_ui_t){0};
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        chronos_clock_view(&v, &ui, &s, true, 1790177425, "Synced");
        audit_frame(frame, &ui, &v, "clock-tabs");
        ui.content = true; ui.row = 0;
        chronos_clock_view(&v, &ui, &s, true, 1790177425, "Synced");
        audit_frame(frame, &ui, &v, "clock-city");
        ui = (chronos_ui_t){.active = 6, .highlighted = 6, .content = true, .row = 9};
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "settings");
        chronos_screen_command(700, &ui, &r, &s, 0);
        ui.external_draft.enabled = true;
        for (unsigned driver = 1; driver <= 2; ++driver) {
            ui.external_page = 1; ui.external_draft.driver = driver;
            chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
            audit_frame(frame, &ui, &v, driver == 1 ? "nokia" : "vfd");
        }
        for (unsigned tab = 0; tab < 7; ++tab) {
            ui = (chronos_ui_t){.active = tab, .highlighted = tab, .content = true};
            chronos_screen_view(&v, &ui, &r, &s, &local, 0, "SD unavailable: changes are session-only", false);
            for (unsigned row = 0; row < v.count; ++row) {
                ui.row = row; dock_width(&ui, &v);
            }
            ui.content = false;
            for (unsigned highlight = 0; highlight < 7; ++highlight) {
                ui.highlighted = highlight; dock_width(&ui, &v);
            }
        }
        for (unsigned tab = 1; tab < 7; ++tab) {
            if (tab == 2 || tab == 5) continue;
            unsigned slots = tab == 6 ? 10 : 1;
            for (unsigned slot = 0; slot < slots; ++slot) {
                ui = (chronos_ui_t){.active = tab, .highlighted = tab, .content = true, .editing = true,
                    .editor_slot = slot, .external_draft = EXTERNAL_CONFIG_DEFAULT};
                ui.external_draft.enabled = true;
                ui.external_draft.content = EXT_WORLD;
                ui.draft[0] = 1; ui.draft[7] = 5; ui.draft[8] = 15; ui.draft[9] = 31;
                strcpy(ui.clock_server, "pool.ntp.org");
                chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
                for (unsigned row = 0; row < v.count; ++row) {
                    ui.row = row; dock_width(&ui, &v);
                }
                ui.row = 0;
                if (tab == 6 && slot == 9) {
                    for (unsigned driver = 0; driver < 3; ++driver) {
                        ui.external_page = 1; ui.external_draft.driver = driver;
                        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
                        for (unsigned row = 0; row < v.count; ++row) { ui.row = row; dock_width(&ui, &v); }
                    }
                }
            }
        }
        ui = (chronos_ui_t){.active = 4, .highlighted = 4, .content = true};
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        chronos_screen_key(UI_ENTER, &ui, &v, &r, &s, 0);
        ui.draft[3] = 250;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "pacer");
        ui.field_edit = 5; strcpy(ui.field_text, "9999999999"); ui.cursor = 10; ui.draft[2] = 1;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "invalid-number");
        ui.field_edit = 0; ui.draft[2] = 0;
        chronos_screen_key(UI_F5, &ui, &v, &r, &s, 0);
        audit_frame(frame, &ui, &v, "leave-draft");
        ui = (chronos_ui_t){.active = 1, .highlighted = 1, .content = true};
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        chronos_screen_key(UI_ENTER, &ui, &v, &r, &s, 0);
        ui.row = 6;
        chronos_screen_key(UI_ENTER, &ui, &v, &r, &s, 0);
        for (unsigned i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
            r.alarms.runtime[i].phase = CHRONOS_ALARM_RINGING;
            snprintf(s.alarms[i].label, sizeof(s.alarms[i].label), "Long morning alarm number %u", i + 1);
        }
        r.countdown.session.state = TIMING_COMPLETE;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, "SD unavailable: changes are session-only", false);
        audit_frame(frame, &ui, &v, "ringing-while-typing");
        ui.field_edit = 1; ui.row = 1; strcpy(ui.field_text, "23:59"); ui.cursor = 4;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "alarm-time");
        ui.field_edit = 3; ui.row = 4;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "weekdays");
        ui.field_edit = 0; ui.notifications = true;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, "SD unavailable: changes are session-only", false);
        audit_frame(frame, &ui, &v, "notices");
        ui.notice_row = 4;
        audit_frame(frame, &ui, &v, "notices-scrolled");
        chronos_runtime_init(&r, &s);
        ui = (chronos_ui_t){.active = 6, .highlighted = 6, .content = true};
        for (unsigned slot = 0; slot < 3; ++slot) {
            chronos_screen_command(slot == 0 ? 23 : slot == 1 ? 65 : 21, &ui, &r, &s, 0);
            chronos_screen_command(slot == 2 ? 902 : 51, &ui, &r, &s, 0);
            ui.row = slot == 0 ? 1 : 0;
            chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
            audit_frame(frame, &ui, &v, slot == 0 ? "ntp-typing" : slot == 1 ? "sound-typing" : "search-typing");
            ui.editing = false; ui.field_edit = 0;
        }
        ui.row = 6; chronos_screen_command(63, &ui, &r, &s, 0);
        chronos_screen_command(640, &ui, &r, &s, 0);
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "brightness-typing");
        ui.row = 2; strcpy(ui.field_text, "100"); ui.cursor = 3;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "led-typing");
        ui = (chronos_ui_t){.active = 1, .content = true};
        chronos_screen_command(100, &ui, &r, &s, 0);
        ui.row = 4;
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        chronos_screen_key(UI_ENTER, &ui, &v, &r, &s, 0);
        chronos_screen_text('3', &ui); chronos_screen_text('7', &ui);
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "snooze-typing");
        chronos_screen_key(UI_ENTER, &ui, &v, &r, &s, 0);
        ui.row = 2;
        chronos_screen_key(UI_ENTER, &ui, &v, &r, &s, 0);
        chronos_screen_view(&v, &ui, &r, &s, &local, 0, NULL, true);
        audit_frame(frame, &ui, &v, "repeat-choice");
    }
}

/* Compare the complete notice raster against an unobstructed text rendering.
 * This catches descenders overwritten by a separator, at both native sizes. */
static void status_header_checks(pax_buf_t* frame) {
    pax_buf_t reference;
    pax_buf_init(&reference, NULL, 800, 40, PAX_BUF_32_8888ARGB);
    chronos_ui_t ui = {.active = 6, .highlighted = 6};
    chronos_ui_view_t v = {.year = 2026, .month = 9, .day = 23, .volume_percent = 100, .battery_valid = true};
    strcpy(v.local_time, "11:59:59 PM");
    strcpy(v.notification, "Settings saved automatically");
    const unsigned capacity[] = {0, 20, 21, 40, 41, 60, 61, 80, 81, 100};
    const unsigned expected[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4};
    for (unsigned size = 0; size < 2; ++size) {
        v.font_size = size;
        const pax_font_t* font = size ? &ibm_plex_sans_regular_16 : &ibm_plex_sans_regular_14;
        const pax_font_t* caption = size ? &ibm_plex_sans_regular_14 : &ibm_plex_sans_regular_12;
        assert(pax_text_size(caption, caption->default_size, v.local_time).x <= 148);
        assert(pax_text_size(caption, caption->default_size, "2026-09-23").x <= 128);
        assert(pax_text_size(caption, caption->default_size, "Vol +/- Muted").x <= 132);
        for (unsigned i = 0; i < sizeof(capacity) / sizeof(*capacity); ++i) {
            v.battery_percent = capacity[i];
            chronos_ui_render(frame, &ui, &v); pax_join();
            uint32_t background = pax_get_pixel(frame, 0, 0);
            for (unsigned bar = 0; bar < 4; ++bar)
                assert((pax_get_pixel(frame, 425 + bar * 8, 18) != background) == (bar < expected[i]));
        }
        pax_background(&reference, pax_get_pixel(frame, 0, 0));
        pax_draw_text(&reference, 0xffe5edf0, font, font->default_size, 20, 0, v.notification);
        pax_join();
        for (int y = 0; y < chronos_font_height(font); ++y)
            for (int x = 20; x < 760; ++x)
                if (pax_get_pixel(frame, x, 344 + y) != pax_get_pixel(&reference, x, y)) { fprintf(stderr, "Notice mismatch size=%u height=%d x=%d y=%d\n", size, chronos_font_height(font), x, y); abort(); }
        audit_frame(frame, &ui, &v, "status-date-battery");
    }
    pax_buf_destroy(&reference);
}
int main(void) {
    typography_checks();
    sync_status_fit_checks();
    pax_buf_t frame;
    pax_buf_init(&frame, NULL, 800, 480, PAX_BUF_32_8888ARGB);
    uint32_t* previous = malloc(800 * 480 * sizeof(*previous));
    bool* mutable = calloc(800 * 480, sizeof(*mutable));
    assert(previous && mutable);
    /* The splash fully replaces a prior frame and changes only the status band. */
    pax_background(&frame, 0xffff00ff);
    chronos_ui_splash(&frame, "Loading settings...");
    unsigned logo_lit = 0;
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x) {
        uint32_t pixel = pax_get_pixel(&frame, x, y);
        previous[y * 800 + x] = pixel;
        assert(pixel != 0xffff00ff);
        if (y < 424 && pixel != 0xff000000) ++logo_lit;
    }
    assert(logo_lit > 10000);
    chronos_ui_splash(&frame, "Starting network...");
    for (int y = 0; y < 440; ++y) for (int x = 0; x < 800; ++x)
        assert(previous[y * 800 + x] == pax_get_pixel(&frame, x, y));
    FILE* splash = fopen("/tmp/chronos-splash-preview.ppm", "wb");
    assert(splash);
    fprintf(splash, "P6\n800 480\n255\n");
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x) {
        uint32_t pixel = pax_get_pixel(&frame, x, y);
        unsigned char rgb[] = {pixel >> 16, pixel >> 8, pixel};
        fwrite(rgb, 1, 3, splash);
    }
    fclose(splash);
    int seconds_width = pax_text_size(CHRONOS_FONT_TIME_MEDIUM, 28, "00").x;
    int clock_width = pax_text_size(CHRONOS_FONT_CLOCK_MAIN, 72, "00:00").x;
    int clock_left = 24 + (380 - clock_width - 16 - seconds_width) / 2;
    int seconds_left = clock_left + clock_width + 16;
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 800; ++x)
            mutable[y * 800 + x] = (x >= seconds_left && x < seconds_left + seconds_width && y >= 169 && y < 206);
    chronos_ui_t ui = {0};
    chronos_ui_view_t view = {.count = 6, .volume_percent = 70};
    const char* cities[] = {"Los Angeles", "Tokyo", "London", "New York", "Sydney"};
    snprintf(view.detail[5], sizeof(view.detail[5]), "Sync now - NTP synchronized");
    view.rows[5] = (chronos_ui_action_t){UI_ACTIVATE, "Sync now", 1};
    for (unsigned theme = 0; theme < 3; ++theme) {
        view.theme = theme;
        for (unsigned format = 0; format < 2; ++format) {
            for (unsigned second = 0; second < 60; ++second) {
                snprintf(view.heading, sizeof(view.heading), "Local\t12:59:%02u%s\t2026-09-14\tDST off", second, format ? " PM" : "");
                for (unsigned city = 0; city < 5; ++city)
                    snprintf(view.detail[city], sizeof(view.detail[city]), "%s\t11:11:%02u%s\t2026-09-14\tDST on", cities[city], second, format ? " AM" : "");
                chronos_ui_render(&frame, &ui, &view);
                pax_join();
                unsigned changed = 0;
                for (int y = 0; y < 480; ++y) {
                    for (int x = 0; x < 800; ++x) {
                        unsigned i = y * 800 + x;
                        uint32_t pixel = pax_get_pixel(&frame, x, y);
                        if (second && previous[i] != pixel) {
                            if (!mutable[i]) {
                                fprintf(stderr, "Text moved outside seconds at (%d,%d), second %u, theme %u, format %u\n", x, y, second, theme, format);
                                return 1;
                            }
                            ++changed;
                        }
                        previous[i] = pixel;
                    }
                }
                assert(!second || changed);
            }
        }
    }
    /* Changing HH:MM never shifts punctuation, seconds, dates or other panels. */
    const char* times[] = {"00:00", "11:11", "18:58", "23:59"};
    for (unsigned t = 0; t < 4; ++t) {
        snprintf(view.heading, sizeof(view.heading), "Local\t%s:59 PM\t2026-09-14\tDST off", times[t]);
        chronos_ui_render(&frame, &ui, &view);
        pax_join();
        for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x) {
            uint32_t pixel = pax_get_pixel(&frame, x, y);
            bool digit_cell = x >= clock_left && x < clock_left + clock_width && y >= 124 && y < 218;
            bool colon_cell = x >= clock_left + 2 * clock_width / 5 && x < clock_left + 3 * clock_width / 5;
            if (t && (!digit_cell || colon_cell)) assert(previous[y * 800 + x] == pixel);
            previous[y * 800 + x] = pixel;
        }
    }
    /* Equal UTC offsets are ordinary status text, not a numeric subset. */
    ui.content = true; ui.row = 0; view.theme = 0;
    snprintf(view.world_difference, sizeof(view.world_difference), "Same as local");
    chronos_ui_render(&frame, &ui, &view);
    pax_join();
    unsigned same_label_pixels = 0;
    for (int y = 134; y < 153; ++y) for (int x = 612; x < 650; ++x)
        same_label_pixels += pax_get_pixel(&frame, x, y) == 0xffa6f75b;
    assert(same_label_pixels > 0);
    ui.content = false; view.world_difference[0] = 0;
    /* Cursor blink must not change the horizontal viewport for long edits. */
    chronos_ui_t editing = {.active = 6, .editing = true, .content = true};
    chronos_ui_view_t edit_view = {.count = 1, .cursor_visible = true};
    snprintf(edit_view.heading, sizeof(edit_view.heading), "Type NTP server - Enter accepts, Esc returns");
    memset(edit_view.detail[0], 'W', 145);
    edit_view.detail[0][145] = 0;
    edit_view.cursor_column = 145;
    chronos_ui_render(&frame, &editing, &edit_view);
    pax_join();
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x)
        previous[y * 800 + x] = pax_get_pixel(&frame, x, y);
    edit_view.cursor_visible = false;
    chronos_ui_render(&frame, &editing, &edit_view);
    pax_join();
    unsigned caret_changes = 0;
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x) {
        if (previous[y * 800 + x] != pax_get_pixel(&frame, x, y)) {
            assert(x == 772 && y >= 124 && y < 156);
            ++caret_changes;
        }
    }
    assert(caret_changes);
    /* Large timer values retain solid interiors and anti-aliased edge coverage. */
    snprintf(view.heading, sizeof(view.heading), "Local\t12:59:59 PM\t2026-09-14\tDST off");
    chronos_ui_view_t saved_view = view;
    view.theme = 0;
    for (unsigned tab = 2; tab <= 4; ++tab) {
        ui.active = tab;
        snprintf(view.heading, sizeof(view.heading), "%s\tRunning\t307445734561:49.5", chronos_ui_tabs[tab]);
        chronos_ui_render(&frame, &ui, &view);
        pax_join();
        unsigned lit = 0, smoothed = 0;
        for (int y = 122; y < 191; ++y) for (int x = 22; x < 776; ++x) {
            uint32_t pixel = pax_get_pixel(&frame, x, y);
            assert((pixel >> 24) == 255);
            smoothed += pixel != 0xff080d10 && pixel != 0xffe5edf0;
            lit += pixel == 0xffe5edf0;
        }
        assert(lit > 0 && smoothed > 0);
    }
    ui.active = 0;
    view = saved_view;
    /* Optional artifact uses the exact firmware renderer, not a UI approximation. */
    const char* output = getenv("CHRONOS_RENDER_PPM");
    if (output) {
        FILE* file = fopen(output, "wb");
        assert(file);
        view.theme = 0;
        snprintf(view.local_time, sizeof(view.local_time), "12:59:59 PM");
        view.font_size = getenv("CHRONOS_RENDER_LARGE") ? 1 : 0;
        ui.highlighted = 6;
        const char* scene = getenv("CHRONOS_RENDER_SCENE");
        if (scene && (!strcmp(scene, "world") || !strcmp(scene, "rtc-sync"))) {
            if (!strcmp(scene, "rtc-sync")) snprintf(view.detail[5], sizeof(view.detail[5]), "Sync now - RTC sampled into system time");
            ui.content = true; ui.row = 0;
            snprintf(view.world_difference, sizeof(view.world_difference), "-05:30 vs local");
        } else if (scene && !strcmp(scene, "timer")) {
            ui.active = 2; ui.content = true; ui.row = 0;
            snprintf(view.heading, sizeof(view.heading), "Stopwatch\tRunning\t12:34.5");
            view.count = 3;
            snprintf(view.detail[0], sizeof(view.detail[0]), "Pause");
            snprintf(view.detail[1], sizeof(view.detail[1]), "Reset");
            snprintf(view.detail[2], sizeof(view.detail[2]), "Split");
            view.rows[0] = (chronos_ui_action_t){UI_ACTIVATE, "Pause", 1};
        } else if (scene && (!strcmp(scene, "settings") || !strcmp(scene, "alarms") ||
                             !strcmp(scene, "activity") || !strcmp(scene, "notifications"))) {
            ui.active = !strcmp(scene, "settings") ? 6 : !strcmp(scene, "activity") ? 5 : 1;
            ui.content = true; ui.row = 0; view.count = 6;
            snprintf(view.heading, sizeof(view.heading), "%s", chronos_ui_tabs[ui.active]);
            const char* settings[] = {"Clock format", "Local timezone", "World cities", "NTP settings", "Alarm sound", "Theme", "Illumination", "Font size", "Save settings"};
            if (ui.active == 6) view.count = 9;
            const char* activity[] = {"Stopwatch\tRunning - 01:24", "Countdown\tIdle", "Pacer\tPaused - 00:01 period", "Alarms: 5 enabled / 0 ringing / 0 snoozed"};
            if (ui.active == 5) view.count = 4;
            for (unsigned i = 0; i < view.count; ++i) {
                if (ui.active == 6) snprintf(view.detail[i], sizeof(view.detail[i]), "%s", settings[i]);
                else if (ui.active == 5) snprintf(view.detail[i], sizeof(view.detail[i]), "%s", activity[i]);
                else snprintf(view.detail[i], sizeof(view.detail[i]), "%u. 23:59 Morning alarm 123 - Enabled", i + 1);
            }
            if (!strcmp(scene, "notifications")) {
                view.alarm_ringing = true;
                snprintf(view.notification, sizeof(view.notification), "Ringing: Café Ḍ / ไทย | Snoozed: Morning alarm | Ringing: Evening reminder | Countdown complete - Space dismisses");
                view.storage_notice = "Settings unavailable - insert an SD card";
            }
        } else if (scene && (!strcmp(scene, "illumination") || !strcmp(scene, "time-menu") || !strcmp(scene, "manual-time"))) {
            chronos_settings_t settings; chronos_runtime_t runtime;
            chronos_settings_defaults(&settings); settings.font_size = view.font_size;
            chronos_runtime_init(&runtime, &settings);
            ui.active = 6; ui.content = true;
            chronos_screen_command(!strcmp(scene, "illumination") ? 63 : !strcmp(scene, "time-menu") ? 680 : 682, &ui, &runtime, &settings, 0);
            if (!strcmp(scene, "manual-time")) { strcpy(ui.clock_server, "2026-09-23 14:30:59"); ui.cursor = 19; }
            chronos_screen_view(&view, &ui, &runtime, &settings, NULL, 0, NULL, true);
        } else if (scene && !strncmp(scene, "external", 8)) {
            chronos_settings_t settings;
            chronos_runtime_t runtime;
            chronos_settings_defaults(&settings);
            settings.theme = view.theme;
            settings.font_size = view.font_size;
            chronos_runtime_init(&runtime, &settings);
            ui.active = 6; ui.content = ui.editing = true; ui.editor_slot = 9; ui.row = 1;
            ui.external_status = EXT_VFD_READY;
            ui.external_draft = settings.external;
            ui.external_draft.enabled = strcmp(scene, "external-disabled") != 0;
            if (!strcmp(scene, "external-nokia")) {
                ui.external_draft.driver = EXT_DRIVER_NOKIA; ui.external_page = 1;
            } else if (!strcmp(scene, "external-vfd")) ui.external_page = 1;
            chronos_screen_view(&view, &ui, &runtime, &settings, NULL, 0, NULL, true);
            assert(view.count <= CHRONOS_UI_ROWS);
        } else if (scene && !strcmp(scene, "timezone")) {
            ui.active = 6; ui.content = true; ui.editing = true; ui.editor_slot = 1; ui.row = 0;
            view.count = 3;
            snprintf(view.heading, sizeof(view.heading), "Local zone - 2 matches - Left/Right pages");
            snprintf(view.detail[0], sizeof(view.detail[0]), "Search: america/argentina");
            snprintf(view.detail[1], sizeof(view.detail[1]), "[x] America/Argentina/Buenos_Aires");
            snprintf(view.detail[2], sizeof(view.detail[2]), "[ ] America/Argentina/ComodRivadavia");
            view.cursor_visible = true; view.cursor_row = 0; view.cursor_column = 25;
            view.rows[0] = (chronos_ui_action_t){UI_EDIT, "Search", 902};
        } else if (scene && (!strcmp(scene, "pacer-editor") || !strcmp(scene, "countdown-editor"))) {
            ui.active = !strcmp(scene, "pacer-editor") ? 4 : 3;
            ui.content = ui.editing = true; ui.row = 0;
            view.count = 3;
            snprintf(view.heading, sizeof(view.heading), "%s settings - F4 saves; Esc cancels", chronos_ui_tabs[ui.active]);
            snprintf(view.detail[0], sizeof(view.detail[0]), "%s: 1", ui.active == 4 ? "Seconds" : "Minutes");
            snprintf(view.detail[1], sizeof(view.detail[1]), "%s: %u", ui.active == 4 ? "Milliseconds" : "Seconds", ui.active == 4 ? 250 : 25);
            snprintf(view.detail[2], sizeof(view.detail[2]), "Save configuration");
            view.rows[0] = (chronos_ui_action_t){UI_EDIT, ui.active == 4 ? "Seconds" : "Minutes", 0};
        } else if (scene && !strcmp(scene, "editor")) {
            ui.active = 1; ui.content = true; ui.editing = true; ui.row = 6;
            snprintf(view.heading, sizeof(view.heading), "Edit label - Enter accepts, Esc cancels");
            view.count = 8;
            const char* rows[] = {"Enabled: [x]", "Time: 07:30", "Repeat: Weekdays", "Days: Enter to select [x] days", "Snooze minutes: 5", "Ring seconds: 15", "Label: Morning alarm", "Save alarm"};
            for (unsigned i = 0; i < 8; ++i) snprintf(view.detail[i], sizeof(view.detail[i]), "%s", rows[i]);
            view.cursor_visible = true; view.cursor_row = 6; view.cursor_column = 20;
            view.rows[6] = (chronos_ui_action_t){UI_APPLY, "Label", 1};
        }
        chronos_ui_render(&frame, &ui, &view);
        pax_join();
        fprintf(file, "P6\n800 480\n255\n");
        for (int y = 0; y < 480; ++y) for (int x = 0; x < 800; ++x) {
            uint32_t pixel = pax_get_pixel(&frame, x, y);
            unsigned char rgb[] = {pixel >> 16, pixel >> 8, pixel};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
        fclose(file);
    }
    layout_checks(&frame);
    footer_checks(&frame, 0);
    footer_checks(&frame, 1);
    navigation_layout_checks(&frame);
    status_header_checks(&frame);
    free(mutable);
    free(previous);
    pax_buf_destroy(&frame);
    puts("Renderer: static pixels unchanged across all seconds, themes, and clock formats");
}
