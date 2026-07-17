#include "dense_sim.hpp"

#include <cstdint>
#include <iostream>

int main()
{
    constexpr dense::TypeMask player_type = UINT64_C(1) << 0u;

    dense::World world;

    world.begin_tick(1);
    world.spawn(193, 100, 100, player_type);

    const dense::ObserverId observer_id = world.create_observer(
        dense::ObserverConfig{40, player_type}
    );
    world.set_observer_position(observer_id, 100, 100);
    world.end_tick();

    for (const dense::ChunkDeltaView group : world.fanout()) {
        std::cout
            << "chunk ("
            << group.chunk_x()
            << ", "
            << group.chunk_y()
            << ")\n";

        for (const dense::DeltaEntry entry : group.entries()) {
            std::cout
                << "  entity="
                << entry.entity_id
                << " operation="
                << static_cast<int>(entry.operation)
                << "\n";
        }
    }

    return 0;
}
