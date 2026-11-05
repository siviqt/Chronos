# CHRONOS manual

CHRONOS 1.2.0 revision 6 / BSP 2.0.0 passed hardware checks on 2026-11-02.
See [the acceptance record](docs/ACCEPTANCE.md).

CHRONOS provides Clock, Alarms, Stopwatch, Countdown, Pacer, Activity and Settings
on Tanmatsu's 800 × 480 display. Timers and alarms continue when changing tabs or
turning off the backlights. Alarms work only while the application is open.

The application uses **Option A: a persistent contextual control dock**. Navigation,
editing and function-key instructions stay at the bottom. The dock changes with
focus and mode; screen titles describe content rather than repeating shortcuts.

## Installation and startup

With the installed ESP-IDF 6.0.2 environment activated, run `make package-appfs`.
Copy `build/release-check/appfs/com.github.siviqt.chronos/`, including its metadata and icons, to
`/sd/apps/com.github.siviqt.chronos/`. Replace those files together when updating; the binary alone
does not install the launcher icon.

Startup loads audio, settings, networking and time, then waits at **Press Space to
continue**. Space opens the interface. The ready splash also shows F1 Exit app,
F6 Lights and side-volume controls. Services run while waiting. F1 shows a
three-second sign-off before returning to AppFS.

## Navigation and the control dock

| Control | Meaning |
| --- | --- |
| Left / Right on tabs | Highlight a tab; the filled tab remains the currently displayed screen. |
| Enter on tabs | Open the highlighted tab. |
| Down on tabs | Open the highlighted tab and enter its content, if it has any. |
| Up / Down in lists | Select or scroll; Up from the first ordinary option returns to tabs. |
| Left / Right in Settings | Switch between the two columns. Up/Down stays within its column. |
| Enter on an option | Open, edit, toggle, open a choice list, or apply, as shown in the dock. |
| Left / Right in a configuration | Adjust only where the dock says Adjust; in catalogues, change pages. |
| Escape | Cancel the current field, return from a subpage, cancel an outer draft, or return to tabs. |
| F1 | Exit app; changed drafts offer Apply and continue, Discard and continue, or Stay. |
| F2 | Pause/Resume the displayed timing feature when available. |
| F3 | Reset the displayed timing feature outside editors. |
| F4 | Start/Stop timing; Apply an enclosing draft; or Save to SD from Settings. |
| F5 | Open the next tab; changed drafts offer the same choices as F1. |
| F6 | Toggle both backlights without pausing services. |
| Tab | Record a stopwatch split from any ordinary tab while it runs, outside editors. |
| Backspace outside field entry | Open/close the full, scrollable Notifications and storage view. |
| Side volume buttons | Change volume by 5%, shown in the header. |

The dock omits unavailable timing/storage shortcuts and does not advertise Enter
on read-only rows. The focus outline identifies the selected row; a second field
outline and caret distinguish text entry. **Unapplied changes** identifies a
changed form. F1/F5's leave prompt initially selects **Stay**. Escape also stays.
Applying an invalid draft keeps the form open and shows an error beside the
selected row. Up/Down and Enter operate the leave prompt; ordinary editing and
timing shortcuts are suspended there and in the Notifications view.

In field entry, Left/Right moves the cursor and Backspace deletes text. Alarm
HH:MM uses fixed digit positions: Backspace moves to the previous digit instead.
Enter accepts a field into its draft; it does not apply the whole form. Escape
restores that field's previous draft value and leaves the form open. F4 accepts a
valid active field and applies its enclosing form. Empty/invalid numeric fields
remain open with their allowed range displayed.

The header shows volume and a battery icon. Five charge bands show empty at
0–20%, one bar above 20%, two above 40%, three above 60%, and full above 80%.
A question mark means battery information is unavailable. The estimate refreshes
every five seconds. Outside Clock, the header also shows the current local date
and time, respecting the chosen 12/24-hour format.

## Apply versus Save to SD

**Apply** commits the configuration to the running application. Committed edits
save automatically after one second when SD is available. **Save to SD**, or F4
on the Settings root, flushes committed changes immediately; F1 also flushes them
on exit. Applying a draft is available without SD, but changes are session-only.
Save to SD is unavailable without storage and the dock explains why.

Clock format, timezone, world-city count/assignments, Theme and Font size are
immediate choices: Enter applies the selected choice. Escape returns without
undoing choices already applied. Multi-field forms (alarms, Countdown, Pacer,
NTP, sound, illumination and External Display) use explicit drafts and F4 Apply.
Escape at an outer draft discards it. Escape from a nested field or subpage only
returns one level. F1/F5 never silently discard a changed draft.

Settings reside in `/sd/chronos/settings.ini`; invalid/missing settings use defaults
with a notice. Legacy `settings.bin` migrates without overwriting the old file.
Reinserting SD retains session settings. Alarm runtime state and stopwatch history
are not persisted. See [settings reference](docs/SETTINGS.md).

## Clock, world cities and synchronization

Clock displays local HH:MM, nearby seconds and AM/PM when applicable, date/DST,
and up to five world cities. The default cities are London, New York, Tokyo,
Sydney and Sofia; local time initially uses UTC and 24-hour format.

The tab-bar dock advertises **Down Compare cities / Sync**. Up/Down selects a
city or Sync now. A selected city's DST label becomes its difference from local
time, for example `+05:30 vs local` or `Same as local`; leaving that city restores
DST status. This includes daylight-saving changes and date rollover. Enter has
no city action. On Sync now it requests synchronization; with NTP disabled the
dock directs you to enable NTP in Settings.

Settings > Clock format changes all clocks. Local timezone and World cities use
the 598-entry SDK catalogue. In World cities, Enter on Number of cities opens
numeric entry (0–5); Enter confirms it, Escape cancels. Enter on a city opens its
catalogue. Confirmed changes apply immediately.

In a catalogue, Up/Down selects and Enter applies a zone; Left/Right changes
pages. Previous/Next page rows provide the same action. Select Search and press
Enter to type. Matching ignores case and treats spaces/underscores alike.
Left/Right moves the caret while typing, Backspace deletes, and Enter or Down
moves to results. Escape first cancels search entry, restoring the previous
query; another Escape leaves the catalogue. A world-city catalogue returns to
World cities, preserving earlier applied choices. `[x]` identifies a saved zone;
equivalent local POSIX rules can match several cities. No matches is explicit.

NTP settings edits enable state and server hostname/IPv4 address. Enter finishes
server text into the draft; Apply NTP or F4 commits both fields. Escape in server
entry restores that field; Escape at the outer form discards the NTP draft.
Networking uses saved AppFS WiFi networks. NTP attempts startup and hourly sync,
retries offline connection every minute, and uses a 15-second response window.
Offline/before synchronization, time comes from the hardware RTC. CHRONOS does
not set RTC time/alarm registers. Duration timing uses a monotonic clock.

## Alarms

Select one of five slots and press Enter. Each draft offers Enabled, Time,
Repeat, Days, Snooze minutes, Ring seconds, Label and Apply alarm.

- Enter toggles Enabled; arrows do not toggle it. Enter on Repeat opens a choice
  list: Up/Down selects, Enter confirms, Escape cancels. Left/Right on the Repeat
  row provides quick adjustment without opening the Days page.
- Enter opens Time, Days, Snooze minutes, Ring seconds or Label for editing.
  Snooze and Ring use typed replacement, just like Countdown numeric fields;
  Left/Right on those rows provides bounded one-unit adjustment.
- Time accepts four digits, such as 0735 for 07:35; the colon is automatic.
- Labels support up to 30 characters in stored settings; keyboard entry uses
  the supported printable input characters, with cursor movement and deletion.
- Once means the next occurrence of the entered local time. Daily repeats every
  day. After choosing Weekdays, navigate to **Days** and press Enter to open
  Monday–Sunday checkboxes; Enter toggles, Done accepts,
  and Escape restores the day selection from before opening the subpage.
  At least one day is required. F4 can apply the whole alarm from this subpage.
- Snooze accepts any whole number from 1–60 minutes. Ring duration is 5–60 seconds.
- Apply alarm or F4 commits; completing a label/time field alone never enables
  or applies an alarm. Saving Once requires an available local clock.

Ringing controls occupy their own line in the bottom dock, leaving navigation
and typing hints visible. **Any letter snoozes all ringing alarms**, taking
priority over text entry. **Space dismisses alerts**, including concurrent
countdown completion and missed notices. With only missed/completed notices,
the dock advertises Space Clear. Alert labels/status remain visible in the
notification summary; Backspace outside field entry opens complete details,
including simultaneous alarms and storage messages. Up/Down scrolls those
notices and Escape returns to the previous screen without losing its draft.

New alarms and snooze expiry wake the display even with backlights off. A saved
display brightness of zero uses 80% for alarm wake-up. Keyboard illumination is
unaffected by alarm wake-up.

## Stopwatch, Countdown and Pacer

F2 Pause/Resume, F3 Reset and F4 Start/Stop control the displayed timer. Stop
freezes its time; Pause/Resume continues the same session. Start begins a fresh
session. Reset restores the idle state. Sessions continue across tabs.

Stopwatch keeps the latest ten splits, newest first, with segment and cumulative
times. Up/Down scrolls history. Tab records a split while Stopwatch runs, even
from another tab; the dock advertises it wherever eligible. Splits remain until
Reset or a fresh Start. Tab Split is absent while stopped/paused or in an editor.

Countdown and Pacer have a Configuration row beneath the timer. Enter opens the
draft. Left/Right adjusts a selected number by one without wrapping; Enter opens
an empty replacement field for typing. Enter Done accepts the number locally,
Escape cancels only that field, and F4/Apply configuration commits the draft.

Countdown has Minutes 0–60, Seconds 0–59 and a Completion sound toggle. Combined
duration must be 00:01–60:00. Applying to idle/stopped/completed Countdown updates
its displayed duration; running/paused sessions keep their current duration and
use the new setting at the next Start. Completion can sound for 15 seconds;
Space acknowledges it and resets the completed display.

Pacer has Seconds 0–4294967 and Milliseconds 0–999, with a combined range of
1–4294967295 ms. For example, 1 and 250 produce 1.250 seconds. The form shows the
combined interval and that it takes effect at the next Start. Its 100 ms ramped
beep respects volume/mute/headphones; rapid beats coalesce.

Activity provides read-only Stopwatch, Countdown, Pacer and alarm summaries.
There are no selectable cards; use tabs or F5 to open a feature.

## Other settings and hardware

Settings uses two columns of five entries. Left: Clock format, Local timezone,
World cities, NTP settings, Alarm sound. Right: Theme, Illumination, Font size,
External Display, Save to SD (always last). Up/Down stays within each column; Left/Right
switches columns. Returning from configuration restores its opening row.

Theme offers Vector, Neon District and Neon Operator. Font size offers Small and
Large (default). Enter applies a choice immediately, retaining the same controls
across themes/sizes. Both sizes display all five cities and all Settings entries.

Illumination edits keyboard 0–50%, display 0–100% and LEDs 0–100% (default 25%).
Left/Right adjusts by 5%;
Enter opens numeric entry for the selected value. Apply illumination/F4 commits
all three values. Escape in the field cancels it; Escape at the form cancels the
draft. LED brightness is the board-wide brightness, including automatic system
LEDs. Zero turns LEDs off; F6 continues to control only the backlights. The
launcher’s LED brightness is restored on exit.

Alarm sound selects a plain filename, searched first in `/sd/chronos`, then
`/sd/apps/com.github.siviqt.chronos`, then `/int/apps/com.github.siviqt.chronos`.
The picker combines these folders and works without SD for internally installed
sounds. Empty uses built-in audio. See [sound storage details](docs/ALARM_SOUNDS.md).
Enter finishes the filename; Apply sound/F4 commits it. WAV support is signed
16-bit PCM, 44.1 kHz, mono/stereo, up to 2 MiB audio data. Missing or invalid audio
falls back to the built-in tone. Side-volume changes persist automatically;
volume zero mutes and headphones suppress the speaker.

External Display configures optional Nokia 5110/PCD8544 or NE-HCS12SS59T-R1 VFD
output. Enter toggles support, opens a choice list for driver/content/bus/GPIO/
world-city settings, or opens numeric entry for contrast/address/brightness.
I2C addresses accept hexadecimal digits (without a `0x` prefix). Left/Right
quickly adjusts the advertised fields; it does not toggle support. Hardware
configuration opens a subpage. Status and wiring explanations are readable but not focus stops.
Escape from hardware returns to its parent draft. Apply/F4 commits the complete
external configuration; Apply and Detect / Retry also requests detection/retry.
Escape from the outer form cancels. See [external displays](docs/external-display.md)
for wiring, constraints and automatic content priority. External displays are
read-only; they have no separate navigation controls.

LED A reports alarm state; LED B reports timing state and pacer pulses. System
LEDs retain their automatic behavior. F1 stops application services and restores
LED configuration before returning to AppFS. See [platform details](docs/PLATFORM.md).

## Verification

Host/render checks, the fresh firmware/package build and user hardware acceptance
pass for 1.2.0 revision 6. See [acceptance](docs/ACCEPTANCE.md).
