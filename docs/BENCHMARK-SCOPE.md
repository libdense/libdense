# Benchmark Scope

## 0.4.2 versus 0.4.4

The current public comparison uses the supplied 0.4.2 release binaries and the
frozen 0.4.4 default build. It includes 21 simulation conditions, six explicit
workset departures, six shared network batches, eight complete in-process
pipelines and eight concurrent loopback UDP pipelines. Five alternating pairs
per condition produce 490 measured processes. 0.4.3 was internal and unpublished.

Simulation timing includes begin-tick, mutations and finalization through grouped
fanout. Shared network timing includes encode, publish and packet flushing.
Complete pipeline timing includes simulation and network work. UDP measures real
loopback sends with four receive workers and separate sender/receiver CPU costs;
application decoding, gameplay and Internet behavior are outside its scope.
Workset departure is one process-CPU operation per trial, with setup outside the
timer. It does not provide a p99 estimate.

All steady-state conditions have 32 warmups. Reported means and percentiles are
medians of the per-process statistics, with observed ranges retained in JSON/CSV.
Exact outputs and allocation balances are checked. Tracked library storage,
process RSS and benchmark capture buffers are separate quantities. Disabled
experiments and earlier internal timings do not contribute to release gains.
See [the complete current results](BENCHMARKS-0.4.4.md).

## 0.3.8 versus 0.4.2

The historical comparison measures complete simulation ticks, including begin-tick
maintenance, mutations, and finalization. Setup is excluded. Both releases run
on the same host and logical CPU with matching compiler options. Seven paired
runs per condition alternate release order after 32 warmup ticks.

The matrix has 42 workloads at 2,000 ticks per run, with 12 selected controls
extended to 10,000 ticks. Five all-to-all scales add 100 through 10,000 entities
and observers. Results cover 826 processes and 3,038,000 measured ticks.

Published timing values are medians of the seven per-run statistics. A reported
p99 is the median of per-run p99 values, not a pooled percentile. Retained memory
is allocator-accounted storage, not process RSS. Post-warmup allocation growth
is reported separately; several churn cases still grow.

Grouped implied deliveries do not include pair expansion, serialization,
network transmission, client processing, or gameplay logic. This comparison
does not rerun the other modules' historical benchmarks. See
[the results and plates](BENCHMARKS-0.4.2.md) for measurements and caveats.

## 0.3.8 run

| Field | Value |
|---|---|
| Date | 2026-09-11 |
| Command | `make benchmark` in the implementation repository |
| Optimization | `-O3` |
| C mode | C11, `-ffp-contract=off` |
| CPU and compiler version | Not recorded in the log |
| Results | `release/benchmarks/benchmark-0.3.8-2026-09-11.txt` |
| Summary | `release/benchmarks/BENCHMARK_SUMMARY.json` |

The text record retains benchmark stdout. Each workload reports its own
population, iteration count, and timing unit. The command builds and runs
module benchmarks sequentially; their times must not be added together as a
server tick. The source build commands do not apply to this release repository.

## Simulation timing

The 0.3.8 fanout-plan and observer-subscription tick samples include
`ds_world_begin_tick()`, mutations, and finalization. Begin-tick work includes
recipient-span carry-forward and cache compaction. Entity-store, dirty-channel,
and recipient-workset results measure the operations named by each benchmark.

Movement covers 100,000 entities over 200 ticks. Observer coverage covers
10,000 observers over 100 ticks, radius 40, chunk size 16, and 36 subscriptions
per observer. Fanout planning covers 1,000 entities and 1,000 observers over
100 ticks. The shared case represents 1,000,000 implied deliveries; the
8-way type-mask case represents 125,000. Both produce 1,000 delta entries and
1,000 subscriber references, so their delivery counts are not interchangeable.

The recipient workset enqueues one source visible to 100 of 2,000 recipients,
repeated 2,000 times. The reported zero steady-state growth applies to that
fixture. Kinetic results compare identical trajectories within the same run;
frequent plan replacement includes stale-event processing and can cost more
than sampled movement.

## Network and other modules

The replication test reports 26,214,400 delivered frames over 400 ticks,
or 65,536 recipient-frames per tick. Its shared-versus-naive comparison
includes publishing and flushing. It does not measure production internet
bandwidth or a complete game server. The adverse-channel test uses the stated
simulated drop, duplication, and reordering rates.

Navigation map-comparison rows are experimental backend comparisons; their
speedup column compares the named backends, not 0.3.8 against an earlier
release. Hierarchical pathfinding reports local tile and graph expansions
separately. Flow-field construction and steering have separate timings.

## Historical records

The 2026-09-11 run contains no DenseDB benchmark, integrated authority-loop profile,
dense-all-to-all scaling ladder, town-square, boundary-thrash, or 1..64-mask
fragmentation run. The 8-way fanout-plan case is included. Earlier results
remain in [the dated benchmark tables](BENCHMARKS-2026-07-28.md), dated JSON,
and `densebench/`. Those charts and measurements are historical and are not
0.3.8 results.

No cross-release speedup is inferred from runs with different timing
boundaries, compiler settings, or machines. A grouped plan's implied delivery
count is distinct from serialization, transport, client processing, and
application gameplay work.
