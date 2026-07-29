# Module Dependencies

## Private implementation direction

```text
dense_core
    ^
    |-- libdense_sim
    |-- libdense_net
    |-- libdense_sched
    |-- libdense_collision
    |-- libdense_nav
    |-- libdense_ai
    `-- DenseDB
```

No public module may depend on another module's private headers. `dense_core` remains private, static-only, and absent from the installed SDK. Modules may include its private headers only from their own implementation files after a benchmark-gated conversion.

## Supported gameplay seams

- `libdense_sched` controls tick phases, clocks, fairness, and budgets.
- `libdense_ai` emits game intents.
- `libdense_nav` converts intent into movement proposals.
- `libdense_collision` validates proposals.
- `libdense_sim` commits positions and owns lifecycle, membership, dirty state, and fanout.
- `libdense_net` consumes fanout and transports encoded replication.
- DenseDB accepts simulation-thread writes and exposes a bounded flush seam.

## Optional compile-time bridges

- `libdense_net` may compile `dense_net_sim_bridge.h` when `DENSE_SIM_DIR` is supplied.
- `libdense_nav` may compile its collision rasterization bridge when `DENSE_COLLISION_DIR` is supplied.

These are explicit public integration seams, not permission to bypass authority boundaries.
