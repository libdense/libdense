# Performance and Allocation Policy

## Measure representative work

Synthetic microbenchmarks are useful for diagnosis, but a structural conversion merges only when a representative workload improves without unacceptable regressions in memory, determinism, initialization, sanitizer behavior, adversarial-key handling, or maintainability.

## Retained memory

Live tick paths should reserve or prewarm storage, retain high-water capacity, and reset logical usage without freeing. Growth must be failure-atomic and overflow checked.

The standing validation pattern is:

```text
prewarm
reset allocation counters
run representative ticks
assert zero steady-state allocations
```

Phase 1 exposes the same process-wide telemetry shape from every module:

```text
current retained bytes
peak retained bytes
growth operation count
live retained heap-object count
allocation failure count
post-prewarm allocation count
```

Module-specific memory snapshots separately report logical live counts and
capacities for major structures. The global counters answer whether memory grew;
the structure snapshots identify where retained capacity lives.

The family integration gate now exercises representative post-prewarm work in
every module: simulation movement/fanout, collision moves and queries, cached A*
and flow sampling, scheduler timers and lanes, AI perception/threat/intent work,
network channel and replication traffic, and durable DenseDB row/watch updates.
`make test-zero-alloc` resets every module counter after warmup and requires both
retained bytes and post-prewarm allocation counts to remain unchanged.

Network sessions provide an explicit prewarm profile rather than requiring
traffic-shaped warmup. Owned frame capacity can be reserved independently for
reliable, sequenced, and unreliable slots, and one shared reliable packet pool
can be sized to expected concurrent outbound in-flight plus inbound reorder
high water. Packet-pool growth after the prewarm boundary is exposed separately
so an undersized operational profile is visible even when global allocations
are aggregated across many sessions.

## SIMD

SIMD work requires:

- a scalar reference implementation;
- identical-result tests;
- process-start runtime feature selection;
- a portable fallback;
- representative benchmark improvement;
- no material small-batch regression.

Do not vectorize work dominated by sparse traversal, gathering, tiny candidate sets, or branches.

## Tick diagnostics

Per-phase records should make this question answerable from one record:

```text
Which phase consumed the tick?
```

## Sparse active work

Registries and work queues are separate concepts. A library may retain a dense
registry for lookup and deterministic iteration while maintaining an indexed
dirty set plus a fair FIFO for work that is actually ready. The Phase 4 network
flush follows this policy: future deadlines are tracked without being ready,
ready peers are selected in FIFO order, and unfinished peers return to the tail.
Metrics must retain both registered and active counts so a dense-versus-sparse
threshold can be based on production density rather than a fixed guess.
