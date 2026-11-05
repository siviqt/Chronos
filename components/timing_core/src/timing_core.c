#include "timing_core.h"
#include <string.h>

/* Saturate extremely long sessions instead of wrapping elapsed time. */
static uint64_t add_saturated(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

/* Read elapsed duration without consulting civil time or mutating state. */
uint64_t timing_session_elapsed(const timing_session_t *session, uint64_t now_us) {
    uint64_t delta = session->state == TIMING_RUNNING && now_us >= session->anchor_us
                         ? now_us - session->anchor_us : 0;
    return add_saturated(session->accumulated_us, delta);
}

/* Start a new elapsed-time session. */
void timing_session_start(timing_session_t *session, uint64_t now_us) {
    *session = (timing_session_t){.state = TIMING_RUNNING, .anchor_us = now_us};
}

/* Clear elapsed time and return to idle. */
void timing_session_reset(timing_session_t *session) {
    *session = (timing_session_t){0};
}

/* Freeze a running session; repeated pause requests are harmless. */
void timing_session_pause(timing_session_t *session, uint64_t now_us) {
    if (session->state != TIMING_RUNNING) return;
    session->accumulated_us = timing_session_elapsed(session, now_us);
    session->state = TIMING_PAUSED;
}

/* Exclude time spent paused from the elapsed duration. */
void timing_session_resume(timing_session_t *session, uint64_t now_us) {
    if (session->state != TIMING_PAUSED) return;
    session->anchor_us = now_us;
    session->state = TIMING_RUNNING;
}

/* Reset volatile lap history together with stopwatch timing. */
void timing_stopwatch_reset(timing_stopwatch_t *watch) {
    *watch = (timing_stopwatch_t){0};
}

/* Start a fresh activity with empty history. */
void timing_stopwatch_start(timing_stopwatch_t *watch, uint64_t now_us) {
    timing_stopwatch_reset(watch);
    timing_session_start(&watch->session, now_us);
}

/* Retain the latest ten segment and cumulative splits in chronological order. */
bool timing_stopwatch_split(timing_stopwatch_t *watch, uint64_t now_us) {
    if (watch->session.state != TIMING_RUNNING) return false;
    uint64_t total = timing_session_elapsed(&watch->session, now_us);
    if (total < watch->last_split_us) return false;
    if (watch->lap_count == TIMING_LAP_CAPACITY) {
        memmove(watch->laps, watch->laps + 1,
                (TIMING_LAP_CAPACITY - 1) * sizeof(watch->laps[0]));
        watch->lap_count--;
    }
    watch->laps[watch->lap_count++] = (timing_lap_t){total - watch->last_split_us, total};
    watch->last_split_us = total;
    return true;
}

/* Validate duration before replacing an existing countdown. */
bool timing_countdown_start(timing_countdown_t *timer, uint64_t duration_us, uint64_t now_us) {
    if (duration_us == 0 || duration_us > TIMING_COUNTDOWN_MAX_US) return false;
    timer->duration_us = duration_us;
    timing_session_start(&timer->session, now_us);
    return true;
}

/* Clamp at zero and latch completion for later user acknowledgement. */
uint64_t timing_countdown_remaining(timing_countdown_t *timer, uint64_t now_us) {
    if (timer->session.state == TIMING_IDLE || timer->session.state == TIMING_COMPLETE) return 0;
    uint64_t elapsed = timing_session_elapsed(&timer->session, now_us);
    if (elapsed >= timer->duration_us) {
        timer->session.accumulated_us = timer->duration_us;
        timer->session.state = TIMING_COMPLETE;
        return 0;
    }
    return timer->duration_us - elapsed;
}

/* Configure a positive period and begin a fresh cadence. */
bool timing_pacer_start(timing_pacer_t *pacer, uint64_t period_us, uint64_t now_us) {
    if (period_us == 0) return false;
    pacer->period_us = period_us;
    pacer->delivered_beats = 0;
    timing_session_start(&pacer->session, now_us);
    return true;
}

/* Derive beat deadlines from total elapsed time to avoid polling drift. */
uint64_t timing_pacer_poll(timing_pacer_t *pacer, uint64_t now_us) {
    if (pacer->session.state != TIMING_RUNNING || pacer->period_us == 0) return 0;
    uint64_t beats = timing_session_elapsed(&pacer->session, now_us) / pacer->period_us;
    if (beats <= pacer->delivered_beats) return 0;
    uint64_t due = beats - pacer->delivered_beats;
    pacer->delivered_beats = beats;
    return due;
}
