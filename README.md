<p align="center">
  <img src="logo.png" alt="Dense logo" width="800">
</p>

# Dense | Current release: 0.4.4

**A high-density multiplayer server library that support up to 600,000 players in a single zone, on a single i9 CPU core.**

Dense is a library family for deterministic multiplayer systems that must remain efficient when large numbers of players, NPCs, projectiles, timers, paths, collision bodies, and replication recipients gather in the same area.

Dense is not a complete game server. It provides explicit modules for simulation, transport, scheduling, collision, navigation, AI, and durable state. Public module boundaries remain separate.

This release repository contains Linux x86-64 shared and static libraries,
CPython 3.11-3.14 wheels, public C headers, documentation, and full Python,
C++, and Rust wrapper source. Core C implementation source is not included.

## Modules

| Module | Responsibility |
|---|---|
| `libdense_sim` | Entity lifecycle, validated positions, spatial membership, dirty state, canonical fanout views, and optional recipient worksets |
| `libdense_net` | Sessions, transport, reliability, queueing, and replication transport that consumes simulation fanout |
| `libdense_sched` | Tick phases, timing wheel, budgets, fairness, token buckets, and the shared overload ladder |
| `libdense_collision` | Authoritative integer collision, broadphase, movement validation, queries, and triggers |
| `libdense_nav` | Sparse navigation grids, deterministic paths, flow fields, caches, and collision rasterization |
| `libdense_ai` | Deterministic agent memory, behavior trees, scheduler-sliced execution, and intent production |
| `densedb` | Single-writer state tables, WATCH views, WAL durability, snapshots, and recovery |

## Authority and dependency direction

```text
dense_core (private, statically merged into each library)
    ^
    |-- libdense_sim
    |-- libdense_net
    |-- libdense_sched
    |-- libdense_collision
    |-- libdense_nav
    |-- libdense_ai
    `-- DenseDB
```

The intended server pipeline is:

```text
input
  -> validate
  -> nav proposal
  -> collision validation
  -> simulation commit
  -> combat / AI intent processing
  -> simulation fanout
  -> network flush
  -> DenseDB flush seam
```

`libdense_sim` is the sole authority for replication grouping. `libdense_net`
consumes borrowed fanout views and does not independently scan entities or
decide visibility groups. Optional recipient worksets derive from the same
finalized chunk/type membership. AI produces intents rather than mutating
authoritative game state directly.

## Benchmarks

Measured on an AMD Ryzen 5 5600X running Linux with libdense 0.4.4.
Component results come from `make benchmark-full`. The moving all-to-all
simulation was measured separately, pinned to one logical CPU, with 1,000
ticks per population and execution path.

### libdense_sim

| Benchmark | Result |
|---|---:|
| Spawn with spatial insertion (1M entities) | 5,546,030 entities/s |
| Entity lookup (1M entities) | 45,441,495 lookups/s |
| Same-cell movement (100k entities) | 50,550,274 moves/s; 1.978 ms mean |
| Chunk-boundary thrash (20M crossings) | 15,078,170 moves/s; 6.632 ms mean |
| Dirty mark, public first-mark (1M entities) | 35,448,508 marks/s |
| Dirty mark, direct slot | 340,932,582 marks/s |
| Next-tick dirty reset | 944,862,250 entities/s |
| Recipient workset, source enqueue (100 visible of 2,000 recipients) | 2,493.696 ns/source; zero steady-state growth |
| Same source with a full recipient scan | 12,445.540 ns/source |
| Stable kinetic plans (2,000 entities), relative to sampled core/public paths | 0.67x / 0.51x time |

| Tick workload | Mean (ms) | p50 (ms) | p95 (ms) | p99 (ms) |
|---|---:|---:|---:|---:|
| Shared fanout: 1,000 entities and 1,000 observers | 0.019 | 0.018 | 0.019 | 0.040 |
| 8-way type-mask fragmentation: 1,000 entities and 1,000 observers | 0.027 | 0.027 | 0.027 | 0.046 |
| 10,000 observers, same coverage | 0.156 | 0.151 | 0.168 | 0.452 |
| 10,000 observers, boundary shift | 3.139 | 3.154 | 3.238 | 3.322 |

The shared fanout plan contains 1,000 entries and 1,000 subscriber references
representing 1,000,000 logical entity deliveries. Its timer includes tick
initialization, direct-slot dirty marking and plan finalization. Network
transmission is measured separately.

#### Moving all-to-all simulation

Every player is a moving entity with an anchored radius-40 observer in a
24x24 box. Players remain mutually visible while crossing cells and chunks;
approximately 87% of entities are dirty each tick. Timing includes tick
initialization, movement, dirty marking, subscription maintenance and fanout
construction.

Public-ID C API path:

| Players | Mean (ms) | p50 (ms) | p95 (ms) | p99 (ms) | Worst (ms) |
|---|---:|---:|---:|---:|---:|
| 10,000 | 0.942 | 0.934 | 1.003 | 1.082 | 1.602 |
| 30,000 | 3.540 | 3.495 | 4.017 | 4.305 | 5.363 |
| 100,000 | 17.970 | 17.754 | 20.411 | 21.453 | 23.169 |
| 200,000 | 41.852 | 42.066 | 45.012 | 48.024 | 50.370 |
| 300,000 | 65.148 | 64.095 | 75.350 | 90.190 | 99.116 |
| 500,000 | 111.824 | 111.454 | 121.537 | 132.341 | 156.939 |

Internal known-slot path, bypassing public entity-ID lookups:

| Players | Mean (ms) | p99 (ms) | Worst (ms) |
|---|---:|---:|---:|
| 10,000 | 0.778 | 1.106 | 1.180 |
| 30,000 | 2.803 | 4.178 | 4.331 |
| 100,000 | 14.141 | 15.473 | 16.025 |
| 200,000 | 31.782 | 35.056 | 38.537 |
| 300,000 | 50.287 | 64.661 | 71.462 |
| 500,000 | 87.744 | 110.624 | 123.228 |

For a **100 ms simulation tick budget (10 Hz)**:

| Simulation path | Estimated population at 100 ms mean | Estimated population at 100 ms p99 |
|---|---:|---:|
| Public-ID C API | ~449,000 | ~347,000 |
| Internal known-slot | ~565,000 | ~454,000 |

**~450,000 is the largest tested public-API population whose worst recorded
tick remained below 100 ms. An i9-14900KS would bench about ~630k based on single-core performance scores.**

### libdense_net

| Benchmark | Result |
|---|---:|
| Replication publish, encode + references (32,768 recipient-frames/tick) | 0.007 ms/tick; 0.2 ns/recipient-frame |
| Session flush | 0.215 ms/tick; 6.6 ns/recipient-frame |
| Per-recipient baseline, send + flush + receive | 1.432 ms/tick |
| Shared publish + flush vs per-recipient baseline | 6.45x fixture timing ratio |
| Shared publish + flush throughput, corrected frame count | ~147.7 M recipient-frames/s |
| Session lookup (5,000 sessions) | 4.74 ns/op |
| Flush-all (5,000 sessions) | 150.68 ns/session |
| Adverse soak: 20% drop, 8% duplicate, 12% reorder | 20,000/20,000 reliable commands delivered in order |

The replication fixture uses in-process loopback transport with 256 entities
and 128 sessions: 32,768 recipient-frames per tick per path. Its printed
throughput counts frames from both paths against only shared-path time;
the throughput above corrects that factor-of-two reporting error. The 6.45x
ratio retains the fixture's timing boundaries: the per-recipient baseline
includes receiver draining, while shared publish + flush excludes it.

### libdense_sched

| Benchmark | Result |
|---|---:|
| Timer schedule (1M timers) | 6.5 ns/op |
| Timer cancel | 18.2 ns/op |
| Advance + fire | 108.1 ns/fired timer |
| Named 7-phase pipeline telemetry | 94.05 ns/tick; 57.53 ns overhead |
| Overload scenario (18 ms offered, 10 ms budget) | 14.25 ms worst tick; 13 escalations and 13 recoveries |

### libdense_collision

| Benchmark | Result |
|---|---:|
| Validated move, slide + bodies + commit + triggers (2,000-body crowd) | 196 ns/move; 5.11 M moves/s |
| Swept circle against 400 statics and 1,500 bodies, 46.7% hit rate | 656 ns/sweep; 1.52 M sweeps/s |

### libdense_nav

| Benchmark | Result |
|---|---:|
| Raw A* (256x256, about 22% walls) | 1,121.5 us/path |
| Cached path reuse, 96.2% hit rate | 44.1 us/path |
| Close requests: line of sight / full A* | 35.448 / 772.075 ns/request |
| Bounded repath lane, 300 requests | At most 2,048 expansions/tick across 2,571 ticks |
| Long route, 512x512 world: hierarchy / full tile A* | 0.015 / 54.134 ms |
| Hierarchical route work | 59 local + 14 graph expansions; 221,817 full tile expansions |
| Flow-field build, 256x256 | 15.53 ms |
| Flow steering, 10,000 agents over 100 ticks | 7.1 ns/agent-step |

### libdense_ai

| Benchmark | Result |
|---|---:|
| Perceive + threat + tree + intent, 10,000 agents sharing one tree | 67 ns/agent-tick |
| Whole-horde tick | 0.67 ms |
| Scheduler slices of 250 to 10,000 agents, 1M total agent-ticks | 6.50-6.53 ms total |

### Historical results

[Earlier benchmark tables and charts](docs/BENCHMARKS-2026-07-28.md) retain the
integrated authority loop, DenseDB, overload-policy, and dense-region scaling
measurements. Those workloads were not included in the 2026-09-11 run.

## Package layout

```text
libdense/
|-- README.md, CHANGELOG.md, VERSION, MANIFEST.md
|-- LICENSE.md, COMMERCIAL-LICENSE.md, SECURITY.md, SUPPORT.md
|-- install.sh, uninstall.sh, verify-release.sh, SHA256SUMS
|-- include/dense/          public C headers (9)
|-- lib/linux-x86_64/       Linux x86-64 shared + static libraries (7)
|-- pkgconfig/              pkg-config templates
|-- bindings/               Python, C++, and Rust wrappers (full source)
|-- docs/                   documentation and per-module references
|-- release/                ABI/API snapshots and benchmark records
`-- densebench/             benchmark charts
```

## Install

Verify the extracted release archive and install:

```bash
./verify-release.sh
sudo ./install.sh
```

The archive verifier excludes Git metadata. In a Git checkout, export the
release with `git archive` before running it. The shared-library bundle
requires Linux x86-64 with glibc 2.33 or newer.

Selected options (see `docs/INSTALLATION.md` for staging and packaging):

```bash
sudo ./install.sh --prefix /opt/dense
./install.sh --prefix /usr --destdir "$PWD/stage"
sudo ./uninstall.sh --prefix /opt/dense
```

Link with pkg-config:

```bash
pkg-config --cflags --libs libdense_sim
pkg-config --cflags --libs libdense_net
pkg-config --cflags --libs libdensedb
```

## Bindings

Binding source ships in full under `bindings/`; see `docs/BINDINGS.md`.

- **Python**: CPython 3.11-3.14 wheels for Linux x86-64, statically
  containing `libdense_sim`.
  `python3.14 -m pip install bindings/python/dist/*cp314*.whl`
- **C++**: header-only C++20 wrapper; `make -C bindings/cpp test`
- **Rust**: dependency-free wrapper crate; `make -C bindings/rust test`

## Determinism and memory policy

Dense targets a deterministic server as a function of initial state, ordered
inputs, configuration, and library versions. The family is gated on a
240-tick record/replay harness with per-tick checksums pinned across `-O3`
and ASan/UBSan builds, plus lifecycle and raw-frame audit logging
(`docs/determinism.md`, `docs/replay-and-audit.md`).

Live tick paths are prewarmed and retain high-water memory. A post-prewarm
growth operation is treated as a steady-state allocation and must be
observable in metrics and standing tests (`docs/performance-policy.md`).

## Documentation

Start at `docs/README.md`. Highlights:

- `docs/INSTALLATION.md` - native install, staging, and linker setup
- `docs/mmo-integration.md` - assembling the modules into one server loop
- `docs/architecture.md` - module boundaries and the authority pipeline
- `docs/modules/` - per-library reference notes
- `docs/BENCHMARK-SCOPE.md` - retained performance claims and exclusions
- `docs/PLATFORM-COMPATIBILITY.md` - supported platform, role, and ABI details

## License

See `LICENSE.md`, `COMMERCIAL-LICENSE.md`, and `SECURITY.md`. Binary
redistribution remains subject to the distribution conditions in
`LICENSE.md`.
