#ifndef DENSE_NET_GROUPED_H
#define DENSE_NET_GROUPED_H

#include "dense_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/* optional, allocation-free grouped state and two-byte Mithsteel codec.
 * all integers on the wire are little endian. serialize through state_write;
 * never memcpy a C struct onto the wire. this version implements the documented
 * 48-byte state schema, not arbitrary C objects. unused fields may be zero.
 * both endpoints must negotiate the same schema and width (0 or 2).
 * no global state, implicit workers, clocks, network I/O or allocation.
 * concurrent calls require disjoint mutable buffers, assets, budgets and stats.
 */
#define DN_GROUPED_VERSION 1u
#define DN_GROUPED_SLOTS 16u
#define DN_GROUPED_STATE_SIZE 48u
#define DN_GROUPED_HEADER_SIZE 20u
#define DN_GROUPED_MAX_SIZE 800u
#define DN_GROUPED_WIDTH_FIELDS 0u
#define DN_GROUPED_WIDTH_MITHSTEEL 2u

enum {
    DN_GROUPED_RAW,
    DN_GROUPED_FIELDS,
    DN_GROUPED_TEMPLATE,
    DN_GROUPED_ASSET
};

typedef struct dn_grouped_state {
    uint64_t id, tick;
    int32_t x, y, vx, vy;
    uint16_t heading, health;
    uint32_t score, flags, input_sequence;
} dn_grouped_state;

/* caller-owned immutable baseline while encoding/decoding. initialize to zero.
 * group is 0..65535; epoch and tick must be nonzero. never reuse an epoch for a
 * different baseline in a connection. mask selects occupied slots. full_mask
 * marks slots containing full state, rather than just identity and tick.
 * populate all selected states with state_write; set full_mask=mask for a full
 * locally captured baseline. do not invent full_mask on a decoded roster.
 */
typedef struct dn_grouped_asset {
    uint32_t group, epoch;
    uint64_t tick;
    uint16_t mask, full_mask;
    uint8_t state[DN_GROUPED_SLOTS][DN_GROUPED_STATE_SIZE];
} dn_grouped_asset;

typedef struct dn_grouped_stats {
    uint64_t records, exceptions, correction_bytes, code_bytes;
    uint64_t trials, fallback, exhausted;
} dn_grouped_stats;

/* write/read reject zero identity or tick and leave output unchanged on error.
 * input and output must not overlap. every array argument requires its declared
 * storage size; header/decode additionally take the actual received byte count.
 */
DN_API bool dn_grouped_state_write(uint8_t out[48], const dn_grouped_state *state);
DN_API bool dn_grouped_state_read(dn_grouped_state *out, const uint8_t state[48]);

/* validates only the 20-byte header, not the body or application schema.
 * mask requires at least two readable bytes; NULL returns zero.
 */
DN_API uint16_t dn_grouped_mask(const uint8_t *data);
DN_API bool dn_grouped_header(const uint8_t *data, size_t size);

/* return encoded byte count, or zero on invalid input (output then unspecified).
 * full=false sends identity/tick only; full=true requires full_mask coverage.
 * decode validates the entire body before modifying out. matching group/epoch
 * assets merge coverage; conflicts reject. a different identity replaces out.
 * the caller must reject stale epochs before choosing a cache slot.
 */
DN_API size_t dn_grouped_asset_encode(uint8_t out[DN_GROUPED_MAX_SIZE],
    const dn_grouped_asset *asset, uint16_t mask, bool full);
DN_API bool dn_grouped_asset_decode(dn_grouped_asset *out,
    const uint8_t *data, size_t size);

/* one group per call; mask is the application's already selected audience data.
 * pass a baseline only for recipients that ACKed that exact epoch and coverage.
 * NULL or changed identities use raw fallback. width=0 groups explicit fields;
 * width=2 tries Mithsteel only with a full baseline, sufficient *budget and
 * tick at/after the baseline and distance <=1024. stale baselines fall back
 * without spending budget or counting trials/exhaustion. the caller owns and
 * replenishes the shared budget. one trial per selected slot, exact correction,
 * explicit fields if prediction is not smaller. budget and stats are required.
 * encode once per coverage/readiness cohort and share through dn_replication.
 * buffers/state/base must not overlap. zero indicates invalid input; counters
 * and budget are unchanged in that case. stats accumulate, modulo uint64_t.
 */
DN_API size_t dn_grouped_encode(uint8_t out[DN_GROUPED_MAX_SIZE], uint32_t group,
    uint64_t tick, uint16_t mask, const uint8_t state[DN_GROUPED_SLOTS][48],
    const dn_grouped_asset *base, unsigned width, unsigned *budget,
    dn_grouped_stats *stats);

/* 1: decoded; 0: required asset/full coverage missing; -1: invalid.
 * no output change for 0/-1. on success unselected slots are zero.
 * caller checks expected width, group, epoch, membership and connection before
 * applying state, and supplies bounded ACK/history/repair/deadline policy.
 * missing state must never be applied approximately. the codec does not manage
 * transport reliability, visibility, request IDs or simulation tick cadence.
 */
DN_API int dn_grouped_decode(uint8_t out[DN_GROUPED_SLOTS][48],
    const uint8_t *data, size_t size, const dn_grouped_asset *base);

/* built-in deterministic velocity predictor: two signed 8-bit indices on a
 * three-unit grid. heading and input sequence are predicted as documented.
 * code returns zero for NULL/unsupported width. predict returns false without
 * modifying out for invalid input. width must be two; no learned dictionary.
 */
DN_API uint16_t dn_grouped_code(const uint8_t state[48], unsigned width);
DN_API bool dn_grouped_predict(uint8_t out[48], const uint8_t base[48],
    uint64_t tick, uint16_t code, unsigned width);

#ifdef __cplusplus
}
#endif
#endif
