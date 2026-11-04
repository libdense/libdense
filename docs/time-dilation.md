# Time Bending / Time Dilation

`dense_sched_time.h` adds an explicit, allocation-free time domain to the ordinary
static/shared `libdense_sched` SDK. Link `Dense::sched` in CMake or
`-ldense_sched` directly. Existing tick clocks, timers and network defaults are
unchanged until an application initializes and polls this clock.

The authority owns one clock for a coherent interaction region. Every completed
step advances simulation by the same `step_ns`. A scale below one makes those
steps occur less often in real time. This changes real-time gameplay/service
speed. It does not turn a slower simulation into equivalent real-time capacity.
There is no automatic load controller; the application explicitly selects scale
targets, optionally using existing overload metrics as input to its policy.

## Minimal complete consumer

```c
#include "dense_sched_time.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    dsc_time_config config;
    dsc_time_config_defaults(&config);
    config.initial_scale_ppm = 500000u;
    dsc_time_origin origin = {1u, 0u, 0u, 0u};
    dsc_time_clock clock;
    assert(dsc_time_init(&clock, &config, &origin) == DSC_OK);
    unsigned real_services = 0u, simulation_steps = 0u;
    for (uint64_t now = 1000000u; now <= 1000000000u; now += 1000000u) {
        ++real_services; /* service real-time input/network work here */
        for (unsigned budget = 0u; budget < 4u; ++budget) {
            dsc_time_step step;
            dsc_result result = dsc_time_poll(&clock, now, &step);
            if (result == DSC_AGAIN) break;
            assert(result == DSC_OK);
            assert(step.dt_ns == 50000000u);
            ++simulation_steps; /* advance opted-in game systems by step.dt_ns */
        }
    }
    assert(real_services == 1000u && simulation_steps == 10u);
    printf("real services=%u simulation steps=%u\n", real_services, simulation_steps);
    return 0;
}
```

The example injects time to demonstrate behavior without sleeping. In a server,
inject a monotonic real-time source. Call network receive/flush with that real
timestamp, including iterations with no simulation step. Never pass dilated
simulation time to ACK/retransmit, keepalive, timeout or rate-limit maintenance.
When using delivery workers, service only idle sessions during an explicit serial
handoff, or run bounded owner-local service on their exclusive worker. Always
finish `dn_delivery_wait` before controller access to worker-owned sessions.

The repository's `make test-time-integration` exercises actual reliable retries,
exact grouped/Mithsteel payloads, both framing profiles and 1/2 delivery workers.
At all three tested scales below, real-time service runs 1000 times per second
and reliable retransmit cadence remains unchanged.

| Scale q | `initial_scale_ppm` | Simulation steps per real second | Simulation time advanced |
| --- | ---: | ---: | ---: |
| 1 | 1000000 | 20 | 1 second |
| 0.5 | 500000 | 10 | 0.5 seconds |
| 0.1 | 100000 | 2 | 0.1 seconds |

These are deterministic scheduling checks, not CPU benchmarks. Replication
bandwidth falls only for streams emitted less often or containing less changed
information. A separate snapshot stream at 20 real-time Hz may retain its traffic.
Performance reports must state q, simulation Hz, and replication Hz per real and
simulation second. No new hardware capacity claim accompanies this API.

## Scale changes and client timelines

Scales are integer parts per million, 1 through 1000000. The default minimum is
100000 (q=0.1), configurable by the authority. Zero, negative, above-one and
below-minimum requests are invalid. The SDK supplies slowdown, not pause or
speedup. Default fixed steps are 50 ms, with at most 100000 scale units of change
at each completed simulation tick. Set `max_scale_change_ppm` explicitly for a
different ramp limit; 1000000 permits an immediate change at the chosen boundary.

Schedule a `dsc_time_control` containing the current epoch, a future effective
tick and the target scale. The target activates **after completing that tick**.
The first bounded ramp step then sets the rate for the following interval. The
just-completed step still used the previous scale and its fixed simulation dt.
Each subsequent completed tick moves the rate toward the target by at most the
configured amount. One future control may be pending. Another valid request
returns `DSC_AGAIN` and does not replace it. A new control can be scheduled after
the previous control activates, even while its ramp continues.

Publish accepted controls reliably with their epoch/effective tick, along with
the agreed configuration. `dsc_time_step` reports the completed tick, its real
deadline, simulation time, old/new scales and whether the control activated.
Clients must receive the event before using that interval or recover an
authoritative snapshot. The SDK does not authenticate, transmit, reorder or
rollback these application control messages.

`dsc_time_get_status` supplies every runtime field needed for a consistent
snapshot, including current/target scale, pending control and fractional phase.
`dsc_time_restore` validates that snapshot with the same configuration and
restores it exactly. Use these for join/resync or persistence; reconstructing
from tick and current scale alone loses fractional phase and ramp state. Encode
fields individually with agreed widths, byte order and boolean representation.
C struct bytes, pointers and padding are not a wire or persistence format.
Applications reject stale snapshots and authenticate authority before restore.

Real timestamps must be expressed in the authority's timeline. Different hosts'
monotonic clock origins are not interchangeable. Applications must translate
their clock readings or replay authoritative relative time. Clock synchronization
and interpolation policy remain application responsibilities.

## Backlog, bounded work and arithmetic

Each poll consumes at most one step in constant work. Repeated calls with the
same real timestamp drain already-due steps without skipping ticks. Limit calls
per service iteration and return to input/network work when that budget is used.
`DSC_AGAIN` means no step is due; it advances internal time/phase but leaves the
step output unchanged. Every hard rejection leaves state and output unchanged.

The default maximum unprocessed real-time gap is one second. A larger gap returns
`DSC_ERR_CAPACITY` without advancing or silently dropping ticks. The application
may deliberately choose a larger bound before starting the timeline, or recover
through `dsc_time_rebase`. Rebase requires a larger epoch and nondecreasing real
time. It preserves completed tick/simulation time/current scale, discards elapsed
backlog and fractional phase, and cancels the pending control and ongoing ramp.
Publish a new baseline after this explicit discontinuity. It is a policy decision,
not transparent lossless recovery. Tick/time exhaustion cannot be fixed by rebase
while preserving counters; start a new explicit timeline if that boundary is reached.

The phase accumulator uses integer units of one millionth of a simulation
nanosecond. A boundary occurs at the first integer real nanosecond that completes
the step; fractional credit earned at the previous scale carries forward. This
prevents repeated rounding loss and defines transitions exactly. There is no
floating point, wide-integer compiler extension, allocation, hidden clock or
thread creation. A fixed step must be at most `UINT64_MAX / 1000000` nanoseconds.
All counter/deadline overflow paths reject instead of wrapping. Status reports
`can_step=false` with a zero next deadline when no next step is representable.

## Authority and timer audit

Opponents sharing a combat interaction domain use one coherent authoritative
timeline. Independently scaled regions need an explicit boundary policy for
projectiles, collisions, migration and cross-region effects. The SDK provides
no automatic region partition or physically equivalent cross-clock interaction.
Moving an actor between domains requires authoritative conversion/rebaselining.

| Work or timer | Required application decision |
| --- | --- |
| Opted-in movement, physics, combat cooldowns | Use fixed simulation steps and the region's simulation tick/time. |
| ACK/retry, keepalive, connection timeout, rate limit | Continue on monotonic real time. |
| Input reception and queue limits | Receive promptly on real time; bound queued work before simulation consumption. |
| Database maintenance, operational deadlines, leases | Keep on real time unless explicitly designed otherwise. |
| Persisted cooldown, respawn, production or offline timers | Record the chosen domain/epoch and define restart/offline semantics individually. |

Budget and overload measurements use real elapsed execution time, not scaled
simulation time. Existing scheduler timer wheels remain domain-neutral: advance a simulation wheel
with completed simulation ticks, and a separate operational wheel with its real
time schedule. Do not mix domains in one wheel or reinterpret saved deadlines
without migration. Monotonic timestamps are not portable wall-clock dates across
process restarts. No existing timer is implicitly converted by this feature.

The ordinary C API also works from C++. Current Python/Rust simulation wrappers
do not wrap it. Linux x86-64 execution is qualified in this preparation; other
platforms still require their native release checks.
