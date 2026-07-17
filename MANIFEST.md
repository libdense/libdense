# Release Manifest

Release: `0.1.0-rc1`

## Native artifacts

```text
lib/linux-x86_64/libdense_sim.so.0.1.0
lib/linux-x86_64/libdense_sim.so.0 -> libdense_sim.so.0.1.0
lib/linux-x86_64/libdense_sim.so -> libdense_sim.so.0
lib/linux-x86_64/libdense_sim.a
lib/linux-x86_64/libdensedb.so.0.1.0
lib/linux-x86_64/libdensedb.so.0 -> libdensedb.so.0.1.0
lib/linux-x86_64/libdensedb.so -> libdensedb.so.0
lib/linux-x86_64/libdensedb.a
```

## Public headers

```text
include/dense/dense_sim.h
include/dense/densedb.h
```

## Bindings

- CPython source and CPython 3.13/3.14 Linux x86-64 wheels;
- header-only C++20 wrapper source;
- dependency-free Rust wrapper source.

## Explicit exclusions

The release does not contain core implementation `.c` files, private core
headers, core object files, core test source, core benchmark source, development
Git metadata, or images.
