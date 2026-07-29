# libdense_sim Rust binding

A dependency-free safe Rust wrapper over the canonical `dense_sim.h` C ABI.

The crate contains a small handwritten FFI module and a safe ownership layer.
Spatial execution, subscriptions, fanout grouping, and kinetic scheduling remain
inside `libdense_sim`.

## Build and test

The build script links the prebuilt static `libdense_sim.a` from
`lib/linux-x86_64` at the package root by default:

```bash
cargo test --manifest-path bindings/rust/Cargo.toml
cargo run --manifest-path bindings/rust/Cargo.toml --example basic_world
```

Override the search directory with:

```bash
DENSE_SIM_LIB_DIR=/path/to/lib cargo test \
    --manifest-path bindings/rust/Cargo.toml
```

## Ownership and threading

`World` owns one `ds_world` and destroys it in `Drop`.

Safe mutation methods require `&mut World`. `FanoutView<'world>` borrows
`&'world World`, so Rust prevents a successful `begin_tick`, movement, entity
mutation, observer mutation, or world destruction while a borrowed fanout view
is live.

`World` is `Send` but not `Sync`. A whole world may be moved to another thread,
but safe Rust cannot concurrently mutate or share one world across threads.
This matches the v0.1 one-world/one-writer rule while allowing separate worlds
on separate worker threads.

`ChunkDeltaView` borrows the finalized C group. Delta iteration returns copied
`DeltaEntry` values. Subscriber slices borrow the C span and cannot outlive the
parent fanout borrow.
