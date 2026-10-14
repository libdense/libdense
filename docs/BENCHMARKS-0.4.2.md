# 0.3.8 versus 0.4.2 Benchmarks

Measured on 2026-10-14 on an Intel Xeon Platinum 8573C, pinned to logical CPU 2,
with GCC 13.3, `-O3`, and `-ffp-contract=off`. Both releases use identical
workloads and timing boundaries. Each condition has seven paired runs with
alternating release order and 32 warmup ticks.

## Selected results

Values are medians of per-run mean tick times in microseconds. Negative change
means less time. Long controls contain 10,000 measured ticks per run; the other
rows below contain 2,000.

| Workload | 0.3.8 (us) | 0.4.2 (us) | Time change |
|---|---:|---:|---:|
| Disjoint changes, 50,000 observers | 82.659 | 7.293 | -91.2% |
| Recipient churn, 10,000 observers | 49.780 | 17.898 | -64.0% |
| Recipient churn, 32 types, 10,000 observers | 2090.465 | 533.745 | -74.5% |
| Fragmented crossings, 10,000 observers | 2221.435 | 604.717 | -72.8% |
| Entity recreation, 10,000 observers (long control) | 79.737 | 28.613 | -64.1% |
| Entity recreation, 50,000 observers (long control) | 311.271 | 50.578 | -83.8% |
| Tiny churn, no dirty entities (long control) | 0.468 | 0.534 | +14.1% |
| Stationary, 10,000 observers (long control) | 3.378 | 3.672 | +8.7% |
| Stationary, 32 types (long control) | 9.577 | 10.645 | +11.2% |
| All-to-all, 10,000 entities and observers | 1470.556 | 1470.328 | 0.0% |

The largest improvements occur in repeated recipient changes and entity
recreation. The 10,000-entity all-to-all mean is essentially unchanged; its p99
increases from 2.384 ms to 2.726 ms. Small and stationary controls can regress,
as shown above. These results do not imply a uniform application speedup.

Retained storage in the disjoint 32-type case falls from 2,759,344 to 1,187,736
bytes (57.0%). Some controls retain an additional 1,256 bytes. Allocator-accounted
memory includes retained capacity and is not process RSS.

## Coverage

The 42-case matrix, 12 longer controls, and five all-to-all scales comprise
47 distinct workloads, 59 conditions, 826 processes, and 3,038,000 measured
ticks. All-to-all runs use 5,000 ticks at 100 entities and 2,000 ticks at the
larger scales. Delivery digests and implied delivery counts match between
versions, and allocation balances pass for all runs.

Post-warmup allocation growth is zero in 770 of 826 processes. The remaining
runs cover observer churn, mixed-size crossings, and all-to-all capacity growth.
These are not universal zero-allocation results.

Timings include begin-tick work, mutations, and finalization, excluding setup.
They measure simulation work and grouped fanout, not pair expansion,
serialization, network transmission, or a complete game server. Percentile
values are medians of per-run percentiles. See [benchmark scope](BENCHMARK-SCOPE.md).

## Plates

| Plate | Comparison |
|---|---|
| 1 | [Overview](../densebench/0.4.2/01-dense-0.3.8-vs-0.4.2-overview.png) |
| 2 | [Observer scaling](../densebench/0.4.2/02-dense-observer-scaling.png) |
| 3 | [All-to-all scaling](../densebench/0.4.2/03-dense-all-to-all.png) |
| 4 | [Entity recreation](../densebench/0.4.2/04-dense-entity-recreation.png) |
| 5 | [Retained memory](../densebench/0.4.2/05-dense-retained-memory.png) |
| 6 | [Work counters](../densebench/0.4.2/06-dense-work-counters.png) |
| 7 | [Controls and tail latency](../densebench/0.4.2/07-dense-controls-and-tails.png) |
| 8 | [Complete matrix, part 1](../densebench/0.4.2/08-dense-complete-matrix-1.png) |
| 9 | [Complete matrix, part 2](../densebench/0.4.2/09-dense-complete-matrix-2.png) |
| 10 | [Complete matrix, part 3](../densebench/0.4.2/10-dense-complete-matrix-3.png) |

## Data

- [Complete comparison CSV](../release/benchmarks/0.4.2/comparison.csv)
- [Summary JSON](../release/benchmarks/0.4.2/summary.json)
- [Matrix samples](../release/benchmarks/0.4.2/matrix.jsonl)
- [Long-control samples](../release/benchmarks/0.4.2/controls.jsonl)
- [All-to-all samples](../release/benchmarks/0.4.2/all-to-all.jsonl)
