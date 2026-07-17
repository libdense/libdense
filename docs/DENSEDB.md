# DenseDB

DenseDB is a single-writer state database whose spatial execution, moving
subscriptions, and recipient planning are delegated to `libdense_sim`.

## Data model

- immutable table schemas;
- dense rows and structure-of-arrays columns;
- scalar and fixed-width byte columns;
- one dirty channel per column;
- globally unique entity IDs across tables; and
- optional spatial X/Y columns that drive `libdense_sim`.

## WATCH model

WATCHes may be fixed-position or entity-following. They select spatial tables,
channels, and a radius, and expose borrowed `SNAPSHOT`, `ENTER`, `UPDATE`, and
`LEAVE` views.

The next successful `ddb_database_begin_tick()` invalidates prior WATCH views.

## Durable open

```c
ddb_durability_config durability = {
    .directory = "/var/lib/densedb/world-1",
    .create_if_missing = true,
    .sync_on_commit = true,
};

ddb_database_open(&config, &durability, &database);
```

A durable directory contains `densedb.wal` and `densedb.snapshot`. One process
owns a durable directory at a time through an exclusive advisory lock.

## Commit boundary

A tick becomes durably committed only after `ddb_database_end_tick()` returns
`DDB_OK`. The finalization path builds WATCH output, appends WAL command records,
writes the tick commit marker, and performs `fdatasync` when requested.

## Recovery and checkpoints

Recovery validates checksums, discards torn or uncommitted WAL tails, replays
only committed ticks newer than the snapshot, and reconstructs spatial state
through the canonical kernel API.

`ddb_database_checkpoint()` writes and syncs a complete temporary snapshot,
atomically replaces the prior snapshot, syncs the directory, and truncates the
WAL to its validated header.

## v0.1 boundaries

DenseDB v0.1 does not provide SQL, joins, distributed consensus, automatic
sharding, multi-master replication, a network database protocol, variable-length
blob storage, schema migration, rollback transactions, or a hosted service.
