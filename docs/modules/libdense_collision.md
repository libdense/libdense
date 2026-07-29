# libdense_collision

**Authoritative collision queries, sweeps, triggers, and movement
validation for high-density multiplayer servers.**

## Phase 5 batch queries and SIMD gate

`dc_query_circle_many()`, `dc_raycast_many()`, and
`dc_circle_aabb_overlap_many()` provide bounded scalar-reference batches beside
`dc_world_move_many()`. The comparison-only AVX2 gather kernel is exact but
failed the performance gate, so production remains scalar. Run
`make benchmark-phase5-batch-simd` to reproduce the decision.


## 0.2.0 MMO integration update

- Added inline trigger membership for the common case: each body stores up to
  `DC_BODY_INLINE_TRIGGERS` (8) sorted trigger slots without a per-body heap
  allocation. Larger sets use retained overflow storage that is reused after
  the body returns to the inline range.
- Added `dc_world_move_many()` for bounded, input-ordered authoritative move
  batches with per-request status/results and explicit work-budget deferral.
- Added trigger-membership and bulk-movement telemetry plus a standing
  post-prewarm zero-allocation batch test.
- Added sparse-world memory introspection with
  `dc_world_get_memory_stats` and an AABB-to-cell sizing estimator.
- The broadphase remains occupied-cell sparse; world extent does not
  allocate a continent-sized array. Static geometry should be inserted
  once during region/world boot and then reused by movement, raycast,
  trigger, and nav-rasterization consumers.
- Added optional `LTO=1` builds for the final release link.

libdense_collision is the authoritative-geometry module of the Dense
ecosystem, alongside `libdense_sim`, `libdense_net`, and
`libdense_sched`. libdense_nav (pathfinding) builds directly on it;
libdense_ai builds on nav. Standalone, source-available C11, zero
dependencies.

The defining property: **there is not a single float in this
library**. All geometry is integer / fixed point - rational slab
tests, Q16.16 sweep times, and exact 128-bit integer square roots (a
portable two-u64 implementation) for circle contacts. Identical
inputs produce identical hits, in identical order, at identical
positions, on every platform - which is what makes the server's
answers replayable (libdense_replay) and client disagreement always
the client's problem.

- **World model**: 2D. Static AABB solids, dynamic circle
  bodies keyed by game-owned u64 entity ids (the same ids
  libdense_sim tracks), AABB triggers, 32-bit layer masks on
  everything.
- **Queries**: point / circle / AABB overlap enumeration.
- **First-hit**: raycast and swept circle with exact Minkowski
  face + corner decomposition, deterministic tie-breaks, contact
  normals, and a one-unit pushout so reported positions never rest in
  penetration.
- **Movement validation**: `dc_validate_move` sweeps a registered
  body toward its intent, optionally slides along axis-aligned
  contacts, optionally blocks on other bodies, optionally commits -
  updating the broadphase and emitting deterministic trigger
  ENTER/LEAVE events (teleports included).
- **Bulk movement**: `dc_world_move_many` processes a bounded prefix of
  requests in authoritative input order. It is the batch-shaped seam for later
  broadphase reuse and SIMD work; this Phase 4 implementation intentionally
  preserves scalar-equivalent semantics before Phase 5 acceleration.
- **Trigger membership**: up to eight sorted trigger slots are stored inline
  in each body. Larger sets grow one retained overflow vector, eliminating the
  common per-body allocation without imposing a hard trigger limit.
- **Broadphase**: uniform grid in the same shape as libdense_sim's
  cell/chunk design; allocation-free steady state after retained capacities
  are warmed.

## Layout

```text
include/dense_collision.h  public ABI
src/                       math (u128 + sweeps), grid, world, query
tests/                     anchors + brute-force fuzz (4800 sweeps,
                           4000 queries) + determinism checksum
benchmarks/                dense crowd (validated moves/s),
                           sweep storm (first-hit throughput),
                           inline trigger + bulk movement
examples/corridor.c        slide through a doorway into a quest zone
```

## Build

```sh
make            # static + shared library
make LTO=1      # release objects suitable for final -flto link
make test       # test suite + exported-symbol check
make benchmark  # dense crowd + sweep storm
make benchmark-batch-move  # inline trigger + bulk movement gate
make benchmark-phase5-batch-simd  # exact scalar vs rejected AVX2 candidate
make example    # corridor walkthrough
make sanitize   # test suite under ASan/UBSan
```

## Sixty-second tour

```c
dc_world *world;
dc_world_create(&world, &(dc_world_config) { .cell_size = 256 });

dc_static_add_aabb(world, wall_min, wall_max, LAYER_WORLD, 0, &wall);
dc_trigger_add_aabb(world, zone_min, zone_max, LAYER_PLAYERS, quest_id,
    &zone);
dc_body_add(world, entity_id, spawn, radius, LAYER_PLAYERS, 0);

/* per tick, per movement intent: */
dc_world_begin_tick(world, tick);
dc_validate_move(world, entity_id, intent_target, LAYER_WORLD,
    DC_MOVE_SLIDE | DC_MOVE_COMMIT, &move);
ds_entity_move(sim, entity_id, move.final_position.x,
    move.final_position.y);          /* feed libdense_sim */

dc_world_trigger_events(world, &events, &count);  /* quest zones etc. */
```

Bounded batches use the same authoritative ordering:

```c
dc_move_request requests[request_count];
dc_move_many_result results[request_count];

uint32_t processed = dc_world_move_many(
    world,
    requests,
    results,
    request_count,
    nav_phase_move_budget
);
```

Call again with `requests + processed` for deferred work. Individual failures
are reported in `results[i].status` and do not stop later requests.

Input limits (validated, `DC_ERR_RANGE` beyond): coordinates within
+-2^28, move deltas within +-2^26 per axis, radii up to 2^20.

## License

Same licensing model as the Dense repository (see the dense repo's
LICENSE.md / COMMERCIAL-LICENSE.md pairing); final license text to be
settled before the first tagged release.
