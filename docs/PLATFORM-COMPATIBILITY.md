# Platform Compatibility

## Native release target

```text
Release:       0.1.0-rc1
OS:            Linux
Architecture:  x86-64
ELF class:     ELF64
Endianness:    little-endian
C ABI:         System V AMD64
DS ABI:        1
DDB ABI:       1
```

The shared libraries declare:

```text
libdense_sim.so.0
libdensedb.so.0
```

DenseDB requires `libdense_sim.so.0`, `libm.so.6`, and `libc.so.6`.
`libdense_sim` requires `libm.so.6` and `libc.so.6`.

## glibc floor

The packaged native libraries reference glibc symbol versions through
`GLIBC_2.33`. Treat glibc 2.33 as the minimum supported runtime for this exact
binary release.

A newer build produced on an older baseline distribution may lower that floor,
but that requires rebuilding and revalidating the release artifacts.

## Other systems

ARM64, 32-bit x86, musl, macOS, Windows, BSD, and other POSIX platforms are not
claimed by this release. The public C ABI may be portable, but separate binaries
and ABI validation are required.

## Python wheels

The provided wheels target:

```text
CPython 3.13 / Linux x86-64
CPython 3.14 / Linux x86-64
```

They are not `abi3` wheels and do not install on a different CPython minor ABI.
