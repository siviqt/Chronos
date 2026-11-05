#include "chronos_ui.h"
#include <stdio.h>
#include <string.h>
const char* const   chronos_ui_tabs[CHRONOS_UI_TABS] = {"Clock", "Alarms",   "Stopwatch", "Countdown",
                                                        "Pacer", "Activity", "Settings"};
const unsigned chronos_ui_tab_sources[CHRONOS_UI_TABS] = {
    EXT_CLOCK, EXT_ALARMS, EXT_STOPWATCH, EXT_COUNTDOWN, EXT_PACER, EXT_ACTIVITY, 0
};
chronos_ui_action_t chronos_ui_action(const chronos_ui_t* ui, const chronos_ui_view_t* view) {
    if (!ui->content) return (chronos_ui_action_t){UI_OPEN, chronos_ui_tabs[ui->highlighted], 0};
    if (ui->row >= view->count) return (chronos_ui_action_t){UI_NONE, "", 0};
    return view->rows[ui->row];
}
int chronos_ui_key(chronos_ui_t* ui, const chronos_ui_view_t* view, chronos_ui_key_t key) {
    if (key == UI_ESCAPE) {
        ui->field_edit = 0;
        if (ui->editing) { ui->editing = false; ui->row = 0; }
        else { ui->content = false; ui->highlighted = ui->active; }
    } else if (key == UI_F5) {
        ui->field_edit = 0;
        ui->active      = (ui->active + 1) % CHRONOS_UI_TABS;
        ui->highlighted = ui->active;
        ui->content = ui->editing = false;
        ui->row                   = 0;
    } else if (!ui->content) {
        if (key == UI_LEFT) ui->highlighted = (ui->highlighted + CHRONOS_UI_TABS - 1) % CHRONOS_UI_TABS;
        if (key == UI_RIGHT) ui->highlighted = (ui->highlighted + 1) % CHRONOS_UI_TABS;
        if (key == UI_ENTER) {
            ui->active = ui->highlighted;
            ui->row    = 0;
        }
        if (key == UI_DOWN && ui->active != 5 && view->count) {
            ui->content = true;
            ui->row     = 0;
        }
    } else if (key == UI_ENTER) {
        chronos_ui_action_t action = chronos_ui_action(ui, view);
        if (action.verb == UI_OPEN || action.verb == UI_EDIT || action.verb == UI_APPLY || action.verb == UI_ACTIVATE || action.verb == UI_TOGGLE || action.verb == UI_CHOOSE || action.verb == UI_DONE || action.verb == UI_BACK) return action.command;
    } else if (key == UI_UP) {
        if (ui->row)
            --ui->row;
        else if (!ui->editing) {
            ui->content     = false;
            ui->highlighted = ui->active;
        }
    } else if (key == UI_DOWN && ui->row + 1 < view->count)
        ++ui->row;
    return 0;
}
void chronos_ui_legend(const chronos_ui_t* ui, const chronos_ui_view_t* view, char* text, size_t size) {
    static const char* const verbs[] = {"", "Open", "Edit", "Apply", "Select", "Unavailable", "Toggle", "Choose", "Done", "Back"};
    chronos_ui_action_t      action  = chronos_ui_action(ui, view);
    if (action.verb == UI_NONE)
        { if (size) text[0] = 0; }
    else
        snprintf(text, size, "Enter %s: %s", verbs[action.verb], action.target);
}

/* One control description feeds both the dock and shortcut availability. */
void chronos_ui_controls(const chronos_ui_t* ui, const chronos_ui_view_t* v, chronos_ui_controls_t* c) {
    *c = (chronos_ui_controls_t){0};
    c->function[0] = "Exit app";
    c->function[4] = "Next tab";
    c->function[5] = "Lights";
    if (ui->leave_prompt) {
        snprintf(c->primary, sizeof(c->primary), "Up/Down Select   Enter Confirm   Esc Stay");
        c->function[0] = c->function[4] = NULL;
        return;
    }
    if (ui->notifications) {
        snprintf(c->primary, sizeof(c->primary), "Up/Down Scroll   Esc Back   Bksp Back");
        return;
    }
    c->typing = ui->field_edit && ui->field_edit != 3 && ui->field_edit != 9;
    if (!ui->editing && ui->active >= 2 && ui->active <= 4) {
        if (v->timer_state == 1 || v->timer_state == 2)
            c->function[1] = v->timer_state == 2 ? "Resume" : "Pause";
        c->function[2] = "Reset";
        c->function[3] = v->timer_state == 1 || v->timer_state == 2 ? "Stop" : "Start";
    }
    bool draft = ui->editing && (ui->active == 1 || ui->active == 3 || ui->active == 4 ||
        (ui->active == 6 && (ui->editor_slot == 3 || ui->editor_slot == 5 || ui->editor_slot == 6 || ui->editor_slot == 9 || ui->editor_slot == 11)));
    if (draft) c->function[3] = "Apply";
    if (ui->active == 6 && !ui->editing && v->storage_available) c->function[3] = "Save to SD";
    if (ui->field_edit == 9) {
        snprintf(c->primary, sizeof(c->primary), "Up/Down Select   Enter Done   Esc Cancel choice");
        snprintf(c->secondary, sizeof(c->secondary), "Bksp Notices");
        return;
    }
    if (c->typing) {
        snprintf(c->primary, sizeof(c->primary), "%s   Left/Right Cursor   Bksp %s",
                 ui->field_edit == 1 ? "Type HHMM" : ui->field_edit == 8 && ui->field_command == 717 ? "Type hex" : ui->field_edit == 5 || ui->field_edit == 7 || ui->field_edit == 8 ? "Type digits" : "Type text",
                 ui->field_edit == 1 ? "Previous digit" : "Delete");
        snprintf(c->secondary, sizeof(c->secondary), "%s   Esc Cancel field",
                 ui->field_edit == 6 ? "Enter/Down Results" : "Enter Done");
        return;
    }
    if (!ui->content) {
        const char* down = ui->highlighted == 0 ? (v->city_count ? "Down Compare cities / Sync" : "Down Sync") :
            ui->highlighted == 5 ? "" : ui->highlighted == 2 ? (v->history_available ? "Down Split history" : "") : "Down Options";
        snprintf(c->primary, sizeof(c->primary), "Left/Right Tabs   Enter Open   %s", down);
    } else {
        chronos_ui_action_t a = chronos_ui_action(ui, v);
        const char* enter = a.verb == UI_EDIT ? "Enter Edit" : a.verb == UI_APPLY ? "Enter Apply" :
            a.verb == UI_TOGGLE ? "Enter Toggle" : a.verb == UI_CHOOSE ? "Enter Choose" :
            a.verb == UI_BACK ? "Enter Back" : a.verb == UI_DONE ? "Enter Done" : a.verb == UI_OPEN ? "Enter Open" :
            a.verb == UI_ACTIVATE ? (ui->active == 0 ? "Enter Sync now" : "Enter Select") : "";
        const char* back = !ui->editing ? "Esc Tabs" : ui->field_edit == 3 ? "Esc Cancel days" :
            ui->active == 6 && (ui->editor_slot == 1 || ui->editor_slot == 2 || ui->editor_slot == 7 ||
                               (ui->editor_slot == 9 && ui->external_page)) ? "Esc Back" : "Esc Cancel";
        c->pages = ui->active == 6 && ui->editing && (ui->editor_slot == 1 || ui->editor_slot == 7) && v->zone_pages > 1;
        c->adjust = ui->editing && !ui->field_edit &&
            ((ui->active == 1 && (ui->row == 2 || ui->row == 4 || ui->row == 5)) ||
             ((ui->active == 3 || ui->active == 4) && ui->row < 2) ||
             (ui->active == 6 && ((ui->editor_slot == 5 && ui->row < 3) ||
                (ui->editor_slot == 9 && (a.verb == UI_CHOOSE || a.verb == UI_EDIT)))));
        const char* horizontal = c->pages ? "Left/Right Pages   " : c->adjust ? "Left/Right Adjust   " :
            ui->active == 6 && !ui->editing ? "Left/Right Column   " : "";
        const char* vertical = ui->active == 2 && !ui->editing ? "Up/Down Scroll" : "Up/Down Select";
        if (!ui->editing && (ui->row == 0 || (ui->active == 6 && ui->row == 5)))
            vertical = "Up Tabs / Down Select";
        snprintf(c->primary, sizeof(c->primary), "%s   %s%s%s%s", vertical, horizontal, enter, *enter ? "   " : "", back);
        if (ui->active == 0) {
            snprintf(c->secondary, sizeof(c->secondary), "%s", a.verb == UI_NONE ?
                     "Selected city shows difference from local time" : a.verb == UI_UNAVAILABLE ?
                     "Sync unavailable: select NTP and enable Wi-Fi in Settings" : "");
        } else if (a.verb == UI_UNAVAILABLE) {
            snprintf(c->secondary, sizeof(c->secondary), "%s", ui->active == 6 && ui->time_menu ? "Sync requires Wi-Fi On and NTP selected/enabled" : ui->active == 6 ?
                     "Unavailable: insert an SD card" : "Choose Weekdays repeat to select days");
        }
    }
    if (!c->secondary[0]) {
        snprintf(c->secondary, sizeof(c->secondary), "%s%s%s",
                 v->split_available ? "Tab Split stopwatch   " : "",
                 "Bksp Notices   ",
                 ui->editing ? (v->draft_dirty ? "Unapplied changes" : "") : "");
    } else {
        size_t n = strlen(c->secondary);
        snprintf(c->secondary + n, sizeof(c->secondary) - n, "%s%s",
                 v->split_available ? "   Tab Split" : "", "   Bksp Notices");
    }
}
