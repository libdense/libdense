#ifndef DENSE_NET_WIRE_H
#define DENSE_NET_WIRE_H

#include "dense_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/* opt-in wire negotiation. ordinary registries and dn_udp_server_create keep
 * legacy framing. no timeout downgrade, packet guessing or live profile change.
 * fixed payloads use 6-byte headers; variable payloads use 8-byte headers with
 * a 16-bit length. both keep explicit opcodes and full 32-bit request IDs.
 * payload values, channel sequences, ACKs and delivery classes are unchanged.
 *
 * use one thread per object, as for dn_session. source registries must remain
 * immutable and alive while a handshake uses them. schema_hash must identify
 * payload meaning, not only sizes. prepared/output registries own their copied
 * descriptors and must outlive all sessions/replication using them.
 *
 * negotiation and tags are compatibility/incarnation checks, not authentication
 * or encryption. the caller supplies admission, authenticated transport when
 * required, retry/deadline policy, control/data demultiplexing, and fresh nonzero
 * nonces across connections and restarts. do not reuse a server nonce; its low
 * 32 bits must differ from DN_WIRE_BOOTSTRAP_MAGIC. see docs/compact-wire.md. */
#define DN_WIRE_LEGACY 1u
#define DN_WIRE_COMPACT 2u
#define DN_WIRE_BOOTSTRAP_SIZE 56u
#define DN_WIRE_BOOTSTRAP_MAGIC UINT32_C(0x31434e44)
#define DN_WIRE_TAG_SIZE 8u

/* caller-owned state. fields are read-only to the caller after initialization.
 * cached contains the current retry message when has_cached is true. */
typedef struct dn_wire_handshake {
    const dn_registry *source;
    uint64_t nonce;
    uint64_t peer_nonce;
    uint64_t digest;
    uint32_t packet_cap;
    uint32_t selected_cap;
    uint16_t supported;
    uint16_t selected;
    unsigned phase;
    bool server;
    uint8_t cached[DN_WIRE_BOOTSTRAP_SIZE];
    bool has_cached;
} dn_wire_handshake;

/* supported is a nonempty DN_WIRE_* bitmask. packet_cap includes any tag and
 * must be 56..65535. incompatible variable bounds disable compact eligibility.
 * server init and client init do not send or allocate a session. */
DN_API dn_result dn_wire_handshake_init(dn_wire_handshake *handshake,
    const dn_registry *registry, bool server, uint16_t supported,
    uint32_t packet_cap, uint64_t nonce);
DN_API dn_result dn_wire_handshake_start(dn_wire_handshake *handshake,
    uint8_t output[DN_WIRE_BOOTSTRAP_SIZE]);
DN_API dn_result dn_wire_handshake_step(dn_wire_handshake *handshake,
    const uint8_t *input, size_t size,
    uint8_t output[DN_WIRE_BOOTSTRAP_SIZE], size_t *output_size);
DN_API bool dn_wire_handshake_ready(const dn_wire_handshake *handshake);

/* output must be uninitialized or destroyed, distinct from source. destroy it
 * with dn_registry_destroy after its users. prepare is a reusable codec cache,
 * not evidence of negotiation/admission. create requires a completed handshake. */
DN_API dn_result dn_wire_registry_prepare(const dn_registry *source,
    uint16_t profile, dn_registry *output);
DN_API dn_result dn_wire_registry_create(const dn_wire_handshake *handshake,
    dn_registry *output);

/* optional datagram incarnation adapter. use only after READY and demultiplex
 * bootstrap retries separately. inner is borrowed and is never closed here.
 * session max_packet_size must be selected_cap - DN_WIRE_TAG_SIZE. a transport
 * that already isolates incarnations can omit the tag. initialized objects
 * cannot be copied, concurrently used, or reinitialized before destruction.
 * inner.receive must consume each successful datagram, store at most the given
 * capacity and report the copied length, truncating longer datagrams to that
 * capacity. The built-in loopback and UDP transports satisfy this contract.
 * The adapter requests selected_cap + 1 bytes, counts and drops oversized,
 * short and foreign-tag datagrams, and tries at most 64 receives per call.
 * A caller buffer smaller than an accepted payload returns BUFFER_TOO_SMALL
 * after consuming that datagram; it cannot be recovered by retrying. Other
 * inner errors pass through. No progress guarantee is made for a custom inner
 * transport that retains oversized datagrams or violates the capacity contract.
 */
typedef struct dn_wire_transport {
    dn_raw_transport inner;
    uint64_t tag;
    size_t capacity;
    uint8_t *scratch;
    uint64_t rejected_datagrams;
} dn_wire_transport;

DN_API dn_result dn_wire_transport_init(dn_wire_transport *transport,
    const dn_wire_handshake *handshake, dn_raw_transport inner);
DN_API void dn_wire_transport_destroy(dn_wire_transport *transport);
DN_API dn_raw_transport dn_wire_transport_raw(dn_wire_transport *transport);

typedef uint64_t (*dn_wire_nonce_fn)(void *context);
typedef struct dn_wire_udp_config {
    size_t maximum_pending;
    uint64_t handshake_timeout_ns;
    uint64_t duplicate_reply_interval_ns;
    dn_wire_nonce_fn next_nonce;
    void *nonce_context;
} dn_wire_udp_config;

typedef struct dn_wire_udp_stats {
    uint64_t packets_sent, bytes_sent, control_sent, control_bytes_sent;
    uint64_t offers, completed, expired, invalid_control, rejected_data;
    uint64_t pending_capacity_drops, duplicate_replies;
    size_t pending, pending_peak;
} dn_wire_udp_stats;

typedef void (*dn_wire_udp_trace_fn)(void *context, uint64_t now_ns,
    bool outgoing, const dn_udp_endpoint *endpoint, const uint8_t *packet, size_t size);

/* explicit compact-only IPv4 endpoint with bounded pending negotiations.
 * use the ordinary server poll/remove/destroy functions. config.max_packet_size
 * is the outer cap. the dispatcher supplies tags, fixed handshake deadlines
 * and duplicate-response limits; it invokes the normal accept callback only
 * after CONFIRM. bootstrap replies are best effort: failed replies retain the
 * committed negotiation and retry limits, and do not fail the poll. receive
 * errors and other control errors retain their existing handling. ordinary data
 * sends still report transport failures. this does not authenticate the peer.
 * next_nonce must satisfy
 * the uniqueness contract above; zero signals failure. both timeouts and the
 * pending limit must be nonzero, with maximum_pending <= max_sessions.
 * use the returned registry for server replication, not config.registry.
 * the source registry must remain alive and immutable until server destruction. */
DN_API dn_result dn_wire_udp_server_create(dn_udp_server **output,
    const dn_udp_server_config *config, const dn_wire_udp_config *wire);
DN_API const dn_registry *dn_wire_udp_server_registry(const dn_udp_server *server);
DN_API const dn_wire_udp_stats *dn_wire_udp_server_get_stats(const dn_udp_server *server);
/* callback packet memory is valid only during the call; do not reenter server. */
DN_API void dn_wire_udp_server_set_trace(dn_udp_server *server,
    dn_wire_udp_trace_fn trace, void *context);

#ifdef __cplusplus
}
#endif

#endif
