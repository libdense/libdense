# Platform Compatibility

Release: `0.3.6`

## Supported native SDKs

| Target | Role | Native libraries | DenseDB |
|---|---|---|---|
| Linux x86-64 | Client and server | Static; release bundles may also include shared ELF libraries | Included |
| Linux ARM64 | Headless server | Static | Included |
| Windows x86-64 | Client | Static MinGW-w64 archives | Not included |

All targets are 64-bit and little-endian. The Linux public layout is frozen in
separate x86-64 and AArch64 records under `release/abi/`.

The shared-library SONAME major remains `0`. The public ABI identifiers remain:

```text
DS_ABI_VERSION=1
DDB_ABI_VERSION=2
```

The aggregate SDK and `libdense_sim` are version 0.3.6. `libdense_net` remains
version 0.3.5; the other native modules and DenseDB remain version 0.3.0.

## Platform roles

Linux x86-64 provides both client and server aggregate targets. Linux ARM64 is
the supported headless-server target. Windows x86-64 is client-only, so its SDK
does not expose DenseDB or a server aggregate.

Every portable SDK includes a target-locked CMake config package. It rejects
the wrong operating system, pointer size, or processor during configuration.

## Linux runtime compatibility

The portable static SDKs are linked into the consumer application, so the
application's final toolchain and runtime determine its libc requirements. For
a shared-library bundle, inspect the exact published artifact with `readelf`
rather than carrying forward the glibc floor of an older release.

## Python wheels

CI builds standard GIL CPython 3.11, 3.12, 3.13, and 3.14 wheels for:

```text
Linux x86-64
Linux ARM64 / AArch64
Windows x86-64 / AMD64
```

The wheels are not `abi3` wheels and do not install on a different CPython
minor ABI. Free-threaded CPython, 32-bit Python, macOS, and Windows ARM are not
supported by this release.

## Other systems

Linux and Windows targets outside the matrix above, macOS, BSD, musl-only
systems, and 32-bit systems require separate builds and validation and are not
claimed by this release.
