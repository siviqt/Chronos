# Tanmatsu platform

Application-owned adapters for board/input, display, SD/internal storage, audio,
LEDs, battery, NTP/read-only RTC and launcher return. Current dependencies are
BSP 2.0.0 and ESP-IDF 6.0.2; SDK and managed source files remain intact.

The storage initialization wrapper leaves SD/LDO4 with the application, including
runtime hosted/standalone ownership and bounded CMD52 handoff on shared detach.
The pinned HAL and explicit timeout option protect panel reads; the application
owns display DMA completion. Two hash-checked generated sources fix panel startup
error propagation and timeout cleanup.

The owner task serializes public APIs. Audio has a private I2S worker and preloads
files before playback, so worker file reads cannot race SD removal. The network
worker uses saved launcher credentials and never writes RTC. Turning Wi-Fi Off
unmounts SD before hosted teardown, powers down radio and remounts standalone;
turning On restores sharing. NTP disable alone retains the controller.

Hardware checks passed on 2026-11-02. See [platform behavior](../../docs/PLATFORM.md),
[architecture](../../ARCHITECTURE.md) and [acceptance](../../docs/ACCEPTANCE.md).
