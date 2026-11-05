#include "chronos_ui_render.h"
#include <string.h>
#include <stdio.h>
#include "chronos_typography.h"
#include <math.h>
/* Readability profiles reuse native rasters; clock digits and icons keep their size.
 * Add another profile here when extending the persisted font-size enum. */
static bool large_text;
#undef CHRONOS_FONT_CAPTION
#undef CHRONOS_FONT_SECONDARY
#undef CHRONOS_FONT_BODY
#undef CHRONOS_FONT_BUTTON
#undef CHRONOS_FONT_SECTION
#define CHRONOS_FONT_CAPTION (large_text ? &ibm_plex_sans_regular_14 : &ibm_plex_sans_regular_12)
#define CHRONOS_FONT_SECONDARY (large_text ? &ibm_plex_sans_regular_16 : &ibm_plex_sans_regular_14)
#define CHRONOS_FONT_BODY (large_text ? &ibm_plex_sans_medium_20 : &ibm_plex_sans_regular_16)
#define CHRONOS_FONT_BUTTON (large_text ? &ibm_plex_sans_medium_20 : &ibm_plex_sans_medium_16)
#define CHRONOS_FONT_SECTION (large_text ? &ibm_plex_sans_semibold_24 : &ibm_plex_sans_semibold_18)
extern const uint32_t chronos_function_pixels[6][1024];
typedef struct { uint32_t background, foreground, accent, selection, active, muted, edge, surface; } palette_t;
static const palette_t palettes[] = {
    {0xff080d10, 0xffe5edf0, 0xffa6f75b, 0xffa6f75b, 0xffa6f75b, 0xff9bafb7, 0xff2b3b3d, 0xff101b20},
    {0xff0c0c17, 0xffeee4ff, 0xff63e9ff, 0xffff80d6, 0xff63e9ff, 0xffacaac4, 0xff30304a, 0xff151525},
    {0xff080f16, 0xffd9eaf0, 0xffa6f75b, 0xff48dbdf, 0xff48dbdf, 0xff9bafb7, 0xff293b48, 0xff101e28}
};
static palette_t palette;
static uint32_t ink;
#define BG palette.background
#define FG palette.foreground
#define GREEN palette.accent
#define CYAN palette.selection
/* Every call uses native raster size. PAX has no widget layout engine, so
 * centering and mixed-font baselines use the generated font metrics. */
static float text_width(const pax_font_t* font, const char* value) {
    return pax_text_size(font, chronos_font_size(font), value).x;
}
static void field(pax_buf_t* b, int x, int y, int width, const pax_font_t* font,
                  uint32_t color, const char* value) {
    pax_clip(b, x, y, width, chronos_font_height(font));
    pax_draw_text(b, color, font, chronos_font_size(font), x, y, value);
    pax_noclip(b);
}
static void text(pax_buf_t* b, int x, int y, const pax_font_t* font, const char* value) {
    field(b, x, y, 780 - x, font, ink, value);
}
/* Numeric portions of structured rows use Mono; labels, filenames, city names
 * and user-entered descriptions remain Sans. Measure cursors with this same
 * routine so switching weights/size never detaches the caret. */
static float row_text(pax_buf_t* b, int x, int y, const char* value, size_t length,
                      const pax_font_t* numeric, bool numbers) {
    float offset = 0;
    int baseline = chronos_font_baseline(numeric);
    if (large_text && baseline < chronos_font_baseline(CHRONOS_FONT_BODY)) baseline = chronos_font_baseline(CHRONOS_FONT_BODY);
    bool literal = !strncmp(value, "Label:", 6) || !strncmp(value, "File:", 5) ||
                   !strncmp(value, "Server:", 7);
    size_t time_start = !strncmp(value, "Time: ", 6) ? 6 : 3;
    for (size_t i = 0; i < length;) {
        if (value[i] == '\t') { offset = ((int)offset / 190 + 1) * 190; ++i; continue; }
        size_t next = pax_utf8_seeknext(value, i);
        char glyph[8] = {0};
        if (next > length || next - i >= sizeof(glyph)) break;
        memcpy(glyph, value + i, next - i);
        bool digit = value[i] >= '0' && value[i] <= '9';
        bool punctuation = strchr(":.+-%", value[i]) &&
            ((i && value[i-1] >= '0' && value[i-1] <= '9') ||
             (value[next] >= '0' && value[next] <= '9'));
        const pax_font_t* font = CHRONOS_FONT_BODY;
        if (numbers && !literal && (digit || punctuation)) {
            if (numeric == CHRONOS_FONT_TIME_NORMAL) {
                if (i >= time_start && i < time_start + 5) font = numeric;
                else if (i < time_start) font = CHRONOS_FONT_TIME_SMALL;
            } else font = numeric;
        }
        if (b) pax_draw_text(b, ink, font, chronos_font_size(font), x + offset,
                            y + baseline - chronos_font_baseline(font), glyph);
        offset += text_width(font, glyph);
        i = next;
    }
    return offset;
}
static void columns(const char* source, char out[4][160]) {
    memset(out, 0, 4 * 160);
    for (unsigned i = 0; i < 4 && *source; ++i) {
        const char* end = strchr(source, '\t');
        size_t n = end ? (size_t)(end - source) : strlen(source);
        if (n > 159) n = 159;
        memcpy(out[i], source, n);
        if (!end) break;
        source = end + 1;
    }
}
static void selection(pax_buf_t* b, int x, int y, int width, int height) {
    pax_draw_rect(b, palette.surface, x, y, width, height);
    pax_outline_rect(b, CYAN, x, y, width, height);
    pax_draw_rect(b, CYAN, x, y, 3, height);
}
static void clock_panel(pax_buf_t* b, const chronos_ui_t* ui, const chronos_ui_view_t* view) {
    char part[4][160];
    columns(view->heading, part);
    field(b, 24, 90, 380, CHRONOS_FONT_SECTION, palette.muted, "LOCAL TIME");
    /* Center the complete clock group in its panel using fixed Mono advances. */
    int main_x = 24 + (380 - (int)text_width(CHRONOS_FONT_CLOCK_MAIN, "00:00") - 16 -
                         (int)text_width(CHRONOS_FONT_TIME_MEDIUM, "00")) / 2;
    int seconds_x = main_x + (int)text_width(CHRONOS_FONT_CLOCK_MAIN, "00:00") + 16;
    int main_y = 124;
    int seconds_y = main_y + chronos_font_baseline(CHRONOS_FONT_CLOCK_MAIN) -
                            chronos_font_baseline(CHRONOS_FONT_TIME_MEDIUM);
    if (strlen(part[1]) >= 8) {
        char hours[6];
        memcpy(hours, part[1], 5); hours[5] = 0;
        field(b, main_x, main_y, seconds_x - main_x, CHRONOS_FONT_CLOCK_MAIN, FG, hours);
        char seconds[3] = {part[1][6], part[1][7], 0};
        field(b, seconds_x, seconds_y, 86, CHRONOS_FONT_TIME_MEDIUM, GREEN, seconds);
        if (strlen(part[1]) > 8) field(b, seconds_x, seconds_y - chronos_font_height(CHRONOS_FONT_SECONDARY), 80, CHRONOS_FONT_SECONDARY, palette.muted, part[1] + 9);
        field(b, 24, 228, 230, CHRONOS_FONT_DATE, palette.muted, part[2]);
        field(b, 262, 228, 140, CHRONOS_FONT_BODY, palette.muted, part[3]);
    } else {
        field(b, main_x, main_y, 300, CHRONOS_FONT_CLOCK_MAIN, FG, "--:--");
        field(b, 24, 228, 380, CHRONOS_FONT_BODY, palette.muted, view->heading);
    }
    pax_draw_line(b, palette.edge, 420, 88, 420, 332);
    field(b, 444, 90, 330, CHRONOS_FONT_SECTION, palette.muted, "WORLD TIME");
    unsigned city = 0;
    int city_height = large_text ? 44 : 43;
    for (size_t i = 0; i < view->count; ++i) {
        bool sync = view->rows[i].verb == UI_ACTIVATE || view->rows[i].verb == UI_UNAVAILABLE;
        int y = sync ? 274 : (large_text ? 120 : 116) + city * city_height;
        int x = sync ? 20 : 438;
        int width = sync ? 384 : 340;
        if (ui->content && i == ui->row) selection(b, x, y - 3, width, sync ? 62 : city_height);
        if (sync) {
            const char* status = view->detail[i];
            if (!strncmp(status, "Sync now - ", 11)) status += 11;
            field(b, 28, y, 364, CHRONOS_FONT_BODY, GREEN, "Sync now");
            const pax_font_t* status_font = CHRONOS_FONT_SECONDARY;
            if (text_width(status_font, status) > 364) status_font = &ibm_plex_sans_regular_12;
            field(b, 28, y + 28, 364, status_font, palette.muted, status);
            continue;
        }
        columns(view->detail[i], part);
        field(b, 446, y, 154, CHRONOS_FONT_BODY, FG, part[0]);
        /* Hide seconds while retaining the optional AM/PM suffix. */
        if (strlen(part[1]) >= 8 && part[1][2] == ':' && part[1][5] == ':')
            memmove(part[1] + 5, part[1] + 8, strlen(part[1] + 8) + 1);
        char world_time[6];
        snprintf(world_time, sizeof(world_time), "%.5s", part[1]);
        /* Full 28px line height cannot share 43px rows with metadata.
         * Its ink occupies the upper line; metadata starts below its baseline. */
        int world_y = y + chronos_font_baseline(CHRONOS_FONT_BODY) - chronos_font_baseline(CHRONOS_FONT_TIME_MEDIUM);
        int metadata_y = y + city_height - chronos_font_height(CHRONOS_FONT_SECONDARY) - 2;
        field(b, 606, world_y, 90, CHRONOS_FONT_TIME_MEDIUM, FG, world_time);
        if (strlen(part[1]) > 5)
            field(b, 702, y, 74, CHRONOS_FONT_SECONDARY, FG, part[1] + 6);
        field(b, 446, metadata_y, 156, CHRONOS_FONT_SECONDARY, palette.muted, part[2]);
        bool difference = ui->content && i == ui->row && view->world_difference[0];
        if (difference && (view->world_difference[0] == '+' || view->world_difference[0] == '-')) {
            char offset[7];
            snprintf(offset, sizeof(offset), "%.6s", view->world_difference);
            field(b, 612, metadata_y, 64, CHRONOS_FONT_TIME_SMALL, GREEN, offset);
            field(b, 680, metadata_y, 94, CHRONOS_FONT_SECONDARY, GREEN, view->world_difference + 6);
        } else field(b, 612, metadata_y, 162, CHRONOS_FONT_SECONDARY, difference ? GREEN : palette.muted,
                     difference ? view->world_difference : part[3]);
        ++city;
    }
    if (!city) field(b, 446, 126, 328, CHRONOS_FONT_BODY, palette.muted, "No world cities selected");
}
void chronos_ui_render(pax_buf_t* b, const chronos_ui_t* ui, const chronos_ui_view_t* view) {
    large_text = view->font_size == 1;
    palette = palettes[view->theme < 3 ? view->theme : 0];
    ink = FG;
    pax_background(b, BG);
    field(b, 20, 7, 420, CHRONOS_FONT_SECTION, FG, "CHRONOS");
    char volume[24];
    if (view->volume_percent) snprintf(volume, sizeof(volume), "Vol +/- %u%%", view->volume_percent);
    else snprintf(volume, sizeof(volume), "Vol +/- Muted");
    field(b, 280, 7, 132, CHRONOS_FONT_CAPTION, palette.muted, volume);
    /* Five charge bands: empty, one/two/three bars, full. Unknown is explicit. */
    pax_outline_rect(b, palette.muted, 420, 11, 34, 16);
    pax_draw_rect(b, palette.muted, 454, 16, 3, 6);
    if (view->battery_valid) {
        unsigned bars = view->battery_percent > 80 ? 4 : view->battery_percent > 60 ? 3 :
                        view->battery_percent > 40 ? 2 : view->battery_percent > 20 ? 1 : 0;
        for (unsigned i = 0; i < bars; ++i) pax_draw_rect(b, palette.muted, 423 + i * 8, 14, 6, 10);
    } else field(b, 432, 7, 20, CHRONOS_FONT_CAPTION, palette.muted, "?");
    if (ui->active != 0) {
        char date[40];
        if (view->year) snprintf(date, sizeof(date), "%04u-%02u-%02u", view->year, view->month, view->day);
        else snprintf(date, sizeof(date), "---- -- --");
        field(b, 492, 7, 128, CHRONOS_FONT_CAPTION, palette.muted, date);
        field(b, 632, 7, 148, CHRONOS_FONT_CAPTION, palette.muted, view->local_time);
    }
    int tab_x = 16;
    for (unsigned i = 0; i < CHRONOS_UI_TABS; ++i) {
        int x = tab_x;
        int width = (int)ceilf(text_width(CHRONOS_FONT_BUTTON, chronos_ui_tabs[i])) + 18;
        tab_x += width + 4;
        if (i == ui->active) pax_draw_rect(b, palette.active, x, 40, width, 28);
        if (!ui->content && i == ui->highlighted) {
            pax_outline_rect(b, CYAN, x - 2, 38, width + 4, 32);
        }
        field(b, x + 8, 40 + (28 - chronos_font_height(CHRONOS_FONT_BUTTON)) / 2, width - 16, CHRONOS_FONT_BUTTON, i == ui->active ? BG : palette.muted, chronos_ui_tabs[i]);
    }
    pax_draw_line(b, palette.edge, 16, 74, 784, 74);
    if (ui->leave_prompt) {
        field(b, 24, 88, 752, CHRONOS_FONT_TITLE, GREEN, "Unapplied changes");
        field(b, 24, 128, 752, CHRONOS_FONT_BODY, FG,
              ui->leave_key == UI_F1 ? "Apply this configuration before exiting?" : "Apply this configuration before changing tabs?");
        const char* choices[] = {"Apply and continue", "Discard and continue", "Stay"};
        for (unsigned i = 0; i < 3; ++i) {
            if (i == ui->leave_choice) selection(b, 16, 176 + i * 48, 768, 42);
            field(b, 28, 182 + i * 48, 744, CHRONOS_FONT_BODY, FG, choices[i]);
        }
    } else if (ui->notifications) {
        field(b, 24, 88, 752, CHRONOS_FONT_TITLE, GREEN, "Notifications and storage");
        unsigned first = ui->notice_row;
        if (view->notice_count && first >= view->notice_count) first = view->notice_count - 1;
        if (!view->notice_count) field(b, 28, 130, 744, CHRONOS_FONT_BODY, FG, "No notifications");
        for (unsigned i = first; i < view->notice_count && i < first + 4; ++i) {
            const char* remaining = view->notices[i];
            for (unsigned line = 0; *remaining && line < 2; ++line) {
                size_t n = 0;
                while (remaining[n]) {
                    size_t next = pax_utf8_seeknext(remaining, n);
                    if (pax_text_size_adv(CHRONOS_FONT_SECONDARY, chronos_font_size(CHRONOS_FONT_SECONDARY), remaining, next,
                        PAX_ALIGN_BEGIN, PAX_ALIGN_BEGIN, -1).x0 > 740) break;
                    n = next;
                }
                pax_draw_text_adv(b, FG, CHRONOS_FONT_SECONDARY, chronos_font_size(CHRONOS_FONT_SECONDARY),
                    28, 124 + (i - first) * 52 + line * 22, remaining, n, PAX_ALIGN_BEGIN, PAX_ALIGN_BEGIN, -1);
                remaining += n;
            }
        }
        if (view->notice_count > 4) {
            char position[32]; snprintf(position, sizeof(position), "%u / %u", first + 1, view->notice_count);
            field(b, 640, 90, 138, CHRONOS_FONT_SECONDARY, palette.muted, position);
        }
    } else if (ui->active == 0 && !ui->editing) {
        clock_panel(b, ui, view);
    } else if (ui->active == 5 && !ui->editing) {
        field(b, 24, 88, 752, CHRONOS_FONT_TITLE, GREEN, "Activity");
        for (size_t i = 0; i < view->count && i < 4; ++i) {
            int x = 24 + (i % 2) * 384, y = 126 + (i / 2) * 104;
            pax_draw_rect(b, palette.surface, x, y, 368, 96);
            char part[4][160];
            columns(view->detail[i], part);
            if (i < 3) {
                field(b, x + 12, y + 8, 344, CHRONOS_FONT_SECTION, FG, part[0]);
                field(b, x + 12, y + 48, 344, CHRONOS_FONT_BODY, GREEN, part[1]);
            } else {
                field(b, x + 12, y + 8, 344, CHRONOS_FONT_SECTION, FG, "Alarms");
                const char* status = strchr(part[0], ':');
                field(b, x + 12, y + 48, 344, CHRONOS_FONT_SECONDARY, palette.muted, status ? status + 2 : part[0]);
            }
        }
    } else if (ui->active == 6 && !ui->editing) {
        field(b, 24, 88, 752, CHRONOS_FONT_TITLE, GREEN, "Settings");
        /* Column-major order keeps all ten options visible at both sizes. */
        for (size_t i = 0; i < view->count; ++i) {
            int x = 24 + (i / 5) * 384, y = 124 + (i % 5) * 43;
            if (ui->content && ui->row == i) selection(b, x - 8, y - 2, 376, 41);
            else pax_draw_line(b, palette.edge, x, y + 38, x + 360, y + 38);
            field(b, x + 4, y + 4, 352, CHRONOS_FONT_BODY, FG, view->detail[i]);
        }
    } else {
        bool timer = ui->active >= 2 && ui->active <= 4 && !ui->editing && strchr(view->heading, '\t');
        int top = timer ? 216 : 124;
        int row_height = large_text ? 42 : 36;
        size_t visible = (342 - top - (view->error[0] ? 24 : 0)) / row_height;
        if (timer) {
            char part[4][160];
            columns(view->heading, part);
            field(b, 24, 88, 300, CHRONOS_FONT_SECTION, palette.muted, part[0]);
            field(b, 590, 90, 188, CHRONOS_FONT_BUTTON, GREEN, part[1]);
            field(b, 22, 122, 754, CHRONOS_FONT_TIME_LARGE, FG, part[2]);
        } else {
            ink = GREEN;
            if (strchr(view->heading, '\t')) {
                char parts[4][160];
                columns(view->heading, parts);
                field(b, 24, 88, 100, CHRONOS_FONT_SECTION, ink, parts[0]);
                ink = GREEN;
                row_text(b, 130, 88, parts[1], strlen(parts[1]), CHRONOS_FONT_TIME_SMALL, true);
                field(b, 330, 88, 300, CHRONOS_FONT_DATE, ink, parts[2]);
            } else {
                const pax_font_t* heading_font = text_width(CHRONOS_FONT_TITLE, view->heading) <= 752 ?
                    CHRONOS_FONT_TITLE : CHRONOS_FONT_SECTION;
                if (large_text && text_width(heading_font, view->heading) > 752) heading_font = CHRONOS_FONT_SECONDARY;
                text(b, 24, 88, heading_font, view->heading);
            }
        }
        size_t first = ui->row >= visible ? ui->row - visible + 1 : 0;
        for (size_t i = first; i < view->count && i < first + visible; ++i) {
            int y = top + (i - first) * row_height + (view->error[0] && i > ui->row ? 24 : 0);
            if (ui->content && i == ui->row) selection(b, 16, y - 2, 768, row_height - 2);
            else pax_draw_line(b, palette.edge, 24, y + row_height - 5, 776, y + row_height - 5);
            ink = FG;
            bool alarm_time = ui->active == 1 && (!ui->editing || !strncmp(view->detail[i], "Time: ", 6));
            const pax_font_t* numeric = alarm_time ? CHRONOS_FONT_TIME_NORMAL : CHRONOS_FONT_TIME_SMALL;
            bool numbers = !(ui->active == 6 && ui->editing && (ui->editor_slot == 1 || ui->editor_slot == 2 || ui->editor_slot == 7));
            int row_y = y + (row_height - 4 - chronos_font_height(numeric)) / 2;
            if (ui->field_edit && ui->field_edit != 3 && ui->field_edit != 9 && i == ui->row)
                pax_outline_rect(b, GREEN, 23, y + 1, 754, row_height - 8);
            int caret = 0, scroll = 0;
            if (view->cursor_column && view->cursor_row == i) {
                size_t length = view->cursor_column;
                if (length > strlen(view->detail[i])) length = strlen(view->detail[i]);
                caret = row_text(NULL, 0, 0, view->detail[i], length, numeric, numbers);
                if (caret > 744) scroll = caret - 744;
            }
            pax_clip(b, 28, y, 748, row_height - 4);
            row_text(b, 28 - scroll, row_y, view->detail[i], strlen(view->detail[i]), numeric, numbers);
            if (view->cursor_visible && view->cursor_row == i)
                pax_draw_line(b, CYAN, 28 + caret - scroll, row_y, 28 + caret - scroll,
                              row_y + chronos_font_height(numeric) - 1);
            pax_noclip(b);
            if (view->error[0] && i == ui->row)
                field(b, 28, y + row_height - 2, 744, CHRONOS_FONT_SECONDARY, GREEN, view->error);
        }
        if (view->count > visible) {
            int track = 342 - top;
            int height = track * visible / view->count;
            int y = top + (track - height) * first / (view->count - visible);
            pax_draw_rect(b, palette.muted, 790, y, 2, height);
        }
    }
    ink = FG;
    pax_draw_line(b, palette.edge, 16, 342, 784, 342);
    /* A compact summary never displaces controls. Full notices are scrollable. */
    char summary[256];
    if (view->notice_count > 1)
        snprintf(summary, sizeof(summary), "%u notices: %s", view->notice_count, view->notices[0]);
    else snprintf(summary, sizeof(summary), "%.250s", view->notification);
    /* Explicit ellipsis marks summaries; the Notices view retains the complete text. */
    size_t summary_len = strlen(summary);
    while (summary_len && text_width(CHRONOS_FONT_SECONDARY, summary) > 750) {
        do { --summary_len; } while (summary_len && ((unsigned char)summary[summary_len] & 0xc0) == 0x80);
        snprintf(summary + summary_len, sizeof(summary) - summary_len, "...");
    }
    /* Large secondary text has a 26-pixel line box; keep the divider below it. */
    field(b, 20, 344, 760, CHRONOS_FONT_SECONDARY, FG, summary);
    pax_draw_line(b, palette.edge, 16, 370, 784, 370);
    field(b, 20, 372, 760, CHRONOS_FONT_SECONDARY, GREEN,
          view->alert_hint[0] ? view->alert_hint : view->context);
    chronos_ui_controls_t controls;
    chronos_ui_controls(ui, view, &controls);

    field(b, 20, 400, 760, CHRONOS_FONT_SECONDARY, GREEN, controls.primary);
    field(b, 20, 424, 760, CHRONOS_FONT_SECONDARY, palette.muted, controls.secondary);
    for (unsigned i = 0; i < 6; ++i) {
        if (!controls.function[i]) continue;
        int x = 16 + i * 128;
        pax_buf_t icon;
        pax_buf_init(&icon, (void*)chronos_function_pixels[i], 32, 32, PAX_BUF_32_8888ARGB);
        pax_draw_image(b, &icon, x, 448);
        pax_buf_destroy(&icon);
        const pax_font_t* font = CHRONOS_FONT_BUTTON;
        if (text_width(font, controls.function[i]) > 94) font = CHRONOS_FONT_SECONDARY;
        field(b, x + 34, 448 + (32 - chronos_font_height(font)) / 2, 94, font, palette.muted, controls.function[i]);
    }
}
