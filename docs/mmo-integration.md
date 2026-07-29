# MMO integration contract

## Integration order

1. Multi-peer UDP demux and session registry.
2. Deterministic one-thread loop skeleton: monotonic clock, named
   phases, budgets, metrics, and overload state.
3. `libdense_sim` plus DenseDB.
4. `libdense_collision`.
5. `libdense_net` replication from the sim fanout view.
6. `libdense_nav`.
7. `libdense_ai`.
8. Tune overload thresholds against real MMO workloads.

Keep the checksum harness green after every step. Thresholds and caps
belong last because they must come from measurement rather than guesses.

## Recommended v1 server loop

One pinned simulation thread owns authoritative state and socket
processing:

```text
monotonic clock -> ticks due
receive-all UDP datagrams
INPUT      decode and rate-limit proposals
VALIDATE   collision validates movement/actions
SPATIAL    commit validated positions to sim/chunk membership/dirty state
COMBAT     resolve authoritative gameplay
AI         consume one global scheduler agent/repath budget
FANOUT     build replication only from libdense_sim fanout views
FLUSH      budgeted session flush and DB handoff
end strict pipeline tick -> one metrics record -> overload observation
```

Do not add an I/O thread for networking until profiling forces it. A
single-thread receive/sim/flush loop preserves the useful property that
the server is a deterministic function of initial state plus input log.

## Movement authority

The order is strict:

1. Input or AI produces a proposed movement intent.
2. `dc_validate_move` validates one move, or `dc_world_move_many` consumes a
   bounded input-ordered prefix for the collision phase, and optionally commits
   collision state.
3. Only the returned validated position is passed to the sim entity
   move operation.
4. Chunk membership and dirty state therefore never observe an invalid
   position.

No game handler may move a sim entity first and repair collision later.

## Replication authority

The sim fanout view is the sole source of replication grouping. Game
systems may choose *what* changed, but must not scan the full entity set
to independently decide *who sees it*. That recreates the full-scan cost
that the spatial subscription kernel exists to remove.

## UDP and sessions

Use `dn_udp_server` for the authority socket:

- endpoint -> session lookup is O(1) average;
- peer slots and inbound queues are bounded;
- one bad peer's protocol error does not stop other peers;
- `dn_udp_server_receive_all` drains nonblocking input before the tick;
- `dn_udp_server_flush_all` fairly advances a rotating subset under
  global, per-session, and session-count budgets.

Apply the shared `dsc_overload_state` to connection token buckets and
replication tiers rather than inventing subsystem-specific load enums.

## Wall-clock policy

The MMO owns monotonic-time translation. Configure one explicit policy:

- **Catch-up:** preserve the nominal schedule and run at most
  `max_catch_up_ticks`; remaining backlog stays visible.
- **Slew:** run one tick and move the next deadline by no more than the
  configured correction.

Feed `max(tick_duration, wall_clock_lateness)` to
`dsc_overload_observe_pressure`. A stall is pressure even when each
catch-up tick itself finishes quickly. Global tick slowdown is the last
valid rung after input, replication, AI, and region shedding.

## Scheduler ownership

Use `dsc_server_pipeline`, not free-form numeric phase IDs, for the authority
loop. Every recurring system runs under exactly one named phase, and every tick
executes input through flush exactly once and in order. Work outside those
phases is a review failure because it cannot answer “which phase ate the tick?”

Each phase records assigned budget, current/EWMA/high-water duration, completed
and deferred work, and budget exhaustion. The integration metrics record also
contains the busiest phase.

All overload-sensitive systems read one `dsc_overload_state`:

| State | Typical policy response |
|---|---|
| Normal | Full AI, nav, replication, and admission quality |
| Elevated | Trim optional path work and distant/cosmetic replication |
| High | Tighten input buckets and reduce AI/nav slice budgets |
| Critical | Apply soft region caps and aggressively defer noncritical work |
| Emergency | Apply hard caps, reject nonessential work, optionally slow ticks |

The reference integration uses `dense_overload_runtime`: measured pressure is
resolved once per tick, and the resulting immutable `dense_overload_limits`
record is passed to input, network flush, AI, navigation, DenseDB, admission,
and clock code. Do not call separate per-subsystem load controllers. The
starter ratios are reviewable data and should be retuned from Phase 7 workload
captures before a production release.

## DenseDB contract

DenseDB follows this integration contract:

- The simulation thread is the only writer.
- Hot gameplay reads use watch views, never ad-hoc row gets.
- Close the simulation tick before sealing durable writes.
- Use `DDB_DURABILITY_WRITE_BEHIND` when tick-close latency must exclude WAL
  I/O. `ddb_database_end_tick()` publishes one immutable pending WAL buffer and
  immediately returns the other retained buffer to the simulation writer.
- Drain pending bytes with `ddb_database_flush_pending_bounded()` inside
  `DSC_PHASE_FLUSH`, or hand that same bounded operation to one I/O consumer.
- If a prior pending tick is still occupied when the next tick reaches its seal
  point, honor `DDB_ERR_BUSY` backpressure; never overwrite or merge the pending
  transaction.
- Keep `DDB_DURABILITY_SYNCHRONOUS` for simpler deployments where end-tick I/O
  is acceptable.
- Checkpoint cadence defines the explicit crash-loss/WAL-replay window.
- `SIGKILL` recovery is a standing test for both fully committed state and a
  partially written uncommitted pending tail.

## Collision and coordinate scale

`libdense_collision` accepts coordinates within ±2^28, movement deltas
within ±2^26 per axis, and radii up to 2^20. Choose and record one
world-unit scale before map content is authored. Example:

```text
1 world unit = 1 millimeter
coordinate reach = about ±268 km
maximum library radius = about 1.05 km
```

A centimeter scale gives about ±2,684 km reach. Pick based on required
precision and world extent; do not retrofit the scale after content is
built.

Insert/rasterize static geometry once at world or region boot. Trigger
events and raycast results are sensing inputs for AI: the game senses;
the AI library remembers.

## Navigation

The authoritative game-side selection and cadence rules are maintained in `docs/PATHFINDING_POLICY.md`. The navigation library supplies mechanisms; the host game selects direct movement, shared flow fields, or cached A* and meters all work through the scheduler.

- One global `dsc_lanes` lane/budget owns all repath work. Submit cached A*
  and flow builds through `dnav_work_queue`; never grant a separate full budget
  to every NPC.
- Use `dnav_line_of_sight` for the policy's close-visible fast path and
  `dnav_route_graph` for pre-authored long-distance region/road routes. Route
  graph edges are plans; local seams and every movement step still pass through
  collision validation.
- Use a flow field whenever more than a handful of agents pursue the
  same target.
- Every walkability/cost mutation goes through `dnav_grid_set_cost` so
  the grid version invalidates path-cache entries and marks fields stale.
- Sparse storage removes up-front continent allocation, but unbounded
  path/flow expansion is still unbounded *work*. Always supply scheduler
  expansion budgets for live workloads.

## AI

- The per-tick max comes from the scheduler and is passed to
  `dai_world_run_slice`; use the returned count to debit the global AI
  allowance.
- Partial ticks rotate the next start slot, so aggressive shedding adds
  latency without permanently starving high-numbered agents.
- Preallocate `initial_intent_capacity`; use `max_intents` as a hard
  safety bound.
- Conditions may read AI/watch/query state. Actions emit intents only.
- Apply intents strictly through nav -> collision -> sim -> net.

## Determinism and audit

`integration/tools/run_determinism.sh` runs the same scripted input log
and per-tick checksum under optimized and sanitized builds. Extend its
world checksum whenever authoritative state is added.

The first post-integration project should be audit logging:

```text
(tick, session identity, inbound frame bytes)
```

Replaying that log from the same initial state must reproduce every
world checksum. This supports byte-for-byte bug and suspected-cheat
reproduction.

## Metrics

Populate one `dense_tick_metrics` record per tick. It contains named
phase durations and stats snapshots from net, collision, nav, and AI,
with adapter fields for sim and DenseDB. Export or persist the record as
one unit so operational analysis can answer which phase consumed the
budget and which shedding state was active.
