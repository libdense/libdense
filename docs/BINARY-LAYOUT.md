# Binary Layout

## Repository layout

The versioned ELF files are canonical. Unversioned and SONAME names are relative
symlinks:

```text
libdense_sim.so -> libdense_sim.so.0 -> libdense_sim.so.0.1.0
libdensedb.so -> libdensedb.so.0 -> libdensedb.so.0.1.0
```

Static archives are included for bindings and fully static application links.

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
