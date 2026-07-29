# Replay and Audit Logging

Phase 6 adds an integration-owned append-only replay format for deterministic
server reproduction and audit tooling. It is deliberately not installed as a
new public library: the game/server owns its long-term input-log policy while
the Dense repository provides a tested reference implementation and harness.

## Deterministic inputs

A replay is bound to three fingerprints in its fixed header:

```text
configuration fingerprint
initial-state fingerprint
Dense family/library fingerprint
```

The reader rejects an expected-header mismatch before returning records. The
library fingerprint is a compile-time manifest of the Dense 0.2 family and its
public ABI generations. An intentional version or ABI change must update that
manifest and the replay baseline.

## Record stream

All integers are canonical little-endian values. Every header and record has an
FNV-1a checksum over its canonical bytes. Records have a monotonically
increasing sequence number and are ordered by tick.

The format records:

- connection, disconnection, and endpoint-rebind lifecycle events;
- the game-owned session ID;
- raw inbound frame bytes after demultiplexing identity is known;
- one authoritative checksum closing every tick.

A tick checksum closes that tick. No later lifecycle or frame record may be
appended to it. The audit pass requires contiguous closed ticks, validates that
frames belong to connected sessions, and rejects invalid lifecycle order.

## Crash-tail behavior

`dense_replay_recover_tail()` scans to the last checksum-closed tick and
truncates an incomplete, checksum-invalid, or sequence-invalid tail. It never
manufactures a missing tick checksum. Corruption therefore loses at most the
unclosed tail and remains visible through the reported truncated-byte count.

## Harness coverage

The standing harness executes this order:

```text
input demux
clock translation and input validation
movement proposal
collision validation
simulation commit
combat
AI slicing
navigation work
fanout generation
network sparse flush selection
DenseDB write-behind buffer swap and bounded flush
```

A record run writes the log and per-tick checksum file. A second release run
replays the recorded bytes. A third ASan/UBSan run replays the same file. All
240 per-tick checksums and the final checksum must match the pinned baselines.

## Commands

```bash
make test-determinism
make test-replay
make test-phase6-audit
make benchmark-phase6-replay
```

The determinism target leaves its generated log at:

```text
integration/build/determinism/input.replay
```

Inspect or validate a log:

```bash
make replay-audit
make replay-dump
```

Supply another path with `REPLAY_LOG`:

```bash
make replay-audit REPLAY_LOG=/path/to/server.replay
make replay-dump REPLAY_LOG=/path/to/server.replay
```

Recover only a damaged append tail:

```bash
make replay-recover REPLAY_LOG=/path/to/server.replay
```

Keep an original copy before recovery when the file is evidence for a cheat or
incident investigation.
