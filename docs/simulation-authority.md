# Simulation Authority Contract

Status: adopted for the single-process, single-shard server integration.

The authoritative mutation order is:

```text
input proposes movement
AI emits intent only
navigation proposes a route or movement target
collision validates the proposed movement
libdense_sim commits the validated position
libdense_sim updates membership, dirty state, and fanout
libdense_net transports the finalized fanout view
DenseDB seals simulation-thread writes at tick close
```

## Ownership boundaries

- `libdense_ai` owns behavior execution, percept/threat state, and intent output.
  It does not mutate simulation, collision, navigation, networking, or database
  state directly.
- `libdense_nav` owns plans, flow fields, LOS, route graphs, and budgeted search
  work. A path is a plan, never permission to move.
- `libdense_collision` owns validation of proposed motion and trigger state. It
  does not commit entity membership to `libdense_sim`.
- `libdense_sim` is the sole authority for committed entity position, chunk
  membership, dirty state, lifecycle, and canonical replication fanout.
- `libdense_net` consumes the borrowed finalized `ds_fanout_view`; it must not
  scan the world or independently regroup visibility.
- DenseDB has one logical writer: the simulation thread. Durable bytes may be
  handed to one I/O consumer only after the active transaction has been sealed
  into an immutable pending buffer.

## Review rules

A system that mutates authoritative game state outside the order above is a
review failure. New cross-library includes are checked by
`tools/check_phase4_authority.py`, which is part of `make test` and `make lint`.

The game/server glue is responsible for translating game-defined AI intents and
navigation proposals into collision requests, then translating validated
collision results into `ds_entity_move()` calls. Neither the AI nor navigation
library receives a direct simulation-world pointer.
