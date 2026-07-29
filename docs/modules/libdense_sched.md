# libdense_sched

**Tick budgets, timers, priority lanes, overload and backpressure for
high-density multiplayer servers.**

## 0.2.0 MMO integration update

- Added `dsc_server_pipeline`, enforcing the canonical input, validate,
  spatial, combat, AI, fanout, and flush phases exactly once and in order.
- Added assigned phase budgets, current/EWMA/high-water duration telemetry,
  completed/deferred work high-water values, exhaustion counts, unaccounted
  time, and busiest-phase reporting.
- Added explicit monotonic-clock policies: bounded catch-up or bounded
  slew after a stall. Wall-clock lateness feeds the same overload ladder
  as tick duration.
- Added one shared overload state enum: normal, elevated, high, critical,
  and emergency. Every game-side shedding mechanism reads this same state.
- Added optional `LTO=1` builds for the final release link.

libdense_sched is the execution-control module of the Dense ecosystem,
alongside `libdense_sim`, `DenseDB`, and `libdense_net`. It is a
standalone, source-available C11 library with no dependencies.

Design and rationale live in [DESIGN.md](DESIGN.md). In short: a dense
region overloads every system in the same tick, and someone has to
decide what runs now, what runs later, and what does not run at all.
libdense_sched owns that decision as data: budgets, deadlines, drain
orders, overload levels, and token rates. The game maps the numbers
onto actions.

- **Timer wheel** - hashed hierarchical wheel (4 x 256 slots, 2^32
  tick horizon), one-shot and periodic timers, O(1) schedule/cancel,
  generation-checked handles, deterministic firing order (due tick,
  then schedule order - preserved across cascades and pinned by a
  randomized model test).
- **Tick budget** - low-level begin/end accounting with up to 32 phases.
- **Server pipeline** - strict seven-phase MMO ordering, explicit per-phase
  budgets, duration/work high-water telemetry, exhaustion counts, and a
  busiest-phase answer for each tick.
- **Priority lanes** - strict tiers, deficit-round-robin weights
  within a tier, FIFO within a lane; bounded capacity with reject
  (backpressure) or counted drop-oldest; budgets bind at item
  boundaries with a one-item progress guarantee.
- **Overload ladder** - hysteresis state machine from tick duration and
  wall-clock lateness to normal/elevated/high/critical/emergency (K hot ticks
  to escalate, M calm ticks to recover, holding band in between). Policy
  stays in the game.
- **Token buckets** - integer-exact refill with carried remainders;
  `take` / `available` / `next_available_ns` for pacing outbound
  bytes, command rates, and I/O.

House rules shared with the rest of Dense: no clocks, no rand, no
threads (time is injected - deterministic replay stays possible);
steady-state hot paths never allocate; everything bounded, overflow
loud, drops counted.

## Layout

```text
include/dense_sched.h    public ABI
src/                     timer_wheel, budget, server_pipeline, clock,
                         lanes, overload, token_bucket, common
tests/                   test suite incl. 20k-op timer model fuzz
benchmarks/              timer, overload, and named-pipeline overhead
examples/shard_tick.c    strict seven-phase shard tick loop
```

## Build

```sh
make            # static + shared library
make LTO=1      # release objects suitable for final -flto link
make test       # test suite + exported-symbol check
make benchmark  # timer, shedding, and named-pipeline benchmarks
make example    # miniature shard tick loop
make sanitize   # test suite under ASan/UBSan
```

## Sixty-second tour

```c
dsc_server_pipeline_begin_tick(&pipeline, now);

dsc_server_pipeline_phase_begin(&pipeline, DSC_PHASE_INPUT, now);
/* receive and validate command framing */
dsc_server_pipeline_record_work(&pipeline, commands, deferred_commands);
dsc_server_pipeline_phase_end(&pipeline, DSC_PHASE_INPUT, now);

/* ... VALIDATE, SPATIAL, COMBAT, AI, FANOUT ... */

dsc_server_pipeline_phase_begin(&pipeline, DSC_PHASE_FLUSH, now);
bytes = dsc_token_bucket_available(&session_bucket, now);
dn_session_flush(session, now, bytes, NULL);
dsc_server_pipeline_record_work(&pipeline, bytes_sent, bytes_deferred);
dsc_server_pipeline_phase_end(&pipeline, DSC_PHASE_FLUSH, now);

duration = dsc_server_pipeline_end_tick(&pipeline, now);
state = dsc_overload_observe_pressure(&overload, duration, lateness_ns);
apply_overload_state(world, state); /* game-owned policy */
```

## License

Same licensing model as the Dense repository (see the dense repo's
LICENSE.md / COMMERCIAL-LICENSE.md pairing); final license text to be
settled before the first tagged release.
