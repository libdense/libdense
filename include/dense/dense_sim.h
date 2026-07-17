#ifndef DENSE_SIM_H
#define DENSE_SIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DS_VERSION_MAJOR 0
#define DS_VERSION_MINOR 1
#define DS_VERSION_PATCH 0
#define DS_VERSION_PRERELEASE "rc1"
#define DS_VERSION_STRING "0.1.0-rc1"
#define DS_ABI_VERSION 1

#if defined(_WIN32) && defined(DS_SHARED)
    #if defined(DS_BUILD)
        #define DS_API __declspec(dllexport)
    #else
        #define DS_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && defined(DS_SHARED)
    #define DS_API __attribute__((visibility("default")))
#else
    #define DS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t ds_entity_id;
typedef uint64_t ds_observer_id;
typedef uint64_t ds_channel_mask;
typedef uint64_t ds_type_mask;
typedef uint64_t ds_tick;

#define DS_CHANNEL_POSITION   (UINT64_C(1) << 0u)
#define DS_CHANNEL_VITALS     (UINT64_C(1) << 1u)
#define DS_CHANNEL_ANIMATION  (UINT64_C(1) << 2u)
#define DS_CHANNEL_APPEARANCE (UINT64_C(1) << 3u)
#define DS_CHANNEL_CUSTOM_0   (UINT64_C(1) << 16u)

typedef struct ds_world ds_world;

typedef enum ds_result {
    DS_OK = 0,
    DS_ERR_INVALID_ARGUMENT,
    DS_ERR_OUT_OF_MEMORY,
    DS_ERR_CAPACITY,
    DS_ERR_ENTITY_EXISTS,
    DS_ERR_ENTITY_NOT_FOUND,
    DS_ERR_OBSERVER_NOT_FOUND,
    DS_ERR_TICK_ALREADY_OPEN,
    DS_ERR_TICK_NOT_OPEN,
    DS_ERR_TICK_ORDER,
    DS_ERR_TICK_NOT_FINALIZED,
    DS_ERR_MOTION_CONFLICT,
} ds_result;

typedef struct ds_world_config {
    int32_t cell_size;
    int32_t chunk_size;
    size_t initial_entity_capacity;
    size_t initial_observer_capacity;
} ds_world_config;

typedef struct ds_observer_config {
    int32_t radius;
    ds_type_mask type_mask;
} ds_observer_config;

typedef struct ds_observer_desc {
    ds_observer_id id;
    int32_t spatial_x;
    int32_t spatial_y;
    int32_t radius;
    ds_type_mask type_mask;
    ds_entity_id anchor_entity_id;
    bool positioned;
    bool anchored;
} ds_observer_desc;

typedef struct ds_entity_desc {
    ds_entity_id id;
    int32_t spatial_x;
    int32_t spatial_y;
    ds_type_mask type_mask;
} ds_entity_desc;

typedef enum ds_motion_mode {
    DS_MOTION_SAMPLED = 0,
    DS_MOTION_KINETIC,
} ds_motion_mode;

/* Linear position is evaluated as (x, y) + (vx, vy) *
 * (tick - start_tick), then floored to the kernel's integer spatial
 * coordinates. until_tick is inclusive. */
typedef struct ds_motion_plan {
    ds_tick start_tick;
    ds_tick until_tick;
    double x;
    double y;
    double vx;
    double vy;
} ds_motion_plan;

typedef struct ds_motion_metrics {
    uint64_t scheduled_events;
    uint64_t processed_events;
    uint64_t stale_events;
    uint64_t cell_crossings;
    uint64_t expiries;
    uint64_t plan_replacements;
    uint64_t sampled_demotions;
    uint64_t correction_steps;
} ds_motion_metrics;

/* v0.1 currently emits UPDATE, ENTER, and LEAVE. SPAWN and REMOVE are
 * reserved operation values for future consumers that distinguish world
 * lifetime from subscription membership lifetime. */
typedef enum ds_delta_op {
    DS_DELTA_UPDATE = 0,
    DS_DELTA_ENTER,
    DS_DELTA_LEAVE,
    DS_DELTA_SPAWN,
    DS_DELTA_REMOVE,
} ds_delta_op;

typedef struct ds_delta_entry {
    ds_entity_id entity_id;
    ds_channel_mask channel_mask;
    ds_delta_op operation;
} ds_delta_entry;

/* One exact reusable fanout group. Every entry applies to every subscriber
 * in the span. The same chunk coordinate may appear in multiple groups when
 * exact recipient sets differ. UPDATE entries carry channel masks; ENTER and
 * LEAVE use a zero channel mask. */
typedef struct ds_chunk_delta {
    int32_t chunk_x;
    int32_t chunk_y;
    const ds_delta_entry *entries;
    size_t entry_count;
    const ds_observer_id *subscribers;
    size_t subscriber_count;
} ds_chunk_delta;

typedef struct ds_fanout_view {
    ds_tick tick;
    const ds_chunk_delta *chunks;
    size_t chunk_count;
    size_t delta_count;
    size_t subscriber_count;
} ds_fanout_view;

DS_API const char *ds_result_string(ds_result result);

DS_API ds_result ds_world_create(
    const ds_world_config *config,
    ds_world **out_world
);
DS_API void ds_world_destroy(ds_world *world);

DS_API ds_result ds_world_begin_tick(ds_world *world, ds_tick tick);
DS_API ds_result ds_world_end_tick(ds_world *world);
DS_API bool ds_world_tick_is_open(const ds_world *world);
DS_API ds_tick ds_world_current_tick(const ds_world *world);
DS_API size_t ds_world_entity_count(const ds_world *world);
DS_API size_t ds_world_entity_capacity(const ds_world *world);
DS_API size_t ds_world_observer_count(const ds_world *world);
DS_API ds_result ds_world_get_motion_metrics(
    const ds_world *world,
    ds_motion_metrics *out_metrics
);
/* Returns the finalized tick's borrowed fanout view. The view and all nested
 * spans remain valid until the next successful ds_world_begin_tick() or until
 * the world is destroyed. */
DS_API ds_result ds_world_get_fanout_view(
    const ds_world *world,
    ds_fanout_view *out_view
);

DS_API ds_result ds_entity_spawn(
    ds_world *world,
    ds_entity_id entity_id,
    int32_t spatial_x,
    int32_t spatial_y,
    ds_type_mask type_mask
);
/* Sampled movement. A successful move demotes an active kinetic plan.
 * Movement does not select or mark an application dirty channel. */
DS_API ds_result ds_entity_move(
    ds_world *world,
    ds_entity_id entity_id,
    int32_t spatial_x,
    int32_t spatial_y
);
DS_API ds_result ds_entity_remove(
    ds_world *world,
    ds_entity_id entity_id
);
DS_API ds_result ds_entity_mark_dirty(
    ds_world *world,
    ds_entity_id entity_id,
    ds_channel_mask channel_mask
);
/* Assign or replace a stable linear kinetic plan. v0.1 rejects entities
 * with anchored observers because observer coverage boundaries require a
 * distinct certificate schedule from entity cell crossings. */
DS_API ds_result ds_entity_set_motion_plan(
    ds_world *world,
    ds_entity_id entity_id,
    const ds_motion_plan *plan
);
/* Materialize the active plan at the current tick through the normal spatial
 * transition path, then demote the entity to sampled motion. */
DS_API ds_result ds_entity_clear_motion_plan(
    ds_world *world,
    ds_entity_id entity_id
);
DS_API ds_result ds_entity_motion_mode(
    const ds_world *world,
    ds_entity_id entity_id,
    ds_motion_mode *out_mode
);
DS_API bool ds_entity_exists(
    const ds_world *world,
    ds_entity_id entity_id
);
DS_API ds_result ds_entity_get(
    const ds_world *world,
    ds_entity_id entity_id,
    ds_entity_desc *out_entity
);

DS_API ds_result ds_observer_create(
    ds_world *world,
    const ds_observer_config *config,
    ds_observer_id *out_observer_id
);
DS_API ds_result ds_observer_destroy(
    ds_world *world,
    ds_observer_id observer_id
);
DS_API ds_result ds_observer_anchor_entity(
    ds_world *world,
    ds_observer_id observer_id,
    ds_entity_id entity_id
);
DS_API ds_result ds_observer_set_position(
    ds_world *world,
    ds_observer_id observer_id,
    int32_t spatial_x,
    int32_t spatial_y
);
DS_API ds_result ds_observer_set_radius(
    ds_world *world,
    ds_observer_id observer_id,
    int32_t radius
);
DS_API bool ds_observer_exists(
    const ds_world *world,
    ds_observer_id observer_id
);
DS_API ds_result ds_observer_get(
    const ds_world *world,
    ds_observer_id observer_id,
    ds_observer_desc *out_observer
);

#ifdef __cplusplus
}
#endif

#endif
