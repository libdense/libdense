# Changelog

## 0.1.0-rc1 — 2026-07-16

First release candidate for the Dense platform.

### libdense_sim

- Dense entity slots with arbitrary 64-bit IDs.
- Sparse cell/chunk spatial membership and centralized crossings.
- Channel-aware dirty tracking.
- Moving observers and persistent spatial subscriptions.
- Exact grouped fanout plans with borrowed views.
- Selective sampled and kinetic motion backends.
- C ABI plus Python, C++, and Rust wrapper sources.

### DenseDB

- Immutable table schemas and structure-of-arrays channel storage.
- Fixed and entity-following WATCH subscriptions.
- Borrowed SNAPSHOT/ENTER/UPDATE/LEAVE streams.
- Tick-commit WAL, atomic snapshots, and recovery.

### Release-candidate hardening

- Frozen public C header baselines and Linux x86-64 ABI layout baseline.
- Stable shared-library SONAMEs.
- Staged install and pkg-config integration tests.
- Deterministic durability parser mutation fuzzing.
- Filesystem failure injection for WAL and checkpoint paths.
- Durable restart/checkpoint soak testing.
- Architecture benchmark regression gates.
