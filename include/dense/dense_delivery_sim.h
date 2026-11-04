#ifndef DENSE_DELIVERY_SIM_H
#define DENSE_DELIVERY_SIM_H
#include "dense_delivery.h"
#include "dense_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Authority-only callbacks. Encode while the view/application state is valid.
 * Map may return DN_ERR_NOT_FOUND to omit an observer without a live session. */
typedef dn_result (*dn_publication_map_fn)(void *context, ds_observer_id id,
    dn_delivery_recipient *out);
typedef dn_result (*dn_publication_encode_fn)(void *context, dn_publication *p,
    const ds_chunk_delta *chunk);

/* Copies exact audiences and encoded data, then seals. For an idle publication,
 * any failure invalidates its sealed state; begin again before reuse. A busy
 * publication is unchanged. Callbacks only map recipients or append payloads;
 * they must not begin, seal, submit, destroy or otherwise reenter the builder.
 * scratch is caller-owned authority storage, reused for each chunk. */
DD_API dn_result dn_publication_capture(dn_publication *p, dn_publication_id id,
    const ds_fanout_view *view, dn_publication_map_fn map,
    dn_publication_encode_fn encode, void *context,
    dn_delivery_recipient *scratch, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
