#include "dense_sim.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr std::size_t entity_count = 100000;
constexpr std::size_t measured_samples = 21;
constexpr std::size_t passes_per_sample = 20;
constexpr dense::TypeMask player_type = UINT64_C(1) << 0u;

using Clock = std::chrono::steady_clock;

struct CWorld final {
    ds_world *world = nullptr;

    CWorld()
    {
        const ds_world_config config{
            8,
            16,
            entity_count,
            1,
        };

        if (ds_world_create(&config, &world) != DS_OK) {
            std::abort();
        }
    }

    ~CWorld()
    {
        ds_world_destroy(world);
    }

    CWorld(const CWorld &) = delete;
    CWorld &operator=(const CWorld &) = delete;
};

void populate(ds_world *world)
{
    if (ds_world_begin_tick(world, 1) != DS_OK) {
        std::abort();
    }

    for (std::size_t index = 0; index < entity_count; ++index) {
        if (
            ds_entity_spawn(
                world,
                static_cast<ds_entity_id>(index),
                0,
                0,
                player_type
            )
            != DS_OK
        ) {
            std::abort();
        }
    }

    if (ds_world_end_tick(world) != DS_OK) {
        std::abort();
    }
}

void populate(dense::World &world)
{
    world.begin_tick(1);

    for (std::size_t index = 0; index < entity_count; ++index) {
        world.spawn(
            static_cast<dense::EntityId>(index),
            0,
            0,
            player_type
        );
    }

    world.end_tick();
}

double run_c_sample(ds_world *world, ds_tick tick_base)
{
    const auto started = Clock::now();

    for (std::size_t pass = 0; pass < passes_per_sample; ++pass) {
        if (ds_world_begin_tick(world, tick_base + pass) != DS_OK) {
            std::abort();
        }

        const std::int32_t x = (pass & 1u) == 0u ? 1 : 2;

        for (std::size_t index = 0; index < entity_count; ++index) {
            if (
                ds_entity_move(
                    world,
                    static_cast<ds_entity_id>(index),
                    x,
                    0
                )
                != DS_OK
            ) {
                std::abort();
            }
        }

        if (ds_world_end_tick(world) != DS_OK) {
            std::abort();
        }
    }

    const auto finished = Clock::now();
    return std::chrono::duration<double, std::milli>(finished - started).count();
}

double run_cpp_sample(dense::World &world, dense::Tick tick_base)
{
    const auto started = Clock::now();

    for (std::size_t pass = 0; pass < passes_per_sample; ++pass) {
        world.begin_tick(tick_base + pass);
        const std::int32_t x = (pass & 1u) == 0u ? 1 : 2;

        for (std::size_t index = 0; index < entity_count; ++index) {
            world.move(
                static_cast<dense::EntityId>(index),
                x,
                0
            );
        }

        world.end_tick();
    }

    const auto finished = Clock::now();
    return std::chrono::duration<double, std::milli>(finished - started).count();
}

double median(std::array<double, measured_samples> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2u];
}

} // namespace

int main()
{
    CWorld c_world;
    dense::World cpp_world(
        dense::WorldConfig{
            8,
            16,
            entity_count,
            1,
        }
    );

    populate(c_world.world);
    populate(cpp_world);

    std::array<double, measured_samples> c_samples{};
    std::array<double, measured_samples> cpp_samples{};
    std::array<double, measured_samples> ratio_samples{};

    for (std::size_t sample = 0; sample < measured_samples; ++sample) {
        const std::uint64_t tick_base = 2u + sample * passes_per_sample;

        if ((sample & 1u) == 0u) {
            c_samples[sample] = run_c_sample(c_world.world, tick_base);
            cpp_samples[sample] = run_cpp_sample(cpp_world, tick_base);
        } else {
            cpp_samples[sample] = run_cpp_sample(cpp_world, tick_base);
            c_samples[sample] = run_c_sample(c_world.world, tick_base);
        }

        ratio_samples[sample] = cpp_samples[sample] / c_samples[sample];
    }

    const double c_ms = median(c_samples);
    const double cpp_ms = median(cpp_samples);
    const double paired_ratio = median(ratio_samples);
    const double calls = static_cast<double>(entity_count * passes_per_sample);

    std::printf("C++ wrapper overhead benchmark\n");
    std::printf("  entities:                %zu\n", entity_count);
    std::printf("  moves/sample:            %.0f\n", calls);
    std::printf("  direct C median:         %.3f ms\n", c_ms);
    std::printf("  C++ wrapper median:      %.3f ms\n", cpp_ms);
    std::printf("  C ns/move:               %.2f\n", c_ms * 1000000.0 / calls);
    std::printf("  C++ ns/move:             %.2f\n", cpp_ms * 1000000.0 / calls);
    std::printf("  median paired ratio:     %.3fx\n", paired_ratio);

    return 0;
}
