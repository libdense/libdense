#ifndef DENSE_SCHED_H
#define DENSE_SCHED_H

/*
 * libdense_sched - tick budgets, timers, priority lanes, overload and
 * backpressure for high-density multiplayer servers.
 *
 * Deterministic by construction: no clocks, no rand, no threads. All
 * time is injected via now_ns / tick parameters. Hot paths do not
 * allocate after warm-up. See DESIGN.md.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DSC_SHARED) && defined(DSC_BUILD)
#define DSC_API __attribute__((visibility("default")))
#else
#define DSC_API
#endif

typedef enum dsc_result {
    DSC_OK = 0,
    /* Not an error: backpressure / budget exhausted / try later. */
    DSC_AGAIN,
    DSC_ERR_INVALID_ARGUMENT,
    DSC_ERR_OUT_OF_MEMORY,
    DSC_ERR_CAPACITY,
    DSC_ERR_NOT_FOUND,
    DSC_ERR_STATE,
} dsc_result;

DSC_API const char *dsc_result_string(dsc_result result);

typedef uint64_t dsc_tick;

/* ------------------------------------------------------------------ */
/* Timer wheel                                                        */
/* ------------------------------------------------------------------ */

typedef struct dsc_timer_wheel dsc_timer_wheel;

/* Slot+generation handle; never dangles, cancel after fire fails. */
typedef uint64_t dsc_timer_id;

typedef struct dsc_timer_wheel_config {
    /* Initial timer slab capacity (grows geometrically). Default 256. */
    size_t initial_capacity;
    /* First tick the wheel is positioned at. Advance starts here. */
    dsc_tick start_tick;
} dsc_timer_wheel_config;

typedef struct dsc_timer_wheel_stats {
    uint64_t scheduled;
    uint64_t cancelled;
    uint64_t fired;
    size_t active;
} dsc_timer_wheel_stats;

/*
 * Called once per fired timer, in deterministic order: ascending due
 * tick, then schedule order within a tick. Periodic timers are
 * rescheduled before the callback runs, so the callback may cancel
 * them with the same id.
 */
typedef void (*dsc_timer_fire_fn)(
    void *context,
    dsc_timer_id timer_id,
    uint64_t user_data,
    dsc_tick due_tick
);

DSC_API dsc_result dsc_timer_wheel_create(
    dsc_timer_wheel **out_wheel,
    const dsc_timer_wheel_config *config
);
DSC_API void dsc_timer_wheel_destroy(dsc_timer_wheel *wheel);

/*
 * Schedule a timer for due_tick (must be > the wheel's current tick).
 * period_ticks == 0: one-shot. period_ticks > 0: periodic, first fire
 * at due_tick then every period_ticks.
 */
DSC_API dsc_result dsc_timer_schedule(
    dsc_timer_wheel *wheel,
    dsc_tick due_tick,
    dsc_tick period_ticks,
    uint64_t user_data,
    dsc_timer_id *out_timer_id
);
DSC_API dsc_result dsc_timer_cancel(
    dsc_timer_wheel *wheel,
    dsc_timer_id timer_id
);

/* Fire everything due through to_tick (inclusive) and move to it. */
DSC_API dsc_result dsc_timer_advance(
    dsc_timer_wheel *wheel,
    dsc_tick to_tick,
    dsc_timer_fire_fn callback,
    void *context
);

DSC_API dsc_tick dsc_timer_current_tick(const dsc_timer_wheel *wheel);
DSC_API const dsc_timer_wheel_stats *dsc_timer_wheel_get_stats(
    const dsc_timer_wheel *wheel
);

typedef struct dsc_timer_wheel_memory_stats {
    size_t timer_capacity;
    size_t timer_slots_used;
    size_t timers_active;
} dsc_timer_wheel_memory_stats;

DSC_API dsc_result dsc_timer_wheel_get_memory_stats(
    const dsc_timer_wheel *wheel,
    dsc_timer_wheel_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Tick budget + phase accounting                                     */
/* ------------------------------------------------------------------ */

#define DSC_BUDGET_MAX_PHASES 32u

typedef struct dsc_phase_stats {
    uint64_t count;
    uint64_t last_ns;
    uint64_t ewma_ns; /* alpha = 1/8 */
    uint64_t max_ns;
} dsc_phase_stats;

typedef struct dsc_budget {
    uint64_t budget_ns;
    uint64_t tick_start_ns;
    uint64_t phase_start_ns;
    uint32_t open_phase;
    bool tick_open;
    dsc_phase_stats phases[DSC_BUDGET_MAX_PHASES];
    uint64_t phase_this_tick_ns[DSC_BUDGET_MAX_PHASES];
    uint32_t phase_mask;
    dsc_phase_stats tick;
    uint64_t overruns;
    uint64_t implicit_phase_closes;
} dsc_budget;

DSC_API dsc_result dsc_budget_init(dsc_budget *budget);
DSC_API dsc_result dsc_budget_begin_tick(
    dsc_budget *budget,
    uint64_t now_ns,
    uint64_t budget_ns
);
/* Ends the tick; returns its duration in ns (0 on misuse). */
DSC_API uint64_t dsc_budget_end_tick(dsc_budget *budget, uint64_t now_ns);
DSC_API uint64_t dsc_budget_remaining_ns(
    const dsc_budget *budget,
    uint64_t now_ns
);
DSC_API bool dsc_budget_overrun(const dsc_budget *budget, uint64_t now_ns);
DSC_API dsc_result dsc_budget_phase_begin(
    dsc_budget *budget,
    uint32_t phase,
    uint64_t now_ns
);
DSC_API dsc_result dsc_budget_phase_end(
    dsc_budget *budget,
    uint32_t phase,
    uint64_t now_ns
);
DSC_API const dsc_phase_stats *dsc_budget_phase(
    const dsc_budget *budget,
    uint32_t phase
);
DSC_API const dsc_phase_stats *dsc_budget_tick_stats(
    const dsc_budget *budget
);
DSC_API uint64_t dsc_budget_phase_this_tick_ns(
    const dsc_budget *budget,
    uint32_t phase
);
DSC_API uint32_t dsc_budget_phase_mask(const dsc_budget *budget);
DSC_API uint64_t dsc_budget_unaccounted_ns(const dsc_budget *budget);


/* ------------------------------------------------------------------ */
/* MMO tick pipeline and wall-clock policy                            */
/* ------------------------------------------------------------------ */

typedef enum dsc_server_phase {
    DSC_PHASE_INPUT = 0,
    DSC_PHASE_VALIDATE,
    DSC_PHASE_SPATIAL,
    DSC_PHASE_COMBAT,
    DSC_PHASE_AI,
    DSC_PHASE_FANOUT,
    DSC_PHASE_FLUSH,
    DSC_PHASE_COUNT,
} dsc_server_phase;

DSC_API const char *dsc_server_phase_name(dsc_server_phase phase);

typedef struct dsc_server_pipeline_config {
    uint64_t tick_budget_ns;
    uint64_t phase_budget_ns[DSC_PHASE_COUNT];
} dsc_server_pipeline_config;

typedef struct dsc_server_phase_metrics {
    uint64_t assigned_budget_ns;
    uint64_t runs;
    uint64_t last_consumed_ns;
    uint64_t ewma_consumed_ns;
    uint64_t high_water_consumed_ns;
    uint64_t last_work_completed;
    uint64_t total_work_completed;
    uint64_t high_water_work_completed;
    uint64_t last_work_deferred;
    uint64_t total_work_deferred;
    uint64_t high_water_work_deferred;
    uint64_t budget_exhaustions;
} dsc_server_phase_metrics;

typedef struct dsc_server_pipeline_stats {
    uint64_t ticks;
    uint64_t last_tick_duration_ns;
    uint64_t high_water_tick_duration_ns;
    uint64_t last_unaccounted_ns;
    uint64_t tick_overruns;
    dsc_server_phase busiest_phase;
} dsc_server_pipeline_stats;

typedef struct dsc_server_pipeline {
    dsc_server_pipeline_config config;
    dsc_budget budget;
    dsc_server_phase_metrics phases[DSC_PHASE_COUNT];
    dsc_server_pipeline_stats stats;
    uint64_t work_completed_this_tick[DSC_PHASE_COUNT];
    uint64_t work_deferred_this_tick[DSC_PHASE_COUNT];
    uint32_t next_phase;
    dsc_server_phase open_phase;
    bool tick_open;
    bool phase_open;
} dsc_server_pipeline;

/*
 * Strict named pipeline: every tick enters INPUT through FLUSH exactly once
 * and in order. The sum of configured phase budgets must not exceed the tick
 * budget; the remainder is explicit unaccounted/loop overhead.
 */
DSC_API dsc_result dsc_server_pipeline_init(
    dsc_server_pipeline *pipeline,
    const dsc_server_pipeline_config *config
);
DSC_API dsc_result dsc_server_pipeline_begin_tick(
    dsc_server_pipeline *pipeline,
    uint64_t now_ns
);
DSC_API dsc_result dsc_server_pipeline_phase_begin(
    dsc_server_pipeline *pipeline,
    dsc_server_phase phase,
    uint64_t now_ns
);
DSC_API dsc_result dsc_server_pipeline_record_work(
    dsc_server_pipeline *pipeline,
    uint64_t completed,
    uint64_t deferred
);
DSC_API uint64_t dsc_server_pipeline_phase_remaining_ns(
    const dsc_server_pipeline *pipeline,
    uint64_t now_ns
);
DSC_API dsc_result dsc_server_pipeline_phase_end(
    dsc_server_pipeline *pipeline,
    dsc_server_phase phase,
    uint64_t now_ns
);
DSC_API uint64_t dsc_server_pipeline_end_tick(
    dsc_server_pipeline *pipeline,
    uint64_t now_ns
);
DSC_API const dsc_server_phase_metrics *dsc_server_pipeline_phase_metrics(
    const dsc_server_pipeline *pipeline,
    dsc_server_phase phase
);
DSC_API const dsc_server_pipeline_stats *dsc_server_pipeline_get_stats(
    const dsc_server_pipeline *pipeline
);
DSC_API const dsc_budget *dsc_server_pipeline_budget(
    const dsc_server_pipeline *pipeline
);

typedef enum dsc_tick_clock_policy {
    /* Preserve wall-clock schedule and return up to max_catch_up_ticks. */
    DSC_TICK_CLOCK_CATCH_UP = 0,
    /* Return one tick and move the deadline forward by a bounded slew. */
    DSC_TICK_CLOCK_SLEW,
} dsc_tick_clock_policy;

typedef struct dsc_tick_clock_config {
    uint64_t tick_interval_ns;
    dsc_tick_clock_policy policy;
    /* CATCH_UP: maximum ticks returned per advance (default 4). */
    uint32_t max_catch_up_ticks;
    /* SLEW: maximum deadline correction after each late tick. */
    uint64_t max_slew_ns_per_tick;
} dsc_tick_clock_config;

typedef struct dsc_tick_clock {
    dsc_tick_clock_config config;
    dsc_tick next_tick;
    uint64_t next_deadline_ns;
    uint64_t advances;
    uint64_t stalls;
    uint64_t catch_up_ticks;
    uint64_t slewed_ns;
    uint64_t max_lateness_ns;
} dsc_tick_clock;

typedef struct dsc_tick_clock_step {
    dsc_tick first_tick;
    uint32_t ticks_due;
    uint32_t backlog_ticks;
    uint64_t lateness_ns;
    uint64_t next_deadline_ns;
    bool stalled;
} dsc_tick_clock_step;

DSC_API dsc_result dsc_tick_clock_init(
    dsc_tick_clock *clock,
    const dsc_tick_clock_config *config,
    dsc_tick start_tick,
    uint64_t start_deadline_ns
);
/* Consume the due wall-clock interval(s) and return ticks to run now. */
DSC_API dsc_result dsc_tick_clock_advance(
    dsc_tick_clock *clock,
    uint64_t now_ns,
    dsc_tick_clock_step *out_step
);

/* ------------------------------------------------------------------ */
/* Priority lanes                                                     */
/* ------------------------------------------------------------------ */

typedef enum dsc_lane_policy {
    /* Full lane rejects new work with DSC_AGAIN (backpressure). */
    DSC_LANE_REJECT = 0,
    /* Full lane drops its oldest item (counted) to admit the new one. */
    DSC_LANE_DROP_OLDEST,
} dsc_lane_policy;

typedef struct dsc_lane_config {
    /* Strict priority: lower tiers drain to exhaustion first. */
    uint32_t tier;
    /* Deficit-round-robin share within the tier (>= 1). */
    uint32_t weight;
    size_t capacity;
    dsc_lane_policy policy;
} dsc_lane_config;

typedef struct dsc_lane_stats {
    uint64_t enqueued;
    uint64_t drained;
    uint64_t rejected;
    uint64_t dropped;
    uint64_t cost_drained;
    size_t depth;
} dsc_lane_stats;

typedef struct dsc_lanes dsc_lanes;

/* Return DSC_OK to continue draining, anything else to stop early. */
typedef dsc_result (*dsc_lane_run_fn)(
    void *context,
    uint32_t lane,
    uint64_t item,
    uint64_t cost
);

DSC_API dsc_result dsc_lanes_create(
    dsc_lanes **out_lanes,
    const dsc_lane_config *configs,
    size_t lane_count
);
DSC_API void dsc_lanes_destroy(dsc_lanes *lanes);
DSC_API dsc_result dsc_lanes_enqueue(
    dsc_lanes *lanes,
    uint32_t lane,
    uint64_t item,
    uint64_t cost
);
/*
 * Drain up to cost_budget of queued work: tiers strictly in order,
 * weighted deficit round robin within a tier, FIFO within a lane.
 * Budgets bind at item boundaries (at most one item of overshoot,
 * guaranteeing progress). Returns DSC_AGAIN while work remains.
 */
DSC_API dsc_result dsc_lanes_drain(
    dsc_lanes *lanes,
    uint64_t cost_budget,
    dsc_lane_run_fn run,
    void *context
);
DSC_API size_t dsc_lanes_depth(const dsc_lanes *lanes);
DSC_API const dsc_lane_stats *dsc_lanes_lane_stats(
    const dsc_lanes *lanes,
    uint32_t lane
);

typedef struct dsc_lanes_memory_stats {
    size_t lane_count;
    size_t total_item_capacity;
    size_t total_depth;
} dsc_lanes_memory_stats;

DSC_API dsc_result dsc_lanes_get_memory_stats(
    const dsc_lanes *lanes,
    dsc_lanes_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Overload levels                                                    */
/* ------------------------------------------------------------------ */

typedef struct dsc_overload_config {
    /* Escalate after this many consecutive ticks >= enter_ns. */
    uint64_t enter_ns;
    uint32_t enter_ticks;
    /* De-escalate after this many consecutive ticks <= recover_ns. */
    uint64_t recover_ns;
    uint32_t recover_ticks;
    /* Highest shared state allowed (DSC_OVERLOAD_ELEVATED..EMERGENCY). */
    uint32_t max_level;
} dsc_overload_config;

typedef struct dsc_overload {
    dsc_overload_config config;
    uint32_t level;
    uint32_t over_streak;
    uint32_t under_streak;
    uint64_t ticks_observed;
    uint64_t over_ticks;
    uint64_t escalations;
    uint64_t recoveries;
} dsc_overload;

DSC_API dsc_result dsc_overload_init(
    dsc_overload *overload,
    const dsc_overload_config *config
);
/* Feed one tick duration; returns the (possibly new) level. */
DSC_API uint32_t dsc_overload_observe(
    dsc_overload *overload,
    uint64_t tick_duration_ns
);
DSC_API uint32_t dsc_overload_level(const dsc_overload *overload);


/* One shared state consumed by every server degradation policy. */
typedef enum dsc_overload_state {
    DSC_OVERLOAD_NORMAL = 0,
    DSC_OVERLOAD_ELEVATED,
    DSC_OVERLOAD_HIGH,
    DSC_OVERLOAD_CRITICAL,
    DSC_OVERLOAD_EMERGENCY,
    DSC_OVERLOAD_STATE_COUNT,
} dsc_overload_state;

/* Source-compatible alias retained for existing Dense 0.2 users. */
typedef dsc_overload_state dsc_server_overload;

/* Deprecated policy-specific names map onto the shared state ladder. */
#define DSC_OVERLOAD_INPUT_SHED DSC_OVERLOAD_ELEVATED
#define DSC_OVERLOAD_REPLICATION_SHED DSC_OVERLOAD_ELEVATED
#define DSC_OVERLOAD_AI_SHED DSC_OVERLOAD_HIGH
#define DSC_OVERLOAD_SOFT_REGION_CAP DSC_OVERLOAD_HIGH
#define DSC_OVERLOAD_HARD_REGION_CAP DSC_OVERLOAD_CRITICAL
#define DSC_OVERLOAD_TICK_SLOWDOWN DSC_OVERLOAD_EMERGENCY

DSC_API const char *dsc_overload_state_name(dsc_overload_state state);

/* Feed max(tick duration, wall-clock lateness) into the ladder. */
DSC_API dsc_overload_state dsc_overload_observe_pressure(
    dsc_overload *overload,
    uint64_t tick_duration_ns,
    uint64_t wall_clock_lateness_ns
);
DSC_API dsc_overload_state dsc_overload_server_state(
    const dsc_overload *overload
);

/* ------------------------------------------------------------------ */
/* Tuned overload controller                                          */
/* ------------------------------------------------------------------ */

#define DSC_OVERLOAD_TRANSITION_COUNT ((size_t)4u)

/*
 * Per-rung pressure thresholds. enter_ns[i] moves state i -> i + 1;
 * recover_ns[i] moves state i + 1 -> i. Thresholds are monotonic and
 * recover_ns[i] must be lower than enter_ns[i]. Only one rung changes per
 * observation, which keeps shedding staged and deterministic.
 */
typedef struct dsc_overload_tuning {
    uint64_t enter_ns[DSC_OVERLOAD_TRANSITION_COUNT];
    uint64_t recover_ns[DSC_OVERLOAD_TRANSITION_COUNT];
    uint32_t enter_ticks[DSC_OVERLOAD_TRANSITION_COUNT];
    uint32_t recover_ticks[DSC_OVERLOAD_TRANSITION_COUNT];
    dsc_overload_state max_state;
} dsc_overload_tuning;

typedef struct dsc_overload_controller_stats {
    dsc_overload_state state;
    uint64_t ticks_observed;
    uint64_t last_pressure_ns;
    uint64_t max_pressure_ns;
    uint64_t state_ticks[DSC_OVERLOAD_STATE_COUNT];
    uint64_t escalations[DSC_OVERLOAD_TRANSITION_COUNT];
    uint64_t recoveries[DSC_OVERLOAD_TRANSITION_COUNT];
} dsc_overload_controller_stats;

typedef struct dsc_overload_controller {
    dsc_overload_tuning tuning;
    dsc_overload_controller_stats stats;
    uint32_t enter_streak;
    uint32_t recover_streak;
} dsc_overload_controller;

/* Starter tuning derived from one measured tick CPU budget. The host should
 * retain benchmark evidence and may replace these values before release. */
DSC_API dsc_result dsc_overload_tuning_init_default(
    dsc_overload_tuning *tuning,
    uint64_t tick_budget_ns
);
DSC_API dsc_result dsc_overload_controller_init(
    dsc_overload_controller *controller,
    const dsc_overload_tuning *tuning
);
DSC_API dsc_overload_state dsc_overload_controller_observe(
    dsc_overload_controller *controller,
    uint64_t tick_duration_ns,
    uint64_t wall_clock_lateness_ns
);
DSC_API dsc_overload_state dsc_overload_controller_state(
    const dsc_overload_controller *controller
);
DSC_API const dsc_overload_controller_stats *
dsc_overload_controller_get_stats(
    const dsc_overload_controller *controller
);

/* ------------------------------------------------------------------ */
/* Token buckets                                                      */
/* ------------------------------------------------------------------ */

typedef struct dsc_token_bucket_config {
    uint64_t capacity;
    /* refill_amount tokens are added every refill_period_ns. */
    uint64_t refill_amount;
    uint64_t refill_period_ns;
    /* Tokens available at init (clamped to capacity). */
    uint64_t initial_tokens;
} dsc_token_bucket_config;

typedef struct dsc_token_bucket {
    dsc_token_bucket_config config;
    uint64_t tokens;
    uint64_t last_refill_ns;
    bool started;
    uint64_t taken;
    uint64_t denied;
} dsc_token_bucket;

DSC_API dsc_result dsc_token_bucket_init(
    dsc_token_bucket *bucket,
    const dsc_token_bucket_config *config
);
/* Refill from elapsed time, then take amount or report DSC_AGAIN. */
DSC_API dsc_result dsc_token_bucket_take(
    dsc_token_bucket *bucket,
    uint64_t now_ns,
    uint64_t amount
);
DSC_API uint64_t dsc_token_bucket_available(
    dsc_token_bucket *bucket,
    uint64_t now_ns
);
/*
 * Earliest now_ns at which take(amount) could succeed; UINT64_MAX if
 * amount exceeds capacity.
 */
DSC_API uint64_t dsc_token_bucket_next_available_ns(
    const dsc_token_bucket *bucket,
    uint64_t amount
);

/* ------------------------------------------------------------------ */
/* Family-wide allocation metrics                                     */
/* ------------------------------------------------------------------ */

typedef struct dsc_allocation_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    uint64_t growth_operations;
    size_t live_object_count;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dsc_allocation_metrics;

/*
 * Process-wide metrics for memory owned by this library. live_object_count
 * is the number of retained heap objects currently owned by the module. Growth
 * and peak counters are lifetime values. Reset starts a post-prewarm window by
 * clearing failure and steady-state allocation counters only.
 */
DSC_API void dsc_get_allocation_metrics(dsc_allocation_metrics *out_metrics);
DSC_API void dsc_reset_allocation_counters(void);

#ifdef __cplusplus
}
#endif

#endif
