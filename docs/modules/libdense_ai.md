# libdense_ai

**Perception, threat, behavior execution, and NPC intent generation
for high-density multiplayer servers.**

## Phase 5 batch updates

`dai_perceive_many()`, `dai_threat_add_many()`, `dai_threat_decay_many()`, and
the condition-evaluation batch API provide bounded scheduler submission.
Game-owned handlers still execute one request at a time, preserving arbitrary
logic and deterministic intent boundaries. Run `make benchmark-phase5-batches`.


## 0.2.0 MMO integration update

- Added preallocated and optionally hard-capped intent storage, avoiding
  surprise reallocations or unbounded intent growth in the AI phase.
- Added `dai_world_run_slice`, which reports the live-agent budget used.
  Partial ticks rotate the next tick's starting slot, preventing
  starvation when the scheduler intentionally sheds AI work.
- Initial agent capacity now preallocates agent and free-slot storage;
  capacity growth is failure-atomic.
- Action handlers remain intent-only: apply results through
  nav -> collision -> sim -> net, never by mutating game state directly.
- Added optional `LTO=1` builds for the final release link.
- Replaced per-agent percept, threat, and cooldown heap allocations with
  world-owned retained size-class pools. Added explicit prewarm and storage
  telemetry APIs for 4/8/16/32 percept slots, 4/8/16 threat slots, and
  8/16/32/64 cooldown stamps.
- Added `dai_world_run_budgeted()`, which requires a nonzero scheduler-supplied
  agent cap and reports completed/deferred work. Compatibility run APIs remain,
  but are no longer the authoritative server integration path.
- Evaluated a complete hot/cold agent-array split under order-balanced
  10,000-agent workloads. The measured gain was only about 1-4% with equal
  retained memory, so production keeps the simpler AoS layout and retains the
  split as a comparison-only build.
- Evaluated the Phase 3 agent-registry `dense_core` conversion under exact
  equivalence and representative 10,000-agent workloads; the specialized
  trusted-id registry remains production because the generic backend regressed
  every measured scenario without reducing retained map bytes.

libdense_ai is the NPC-mind module of the Dense ecosystem - the top
of the gameplay stack (sim, DB, net, sched, collision, nav, ai).
Standalone, source-available C11, zero dependencies.

The one architectural rule: **intents out, never mutations**. A tick
of AI turns percept memory and threat state into an ordered intent
buffer; the game applies intents through the authoritative pipeline
(nav -> collision -> sim -> net). No clocks, no global rand, no world
pointers - identical inputs replay to byte-identical intent streams.

- **Perception** - the game senses (libdense_sim observers,
  dc_raycast line of sight); the library remembers: bounded per-agent
  memories with tick-stamped sightings, TTL expiry, and deterministic
  stalest-first displacement. `dai_percepts_nearest` and ordered
  iteration for handlers.
- **Threat** - classic aggro tables with lazy linear decay (no
  per-tick sweeps), bounded capacity with lowest-threat eviction,
  total-order top/ties, drop (death) and clear (leash).
- **Behavior execution** - immutable, refcounted behavior trees
  shared by any number of agents (one tree, ten thousand minds).
  Selector/sequence composites, condition and action leaves
  dispatched to two game-registered handlers, invert and per-agent
  cooldown decorators, and RUNNING memory that resumes directly at
  the running action next tick.
- **Intent generation** - `dai_intent { agent, kind, target,
  position, data }` with game-defined kinds, emitted in deterministic
  execution order, exposed as a borrowed per-tick view.
- **Scheduler-owned staggering** - `dai_world_run_budgeted()` requires a
  nonzero `max_agents` cap from libdense_sched, reports completed/deferred work,
  and returns `DAI_AGAIN` with a resumable cursor.
- **Retained variable storage** - percepts, threats, and cooldown stamps use
  world-owned size-class pools and can be explicitly prewarmed before live
  ticks; oversized configurations retain a direct-allocation fallback.
- **Deterministic randomness** - `dai_agent_random` derives values
  from (seed, agent, tick, draw); variety without breaking replay.

## Layout

```text
include/dense_ai.h   public ABI
src/                 agent, retained storage pools, percept, threat, tree
tests/               semantics + determinism checksum
benchmarks/          horde, stagger, registry, storage, hot/cold comparisons
examples/guard.c     one narrated guard
examples/dense_crowd_demo.c  full-ecosystem demo (nav + collision)
```

## Build

```sh
make            # static + shared library
make LTO=1      # release objects suitable for final -flto link
make test       # test suite + exported-symbol check
make benchmark  # horde + stagger
make benchmark-agent-registry  # isolated Phase 3 old-versus-new gate
make benchmark-phase4-ai       # pools, scheduler cap, hot/cold merge gate
make benchmark-phase5-batches  # percept, threat, and condition batches
make example    # narrated guard
make sanitize   # test suite under ASan/UBSan

# Full-ecosystem demo (sense -> think -> intend -> act):
make demo DENSE_NAV_DIR=../libdense_nav \
          DENSE_COLLISION_DIR=../libdense_collision
```

## Sixty-second tour

```c
dai_tree_create(&tree);
dai_tree_composite_begin(tree, DAI_COMPOSITE_SELECTOR);
  dai_tree_composite_begin(tree, DAI_COMPOSITE_SEQUENCE);
    dai_tree_condition(tree, COND_SEES_ENEMY, 0);
    dai_tree_cooldown_begin(tree, 5);
      dai_tree_action(tree, ACT_ATTACK, 0);
    dai_tree_cooldown_end(tree);
  dai_tree_composite_end(tree);
  dai_tree_action(tree, ACT_PATROL, 0);
dai_tree_composite_end(tree);
dai_tree_finalize(tree);          /* one tree ... */

dai_world_prewarm_storage(ai, &(dai_world_storage_reserve) {
    .percept_blocks = {0, expected_npcs},  /* 8-slot class */
    .threat_blocks = {0, expected_npcs},   /* 8-slot class */
    .cooldown_blocks = {expected_npcs},    /* 8-stamp class */
});

dai_agent_add(ai, npc_id, &(dai_agent_config) { .tree = tree });
                                   /* ... N minds */

dai_world_begin_tick(ai, tick);
dai_perceive(ai, npc_id, &sighting);       /* from sim/collision */
dai_threat_add(ai, npc_id, attacker, dmg);
(void)dai_world_run_budgeted(
    ai,
    &(dai_run_budget) { .max_agents = sched_agents },
    &run_report
);
dai_world_intents(ai, &intents, &count);   /* -> nav, collision, net */
```

The original 0.1 development baseline measured 10,000 agents at
107 ns per agent-tick on its development container. Treat that as a
historical baseline only: rerun `make benchmark` on the deployment
hardware after selecting scheduler caps and intent limits.

## License

Same licensing model as the Dense repository (see the dense repo's
LICENSE.md / COMMERCIAL-LICENSE.md pairing); final license text to be
settled before the first tagged release.
