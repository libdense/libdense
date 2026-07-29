#ifndef DENSE_NAV_H
#define DENSE_NAV_H

/*
 * libdense_nav - pathfinding, flow fields, path sharing, and
 * navigation data for high-density multiplayer servers.
 *
 * Integer-only and deterministic: identical inputs produce identical
 * paths, fields, and cache behavior on every platform. See DESIGN.md.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DNAV_SHARED) && defined(DNAV_BUILD)
#define DNAV_API __attribute__((visibility("default")))
#else
#define DNAV_API
#endif

typedef enum dnav_result {
    DNAV_OK = 0,
    /* Not an error: more budgeted work remains (resumable builds). */
    DNAV_AGAIN,
    DNAV_ERR_INVALID_ARGUMENT,
    DNAV_ERR_OUT_OF_MEMORY,
    DNAV_ERR_RANGE,
    DNAV_ERR_NO_PATH,
    /* Search exceeded max_expansions before reaching the goal. */
    DNAV_ERR_BUDGET,
    DNAV_ERR_STALE,
    DNAV_ERR_CAPACITY,
    DNAV_ERR_STATE,
    DNAV_ERR_NOT_FOUND,
} dnav_result;

DNAV_API const char *dnav_result_string(dnav_result result);

/* ------------------------------------------------------------------ */
/* Grid                                                               */
/* ------------------------------------------------------------------ */

typedef int32_t dnav_coord;

typedef struct dnav_tile {
    int32_t x;
    int32_t y;
} dnav_tile;

typedef struct dnav_vec2 {
    dnav_coord x;
    dnav_coord y;
} dnav_vec2;

/* Tile costs: 1..254 multiply the base step cost; 255 is a wall. */
#define DNAV_BLOCKED ((uint8_t)255u)
#define DNAV_COST_DEFAULT ((uint8_t)1u)

#define DNAV_GRID_MAX_DIMENSION 65535u
#define DNAV_GRID_CHUNK_DIMENSION 32u

typedef struct dnav_grid dnav_grid;

typedef struct dnav_grid_config {
    /* World position of tile (0, 0)'s minimum corner. */
    dnav_vec2 origin;
    dnav_coord tile_size; /* > 0 */
    uint32_t width;       /* 1 .. DNAV_GRID_MAX_DIMENSION */
    uint32_t height;
} dnav_grid_config;

DNAV_API dnav_result dnav_grid_create(
    dnav_grid **out_grid,
    const dnav_grid_config *config
);
DNAV_API void dnav_grid_destroy(dnav_grid *grid);

DNAV_API dnav_result dnav_grid_set_cost(
    dnav_grid *grid,
    dnav_tile tile,
    uint8_t cost
);
DNAV_API uint8_t dnav_grid_get_cost(const dnav_grid *grid, dnav_tile tile);
DNAV_API bool dnav_grid_walkable(const dnav_grid *grid, dnav_tile tile);
/* Bumped by every mutation; fields and cached paths record it. */
DNAV_API uint64_t dnav_grid_version(const dnav_grid *grid);
DNAV_API dnav_result dnav_grid_dimensions(
    const dnav_grid *grid,
    uint32_t *out_width,
    uint32_t *out_height
);

typedef struct dnav_grid_memory_stats {
    uint64_t logical_tiles;
    size_t allocated_chunks;
    size_t non_default_tiles;
    size_t chunk_map_capacity;
    size_t allocated_bytes;
} dnav_grid_memory_stats;

/* Actual sparse backing currently allocated by the grid. */
DNAV_API dnav_result dnav_grid_get_memory_stats(
    const dnav_grid *grid,
    dnav_grid_memory_stats *out_stats
);
/* Bytes a one-byte-per-tile dense cost array would require. */
DNAV_API uint64_t dnav_grid_estimate_dense_bytes(
    uint32_t width,
    uint32_t height
);

DNAV_API dnav_result dnav_world_to_tile(
    const dnav_grid *grid,
    dnav_vec2 world,
    dnav_tile *out_tile
);
/* Center of the tile in world coordinates. */
DNAV_API dnav_result dnav_tile_to_world(
    const dnav_grid *grid,
    dnav_tile tile,
    dnav_vec2 *out_world
);

/* Corner-safe integer supercover line-of-sight over walkable tiles. */
DNAV_API bool dnav_line_of_sight(
    const dnav_grid *grid,
    dnav_tile from,
    dnav_tile to
);

typedef struct dnav_tile_pair {
    dnav_tile from;
    dnav_tile to;
} dnav_tile_pair;

/* Process up to work_budget entries in input order. */
DNAV_API size_t dnav_line_of_sight_many(
    const dnav_grid *grid,
    const dnav_tile_pair *pairs,
    bool *out_visible,
    size_t count,
    size_t work_budget
);

DNAV_API size_t dnav_grid_get_cost_many(
    const dnav_grid *grid,
    const dnav_tile *tiles,
    uint8_t *out_costs,
    size_t count,
    size_t work_budget
);

/* ------------------------------------------------------------------ */
/* Paths (immutable, refcounted, shareable)                           */
/* ------------------------------------------------------------------ */

typedef struct dnav_path dnav_path;

DNAV_API size_t dnav_path_length(const dnav_path *path);
DNAV_API dnav_tile dnav_path_tile(const dnav_path *path, size_t index);
DNAV_API dnav_vec2 dnav_path_waypoint(const dnav_path *path, size_t index);
/* Total cost in step units (10 straight / 14 diagonal, x tile cost). */
DNAV_API uint64_t dnav_path_cost(const dnav_path *path);
/* Grid version the path was computed against. */
DNAV_API uint64_t dnav_path_version(const dnav_path *path);
DNAV_API void dnav_path_retain(dnav_path *path);
DNAV_API void dnav_path_release(dnav_path *path);

/* ------------------------------------------------------------------ */
/* A* pathfinder                                                      */
/* ------------------------------------------------------------------ */

typedef struct dnav_pathfinder dnav_pathfinder;

typedef struct dnav_path_options {
    /* 0 means unlimited. */
    uint32_t max_expansions;
    /* Greedy line-of-sight waypoint reduction (still tile-walkable). */
    bool smooth;
} dnav_path_options;

typedef struct dnav_pathfinder_stats {
    uint64_t searches;
    uint64_t expansions;
    uint64_t no_path;
    uint64_t budget_exhausted;
} dnav_pathfinder_stats;

/* Scratch grows with visited work and is reused across searches. */
DNAV_API dnav_result dnav_pathfinder_create(
    dnav_pathfinder **out_finder,
    const dnav_grid *grid
);
DNAV_API void dnav_pathfinder_destroy(dnav_pathfinder *finder);
DNAV_API dnav_result dnav_path_find(
    dnav_pathfinder *finder,
    const dnav_grid *grid,
    dnav_tile start,
    dnav_tile goal,
    const dnav_path_options *options,
    dnav_path **out_path
);

/*
 * Resumable A*: begin once, then continue under the caller's shared global
 * expansion budget. DNAV_AGAIN preserves search state for the next slice.
 */
DNAV_API dnav_result dnav_path_search_begin(
    dnav_pathfinder *finder,
    const dnav_grid *grid,
    dnav_tile start,
    dnav_tile goal,
    const dnav_path_options *options
);
DNAV_API dnav_result dnav_path_search_continue(
    dnav_pathfinder *finder,
    const dnav_grid *grid,
    uint32_t budget_expansions,
    uint32_t *out_expansions,
    dnav_path **out_path
);
DNAV_API void dnav_path_search_cancel(dnav_pathfinder *finder);
DNAV_API bool dnav_path_search_active(const dnav_pathfinder *finder);
DNAV_API const dnav_pathfinder_stats *dnav_pathfinder_get_stats(
    const dnav_pathfinder *finder
);

typedef struct dnav_pathfinder_memory_stats {
    size_t visited_nodes;
    size_t node_capacity;
    size_t heap_capacity;
    size_t route_capacity;
    size_t allocated_bytes;
} dnav_pathfinder_memory_stats;

DNAV_API dnav_result dnav_pathfinder_get_memory_stats(
    const dnav_pathfinder *finder,
    dnav_pathfinder_memory_stats *out_stats
);


typedef struct dnav_flow_field dnav_flow_field;

/* ------------------------------------------------------------------ */
/* Hierarchical route graph                                            */
/* ------------------------------------------------------------------ */

typedef uint32_t dnav_route_node_id;
#define DNAV_ROUTE_NODE_NONE UINT32_MAX

typedef struct dnav_route_graph dnav_route_graph;

typedef struct dnav_route_graph_config {
    size_t node_capacity;
    size_t directed_edge_capacity;
} dnav_route_graph_config;

typedef struct dnav_route_graph_stats {
    uint64_t searches;
    uint64_t expansions;
    uint64_t no_route;
    uint64_t budget_exhausted;
} dnav_route_graph_stats;

typedef struct dnav_route_graph_memory_stats {
    size_t node_capacity;
    size_t nodes;
    size_t directed_edge_capacity;
    size_t directed_edges;
    size_t allocated_bytes;
} dnav_route_graph_memory_stats;

DNAV_API dnav_result dnav_route_graph_create(
    dnav_route_graph **out_graph,
    const dnav_route_graph_config *config
);
DNAV_API void dnav_route_graph_destroy(dnav_route_graph *graph);
DNAV_API dnav_result dnav_route_graph_add_node(
    dnav_route_graph *graph,
    dnav_tile tile,
    dnav_route_node_id *out_node
);
DNAV_API dnav_result dnav_route_graph_add_edge(
    dnav_route_graph *graph,
    dnav_route_node_id from,
    dnav_route_node_id to,
    uint32_t cost
);
DNAV_API dnav_result dnav_route_graph_add_bidirectional_edge(
    dnav_route_graph *graph,
    dnav_route_node_id a,
    dnav_route_node_id b,
    uint32_t cost
);
/* Freeze topology and build deterministic adjacency. */
DNAV_API dnav_result dnav_route_graph_finalize(dnav_route_graph *graph);
DNAV_API bool dnav_route_graph_finalized(const dnav_route_graph *graph);
DNAV_API size_t dnav_route_graph_node_count(const dnav_route_graph *graph);
DNAV_API size_t dnav_route_graph_edge_count(const dnav_route_graph *graph);
DNAV_API dnav_tile dnav_route_graph_node_tile(
    const dnav_route_graph *graph,
    dnav_route_node_id node
);
/* Octile-nearest node in 10/14 tile-step units; ties use lowest id. */
DNAV_API dnav_result dnav_route_graph_nearest(
    const dnav_route_graph *graph,
    dnav_tile tile,
    uint32_t max_distance,
    dnav_route_node_id *out_node,
    uint32_t *out_distance
);
/*
 * Deterministic graph route. max_expansions == 0 is unlimited. On
 * DNAV_ERR_CAPACITY, out_node_count receives the required route length.
 */
DNAV_API dnav_result dnav_route_graph_find(
    dnav_route_graph *graph,
    dnav_route_node_id start,
    dnav_route_node_id goal,
    uint32_t max_expansions,
    dnav_route_node_id *out_nodes,
    size_t node_capacity,
    size_t *out_node_count,
    uint64_t *out_cost,
    uint32_t *out_expansions
);
DNAV_API const dnav_route_graph_stats *dnav_route_graph_get_stats(
    const dnav_route_graph *graph
);
DNAV_API dnav_result dnav_route_graph_get_memory_stats(
    const dnav_route_graph *graph,
    dnav_route_graph_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Global budgeted navigation work queue                               */
/* ------------------------------------------------------------------ */

typedef uint64_t dnav_work_id;
#define DNAV_WORK_ID_INVALID UINT64_C(0)

typedef enum dnav_work_kind {
    DNAV_WORK_PATH = 0,
    DNAV_WORK_FLOW_FIELD,
} dnav_work_kind;

typedef struct dnav_work_queue dnav_work_queue;

typedef struct dnav_work_queue_config {
    size_t capacity;
    uint32_t expansion_slice;
    size_t max_flow_goals_per_job;
    size_t path_cache_capacity;
    uint32_t path_cache_cluster_bits;
} dnav_work_queue_config;

typedef struct dnav_path_work_request {
    dnav_tile start;
    dnav_tile goal;
    dnav_path_options options;
    uint64_t user_data;
} dnav_path_work_request;

typedef struct dnav_flow_work_request {
    dnav_flow_field *field;
    const dnav_tile *goals;
    size_t goal_count;
    uint64_t user_data;
} dnav_flow_work_request;

typedef struct dnav_work_result {
    dnav_work_id id;
    dnav_work_kind kind;
    dnav_result result;
    uint64_t user_data;
    uint64_t expansions;
    uint32_t slices;
    /* PATH success transfers one retained reference to the caller. */
    dnav_path *path;
} dnav_work_result;

typedef struct dnav_work_queue_stats {
    uint64_t path_submitted;
    uint64_t flow_submitted;
    uint64_t completed;
    uint64_t failed;
    uint64_t cancelled;
    uint64_t path_cache_hits;
    uint64_t path_cache_misses;
    uint64_t slices;
    uint64_t expansions;
    uint64_t budget_limited_drains;
    size_t pending;
    size_t completed_pending;
    size_t high_water_pending;
} dnav_work_queue_stats;

typedef struct dnav_work_queue_memory_stats {
    size_t capacity;
    size_t max_flow_goals_per_job;
    size_t pending;
    size_t completed_pending;
    size_t pathfinder_bytes;
    size_t path_cache_capacity;
    size_t allocated_bytes;
} dnav_work_queue_memory_stats;

DNAV_API dnav_result dnav_work_queue_create(
    dnav_work_queue **out_queue,
    const dnav_grid *grid,
    const dnav_work_queue_config *config
);
DNAV_API void dnav_work_queue_destroy(dnav_work_queue *queue);
DNAV_API dnav_result dnav_work_queue_submit_path(
    dnav_work_queue *queue,
    const dnav_path_work_request *request,
    dnav_work_id *out_id
);
DNAV_API dnav_result dnav_work_queue_submit_flow(
    dnav_work_queue *queue,
    const dnav_flow_work_request *request,
    dnav_work_id *out_id
);
DNAV_API dnav_result dnav_work_queue_cancel(
    dnav_work_queue *queue,
    dnav_work_id id
);
/*
 * Drain all work through one shared expansion budget. max_slices == 0 means
 * unlimited slices; budget_expansions == 0 means unlimited expansions.
 */
DNAV_API dnav_result dnav_work_queue_drain(
    dnav_work_queue *queue,
    uint32_t budget_expansions,
    uint32_t max_slices,
    uint32_t *out_expansions
);
DNAV_API bool dnav_work_queue_pop_result(
    dnav_work_queue *queue,
    dnav_work_result *out_result
);
DNAV_API const dnav_work_queue_stats *dnav_work_queue_get_stats(
    const dnav_work_queue *queue
);
DNAV_API dnav_result dnav_work_queue_get_memory_stats(
    const dnav_work_queue *queue,
    dnav_work_queue_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Flow fields                                                        */
/* ------------------------------------------------------------------ */

#define DNAV_FLOW_NO_DIRECTION ((uint8_t)8u)
#define DNAV_FLOW_UNREACHED UINT32_MAX

typedef struct dnav_flow_sample_result {
    /* 0..7 step direction toward the goal set, or
     * DNAV_FLOW_NO_DIRECTION (goal tile or not yet reached). */
    uint8_t direction;
    /* Integer distance in step units; DNAV_FLOW_UNREACHED if the
     * spill has not settled this tile (yet). */
    uint32_t distance;
} dnav_flow_sample_result;

DNAV_API dnav_result dnav_flow_field_create(
    dnav_flow_field **out_field,
    const dnav_grid *grid
);
DNAV_API void dnav_flow_field_destroy(dnav_flow_field *field);
/*
 * (Re)start a build toward a goal set. Follow with build_continue
 * until DNAV_OK; DNAV_AGAIN means budget ran out and state persists.
 */
DNAV_API dnav_result dnav_flow_field_build(
    dnav_flow_field *field,
    const dnav_grid *grid,
    const dnav_tile *goals,
    size_t goal_count,
    uint32_t budget_expansions
);
DNAV_API dnav_result dnav_flow_field_build_continue(
    dnav_flow_field *field,
    const dnav_grid *grid,
    uint32_t budget_expansions
);
DNAV_API dnav_flow_sample_result dnav_flow_sample(
    const dnav_flow_field *field,
    dnav_tile tile
);

DNAV_API size_t dnav_flow_sample_many(
    const dnav_flow_field *field,
    const dnav_tile *tiles,
    dnav_flow_sample_result *out_samples,
    size_t count,
    size_t work_budget
);
/* Tile-space step vector for a direction index (0..7). */
DNAV_API dnav_tile dnav_flow_direction_step(uint8_t direction);
DNAV_API bool dnav_flow_field_is_current(
    const dnav_flow_field *field,
    const dnav_grid *grid
);
DNAV_API bool dnav_flow_field_complete(const dnav_flow_field *field);

typedef struct dnav_flow_field_memory_stats {
    size_t reached_nodes;
    size_t node_capacity;
    size_t heap_capacity;
    size_t allocated_bytes;
} dnav_flow_field_memory_stats;

DNAV_API dnav_result dnav_flow_field_get_memory_stats(
    const dnav_flow_field *field,
    dnav_flow_field_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Path cache (clustered sharing)                                     */
/* ------------------------------------------------------------------ */

typedef struct dnav_path_cache dnav_path_cache;

typedef struct dnav_path_cache_config {
    /* Maximum cached paths (LRU beyond). Default 256. */
    size_t capacity;
    /* Start/goal tiles quantize to 2^cluster_bits tile clusters
     * for key purposes. Default 2 (4 x 4 tiles). */
    uint32_t cluster_bits;
} dnav_path_cache_config;

typedef struct dnav_path_cache_stats {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t stale_drops;
    size_t entries;
} dnav_path_cache_stats;

DNAV_API dnav_result dnav_path_cache_create(
    dnav_path_cache **out_cache,
    const dnav_path_cache_config *config
);
DNAV_API void dnav_path_cache_destroy(dnav_path_cache *cache);
/*
 * Shared lookup: a hit returns the cached path (retained for the
 * caller); a miss searches from the actual start/goal and caches the
 * result under the cluster key. Caller releases the path either way.
 */
DNAV_API dnav_result dnav_path_cache_find(
    dnav_path_cache *cache,
    dnav_pathfinder *finder,
    const dnav_grid *grid,
    dnav_tile start,
    dnav_tile goal,
    const dnav_path_options *options,
    dnav_path **out_path
);
DNAV_API const dnav_path_cache_stats *dnav_path_cache_get_stats(
    const dnav_path_cache *cache
);

typedef struct dnav_path_cache_memory_stats {
    size_t path_capacity;
    size_t paths_live;
    size_t map_capacity;
    size_t allocated_bytes;
} dnav_path_cache_memory_stats;

DNAV_API dnav_result dnav_path_cache_get_memory_stats(
    const dnav_path_cache *cache,
    dnav_path_cache_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Family-wide allocation metrics                                     */
/* ------------------------------------------------------------------ */

typedef struct dnav_allocation_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    uint64_t growth_operations;
    size_t live_object_count;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dnav_allocation_metrics;

/*
 * Process-wide metrics for memory owned by this library. live_object_count
 * is the number of retained heap objects currently owned by the module. Growth
 * and peak counters are lifetime values. Reset starts a post-prewarm window by
 * clearing failure and steady-state allocation counters only.
 */
DNAV_API void dnav_get_allocation_metrics(dnav_allocation_metrics *out_metrics);
DNAV_API void dnav_reset_allocation_counters(void);

#ifdef __cplusplus
}
#endif

#endif
