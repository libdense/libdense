# libdense_sim

`libdense_sim` is the high-density dynamic spatial subscription and recipient
planning kernel.

## Optional recipient worksets

The canonical `ds_fanout_view` remains the preferred encode-once output. Use a
separate `ds_recipient_workset` when payload selection, admission, or cadence
must differ per observer:

```c
ds_recipient_workset *workset = NULL;
ds_recipient_workset_view view;

ds_recipient_workset_create(NULL, &workset);
ds_recipient_workset_sync(workset, world);
ds_recipient_workset_enqueue_source(
    workset,
    world,
    entity_id,
    DS_CHANNEL_POSITION
);
ds_recipient_workset_get_view(workset, observer_id, &view);
```

Synchronize after `ds_world_end_tick()`. Consecutive finalized ticks consume
only ENTER/LEAVE membership changes; missed synchronization or a world change
causes an authoritative rebuild. Dirty masks for still-visible entities are
retained. Recipient entries are sorted by entity ID, and acknowledge/clear
operations require the view's generation and fingerprint so stale membership
cannot clear current work.

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
- kinetic motion requires an explicit stable linear plan;
- borrowed fanout views remain valid only until the next successful
  `ds_world_begin_tick()` or world destruction; and
- borrowed recipient-workset spans remain valid until the next successful
  sync, enqueue, acknowledge, clear, or workset destruction.

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

Entities with anchored observers cannot use kinetic motion because
observer coverage boundaries require separate subscription certificates.

## Concurrency

A world is single-writer. Separate worlds may be assigned to separate worker
threads. The public ABI does not make one world concurrently mutable.

## Factorized membership

Since 0.3.7, the core stores entity chunk/type factors and observer chunk
subscriptions instead of an individual membership node per visible pair.
Entities with the same old/new visibility transition share recipient
comparisons and lifecycle spans. 0.3.8 retains sorted spans across ticks,
invalidates them on subscriber changes, and uses a subscriber-event delta
when affordable. Coverage nodes use a torus-indexed rectangle.

Finalized semantics remain exact: ENTER supplies final state to new
recipients, LEAVE identifies the old finalized chunk, and UPDATE reaches
continuing recipients with the coalesced dirty mask. Leaving and returning
within a tick does not create transient visibility events. Recreating an
entity ID produces the old lifetime's LEAVE and the new lifetime's ENTER.

A fanout group applies every entry to every subscriber. Subscriber IDs are
sorted, and one chunk can have multiple groups. Consumers must not use a
particular group index as an identity. Allocation failure during finalization
leaves the tick open for retry.

For N entities and N observers sharing one chunk/type, the core stores O(N)
factors while representing N*N visible pairs. Fragmented masks and coverage
increase the number and size of factors. Explicit recipient worksets and
per-recipient delivery still have costs proportional to their requested
output.

`ds_world_memory_stats.membership_capacity` reports zero. The
`fanout_subscriber_capacity` field reports retained UPDATE exclusion scratch.
`ds_get_allocation_metrics()` includes the module's retained allocations.
