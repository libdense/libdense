#ifndef DENSE_COLLISION_H
#define DENSE_COLLISION_H

/*
 * libdense_collision - authoritative collision queries, sweeps,
 * triggers, and movement validation for high-density multiplayer
 * servers.
 *
 * All geometry is integer / fixed point; there are no floats in this
 * library. Identical inputs produce identical results on every
 * platform. See DESIGN.md for the full contract.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DC_SHARED) && defined(DC_BUILD)
#define DC_API __attribute__((visibility("default")))
#else
#define DC_API
#endif

typedef enum dc_result {
    DC_OK = 0,
    DC_ERR_INVALID_ARGUMENT,
    DC_ERR_OUT_OF_MEMORY,
    DC_ERR_CAPACITY,
    DC_ERR_NOT_FOUND,
    DC_ERR_EXISTS,
    /* Coordinate, delta, or radius outside the documented limits. */
    DC_ERR_RANGE,
} dc_result;

DC_API const char *dc_result_string(dc_result result);

/* ------------------------------------------------------------------ */
/* Coordinates and limits                                             */
/* ------------------------------------------------------------------ */

typedef int32_t dc_coord;

typedef struct dc_vec2 {
    dc_coord x;
    dc_coord y;
} dc_vec2;

/* Hard input limits (validated; DC_ERR_RANGE beyond them). */
#define DC_COORD_MAX ((dc_coord)(1 << 28))
#define DC_MOVE_DELTA_MAX ((dc_coord)(1 << 26))
#define DC_RADIUS_MAX ((dc_coord)(1 << 20))

/* Sweep times are Q16.16: 0 .. DC_T_ONE == the full motion. */
#define DC_T_ONE ((uint32_t)65536u)

typedef uint64_t dc_static_id;
typedef uint64_t dc_trigger_id;
/* Bodies are keyed by game-owned 64-bit entity ids. */
typedef uint64_t dc_body_id;

/* Trigger memberships at or below this count stay inside each body slot. */
#define DC_BODY_INLINE_TRIGGERS ((size_t)8u)

typedef uint32_t dc_layer_mask;

/* ------------------------------------------------------------------ */
/* World                                                              */
/* ------------------------------------------------------------------ */

typedef struct dc_world dc_world;

typedef struct dc_world_config {
    /* Broadphase cell edge length in coordinate units (> 0). */
    dc_coord cell_size;
    size_t initial_static_capacity;
    size_t initial_body_capacity;
    size_t initial_trigger_capacity;
} dc_world_config;

typedef struct dc_world_stats {
    size_t statics;
    size_t bodies;
    size_t triggers;
    uint64_t queries;
    uint64_t sweeps;
    uint64_t moves_validated;
    uint64_t moves_blocked;
    uint64_t move_batches;
    uint64_t move_requests_deferred;
    uint64_t trigger_events;
} dc_world_stats;

DC_API dc_result dc_world_create(
    dc_world **out_world,
    const dc_world_config *config
);
DC_API void dc_world_destroy(dc_world *world);
DC_API const dc_world_stats *dc_world_get_stats(const dc_world *world);


typedef struct dc_world_memory_stats {
    size_t allocated_bytes;
    size_t occupied_cells;
    size_t cell_map_capacity;
    size_t membership_nodes_live;
    size_t membership_node_capacity;
    size_t static_capacity;
    size_t body_capacity;
    size_t trigger_capacity;
    size_t body_inline_trigger_memberships;
    size_t body_overflow_trigger_memberships;
    size_t body_trigger_overflow_capacity;
    size_t body_trigger_overflow_bytes;
} dc_world_memory_stats;

/* Snapshot of sparse broadphase and slab allocation sizes. */
DC_API dc_result dc_world_get_memory_stats(
    const dc_world *world,
    dc_world_memory_stats *out_stats
);
/* Number of broadphase cells touched by an inclusive AABB. */
DC_API dc_result dc_world_estimate_aabb_cells(
    dc_coord cell_size,
    dc_vec2 min,
    dc_vec2 max,
    uint64_t *out_cells
);

/* ------------------------------------------------------------------ */
/* Static solids (axis-aligned boxes)                                 */
/* ------------------------------------------------------------------ */

DC_API dc_result dc_static_add_aabb(
    dc_world *world,
    dc_vec2 min,
    dc_vec2 max,
    dc_layer_mask layers,
    uint64_t user_data,
    dc_static_id *out_id
);
DC_API dc_result dc_static_remove(dc_world *world, dc_static_id id);

/* ------------------------------------------------------------------ */
/* Dynamic bodies (circles, game-owned ids)                           */
/* ------------------------------------------------------------------ */

DC_API dc_result dc_body_add(
    dc_world *world,
    dc_body_id body_id,
    dc_vec2 center,
    dc_coord radius,
    dc_layer_mask layers,
    uint64_t user_data
);
DC_API dc_result dc_body_remove(dc_world *world, dc_body_id body_id);
/*
 * Commit a new position (teleport semantics: no sweep). Updates the
 * broadphase and emits trigger enter/leave events for the tick.
 */
DC_API dc_result dc_body_set_position(
    dc_world *world,
    dc_body_id body_id,
    dc_vec2 center
);
DC_API dc_result dc_body_get(
    const dc_world *world,
    dc_body_id body_id,
    dc_vec2 *out_center,
    dc_coord *out_radius
);

/* ------------------------------------------------------------------ */
/* Overlap queries                                                    */
/* ------------------------------------------------------------------ */

typedef enum dc_hit_kind {
    DC_HIT_STATIC = 0,
    DC_HIT_BODY,
} dc_hit_kind;

typedef struct dc_overlap {
    dc_hit_kind kind;
    uint64_t id;        /* dc_static_id or dc_body_id */
    uint64_t user_data;
} dc_overlap;

/* Return false to stop the query early. */
typedef bool (*dc_overlap_fn)(void *context, const dc_overlap *overlap);

DC_API dc_result dc_query_point(
    dc_world *world,
    dc_vec2 point,
    dc_layer_mask layers,
    dc_overlap_fn callback,
    void *context
);
DC_API dc_result dc_query_circle(
    dc_world *world,
    dc_vec2 center,
    dc_coord radius,
    dc_layer_mask layers,
    dc_overlap_fn callback,
    void *context
);
DC_API dc_result dc_query_aabb(
    dc_world *world,
    dc_vec2 min,
    dc_vec2 max,
    dc_layer_mask layers,
    dc_overlap_fn callback,
    void *context
);

/* Batch overlap requests. Each processed request keeps its own callback and
 * context; input order is authoritative and work_budget bounds requests. */
typedef struct dc_query_circle_request {
    dc_vec2 center;
    dc_coord radius;
    dc_layer_mask layers;
    dc_overlap_fn callback;
    void *context;
} dc_query_circle_request;

typedef struct dc_query_many_result {
    dc_result status;
    size_t overlap_count;
} dc_query_many_result;

DC_API uint32_t dc_query_circle_many(
    dc_world *world,
    const dc_query_circle_request *requests,
    dc_query_many_result *results,
    uint32_t count,
    uint32_t work_budget
);

/* Pure exact circle-vs-AABB batch kernel. Production uses the scalar
 * reference; accelerated candidates must clear the representative gate. */
typedef struct dc_circle_aabb_test {
    dc_vec2 center;
    dc_coord radius;
    dc_vec2 min;
    dc_vec2 max;
} dc_circle_aabb_test;

DC_API dc_result dc_circle_aabb_overlap_many(
    const dc_circle_aabb_test *tests,
    bool *out_overlaps,
    size_t count
);

/* ------------------------------------------------------------------ */
/* Raycast and swept circle                                           */
/* ------------------------------------------------------------------ */

typedef struct dc_hit {
    bool hit;
    /* Q16.16 time of impact along the motion, 0..DC_T_ONE. */
    uint32_t t;
    dc_vec2 position;   /* center position at impact (backed off) */
    /* Face hits: axis unit normal. Corner/body hits: un-normalized
     * radial vector from the struck point/center toward the mover. */
    dc_vec2 normal;
    dc_hit_kind kind;
    uint64_t id;
    uint64_t user_data;
} dc_hit;

DC_API dc_result dc_raycast(
    dc_world *world,
    dc_vec2 from,
    dc_vec2 to,
    dc_layer_mask layers,
    dc_hit *out_hit
);

typedef struct dc_raycast_request {
    dc_vec2 from;
    dc_vec2 to;
    dc_layer_mask layers;
} dc_raycast_request;

typedef struct dc_raycast_many_result {
    dc_result status;
    dc_hit hit;
} dc_raycast_many_result;

DC_API uint32_t dc_raycast_many(
    dc_world *world,
    const dc_raycast_request *requests,
    dc_raycast_many_result *results,
    uint32_t count,
    uint32_t work_budget
);
/*
 * Sweep a circle of the given radius from -> to. exclude_body skips
 * one body id (the mover); pass 0 when not applicable (0 is never a
 * valid body id to register).
 */
DC_API dc_result dc_sweep_circle(
    dc_world *world,
    dc_vec2 from,
    dc_vec2 to,
    dc_coord radius,
    dc_layer_mask layers,
    dc_body_id exclude_body,
    dc_hit *out_hit
);

/* ------------------------------------------------------------------ */
/* Movement validation                                                */
/* ------------------------------------------------------------------ */

#define DC_MOVE_SLIDE ((uint32_t)0x1u)
/* Also collide against bodies whose layers intersect the mask. */
#define DC_MOVE_BLOCK_ON_BODIES ((uint32_t)0x2u)
/* Commit the final position into the world (updates triggers). */
#define DC_MOVE_COMMIT ((uint32_t)0x4u)

typedef struct dc_move_result {
    dc_vec2 final_position;
    bool moved;      /* final != from */
    bool blocked;    /* something interrupted the motion */
    bool slid;       /* part of the motion was redirected */
    dc_hit first_hit;
} dc_move_result;

typedef struct dc_move_request {
    dc_body_id body_id;
    dc_vec2 to;
    dc_layer_mask blocking_layers;
    uint32_t flags;
} dc_move_request;

typedef struct dc_move_many_result {
    dc_result status;
    dc_move_result move;
} dc_move_many_result;

/*
 * Authoritative move for a registered body: sweep from its current
 * position toward `to`, optionally slide along axis-aligned contacts,
 * optionally commit the result (broadphase + trigger events).
 */
DC_API dc_result dc_validate_move(
    dc_world *world,
    dc_body_id body_id,
    dc_vec2 to,
    dc_layer_mask blocking_layers,
    uint32_t flags,
    dc_move_result *out_result
);

/*
 * Process up to work_budget requests in input order. Each processed request
 * receives an independent status/result entry, so one invalid or missing body
 * does not prevent later requests from running. The return value is the number
 * of entries processed; entries beyond that count are left untouched.
 *
 * A work_budget of zero processes no requests. Use UINT32_MAX to process all
 * submitted requests. Request ordering is authoritative when body blocking or
 * commit flags are enabled.
 */
DC_API uint32_t dc_world_move_many(
    dc_world *world,
    const dc_move_request *requests,
    dc_move_many_result *results,
    uint32_t count,
    uint32_t work_budget
);

/* ------------------------------------------------------------------ */
/* Triggers                                                           */
/* ------------------------------------------------------------------ */

DC_API dc_result dc_trigger_add_aabb(
    dc_world *world,
    dc_vec2 min,
    dc_vec2 max,
    dc_layer_mask layers,
    uint64_t user_data,
    dc_trigger_id *out_id
);
DC_API dc_result dc_trigger_remove(dc_world *world, dc_trigger_id id);

typedef enum dc_trigger_event_kind {
    DC_TRIGGER_ENTER = 0,
    DC_TRIGGER_LEAVE,
} dc_trigger_event_kind;

typedef struct dc_trigger_event {
    dc_trigger_event_kind kind;
    dc_trigger_id trigger_id;
    dc_body_id body_id;
    uint64_t trigger_user_data;
} dc_trigger_event;

/* Resets the tick's trigger event buffer. */
DC_API dc_result dc_world_begin_tick(dc_world *world, uint64_t tick);
/*
 * Borrowed view of this tick's events, in deterministic order (commit
 * order, then trigger id). Valid until the next begin_tick.
 */
DC_API dc_result dc_world_trigger_events(
    const dc_world *world,
    const dc_trigger_event **out_events,
    size_t *out_count
);

/* ------------------------------------------------------------------ */
/* Family-wide allocation metrics                                     */
/* ------------------------------------------------------------------ */

typedef struct dc_allocation_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    uint64_t growth_operations;
    size_t live_object_count;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dc_allocation_metrics;

/*
 * Process-wide metrics for memory owned by this library. live_object_count
 * is the number of retained heap objects currently owned by the module. Growth
 * and peak counters are lifetime values. Reset starts a post-prewarm window by
 * clearing failure and steady-state allocation counters only.
 */
DC_API void dc_get_allocation_metrics(dc_allocation_metrics *out_metrics);
DC_API void dc_reset_allocation_counters(void);

#ifdef __cplusplus
}
#endif

#endif
