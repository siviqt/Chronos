# Editable SD settings

CHRONOS keeps its existing application directory, `/sd/chronos`. The new primary
configuration is `/sd/chronos/settings.ini`. Exit CHRONOS before removing the card
for editing, safely eject it from the computer, then restart CHRONOS to load edits.
Reinserting a card while CHRONOS runs keeps current session settings; it does not
reload the file. Pending session changes will be saved to the inserted card.

A first run with a writable card creates the file. Committed UI changes, including
hardware volume buttons, save after one second without further changes. Explicit
Save to SD and F1 exit flush pending changes immediately. Unchanged settings are not automatically rewritten; explicit Save to SD always
rewrites the current schema, removing legacy date keys. Failed automatic writes retry at most every 30 seconds while
storage is mounted. Without SD, changes remain usable for the session.

The format is a flat INI-style key/value file: one setting per line, with dotted
keys for city/alarm slots. There are no section headers. Blank lines and whole-line
`#` or `;` comments are accepted; inline comments are not. Keys are case-sensitive.
Keep `format=1`. Strings are double-quoted UTF-8; escape a quote as `\"` and a
backslash as `\\`. Booleans use `0`/`1`. Whitespace around keys and values is
allowed; LF and CRLF line endings are supported. Save as UTF-8 without a BOM.
Application saves rewrite the complete file, so custom comments are not preserved.

This is a valid partial configuration; omitted fields receive normal defaults:

```ini
format=1
volume_percent=45
use_24_hour=1
ntp_enabled=1
ntp_server="pool.ntp.org"
timezone="MST7MDT,M3.2.0/2,M11.1.0/2"
city_count=2
city.1.name="Europe/London"
city.1.timezone="GMT0BST,M3.5.0/1,M10.5.0/2"
city.2.name="Asia/Tokyo"
city.2.timezone="JST-9"
alarm.1.enabled=1
alarm.1.repeat=1
alarm.1.hour=7
alarm.1.minute=30
alarm.1.label="Wake up"
```

The generated file lists every editable field. Slot numbers are 1–5.

| Field | Values |
| --- | --- |
| `theme` | 0 Vector, 1 Neon District, 2 Neon Operator |
| `volume_percent` | 0–100; zero mutes playback |
| `keyboard_percent`, `display_percent` | 5–100; loaded legacy values are clamped |
| `led_percent` | 0–100, default 25; board-wide LED brightness, zero turns LEDs off |
| `ntp_enabled`, `use_24_hour`, `countdown_buzzer` | 0 or 1 |
| `wifi_enabled` | 0 or 1; absent defaults to Off (0) |
| `time_source` | 0 NTP, 1 RTC, 2 Manual; absent inferred from ntp_enabled |
| `countdown_seconds` | 1–3600 |
| `pacer_period_ms` | 1–4294967295; UI offers a narrower practical range |
| `city_count` | 0–5 |
| `city.N.name` | Up to 39 Unicode characters within 120 UTF-8 bytes |
| `timezone`, `city.N.timezone` | Explicit POSIX TZ rule, **not** an IANA identifier |
| `alarm_sound` | Plain WAV filename under `/sd/chronos`; empty selects built-in tone |
| `alarm.N.enabled` | 0 or 1 |
| `alarm.N.repeat` | 0 once, 1 daily, 2 selected weekdays |
| `alarm.N.date.year/month/day` | Legacy input only: ignored and never written |
| `alarm.N.hour/minute` | 0–23 / 0–59 |
| `alarm.N.weekdays` | Bit mask 0–127, Monday bit 0 through Sunday bit 6; nonzero for weekday repetition |
| `alarm.N.snooze_minutes` | Any integer 1–60 |
| `alarm.N.ring_seconds` | 5–60 |
| `alarm.N.label` | Up to 30 Unicode characters |

Choose IANA names in Settings > Local timezone or World cities. Enter a world
slot to open the same 598-entry SDK catalogue; Left/Right changes pages, Up/Down
selects a row, and Enter applies. Previous/Next page rows are also available.
World selections store the IANA display name and the SDK's POSIX rule. The clock
uses the existing libc DST implementation. The catalogue is a snapshot, not a
full historical IANA transition database: irregular government/lunar-calendar
changes are limited by the SDK's POSIX data. See PLATFORM.md for provenance.

Legacy alarm date keys are ignored. Brightness values within the byte grammar
are clamped to their current ranges; invalid numeric syntax is still rejected.
Other invalid, unknown, duplicate, oversized, or out-of-range fields reject the whole
file and start with defaults plus a warning. Missing fields use defaults. A bad
file is not automatically overwritten until a setting changes or Save to SD is
explicitly selected. A corrupt text file never silently falls back to older binary
settings. Preserve your original before trying to repair it.

When the text file is absent, CHRONOS loads the old `settings.bin`, including its
optional weekday/theme fields, and writes an INI equivalent. The binary file is
left untouched for downgrade/recovery; old firmware will not see subsequent INI
changes. Legacy files have no volume field, so migration starts at 70%.
The shared storage layer retains `.tmp`/`.bak` replacement and missing-primary
backup recovery for both formats. FAT cannot guarantee durability during abrupt
power loss. Runtime alarm state and timer/stopwatch history are never saved.

### Readability and timezone search

`font_size=0` selects Small (the original typography); `font_size=1` selects
Large. Missing keys in older settings files use Large; explicit saved choices are preserved. The Settings → Font size
choice applies immediately and uses the existing automatic save mechanism.
Large uses native 20px body/button and 16px secondary fonts, taller list rows,
and four world cities per view; move Down to reveal the fifth city. Clock digits
and function-key icons retain their original sizes.

In either timezone catalogue, select Search with Enter, then type to filter city,
region, full zone name, or POSIX rule. Matching ignores case and treats spaces
and underscores alike. Backspace clears text; Enter/Down moves into the results.
Esc cancels search entry first; another Esc leaves the catalogue (returning to
World cities for a city slot). Left/Right moves the caret
while typing and changes result pages otherwise. `[x]` marks the current zone;
local settings store a POSIX rule, so equivalent catalogue aliases are also marked.

Activity shows one status card per timer plus an alarm summary, without a content
selector. Idle timers omit time values; other states retain elapsed/remaining time.
While any alarm rings, the control dock shows `Any letter Snooze all ringing`
and `Space Dismiss alerts`, without replacing local navigation/typing hints.
Backspace outside field entry opens complete notifications and storage details.

## External Display

The External Display editor uses a draft: Left/Right or Enter changes values,
Apply/F4 commits; Esc returns from hardware to its parent, or cancels the outer
draft. Relevant driver and hardware controls appear
only when enabled. Committed changes use the same automatic save mechanism.

| INI key | Values / default |
| --- | --- |
| `external.enabled` | 0 disabled (default), 1 enabled |
| `external.driver` | 0 automatic (default), 1 Nokia, 2 VFD |
| `external.content` | 0 automatic, 1 Clock, 2 Alarms, 3 Stopwatch, 4 Countdown, 5 Pacer, 6 Activity, 7 World Time |
| `external.world_city` | Zero-based city slot, default 0; missing slots remain unavailable |
| `external.clk`, `external.din`, `external.ce`, `external.dc`, `external.rst` | Nokia GPIO defaults 2, 3, 15, 4, 5; unique pins from 2, 3, 4, 5, 15, 34 |
| `external.contrast` | Nokia 0–127, default 60 |
| `external.vfd_bus` | 0 CATT then internal (default), 1 CATT, 2 internal |
| `external.vfd_address` | Decimal 16–47 (0x10–0x2F), default 19 (0x13); internal 16 is rejected |
| `external.vfd_brightness` | Filament current 0–110, default 110 |

Missing keys retain disabled defaults for older configuration files. Unknown
content IDs that fit the stored byte are preserved as unavailable sources.
Syntactically valid but invalid external hardware values are retained and
reported by the external subsystem without rejecting other Chronos settings.
Hardware changes apply without restarting. See [External displays](external-display.md)
for protocol limits, status messages, validation and retry behavior.

Existing INI files and legacy binary settings without `led_percent` use 25%.
The legacy binary layout is unchanged; new saves write the LED setting to INI.

## Time-source and alarm behavior

Settings > Time groups clock format, timezone, NTP, RTC and Manual controls.
Wi-Fi is a separate persisted toggle and must be On for NTP; selecting NTP does
not implicitly enable it. RTC seeds system RAM on selection/startup. Manual
accepts local YYYY-MM-DD HH:MM:SS and never writes RTC; it requires fresh entry
after a restart. No manual timestamp is stored. System time established by NTP
continues offline rather than reverting to RTC. Once alarms disable after firing
and persist that state; snooze/ringing continue in runtime. See the
[manual](../MANUAL.md) and [acceptance](ACCEPTANCE.md) for the current release.
