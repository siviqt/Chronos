#ifndef TIMING_CORE_H
#define TIMING_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TIMING_LAP_CAPACITY 10
#define TIMING_COUNTDOWN_MAX_US UINT64_C(3600000000)

typedef enum { TIMING_IDLE, TIMING_RUNNING, TIMING_PAUSED, TIMING_COMPLETE, TIMING_STOPPED } timing_state_t;
typedef struct {
    timing_state_t state;
    uint64_t accumulated_us;
    uint64_t anchor_us;
} timing_session_t;
typedef struct { uint64_t lap_us, total_us; } timing_lap_t;
typedef struct {
    timing_session_t session;
    uint64_t last_split_us;
    size_t lap_count;
    timing_lap_t laps[TIMING_LAP_CAPACITY];
} timing_stopwatch_t;
typedef struct { timing_session_t session; uint64_t duration_us; } timing_countdown_t;
typedef struct {
    timing_session_t session;
    uint64_t period_us;
    uint64_t delivered_beats;
} timing_pacer_t;

/* Zero-initialize objects before use. All timestamps are monotonic microseconds.
 * Call from one owner; objects allocate no memory and perform no hardware I/O.
 * Start starts a fresh session; resume only continues a paused session. */
void timing_session_start(timing_session_t *session, uint64_t now_us);
void timing_session_reset(timing_session_t *session);
void timing_session_pause(timing_session_t *session, uint64_t now_us);
void timing_session_resume(timing_session_t *session, uint64_t now_us);
uint64_t timing_session_elapsed(const timing_session_t *session, uint64_t now_us);
void timing_stopwatch_start(timing_stopwatch_t *watch, uint64_t now_us);
void timing_stopwatch_reset(timing_stopwatch_t *watch);
bool timing_stopwatch_split(timing_stopwatch_t *watch, uint64_t now_us);
bool timing_countdown_start(timing_countdown_t *timer, uint64_t duration_us, uint64_t now_us);
/* Poll before dispatching pause controls; completion is latched until reset/start. */
uint64_t timing_countdown_remaining(timing_countdown_t *timer, uint64_t now_us);
bool timing_pacer_start(timing_pacer_t *pacer, uint64_t period_us, uint64_t now_us);
/* First beat follows one period. Returns beats due since last poll, so the
 * caller can coalesce delayed beats into one signal rather than burst audio. */
uint64_t timing_pacer_poll(timing_pacer_t *pacer, uint64_t now_us);

#endif
