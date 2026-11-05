#ifndef CHRONOS_UI_H
#define CHRONOS_UI_H
#include <stdbool.h>
#include <stddef.h>
#include "external_display.h"
#define CHRONOS_UI_TABS 7
#define CHRONOS_UI_ROWS 13
/* Portable focus controller. The owner supplies current availability before
 * dispatch AND render. Enter returns exactly the action described by the legend. */
typedef enum {
    UI_LEFT,
    UI_RIGHT,
    UI_UP,
    UI_DOWN,
    UI_ENTER,
    UI_F5, UI_ESCAPE, UI_F2, UI_F3, UI_F4, UI_TAB, UI_F1
} chronos_ui_key_t;
typedef enum {
    UI_NONE,
    UI_OPEN,
    UI_EDIT,
    UI_APPLY,
    UI_ACTIVATE,
    UI_UNAVAILABLE, UI_TOGGLE, UI_CHOOSE, UI_DONE, UI_BACK
} chronos_ui_verb_t;
typedef struct {
    chronos_ui_verb_t verb;
    const char*       target;
    int               command;
} chronos_ui_action_t;
typedef struct {
    unsigned active, highlighted, row;
    bool     content, editing;
    /* Transient navigation state; never persisted with configuration. */
    bool exit_requested, leave_prompt, notifications;
    unsigned leave_key, leave_choice, notice_row;
    unsigned return_row;
    char text_backup[254];
    bool sound_reload_requested, sound_scan_requested;
    external_config_t external_draft;
    unsigned external_page;
    external_status_t external_status;
    bool external_retry_requested;
    bool sync_requested, clock_reconfigure_requested, settings_save_requested;
    bool time_menu, manual_requested;
    int64_t manual_seconds;
    char clock_server[254];
    unsigned editor_slot;
    unsigned zone_page, zone_city;
    char zone_search[65];
    unsigned draft[10];
    char label[121];
    unsigned field_edit, cursor, weekday_backup;
    /* Modes 8/9: shared numeric entry / choice list, with parent focus retained. */
    unsigned field_command, field_parent;
    bool field_error, battery_valid;
    unsigned battery_percent;
    char field_text[121];
} chronos_ui_t;
typedef struct {
    unsigned            theme, volume_percent, font_size;
    bool alarm_ringing, split_available, storage_available, draft_dirty;
    unsigned timer_state, city_count, zone_pages;
    bool history_available, battery_valid;
    unsigned battery_percent;
    char alert_hint[128], error[128], context[96];
    char notices[8][192];
    unsigned notice_count;
    char                local_time[24];
    char                heading[128];
    char                detail[CHRONOS_UI_ROWS][160];
    chronos_ui_action_t rows[CHRONOS_UI_ROWS];
    size_t              count;
    bool cursor_visible;
    unsigned cursor_row, cursor_column;
    unsigned year, month, day, minute_of_day;
    char                world_difference[24]; /* Selected city relative to local time. */
    char                notification[768];
    char                storage_message[192];
    const char*         storage_notice;
} chronos_ui_view_t;
typedef struct {
    char primary[192], secondary[192];
    const char* function[6];
    bool adjust, pages, typing;
} chronos_ui_controls_t;
void chronos_ui_controls(const chronos_ui_t* ui, const chronos_ui_view_t* view, chronos_ui_controls_t* controls);
extern const char* const chronos_ui_tabs[CHRONOS_UI_TABS];
/* Stable content IDs associated with functional tabs; zero excludes Settings. */
extern const unsigned chronos_ui_tab_sources[CHRONOS_UI_TABS];
chronos_ui_action_t      chronos_ui_action(const chronos_ui_t* ui, const chronos_ui_view_t* view);
int                      chronos_ui_key(chronos_ui_t* ui, const chronos_ui_view_t* view, chronos_ui_key_t key);
void chronos_ui_legend(const chronos_ui_t* ui, const chronos_ui_view_t* view, char* text, size_t size);
#endif
