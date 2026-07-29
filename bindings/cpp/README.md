# libdense_sim C++ binding

A small C++20 RAII wrapper over the canonical `dense_sim.h` C ABI.

The wrapper does not implement spatial, subscription, fanout, or kinetic logic.
It owns a `ds_world` through `dense::World` and translates `ds_result` failures
into `dense::Error`.

## Build and test

```bash
make -C bindings/cpp test
make -C bindings/cpp strict
make -C bindings/cpp example
```

## Example

```cpp
#include "dense_sim.hpp"

#include <cstdint>

int main()
{
    constexpr dense::TypeMask player_type = UINT64_C(1) << 0u;

    dense::World world;

    world.begin_tick(1);
    world.spawn(193, 100, 100, player_type);
    world.mark_dirty(193, dense::channel_position);
    world.end_tick();

    for (const dense::ChunkDeltaView group : world.fanout()) {
        for (const dense::DeltaEntry entry : group.entries()) {
            consume(entry);
        }
    }
}
```

## Borrowed fanout lifetime

`FanoutView`, `ChunkDeltaView`, `DeltaEntryRange`, and `SubscriberRange` retain a
small shared control block. They keep the native world alive after the owning
`World` C++ object is moved or destroyed.

A successful `World::begin_tick()` invalidates all prior borrowed fanout views.
Every wrapper access validates the generation before reading a C span and throws
`std::logic_error` when the view is stale. `World::close()` also invalidates
existing views.

The wrapper intentionally returns copied `DeltaEntry` values and observer IDs
from checked iterators instead of exposing a raw `std::span` that could outlive
the C ABI invalidation point.
