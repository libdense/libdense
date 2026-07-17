# libdense_sim Rust binding

A dependency-free safe Rust wrapper over the canonical `dense_sim.h` C ABI.

```bash
make -C bindings/rust test
make -C bindings/rust example
```

The crate links `../../lib/linux-x86_64/libdense_sim.a` by default.

```bash
DENSE_SIM_LIB_DIR=/path/to/lib cargo test --manifest-path bindings/rust/Cargo.toml
DENSE_SIM_LINK_MODE=dynamic cargo test --manifest-path bindings/rust/Cargo.toml
```

`World` is `Send` but not `Sync`. `FanoutView` borrows `World`, so safe Rust
prevents mutation or destruction while a native fanout view is live.
