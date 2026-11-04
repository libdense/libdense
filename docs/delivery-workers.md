# Optional delivery workers

`libdense_delivery` is an explicit Linux SDK component. One worker runs inline on
the calling thread. More than one creates persistent POSIX threads. Neither
installing the library nor linking it starts workers; pool creation does. The
ordinary network library still creates no threads. Windows support is pending;
this iteration validates Linux x86-64. No native ARM64 or performance result is
claimed by these integration tests.

Build and install from the selected release source:

```sh
make -j4 DELIVERY_WORKERS=1
make DELIVERY_WORKERS=1 install PREFIX=/your/prefix
make test-delivery-workers
```

The installed headers are `dense_delivery.h` and `dense_delivery_sim.h`. The
separate static/shared library exports 16 functions. Link it before net and sim:

```sh
cc consumer.c $(pkg-config --cflags --libs libdense_delivery) -o consumer
```

Use `pkg-config --static --libs` when linking static archives. The source install
uses `include/`; the public binary bundle uses `include/dense/`. Its pkg-config
files supply the correct include directory. The existing simulation language
wrappers do not wrap delivery; C++ can call these C headers directly.

CMake consumers use an explicit component and target:

```cmake
find_package(Dense 0.4.5 CONFIG REQUIRED COMPONENTS delivery)
target_link_libraries(server PRIVATE Dense::delivery)
```

For a custom nested binary-bundle `--libdir`, set `Dense_DIR` to
`<prefix>/<libdir>/cmake/Dense` when configuring CMake.

CMake imports the static library and its net, sim and Threads dependencies.
`Dense::runtime`, `Dense::server` and `Dense::all` retain their ordinary component
sets. Source installations and portable static bundles include the delivery
target only when built/staged with `DELIVERY_WORKERS=1`. The binary preparation
bundle includes it as an available opt-in.

The supported library excludes relay symbols and private core entry points.
The separate research Makefile retains its combined archive for historical
experiments. Relay serialization, multi-host service, session migration and
crash recovery are not part of this worker SDK contract.

## Ownership and publication

The authoritative tick still owns simulation mutation, visibility and collision
order. Capture application payloads and the finalized exact recipient sets
before the world or application fields change. `dn_publication_capture` adapts
a `ds_fanout_view`; its encoder writes payloads using `dn_publication_write`.
Applications can also build publications directly, using one audience for many
groups to avoid duplicating a common recipient list.

The publication owns its encoded bytes, descriptors and recipient incarnations.
Its registry must stay immutable and alive. Builder capacities are fixed at
creation. No builder operation grows memory. Failed appends leave prior builder
entries intact. Any failed capture on an idle publication invalidates its sealed
state, including argument-validation failures before the new capture begins.
Call begin again before reuse; an older publication cannot be submitted after a
failed replacement. A busy publication is unchanged. Capture callbacks only map
recipients or append payloads; they must not begin, seal, submit, destroy or
otherwise reenter the builder. Only a sealed object can be submitted. The builder
is not a concurrent API.

Identity is `(world, authority epoch, publication sequence)`. Sequence is
consecutive within one pool/stream and starts at the configured first sequence.
Ticks may have gaps or be equal, but cannot go backwards. Sequence and internal
dispatch exhaustion fail explicitly. Group and audience indices are local to
that publication, not persistent simulation group IDs. Exact full audiences
provide membership for each publication; no membership deltas or repair cache
are assumed. `DN_DELIVERY_PUBLICATION_VERSION` describes this local contract,
not an implemented network wire encoding.

Each recipient contains an observer ID, nonzero connection generation and
worker owner. An audience input must be strictly sorted by unique ID. The
authority mapping must be stable: an ID has the same generation/owner across
all audiences in the publication, distinct IDs resolve to distinct sessions,
and each session has exactly one worker. These are application preconditions.
The resolver must reject an old generation rather than deliver to a replacement
connection. Routing is captured once, partitioned by owner in O(R + W) work per
audience. Each worker visits only its recipients, not every other owner's list.

Each worker has a private replication object, packet-plan cache and recipient
scratch. Existing shared batches/refcounts never cross workers. Application
payload encoding is done once by the authority. This first adapter validates
and copies it there, then each participating worker repeats frame construction
and copies into its own replication storage. Thus there can be W copies/plans
of shared data. Removing that cost with a safe immutable import is a future
experiment. No entity-by-recipient payload array is created.

## Session and transport contract

Create and configure sessions before submission. During active worker execution,
only the assigned worker may touch its sessions, session sets, callbacks, queues,
transport buffers and mutable registry-related application state. Read-only
lookup maps may be shared while frozen. Use per-worker UDP servers/transports;
do not share one existing mutable `dn_udp_server` across workers. A common
transport is permitted only if the application independently supplies the
required synchronization and accounts for its cost.

An optional bounded `service(context, now_ns)` callback runs on each unfinished
owner before consumption/resume. It can poll that owner's ingress, ACKs and
timers. It must not change the captured mapping or enqueue conflicting tick
work. An idle server also needs its normal owner-local maintenance loop; an
empty publication does not automatically service every live session.

The controller calls submit, then wait. Workers preserve each client's group
and frame order. Ordering of callbacks across different clients is concurrent
and is not promised to match a serial loop. Applications needing global callback
ordering must keep that work on the authority. `flush_each_group=false` is the
default, matching bridge enqueue-then-flush behavior. Setting it true reproduces
the sender density fixture's bounded batch/flush cadence. Changing this option
can change coalescing/packet boundaries, so match the direct comparison policy.

Once wait returns, no workers are executing. The controller may inspect stats
and perform a deliberate serial ownership handoff before the next submit/resume.
Do not move a live session to a different worker while it retains batch state
from its old replication owner. Session migration is deferred; recreate and
bootstrap under a new incarnation for an explicit ownership change.

## Admission, backpressure and failures

There is exactly one in-flight publication. Submit is nonblocking for a threaded
pool and synchronous work plus admission for the one-worker inline pool.
`DN_OK` means local admission only. Always call wait, even in inline mode. A
busy pool returns `DN_AGAIN` without accepting or consuming the new publication.
Wrong identity, duplicate/gap sequence, regressing tick/time, wrong registry,
wrong worker count and recipient-scratch overflow reject before any mutation.

Wait has three outcomes:

| Result | Ownership and next action |
| --- | --- |
| `DN_OK` | All workers completed local enqueue/flush. The publication can be reused/destroyed. This is not a reliable-client ACK or proof of physical delivery. |
| `DN_AGAIN` | The same publication stays retained. Service the owner-local inputs and call resume, then wait. Completed workers/groups are not enqueued again. |
| Hard error | Pool is poisoned and publication stays retained until pool destruction. Some recipients may have advanced; do not replay this publication through a replacement pool. Destroy, reconcile/recreate affected sessions and bootstrap explicitly. |

Existing network semantics remain in force. In particular, a best-effort
packet can already be consumed/dropped when its transport returns `DN_AGAIN`;
resume does not recreate that packet. Reliable ACK/retransmit/window behavior
uses the existing session code. `group_end` retains its existing queue-admission
and drop/backpressure behavior; successful publication does not mean every
frame was admitted. Check session statistics and provision capacities as before.

The adapter does not provide transactional rollback of partial consumer errors.
It fails closed instead of guessing an audience, retrying the full group or
silently advancing the stream. The first failed worker is reported in stable
index order, with completed/blocked counts. Destroy joins active workers before
releasing borrowed storage. It never destroys application sessions or performs
implicit reliable delivery. Stop/recovery policy belongs to the server.

## Memory and validation

Publication memory and pool recipient scratch are fully bounded and reported.
Replication/session storage is reported by existing net metrics and retains
its existing prewarm and capacity policies. Thread stacks, libc allocations and
application maps/sinks are not in adapter byte totals. Process RSS is reported
separately by the benchmark. A representative fixed-shape UDP test has zero net
allocations after warmup. The variable Mithsteel density fixture can still
grow its pre-existing storage after four warmup ticks; per-worker copies can
multiply that growth. No universal allocation-free claim is made.

The focused test compares exact per-client packets and statistics against the
direct API at 1/2/4/8 workers, both wire profiles and both flush policies. It
covers reliable/sequenced/unreliable lanes, heterogeneous packet caps, partial
transport failure and resume, stale generations, duplicate/gap/epoch rejection,
builder bounds, world destruction before consumption, observer movement and
ENTER/UPDATE/LEAVE capture. A real UDP loopback control compares received
datagrams and checks fixed-shape steady allocations. See the research checkpoint
for sanitizer coverage, measurement limits and remaining gates.

