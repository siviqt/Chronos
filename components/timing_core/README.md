# timing_core

Portable C11 stopwatch, countdown, and pacer with caller-supplied monotonic
microsecond timestamps. Public API: `include/timing_core.h`.

Zero-initialize caller-owned objects and serialize access. Start/reset clears a
session; pause/resume excludes paused time. Stopwatch retains the latest ten
segment/cumulative splits. Countdown accepts 1 microsecond through 60 minutes.
Pacer reports due beats without accumulating polling drift.

See the timing API section in ARCHITECTURE.md for lifecycle and integration
contracts. Run `make test-host` from the project root. No hardware dependencies.
