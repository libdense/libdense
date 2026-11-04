# Network modes in v0.4.5

Legacy framing remains the default and recommended starting point. Compact is
an explicit SDK choice. Both endpoints must select compatible profiles and
schemas. `dense_net_wire.h` is installed with the other public headers.

| Mode | Availability | Main cost or limit |
| --- | --- | --- |
| Legacy | Default SDK and demos | 16-byte frame headers |
| Compact | SDK and both demos | Negotiation and bounded connection state; 6-/8-byte frame headers |
| Grouped | Public SDK and Arena | Schema-specific rosters, acknowledged baselines and history |
| Two-byte Mithsteel | Public SDK and Arena | Prediction work, exact corrections and periodic baselines |

Run these in separate terminals from the extracted release directory:

```sh
bin/linux-x86_64/arena-server --bind 127.0.0.1:27015 --wire mithsteel
bin/linux-x86_64/arena-bot --connect 127.0.0.1:27015 --wire mithsteel
```

Choose `legacy`, `compact`, `grouped` or `mithsteel` on both Arena endpoints.
Driving uses `driving-server` and `driving-bot`, with `legacy` or `compact`.
Omitting `--wire` selects legacy. These are unauthenticated local/LAN demos;
connection tags are not authentication. They have no SDL or graphical-client
dependency. Demo implementation source is not included.

The Arena adapter reconstructs all 48 bytes of each selected snapshot. Its
two-byte code chooses one of 65,536 predictor patterns; exact corrections carry
remaining differences. `--mithsteel-budget N` accepts 0-16384 prediction trials
per tick. Exhaustion sends explicit fields. Missing baseline readiness uses
full-identity state until acknowledged. This is not a promise of two bytes for
every delivered update or an arbitrary-schema serializer.

The demos support 256 entities and 64 peers. Grouped/Mithsteel reserve 1356256
application bytes on the server and 28472 per selected client, before SDK queues
and simulation allocations. Mithsteel refreshes active baselines every 20 ticks;
the sender retains 64 ticks of history and receivers retain two assets per
group with at most 128 pending samples. Repair deadline: 2.5 seconds. Expired
history, invalid coverage or exhausted queues closes the connection.

Custom applications use the [grouped/Mithsteel SDK API](grouped-state.md),
optionally with the [compact API](compact-wire.md). The public codec defines its
48-byte state schema and supports 65536 group indices. The 256-entity and 64-peer
limits above belong to the supplied application adapter. Applications define
admission, ACKs, history, capacities and recovery policy. The [worker SDK](delivery-workers.md) is available separately. [Time controls](time-dilation.md) are also independently available; Oracle and one-byte Mithsteel remain
experimental.
