# libdense_net

**Replication encoding, queues, frames, sessions, and transport for
high-density multiplayer systems.**

## 0.3.1–0.3.5 additive interfaces

- `dn_registry_init_with_payload_bounds()` opts selected opcodes into inclusive
  minimum/maximum payload sizes while preserving the descriptor ABI and frame
  format.
- `dn_session_send_sequenced_keyed()` accepts an application-owned stable
  coalescing key without changing the existing descriptor-derived send path.
- `dn_session_precommit_prewarmed_storage()` touches already-reserved payload
  and fixed-metadata pages for deployments that explicitly choose resident
  prewarmed storage.
- Session, UDP transport, and UDP server memory-region visitors expose
  read-only allocation attribution without reading allocation contents.
- UDP authority servers can opt into resetting empty inbound-ring cursors and
  can report reset count, depth peak, cursor-slot high water, and live inbound
  payload bytes.

All additions are default-off or observational. Existing wire format, delivery
ordering, ordinary policies, and SONAME major `0` are unchanged.

## Phase 5 batch encoding

`dn_frame_encode_many()` and `dn_replication_group_write_many()` provide bounded
input-ordered encoding batches. ACK windows and due retransmits retain their
existing validated session and compact-active-set paths. Run
`make benchmark-phase5-batches`.


## 0.2.0 MMO integration update

- Phase 4 replaces the registered-session flush scan with a retained dirty
  activity set and fair ready-session FIFO. Reliable/ack, sequenced/unreliable,
  retransmit, timed, and currently due sessions remain separately measurable.
- `dn_session_set_flush_all()` now visits only sessions ready at pass start,
  respects global/per-session/session-count budgets, and requeues unfinished
  work at the tail for exact cross-tick round-robin fairness.
- `dn_session_set_get_activity_stats()` exposes active categories, queue depth,
  selected sessions, skipped idle sessions, and pass totals.
- `make benchmark-sparse-flush` measures 1%, 10%, and 100% activity at 10,000
  sessions plus 1% activity at 50,000 sessions and a 4,096-session fairness proof.

- Reliable retransmit and out-of-order receive packets now use one shared
  retained fixed-size packet pool instead of per-slot `realloc`. The pool grows
  in bounded blocks, recycles buffers after ACK/delivery, and can be sized before
  live ticks with `dn_session_prewarm()`.
- `dn_session_prewarm()` can also reserve owned frame bytes for reliable,
  sequenced, and unreliable queue slots. Packet-pool capacity/high-water/growth
  telemetry identifies profiles that were undersized after prewarming.
- `dn_session_precommit_prewarmed_storage()` separately touches reserved
  owned-frame and reliable-packet payload pages plus fixed queue metadata,
  packet scratch, and reliable-packet metadata while a session is idle. Exact
  retained-byte and precommit-state telemetry covers every storage class.
  Ordinary prewarm and operating-system lazy commitment remain unchanged.
- `make benchmark-packet-storage` measures first-burst allocation removal and
  steady packet-buffer recycling.

- Completed the Phase 3 endpoint/session registry gates. Both generic group-map
  conversions were rejected after exact equivalence and representative
  large-peer, miss, churn, rebinding, and active-flush benchmarks.
- The production endpoint map now uses deterministic seeded hashing and
  tombstone-free backshift deletion, so sustained peer churn cannot exhaust a
  fixed table with stale tombstones.
- `make benchmark-network-maps` reruns both comparison gates; experimental
  storage is comparison-only and not installed.

- Added `dn_udp_server`: one nonblocking authority socket with bounded
  IPv4 endpoint -> session demultiplexing, per-peer inbound queues,
  lazy session acceptance, isolated protocol errors, and peer removal.
- Added `dn_session_set`: O(1) session lookup and fair round-robin,
  globally and per-session budgeted active-session flush passes. A
  dedicated benchmark covers 1,000 and 5,000 sessions.
- The recommended v1 loop remains single-threaded: drain available
  datagrams before the tick and run the bounded active-session flush
  pass in `DSC_PHASE_FLUSH`.
- Added optional `LTO=1` builds for the final release link.

libdense_net is the network module of the Dense ecosystem, alongside
`libdense_sim` (spatial execution and subscription kernel) and
`DenseDB` (channel-aware state database). The implementation is maintained as a
standalone-buildable C11 module in the Dense source repository. It privately
uses the non-installed `dense_core` dirty-set implementation. UDP uses POSIX
sockets on Linux and Winsock on Windows.

In short:

- **Schema-agnostic.** Games own their protocol (opcodes, payloads,
  schema hash). libdense_net consumes a runtime table of message
  descriptors and enforces framing and delivery semantics around it.
  The frame layout is byte-identical to the Dense C MMO Phase 5 wire.
- **Delivery labels have teeth.** `reliable` (ordered, must arrive),
  `sequenced` (last-write-wins), and `unreliable` are enforced by a
  channel stack with acks, adaptive retransmission, and staleness
  drops, over any datagram transport.
- **Encode once, deliver to many.** The replication batcher writes each
  fanout group's frames exactly once into a tick arena and shares the
  bytes with every recipient session, the same rule libdense_sim's
  grouped recipient plans are built on. An optional bridge publishes a
  `ds_fanout_view` directly.
- **Deterministic by construction.** No internal clocks or rand;
  time is injected. A seeded adverse-network transport (drop /
  duplicate / reorder) makes delivery semantics provable in tests.

## Layout

```text
include/dense_net.h              public ABI
include/dense_net_sim_bridge.h   optional libdense_sim bridge
src/                             implementation
tests/                           test suite + ABI export baseline
benchmarks/                      replication fanout + adverse soak
examples/echo_pair.c             handshake + state over loopback
```

## Build

```sh
make            # static + shared library
make LTO=1      # release objects suitable for final -flto link
make test       # test suite + exported-symbol check
make benchmark  # replication fanout + adverse channel soak
make benchmark-network-maps  # Phase 3 endpoint/session comparison
make benchmark-sparse-flush # Phase 4 active-session flush
make benchmark-packet-storage # Phase 4 retained packet prewarm
make benchmark-phase5-batches # frame and replication encoding batches
make example    # loopback client/authority pair
make sanitize   # test suite under ASan/UBSan

# With the libdense_sim fanout bridge + its end-to-end test:
make DENSE_SIM_DIR=/path/to/dense/libdense_sim test
```

## Sixty-second tour

```c
static const dn_message_desc MESSAGES[] = {
    { .opcode = 1025, .name = "combat_use_skill",
      .direction = DN_DIRECTION_CLIENT_TO_AUTHORITY,
      .delivery = DN_DELIVERY_RELIABLE, .payload_size = 24 },
    { .opcode = 1032, .name = "combat_entity_snapshot",
      .direction = DN_DIRECTION_AUTHORITY_TO_CLIENT,
      .delivery = DN_DELIVERY_SEQUENCED, .payload_size = 26,
      .key_offset = 4, .key_size = 8 /* coalesce per entity_id */ },
};

/* Optional bounded-variable payloads keep the descriptor ABI and frame layout. */
static const dn_message_desc VARIABLE_MESSAGE = {
    .opcode = 775,
    .name = "transform_batch_v3",
    .direction = DN_DIRECTION_AUTHORITY_TO_CLIENT,
    .delivery = DN_DELIVERY_SEQUENCED,
    .payload_size = 1064,
};
static const dn_payload_bounds VARIABLE_BOUNDS = {
    .opcode = 775,
    .min_payload_size = 44,
    .max_payload_size = 1064,
};

dn_protocol_config protocol = {
    .frame_magic = 0x4f4d4d44,          /* game-owned: "DMMO" */
    .protocol_major = 0,
    .schema_hash = 0x2f39f05c0e521e44,  /* from the game's generator */
    .max_payload_size = 4096,
};

dn_registry registry;
/* Fixed-size registry: */
dn_registry_init(&registry, &protocol, MESSAGES, 2);

/* For a bounded-variable registry, use the opt-in initializer instead. */
/* dn_registry_init_with_payload_bounds(
 *     &registry, &protocol, &VARIABLE_MESSAGE, 1, &VARIABLE_BOUNDS, 1
 * );
 */

dn_session *session;
dn_session_create(&session, &registry, transport, NULL);

dn_session_prewarm(session, &(dn_session_prewarm_config) {
    .reliable_frame_bytes_per_slot = DN_FRAME_HEADER_SIZE + 24,
    .reliable_packet_buffers = 8, /* expected in-flight + reordered high water */
});

dn_session_send(session, 1025, request_id, payload, 24);
dn_session_flush(session, now_ns, byte_budget, NULL);
dn_session_receive(session, now_ns, on_frame, ctx);
```

## License

See `LICENSE.md` and `COMMERCIAL-LICENSE.md` at the release root.
