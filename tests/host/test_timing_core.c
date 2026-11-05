#include "timing_core.h"
#include <assert.h>
#include <stdio.h>

/* Verify pause exclusion, repeat controls, saturation, and reset. */
static void test_session(void) {
    timing_session_t s = {0};
    timing_session_resume(&s, 100);
    assert(s.state == TIMING_IDLE);
    timing_session_start(&s, 100);
    assert(timing_session_elapsed(&s, 90) == 0);
    timing_session_pause(&s, 150);
    timing_session_pause(&s, 200);
    assert(timing_session_elapsed(&s, 1000) == 50);
    timing_session_resume(&s, 1000);
    timing_session_resume(&s, 1050);
    assert(timing_session_elapsed(&s, 1100) == 150);
    s.accumulated_us = UINT64_MAX - 1;
    assert(timing_session_elapsed(&s, 1100) == UINT64_MAX);
    timing_session_reset(&s);
    assert(s.state == TIMING_IDLE && timing_session_elapsed(&s, 2000) == 0);
}

/* Verify segment/cumulative splits and eviction of the oldest history. */
static void test_stopwatch(void) {
    timing_stopwatch_t w = {0};
    assert(!timing_stopwatch_split(&w, 0));
    timing_stopwatch_start(&w, 100);
    for (uint64_t i = 1; i <= 12; ++i) assert(timing_stopwatch_split(&w, 100 + i * 10));
    assert(w.lap_count == 10);
    assert(w.laps[0].total_us == 30 && w.laps[9].total_us == 120);
    for (size_t i = 0; i < w.lap_count; ++i) assert(w.laps[i].lap_us == 10);
    timing_session_pause(&w.session, 230);
    assert(!timing_stopwatch_split(&w, 500));
    timing_session_resume(&w.session, 1000);
    assert(timing_stopwatch_split(&w, 1010));
    assert(w.laps[9].lap_us == 20 && w.laps[9].total_us == 140);
    timing_stopwatch_reset(&w);
    assert(w.lap_count == 0 && w.session.state == TIMING_IDLE);
}

/* Verify concurrent sessions, late polls, countdown limits, and beat phase. */
static void test_concurrent(void) {
    timing_countdown_t c = {0};
    timing_pacer_t p = {0};
    timing_stopwatch_t w = {0};
    assert(timing_countdown_remaining(&c, 0) == 0);
    assert(!timing_countdown_start(&c, 0, 0));
    assert(!timing_countdown_start(&c, TIMING_COUNTDOWN_MAX_US + 1, 0));
    assert(timing_countdown_start(&c, TIMING_COUNTDOWN_MAX_US, 0));
    assert(timing_countdown_start(&c, 1000, 100));
    assert(!timing_countdown_start(&c, 0, 200));
    assert(c.duration_us == 1000 && c.session.anchor_us == 100);
    assert(!timing_pacer_start(&p, 0, 0));
    assert(timing_pacer_start(&p, 100, 100));
    timing_stopwatch_start(&w, 100);
    assert(timing_pacer_poll(&p, 199) == 0);
    assert(timing_pacer_poll(&p, 200) == 1);
    assert(timing_pacer_poll(&p, 450) == 2);
    assert(timing_pacer_poll(&p, 450) == 0);
    timing_session_pause(&c.session, 450);
    timing_session_pause(&p.session, 450);
    assert(timing_countdown_remaining(&c, 900) == 650);
    assert(timing_pacer_poll(&p, 900) == 0);
    timing_session_resume(&c.session, 900);
    timing_session_resume(&p.session, 900);
    assert(timing_pacer_poll(&p, 949) == 0);
    assert(timing_pacer_poll(&p, 950) == 1);
    assert(timing_countdown_remaining(&c, 1549) == 1);
    assert(timing_countdown_remaining(&c, 1550) == 0);
    assert(c.session.state == TIMING_COMPLETE);
    timing_session_resume(&c.session, 2000);
    assert(c.session.state == TIMING_COMPLETE);
    assert(timing_countdown_remaining(&c, 3000) == 0);
    assert(timing_session_elapsed(&w.session, 3000) == 2900);
    assert(timing_countdown_start(&c, 10, 3000));
    assert(timing_countdown_remaining(&c, 5000) == 0);
}

/* Run the portable timing regression suite without any SDK or device. */
int main(void) {
    test_session();
    test_stopwatch();
    test_concurrent();
    puts("timing_core: all tests passed");
    return 0;
}
