# Changelog

## 0.2.0 — 2026-07-29

Dense 0.2.0 completes the MMO stack: `libdense_net`, `libdense_sched`,
`libdense_collision`, `libdense_nav`, and `libdense_ai` join `libdense_sim`
and DenseDB as prebuilt libraries, with the private `dense_core` primitives
statically merged into each shipped artifact. The development record for the
release follows.

### Phase 8 integration and overload tuning

- Added a per-rung `dsc_overload_controller` with monotonic enter/recovery
  thresholds, independent hysteresis streaks, transition/state telemetry, and
  pressure defined as the greater of tick duration and wall-clock lateness.
- Added the integration-owned overload policy resolver. One
  `dsc_overload_state` now produces input, replication, AI, navigation,
  DenseDB, region-admission, optional-work, and emergency tick-period limits.
- Integrated those limits into the 240-tick record/replay authority harness.
  The scripted load escalates at ticks 63/66/68/69 and recovers at
  129/169/199/219, returning to normal without state flapping.
- Added Phase 8 unit, sanitizer, zero-allocation, source-contract, and stress
  benchmark gates. The intentional expanded deterministic baseline is
  `f9ee2ebe47713aee`; `2ef94655f7a5e0df` remains the Phase 6 historical
  baseline.

### Earlier 0.2.0 development (2026-07-27)

### Phase 7 benchmark and profiling gates

- Added `make benchmark-phase7-profile`, `benchmark-phase7-quick`,
  `benchmark-phase7-full`, and `benchmark-phase7-gate`.
- Extended the deterministic authority harness with an observational `--profile`
  mode that records actual per-phase wall time, budgets, completed/deferred
  work, fanout, network, collision, AI, DenseDB, and checksum data without
  feeding timing back into deterministic state.
- Added JSONL tick output plus JSON/Markdown p50, p95, p99, maximum, budget,
  busiest-phase, slowest-tick, and aggregate-work reports.
- Added a catalog-driven quick/full benchmark runner that captures independent
  logs and machine metadata while covering every required Phase 7 network,
  collision, navigation, AI, scheduler, database, replay, and batch workload.
- Added `make test-phase7-audit` to enforce workload coverage, profile schema,
  root targets, phase documentation, and generated-profile structural gates.
- Added aggregate retained/peak bytes, live allocation objects, growth/failure
  counters, and profile-window allocation observations to integrated tick
  records and summaries; the dedicated post-prewarm zero-allocation gate remains
  authoritative.
- Final verification completed 8/8 quick and 18/18 full scenarios. The recorded
  240-tick profile had zero budget violations, 205.875 microsecond p99 latency,
  and `flush` as the busiest phase on 236 ticks; absolute timings remain
  host-specific and are stored with machine metadata.
- Normalized absolute checkout paths out of retained scenario logs so release
  benchmark artifacts do not encode the packager's filesystem layout.
- Fixed the Phase 3 session-map benchmark equivalence checksum so it no longer
  includes allocator-dependent pointer addresses; backend comparisons now use
  deterministic session indices and counts only.

### Repository consolidation

- Merged the Dense family modules into one repository root while preserving separate public APIs, tests, benchmarks, examples, changelogs, versions, and ABI export baselines.
- Added root whole-family and per-module build, test, sanitizer, determinism, benchmark, LTO, install, uninstall, package, format, lint, and ABI targets.
- Added shared build policy in `mk/common.mk` and root architecture/build/install/determinism/performance/dependency documentation.
- Added durable phase changelogs under `docs/phase-changelogs/`.
- Added `docs/PATHFINDING_POLICY.md` as the adopted game-side navigation selection and budgeting policy, while keeping `libdense_nav` policy-free and recording congestion-aware costs as a deferred design question.

### Pre-Phase 2 packaging correction

- Corrected source ZIP creation so timezone-naive archive timestamps cannot be restored several hours in the future when extracted west of the packaging host.
- Added deterministic `.tar.gz` and `.zip` source packages with sorted entries, canonical archive roots, normalized ownership and permissions, and a shared historical `SOURCE_DATE_EPOCH`.
- Added `make check-clock-skew`, `make fix-clock-skew`, and `make test-source-package`, plus archive timestamp, exclusion, and byte-for-byte reproducibility verification.

### Phase 6 determinism, replay, and audit

- Added an integration-owned canonical replay format with configuration,
  initial-state, and Dense-family fingerprints; connection lifecycle records;
  session IDs; raw inbound frame bytes; monotonically sequenced records; and
  one checksum-closing record per tick.
- Added replay audit and recovery logic that validates lifecycle ordering,
  connected-session ownership, contiguous checksum-closed ticks, header/record
  checksums, and truncates only to the last complete tick after a torn or
  corrupt append tail.
- Expanded the deterministic harness to cover clock translation, input demux
  and validation, movement proposal, collision validation, simulation commit,
  combat, AI slicing, navigation work, simulation fanout, network sparse flush
  selection, and DenseDB write-behind buffer swapping/bounded flush.
- The runner now compares release record, release replay, and ASan/UBSan replay
  across 240 pinned per-tick checksums. The intentional expanded-harness final
  baseline is `2ef94655f7a5e0df`; the former `85363eafdc0ad3da` value remains
  documented as the pre-Phase-6 harness baseline.
- Added `make test-replay`, `make test-phase6-audit`,
  `make benchmark-phase6-replay`, and replay `dump`, `audit`, and `recover`
  commands, plus permanent replay/audit documentation and benchmark records.

### Phase 5 batch APIs and selective SIMD

- Added bounded scalar-reference batch APIs for collision circle queries,
  raycasts, and exact circle/AABB tests; navigation LOS, sparse grid-cost, and
  flow sampling; AI percept, threat, decay, and condition work; and network
  frame/replication encoding.
- Added exact per-item result, budget, deferred-output, sanitizer, and family
  post-prewarm zero-allocation coverage for the new batch surfaces.
- Evaluated an exact runtime-detected AVX2 gather implementation for the
  collision circle/AABB kernel. It was materially slower than the scalar
  reference on the representative one-million-test workload and remains
  comparison-only; production dispatch stays portable scalar.
- Added `make benchmark-phase5-batches` and
  `make benchmark-phase5-collision-simd`, plus a permanent benchmark record
  explaining why no new SIMD path was merged.

### Phase 4 library-specific structural improvements

- Added a standing simulation-authority audit and contract: AI emits intents, navigation proposes, collision validates, `libdense_sim` alone commits position/membership/dirty/fanout state, networking consumes finalized fanout, and DenseDB remains single-writer.
- Added `DDB_DURABILITY_WRITE_BEHIND`, retained double-buffer WAL sealing, explicit durability prewarm, bounded pending flush work, pending/backpressure telemetry, and source-compatible synchronous default behavior.
- Added real `SIGKILL` recovery tests for fully synced committed ticks and partially written uncommitted tails.
- Added `make benchmark-phase4-densedb-write-behind`; the recorded 1,000-update workload reduced simulation-thread seal time from 590.77 microseconds to 32.41 microseconds while moving bounded WAL work to the flush path.

- Replaced `dn_session_set_flush_all()`'s dense registered-session scan with a retained `dense_core` indexed dirty activity set and ready-session FIFO. Reliable/ack, sequenced/unreliable, retransmit, timed, due, and selected work remain distinct and measurable.
- Added exact tail-requeue round-robin fairness under global byte, per-session byte, and maximum-session budgets. Future retransmit and keepalive work is promoted only when due; receive timeouts remain visible through `dn_session_set_next_due_ns()` without entering the outbound queue.
- Added active-session removal and dense-slot relocation handling so queued work remains correct during registry churn; destroying a still-bound session now defensively detaches it from its non-owning set.
- Extended session-set memory snapshots with dirty-set/queue capacities and added `dn_session_set_get_activity_stats()` for ready/category counts, queue depth, selected sessions, skipped idle sessions, and flush-pass totals.
- Added `make benchmark-phase4-network-sparse-flush`. Recorded verification improved 1%-active workloads by approximately 33–44x and a 10%-active workload by approximately 1.9x while preserving exact 4,096-session bounded-pass fairness. The 100%-active case is explicitly recorded as approximately 0.52x and remains observable for a future measured hybrid threshold.
- Privately embedded the required `dense_core` dirty-set implementation in `libdense_net` while retaining hidden symbols and no installed `dense_core` artifact.

- Replaced per-slot reliable sent/reorder `realloc` buffers with one retained
  fixed-size packet pool shared by outbound retransmit retention and inbound
  out-of-order holding. Reliable packets are now built directly in their
  retained pool buffer and recycled after ACK or ordered delivery.
- Added `dn_session_prewarm()` for explicit per-lane owned-frame reservation and
  reliable packet-buffer high-water sizing. Extended session memory telemetry
  with packet-pool capacity, in-use, high-water, retained-byte, lifetime-growth,
  and post-prewarm-growth fields.
- Added `make benchmark-phase4-network-packet-storage`. Recorded first-burst
  workloads eliminated all timed allocations and improved by approximately
  1.3x, while 100,000 steady acquire/ACK/release cycles remained
  allocation-free.
- Added `dsc_server_pipeline`, a strict allocation-free wrapper that requires
  input, validate, spatial, combat, AI, fanout, and flush exactly once and in
  order for every tick.
- Added explicit per-phase budgets plus last/EWMA/high-water consumed time,
  completed/deferred work totals and high-water values, exhaustion counts,
  unaccounted time, and busiest-phase telemetry.
- Normalized the shared overload state to `normal`, `elevated`, `high`,
  `critical`, and `emergency`, retaining deprecated source aliases for the old
  policy-specific rung names.
- Migrated the cross-library determinism harness and tick metrics adapter to the
  strict scheduler pipeline without changing the pinned checksum. Added
  `make benchmark-phase4-scheduler-policy`; recorded fixed telemetry overhead
  was approximately 77 ns per seven-phase tick on the verification host.
- Replaced per-body trigger-membership heap vectors in `libdense_collision`
  with an eight-entry inline sorted set and retained overflow storage for larger
  sets. Added memory telemetry distinguishing active inline memberships,
  overflow memberships, retained overflow capacity, and overflow bytes.
- Added `dc_world_move_many()` for deterministic input-ordered movement batches
  under a request-count work budget. Each processed request reports an
  independent status and move result, while unprocessed entries remain
  untouched and deferred work is recorded in world statistics.
- Made trigger recomputation and trigger/body removal reserve required event and
  membership capacity before emitting changes, preventing partial trigger-event
  application on growth failure.
- Added exact scalar-versus-batch equivalence, inline/overflow transition,
  bounded-budget, trigger-removal, sanitizer, and post-prewarm zero-allocation
  coverage plus `make benchmark-phase4-collision-batch`.
- Exposed corner-safe `dnav_line_of_sight()` and added resumable A* begin/continue
  APIs while preserving the one-shot `dnav_path_find()` contract.
- Added a retained fixed-capacity navigation work queue that meters cached A*
  and resumable flow-field jobs through one global expansion budget, with
  generation IDs, cancellation, completion records, cache-hit/work telemetry,
  and post-prewarm zero-allocation coverage.
- Added a deterministic fixed-capacity route graph for pre-authored region/road
  hierarchy, with finalized CSR adjacency, deterministic shortest-route ties,
  nearest-node lookup, graph-expansion budgets, and memory statistics.
- Added `make benchmark-phase4-navigation`. The recorded direct-LOS workload was
  approximately 25x cheaper than full A*, the bounded 300-request lane capped
  each tick at 2,048 of 5.27 million total expansions, and the hierarchical
  long-route workload reduced local tile expansion by more than three orders of
  magnitude.

- Replaced per-agent AI percept, threat, and cooldown allocations with retained
  world-owned size-class page pools. Added explicit class prewarming, direct
  oversized fallbacks, per-class page/block/high-water telemetry, and retained
  spawn/despawn/tree-rebind reuse.
- Added `dai_world_run_budgeted()` as the authoritative scheduler API. It rejects
  zero caps, reports completed/deferred slots, preserves deterministic round-robin
  fairness, and contributes budget exhaustion/deferred-slot counters. Migrated
  integration, zero-allocation, and determinism workloads to the new contract.
- Evaluated a complete hot/cold agent-array split under exact equivalence and an
  order-balanced nine-round 10,000-agent benchmark. The split improved measured
  workloads by only approximately 1-4% at equal retained memory, so production
  retains the simpler AoS record layout and the split remains comparison-only.
- Added `make benchmark-phase4-ai`. Recorded prewarming removed all 471 timed
  first-spawn pool growth operations in the 10,000-agent workload, retained
  80,000 remove/add operations without allocation, and completed a 512-agent
  scheduler cap in exactly 20 passes.

### Phase 3 targeted map conversions

- Began Phase 3 with the navigation A* node-map merge gate. Added an exact dual-backend equivalence test and representative short, long, unreachable, obstacle-density, expansion-budget, and retained-capacity benchmarks.
- Added private `dc_group_map_get_or_insert()` support for single-probe get-or-initialize workloads.
- Rejected the A* node-map conversion after the group-map backend regressed substantial-search throughput by approximately 6–24% across verification runs, short-search throughput by approximately 50–55%, and total retained pathfinder memory by approximately 4–5% in the verification suite.
- Kept the specialized generation-stamped A* node table as the production backend and isolated the experimental group-map implementation in a comparison-only archive, leaving installed `libdense_nav` libraries free of this rejected dependency.
- Renamed private `dense_core` status constants to the `DC_CORE_*` namespace after the comparison build exposed a C enumerator collision with the established public `libdense_collision` `DC_*` API.
- Added an exact dual-backend flow-field node-map harness covering one-shot and sliced builds, partial-field sampling, multiple goals, unreachable regions, restarts, same-region reuse, shifted-region reuse, and mixed hit/miss sampling.
- Rejected the flow-field node-map conversion: the group-map backend regressed every representative workload, increased ordinary field memory by approximately 46–56%, and retained the union of shifted local regions, producing an approximately 11x memory ratio in that workload.
- Kept the specialized generation-stamped flow node table in production and isolated both rejected navigation map backends in one comparison-only archive.
- Completed the navigation sparse chunk-map gate with exact large-world, mutation, relocation, and churn equivalence tests plus representative sparse, dense-town, mixed-hit, mutation, and create/destroy benchmarks.
- Made `dc_group_map` per-operation probe accounting opt-in, then reran the candidate without diagnostic hot-path overhead. The generic map remained approximately 2–4x slower and was rejected; production sparse-grid storage remains unchanged and comparison-only.
- Completed the collision sparse-cell map gate with exact high-coordinate, rasterization, query, body-movement, trigger, removal, and memory-stat equivalence coverage.
- Rejected the collision cell-map conversion after the group-map backend regressed direct lookup workloads by approximately 84–87%, rapid body relocation by approximately 33%, and static boot rasterization by approximately 48%, with no retained map-memory reduction at the measured capacities. Production collision broadphase storage remains unchanged and comparison-only.
- Completed the collision body-registry gate with exact trusted-id lookup,
  failed lookup, duplicate rejection, erase, stale-id rejection, deterministic
  slot reuse, movement, and query equivalence coverage.
- Rejected the body-registry conversion after the group-map backend regressed
  successful lookup by approximately 86%, failed lookup by approximately 73%,
  direct registry churn by approximately 81%, full body slot-reuse churn by
  approximately 65%, and dense movement by approximately 37%, while retaining
  equal map bytes at the measured capacities. Production body storage remains
  unchanged and comparison-only.
- Completed the AI agent-registry gate with exact trusted-id map operations,
  duplicate and removed-id rejection, deterministic slot reuse, perception and
  threat lookup, tree rebinding, deterministic randomness, intent output, and
  scheduler-slice equivalence coverage.
- Rejected the AI registry conversion after the group-map backend regressed
  successful lookup by approximately 84%, failed lookup by approximately 77%,
  direct registry churn by approximately 83%, mass spawn/despawn by
  approximately 27%, tree rebinding by approximately 52–65%, and 10,000-agent
  full/sliced execution by approximately 50%, with equal retained map bytes.
  Production AI storage remains unchanged and comparison-only.

- Completed the network endpoint-map gate with exact endpoint lookup,
  update, removal, repeated rebinding, and sustained-churn equivalence plus
  1,000/10,000/50,000-peer, hostile-miss, churn, and rebinding benchmarks.
- Rejected the endpoint group-map conversion after normal lookup regressed by
  approximately 70–74% and miss-heavy traffic by approximately 37%; churn and
  rebinding were only tied and retained bytes were equal.
- Retained endpoint-map hardening exposed by the gate: robust deterministic
  per-configuration hash seeding and tombstone-free backshift deletion, which
  prevents fixed-capacity saturation under long-running peer churn.
- Completed the network session-map gate with exact dense relocation,
  duplicate/removal behavior, repeated ID churn, and paired real-session flush
  equivalence plus 1,000/10,000/50,000-session, miss, churn, and
  1%/10%/100%-active flush workloads.
- Rejected the session group-map conversion after lookup regressed by
  approximately 54–76%, misses by approximately 32%, and churn by
  approximately 54%, with equal retained bytes. Active flush was effectively
  unchanged because the ID map is outside the dense-vector flush loop.
- Marked Phase 3 complete. All eight generic map candidates failed their
  representative merge gates, and all comparison backends remain isolated
  from installed production libraries.

### Phase 2 shared performance primitives

- Implemented the private static `dense_core` archive with hidden visibility and root build, test, sanitizer, benchmark, LTO, ABI-leak, and non-installation gates.
- Added a configurable `uint64_t` group map with generic values, context-specific hash/seed policy, tombstones, bounded load, failure-atomic rehash, deterministic iteration, opt-in operation/probe metrics, 16-byte SSE2 probing, and an 8-byte SWAR path.
- Added the indexed dirty set, retained aligned arena, generation slot pool, canonical span hash with scalar/AVX2 equivalence, and retained insertion/radix sorter with deterministic heap fallback.
- Added standalone primitive correctness/failure tests and representative microbenchmarks while leaving all existing module storage implementations intact for Phase 3 old-versus-new workload comparisons.
- Kept `dense_core` out of the installed public SDK and added root checks preventing symbols defined by the private archive from leaking through public shared libraries.

### Phase 1 correctness and allocation work

- Made AI behavior-tree rebinding failure-atomic, with validation and replacement allocation before commit and old-state release after commit.
- Added forced-allocation-failure and repeated-rebind tests that verify the old AI binding remains usable after failure.
- Replaced per-publish network fanout recipient allocation with retained replication-owned scratch, explicit reserve/prewarm operations, high-water/allocation metrics, and a zero-steady-state-allocation assertion.
- Replaced lazy live-thread AVX2 hash dispatch mutation with explicit once-per-process `ds_runtime_init()` initialization and a safe world-creation fallback.
- Added one family-wide allocation telemetry contract to every module, exposing exact current/peak retained bytes, growth operations, live retained heap objects, allocation failures, and resettable post-prewarm allocation counts.
- Added major-structure capacity snapshots for simulation worlds, AI worlds, scheduler timers/lanes, network sessions/session sets/UDP servers/replication, navigation path caches, and DenseDB databases; existing collision and navigation snapshots remain available.
- Added a cross-family integration test that verifies capacity reporting, growth telemetry, and complete retained-memory return to zero after destruction.
- Added representative post-prewarm zero-allocation workloads for simulation, collision, navigation, scheduling, AI, networking, and durable DenseDB operation; `make test-zero-alloc` runs the family gate directly and `make test-zero-alloc-sanitize` rebuilds it under ASan/UBSan.
- Replaced per-record DenseDB WAL serialization buffers with one retained durability scratch buffer, removing live-tick heap growth from durable row updates, movement, watch changes, and schema/row records after prewarming.
- Normalized the merged development version to `0.2.0-dev`.
- Isolated release and ASan/UBSan determinism dependencies into separate build directories, preventing stale sanitizer archives from contaminating normal `make test` runs.
- Sanitized the complete determinism dependency set at `-O1`, and cleared inherited `LD_PRELOAD`/`LD_AUDIT` only for the sanitizer harness so `libasan` initializes first.
- Pinned the Phase 1 deterministic checksum at `85363eafdc0ad3da`; release and sanitizer runs must match both each other and the recorded baseline.
- Completed the active-source and legacy-document assumption audit, correcting obsolete single-peer authority, flush-all, standalone-repository, absent-simulation, and pre-monorepo build guidance. Added `make test-phase1-assumptions` as a standing regression gate.
- Marked Phase 1 complete after normal, sanitizer, zero-allocation, determinism, LTO, ABI/export, and staged-install verification.

### Performance

- `ds_id_map` and `ddb_entity_map` rewritten as SIMD group-probing hash
  tables (Swiss-table layout): 16 control bytes are probed per SSE2
  compare on x86-64 (portable SWAR fallback elsewhere), erase is O(1) via
  tombstones instead of cluster re-insertion, rehash no longer zeroes key
  and value arrays, and key hashing uses a single-multiply Fibonacci mix.
- Recipient/subscriber span hashing now runs four independent
  multiply-xor lanes with an AVX2 implementation selected at runtime via
  CPUID (bit-identical scalar fallback).
- Sorted recipient and subscriber id runs use an insertion/LSD-radix
  hybrid sort (skipping constant bytes) instead of qsort.
- Fanout subscriber-difference merging reserves its worst case once and
  writes directly into scratch, removing per-element capacity checks;
  span equality checks use memcmp.

Measured on the default -O3 build (median of 5 interleaved runs,
Xeon x86-64): fanout fragmentation -33/-36%, dense all-to-all 500
players -27%, entity id lookups -26%, public dirty re-marks -25%,
fanout plan build -24%, observer boundary thrash -14%, DenseDB
appearance updates -26%, WATCH stream finalization -17%, WAL commit
-22%. Public C ABI and exported symbol surface unchanged.

## 0.1.0-rc1 — 2026-07-16

First release candidate for the Dense platform.

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
