# Changelog

## 0.4.4 - 2026-10-16

- Prepare eligible homogeneous replication groups once.
- Merge workset lifecycle edits once per recipient.
- Reuse source positions during workset enqueue.
- Share immutable recipient-span backing.
- Lazily index longer current/previous type-mask span lists.
- [WIP] Unreleased: compact audiences, queue/storage variants, block replacements.

## 0.4.2 - 2026-10-13

- Copy whole recipient spans and reuse cached hashes for entity removal and
  recreation, preserving separate LEAVE/ENTER lifetimes.
- Recycle cached difference buffers with bounded second-chance eviction.
- Share identities across exactly equal recipient sets.
- Cache recurring cross-chunk recipient differences and reuse forward/reverse
  transitions after a second observation.
- Apply sparse recipient edits with binary searches and bulk copies.

## 0.4.0 - 2026-10-07

- [Experimental] Add Windows x86-64 compatibility-server support and a
  DenseDB Win32 file backend.
- Reuse UPDATE recipient spans across chunk crossings when no recipients leave.
- Reuse resolved fanout groups for non-consecutive UPDATE bundles and coalesce
  consecutive entries sharing the same recipient run.
- Borrow previous recipient spans when observers arrive without departures;
  skip UPDATE exclusion scans when all current recipients receive ENTER.
- Derive same-chunk ENTER/LEAVE recipients from journaled changes and reuse
  cached spans when the event journal has no net change.

## 0.3.8 - 2026-09-11

- Persist sorted simulation recipient spans across ticks with per-chunk
  invalidation and event-based updates.
- Store observer coverage nodes in a torus-indexed array and order lifecycle
  and dirty-update records by shared transition classes.
- Borrow subscriber runs in finalized fanout views.
- Preserve tick-start snapshots when allocation fails so finalization can be
  retried.
- Reject anchored coverage outside signed chunk-coordinate bounds before
  committing a move.
- Check event/index capacity limits and empty-buffer handling.
- Include begin-tick work in full-tick benchmark timings and account for the
  event journal and update-sorting buffers in memory totals.
- Update public headers, API snapshots, wrapper versions, and benchmark
  results.

### Current benchmarks

| Workload | 0.2.0 | 0.3.8 | Time reduction |
|---|---:|---:|---:|
| Spawn 1 million entities | 211.787 ms | 173.823 ms | 17.9% |
| Lookup 1 million entities | 20.637 ms | 16.692 ms | 19.1% |
| Move 100,000 entities within cells | 2.536 ms | 2.001 ms | 21.1% |
| Move 100,000 entities across boundaries | 8.461 ms | 6.381 ms | 24.6% |
| First dirty mark, 1 million entities | 33.452 ms | 24.184 ms | 27.7% |
| Shift 10,000 observers across boundaries | 5.140 ms | 2.932 ms | 43.0% |
| Shared fanout plan | 0.055 ms | 0.027 ms | 50.9% |
| 8-way fragmented fanout plan | 0.094 ms | 0.027 ms | 71.3% |
| Network publish + flush | 0.768 ms | 0.709 ms | 7.7% |
| Session flush, 5,000 sessions | 118.68 ns/session | 56.11 ns/session | 52.7% |
| Crowd collision movement | 241 ns/move | 198 ns/move | 17.8% |
| Collision sweep | 772 ns/sweep | 671 ns/sweep | 13.1% |
| Cached navigation | 53.7 us/path | 44.1 us/path | 17.9% |
| AI, 10,000 agents | 0.83 ms | 0.67 ms | 19.3% |

## 0.3.7 - 2026-09-11

- Replace per-observer/entity membership storage with exact chunk/type
  factors and shared transition differences.
- Share ENTER/LEAVE recipient spans and UPDATE exclusions through fanout
  planning. Recipient worksets retain their explicit per-recipient state.

## 0.3.0 - 2026-08-08

- Added supported portable static SDK builds for Linux x86-64, Linux ARM64,
  and Windows x86-64.
- Added Winsock backend and Windows-safe public export.
- Added CI-built CPython 3.11-3.14 wheels for Linux x86-64, Linux ARM64, and
  Windows x86-64.

## 0.2.0 - 2026-07-29

Dense 0.2.0 completes the MMO stack: `libdense_net`, `libdense_sched`,
`libdense_collision`, `libdense_nav`, and `libdense_ai` join `libdense_sim`
and DenseDB as prebuilt libraries, with the private `dense_core` primitives
statically merged into each shipped artifact. The development record for the
release follows.

### libdense_sim

- Dense entity slots with arbitrary 64-bit IDs.
- Sparse cell/chunk spatial membership and centralized crossings.
- Channel-aware dirty tracking.
- Moving observers and persistent spatial subscriptions.
- Exact grouped fanout plans with borrowed views.
- Selective sampled and kinetic motion backends.
- C ABI plus Python, C++, and Rust wrapper sources.

### DenseDB

- Immutable table schemas and structure-of-arrays channel storage.
- Fixed and entity-following WATCH subscriptions.
- Borrowed SNAPSHOT/ENTER/UPDATE/LEAVE streams.
- Tick-commit WAL, atomic snapshots, and recovery.

### Release-candidate hardening

- Frozen public C header baselines and Linux x86-64 ABI layout baseline.
- Stable shared-library SONAMEs.
- Staged install and pkg-config integration tests.
- Deterministic durability parser mutation fuzzing.
- Filesystem failure injection for WAL and checkpoint paths.
- Durable restart/checkpoint soak testing.
- Architecture benchmark regression gates.
