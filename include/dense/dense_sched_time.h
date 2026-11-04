#ifndef DENSE_SCHED_TIME_H
#define DENSE_SCHED_TIME_H

/* Explicit Time Bending / Time Dilation. No clocks, allocation or threads.
 * All time values are nanoseconds. See docs/time-dilation.md for authority,
 * replication, backlog and network-service responsibilities. */
#include "dense_sched.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSC_TIME_SCALE_ONE 1000000u

typedef struct dsc_time_config {
    /* Fixed simulation step, 1..UINT64_MAX / DSC_TIME_SCALE_ONE. */
    uint64_t step_ns;
    /* Maximum unprocessed real-time gap. Exceeding it rejects the poll. */
    uint64_t max_backlog_ns;
    /* Integer parts per million: 1..DSC_TIME_SCALE_ONE, no pause/speedup. */
    uint32_t initial_scale_ppm;
    uint32_t min_scale_ppm;
    /* Maximum change at each completed tick, 1..DSC_TIME_SCALE_ONE. */
    uint32_t max_scale_change_ppm;
} dsc_time_config;

typedef struct dsc_time_origin {
    uint64_t epoch;
    uint64_t tick;
    uint64_t real_ns;
    uint64_t simulation_ns;
} dsc_time_origin;

/* A scale target becomes active immediately AFTER completing effective_tick.
 * Its first bounded ramp step sets the rate for the following interval. */
typedef struct dsc_time_control {
    uint64_t epoch;
    uint64_t effective_tick;
    uint32_t target_scale_ppm;
} dsc_time_control;

/* Caller-owned storage. Initialize through the API; do not edit its fields,
 * serialize its native layout, or access it concurrently. */
typedef struct dsc_time_clock {
    dsc_time_config config;
    dsc_time_origin anchor;
    uint64_t observed_real_ns;
    uint64_t phase_units;
    uint32_t scale_ppm;
    uint32_t target_scale_ppm;
    dsc_time_control pending;
    bool change_pending;
    bool initialized;
} dsc_time_clock;

typedef struct dsc_time_step {
    dsc_time_origin end;
    uint64_t dt_ns;
    uint32_t scale_ppm;
    uint32_t next_scale_ppm;
    bool control_applied;
    bool scale_changed;
} dsc_time_step;

typedef struct dsc_time_status {
    dsc_time_origin anchor;
    uint64_t observed_real_ns;
    /* Simulation phase in 1 / DSC_TIME_SCALE_ONE nanoseconds, below one step.
     * anchor.real_ns is the processed real-time cursor, which can be between
     * ticks. anchor.simulation_ns and anchor.tick count completed steps only. */
    uint64_t phase_units;
    uint64_t next_deadline_ns;
    uint32_t scale_ppm;
    uint32_t target_scale_ppm;
    dsc_time_control pending;
    bool change_pending;
    bool can_step;
} dsc_time_status;

/* 20 Hz, q=1, minimum q=0.1, at most 0.1 scale change per tick, 1 s backlog. */
DSC_API void dsc_time_config_defaults(dsc_time_config *config);
/* Nonzero epoch; initial tick/time and at least one deadline must fit uint64.
 * Init and every rejecting operation leave both state and output unchanged. */
DSC_API dsc_result dsc_time_init(dsc_time_clock *clock,
    const dsc_time_config *config, const dsc_time_origin *origin);
/* One pending control. Require matching epoch and a future effective tick.
 * DSC_AGAIN means a control is already pending; it is not replaced. */
DSC_API dsc_result dsc_time_schedule(dsc_time_clock *clock,
    const dsc_time_control *control);
/* Consume at most ONE fixed simulation step. DSC_OK supplies that step;
 * DSC_AGAIN means no step yet and leaves out_step unchanged (time/phase advance).
 * Calls must use nondecreasing real_now_ns, including while draining backlog.
 * DSC_ERR_CAPACITY rejects excessive backlog or exhausted time/tick counters.
 * Output objects must not alias clock storage. Cap calls per service iteration
 * in the application, and continue network/input maintenance on real time. */
DSC_API dsc_result dsc_time_poll(dsc_time_clock *clock, uint64_t real_now_ns,
    dsc_time_step *out_step);
DSC_API dsc_result dsc_time_get_status(const dsc_time_clock *clock,
    dsc_time_status *out_status);
/* Restore an authoritative field-by-field snapshot with the same configuration.
 * Validates scale, phase, pending control, backlog and derived deadline. This
 * intentionally replaces the whole timeline; authentication and stale-snapshot
 * rejection belong to the application. Native struct bytes are not a wire format. */
DSC_API dsc_result dsc_time_restore(dsc_time_clock *clock,
    const dsc_time_config *config, const dsc_time_status *status);
/* Explicit discontinuity: require a larger epoch and nondecreasing real time.
 * Preserve completed tick/simulation time/current scale. Discard fractional
 * phase and elapsed backlog, cancel pending controls and stop the ramp.
 * Publish a new authoritative baseline before clients use the new timeline. */
DSC_API dsc_result dsc_time_rebase(dsc_time_clock *clock, uint64_t new_epoch,
    uint64_t real_now_ns);

#ifdef __cplusplus
}
#endif
#endif
