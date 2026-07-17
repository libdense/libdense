#include "dense_sim.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

constexpr dense::TypeMask player_type = UINT64_C(1) << 0u;
constexpr dense::TypeMask monster_type = UINT64_C(1) << 1u;

void test_world_lifecycle_and_entity_access()
{
    dense::World world;

    assert(world.is_open());
    assert(world.entity_count() == 0u);

    world.begin_tick(1);
    world.spawn(193, -1, -9, player_type);
    world.move(193, 4, 5);
    world.mark_dirty(193, dense::channel_position);
    world.end_tick();

    const dense::Entity entity = world.entity(193);
    assert(entity.id == 193u);
    assert(entity.spatial_x == 4);
    assert(entity.spatial_y == 5);
    assert(entity.type_mask == player_type);
    assert(world.entity_exists(193));

    bool threw = false;
    try {
        world.begin_tick(1);
    } catch (const dense::Error &error) {
        threw = true;
        assert(error.result() == DS_ERR_TICK_ORDER);
    }
    assert(threw);

    world.close();
    assert(!world.is_open());

    threw = false;
    try {
        static_cast<void>(world.entity_count());
    } catch (const std::logic_error &) {
        threw = true;
    }
    assert(threw);
}

void test_observer_fanout_and_view_invalidation()
{
    dense::World world;

    world.begin_tick(1);
    world.spawn(1, 0, 0, player_type);
    world.spawn(2, 2, 0, monster_type);

    const dense::ObserverId observer_id = world.create_observer(
        dense::ObserverConfig{
            40,
            player_type | monster_type,
        }
    );
    world.set_observer_position(observer_id, 0, 0);
    world.end_tick();

    const dense::FanoutView initial = world.fanout();
    assert(initial.tick() == 1u);
    assert(initial.delta_count() == 2u);

    std::size_t enter_count = 0;
    for (const dense::ChunkDeltaView group : initial) {
        assert(!group.subscribers().empty());
        for (const dense::DeltaEntry entry : group.entries()) {
            if (entry.operation == dense::DeltaOp::enter) {
                ++enter_count;
            }
        }
    }
    assert(enter_count == 2u);

    world.begin_tick(2);

    bool threw = false;
    try {
        static_cast<void>(initial.tick());
    } catch (const std::logic_error &) {
        threw = true;
    }
    assert(threw);

    world.move(1, 1, 0);
    world.mark_dirty(1, dense::channel_position);
    world.end_tick();

    const dense::FanoutView update = world.fanout();
    assert(update.delta_count() == 1u);

    const dense::ChunkDeltaView group = update.at(0);
    assert(group.entries().at(0).entity_id == 1u);
    assert(group.entries().at(0).operation == dense::DeltaOp::update);
    assert(group.subscribers().at(0) == observer_id);
}

void test_view_keeps_world_alive()
{
    dense::FanoutView view = [] {
        dense::World world;
        world.begin_tick(1);
        world.spawn(7, 0, 0, player_type);
        const dense::ObserverId observer_id = world.create_observer(
            dense::ObserverConfig{40, player_type}
        );
        world.set_observer_position(observer_id, 0, 0);
        world.end_tick();
        return world.fanout();
    }();

    assert(view.tick() == 1u);
    assert(view.delta_count() == 1u);
    assert(view.at(0).entries().at(0).entity_id == 7u);
}

void test_world_move_preserves_views_control_block()
{
    dense::World first;
    first.begin_tick(1);
    first.spawn(8, 0, 0, player_type);
    const dense::ObserverId observer_id = first.create_observer(
        dense::ObserverConfig{40, player_type}
    );
    first.set_observer_position(observer_id, 0, 0);
    first.end_tick();

    const dense::FanoutView view = first.fanout();
    dense::World second = std::move(first);

    assert(view.delta_count() == 1u);
    assert(second.entity_exists(8));

    second.begin_tick(2);

    bool threw = false;
    try {
        static_cast<void>(view.delta_count());
    } catch (const std::logic_error &) {
        threw = true;
    }
    assert(threw);
}

void test_kinetic_motion()
{
    dense::World world;

    world.begin_tick(1);
    world.spawn(50, 7, 0, player_type);
    world.set_motion_plan(
        50,
        dense::MotionPlan{
            1,
            100,
            7.0,
            0.0,
            1.0,
            0.0,
        }
    );
    world.end_tick();

    assert(world.motion_mode(50) == dense::MotionMode::kinetic);

    world.begin_tick(2);
    world.end_tick();

    const dense::Entity entity = world.entity(50);
    assert(entity.spatial_x == 8);
    assert(entity.spatial_y == 0);

    const dense::MotionMetrics metrics = world.motion_metrics();
    assert(metrics.processed_events >= 1u);
    assert(metrics.cell_crossings >= 1u);

    world.begin_tick(3);
    world.move(50, 20, 0);
    world.end_tick();

    assert(world.motion_mode(50) == dense::MotionMode::sampled);
    assert(world.motion_metrics().sampled_demotions >= 1u);
}

} // namespace

int main()
{
    test_world_lifecycle_and_entity_access();
    test_observer_fanout_and_view_invalidation();
    test_view_keeps_world_alive();
    test_world_move_preserves_views_control_block();
    test_kinetic_motion();
    return 0;
}
