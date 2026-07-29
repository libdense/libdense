# Pathfinding Policy

Game-side policy for using libdense_nav in the C MMO. The library is
policy-free by design: it provides A*, a shared path cache, flow
fields, and line of sight, and never decides which to use. This
document is where that decision lives.

Status: policy adopted for the single-shard server. One item (the
congestion layer) is flagged as an open design question and is NOT
adopted yet.

---

## 1. The selection ladder

Choose the movement mechanism per agent per repath, in this order.
The first rule that matches wins.

### Rule 1: close and visible: walk straight, no pathfinding

If the target is within CHASE_DIRECT_RANGE and dnav_line_of_sight
from agent to target is clear, do not path at all. Emit a movement
intent straight at the target and let dc_validate_move handle the
rest.

- This is the cheapest case and, in combat range, the most common.
- LOS over the grid is O(tiles crossed); there is no path object,
  no cache entry, nothing to invalidate.
- Re-check LOS on the agent's repath cadence (section 3), not every
  tick.

### Rule 2: shared goal: one flow field, everyone samples

If a goal is (or is expected to be) chased by FLOW_MIN_CHASERS or
more agents, build one flow field over the goal tiles and have every
chaser call dnav_flow_sample.

- The field costs one bounded Dijkstra whether it serves 3 agents
  or 300. Chaser count, not distance, is what makes it pay.
- Build under a sched budget with dnav_flow_field_build_continue;
  a partial field is usable in the region already covered.
- Typical shared goals: the pulled player in a camp, a siege door,
  a leash point, a spawn rally, the intruder in the demo.
- Ownership: the game owns field lifetime. Key fields by goal
  identity (entity id or fixed location), drop them when the last
  chaser releases or the goal moves more than FLOW_REBUILD_DISTANCE
  tiles, and rebuild on grid version change.

Starting values: FLOW_MIN_CHASERS = 4, FLOW_REBUILD_DISTANCE = 4
tiles. Tune from metrics (section 6).

### Rule 3: unique goal: cached A*

Everything else (patrols, fetches, scripted moves, a lone chaser)
goes through dnav_path_find via the pathfinder's clustered LRU
cache.

- Agents whose (start cluster, goal cluster) match get the same
  refcounted path object; the cache makes "many agents, same route"
  collapse into one search.
- Paths are stamped with the grid version. A stale path is a signal
  to repath, not an error.
- All A* work runs under one global repath budget (section 4).

### Rule 4: no path found

If A* fails or the flow field marks the agent's tile unreachable,
do not retry every tick. Set the agent's repath cooldown to the
failure backoff (start: 2x normal cadence), emit the game's
fallback intent (hold, wander, or leash), and let perception TTL
age the target out naturally.

---

## 2. Distance governs cadence, not algorithm

The ladder above picks the mechanism. Distance and relevance pick
how OFTEN an agent re-paths:

- Near agents (engaged, on-screen density): repath on target-moved
  or every REPATH_NEAR ticks, whichever is later.
- Far agents (approaching, ambient): every REPATH_FAR ticks.
- Dormant agents (no percepts, full threat decay): no pathfinding
  at all; they do not hold paths.

Starting values at 20 Hz: REPATH_NEAR = 10 ticks (0.5 s),
REPATH_FAR = 60 ticks (3 s). Under overload (section 4) these
stretch; they never shrink below the starting values.

Flow-field consumers have no repath cadence at all: sampling is
O(1) per tick and the cadence question moves to field rebuilds.

---

## 3. The conga line

Shared paths and flow fields converge agents into single file
through the same optimal corridor. The library does this on
purpose (sharing is the efficiency win) and provides no
mitigation. The game applies these, in order; 1 and 2 are adopted,
3 is the open question, 4 is deferred.

### 3.1 Adopted: deterministic lateral jitter

When converting a shared path waypoint or flow sample into a
movement intent, offset the step direction laterally by a small
per-agent constant derived from dai_agent_random (seed, agent id).
The offset is stable for the agent's lifetime (draw it once at
spawn or derive it from the id), bounded to +/- JITTER_MAX of a
tile, and replay-safe because it comes from the deterministic
stream.

- Breaks perfect single file in open ground for one multiply and
  one add per agent per tick.
- Starting value: JITTER_MAX = 0.35 tiles.

### 3.2 Adopted: collision does the queuing

Agent-vs-agent blocking stays ON for walking NPCs
(DC_MOVE_BLOCK_ON_BODIES with slide). At a chokepoint agents
physically queue, which is a correct and readable conga line;
combined with 3.1 the line only forms where geometry forces it.

- Exception list (ghosts, swarms, bosses) is game data, not
  policy.

### 3.3 Open design question: congestion-aware costs (NOT adopted)

Tile costs are writable (dnav_grid_set_cost), so crowd density
could be written into the cost layer and flow fields rebuilt to
route around jams. Do not do this naively: cost writes bump the
grid version, which invalidates the entire path cache every tick
and destroys its value.

Adopting this requires one of:

- a separate congestion overlay the flow builder reads without
  touching the base grid version (library addition to libdense_nav),
  or
- a coarse cadence (write congestion, rebuild fields, every
  CONGESTION_PERIOD ticks) with the cache invalidation accepted as
  a known cost.

Decision deferred until a real scenario shows sustained jamming
that 3.1 + 3.2 do not resolve. If that happens, prefer the overlay
design and treat it as a nav work item, not a game-side hack.

### 3.4 Deferred: local avoidance forces

Separation/steering between nav output and collision validation.
Not built, not scheduled. Revisit only if jitter plus physical
queuing looks bad in playtests.

---

## 4. Budgets and overload

All pathfinding is metered by libdense_sched; nothing paths outside
a budget.

- One global repath lane: every A* request goes through a single
  dsc lane (strict tier below combat, above ambient AI). The lane
  drains under the tick's nav phase budget; requests that do not
  fit wait. There is no per-NPC right to path.
- Flow builds share the same phase budget via
  dnav_flow_field_build_continue chunks.
- Under overload (dsc_overload level rises): REPATH_NEAR and
  REPATH_FAR stretch (double per level as a starting rule), flow
  rebuild cadence stretches, and dormant demotion gets more
  aggressive. Shedding pathfinding is invisible for several
  seconds; shedding movement is not. Path work sheds first.

---

## 5. Invalidation rules

- Grid version changes (door opens, wall destroyed, bridge
  collapses): stale paths are detected by version mismatch on use;
  agents repath on their next cadence tick, not synchronously. Flow
  fields keyed to the changed region rebuild under budget.
- Do not mass-invalidate proactively; version stamps make lazy
  invalidation safe and spread the repath cost across the cadence
  window instead of spiking one tick.
- Collision remains authoritative: a nav path is a plan, never a
  permission. Every step still goes through dc_validate_move.

---

## 6. Metrics to watch

Exported already; aggregate per tick in the host metrics record:

- pathfinder stats: searches, cache hits, cache hit rate (target:
  above 0.5 in camps; near 0 is fine in scattered ambient zones)
- repath lane depth and shed count (sustained depth means the
  budget or the cadences are wrong)
- flow fields live, rebuilds per second, mean build slices
- share ratio: agents on shared paths or fields vs solo A* (this
  is the dense rule made visible; falling share ratio in a crowd
  scene is a regression)

---

## 7. Constants summary (starting values, tune from metrics)

    CHASE_DIRECT_RANGE      12 tiles
    FLOW_MIN_CHASERS        4
    FLOW_REBUILD_DISTANCE   4 tiles
    REPATH_NEAR             10 ticks
    REPATH_FAR              60 ticks
    FAILURE_BACKOFF         2x cadence
    JITTER_MAX              0.35 tiles
    CONGESTION_PERIOD       (unset; section 3.3 not adopted)
