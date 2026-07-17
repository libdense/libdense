#ifndef DENSEDB_H
#define DENSEDB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dense_sim.h"

#define DDB_VERSION_MAJOR 0
#define DDB_VERSION_MINOR 1
#define DDB_VERSION_PATCH 0
#define DDB_VERSION_PRERELEASE "rc1"
#define DDB_VERSION_STRING "0.1.0-rc1"
#define DDB_ABI_VERSION 1

#if defined(_WIN32) && defined(DDB_SHARED)
    #if defined(DDB_BUILD)
        #define DDB_API __declspec(dllexport)
    #else
        #define DDB_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && defined(DDB_SHARED)
    #define DDB_API __attribute__((visibility("default")))
#else
    #define DDB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ddb_table_id;
typedef uint64_t ddb_watch_id;

#define DDB_NO_COLUMN UINT32_MAX

typedef struct ddb_database ddb_database;

typedef enum ddb_result {
    DDB_OK = 0,
    DDB_ERR_INVALID_ARGUMENT,
    DDB_ERR_OUT_OF_MEMORY,
    DDB_ERR_CAPACITY,
    DDB_ERR_SCHEMA,
    DDB_ERR_TABLE_EXISTS,
    DDB_ERR_TABLE_NOT_FOUND,
    DDB_ERR_ENTITY_EXISTS,
    DDB_ERR_ENTITY_NOT_FOUND,
    DDB_ERR_COLUMN_NOT_FOUND,
    DDB_ERR_CHANNEL_NOT_FOUND,
    DDB_ERR_TYPE_MISMATCH,
    DDB_ERR_SPATIAL_COLUMN,
    DDB_ERR_TICK_ALREADY_OPEN,
    DDB_ERR_TICK_NOT_OPEN,
    DDB_ERR_TICK_ORDER,
    DDB_ERR_TICK_NOT_FINALIZED,
    DDB_ERR_WATCH_NOT_FOUND,
    DDB_ERR_WATCH_MODE,
    DDB_ERR_IO,
    DDB_ERR_CORRUPT,
    DDB_ERR_VERSION,
    DDB_ERR_DURABILITY,
    DDB_ERR_KERNEL,
} ddb_result;

typedef enum ddb_column_type {
    DDB_COLUMN_I8 = 0,
    DDB_COLUMN_U8,
    DDB_COLUMN_I16,
    DDB_COLUMN_U16,
    DDB_COLUMN_I32,
    DDB_COLUMN_U32,
    DDB_COLUMN_I64,
    DDB_COLUMN_U64,
    DDB_COLUMN_F32,
    DDB_COLUMN_F64,
    DDB_COLUMN_BOOL,
    DDB_COLUMN_FIXED_BYTES,
} ddb_column_type;

typedef struct ddb_bytes {
    const void *data;
    size_t size;
} ddb_bytes;

typedef struct ddb_value {
    ddb_column_type type;
    union {
        int8_t i8;
        uint8_t u8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f32;
        double f64;
        bool boolean;
        ddb_bytes bytes;
    } as;
} ddb_value;


typedef enum ddb_watch_target {
    DDB_WATCH_FIXED = 0,
    DDB_WATCH_AROUND_ENTITY,
} ddb_watch_target;

typedef enum ddb_watch_phase {
    DDB_WATCH_PHASE_SNAPSHOT = 0,
    DDB_WATCH_PHASE_DELTA,
} ddb_watch_phase;

typedef enum ddb_watch_operation {
    DDB_WATCH_SNAPSHOT = 0,
    DDB_WATCH_ENTER,
    DDB_WATCH_UPDATE,
    DDB_WATCH_LEAVE,
} ddb_watch_operation;

typedef struct ddb_watch_config {
    const ddb_table_id *table_ids;
    size_t table_count;
    ds_channel_mask channel_mask;
    int32_t radius;
} ddb_watch_config;

typedef struct ddb_watch_info {
    ddb_watch_id id;
    ds_observer_id observer_id;
    ddb_watch_target target;
    ds_entity_id anchor_entity_id;
    int32_t spatial_x;
    int32_t spatial_y;
    int32_t radius;
    ds_channel_mask channel_mask;
    const ddb_table_id *table_ids;
    size_t table_count;
    bool snapshot_pending;
} ddb_watch_info;

typedef struct ddb_watch_field {
    uint32_t column_index;
    const char *name;
    ddb_column_type type;
    ds_channel_mask channel_mask;
    const void *data;
    size_t size;
} ddb_watch_field;

typedef struct ddb_watch_delta {
    ddb_table_id table_id;
    ds_entity_id entity_id;
    ddb_watch_operation operation;
    ds_channel_mask channel_mask;
    size_t field_start;
    size_t field_count;
} ddb_watch_delta;

typedef struct ddb_watch_view {
    ddb_watch_id watch_id;
    ds_tick tick;
    ddb_watch_phase phase;
    const ddb_watch_delta *deltas;
    size_t delta_count;
    const ddb_watch_field *fields;
    size_t field_count;
} ddb_watch_view;

/* Configuration for a single-process durable database directory.
 * ddb_database_open() takes a nonblocking exclusive advisory lock on the WAL.
 * When sync_on_commit is false, an acknowledged tick is committed to the
 * process-visible WAL but is not guaranteed to survive power loss. */
typedef struct ddb_durability_config {
    const char *directory;
    bool create_if_missing;
    bool sync_on_commit;
} ddb_durability_config;

/* committed_ticks counts commits performed by this open database instance.
 * recovered_tick is the latest committed tick represented by recovered or
 * newly committed state. */
typedef struct ddb_durability_info {
    bool enabled;
    bool sync_on_commit;
    ds_tick snapshot_tick;
    ds_tick recovered_tick;
    uint64_t committed_ticks;
    uint64_t wal_bytes;
} ddb_durability_info;

typedef struct ddb_database_config {
    ds_world_config spatial;
    size_t initial_table_capacity;
    size_t initial_entity_capacity;
} ddb_database_config;

typedef struct ddb_column_schema {
    const char *name;
    ddb_column_type type;
    ds_channel_mask channel_mask;
    size_t fixed_size;
} ddb_column_schema;

typedef struct ddb_table_schema {
    const char *name;
    ds_type_mask type_mask;
    const ddb_column_schema *columns;
    size_t column_count;
    uint32_t spatial_x_column;
    uint32_t spatial_y_column;
    size_t initial_row_capacity;
} ddb_table_schema;

typedef struct ddb_table_info {
    ddb_table_id id;
    const char *name;
    ds_type_mask type_mask;
    size_t row_count;
    size_t row_capacity;
    size_t column_count;
    size_t channel_count;
    uint32_t spatial_x_column;
    uint32_t spatial_y_column;
    bool spatial;
} ddb_table_info;

typedef struct ddb_column_info {
    uint32_t index;
    const char *name;
    ddb_column_type type;
    ds_channel_mask channel_mask;
    size_t element_size;
} ddb_column_info;

typedef struct ddb_column_view {
    uint32_t index;
    const char *name;
    ddb_column_type type;
    ds_channel_mask channel_mask;
    size_t element_size;
    const void *data;
    size_t row_count;
} ddb_column_view;

typedef struct ddb_channel_view {
    ds_channel_mask channel_mask;
    const ddb_column_view *columns;
    size_t column_count;
    size_t row_count;
    uint64_t storage_generation;
} ddb_channel_view;

typedef struct ddb_column_update {
    uint32_t column_index;
    ddb_value value;
} ddb_column_update;

typedef struct ddb_move_command {
    ds_entity_id entity_id;
    int32_t spatial_x;
    int32_t spatial_y;
} ddb_move_command;

typedef struct ddb_metrics {
    uint64_t rows_inserted;
    uint64_t rows_removed;
    uint64_t row_updates;
    uint64_t spatial_moves;
    uint64_t dirty_mark_calls;
    uint64_t column_writes;
    uint64_t bytes_written;
    uint64_t watch_snapshots_built;
    uint64_t watch_delta_batches_built;
    uint64_t watch_deltas_built;
    uint64_t watch_fields_built;
    uint64_t watch_value_bytes_referenced;
    uint64_t wal_records_written;
    uint64_t wal_bytes_written;
    uint64_t wal_commits;
    uint64_t snapshots_written;
    uint64_t snapshot_bytes_written;
    uint64_t recovery_records_replayed;
    uint64_t recovery_ticks_replayed;
} ddb_metrics;

DDB_API const char *ddb_result_string(ddb_result result);
DDB_API size_t ddb_column_type_size(ddb_column_type type);

DDB_API ddb_result ddb_database_create(
    const ddb_database_config *config,
    ddb_database **out_database
);
DDB_API void ddb_database_destroy(ddb_database *database);
/* Opens or creates a durable database, restores a validated snapshot, and
 * replays checksum-valid committed WAL ticks. Torn/uncommitted WAL tails are
 * truncated. Active WATCHes expose a fresh snapshot view after recovery. */
DDB_API ddb_result ddb_database_open(
    const ddb_database_config *config,
    const ddb_durability_config *durability,
    ddb_database **out_database
);
/* Syncs the committed WAL. Rejects an open or commit-pending tick. */
DDB_API ddb_result ddb_database_flush(ddb_database *database);
/* Atomically replaces the full snapshot, fsyncs it and the directory, then
 * resets the WAL to its validated header. Requires a finalized tick. */
DDB_API ddb_result ddb_database_checkpoint(ddb_database *database);
DDB_API ddb_result ddb_database_get_durability_info(
    const ddb_database *database,
    ddb_durability_info *out_info
);

DDB_API ddb_result ddb_database_begin_tick(
    ddb_database *database,
    ds_tick tick
);
/* Finalizes spatial/WATCH work, writes the tick records plus TICK_COMMIT,
 * and applies the configured sync policy before returning DDB_OK. A failed
 * durable commit remains retryable through another end_tick() call. */
DDB_API ddb_result ddb_database_end_tick(ddb_database *database);
DDB_API bool ddb_database_tick_is_open(const ddb_database *database);
DDB_API ds_tick ddb_database_current_tick(const ddb_database *database);
DDB_API size_t ddb_database_table_count(const ddb_database *database);
DDB_API size_t ddb_database_entity_count(const ddb_database *database);
DDB_API size_t ddb_database_watch_count(const ddb_database *database);
DDB_API ddb_result ddb_database_get_metrics(
    const ddb_database *database,
    ddb_metrics *out_metrics
);
DDB_API ddb_result ddb_database_find_entity_table(
    const ddb_database *database,
    ds_entity_id entity_id,
    ddb_table_id *out_table_id
);

DDB_API ddb_result ddb_table_create(
    ddb_database *database,
    const ddb_table_schema *schema,
    ddb_table_id *out_table_id
);
DDB_API ddb_result ddb_table_find(
    const ddb_database *database,
    const char *name,
    ddb_table_id *out_table_id
);
DDB_API ddb_result ddb_table_get_info(
    const ddb_database *database,
    ddb_table_id table_id,
    ddb_table_info *out_info
);
DDB_API ddb_result ddb_table_get_column_info(
    const ddb_database *database,
    ddb_table_id table_id,
    uint32_t column_index,
    ddb_column_info *out_info
);
/* Borrowed structure-of-arrays view. Column data remains valid until a row
 * insert/remove grows or compacts this table, or until database destruction. */
DDB_API ddb_result ddb_table_get_channel_view(
    ddb_database *database,
    ddb_table_id table_id,
    ds_channel_mask channel_mask,
    ddb_channel_view *out_view
);

DDB_API ddb_result ddb_row_insert(
    ddb_database *database,
    ddb_table_id table_id,
    ds_entity_id entity_id,
    const ddb_value *values,
    size_t value_count
);
DDB_API ddb_result ddb_row_remove(
    ddb_database *database,
    ddb_table_id table_id,
    ds_entity_id entity_id
);
DDB_API bool ddb_row_exists(
    const ddb_database *database,
    ddb_table_id table_id,
    ds_entity_id entity_id
);
/* Scalar values are copied. FIXED_BYTES returns a borrowed pointer into table
 * storage and must not be retained across row mutation or database destruction. */
DDB_API ddb_result ddb_row_get_value(
    const ddb_database *database,
    ddb_table_id table_id,
    ds_entity_id entity_id,
    uint32_t column_index,
    ddb_value *out_value
);
DDB_API ddb_result ddb_row_update(
    ddb_database *database,
    ddb_table_id table_id,
    ds_entity_id entity_id,
    const ddb_column_update *updates,
    size_t update_count
);
DDB_API ddb_result ddb_row_move(
    ddb_database *database,
    ddb_table_id table_id,
    ds_entity_id entity_id,
    int32_t spatial_x,
    int32_t spatial_y
);
/* Sequential batch. On failure, out_applied reports completed commands and
 * earlier moves remain committed; no rollback is attempted. */
DDB_API ddb_result ddb_row_move_many(
    ddb_database *database,
    ddb_table_id table_id,
    const ddb_move_command *commands,
    size_t command_count,
    size_t *out_applied
);

DDB_API ddb_result ddb_watch_create_fixed(
    ddb_database *database,
    const ddb_watch_config *config,
    int32_t spatial_x,
    int32_t spatial_y,
    ddb_watch_id *out_watch_id
);
DDB_API ddb_result ddb_watch_create_around_entity(
    ddb_database *database,
    const ddb_watch_config *config,
    ds_entity_id anchor_entity_id,
    ddb_watch_id *out_watch_id
);
DDB_API ddb_result ddb_watch_destroy(
    ddb_database *database,
    ddb_watch_id watch_id
);
DDB_API bool ddb_watch_exists(
    const ddb_database *database,
    ddb_watch_id watch_id
);
DDB_API ddb_result ddb_watch_get_info(
    const ddb_database *database,
    ddb_watch_id watch_id,
    ddb_watch_info *out_info
);
DDB_API ddb_result ddb_watch_set_position(
    ddb_database *database,
    ddb_watch_id watch_id,
    int32_t spatial_x,
    int32_t spatial_y
);
DDB_API ddb_result ddb_watch_set_radius(
    ddb_database *database,
    ddb_watch_id watch_id,
    int32_t radius
);
/* Borrowed view of one finalized WATCH stream. Field pointers reference final
 * table storage. The view is invalidated by the next successful
 * ddb_database_begin_tick(), watch destruction, or database destruction. */
DDB_API ddb_result ddb_watch_get_view(
    const ddb_database *database,
    ddb_watch_id watch_id,
    ddb_watch_view *out_view
);

#ifdef __cplusplus
}
#endif

#endif
