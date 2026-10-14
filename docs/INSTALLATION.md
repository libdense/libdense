# Installation

## Files installed

```text
<PREFIX>/include/dense/dense_sim.h
<PREFIX>/include/dense/dense_net.h
<PREFIX>/include/dense/dense_net_sim_bridge.h
<PREFIX>/include/dense/dense_sched.h
<PREFIX>/include/dense/dense_collision.h
<PREFIX>/include/dense/dense_nav.h
<PREFIX>/include/dense/dense_nav_collision_bridge.h
<PREFIX>/include/dense/dense_ai.h
<PREFIX>/include/dense/densedb.h
<PREFIX>/include/dense/dense_sim.hpp
<PREFIX>/<LIBDIR>/libdense_sim.so*        libdense_sim.a
<PREFIX>/<LIBDIR>/libdense_net.so*        libdense_net.a
<PREFIX>/<LIBDIR>/libdense_sched.so*      libdense_sched.a
<PREFIX>/<LIBDIR>/libdense_collision.so*  libdense_collision.a
<PREFIX>/<LIBDIR>/libdense_nav.so*        libdense_nav.a
<PREFIX>/<LIBDIR>/libdense_ai.so*         libdense_ai.a
<PREFIX>/<LIBDIR>/libdensedb.so*          libdensedb.a
<PREFIX>/<LIBDIR>/pkgconfig/libdense_*.pc and libdensedb.pc
<PREFIX>/share/doc/dense/*
<PREFIX>/share/dense/install-manifest.txt
```

Defaults:

```text
PREFIX=/usr/local
DESTDIR=
LIBDIR=lib
```

## Direct install

The included `install.sh` installs the Linux x86-64 release bundle:

```bash
sudo ./install.sh
sudo ./install.sh --prefix /opt/dense
sudo ./install.sh --prefix /usr --libdir lib64
```

## Platform requirements

This bundle targets Linux x86-64. Its shared libraries require glibc 2.33 or
newer. Static archives are included for applications that link Dense statically.
See [platform compatibility](PLATFORM-COMPATIBILITY.md) for wheel requirements
and other targets, which require separate SDK builds.

## Staged install

```bash
rm -rf stage
./install.sh --prefix /usr --destdir "$PWD/stage"
```

`DESTDIR` is prepended only on disk. The generated `pkg-config` files retain the
logical prefix, which is required for packaging.

## Installer options

```text
--prefix PATH
--destdir PATH
--libdir NAME
--no-docs
--no-cpp
--no-ldconfig
--dry-run
--help
```

Environment variables `PREFIX`, `DESTDIR`, and `LIBDIR` are also accepted.
Flags take precedence.

## Runtime linker

The installer runs `ldconfig` only for a direct root install when available.
For a custom prefix, use an rpath or configure the runtime search path:

```bash
export LD_LIBRARY_PATH="/opt/dense/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

## pkg-config

```bash
export PKG_CONFIG_PATH="/opt/dense/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --cflags --libs libdense_sim
pkg-config --cflags --libs libdense_net
pkg-config --cflags --libs libdense_sched
pkg-config --cflags --libs libdense_collision
pkg-config --cflags --libs libdense_nav
pkg-config --cflags --libs libdense_ai
pkg-config --cflags --libs libdensedb
```

## Uninstall

Use the same prefix, destination root, and library directory used at install:

```bash
sudo ./uninstall.sh --prefix /opt/dense
./uninstall.sh --prefix /usr --destdir "$PWD/stage"
```

The uninstaller removes only paths recorded in the installation manifest.

## Python

Python wheels are managed by `pip`, not by `install.sh`:

```bash
python3.14 -m pip install bindings/python/dist/*cp314*.whl
python3.14 -m pip uninstall dense-sim
```

The four included wheels cover standard GIL CPython 3.11 through 3.14 on Linux
x86-64. Select the wheel matching the interpreter's minor version.
