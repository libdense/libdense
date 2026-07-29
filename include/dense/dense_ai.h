#ifndef DENSE_AI_H
#define DENSE_AI_H

/*
 * libdense_ai - perception, threat, behavior execution, and NPC
 * intent generation for high-density multiplayer servers.
 *
 * Deterministic by construction: no clocks, no global rand, no world
 * mutation - a tick of AI turns percept memory and threat state into
 * an ordered intent buffer the game applies through the
 * authoritative pipeline. See DESIGN.md.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DAI_SHARED) && defined(DAI_BUILD)
#define DAI_API __attribute__((visibility("default")))
#else
#define DAI_API
#endif

typedef enum dai_result {
    DAI_OK = 0,
    /* Not an error: budget exhausted, resumable work remains. */
    DAI_AGAIN,
    DAI_ERR_INVALID_ARGUMENT,
    DAI_ERR_OUT_OF_MEMORY,
    DAI_ERR_CAPACITY,
    DAI_ERR_NOT_FOUND,
    DAI_ERR_EXISTS,
    DAI_ERR_STATE,
} dai_result;

DAI_API const char *dai_result_string(dai_result result);

typedef uint64_t dai_agent_id;
typedef uint64_t dai_entity_id;

typedef struct dai_vec2 {
    int32_t x;
    int32_t y;
} dai_vec2;

/* ------------------------------------------------------------------ */
/* Behavior trees (immutable, shared, refcounted)                     */
/* ------------------------------------------------------------------ */

typedef struct dai_tree dai_tree;

#define DAI_TREE_MAX_NODES 255u

typedef enum dai_composite_kind {
    /* Children until one SUCCEEDS (or RUNNING); else FAILURE. */
    DAI_COMPOSITE_SELECTOR = 0,
    /* Children until one FAILS (or RUNNING); else SUCCESS. */
    DAI_COMPOSITE_SEQUENCE,
} dai_composite_kind;

typedef enum dai_status {
    DAI_STATUS_FAILURE = 0,
    DAI_STATUS_SUCCESS,
    DAI_STATUS_RUNNING,
} dai_status;

DAI_API dai_result dai_tree_create(dai_tree **out_tree);
DAI_API void dai_tree_retain(dai_tree *tree);
DAI_API void dai_tree_release(dai_tree *tree);

DAI_API dai_result dai_tree_composite_begin(
    dai_tree *tree,
    dai_composite_kind kind
);
DAI_API dai_result dai_tree_composite_end(dai_tree *tree);
/* Gates its single child: child runs only off cooldown; a SUCCESS
 * result arms the cooldown for this agent. */
DAI_API dai_result dai_tree_cooldown_begin(
    dai_tree *tree,
    uint32_t cooldown_ticks
);
DAI_API dai_result dai_tree_cooldown_end(dai_tree *tree);
/* Inverts its single child's SUCCESS/FAILURE (RUNNING passes). */
DAI_API dai_result dai_tree_invert_begin(dai_tree *tree);
DAI_API dai_result dai_tree_invert_end(dai_tree *tree);
/* Leaves: ids and params are game-defined. */
DAI_API dai_result dai_tree_condition(
    dai_tree *tree,
    uint32_t condition_id,
    uint64_t param
);
DAI_API dai_result dai_tree_action(
    dai_tree *tree,
    uint32_t action_id,
    uint64_t param
);
/* Validates structure and freezes the tree. */
DAI_API dai_result dai_tree_finalize(dai_tree *tree);

/* ------------------------------------------------------------------ */
/* World                                                              */
/* ------------------------------------------------------------------ */

typedef struct dai_world dai_world;

typedef struct dai_world_config {
    /* Seed folded into dai_agent_random (replay: keep it stable). */
    uint64_t seed;
    size_t initial_agent_capacity;
    /* Preallocate the per-tick intent buffer during world creation. */
    size_t initial_intent_capacity;
    /* Hard bound for one tick's intents; 0 means unbounded growth. */
    size_t max_intents;
} dai_world_config;

typedef struct dai_agent_config {
    uint32_t faction;
    /* Bounded memories; defaults 16 / 16 when zero. */
    uint32_t perception_capacity;
    uint32_t threat_capacity;
    /* Sightings expire after this many ticks (default 100). */
    uint32_t perception_ttl_ticks;
    /* Linear threat decay applied per tick (default 0: none). */
    uint64_t threat_decay_per_tick;
    /* Behavior tree (retained); may be NULL for a mindless agent. */
    dai_tree *tree;
} dai_agent_config;

typedef struct dai_world_stats {
    size_t agents;
    uint64_t percepts_stored;
    uint64_t percepts_displaced;
    uint64_t threat_evictions;
    uint64_t agents_ticked;
    uint64_t nodes_evaluated;
    uint64_t intents_emitted;
    uint64_t run_budget_exhaustions;
    uint64_t run_slots_deferred;
} dai_world_stats;

#define DAI_PERCEPT_STORAGE_CLASS_COUNT 4u
#define DAI_THREAT_STORAGE_CLASS_COUNT 3u
#define DAI_COOLDOWN_STORAGE_CLASS_COUNT 4u

typedef struct dai_storage_class_stats {
    uint32_t slot_capacity;
    size_t pages;
    size_t blocks_retained;
    size_t blocks_in_use;
    size_t blocks_high_water;
    size_t retained_bytes;
    uint64_t growth_operations;
} dai_storage_class_stats;

typedef struct dai_world_storage_reserve {
    /* Classes are percept 4/8/16/32, threat 4/8/16, cooldown 8/16/32/64. */
    size_t percept_blocks[DAI_PERCEPT_STORAGE_CLASS_COUNT];
    size_t threat_blocks[DAI_THREAT_STORAGE_CLASS_COUNT];
    size_t cooldown_blocks[DAI_COOLDOWN_STORAGE_CLASS_COUNT];
} dai_world_storage_reserve;

typedef struct dai_world_storage_stats {
    dai_storage_class_stats percept[DAI_PERCEPT_STORAGE_CLASS_COUNT];
    dai_storage_class_stats threat[DAI_THREAT_STORAGE_CLASS_COUNT];
    dai_storage_class_stats cooldown[DAI_COOLDOWN_STORAGE_CLASS_COUNT];
    size_t direct_percept_blocks_in_use;
    size_t direct_threat_blocks_in_use;
    size_t direct_cooldown_blocks_in_use;
    size_t direct_retained_bytes;
} dai_world_storage_stats;

typedef struct dai_world_memory_stats {
    size_t agents_live;
    size_t agent_capacity;
    size_t agent_map_capacity;
    size_t intent_capacity;
    size_t perception_slot_capacity;
    size_t threat_slot_capacity;
    size_t cooldown_slot_capacity;
    size_t variable_storage_retained_bytes;
} dai_world_memory_stats;

DAI_API dai_result dai_world_create(
    dai_world **out_world,
    const dai_world_config *config
);
DAI_API void dai_world_destroy(dai_world *world);
DAI_API const dai_world_stats *dai_world_get_stats(
    const dai_world *world
);
DAI_API dai_result dai_world_get_memory_stats(
    const dai_world *world,
    dai_world_memory_stats *out_stats
);
DAI_API dai_result dai_world_prewarm_storage(
    dai_world *world,
    const dai_world_storage_reserve *reserve
);
DAI_API dai_result dai_world_get_storage_stats(
    const dai_world *world,
    dai_world_storage_stats *out_stats
);

DAI_API dai_result dai_agent_add(
    dai_world *world,
    dai_agent_id agent_id,
    const dai_agent_config *config
);
DAI_API dai_result dai_agent_remove(dai_world *world, dai_agent_id agent_id);
/* Swap brains (retains new, releases old); resets running state. */
DAI_API dai_result dai_agent_set_tree(
    dai_world *world,
    dai_agent_id agent_id,
    dai_tree *tree
);

/* ------------------------------------------------------------------ */
/* Perception                                                         */
/* ------------------------------------------------------------------ */

typedef struct dai_percept {
    dai_entity_id entity_id;
    dai_vec2 position;
    uint32_t faction;
    uint32_t kind;     /* game-defined */
    uint32_t flags;    /* game-defined */
    /* Signal strength / confidence; game-defined scale. */
    uint32_t strength;
    /* Filled by the library on read: last tick this was seen. */
    uint64_t last_seen_tick;
} dai_percept;

DAI_API dai_result dai_perceive(
    dai_world *world,
    dai_agent_id agent_id,
    const dai_percept *percept
);

typedef struct dai_percept_update {
    dai_agent_id agent_id;
    dai_percept percept;
} dai_percept_update;

DAI_API size_t dai_perceive_many(
    dai_world *world,
    const dai_percept_update *updates,
    dai_result *out_results,
    size_t count,
    size_t work_budget
);
/* Live (unexpired) percepts, deterministic order (entity id asc). */
DAI_API dai_result dai_percepts_count(
    const dai_world *world,
    dai_agent_id agent_id,
    size_t *out_count
);
DAI_API dai_result dai_percepts_get(
    const dai_world *world,
    dai_agent_id agent_id,
    size_t index,
    dai_percept *out_percept
);
/* Nearest live percept whose faction bit intersects faction_mask
 * (u32 mask over faction as a bit index modulo 32 is NOT applied:
 * mask matches (1 << (faction % 32))). Ties: lower entity id. */
DAI_API bool dai_percepts_nearest(
    const dai_world *world,
    dai_agent_id agent_id,
    dai_vec2 from,
    uint32_t faction_mask,
    dai_percept *out_percept
);

/* ------------------------------------------------------------------ */
/* Threat                                                             */
/* ------------------------------------------------------------------ */

DAI_API dai_result dai_threat_add(
    dai_world *world,
    dai_agent_id agent_id,
    dai_entity_id entity_id,
    int64_t amount
);

typedef struct dai_threat_update {
    dai_agent_id agent_id;
    dai_entity_id entity_id;
    int64_t amount;
} dai_threat_update;

typedef struct dai_threat_decay_result {
    dai_result status;
    uint32_t entries_updated;
    uint32_t entries_removed;
} dai_threat_decay_result;

DAI_API size_t dai_threat_add_many(
    dai_world *world,
    const dai_threat_update *updates,
    dai_result *out_results,
    size_t count,
    size_t work_budget
);

/* Materialize lazy decay for selected agents at the current world tick. */
DAI_API size_t dai_threat_decay_many(
    dai_world *world,
    const dai_agent_id *agent_ids,
    dai_threat_decay_result *out_results,
    size_t count,
    size_t work_budget
);
DAI_API bool dai_threat_top(
    const dai_world *world,
    dai_agent_id agent_id,
    dai_entity_id *out_entity,
    int64_t *out_threat
);
DAI_API int64_t dai_threat_of(
    const dai_world *world,
    dai_agent_id agent_id,
    dai_entity_id entity_id
);
DAI_API dai_result dai_threat_drop(
    dai_world *world,
    dai_agent_id agent_id,
    dai_entity_id entity_id
);
DAI_API dai_result dai_threat_clear(
    dai_world *world,
    dai_agent_id agent_id
);

/* ------------------------------------------------------------------ */
/* Ticking, handlers, intents                                        */
/* ------------------------------------------------------------------ */

typedef bool (*dai_condition_fn)(
    void *context,
    dai_world *world,
    dai_agent_id agent_id,
    uint32_t condition_id,
    uint64_t param
);
typedef dai_status (*dai_action_fn)(
    void *context,
    dai_world *world,
    dai_agent_id agent_id,
    uint32_t action_id,
    uint64_t param
);

DAI_API dai_result dai_world_set_handlers(
    dai_world *world,
    dai_condition_fn condition_handler,
    dai_action_fn action_handler,
    void *context
);

typedef struct dai_condition_request {
    dai_agent_id agent_id;
    uint32_t condition_id;
    uint64_t param;
} dai_condition_request;

typedef struct dai_condition_result {
    dai_result status;
    bool value;
} dai_condition_result;

DAI_API dai_result dai_condition_evaluate(
    dai_world *world,
    dai_agent_id agent_id,
    uint32_t condition_id,
    uint64_t param,
    bool *out_value
);

/* Batch arbitrary homogeneous or mixed conditions through the registered
 * handler. Function-call and game-data branching remain scalar by design. */
DAI_API size_t dai_condition_evaluate_many(
    dai_world *world,
    const dai_condition_request *requests,
    dai_condition_result *out_results,
    size_t count,
    size_t work_budget
);

/* Resets the intent buffer and the run cursor for this tick. */
DAI_API dai_result dai_world_begin_tick(dai_world *world, uint64_t tick);
DAI_API uint64_t dai_world_tick(const dai_world *world);
/*
 * Compatibility runner. Run up to max_agents agents' trees (0 = all
 * remaining) in deterministic round-robin slot order. New server code
 * should use dai_world_run_budgeted(), where a nonzero scheduler cap is
 * mandatory and reported explicitly.
 */
DAI_API dai_result dai_world_run(dai_world *world, size_t max_agents);
/* Compatibility slice API. New scheduler integration should use the
 * mandatory dai_world_run_budgeted() contract below. */
DAI_API dai_result dai_world_run_slice(
    dai_world *world,
    size_t max_agents,
    size_t *out_agents_ticked
);

typedef struct dai_run_budget {
    /* Required scheduler-supplied cap for this call; must be nonzero. */
    size_t max_agents;
} dai_run_budget;

typedef struct dai_run_report {
    size_t agents_ticked;
    size_t slots_remaining;
    bool complete;
} dai_run_report;

/* Preferred scheduler integration. Unlike the compatibility slice API,
 * max_agents is mandatory and zero is rejected rather than meaning unlimited. */
DAI_API dai_result dai_world_run_budgeted(
    dai_world *world,
    const dai_run_budget *budget,
    dai_run_report *out_report
);
DAI_API bool dai_world_run_complete(const dai_world *world);

typedef struct dai_intent {
    dai_agent_id agent_id;
    uint32_t kind;          /* game-defined */
    dai_entity_id target_entity;
    dai_vec2 position;
    uint64_t data;
} dai_intent;

/* Reserve outside the hot tick; never shrinks the buffer. */
DAI_API dai_result dai_world_reserve_intents(
    dai_world *world,
    size_t capacity
);
DAI_API size_t dai_world_intent_capacity(const dai_world *world);

/* Emit from action handlers (or game systems). */
DAI_API dai_result dai_intent_emit(
    dai_world *world,
    dai_agent_id agent_id,
    uint32_t kind,
    dai_entity_id target_entity,
    dai_vec2 position,
    uint64_t data
);
/* Borrowed view, valid until the next begin_tick. */
DAI_API dai_result dai_world_intents(
    const dai_world *world,
    const dai_intent **out_intents,
    size_t *out_count
);

/* Deterministic per-agent, per-tick randomness (see DESIGN.md). */
DAI_API uint64_t dai_agent_random(
    dai_world *world,
    dai_agent_id agent_id
);

/* ------------------------------------------------------------------ */
/* Family-wide allocation metrics                                     */
/* ------------------------------------------------------------------ */

typedef struct dai_allocation_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    uint64_t growth_operations;
    size_t live_object_count;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dai_allocation_metrics;

/*
 * Process-wide metrics for memory owned by this library. live_object_count
 * is the number of retained heap objects currently owned by the module. Growth
 * and peak counters are lifetime values. Reset starts a post-prewarm window by
 * clearing failure and steady-state allocation counters only.
 */
DAI_API void dai_get_allocation_metrics(dai_allocation_metrics *out_metrics);
DAI_API void dai_reset_allocation_counters(void);

#ifdef __cplusplus
}
#endif

#endif
