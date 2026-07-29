#ifndef DENSE_NET_H
#define DENSE_NET_H

/*
 * libdense_net - replication encoding, queues, frames, sessions, and
 * transport for high-density multiplayer systems.
 *
 * The library is schema-agnostic: games register their own message
 * descriptors (opcodes belong to the game) and libdense_net enforces
 * framing and delivery semantics around them. See DESIGN.md.
 *
 * Threading: none. Time: injected via now_ns parameters. Hot paths do
 * not allocate after warm-up.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DN_SHARED) && defined(DN_BUILD)
#define DN_API __attribute__((visibility("default")))
#else
#define DN_API
#endif

/* ------------------------------------------------------------------ */
/* Results                                                            */
/* ------------------------------------------------------------------ */

typedef enum dn_result {
    DN_OK = 0,
    /* Not an error: retry later (backpressure, empty, budget/window). */
    DN_AGAIN,
    DN_ERR_INVALID_ARGUMENT,
    DN_ERR_OUT_OF_MEMORY,
    DN_ERR_CAPACITY,
    DN_ERR_BAD_MAGIC,
    DN_ERR_VERSION_MISMATCH,
    DN_ERR_UNKNOWN_OPCODE,
    DN_ERR_PAYLOAD_SIZE,
    DN_ERR_BUFFER_TOO_SMALL,
    DN_ERR_MALFORMED,
    DN_ERR_CLOSED,
    DN_ERR_TRANSPORT,
    DN_ERR_STATE,
    DN_ERR_EPOCH,
    DN_ERR_DUPLICATE_OPCODE,
    DN_ERR_NOT_FOUND,
} dn_result;

DN_API const char *dn_result_string(dn_result result);

/* ------------------------------------------------------------------ */
/* Message registry                                                   */
/* ------------------------------------------------------------------ */

typedef enum dn_direction {
    DN_DIRECTION_CLIENT_TO_AUTHORITY = 0,
    DN_DIRECTION_AUTHORITY_TO_CLIENT,
    DN_DIRECTION_BIDIRECTIONAL,
} dn_direction;

typedef enum dn_delivery {
    /* Ordered state transitions that must arrive. */
    DN_DELIVERY_RELIABLE = 0,
    /* Only the newest applicable state is required. */
    DN_DELIVERY_SEQUENCED,
    /* Optional transient data. */
    DN_DELIVERY_UNRELIABLE,
} dn_delivery;

/*
 * One registered message. payload_size is fixed (the protocol contract
 * requires fixed-size records on hot paths). For sequenced messages,
 * key_offset/key_size select the bytes inside the payload that
 * identify the coalescing key (for example an entity id); key_size 0
 * coalesces on the opcode alone. key_size must be 0, 1, 2, 4, or 8 and
 * the key range must lie inside the payload.
 */
typedef struct dn_message_desc {
    uint16_t opcode;
    const char *name;
    dn_direction direction;
    dn_delivery delivery;
    uint32_t payload_size;
    uint16_t key_offset;
    uint16_t key_size;
} dn_message_desc;

/*
 * Game-owned wire identity. frame_magic and protocol_major are written
 * into every frame; schema_hash is exposed for the game's handshake.
 */
typedef struct dn_protocol_config {
    uint32_t frame_magic;
    uint16_t protocol_major;
    uint64_t schema_hash;
    uint32_t max_payload_size;
} dn_protocol_config;

typedef struct dn_registry {
    dn_protocol_config config;
    dn_message_desc *messages;
    size_t message_count;
    /* opcode -> message index + 1, 0 = unknown. */
    uint32_t *opcode_index;
} dn_registry;

DN_API dn_result dn_registry_init(
    dn_registry *registry,
    const dn_protocol_config *config,
    const dn_message_desc *messages,
    size_t message_count
);
DN_API void dn_registry_destroy(dn_registry *registry);
DN_API const dn_message_desc *dn_registry_find(
    const dn_registry *registry,
    uint16_t opcode
);

/* ------------------------------------------------------------------ */
/* Frames (wire-compatible with the C MMO Phase 5 framing)            */
/* ------------------------------------------------------------------ */

#define DN_FRAME_HEADER_SIZE ((size_t)16)

typedef struct dn_frame_header {
    uint16_t protocol_major;
    uint16_t opcode;
    uint32_t payload_size;
    uint32_t request_id;
} dn_frame_header;

DN_API dn_result dn_frame_encode(
    const dn_registry *registry,
    uint16_t opcode,
    uint32_t request_id,
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
);

typedef struct dn_frame_encode_request {
    uint16_t opcode;
    uint32_t request_id;
    const uint8_t *payload;
    size_t payload_size;
    uint8_t *output;
    size_t output_capacity;
} dn_frame_encode_request;

typedef struct dn_frame_encode_result {
    dn_result status;
    size_t output_size;
} dn_frame_encode_result;

DN_API size_t dn_frame_encode_many(
    const dn_registry *registry,
    const dn_frame_encode_request *requests,
    dn_frame_encode_result *out_results,
    size_t count,
    size_t work_budget
);
DN_API dn_result dn_frame_decode(
    const dn_registry *registry,
    const uint8_t *frame,
    size_t frame_size,
    dn_frame_header *out_header,
    const uint8_t **out_payload
);

/* ------------------------------------------------------------------ */
/* Raw datagram transports                                            */
/* ------------------------------------------------------------------ */

typedef dn_result (*dn_raw_send_fn)(
    void *state,
    const uint8_t *packet,
    size_t packet_size
);
typedef dn_result (*dn_raw_receive_fn)(
    void *state,
    uint8_t *packet,
    size_t packet_capacity,
    size_t *packet_size
);
typedef void (*dn_raw_close_fn)(void *state);

typedef struct dn_raw_transport {
    void *state;
    dn_raw_send_fn send;
    dn_raw_receive_fn receive;
    dn_raw_close_fn close;
} dn_raw_transport;

DN_API dn_result dn_raw_transport_send(
    dn_raw_transport *transport,
    const uint8_t *packet,
    size_t packet_size
);
DN_API dn_result dn_raw_transport_receive(
    dn_raw_transport *transport,
    uint8_t *packet,
    size_t packet_capacity,
    size_t *packet_size
);
DN_API void dn_raw_transport_close(dn_raw_transport *transport);

/* Loopback: two bounded SPSC rings forming a client/authority pair.  */

typedef struct dn_loopback_link dn_loopback_link;

DN_API dn_result dn_loopback_link_create(
    dn_loopback_link **out_link,
    size_t queue_capacity,
    size_t max_packet_size
);
DN_API void dn_loopback_link_destroy(dn_loopback_link *link);
DN_API dn_raw_transport dn_loopback_client_transport(dn_loopback_link *link);
DN_API dn_raw_transport dn_loopback_authority_transport(dn_loopback_link *link);

/*
 * UDP: non-blocking datagram socket. bind_address may be NULL (client
 * side, ephemeral port). remote_address may be NULL, in which case the
 * transport locks onto the first peer a packet is received from.
 * Addresses are "host:port" strings (IPv4).
 */
typedef struct dn_udp_transport dn_udp_transport;

DN_API dn_result dn_udp_transport_create(
    dn_udp_transport **out_transport,
    const char *bind_address,
    const char *remote_address,
    size_t max_packet_size
);
DN_API void dn_udp_transport_destroy(dn_udp_transport *transport);
DN_API dn_raw_transport dn_udp_transport_raw(dn_udp_transport *transport);
DN_API dn_result dn_udp_transport_local_port(
    const dn_udp_transport *transport,
    uint16_t *out_port
);

/*
 * Flaky: deterministic adverse-network wrapper around another raw
 * transport. Rates are per-mille (0..1000). Reordering holds a packet
 * back and releases it after later traffic. Seeded xorshift; identical
 * seeds replay identical fates.
 */
typedef struct dn_flaky_transport dn_flaky_transport;

typedef struct dn_flaky_config {
    uint32_t drop_per_mille;
    uint32_t duplicate_per_mille;
    uint32_t reorder_per_mille;
    uint64_t seed;
} dn_flaky_config;

DN_API dn_result dn_flaky_transport_create(
    dn_flaky_transport **out_transport,
    dn_raw_transport inner,
    const dn_flaky_config *config,
    size_t max_packet_size
);
DN_API void dn_flaky_transport_destroy(dn_flaky_transport *transport);
DN_API dn_raw_transport dn_flaky_transport_raw(dn_flaky_transport *transport);

/* ------------------------------------------------------------------ */
/* Sessions                                                           */
/* ------------------------------------------------------------------ */

typedef enum dn_session_state {
    DN_SESSION_CONNECTING = 0,
    DN_SESSION_ESTABLISHED,
    DN_SESSION_CLOSED,
} dn_session_state;

typedef struct dn_session_config {
    /* Whole channel packet cap, header included. Default 1200. */
    size_t max_packet_size;
    /* Reliable lane: max queued frames and owned payload bytes. */
    size_t reliable_frame_capacity;
    size_t reliable_byte_capacity;
    /* Sequenced lane: max distinct (opcode, key) slots. */
    size_t sequenced_slot_capacity;
    /* Unreliable lane: max queued frames (oldest dropped beyond). */
    size_t unreliable_frame_capacity;
    /* Reliable channel window (packets in flight). Default 64. */
    size_t reliable_window;
    /* Retransmit clamps, nanoseconds. Defaults 40ms / 1s. */
    uint64_t rto_min_ns;
    uint64_t rto_max_ns;
    /* Keepalive cadence and peer silence timeout. 0 disables. */
    uint64_t keepalive_interval_ns;
    uint64_t receive_timeout_ns;
} dn_session_config;

DN_API void dn_session_config_defaults(dn_session_config *config);

typedef struct dn_session_stats {
    uint64_t frames_sent;
    uint64_t frames_received;
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t retransmitted_packets;
    uint64_t stale_sequenced_packets_dropped;
    uint64_t duplicate_reliable_packets_dropped;
    uint64_t unreliable_frames_dropped;
    uint64_t sequenced_frames_coalesced;
    uint64_t rtt_ns;
} dn_session_stats;

typedef struct dn_session dn_session;

typedef void (*dn_session_frame_fn)(
    void *context,
    const dn_frame_header *header,
    const uint8_t *payload
);

DN_API dn_result dn_session_create(
    dn_session **out_session,
    const dn_registry *registry,
    dn_raw_transport transport,
    const dn_session_config *config
);
DN_API void dn_session_destroy(dn_session *session);

DN_API dn_session_state dn_session_get_state(const dn_session *session);
DN_API void dn_session_set_established(dn_session *session);
DN_API void dn_session_close(dn_session *session);
DN_API const dn_session_stats *dn_session_get_stats(const dn_session *session);

typedef struct dn_session_memory_stats {
    size_t reliable_queue_capacity;
    size_t sequenced_queue_capacity;
    size_t sequenced_map_capacity;
    size_t unreliable_queue_capacity;
    size_t reliable_window_capacity;
    size_t owned_frame_bytes;
    size_t packet_scratch_bytes;
    size_t packet_pool_buffer_capacity;
    size_t packet_pool_buffers_in_use;
    size_t packet_pool_buffer_high_water;
    size_t packet_pool_retained_bytes;
    uint64_t packet_pool_growth_operations;
    uint64_t packet_pool_growth_after_prewarm;
} dn_session_memory_stats;

DN_API dn_result dn_session_get_memory_stats(
    const dn_session *session,
    dn_session_memory_stats *out_stats
);

typedef struct dn_session_prewarm_config {
    /* Owned encoded-frame bytes retained for every configured lane slot. */
    size_t reliable_frame_bytes_per_slot;
    size_t sequenced_frame_bytes_per_slot;
    size_t unreliable_frame_bytes_per_slot;
    /* Shared fixed-size buffers for sent and out-of-order reliable packets. */
    size_t reliable_packet_buffers;
} dn_session_prewarm_config;

/*
 * Reserve the configured steady-state session storage before live ticks.
 * Zero fields leave that storage class unchanged. Reliable packet buffers are
 * shared between outbound retransmit retention and inbound reorder holding;
 * size this as expected concurrent in-flight plus out-of-order packets. The
 * maximum useful value is two times reliable_window.
 */
DN_API dn_result dn_session_prewarm(
    dn_session *session,
    const dn_session_prewarm_config *config
);

/* Queue one message; routed by the descriptor's delivery class. */
DN_API dn_result dn_session_send(
    dn_session *session,
    uint16_t opcode,
    uint32_t request_id,
    const uint8_t *payload,
    size_t payload_size
);

/*
 * Drain queues into channel packets within byte_budget (SIZE_MAX for
 * unlimited), retransmitting reliable packets due at now_ns first.
 * Returns DN_OK when everything queued has been emitted, DN_AGAIN when
 * budget, window, or transport backpressure stopped the drain early.
 */
DN_API dn_result dn_session_flush(
    dn_session *session,
    uint64_t now_ns,
    size_t byte_budget,
    size_t *out_bytes_sent
);

/*
 * Pump the transport: process acks and delivery semantics and invoke
 * the callback once per received, validated frame, in channel order.
 */
DN_API dn_result dn_session_receive(
    dn_session *session,
    uint64_t now_ns,
    dn_session_frame_fn callback,
    void *context
);

/*
 * Earliest of: next reliable retransmit deadline, next keepalive send,
 * and receive-timeout expiry. UINT64_MAX when nothing is pending.
 */
DN_API uint64_t dn_session_next_due_ns(const dn_session *session);


/* ------------------------------------------------------------------ */
/* Session registry                                                   */
/* ------------------------------------------------------------------ */

/*
 * Non-owning, dense session registry: O(1) expected lookup by game-owned
 * session id and fair round-robin flush iteration. The registry does not
 * own sessions. Explicit removal is preferred; dn_session_destroy also
 * detaches a still-registered session defensively in the single-threaded path.
 */
typedef struct dn_session_set dn_session_set;

typedef struct dn_session_set_flush_stats {
    size_t sessions_total;
    size_t sessions_visited;
    size_t sessions_pending;
    size_t bytes_sent;
} dn_session_set_flush_stats;

DN_API dn_result dn_session_set_create(
    dn_session_set **out_set,
    size_t initial_capacity
);
DN_API void dn_session_set_destroy(dn_session_set *set);
DN_API dn_result dn_session_set_add(
    dn_session_set *set,
    uint64_t session_id,
    dn_session *session
);
DN_API dn_result dn_session_set_remove(
    dn_session_set *set,
    uint64_t session_id,
    dn_session **out_session
);
DN_API dn_session *dn_session_set_find(
    const dn_session_set *set,
    uint64_t session_id
);
DN_API size_t dn_session_set_count(const dn_session_set *set);

typedef struct dn_session_set_memory_stats {
    size_t session_count;
    size_t session_capacity;
    size_t map_capacity;
    size_t activity_slot_capacity;
    size_t activity_dirty_capacity;
    size_t activity_retained_bytes;
    size_t flush_queue_capacity;
} dn_session_set_memory_stats;

DN_API dn_result dn_session_set_get_memory_stats(
    const dn_session_set *set,
    dn_session_set_memory_stats *out_stats
);

typedef struct dn_session_set_activity_stats {
    size_t sessions_registered;
    size_t sessions_ready;
    size_t sessions_reliable;
    size_t sessions_unreliable;
    size_t sessions_retransmit;
    size_t sessions_timed;
    size_t sessions_due;
    size_t flush_queue_depth;
    size_t sessions_selected_last_pass;
    size_t idle_sessions_skipped_last_pass;
    uint64_t flush_passes;
    uint64_t idle_sessions_skipped_total;
} dn_session_set_activity_stats;

DN_API dn_result dn_session_set_get_activity_stats(
    const dn_session_set *set,
    dn_session_set_activity_stats *out_stats
);
/* Dense deterministic order for metrics/debugging; NULL out of range. */
DN_API dn_session *dn_session_set_at(
    const dn_session_set *set,
    size_t index,
    uint64_t *out_session_id
);
/*
 * Fair global flush. byte_budget is shared across all visited sessions;
 * per_session_byte_budget prevents one peer monopolizing the pass.
 * Only active sessions are visited. max_sessions == 0 visits every session
 * that was ready at the beginning of the pass. A zero byte budget value
 * means unlimited for that budget. Returns DN_AGAIN while ready work remains.
 */
DN_API dn_result dn_session_set_flush_all(
    dn_session_set *set,
    uint64_t now_ns,
    size_t byte_budget,
    size_t per_session_byte_budget,
    size_t max_sessions,
    dn_session_set_flush_stats *out_stats
);
DN_API uint64_t dn_session_set_next_due_ns(const dn_session_set *set);

/* ------------------------------------------------------------------ */
/* Multi-peer UDP authority                                           */
/* ------------------------------------------------------------------ */

typedef struct dn_udp_endpoint {
    /* Host-order IPv4 address and port. */
    uint32_t ipv4;
    uint16_t port;
} dn_udp_endpoint;

typedef struct dn_udp_server dn_udp_server;

typedef struct dn_udp_server_config {
    const char *bind_address; /* required, IPv4 "host:port" */
    size_t max_sessions;
    size_t inbound_packets_per_session;
    size_t max_packet_size;
    const dn_registry *registry;
    const dn_session_config *session_config; /* copied; NULL = defaults */
} dn_udp_server_config;

typedef struct dn_udp_server_stats {
    uint64_t datagrams_received;
    uint64_t bytes_received;
    uint64_t sessions_accepted;
    uint64_t sessions_removed;
    uint64_t capacity_drops;
    uint64_t inbound_queue_drops;
    uint64_t protocol_errors;
    size_t sessions;
} dn_udp_server_stats;

/* Return false to reject a newly-created endpoint/session. */
typedef bool (*dn_udp_server_accept_fn)(
    void *context,
    const dn_udp_endpoint *endpoint,
    dn_session *session
);

typedef void (*dn_udp_server_frame_fn)(
    void *context,
    const dn_udp_endpoint *endpoint,
    dn_session *session,
    const dn_frame_header *header,
    const uint8_t *payload
);

DN_API dn_result dn_udp_server_create(
    dn_udp_server **out_server,
    const dn_udp_server_config *config
);
DN_API void dn_udp_server_destroy(dn_udp_server *server);
DN_API dn_result dn_udp_server_local_port(
    const dn_udp_server *server,
    uint16_t *out_port
);
/*
 * recvfrom until EAGAIN or datagram_budget (0 = unlimited), demux by
 * endpoint, then pump only sessions touched by this receive pass.
 */
DN_API dn_result dn_udp_server_receive_all(
    dn_udp_server *server,
    uint64_t now_ns,
    size_t datagram_budget,
    dn_udp_server_accept_fn accept,
    dn_udp_server_frame_fn frame,
    void *context,
    size_t *out_datagrams
);
DN_API dn_result dn_udp_server_flush_all(
    dn_udp_server *server,
    uint64_t now_ns,
    size_t byte_budget,
    size_t per_session_byte_budget,
    size_t max_sessions,
    dn_session_set_flush_stats *out_stats
);
DN_API dn_session *dn_udp_server_find_session(
    const dn_udp_server *server,
    const dn_udp_endpoint *endpoint
);
DN_API dn_result dn_udp_server_remove_session(
    dn_udp_server *server,
    const dn_udp_endpoint *endpoint
);
DN_API size_t dn_udp_server_session_count(const dn_udp_server *server);
DN_API const dn_udp_server_stats *dn_udp_server_get_stats(
    const dn_udp_server *server
);

typedef struct dn_udp_server_memory_stats {
    size_t session_capacity;
    size_t sessions_live;
    size_t endpoint_map_capacity;
    size_t inbound_packets_per_session;
    size_t max_packet_size;
} dn_udp_server_memory_stats;

DN_API dn_result dn_udp_server_get_memory_stats(
    const dn_udp_server *server,
    dn_udp_server_memory_stats *out_stats
);
DN_API dn_result dn_udp_endpoint_parse(
    const char *address,
    dn_udp_endpoint *out_endpoint
);
DN_API dn_result dn_udp_endpoint_format(
    const dn_udp_endpoint *endpoint,
    char *output,
    size_t output_capacity
);

/* ------------------------------------------------------------------ */
/* Replication batcher (encode once, deliver to many)                 */
/* ------------------------------------------------------------------ */

typedef struct dn_replication dn_replication;

typedef struct dn_replication_config {
    /* Tick arena for encoded frames; grows geometrically, reused. */
    size_t initial_arena_bytes;
} dn_replication_config;

DN_API dn_result dn_replication_create(
    dn_replication **out_replication,
    const dn_registry *registry,
    const dn_replication_config *config
);
DN_API void dn_replication_destroy(dn_replication *replication);

/* Resets the arena and advances the borrow epoch. */
DN_API dn_result dn_replication_begin_tick(
    dn_replication *replication,
    uint64_t tick
);

DN_API dn_result dn_replication_group_begin(dn_replication *replication);

/* Encode one frame into the shared tick arena (exactly once). */
DN_API dn_result dn_replication_group_write(
    dn_replication *replication,
    uint16_t opcode,
    uint32_t request_id,
    const uint8_t *payload,
    size_t payload_size
);

typedef struct dn_replication_frame_request {
    uint16_t opcode;
    uint32_t request_id;
    const uint8_t *payload;
    size_t payload_size;
} dn_replication_frame_request;

DN_API size_t dn_replication_group_write_many(
    dn_replication *replication,
    const dn_replication_frame_request *requests,
    dn_result *out_results,
    size_t count,
    size_t work_budget
);

/*
 * Publish the open group to every recipient session. Sequenced and
 * unreliable entries borrow the shared arena bytes (valid until the
 * next begin_tick; flush before then). Reliable entries are copied
 * into each session's owned reliable lane.
 */
DN_API dn_result dn_replication_group_end(
    dn_replication *replication,
    dn_session *const *recipients,
    size_t recipient_count
);

typedef struct dn_replication_tick_stats {
    uint64_t tick;
    size_t groups;
    size_t frames_encoded;
    size_t bytes_encoded;
    size_t recipient_frame_refs;
} dn_replication_tick_stats;

DN_API const dn_replication_tick_stats *dn_replication_get_tick_stats(
    const dn_replication *replication
);

typedef struct dn_replication_memory_stats {
    size_t arena_block_count;
    size_t arena_block_capacity;
    size_t arena_bytes_capacity;
    size_t group_frame_capacity;
    size_t recipient_scratch_capacity;
} dn_replication_memory_stats;

DN_API dn_result dn_replication_get_memory_stats(
    const dn_replication *replication,
    dn_replication_memory_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Family-wide allocation metrics                                     */
/* ------------------------------------------------------------------ */

typedef struct dn_allocation_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    uint64_t growth_operations;
    size_t live_object_count;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dn_allocation_metrics;

/*
 * Process-wide metrics for memory owned by this library. live_object_count
 * is the number of retained heap objects currently owned by the module. Growth
 * and peak counters are lifetime values. Reset starts a post-prewarm window by
 * clearing failure and steady-state allocation counters only.
 */
DN_API void dn_get_allocation_metrics(dn_allocation_metrics *out_metrics);
DN_API void dn_reset_allocation_counters(void);

#ifdef __cplusplus
}
#endif

#endif
