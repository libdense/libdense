# Opt-in compact framing

Compact framing is an opt-in SDK feature in v0.4.5. The normal SDK
includes `dense_net_wire.h` and its exported functions. Existing registry,
session and UDP entry points continue to use legacy framing until the
application explicitly selects a negotiated compact registry or endpoint.
The family release is 0.4.5; existing component SONAME majors are unchanged.

| Field | Legacy frame | Compact frame |
|---|---:|---:|
| Magic and protocol major | 6 bytes | Negotiated once |
| Opcode | 2 bytes | 2 bytes |
| Request ID | 4 bytes | 4 bytes |
| Payload length | 4 bytes | Implicit for exact-size messages; 2 bytes for variable messages |
| Total frame header | 16 bytes | 6 or 8 bytes |

`DN_FRAME_HEADER_SIZE` remains the legacy 16-byte constant. Payload values,
delivery classes, packet sequences, ACKs, retransmission, queue replacement
keys and repair cadence are unchanged. The variable-message ceiling is 65,535
payload bytes; the negotiated packet cap can impose a smaller usable message
size. No fragmentation is supplied. Fixed-size descriptors can describe larger
values, but this does not make them fit in a UDP datagram.

## Connection contract

1. Initialize an ordinary immutable message registry, including payload bounds
   and an application schema hash covering payload semantics and game identity.
2. Initialize client/server `dn_wire_handshake` objects with allowed profile
   bits, an outer datagram cap, and fresh nonzero nonces. The server nonce's low
   32 bits must differ from `DN_WIRE_BOOTSTRAP_MAGIC`.
3. Exchange OFFER, SELECT, CONFIRM and READY using the fixed 56-byte bootstrap.
   Check all return values, enforce fixed deadlines and bound retries. Matching
   descriptor hashes do not authenticate the peer.
4. After `dn_wire_handshake_ready`, create the connection's registry with
   `dn_wire_registry_create`. `dn_wire_registry_prepare` is available to share
   immutable server encoding tables; preparing a table does not admit a peer.
5. Bind all data and ACK packets to that connection incarnation before session
   processing. The optional `dn_wire_transport` adds an eight-byte server tag.
   When using it, set the session cap to `selected_cap - DN_WIRE_TAG_SIZE` and
   demultiplex retried bootstrap messages separately. An existing authenticated
   transport that isolates connection incarnations may supply its own binding.
6. Keep source registries alive while handshakes use them. Keep prepared
   registries alive until their sessions and replication objects are destroyed.
   Destroy registries with `dn_registry_destroy`. The tag adapter borrows its
   inner transport and never closes it.

The caller owns admission, nonce uniqueness across reconnects/restarts, security,
and scheduling. Tags and compatibility hashes provide neither authentication
nor cryptographic replay protection. Bind negotiation to authenticated transport
where the deployment requires it. Profile selection remains fixed for the connection lifetime.

`DN_WIRE_LEGACY` means the legacy inner codec negotiated between upgraded peers.
The negotiated bootstrap requires upgraded peers.

## UDP dispatcher and examples

`dn_wire_udp_server_create` creates a compact-only IPv4 endpoint using the
existing server/session APIs. Pending negotiations are bounded by
`maximum_pending <= max_sessions`. A fixed handshake timeout and a duplicate
reply interval are mandatory. A normal session is allocated only after valid
CONFIRM, and the existing accept callback decides admission. Use
`dn_wire_udp_server_registry` for server replication. Never mix different wire
contracts in one replication group.

The dispatcher checks connection tags before sequence or ACK processing and
reads one byte beyond its cap to reject oversized datagrams. The generic tag
adapter uses the same extra-byte defense for transports that truncate reads.
Handshake storage is allocated only for compact endpoints; legacy peer-table
entries retain their previous size. Compact storage scales with configured
pending and session limits, plus one shared registry and send buffer. The source
registry remains borrowed for the server lifetime.

See [network modes](network-modes-0.4.5.md) for the prebuilt headless demos.

## Compatibility and scope

Existing public structs and symbols are preserved; 13 wire functions are added.
Legacy is still the default. Native binaries target Linux x86-64. Other targets
require separate builds and runtime validation. Grouped/Mithsteel are available through the separate
[public codec API](grouped-state.md). Research additions are excluded.
