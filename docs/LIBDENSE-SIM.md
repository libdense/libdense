# libdense_sim

`libdense_sim` is the high-density dynamic spatial subscription and recipient
planning kernel.

## Public invariants

- one `ds_world` has one writer;
- mutations occur only inside an open tick;
- tick numbers increase monotonically;
- public entity IDs are arbitrary `uint64_t` values;
- dirty channels are application-defined `uint64_t` masks;
- repeated dirty marks coalesce into one entity mask per tick;
- negative coordinates use mathematical floor semantics;
- observer subscriptions and visibility are maintained incrementally;
- `ENTER` and `LEAVE` represent finalized membership, not transient mutation
  order;
- each `ds_chunk_delta` is an exact recipient group;
- `UPDATE` carries coalesced channel masks;
- sampled motion is the default path;
- kinetic motion requires an explicit stable linear plan; and
- borrowed fanout views remain valid only until the next successful
  `ds_world_begin_tick()` or world destruction.

## Tick lifecycle

```c
ds_world_begin_tick(world, tick);

ds_entity_move(world, entity_id, x, y);
ds_entity_mark_dirty(world, entity_id, DS_CHANNEL_POSITION);

ds_world_end_tick(world);
ds_world_get_fanout_view(world, &fanout);
```

The kernel returns grouped recipient plans. It does not encode packets or own
application payloads.

## Motion

Sampled movement uses `ds_entity_move()`. Stable linear trajectories may use
`ds_entity_set_motion_plan()`, which schedules cell-certificate failures rather
than issuing per-tick position mutations. Explicit sampled movement demotes an
active kinetic plan.

Entities with anchored observers cannot use kinetic motion in v0.1 because
observer coverage boundaries require separate subscription certificates.

## Concurrency

A world is single-writer. Separate worlds may be assigned to separate worker
threads. The public ABI does not make one world concurrently mutable.
