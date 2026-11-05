# Tanmatsu platform

CHRONOS 1.2.0 revision 6 uses ESP-IDF 6.0.2, Badge.Team BSP 2.0.0, MIPI DSI 2.0.0,
Tanmatsu Wi-Fi 1.3.1 and PAX 2.2.2. Hardware checks passed on 2026-11-02; see
[acceptance](ACCEPTANCE.md). The installed SDK and managed component sources are
not modified.

The owner task serializes board/input, storage, sound loading, settings, display,
illumination, LED and battery policy. Audio and network have dedicated workers;
external output has a coalescing worker. Shared NVS initialization never erases
launcher data on failure. RTC time and calibration settings are read-only.

Display setup requests RGB888 and one scanout framebuffer, with the BSP rotation
mapping the native 480×800 panel to the 800×480 UI. The application owns a separate
PAX render source and completion callback, joins PAX work and waits for DMA2D
completion before another render. Timeout retains in-flight memory until reset.
Do not substitute the BSP's no-op wait for that ownership fence.

`components/esp_hal_lcd` supplies bounded 50 ms panel reads. Board defaults enable
`CONFIG_MIPI_DSI_ABSTRACTION_USE_VENDORED_ESP_HAL_LCD=y`. The MIPI driver detects
the second panel at ID `42 00 01`, retaining the original init sequence for other
IDs. BSP startup applies the launcher's `system/lcd_vcom` value without writing it.

`tools/prepare_bsp_compat.py` checks original source SHA-256 values and generates
two corrected translation units in the build directory. The Tanmatsu display
helper returns the ST7701 initializer's real error. ID/OTP read timeout paths
release their IO/bus allocations and reset diagnostic validity before retry.
CMake replaces exactly those component sources; unexpected upstream inputs fail
configuration. [Provenance](PROVENANCE.md) records the pinned originals.

CHRONOS wraps `bsp_storage_initialize` because BSP startup otherwise reserves
LDO4, while its storage API lacks the runtime host-ownership choice and recovery
needed here. Do not mix BSP storage calls with the application backend.
The SD adapter uses slot 0, clock GPIO43, command GPIO44, data GPIO39–42, LDO4 and
an internal DMA buffer. It starts in four-bit mode at 20 MHz. Transient timeout,
CRC or response errors get one 400-kHz, one-bit retry. Formatting is disabled.
`/sd/chronos` is created only after mounting; settings use `settings.ini` there.

ESP-Hosted normally owns controller/slot 1 before `app_main`. SD settings load
using shared ownership before radio service startup. Shared detach first issues
a bounded CMD52 transaction on live slot 1, moving the ISR away from slot 0;
failed-mount cleanup follows the same path. Standalone teardown never touches
an inactive radio. IDF consumes the card/context even on final VFS removal errors,
so the application clears that handle and releases its own resources once.

Turning Wi-Fi Off stops the worker, unmounts SD, deinitializes ESP-Hosted and powers
off the radio, then remounts SD standalone. Turning it On restores hosted sharing.
NTP disable alone retains the controller. SD reinsertion preserves session
settings and pending changes rather than silently reloading an old configuration.

Internal package sounds use the existing `locfd` FAT partition at `/int`, with
matching wear-levelling parameters and no formatting. Internal storage unmounts
after audio shutdown. Lookup combines user sounds and canonical/legacy SD/internal
package directories; [sound storage](ALARM_SOUNDS.md) gives the exact priority.

The audio worker borrows the BSP I2S channel and writes 256-frame stereo blocks at
44.1 kHz. State and preloaded clip swaps use a mutex; blocking writes occur outside
it. The worker never opens files. Accepted input is signed PCM16 RIFF/WAVE,
mono/stereo at 8–48 kHz, with up to 2 MiB each for source and converted audio and
64 KiB container overhead. Resampling preserves duration/pitch. Invalid, missing,
unreadable or unallocatable clips use the generated tone.

Alarm/countdown audio has priority over pacer pulses. The pacer uses a 100 ms
ramped waveform, coalesces requests and completes the current pulse on pause/stop.
While running it keeps the channel ready with silence between beats. Otherwise,
eight silent blocks drain before shutdown. Startup clears DMA and allows 60 ms
for speaker settling. Partial/zero-progress writes resume the same block within
four attempts; persistent failure disables output and latches the error.

Volume defaults to 70%, persists in INI and changes by 5% per side-button press.
Zero holds amplifier/I2S off. BSP 2.0.0 uses MSB I2S framing and a 0.70 codec scale;
the application preserves that scale. Headphones suppress the speaker; force
speaker stays disabled. Shutdown joins the worker, disables the borrowed channel
and frees clips only after ownership is released. The channel itself is not deleted.

User LEDs A/B are BSP indices 4/5. Color channels are capped at 128/255;
persisted global brightness is 0–100%, default 25%. Initialization temporarily
uses a 10% floor, then applies the saved value including deliberate zero.
Successful writes are cached; failures retry. Exit clears A/B and restores the
launcher's original global brightness/mode. System LEDs keep automatic control.

- A: enabled green; snoozed amber at 1 Hz; missed red at 1 Hz; ringing red at
  4 Hz, with ringing > missed > snoozed > enabled priority.
- B: stopwatch cyan; countdown blue; alternating each second when both run;
  paused-only amber; 80 ms magenta pacer overlay. Completed countdown blinks blue
  at 4 Hz until acknowledged and takes priority over other B states.

F6 toggles display/keyboard backlights without pausing services. Alarms can wake
the display; configured brightness is restored rather than overwritten. Battery
information is read-only, polled every five seconds and treated as an estimate.
Missing/error/non-finite readings display unknown. Bands are 0–20, >20–40,
>40–60, >60–80 and >80–100%.

Typing, Backspace, Tab and letter/Space alarm shortcuts come from BSP text events.
Duplicate raw scancodes are ignored. Arrows, Enter, Escape, function and volume
keys use navigation events. Shift and held-key repeat follow BSP text behavior.

The network worker uses saved launcher networks and bounded polling. A connection
pass allows 15 seconds, with up to five seconds per network excluding SDK RPC
execution. It checks cancellation every 100 ms; failed offline attempts retry
after a minute. SNTP runs at startup, hourly and on request, with a 15-second
response window. Source selection samples RTC into system RAM when needed;
established system time continues offline. Manual time requires fresh entry after
restart. Neither NTP nor Manual changes hardware RTC/alarm registers.

All civil-time conversion runs in the owner and restores local TZ after world
clock formatting. The catalogue contains 598 upstream POSIX rules; it cannot
represent every irregular historical/calendar-based transition. Duration timers
use monotonic time throughout. F1 joins workers and releases application resources
before the BSP launcher restart path. Device regression checks are in
`tests/device/README.md`; host failures are injected through test-only SDK stubs.
