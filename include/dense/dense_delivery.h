#ifndef DENSE_DELIVERY_H
#define DENSE_DELIVERY_H

/* Opt-in local delivery SDK. See docs/delivery-workers.md for ownership rules.
 * Link libdense_delivery separately; no workers are started by libdense_net. */
#include "dense_net.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && defined(DD_SHARED) && defined(DD_BUILD)
#define DD_API __attribute__((visibility("default")))
#else
#define DD_API
#endif

#define DN_DELIVERY_PUBLICATION_VERSION 1u

typedef struct dn_publication dn_publication;
typedef struct dn_delivery_pool dn_delivery_pool;

typedef struct dn_publication_id {
    uint64_t world, epoch, sequence, tick;
} dn_publication_id;

typedef struct dn_delivery_recipient {
    uint64_t id, generation;
    size_t owner;
} dn_delivery_recipient;

typedef struct dn_publication_limits {
    size_t workers, audiences, groups, frames, recipients, encoded_bytes;
} dn_publication_limits;

/* All capacities are hard bounds, allocated at creation. The registry stays
 * immutable and alive through publication and pool destruction. */
DD_API dn_result dn_publication_create(dn_publication **out, const dn_registry *registry,
    const dn_publication_limits *limits);
DD_API dn_result dn_publication_destroy(dn_publication *publication);
DD_API dn_result dn_publication_begin(dn_publication *publication, dn_publication_id id);
/* Recipients must have strictly increasing, unique IDs and nonzero generations.
 * An audience may be reused by many groups within this publication. */
DD_API dn_result dn_publication_audience(dn_publication *publication,
    const dn_delivery_recipient *recipients, size_t count, size_t *out_audience);
DD_API dn_result dn_publication_group(dn_publication *publication, size_t audience);
/* Copies and validates application payload immediately; no caller bytes survive.
 * A new group or seal closes the current group. */
DD_API dn_result dn_publication_write(dn_publication *publication, uint16_t opcode,
    uint32_t request_id, const uint8_t *payload, size_t size);
DD_API dn_result dn_publication_seal(dn_publication *publication);
DD_API size_t dn_publication_retained_bytes(const dn_publication *publication);

/* Called on the recipient's exclusive worker. Return NULL on missing/stale
 * incarnation. Each distinct ID must resolve to a distinct session, exclusively
 * owned by the specified worker. No shared mutable UDP server/transport state. */
typedef dn_session *(*dn_delivery_resolve_fn)(void *context,
    uint64_t id, uint64_t generation);
/* Optional owner-local ingress/ACK pump, once per active worker per dispatch.
 * Must be bounded and must not change the publication's membership mapping. */
typedef dn_result (*dn_delivery_service_fn)(void *context, uint64_t now_ns);

typedef struct dn_delivery_config {
    size_t workers; /* 1 = inline caller, default; >1 = persistent POSIX workers */
    size_t recipient_capacity; /* maximum recipients on any one worker/group */
    uint64_t world, epoch, first_sequence;
    dn_delivery_resolve_fn resolve;
    void *const *contexts; /* one exclusive context per worker; copied */
    bool flush_each_group; /* default false matches bridge enqueue-then-flush */
    dn_delivery_service_fn service;
} dn_delivery_config;

typedef struct dn_delivery_status {
    size_t workers, completed_workers, blocked_workers, failed_worker;
    dn_result result;
    bool poisoned;
    uint64_t accepted_publications, rejected_submissions, resume_count;
    size_t adapter_retained_bytes; /* excludes replication/session allocations */
} dn_delivery_status;

DD_API void dn_delivery_config_defaults(dn_delivery_config *config);
DD_API dn_result dn_delivery_pool_create(dn_delivery_pool **out,
    const dn_registry *registry, const dn_delivery_config *config);
/* Single controller only. Exactly one publication can be in flight. Submit
 * rejects busy/duplicate/gap/wrong-epoch/capacity BEFORE consumer mutation.
 * Success is local admission, not delivery. Always wait after success. */
DD_API dn_result dn_delivery_submit(dn_delivery_pool *pool, dn_publication *publication,
    uint64_t now_ns);
/* DN_AGAIN retains the publication. Resume only flushes unfinished work, never
 * re-enqueues an admitted group. Hard or partial-admission failures poison the
 * pool: destroy it and explicitly rebuild/bootstrap before proceeding. */
DD_API dn_result dn_delivery_wait(dn_delivery_pool *pool, dn_delivery_status *status);
DD_API dn_result dn_delivery_resume(dn_delivery_pool *pool, uint64_t now_ns);
/* Joins active workers before releasing publication/replication storage. Does
 * not destroy application sessions. Never call from a worker callback. */
DD_API void dn_delivery_pool_destroy(dn_delivery_pool *pool);
/* fixed pool arrays, excluding replication/session allocations and thread stacks */
DD_API size_t dn_delivery_pool_retained_bytes(const dn_delivery_pool *pool);

#ifdef __cplusplus
}
#endif
#endif
