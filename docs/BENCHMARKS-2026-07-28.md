# Historical benchmarks - 2026-07-28


These historical tables summarize the full benchmark run recorded on
2026-07-28 on an AMD Ryzen 5 5600X (Linux x86-64, release `-O3` build).
The raw log ships in this package at
`release/benchmarks/benchmark_full_Ryzen_5600x_7.28.26.txt`, and
`docs/BENCHMARK-SCOPE.md` defines which claims are retained. Benchmarks are
single-threaded unless stated; Dense targets a deterministic single-writer
tick.

<p align="center">
  <img src="../densebench/densescalingticktime.png" alt="Dense scaling tick time" width="620">
</p>
<p align="center">
  <img src="../densebench/densescalingperplayer.png" alt="Dense per-player scaling" width="620">
</p>

### Integrated authority loop

The deterministic 240-tick authority harness runs every module in one
pipeline (input, validate, spatial, combat, AI, fanout, flush) with
record/replay checksums pinned across release and ASan/UBSan builds.

| Tick latency | Time |
|---|---:|
| p50 | 14.3 us |
| p95 | 20.9 us |
| p99 | 56.7 us |
| max | 248.1 us |

Representative simulation gate scenarios (`release/benchmarks/rc-gate.txt`):

| Scenario | Tick time |
|---|---:|
| 1,000 entities, dense shared cell | 0.207 ms |
| Town: 100,000 entities, 1,000 observers | 0.112 ms |
| 1,000 observers crossing chunk boundary | 4.183 ms |
| 64-way type-mask fragmentation | 0.813 ms |

### libdense_sim

| Benchmark | Result |
|---|---:|
| Spawn with spatial insertion (1M entities) | 4.72 M entities/s |
| Entity lookup (1M entities) | 48.5 M lookups/s |
| Same-cell movement (100k entities) | 39.4 M moves/s |
| Chunk-boundary thrash (20M crossings) | 11.8 M moves/s |
| Dirty mark, public first-mark (1M entities) | 29.9 M marks/s |
| Dirty mark, direct slot | 278.5 M marks/s |
| Next-tick dirty reset | 772.9 M entities/s |
| Fanout plan, shared recipient set (1M implied deliveries/tick) | 0.055 ms/tick |
| Observer boundary shift (10k observers, 12 chunk edges each) | 5.14 ms/tick |
| Kinetic motion, stable plans vs sampled baseline | 0.57x cost |

### libdense_net

| Benchmark | Result |
|---|---:|
| Replication publish (encode + refs, 65,536 recipient-frames/tick) | 10.7 ns/frame |
| Session flush | 12.7 ns/frame |
| Shared fanout vs per-recipient naive sends | 1.66x faster |
| Implied deliveries | 85.3 M/s |
| Session lookup (5,000 sessions) | 5.9 ns/op |
| Flush-all visit cost (5,000 sessions) | 118.7 ns/session |
| Batch frame encode vs scalar | 1.27x faster |
| Adverse soak: 20% drop, 8% dup, 12% reorder | 20,000/20,000 reliable commands in order |

### libdense_sched

| Benchmark | Result |
|---|---:|
| Timer schedule (1M timers) | 8.1 ns/op |
| Timer cancel | 12.3 ns/op |
| Advance + fire | 66.0 ns/fired timer |
| Named 7-phase pipeline telemetry | 99.4 ns/tick |
| Overload scenario (18 ms offered vs 10 ms budget) | worst tick 14.25 ms, 13/13 escalations recovered |

### libdense_collision

| Benchmark | Result |
|---|---:|
| Validated move (slide + bodies + commit + triggers, 2,000-body crowd) | 241 ns/move (4.14 M moves/s) |
| Swept circle vs 400 statics + 1,500 bodies, 46.7% hit rate | 772 ns/sweep (1.30 M sweeps/s) |

### libdense_nav

| Benchmark | Result |
|---|---:|
| Raw A* (256x256, ~22% walls) | 1,368 us/path |
| Cached dense path reuse (96.2% hit rate) | 53.7 us/path |
| Line of sight vs full A* on close requests | 29.3 ns vs 880.6 ns (30.0x) |
| Bounded 300-request repath lane vs unbounded drain | 2,570x smaller peak burst |
| Hierarchical long route (512x512) vs full tile A* | 0.017 ms vs 63.9 ms (3,759x fewer tile expansions) |
| Flow-field crowd steering (10k agents) | 9.1 ns/agent-step |
| Batch APIs (cost, LOS, flow sample) | 1.07-1.14x vs scalar |

### libdense_ai

| Benchmark | Result |
|---|---:|
| Horde: perceive + threat + tree + intent (10k agents, one tree) | 83 ns/agent-tick |
| Whole-horde tick (10k agents) | 0.83 ms |
| Scheduler slicing (250 to 10,000-agent slices) | equal total cost across slice sizes |
| Batch condition checks vs scalar | 1.48x faster |

### DenseDB

| Benchmark | Result |
|---|---:|
| Direct hp SoA column scan (100k rows) | 0.032 ms |
| Vitals u16 column update (100k rows) | 13.97 ms mean |
| WATCH churn finalization (120,000 deltas/tick) | 6.21 ms/tick |
| WAL commit, no sync (1,000 updates/tick) | 0.149 ms mean |
| Write-behind seal vs synchronous end-tick | 38.7 us vs 617.3 us (15.96x) |
| Snapshot + WAL recovery (10k rows, 100 update ticks) | 124.5 ms |

### Overload ladder

One tuned controller drives input admission, replication volume, AI agent
budgets, navigation expansion, region admission, database flush, and the
emergency tick period through the deterministic authority loop
(`release/benchmarks/overload-tuning.txt`):

| State | Enter/recover (ms) | Input/tick | Repl KiB/tick | AI agents | Nav expansions | Tick period |
|---|---:|---:|---:|---:|---:|---:|
| normal | - | 2,000 | 3,906 | 10,000 | 200,000 | 50.0 ms |
| elevated | 16/12 | 2,000 | 3,320 | 7,500 | 150,000 | 50.0 ms |
| high | 18/14 | 1,500 | 2,344 | 5,000 | 100,000 | 50.0 ms |
| critical | 20/16 | 1,000 | 1,367 | 2,500 | 50,000 | 50.0 ms |
| emergency | 25/18 | 500 | 781 | 1,000 | 20,000 | 62.5 ms |

The controller itself costs 52.9 ns/observation. Under a scripted 3x
overload the ladder escalates in 6 ticks, recovers in order, and returns to
normal without state flapping.

<p align="center">
  <img src="../densebench/dense-bench-scenario.png" alt="Dense benchmark scenario" width="620">
</p>

