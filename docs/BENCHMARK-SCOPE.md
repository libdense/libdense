# Benchmark Scope

## Dense-region snapshot

```text
players:             10,000
area:                24 x 24 spatial units
observer radius:     40
observers:           one entity-following observer per player
ticks per run:       100
public C API mean:   4.864 ms
public C API p95:    5.016 ms
public C API p99:    5.850 ms
hardware:            AMD Ryzen 5 5600X
```

At 20 ticks per second, a 4.864 ms mean corresponds to approximately 9.7% of one
CPU core.

## Included work

- entity movement;
- spatial membership maintenance;
- entity-following observer movement;
- visibility and subscription reconciliation;
- dirty-state preservation; and
- grouped recipient-plan construction.

## Excluded work

- gameplay and combat logic;
- NPC AI and general physics;
- packet serialization and compression;
- encryption;
- socket transmission and acknowledgements;
- client rendering; and
- persistence outside the specific DenseDB benchmarks.

The result measures the Dense spatial and replication-planning subsystem, not a
complete 10,000-player server tick.

Retained machine-readable and text results are in `release/benchmarks/` and
`densebench/`.
