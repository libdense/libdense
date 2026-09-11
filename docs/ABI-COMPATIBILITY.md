# ABI Compatibility

The public headers define `DS_ABI_VERSION=1` and `DDB_ABI_VERSION=2`.

Compatibility records are retained in:

```text
release/api/dense_sim.api
release/api/densedb.api
release/abi/linux-x86_64.txt
release/abi/linux-aarch64.txt
```

The verifier compares exported native symbols against the normalized public API
snapshots and checks the shared-library SONAMEs. The
`release/api/dense_sim-0.3.0.api` snapshot retains the pre-extension baseline;
`release/api/dense_sim.api` records the 0.3.8 version of the unchanged 0.3.6 public surface.

Within one ABI major version, consumers should still rebuild when public struct
layout or function declarations change during a prerelease series. The current
aggregate release is `0.3.8`, not a final 1.0 ABI commitment.

## Borrowed views

Fanout and WATCH views point into library-owned memory. They must not be retained
past their documented invalidation boundary. Language bindings preserve these
rules through generation checks or language lifetimes where possible.

## Simulation 0.3.8

The 0.3.8 changes preserve all 0.3.6 function declarations, structure layouts,
and exported symbols. `membership_capacity` remains a public diagnostic and
reports zero since 0.3.7. `fanout_subscriber_capacity` reports retained UPDATE
exclusion scratch; use allocation metrics for total retained memory.

Fanout group and entry order can differ from 0.3.6. Group indices are not
persistent identities. 0.3.8 preserves 0.3.7 ordering.
