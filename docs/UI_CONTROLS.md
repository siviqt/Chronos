# CHRONOS UI controls

The current 1.2.0 revision 6 control contract passed hardware checks on
2026-11-02; see [acceptance](ACCEPTANCE.md).

## Interaction contract

- Tabs: Left/Right highlight, Enter opens, Down opens and enters the highlighted
  tab. Activity and empty Stopwatch have no content focus.
- Lists: Up/Down selects/scrolls; Escape returns to tabs. Settings preserves
  spatial columns. Informational hardware rows are not focus stops.
- Fields: Enter opens/accepts; caret and inset outline identify typing;
  Left/Right moves the caret, Backspace deletes (previous digit for fixed HH:MM).
  Escape cancels this field, retaining other draft edits.
- Numeric fields use typed replacement, following Countdown. Direct numeric
  adjustment clamps to the permitted range. Hex address entry is labelled explicitly.
- Enter on Repeat or external discrete settings opens a choice list. Up/Down
  selects, Enter confirms and Escape cancels. Left/Right on the parent row cycles
  choices where advertised; selecting Weekdays never opens Days automatically.
- Booleans toggle only with Enter. Arrow keys never duplicate that toggle.
  Catalogue arrows change pages; brightness arrows adjust by five percentage points.
- Nested weekdays, world catalogue and external hardware return to their parent.
  Escape from the outer draft discards it. Immediate settings remain applied.
- F4 Apply commits all draft forms; valid active fields are accepted first.
  Save to SD is separate, unavailable without SD. Committed settings autosave.
- F1/F5 protect changed drafts with Apply and continue / Discard and continue /
  Stay. Stay is selected initially. Invalid application returns to the form.
- F2/F3/F4 retain timer meanings outside editors. Tab Split is advertised across
  all ordinary tabs only while Stopwatch runs. F6 and volume remain global.
- Alerts never replace navigation/typing instructions. Letters snooze all ringing
  alarms before text entry; Space acknowledges applicable alerts. Missed-only
  notices also advertise Space. Backspace outside fields opens full notices.

## Screen coverage

| Surface | Normalized behavior |
| --- | --- |
| Ready splash | Space continue, Exit app, Lights and volume instructions. |
| Clock | Entry and selection explicitly advertise comparison; Sync availability explained. |
| Alarms list | Edit selected slot; restore focus on return. |
| Alarm form/time/label/weekdays | Exact Enter verbs, direct adjustments, local cancellation, whole-form Apply. |
| Stopwatch/history | State-aware timing controls, history scrolling, global Split. |
| Countdown/Pacer main | Open Configuration; independent timing shortcuts. |
| Countdown/Pacer forms | Direct adjustments, typed replacement, explicit ranges, combined duration/interval, next-Start status. |
| Activity | Read-only cards; tab controls and eligible global Split. |
| Settings | Two spatial columns, Save to SD clearly distinct from Apply. |
| Format/Theme/Font size | Immediate Enter Apply; cancel unchosen selection. |
| Local/world catalogue/search | Accurate page/caret/result hints and nested return. |
| World cities | Typed count (0–5) and catalogue opening; confirmed changes apply immediately. |
| NTP/sound | Enter Done during typing; F4 Apply for whole form. |
| Illumination | Quick adjustment or typed brightness; F4 applies keyboard, display and LED brightness. |
| External main/Nokia/VFD | Accurate Open/Edit/Toggle/Choose/Apply/Back actions; info skipped by focus; parent return. |
| Notifications/storage | Counted/ellipsized summary plus complete scrollable detail; local controls retained. |
| Leave prompt | Three explicit choices; invalid drafts remain open. |
| Sign-off/external output | Noninteractive; no invented navigation actions. |

## Header and status layout

Status text starts at y=344 with its divider at y=370, containing the complete
26-pixel Large line box. The header shows battery beside volume, and date/time
outside Clock. Battery bands are 0–20, >20–40, >40–60, >60–80 and >80–100%;
`?` means unavailable. Read-only BSP polling runs every five seconds.


## Implementation and automated evidence

`chronos_ui_controls()` is shared by dock rendering and function-key availability.
`chronos_screen_view()` supplies runtime/storage availability and draft state.
Screen commands preserve opening focus. UI transient state is not persisted.

`make test-host` includes new regressions for spatial navigation, F1/F5's three
choices, invalid Apply during navigation, field cancellation, NTP/sound/brightness
F4, nested catalogues/hardware, cross-tab Split, and simultaneous notifications.
`make test-render` enumerates form rows/modes and checks control text widths for
both font sizes, in addition to existing theme/pixel tests. Production-renderer
previews cover Pacer, invalid numeric entry, leave prompt, alarm time/label/days,
notices and scrolling, NTP/sound/search/brightness typing, typed snooze and Repeat
choices. Pixel tests compare status text against an unobstructed raster and verify
each battery band at its boundary. Platform tests cover brightness caching/retry/
restoration and absent/invalid/failed battery readings.

The host suite, renderer suite and ESP-IDF 6.0.2 AppFS build passed for this update.
Hardware acceptance is recorded in `ACCEPTANCE.md`; device regression checks are
in `tests/device/README.md`.
