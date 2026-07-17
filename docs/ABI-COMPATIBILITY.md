# ABI Compatibility

The public headers define `DS_ABI_VERSION=1` and `DDB_ABI_VERSION=1`.

Compatibility records are retained in:

```text
release/api/dense_sim.api
release/api/densedb.api
release/abi/linux-x86_64.txt
```

The verifier compares exported native symbols against the normalized public API
snapshots and checks the shared-library SONAMEs.

Within one ABI major version, consumers should still rebuild when public struct
layout or function declarations change during a prerelease series. The current
release is `0.1.0-rc1`, not a final 1.0 ABI commitment.

## Borrowed views

Fanout and WATCH views point into library-owned memory. They must not be retained
past their documented invalidation boundary. Language bindings preserve these
rules through generation checks or language lifetimes where possible.
