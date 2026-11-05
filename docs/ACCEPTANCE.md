# CHRONOS acceptance

**CHRONOS 1.2.0 revision 6 passed hardware checks**, confirmed by the user on
2026-11-02. This is the current accepted release.

| Item | Accepted release |
| --- | --- |
| Target | Tanmatsu / ESP32-P4 |
| ESP-IDF | 6.0.2 |
| Badge.Team BSP / MIPI DSI | 2.0.0 / 2.0.0 |
| Tanmatsu Wi-Fi / PAX | 1.3.1 / 2.2.2 |
| Firmware bytes | 1,620,240 |
| Free application partition space | 476,912 bytes (23%) |
| Firmware SHA-256 | `d7237e25a4ea1329f5ef3fb62592d82a9283956712e24be880a84d64be3e6494` |

The user's overall hardware confirmation is the acceptance evidence. Separate
panel-variant, fault-injection, electrical and latency measurements were not
reported. Automated tests are recorded separately from that physical acceptance.

Host tests, real-PAX renderer tests and a clean ESP-IDF firmware/package build
pass. Checks cover panel ID/OTP timeout cleanup and error propagation, storage
interception and controller handoff, CATT ownership, input, settings, alarms,
clock/network, audio, UI and package integrity. Dependency hashes, required
configuration, compiled compatibility sources and link-map references are checked.

The accepted build is `build/release-check/`; its package is
`build/release-check/appfs/com.github.siviqt.chronos/`. The app-repository folder
contains the verified repository payload. `release.json` identifies the exact
accepted binary; `BUILD_INFO.json` and `SHA256SUMS` describe the local package.

Cleanup retains this binary unchanged and removes prior builds, previews,
backups, experimental outputs and superseded reports. Current source, tests,
asset-generation inputs, license notices and dependency pins remain available.
For subsequent changes, use the device regression checklist in
`tests/device/README.md` and repeat acceptance for the resulting binary.
