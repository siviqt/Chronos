# CHRONOS architecture

Current release: **1.2.0 revision 6**, BSP 2.0.0 / ESP-IDF 6.0.2, accepted on
hardware by the user on 2026-11-02. The exact firmware is recorded in
`release.json` and [the acceptance record](docs/ACCEPTANCE.md).

The application uses ESP-IDF component discovery under `components/`.

| Component | Responsibility |
| --- | --- |
| `chronos_app` | Application owner loop, settings policy, alarms, clock/timezones, screen commands and external-display selection. |
| `timing_core` | Portable monotonic stopwatch, countdown and pacer state. |
| `settings_store` | Caller-supplied schema/directory persistence and recoverable FAT replacement. |
| `chronos_ui` | Portable focus/actions, editor state, IBM Plex typography and PAX rendering. |
| `tanmatsu_platform` | Board/input, display, storage, audio, clock/network, LEDs, battery and launcher return. |
| `external_display` | Portable formatting, latest-frame worker, Nokia/VFD adapters and transport ownership. |
| `nokia5110` | Vendored native controller, GPIO transport, canvas/font and controller tests. |
| `esp_hal_lcd` | Pinned LCD HAL with bounded panel reads. |

`main` invokes `chronos_app`. Portable components do not depend on application
UI policy. Registered source lists in component CMake files define the firmware;
SDK stubs and test hooks are confined to host tests.

The application task owns settings, timing sessions, civil-time conversion,
input handling and presentation. A loop iteration drains at most 16 input events,
updates actions after each event and services timers independently of focus.
Civil time is read once per second. Periodic rendering is limited to about 10 Hz;
navigation requests immediate presentation and unchanged snapshots skip rendering.
Large view snapshots use static storage rather than task-stack copies.

The seven tabs are Clock, Alarms, Stopwatch, Countdown, Pacer, Activity and
Settings. The controller's action descriptor supplies both dock text and key
availability. The screen layer owns drafts and commands; UI transient state is
never persisted. F1/F5 protect changed drafts with Apply, Discard or Stay.
See [the UI contract](docs/UI_CONTROLS.md) and [manual](MANUAL.md).

Stopwatch/countdown/pacer durations, snooze and ring deadlines use monotonic
microseconds. Wall-clock corrections affect civil alarm scheduling without
changing elapsed sessions. One-shot alarms derive their next occurrence from
local time and persist their disabled state after firing. Alarm output has
priority over countdown and cadence audio. The runtime combines LED, audio and
backlight policy without depending on the active tab.

Settings are stored at `/sd/chronos/settings.ini` using format 1. Committed changes
save after a one-second debounce; explicit Save and exit flush them. Drafts remain
separate. The store writes temporary/backup files because FAT rename does not
replace an existing target. Legacy binary settings remain readable for migration;
new saves use INI. This compatibility code is part of the current release.
See [settings](docs/SETTINGS.md) for the complete schema and validation rules.

Startup initializes shared NVS without erasing it on error, initializes the BSP,
then presents the splash while independent audio, LED and storage services start.
SD settings load before network startup. Internal `locfd` FAT is mounted for
package sounds without formatting. Source selection seeds system RAM from RTC,
uses NTP when enabled, or accepts session-only Manual time. CHRONOS never writes
hardware RTC/alarm registers or launcher calibration settings.

The display adapter requests RGB888 and one scanout framebuffer, creates a PAX
source buffer and applies the BSP's native rotation. It owns the DMA completion
callback and waits before allowing another render. A timeout blocks reuse/free of
an in-flight source. The pinned HAL bounds panel reads to 50 ms, and
`CONFIG_MIPI_DSI_ABSTRACTION_USE_VENDORED_ESP_HAL_LCD=y` enables driver detection.
Two source-hash-checked generated translation units propagate panel initialization
errors and release ID/OTP-timeout IO/bus resources. SDK/managed sources are intact.

The application wraps `bsp_storage_initialize` so the BSP does not reserve its
SD/LDO4 resources. Hosted slot 1 normally exists before `app_main`; slot 0 mounts
with shared ownership. Shared detach performs a bounded CMD52 handoff before
removing slot 0, including failed-mount cleanup. Turning Wi-Fi Off stops the
worker, unmounts SD, deinitializes hosted transport and powers off the radio,
then remounts standalone SD. Turning Wi-Fi On restores hosted sharing. NTP disable
alone leaves the controller alive. Existing card contents are never formatted.

The network worker uses saved launcher credentials and bounded connection/SNTP
attempts. Established system time continues offline. Timezone conversion is
serialized by the owner and restores local TZ after formatting world cities.
The vendored catalogue contains 598 POSIX rules; it is not a historical tzdata
transition database.

The audio worker borrows the BSP I2S channel and writes bounded stereo blocks.
The owner preloads files; the worker never accesses storage. WAV input is PCM16,
mono/stereo, 8–48 kHz, resampled to 44.1 kHz with separate 2 MiB source/output limits.
Missing or invalid files select generated audio. The five search directories cover
user sounds and canonical/legacy SD/internal packages. See
[sound storage](docs/ALARM_SOUNDS.md). Mute/exit disable output, pacer gaps remain
ready until stop, and partial writes resume within bounded retries.

LED A/B use indices 4/5; global mode/brightness are restored on exit. Battery
status is read-only and an estimate. F6 controls backlights; duration sessions
continue, and an alarm can wake the display. External-display updates use a
coalescing worker with bounded retry; CATT and primary BSP buses are borrowed,
not deleted. See [platform behavior](docs/PLATFORM.md) and
[external displays](docs/external-display.md).

F1 shows the sign-off, stops services, joins audio/network/external workers,
restores LEDs, unmounts storage, shuts down presentation and uses the BSP launcher
restart path. Resources still owned by a timed-out worker/DMA transfer remain
allocated until reset rather than being freed underneath that activity.

`build/release-check/` is the single current firmware build directory. Host test
outputs live beneath its `host/` directory. `make check-release` runs host/render
checks and a temporary fresh firmware/package build, then removes that temporary
build without replacing the accepted binary. The packager checks dependencies,
configuration, linked fixes, image size, assets, documentation and checksums.
Hardware acceptance applies only when the firmware hash/version/revision match
`release.json`; new binaries are marked pending.

Metadata in `assets/icons/metadata.json` supplies both firmware and package
versioning. `make package-appfs` creates the complete local package;
`make sync-app-repository APP_REPOSITORY_DIR=...` exports the 14 repository files:
firmware, metadata, three icons, five WAVs, two user guides and two license notices.
Build records and individual license copies remain in the local build package.
Current asset generation inputs, generated resources, tests and pinned dependencies
are retained. [Provenance](docs/PROVENANCE.md) records their source and licenses.
