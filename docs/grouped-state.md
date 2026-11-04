# Grouped state and two-byte Mithsteel SDK

`dense_net_grouped.h` is part of the installed C SDK. Its `dn_grouped_*` functions
are exported by the normal static and shared `libdense_net` libraries. Calling
these functions explicitly enables the codec for an application's payloads.
No experimental build flag, game executable, worker pool or global mode is needed.
Legacy networking remains the default.

The API implements the same 48-byte state schema and predictor used by the
validated grouped/Mithsteel integration. It is reusable by applications that
serialize this schema. It is not an arbitrary-structure serializer. Applications
can leave unused fields zero; changing their meaning, units or predictor requires
a distinct negotiated schema. Python, C++ and Rust simulation wrappers do not
currently wrap these network APIs; C++ can include the C header directly.

## Minimal complete consumer

```c
#include "dense/dense_net_grouped.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    dn_grouped_asset captured = {0}, installed = {0};
    captured.group = 42u;
    captured.epoch = 1u;
    captured.tick = 10u;
    captured.mask = 1u;
    captured.full_mask = 1u;
    dn_grouped_state state = {0};
    state.id = 123u;
    state.tick = 10u;
    state.vx = 3;
    state.health = 100u;
    state.input_sequence = 10u;
    assert(dn_grouped_state_write(captured.state[0], &state));

    uint8_t asset[DN_GROUPED_MAX_SIZE], packet[DN_GROUPED_MAX_SIZE];
    size_t size = dn_grouped_asset_encode(asset, &captured, 1u, true);
    assert(size != 0u);
    assert(dn_grouped_asset_decode(&installed, asset, size));
    /* the application must receive an ACK for this epoch and coverage before
     * selecting captured as the sender baseline for that recipient. */

    uint8_t current[DN_GROUPED_SLOTS][48] = {{0}};
    uint8_t restored[DN_GROUPED_SLOTS][48];
    state.tick = 11u;
    state.x = 3;
    state.input_sequence = 11u;
    assert(dn_grouped_state_write(current[0], &state));
    unsigned budget = 16u;
    dn_grouped_stats stats = {0};
    size = dn_grouped_encode(packet, 42u, 11u, 1u,
        (const uint8_t (*)[48])current, &captured,
        DN_GROUPED_WIDTH_MITHSTEEL, &budget, &stats);
    assert(size != 0u);
    assert(dn_grouped_decode(restored, packet, size, &installed) == 1);
    assert(memcmp(current[0], restored[0], 48u) == 0);
    printf("Exact state: %zu encoded bytes\n", size);
    return 0;
}
```

Save that program as `grouped-consumer.c`. With an installed SDK:

```bash
cc -std=c11 -Wall -Wextra grouped-consumer.c -ldense_net -o grouped-consumer
./grouped-consumer
```

Use `DN_GROUPED_WIDTH_FIELDS` to select grouped explicit fields. For Mithsteel,
`budget` counts predictor trials shared across calls within your chosen interval.
Reset it explicitly. Insufficient budget, an incomplete prediction baseline, an
unsupported prediction horizon, or corrections larger than explicit fields uses
exact explicit fields. Missing identities use full-identity raw state. The codec
never reduces accuracy or chooses a slower publication schedule.

## State schema

| Byte offset | Field | Type |
| --- | --- | --- |
| 0 | Full entity identity, including application incarnation | uint64, nonzero |
| 8 | Publication tick | uint64, nonzero |
| 16, 20 | X, Y | int32 |
| 24, 28 | Velocity X, Y | int32 |
| 32 | Heading | uint16 |
| 34 | Health | uint16 |
| 36 | Score | uint32 |
| 40 | Flags | uint32 |
| 44 | Applied input sequence | uint32 |

Wire integers are little endian. Use `dn_grouped_state_write/read`, not native
struct copies. A group has 16 slots and a 16-bit coverage mask. Group indices
range from 0 through 65535, independently of entity IDs. This is a wire group
namespace, not an allocation of storage for every possible group.

The two-byte code contains two signed 8-bit velocity indices on a three-unit
grid. Position is predicted from the baseline and elapsed ticks, saturating at
int32 limits. Nonzero motion predicts an eight-direction heading in a 256-unit
circle; input sequence advances with elapsed ticks and saturates at UINT32_MAX.
All differences, including unrelated application field values, receive exact
signed residual corrections. There are 65,536 possible codes. This does not mean
an entire packet or every state update occupies two bytes. The publication tick
must be within 1024 ticks after the full baseline for prediction to succeed.

The 20-byte header contains coverage, group, epoch, tick, width, mode and reserved
zero bytes. A full 16-slot baseline or raw group uses 660 bytes. The declared
800-byte buffer includes room for the encoder's bounded trial work. A perfectly
predicted 16-slot template occupies 54 bytes before transport framing. Costs of
baselines, ACKs, corrections and repairs must be included in bandwidth accounting.
The state packet's first eight bytes can be a sequenced replacement key.

## Integrating delivery

1. Negotiate an application schema that includes this codec version, field
   semantics and the selected width. Use the compact-wire negotiation API if
   desired; the codec itself is independent of framing.
2. Choose visibility and publication masks using the application's existing
   policy. Capture full IDs and state before mutating the next tick.
3. Send the required asset coverage reliably. `full=false` sends an identity
   roster for grouped fields. Mithsteel needs `full=true` and a local
   `full_mask` covering every predicted slot. Decode assets into zero-initialized
   caches. Reject stale epochs and conflicting connection incarnations.
4. Match the peer's ACK to group, epoch and coverage. Before that ACK, pass NULL
   to the encoder for that peer's cohort. Never treat queue admission as an ACK.
5. Encode once for each distinct coverage/readiness cohort. Supply the resulting
   bytes to `dn_replication_group_write` and distribute using
   `dn_replication_group_end`. Register an application-owned variable-size opcode
   with bounds 20..800. Do not combine distinct per-entity request IDs silently.
6. Check the expected group and width before decoding. A result of 1 supplies
   exact selected state; 0 means a prerequisite is missing; -1 is invalid.
   Results 0/-1 leave the output untouched. Do not apply it. Unselected output
   slots are zero on success.
7. Use bounded history and reliable repair of the exact named sample, or an
   explicit resynchronization/disconnect policy. Keep reliable events separate.
   Never substitute a newer or approximate state for a missing exact sample.

Asset decoding merges compatible coverage for the same group, epoch and tick.
`full_mask` tracks which slots actually contain full state. A roster cannot
silently become a prediction baseline. Both sender and receiver must retain the
same immutable asset until dependent work completes. These functions deliberately
leave admission, ACKs, history retention and recovery policy to the application;
they do not start a connection manager.

## Cost and support

The codec allocates no heap storage and has no global mutable state. Each call
processes at most 16 selected records. Use separate mutable buffers, budgets and
statistics on concurrent threads. Callers own all cache/history memory. The
prediction budget bounds trials, not all encoding, transport or simulation work.

The Linux x86-64 candidate has direct public-API round-trip, malformed-packet,
full-baseline, sanitizer, installed-consumer and live adapter coverage recorded
in its release validation. Other platforms need native runtime validation.
Historical benchmark percentages describe their recorded workloads; public API
extraction is not a new performance measurement. One-byte Mithsteel and Oracle
remain experimental and are not exported by this header.
