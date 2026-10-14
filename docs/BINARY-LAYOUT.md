# Binary Layout

## Repository layout

The versioned ELF files are canonical. Unversioned and SONAME names are relative
symlinks:

```text
libdense_sim.so -> libdense_sim.so.0 -> libdense_sim.so.0.4.2
libdense_net.so -> libdense_net.so.0 -> libdense_net.so.0.3.5
libdense_sched.so -> libdense_sched.so.0 -> libdense_sched.so.0.3.0
libdense_collision.so -> libdense_collision.so.0 -> libdense_collision.so.0.3.0
libdense_nav.so -> libdense_nav.so.0 -> libdense_nav.so.0.3.0
libdense_ai.so -> libdense_ai.so.0 -> libdense_ai.so.0.3.0
libdensedb.so -> libdensedb.so.0 -> libdensedb.so.0.3.0
```

Static archives are included for bindings and static linkage of Dense into
applications. This bundle targets Linux x86-64. It uses the installer and
pkg-config templates described below; it does not contain a portable CMake
SDK or binaries for other targets.

## Installed layout

The installer uses a namespaced header directory while keeping conventional
library names:

```text
/usr/local/include/dense/
/usr/local/lib/
/usr/local/lib/pkgconfig/
```

`pkg-config` adds `/usr/local/include/dense`, so the canonical ABI headers keep
their existing internal includes such as `#include "dense_sim.h"`.

## Python wheels

The CPython extension statically contains `libdense_sim` and has no runtime
`libdense_sim.so` dependency.
