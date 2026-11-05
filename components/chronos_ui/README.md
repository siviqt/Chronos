# CHRONOS UI

The approved convention is **Option A: persistent contextual control dock**.
See [the UI contract](../../docs/UI_CONTROLS.md) and [manual](../../MANUAL.md).

`chronos_ui_controls()` describes the two contextual lines and six function-key
slots. The renderer consumes this descriptor; the screen handler uses the same
function-key availability before dispatch. Row verbs describe actual Enter
behavior (Open, Edit, Toggle, Choose, Done, Apply, Back). Read-only rows have no
Enter legend. Clock's comparison behavior is described even though Enter does
nothing on a city. All eligible tabs advertise the global stopwatch split.

## Ownership and focus

The application builds the view before each input and render. The screen layer
owns drafts, validation and effects. The portable UI layer owns tab/list focus
and control descriptions. No settings or timer services depend on render state.

Filled tabs indicate the open screen; an outline indicates keyboard focus.
Left/Right highlights, Enter opens, and Down opens/enters the highlighted tab.
Settings uses spatial column navigation. Escape cancels a field, backs out of a
nested page, cancels an outer draft, or returns content focus to tabs. Immediate
choice screens retain already-applied choices. Returning from a configuration
restores its opening row. F1/F5 prompt only when a draft differs from committed
values or has a pending numeric replacement. Stay is the initial choice.

## Layout

PAX draws the existing 800 × 480 layout using IBM Plex native rasters, the three
existing palettes and the original Tanmatsu function-key icons. No LVGL or
bitmap scaling is introduced. The content area still ends at y=342, preserving
five world cities, ten Settings entries and the large clock/timer digits.

A one-line notification summary is at y=348; summaries explicitly ellipsize.
The bottom dock begins at y=368: alert controls or contextual status at y=372,
local key controls at y=400, secondary controls at y=424, function-key slots at
y=448. Thus ordinary instructions use the final 80 pixels; alerts have their own
additional line without hiding local controls. Full notifications/storage details
are scrollable using Backspace outside a field. Backspace keeps its normal
editing meaning within fields. Escape returns from the detail view.

Selection has a dark fill, thin outline and leading marker. Typing adds an inset
outline and blinking caret. Errors use an extra line immediately beneath the
selected row; list capacity adjusts so neither the error nor later rows overlap
the notification area. Titles contain no duplicate key legends. Save to SD is
distinct from Apply; only actual storage availability enables its shortcut.

## Typography and verification

Numeric values use IBM Plex Mono; labels and descriptions use Sans. Native
font metrics drive widths, baselines and caret placement. Small/Large profiles
retain 72px clock digits, 56px timer digits and 32px function-key icons. Function
labels fall back to the secondary font when needed to fit their 94px slot.

`make test-host` tests focus, real input sequences, nested cancellation,
Apply/Discard/Stay, validation, form commits, unavailable shortcuts and alerts.
`make test-render` checks pixels and dock widths across real screens/rows/modes
in both text sizes. Optional `CHRONOS_AUDIT_PREVIEW=/existing/directory` exports
actual renderer frames for inspection. Hardware checks for 1.2.0 revision 6 passed on
2026-11-02; see [acceptance](../../docs/ACCEPTANCE.md).
