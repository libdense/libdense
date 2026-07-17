# Dense (libdense_sim, DenseDB)

<p align="center">
  <img src="logo.png" alt="Dense logo" width="800">
</p>

**A high-density multiplayer state engine**

This is the public binary release repository for Dense 0.1.0-rc1. It contains
compiled `libdense_sim` and DenseDB libraries, public C ABI headers, language
bindings, compatibility records, installation scripts, and user documentation.

The private C implementation source for `libdense_sim` and DenseDB is not
included. Binding source is included so applications can inspect, build, and
integrate the supported language wrappers.

## Included

- `libdense_sim` shared and static Linux x86-64 libraries;
- `libdensedb` shared and static Linux x86-64 libraries;
- public `dense_sim.h` and `densedb.h` C ABI headers;
- CPython 3.13 and 3.14 Linux x86-64 wheels;
- source for the CPython, C++20, and Rust bindings;
- ABI layout and public API snapshots;
- retained benchmark result data;
- install, uninstall, verification, and checksum scripts; and
- public documentation and licensing notices.

## Not Included

- `libdense_sim` implementation source;
- DenseDB implementation source;
- private core tests or benchmark source;
- object files, private headers, or internal build directories;
- Git history from the development repository; or
- logos and other images.

## Platform

```text
Operating system: Linux
Architecture:     x86-64
Release:          0.1.0-rc1
C ABI version:    1
Required glibc:   2.33 or newer
```

The native libraries use SONAMEs `libdense_sim.so.0` and `libdensedb.so.0`.
DenseDB dynamically depends on `libdense_sim.so.0`.

See `docs/PLATFORM-COMPATIBILITY.md` before deploying to an older Linux
distribution.

## Repository Layout

```text
include/dense/                 public C headers
lib/linux-x86_64/              shared and static native libraries
pkgconfig/                     install-time pkg-config templates
bindings/python/               CPython binding source and wheels
bindings/cpp/                  header-only C++20 wrapper
bindings/rust/                 safe Rust wrapper crate
release/api/                   normalized public API snapshots
release/abi/                   retained ABI layout snapshot
release/benchmarks/            retained benchmark summary and gate
densebench/                    retained local benchmark text output
docs/                          public documentation
install.sh                     native SDK installer
uninstall.sh                   manifest-based uninstaller
verify-release.sh              release integrity checks
```

## Install Native Libraries

Install under `/usr/local`:

```bash
sudo ./install.sh
```

Install under another prefix:

```bash
sudo ./install.sh --prefix /opt/dense
```

Create a package staging tree without changing the host:

```bash
rm -rf stage
./install.sh --prefix /usr --destdir "$PWD/stage"
```

The installer deploys the C headers, shared/static libraries, C++ header,
`pkg-config` files, documentation, and an exact installation manifest.

Uninstall using the same prefix:

```bash
sudo ./uninstall.sh --prefix /opt/dense
```

See `docs/INSTALLATION.md` for all options.

## Compile Against the C ABI

After installation:

```bash
cc application.c $(pkg-config --cflags --libs libdense_sim) -o application
cc database.c $(pkg-config --cflags --libs libdensedb) -o database
```

Public includes may be written as:

```c
#include "dense_sim.h"
#include "densedb.h"
```

The `pkg-config` files add the namespaced Dense header directory.

## Python

Install the wheel matching the interpreter:

```bash
python3.13 -m pip install bindings/python/dist/*cp313*.whl
python3.14 -m pip install bindings/python/dist/*cp314*.whl
```

The wheels statically contain `libdense_sim`; a separate system install is not
required for the Python package. The binding source can also be rebuilt against
the packaged static library:

```bash
make -C bindings/python PYTHON=python3.14 test
```

The official Dense Python binding is available from PyPI as [`dense-sim`](https://pypi.org/project/dense-sim/).

```bash
python3 -m pip install "dense-sim==0.1.0rc1"
```

The package currently supports CPython 3.11, 3.12, 3.13, and 3.14 on Linux x86-64.

See the [Python binding README](bindings/python/README.md) for installation instructions, API usage, examples, and development information.

## C++

The C++20 wrapper is header-only and links to `libdense_sim`:

```bash
make -C bindings/cpp test
```

After system installation:

```cpp
#include <dense/dense_sim.hpp>
```

## Rust

The Rust crate links to the packaged static library by default:

```bash
make -C bindings/rust test
```

Override the native artifact directory with `DENSE_SIM_LIB_DIR`. Set
`DENSE_SIM_LINK_MODE=dynamic` to request dynamic linking instead of static
linking.

## Verify the Release

```bash
./verify-release.sh
./tools/generate-checksums.sh
sha256sum --check SHA256SUMS
```

The verifier rejects core implementation source outside `bindings/`, broken
SONAME links, missing public artifacts, inconsistent exported symbols, image
references, and unexpected build products.

## What `libdense_sim` Provides

`libdense_sim` owns spatial execution state and provides arbitrary 64-bit entity
IDs, dense entity storage, dynamic cells and chunks, incremental movement,
fixed and entity-following observers, persistent subscriptions, dirty-channel
coalescing, finalized `ENTER`/`UPDATE`/`LEAVE` operations, grouped recipient
plans, sampled movement, and optional kinetic motion plans.

It does not own health, inventory, equipment, authentication, gameplay rules,
network transport, serialization, or application payloads.

## What DenseDB Provides

DenseDB adds channel-oriented application state and durability while delegating
spatial execution to `libdense_sim`. It provides immutable schemas,
structure-of-arrays columns, spatial tables, fixed and entity-following WATCH
subscriptions, borrowed snapshot/delta views, tick-level WAL commits, atomic
snapshots, and snapshot-plus-WAL recovery.

DenseDB uses a single-writer tick model. A durable tick is acknowledged only
after `ddb_database_end_tick()` succeeds.

## Benchmark Snapshot

<p align="center">
  <img src="densebench/dense-bench-scenario.png" alt="Dense logo" width="800">
</p>


> **10,000 moving players, all visible to one another, processed in an average of 4.864 milliseconds per tick.**

All benchmark results were all measured locally on an AMD Ryzen 5 5600X.

Test configuration:

```text
players:             10,000
area:                24 × 24 spatial units
observer radius:     40
observers:           one entity-following observer per player
ticks per run:       100
public C API mean:   4.864 ms
public C API p95:    5.016 ms
public C API p99:    5.850 ms
```

At 20 updates per second, the tick interval is 50 milliseconds. A 4.864 ms mean consumes approximately 9.7% of one CPU core:

```text
4.864 ms per tick × 20 ticks per second
    =
97.28 ms of CPU time per second
```

Increasing the population from 1,000 to 10,000 increased implied entity-to-recipient relationships by approximately 100×, while public-API processing time increased by approximately 16.9×.

See `docs/BENCHMARK-SCOPE.md` and `release/benchmarks/`.

## License

Dense is source-available under the **Dense Community Source License 1.0**.

The license permits:

- noncommercial use;
- personal, educational, academic, and research use;
- source inspection and modification;
- qualifying small-commercial use while the licensee group remains below the revenue threshold defined by the license.

Commercial use at or above the threshold requires a separate written commercial agreement.

Dense is **not** distributed under an OSI-approved open-source license.

See:

- [`LICENSE.md`](LICENSE.md)
- [`COMMERCIAL-LICENSE.md`](COMMERCIAL-LICENSE.md)
