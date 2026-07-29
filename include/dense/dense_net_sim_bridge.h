#ifndef DENSE_NET_SIM_BRIDGE_H
#define DENSE_NET_SIM_BRIDGE_H

/*
 * Optional bridge between libdense_sim's grouped fanout plans and the
 * libdense_net replication batcher. This is the only libdense_net
 * header that includes dense_sim.h; build it only when libdense_sim is
 * available (make DENSE_SIM_DIR=/path/to/dense/libdense_sim).
 */

#include "dense_net.h"

#include <dense_sim.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Encode one chunk delta's entries into the open replication group.
 * Call dn_replication_group_write once per frame to emit. Returning a
 * non-DN_OK result aborts the publish and is passed through.
 */
typedef dn_result (*dn_sim_bridge_encode_fn)(
    void *context,
    dn_replication *replication,
    const ds_chunk_delta *chunk
);

/*
 * Resolve an observer id to its session. Return NULL for observers
 * with no live session (their share of the group is skipped).
 */
typedef dn_session *(*dn_sim_bridge_resolve_fn)(
    void *context,
    ds_observer_id observer_id
);


typedef struct dn_sim_bridge_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    size_t recipient_capacity;
    size_t recipient_high_water;
    uint64_t growth_operations;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dn_sim_bridge_metrics;

/*
 * Reserve retained recipient scratch explicitly. The scratch is owned by
 * the replication batcher and reused across every subsequent publish.
 */
DN_API dn_result dn_sim_bridge_reserve(
    dn_replication *replication,
    size_t recipient_capacity
);

/*
 * Reserve for the largest subscriber span in a finalized fanout view and
 * mark the bridge as prewarmed. Any later publish-time growth is counted
 * as a steady-state allocation.
 */
DN_API dn_result dn_sim_bridge_prewarm(
    dn_replication *replication,
    const ds_fanout_view *view
);

/*
 * Start a representative no-allocation measurement window. Capacity and
 * high-water values are retained; failure and steady-state counters reset.
 */
DN_API void dn_sim_bridge_reset_allocation_counters(
    dn_replication *replication
);

DN_API dn_result dn_sim_bridge_get_metrics(
    const dn_replication *replication,
    dn_sim_bridge_metrics *out_metrics
);

/*
 * Publish a finalized fanout view: one replication group per chunk
 * delta, encoded once, delivered to every resolved subscriber session.
 * Call between dn_replication_begin_tick and the sessions' flushes.
 */
DN_API dn_result dn_sim_bridge_publish(
    dn_replication *replication,
    const ds_fanout_view *view,
    dn_sim_bridge_encode_fn encode,
    dn_sim_bridge_resolve_fn resolve,
    void *context
);

#ifdef __cplusplus
}
#endif

#endif
