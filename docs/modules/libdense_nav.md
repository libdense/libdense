# libdense_nav

**Pathfinding, flow fields, path sharing, and navigation data for
high-density multiplayer servers.**

## Phase 5 batch sampling

`dnav_line_of_sight_many()`, `dnav_grid_get_cost_many()`, and
`dnav_flow_sample_many()` submit deterministic input-ordered work under an item
budget. They remain scalar because sparse lookup and branch-heavy line traversal
did not justify SIMD. Run `make benchmark-phase5-batches`.


## Phase 3 navigation map gates

The first three targeted conversions compared the specialized A* node table,
flow-field node table, and sparse grid chunk map with `dc_group_map`. All generic
backends produced exact deterministic results but failed their representative
performance or memory gates, so all three conversions were rejected. Production
builds keep the existing structures; experimental backends are compiled only for
their equivalence tests and `make benchmark-astar-map`, `make benchmark-flow-map`,
and `make benchmark-grid-map`. See the corresponding reports under
`../docs/benchmarks/`.

## Phase 4 hierarchy and global work budget

The library now exposes the mechanisms required by the adopted host policy
without selecting policy itself:

- `dnav_line_of_sight()` for close-visible direct movement;
- resumable A* begin/continue operations;
- `dnav_work_queue` for cached A* and flow builds under one global expansion
  budget;
- `dnav_route_graph` for deterministic pre-authored region/road hierarchy and
  short local tile-search seams.

The queue and route graph are fixed-capacity retained structures and are covered
by the family post-prewarm zero-allocation gate. Selection thresholds, repath
cadence, overload stretching, jitter, and the deferred congestion overlay remain
in `../docs/PATHFINDING_POLICY.md`.

Run `make benchmark-phase4-navigation` for the direct-LOS, 300-request global
budget, and long-distance hierarchy workloads.

## 0.2.0 MMO integration update

- Replaced the dense whole-grid cost array with 32x32 sparse chunks.
  Only chunks containing non-default costs allocate memory, and a chunk
  is released when its last modified tile returns to default.
- Replaced pathfinder and flow-field world-sized scratch arrays with
  open-addressed workspaces proportional to tiles actually visited.
- Raised the logical grid limit to 65,535x65,535 while retaining u32
  deterministic tile indices. Added grid/path/flow memory statistics.
- Cache and flow invalidation still key strictly on the grid version;
  every world mutation must pass through `dnav_grid_set_cost`.
- Added optional `LTO=1` builds for the final release link.

libdense_nav is the navigation module of the Dense ecosystem. It sits
directly on libdense_collision (an optional bridge rasterizes
walkability from the authoritative static world) and directly under
libdense_ai. Standalone, source-available C11, zero dependencies.

The library deliberately does not choose between direct movement, A*, cached
paths, or flow fields. The adopted host-game policy for those choices, repath
cadence, overload shedding, deterministic lateral jitter, and lazy
invalidation is maintained at `../docs/PATHFINDING_POLICY.md`. Congestion-aware
costs remain explicitly unadopted pending an overlay design and measurements.

Navigation cost in an MMO scales with how many agents want paths at
the same time to the same places. The library exposes a hierarchy of
mechanisms implementing the Dense reuse rule:

- **Direct line of sight** (`dnav_line_of_sight`) for close visible movement
  without constructing a path object.
- **A\*** (`dnav_pathfinder`) for the singular question: deterministic,
  resumable under expansion slices, sparse reusable scratch proportional to
  visited tiles, octile costs (10/14) with weighted tiles, no corner cutting,
  and optional line-of-sight smoothing that stays tile-walkable.
- **Flow fields** (`dnav_flow_field`) for the crowd: one budgeted,
  *resumable* multi-source Dijkstra from the goal set; every agent
  then steers with an O(1) sparse-table lookup per tick. Ten or ten thousand
  agents cost the same build.
- **Path sharing** (`dnav_path_cache`) for everything between:
  clustered (start, goal) keys onto refcounted immutable paths, LRU
  bounded, automatically invalidated by the grid version counter.
- **Route hierarchy** (`dnav_route_graph`) for long-distance region, road,
  entrance, and portal routes, leaving only short local seams to tile A*.
- **One global lane** (`dnav_work_queue`) for cached A* and flow-field builds
  under one caller-supplied expansion budget.

The tile-space mechanisms read one navigation data structure: an integer tile grid
(`dnav_grid`) positioned in the same int32 world coordinates
libdense_sim and libdense_collision use, with per-tile costs 1..254
(mud, roads, threat painting) or blocked, and a version counter every
mutation bumps.

Everything is integer-only and deterministic; searches, fields, and
cache behavior replay identically on every platform.

## Layout

```text
include/dense_nav.h                  public ABI
include/dense_nav_collision_bridge.h optional dc_world rasterizer
src/                                 grid, path (A* + LOS), route,
                                     flow, cache, work, collision_bridge
tests/                               Dijkstra-reference fuzz, flow/cache,
                                     hierarchy/work, determinism
benchmarks/                          path/cache, flow crowd, map gates,
                                     hierarchy/global budget
examples/maze.c                      ascii A* path + flow arrows
```

## Build

```sh
make            # static + shared library
make LTO=1      # release objects suitable for final -flto link
make test       # test suite + exported-symbol check
make benchmark  # A*/cache, map gates, and flow crowd
make benchmark-astar-map # Phase 3 old-versus-new A* node map
make benchmark-flow-map  # Phase 3 old-versus-new flow node map
make benchmark-grid-map  # Phase 3 old-versus-new sparse chunk map
make benchmark-phase4-navigation # LOS, global budget, route hierarchy
make benchmark-phase5-batches # LOS, sparse cost, and flow sample batches
make example    # ascii maze
make sanitize   # test suite under ASan/UBSan

# With the libdense_collision rasterizer bridge + its test:
make DENSE_COLLISION_DIR=/path/to/libdense_collision test
```

## Sixty-second tour

```c
dnav_grid *grid;
dnav_grid_create(&grid, &(dnav_grid_config) {
    .origin = { 0, 0 }, .tile_size = 100,
    .width = 256, .height = 256,
});
dnav_rasterize_collision(grid, dc, LAYER_WORLD, NPC_RADIUS);

/* host policy rule 1: no path object when close and visible */
if (dnav_line_of_sight(grid, npc_tile, goal_tile)) {
    emit_direct_intent(npc_tile, goal_tile);
}

/* shared goal: submit one retained flow build to the global nav lane */
dnav_work_queue_submit_flow(work_queue, &flow_request, &work_id);

/* unique goal: cached/resumable A* through the same expansion budget */
dnav_work_queue_submit_path(work_queue, &path_request, &work_id);
dnav_work_queue_drain(work_queue, sched_expansion_budget, 0, &used);

/* long travel: host-authored region/road graph plus local A* seams */
dnav_route_graph_find(route_graph, route_start, route_goal, graph_budget,
    route_nodes, route_capacity, &route_count, &cost, &expansions);
```

The original 0.1 development benchmark used a 256x256 grid and
reported dense-array timings. The 0.2 sparse backing changes the memory
and cache profile, so rerun `make benchmark` on the deployment hardware
before selecting global repath and flow-build budgets.

## License

Same licensing model as the Dense repository (see the dense repo's
LICENSE.md / COMMERCIAL-LICENSE.md pairing); final license text to be
settled before the first tagged release.
