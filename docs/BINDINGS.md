# Bindings

The C ABI is the canonical implementation boundary. Bindings do not reimplement
spatial indexing, subscription maintenance, kinetic scheduling, recipient
planning, or DenseDB durability.

## Python

Prebuilt wheels are provided for CPython 3.13 and 3.14 on Linux x86-64. They
statically include `libdense_sim`.

```bash
python3.14 -m pip install bindings/python/dist/*cp314*.whl
```

Build the included source against the packaged static archive:

```bash
make -C bindings/python PYTHON=python3.14 test
```

Overrides:

```text
DENSE_SIM_INCLUDE_DIR
DENSE_SIM_LIB_DIR
DENSE_SIM_STATIC_LIBRARY
```

The buffer-based batch APIs accept compatible contiguous native buffers and
avoid Python-element conversion.

## C++

The C++20 wrapper is header-only. It owns `ds_world` through RAII and translates
`ds_result` failures into `dense::Error`.

```bash
make -C bindings/cpp test
```

Borrowed fanout wrapper objects validate their generation before accessing C
spans and reject stale access after a successful `begin_tick()` or `close()`.

## Rust

The Rust wrapper is dependency-free and uses a handwritten FFI layer.

```bash
make -C bindings/rust test
```

`World` is `Send` but not `Sync`. `FanoutView` borrows the world, preventing
mutation or destruction through safe Rust while a borrowed view is live.

Environment overrides:

```text
DENSE_SIM_LIB_DIR=/path/to/native/libs
DENSE_SIM_LINK_MODE=static|dynamic
```
