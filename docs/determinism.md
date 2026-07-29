# Determinism Policy

The authoritative server should be a deterministic function of:

```text
initial state
ordered input log
configuration fingerprint
library versions
```

## Required ordering

The Phase 6 harness records and replays the complete integration authority
order: input demultiplexing, clock translation and validation, movement
proposal, collision validation, simulation commit, combat, AI slicing,
navigation work, simulation fanout generation, network flush selection, and
DenseDB buffer swapping/bounded flush. `make test-phase6-audit` pins this order
and fails if the harness markers are removed or reordered.

## Build comparison

```bash
make test-determinism
```

The script builds the same harness with:

- `-O3`
- `-O1 -g -fsanitize=address,undefined`

The runner first records a replay, then replays the same raw input log in a
second release run and in an ASan/UBSan run. All 240 per-tick checksums must
match `integration/tests/expected_determinism_ticks.txt`, and the final checksum
must match `integration/tests/expected_determinism_checksum.txt`:

```text
f9ee2ebe47713aee
```

The Phase 1 checksum `85363eafdc0ad3da` is retained in the Phase 6 changelog as
the baseline for the smaller pre-replay harness. See `docs/replay-and-audit.md`.

## CPU dispatch

Call `ds_runtime_init()` during process startup before shard or worker threads
start. `ds_world_create()` retains a thread-safe fallback, but startup
initialization is the required production pattern. Dispatch selection must not
mutate shared function pointers lazily from live worker threads.

## Intentional checksum changes

An intentional deterministic behavior change must update the pinned checksum
and document the reason in the affected module changelog, the root changelog,
and the active phase changelog. A compiler-only or sanitizer-only checksum
change is a failure, not a baseline update.

Phase 8 intentionally advances the active baseline because replay now applies the shared overload policy to AI, navigation, network, database, region-admission, and tick-period decisions. The Phase 6 baseline `2ef94655f7a5e0df` remains historical.
