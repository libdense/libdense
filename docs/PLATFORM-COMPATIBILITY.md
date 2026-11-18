# Platform Compatibility

Release: `0.4.8` (release candidate)

## Included artifacts

| Target | Role | Native libraries | Python wheels |
|---|---|---|---|
| Linux x86-64 | Client and server | Nine shared libraries and nine static archives: the seven family libraries including DenseDB, the optional delivery SDK, and experimental libdense_gpu | CPython 3.11, 3.12, 3.13, and 3.14 |

The artifacts are 64-bit and little-endian. The shared-library SONAME major
remains `0`. The public ABI identifiers remain:

```text
DS_ABI_VERSION=1
DDB_ABI_VERSION=2
```

The aggregate SDK, `libdense_sim` and `libdense_delivery` are version 0.4.8. `libdense_net` remains
version 0.3.5; the other native modules and DenseDB remain version 0.3.0.
Experimental `libdense_gpu` is version 0.1.7.
Linux layout records for x86-64 and AArch64 are retained under `release/abi/`.

## Linux runtime compatibility

The included shared-library bundle requires glibc 2.34 or newer, as determined
from its ELF symbol requirements. The optional delivery library sets this
floor. The other shared SDK libraries require at most glibc 2.33, and
libdense_gpu at most glibc 2.16. Static archives are linked into the consumer
application; the final toolchain and selected system libraries determine that
application's runtime requirements.

The Python extensions require glibc symbols through version 2.14 and have no
runtime `libdense_sim.so` dependency. The Python interpreter may require a
newer glibc. Wheels use the `linux_x86_64` platform tag, not a manylinux tag.

## libdense_gpu backends

The CPU backend is the default and needs nothing beyond libc. The OpenGL 4.3
compute backend needs a context of version 4.3 or later created by the
application and an OpenGL loader supplied through the configuration; the
library itself links no OpenGL library. It is qualified on one machine (RTX
3070, NVIDIA 535.261.03) and checked on Mesa llvmpipe. AMD and Intel drivers,
Windows and WebGL/OpenGL ES are not covered.

## Python compatibility

Wheels target standard GIL CPython 3.11, 3.12, 3.13, and 3.14. They are not
`abi3` wheels and do not install on a different CPython minor ABI.
Free-threaded CPython and 32-bit Python are not supported by these wheels.

## Other targets

Linux ARM64 headless-server SDKs and experimental Windows x86-64 client and
compatibility-server SDKs require separate builds. Their binaries and wheels
are not included here. The Windows DenseDB backend retains WAL history during
checkpoints; see [Windows hosting](windows-hosting.md).

macOS, BSD, musl-only systems, Windows ARM, and 32-bit systems require separate
builds and validation and are not claimed by this release.

The optional headless demo executables require glibc 2.34 or newer. The shared
SDK libraries require glibc 2.34 or newer; Python extension symbols require
glibc 2.14, subject to the interpreter's own requirements.
