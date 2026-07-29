# DenseDB

DenseDB is a single-writer state database whose spatial execution, moving
subscriptions, and recipient planning are delegated to `libdense_sim`.

## Durable open

`ddb_database_create()` still creates an in-memory database.

`ddb_database_open()` creates or recovers a durable database directory:

```c
ddb_database_config config = {
    .spatial = {
        .cell_size = 8,
        .chunk_size = 16,
        .initial_entity_capacity = 100000,
        .initial_observer_capacity = 10000,
    },
    .initial_table_capacity = 8,
    .initial_entity_capacity = 100000,
};

ddb_durability_config durability = {
    .directory = "/var/lib/densedb/world-1",
    .create_if_missing = true,
    .sync_on_commit = true,
    .mode = DDB_DURABILITY_SYNCHRONOUS,
};

ddb_database *database = NULL;

ddb_result result = ddb_database_open(
    &config,
    &durability,
    &database
);
```

The directory contains:

```text
densedb.wal
densedb.snapshot
```

A stale `densedb.snapshot.tmp` is ignored and removed during open.

## Commit boundary

Durability is tick-based and has two modes.

### Synchronous mode

`DDB_DURABILITY_SYNCHRONOUS` is the source-compatible default.
`ddb_database_end_tick()` finalizes spatial/WATCH state, seals the retained WAL
buffer, writes it, and applies `sync_on_commit` before returning. I/O failures
leave the pending commit retryable through another `ddb_database_end_tick()` or
`ddb_database_flush()`.

### Write-behind mode

`DDB_DURABILITY_WRITE_BEHIND` separates simulation-thread tick close from WAL
I/O:

```text
finish simulation writes
finalize WATCH output
append fixed-size TICK_COMMIT
swap retained active/pending WAL buffers
publish immutable pending tick
continue the next tick in the other buffer
```

Reserve both buffers before live ticks:

```c
ddb_database_prewarm_durability(database, 256 * 1024, 4096);
```

Drain pending bytes under the scheduler flush phase:

```c
while (ddb_database_has_pending_write(database)) {
    ddb_flush_report report;
    ddb_database_flush_pending_bounded(
        database,
        4096,
        &report
    );
}
```

One external I/O consumer may call the bounded flush function while the
simulation thread records the next tick into the other retained buffer. Only one
pending tick is allowed. If the next tick reaches its seal point before the
previous pending write completes, `ddb_database_end_tick()` returns
`DDB_ERR_BUSY`; flush the pending tick and retry the seal.

`sync_on_commit=true` applies `fdatasync` when a pending tick finishes.
`ddb_database_flush()` drains any pending tick and explicitly syncs the WAL.

Destroying a write-behind database with an unfinished pending tick does not
acknowledge that tick. Recovery truncates any partial or complete-but-uncommitted
tail to the last checksum-valid `TICK_COMMIT`.

## WAL recovery

The WAL is a little-endian, versioned binary format. Every record contains:

```text
record type
tick
payload length
header CRC-32
payload CRC-32
```

The independent header checksum lets recovery distinguish a valid record
header followed by a torn payload from a corrupted length/type header.

Commands are followed by one `TICK_COMMIT` record. Recovery:

1. validates the WAL header and spatial configuration;
2. scans to the last checksum-valid committed tick;
3. truncates a torn or complete-but-uncommitted tail;
4. replays only committed ticks newer than the snapshot;
5. rebuilds table state and `libdense_sim` spatial state through the same
   DenseDB command paths used during normal execution.

A checksum failure inside committed data is reported as `DDB_ERR_CORRUPT`
rather than silently discarding state.

## Checkpoints

A checkpoint requires a finalized database with no pending durability commit:

```c
ddb_database_checkpoint(database);
```

The checkpoint path is:

```text
build complete snapshot payload
        |
        v
write densedb.snapshot.tmp
        |
        v
fsync temporary file
        |
        v
rename over densedb.snapshot
        |
        v
fsync database directory
        |
        v
truncate WAL to its validated header
```

The snapshot stores:

- spatial cell/chunk configuration;
- immutable table schemas;
- all entity IDs and column values;
- stable DenseDB watch IDs;
- fixed WATCH positions;
- entity-following WATCH anchors;
- WATCH table/channel/radius configuration;
- the committed snapshot tick.

The snapshot is checksummed and versioned. If a crash occurs after snapshot
replacement but before WAL truncation, recovery loads the new snapshot and
skips older WAL ticks.

## WATCH recovery

DenseDB watch IDs remain stable across restart. Internal `libdense_sim`
observer IDs may change.

On the final recovered tick, active observers are recreated inside the kernel
and their current visibility is finalized through the canonical subscription
engine. Each recovered WATCH therefore exposes a fresh
`DDB_WATCH_PHASE_SNAPSHOT` view immediately after `ddb_database_open()`.

DenseDB does not reconstruct AOI with a second spatial implementation.

## Durability information

```c
ddb_durability_info info;

ddb_database_get_durability_info(database, &info);
```

The view reports whether durability is enabled, the selected mode and sync
policy, snapshot/recovered ticks, sealed and committed tick counts, busy-seal
backpressure, pending tick/byte progress, and WAL size.

`ddb_database_flush()` explicitly syncs the WAL descriptor. Normal durable
commits already sync when `sync_on_commit=true`.

## WATCH model

A fixed watch selects explicit spatial tables and channels around a position:

```c
ddb_table_id tables[] = {players, monsters};

ddb_watch_config config = {
    .table_ids = tables,
    .table_count = 2,
    .channel_mask =
        DS_CHANNEL_POSITION |
        DS_CHANNEL_VITALS |
        DS_CHANNEL_ANIMATION,
    .radius = 40,
};
```

A moving watch follows an entity with `ddb_watch_create_around_entity()`.
WATCH output remains borrowed and column-oriented: fields point directly into
final structure-of-arrays table storage, and the next successful
`ddb_database_begin_tick()` invalidates prior views.

## Build

```bash
make
make test
make sanitize
make strict
make benchmark
make benchmark-write-behind
make benchmark-native
make example
```

From the repository root:

```bash
make densedb-test
make densedb-sanitize
make densedb-strict
make densedb-benchmark-native
make densedb-example
```

## Current boundaries

- one database writer;
- one durable process owns a database directory at a time through a nonblocking
  exclusive advisory lock on `densedb.wal`;
- schemas are immutable;
- table deletion is not implemented;
- entity IDs are globally unique across tables;
- durable schema/watch mutations require an open tick;
- command batches remain sequential and non-rollback;
- snapshot creation currently builds the snapshot payload in memory;
- WAL/snapshot formats are v1 and reject unknown versions;
- WAL replication, standby streaming, network protocols, SQL, joins,
  clustering, and distributed consensus remain excluded.
