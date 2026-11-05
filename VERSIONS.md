# Current release

**CHRONOS 1.2.0 revision 6 — hardware accepted 2026-11-02.**

- Badge.Team BSP and MIPI DSI 2.0.0; ESP-IDF 6.0.2; Tanmatsu Wi-Fi 1.3.1;
  PAX 2.2.2. Explicit panel timeout detection, pinned LCD HAL and DMA completion
  fencing are retained.
- Hash-checked generated display fixes propagate initialization errors and release
  ID/OTP-timeout resources. Runtime SD/LDO4 ownership and hosted-slot handoff
  preserve radio-off/on and card recovery behavior.
- Canonical `com.github.siviqt.chronos` packaging includes all five WAVs and supports
  legacy package sound directories. Firmware and package metadata share one version.
- Host/render tests, fresh firmware build and artifact verification pass. The
  accepted binary is 1,620,240 bytes and leaves 23% of the application partition free.
- Hardware checks passed, as confirmed by the user. `release.json` and
  [the acceptance record](docs/ACCEPTANCE.md) identify the exact validated binary.

Only the current source and build are retained in the project working tree.
Earlier release history remains in Git.
