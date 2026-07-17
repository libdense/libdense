# libdense_sim C++ binding

A header-only C++20 RAII wrapper over the canonical `dense_sim.h` C ABI.

```bash
make -C bindings/cpp test
make -C bindings/cpp strict
make -C bindings/cpp example
```

The Makefile links against the packaged `lib/linux-x86_64/libdense_sim.a`.
Override `INCLUDE_DIR`, `LIB_DIR`, or `CORE_LIB` when using another installed
artifact.

```cpp
#include <dense/dense_sim.hpp>

int main()
{
    dense::World world;
    world.begin_tick(1);
    world.spawn(193, 100, 100, UINT64_C(1));
    world.mark_dirty(193, dense::channel_position);
    world.end_tick();
}
```

Borrowed wrapper views retain a shared world control block and validate their
generation before accessing native spans. Stale access throws
`std::logic_error`.
