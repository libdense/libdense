# Release Manifest

Release: `0.3.8`

Component versions:

```text
libdense_sim:                                      0.3.8
libdense_net:                                      0.3.5
libdense_collision, nav, sched, ai, DenseDB:       0.3.0
Python, C++, and Rust wrapper packages:            0.3.8
```

## Native artifacts

The repository contains release metadata and wrapper source. Native SDKs and
wheels populate the paths below during separate binary assembly.

Seven libraries, each shipped as a versioned shared object with relative
SONAME links plus a self-contained static archive:

```text
lib/linux-x86_64/libdense_sim.so.0.3.8        libdense_sim.a
lib/linux-x86_64/libdense_net.so.0.3.5        libdense_net.a
lib/linux-x86_64/libdense_sched.so.0.3.0      libdense_sched.a
lib/linux-x86_64/libdense_collision.so.0.3.0  libdense_collision.a
lib/linux-x86_64/libdense_nav.so.0.3.0        libdense_nav.a
lib/linux-x86_64/libdense_ai.so.0.3.0         libdense_ai.a
lib/linux-x86_64/libdensedb.so.0.3.0          libdensedb.a
```

Every shared library keeps SONAME major `0`. Unversioned and SONAME links are
relative links to the corresponding component-versioned file.
The private `dense_core` shared-primitive archive is statically merged into
each artifact with hidden visibility and is not shipped separately.

CI also publishes static SDK artifacts for Linux ARM64 (headless server) and
Windows x86-64 (client). The Windows SDK omits DenseDB. Each portable SDK
contains its own architecture-locked CMake config package.

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

- CPython source and CPython 3.11-3.14 wheels for `dense_sim` on Linux x86-64,
  Linux ARM64, and Windows x86-64;
- header-only C++20 wrapper source;
- dependency-free Rust wrapper source.

## Release records

```text
release/abi/        exported-symbol lists and Linux ABI layout baselines
release/api/        current API snapshots plus the dense_sim 0.3.0 baseline
release/benchmarks/ 0.3.8 benchmark output and dated historical records
```

## Explicit exclusions

The release does not contain core implementation `.c` files, private core
headers, core object files, the private `dense_core` archive, core test or
benchmark source, internal phase notes, development reports, game assets, or
development Git metadata. Binding source under `bindings/` is intentionally
included.
