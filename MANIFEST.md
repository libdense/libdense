# Release Manifest

Release: `0.2.0`

## Native artifacts

Seven libraries, each shipped as a versioned shared object with relative
SONAME links plus a self-contained static archive:

```text
lib/linux-x86_64/libdense_sim.so.0.2.0        libdense_sim.a
lib/linux-x86_64/libdense_net.so.0.2.0        libdense_net.a
lib/linux-x86_64/libdense_sched.so.0.2.0      libdense_sched.a
lib/linux-x86_64/libdense_collision.so.0.2.0  libdense_collision.a
lib/linux-x86_64/libdense_nav.so.0.2.0        libdense_nav.a
lib/linux-x86_64/libdense_ai.so.0.2.0         libdense_ai.a
lib/linux-x86_64/libdensedb.so.0.2.0          libdensedb.a
```

For every library, `lib<name>.so -> lib<name>.so.0 -> lib<name>.so.0.2.0`.
The private `dense_core` shared-primitive archive is statically merged into
each artifact with hidden visibility and is not shipped separately.

## Public headers

```text
include/dense/dense_sim.h
include/dense/dense_net.h
include/dense/dense_net_sim_bridge.h
include/dense/dense_sched.h
include/dense/dense_collision.h
include/dense/dense_nav.h
include/dense/dense_nav_collision_bridge.h
include/dense/dense_ai.h
include/dense/densedb.h
```

## Bindings

- CPython source and CPython 3.13/3.14 Linux x86-64 wheels for `dense_sim`;
- header-only C++20 wrapper source;
- dependency-free Rust wrapper source.

## Release records

```text
release/abi/        exported-symbol lists for all seven libraries
release/api/        frozen public API snapshots (dense_sim, densedb)
release/benchmarks/ benchmark summaries and the 0.2.0 full-run record
```

## Explicit exclusions

The release does not contain core implementation `.c` files, private core
headers, core object files, the private `dense_core` archive, core test
source, core benchmark source, or development Git metadata. Binding source
under `bindings/` is intentionally included.
