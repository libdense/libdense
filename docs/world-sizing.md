# Whole-world sizing audit

## Previous dense navigation costs

The former implementation allocated by logical map area:

| Structure | Bytes per tile | 4,096² | 65,535² |
|---|---:|---:|---:|
| Grid costs | 1 | 16 MiB | 4.00 GB |
| Pathfinder g/parent/stamp/closed | 13 | 208 MiB | 55.83 GB |
| Flow distance/direction/settled | 6 | 96 MiB | 25.77 GB |
| One grid + one finder + one flow | 20 | 320 MiB | 85.90 GB |

That is before heaps, paths, caches, multiple agent-radius grids, or
multiple simultaneous flow fields. It is unsuitable for a continent.

## Current sparse navigation backing

`dnav_grid` now uses 32x32 chunks. A missing chunk means every tile has
default cost 1. A chunk allocates only when at least one tile differs
and is reclaimed when its last modified tile returns to default.

Pathfinder and flow nodes are open-addressed tables containing only
visited/reached tiles. Their memory scales with the search spill, not
with logical world dimensions.

The maximum-world unit test creates a 65,535x65,535 logical grid:

- zero chunks at creation;
- two far-apart modified tiles allocate two chunks and stay below
  64 KiB total grid backing in the test configuration;
- a local A* route and a 128-expansion flow slice each stay below
  256 KiB workspace.

Use these APIs in startup telemetry and load tests:

```text
dnav_grid_get_memory_stats
dnav_pathfinder_get_memory_stats
dnav_flow_field_get_memory_stats
dnav_grid_estimate_dense_bytes
```

Sparse storage is not a license to run a continent-wide unbounded flow
spill. Bound expansions and partition gameplay work by active regions.

## Collision broadphase

Collision already uses an occupied-cell hash grid. Empty world space
allocates no cells. The new APIs expose the proof at runtime:

```text
dc_world_get_memory_stats
dc_world_estimate_aabb_cells
```

For an inclusive AABB and cell size `C`, the touched-cell count is:

```text
(floor(max_x / C) - floor(min_x / C) + 1)
* (floor(max_y / C) - floor(min_y / C) + 1)
```

Large static shapes can therefore be expensive even in a sparse grid.
Use the estimator during map import, split pathological continent-sized
AABBs, and report occupied cells/membership nodes after boot.

## Simulation chunk and capacity audit

`libdense_sim` uses hash-backed cell and chunk buckets allocated only for
occupied coordinates. When the final member leaves a bucket, the coordinate
entry is removed and its slot is returned to the retained free list. Logical
world extent therefore does not allocate a whole-world cell or chunk array.

Entity, observer, subscription, dirty, crossing, fanout, and kinetic storage
scale with retained object/work capacity rather than coordinate-domain size.
Use the public capacity snapshot and process allocation counters in startup and
load telemetry:

```text
ds_world_get_memory_stats
ds_allocation_get_metrics
```

The Phase 1 family gate prewarms representative movement, crossing, observer,
dirty, and fanout work, resets the allocation counters, and verifies no retained
bytes or allocation counts grow during the measured ticks.

A complete world-sizing report should include:

```text
logical map extent
active simulation cell/chunk buckets
occupied collision cells and membership nodes
allocated navigation cost chunks
entity, observer, subscription, and fanout capacities
current and peak retained bytes for every module
post-prewarm allocation counts
```

Sparse storage does not make work unbounded. Query radii, path expansions,
flow-field slices, fanout output, and flush work must still run under named
scheduler budgets.
