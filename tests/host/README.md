# CHRONOS host verification

Run `make test-host test-render` from the project root. A C11 compiler, GNU-compatible
linker and CMake are required. Fetch managed dependencies once with ESP-IDF 6.0.2
and `make configure`; subsequent host runs need no SDK activation or network.
Production sources compile against host-only SDK hooks with warnings as errors.
Those hooks are never part of the firmware include path.

Outputs use `build/release-check/host/` by default (`HOST_BUILD_DIR` can override it).
The current suites cover:

| Area | Coverage |
| --- | --- |
| Timing/alarms/signals | Monotonic boundaries, pause/resume/splits, delayed pacer polls, recurrence/DST, snooze, simultaneous alerts, LED/backlight priority. |
| Settings | INI/schema bounds, quoted text, partial/invalid files, legacy migration, FAT replacement/recovery and injected commit failures. |
| PCM/audio | Real WAV parsing/resampling, size/format failure, fallback, volume/headphones, DMA clearing, silence, bounded partial writes and complete pacer beats. |
| Storage/platform | Wrapper interception, LDO/DMA cleanup, shared-slot handoff, failed mounts, standalone behavior, hotplug, LED restoration and battery error cases. |
| Clock/network | Timezone/date/fractional offsets, 598 catalogue rules, NTP retries/cancellation, retained time and RTC/Manual selection. |
| Input/UI/startup | Duplicate BSP event filtering, repeat/typing, forms, shortcuts, draft protection, constructor-owned storage, recovered loads, sound directory priority and clean exit. |
| External output | Nokia controller/canvas, VFD protocol and borrowed buses, worker failures, coalescing, retry and shutdown. |
| BSP display | Actual generated ST7701/BSP helper code, ID/OTP timeout cleanup, original errors, retry and both panel init sequences. |
| Release tools | Missing/corrupt/stale package files, broken links, checksum coverage and unreviewed dependency rejection. |

The SD test wraps mkdir/stat rather than creating `/sd`. Settings tests wrap
rename to emulate FAT semantics. The BSP display harness uses the same
hash-checked source generator as the firmware and centralized SDK shim declarations.

`make test-render` uses real PAX and bundled IBM Plex resources. It checks all
seconds, both clock formats, themes, world rows, font sizes, timer interiors,
editor/view widths, battery stages and complete control/status layout. A preview
can be produced with:

```sh
CHRONOS_RENDER_PPM=/tmp/clock.ppm build/release-check/host/render/test_render
```

Use `CHRONOS_RENDER_SCENE=timer`, `editor`, `world` or `pacer-editor` for other
fixture views. Render fixtures are not live device state.

With ESP-IDF active, `make check-release` runs the suites and builds a fresh
firmware/package in a temporary directory, removing it afterward. The retained
accepted firmware is not overwritten. `make verify-release` checks the retained
configuration, registry pins/hashes, compiled overrides, link-map references and
partition bound. Packaging also verifies WAVs, docs, metadata and checksums.

For an optional sanitizer run:

```sh
ASAN_OPTIONS=detect_leaks=0 make test-host HOST_CFLAGS='-std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined'
```

Physical behavior is tracked separately in [acceptance](../../docs/ACCEPTANCE.md)
and the device regression checklist. The current release passed user hardware checks.
