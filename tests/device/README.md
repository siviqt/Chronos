# Tanmatsu device regression checks

CHRONOS 1.2.0 revision 6 passed user hardware checks on 2026-11-02. The exact
accepted firmware is recorded in [acceptance](../../docs/ACCEPTANCE.md) and
`release.json`. These checks guide future changes; they do not claim separately
recorded measurements for every case.

Install the complete `com.github.siviqt.chronos` folder through the launcher,
using `/sd/apps/com.github.siviqt.chronos/` for SD installation. Host/render tests
and compilation complement physical checks.

- Repeat cold starts, F1 return and relaunch with/without SD and Wi-Fi. Check the
  splash, icons, orientation, saved brightness and responsive startup controls.
- Traverse all seven tabs, both font sizes and all themes. Watch clock/world rows
  for stable labels and separators while seconds advance. Check long labels,
  scrolling, complete status text, battery/volume and contextual dock visibility.
- Exercise typing, held keys, Backspace, nested Escape, whole-form F4 Apply and
  F1/F5 Apply/Discard/Stay. Check typed numeric bounds, weekdays and selectors;
  drafts must not silently save or disappear.
- Run stopwatch/splits, countdown and pacer across tabs and F6 backlight changes.
  Check pause/resume/reset, completion, short pulse tails and continuous timing
  through NTP/manual clock corrections.
- Fire Once/Daily/Weekdays alarms; exercise snooze, dismiss, ring timeout,
  simultaneous alerts, one-shot persistence, midnight/DST and display wake.
- Cycle Wi-Fi Off/On during connection and synchronization, saving to SD before
  and after transitions. Check bounded NTP failure, reconnect, retained system
  time, RTC sampling and Manual time. Hardware RTC/alarm registers must not change.
- Remove/reinsert SD with pending settings, audio and network activity. Confirm
  no formatting, no stale handles, session settings retained and saving recovered.
  Check invalid/full/unwritable cards and legacy INI/binary migration separately.
- Play the five bundled WAVs from SD and internal installation, including internal
  playback without SD. User sounds take priority. Missing/corrupt/unsupported
  files must fall back audibly and recover after storage returns.
- Test volume limits, mute/unmute, speaker/headphones, quiet idle, separate pacer
  beats and stop during a beat. F1 must stop all audio and other app activity.
- Check LED A alarm priority, LED B timing priority, saved global brightness,
  explicit zero, launcher mode/brightness restoration and F6/alarm backlight policy.
- Exercise Nokia/VFD content, independent timing selection, fixed/automatic modes,
  wiring settings, CATT retry, disconnect/reconnect, keyboard activity and exit.
- Capture startup/error logs with the matching current ELF. Record board/panel and
  launcher versions. Variant-specific LCD and panel-no-response checks require
  matching hardware; report their scope explicitly.

Record future binary hashes and the user's confirmation in `release.json` and
`docs/ACCEPTANCE.md`. Never infer physical acceptance from automated tests alone.
