#ifndef DENSE_SIM_HPP
#define DENSE_SIM_HPP

#include "dense_sim.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dense {

using EntityId = ds_entity_id;
using ObserverId = ds_observer_id;
using ChannelMask = ds_channel_mask;
using TypeMask = ds_type_mask;
using Tick = ds_tick;

inline constexpr ChannelMask channel_position = DS_CHANNEL_POSITION;
inline constexpr ChannelMask channel_vitals = DS_CHANNEL_VITALS;
inline constexpr ChannelMask channel_animation = DS_CHANNEL_ANIMATION;
inline constexpr ChannelMask channel_appearance = DS_CHANNEL_APPEARANCE;
inline constexpr ChannelMask channel_custom_0 = DS_CHANNEL_CUSTOM_0;

class Error final : public std::runtime_error {
public:
    Error(ds_result result, const std::string &context)
        : std::runtime_error(
              context + ": " + std::string(ds_result_string(result))
          ),
          result_(result)
    {
    }

    [[nodiscard]] ds_result result() const noexcept
    {
        return result_;
    }

private:
    ds_result result_;
};

inline void check(ds_result result, const char *context)
{
    if (result != DS_OK) {
        throw Error(result, context);
    }
}

struct WorldConfig {
    std::int32_t cell_size = 8;
    std::int32_t chunk_size = 16;
    std::size_t initial_entity_capacity = 1024;
    std::size_t initial_observer_capacity = 64;
};

struct ObserverConfig {
    std::int32_t radius = 40;
    TypeMask type_mask = 0;
};

struct Entity {
    EntityId id = 0;
    std::int32_t spatial_x = 0;
    std::int32_t spatial_y = 0;
    TypeMask type_mask = 0;
};

struct Observer {
    ObserverId id = 0;
    std::int32_t spatial_x = 0;
    std::int32_t spatial_y = 0;
    std::int32_t radius = 0;
    TypeMask type_mask = 0;
    EntityId anchor_entity_id = 0;
    bool positioned = false;
    bool anchored = false;
};

enum class MotionMode : int {
    sampled = DS_MOTION_SAMPLED,
    kinetic = DS_MOTION_KINETIC,
};

struct MotionPlan {
    Tick start_tick = 0;
    Tick until_tick = 0;
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
};

struct MotionMetrics {
    std::uint64_t scheduled_events = 0;
    std::uint64_t processed_events = 0;
    std::uint64_t stale_events = 0;
    std::uint64_t cell_crossings = 0;
    std::uint64_t expiries = 0;
    std::uint64_t plan_replacements = 0;
    std::uint64_t sampled_demotions = 0;
    std::uint64_t correction_steps = 0;
};

struct WorldMemoryStats {
    std::size_t entity_capacity = 0;
    std::size_t entity_map_capacity = 0;
    std::size_t cell_bucket_capacity = 0;
    std::size_t chunk_bucket_capacity = 0;
    std::size_t observer_capacity = 0;
    std::size_t observer_map_capacity = 0;
    std::size_t subscription_capacity = 0;
    std::size_t membership_capacity = 0;
    std::size_t crossing_capacity = 0;
    std::size_t dirty_capacity = 0;
    std::size_t observer_dirty_capacity = 0;
    std::size_t fanout_chunk_capacity = 0;
    std::size_t fanout_entry_capacity = 0;
    std::size_t fanout_subscriber_capacity = 0;
    std::size_t kinetic_event_capacity = 0;
    std::size_t kinetic_bucket_capacity = 0;
};

struct AllocationMetrics {
    std::size_t current_retained_bytes = 0;
    std::size_t peak_retained_bytes = 0;
    std::uint64_t growth_operations = 0;
    std::size_t live_object_count = 0;
    std::uint64_t allocation_failures = 0;
    std::uint64_t steady_state_allocations = 0;
};

inline void runtime_init()
{
    check(ds_runtime_init(), "ds_runtime_init");
}

[[nodiscard]] inline bool runtime_has_avx2() noexcept
{
    return ds_runtime_has_avx2();
}

[[nodiscard]] inline AllocationMetrics allocation_metrics() noexcept
{
    ds_allocation_metrics metrics{};

    ds_get_allocation_metrics(&metrics);
    return AllocationMetrics{
        metrics.current_retained_bytes,
        metrics.peak_retained_bytes,
        metrics.growth_operations,
        metrics.live_object_count,
        metrics.allocation_failures,
        metrics.steady_state_allocations,
    };
}

inline void reset_allocation_counters() noexcept
{
    ds_reset_allocation_counters();
}

enum class DeltaOp : int {
    update = DS_DELTA_UPDATE,
    enter = DS_DELTA_ENTER,
    leave = DS_DELTA_LEAVE,
    spawn = DS_DELTA_SPAWN,
    remove = DS_DELTA_REMOVE,
};

struct DeltaEntry {
    EntityId entity_id = 0;
    ChannelMask channel_mask = 0;
    DeltaOp operation = DeltaOp::update;
};

struct RecipientWorksetConfig {
    std::size_t initial_recipient_capacity = 64;
    std::size_t initial_visible_capacity_per_recipient = 64;
};

struct RecipientWorksetEntry {
    EntityId entity_id = 0;
    ChannelMask channel_mask = 0;
};

struct RecipientWorksetView {
    Tick tick = 0;
    ObserverId observer_id = 0;
    std::uint64_t visible_set_generation = 0;
    std::uint64_t visible_set_fingerprint = 0;
    std::vector<RecipientWorksetEntry> entries;
    std::size_t dirty_count = 0;
};

struct RecipientWorksetStats {
    std::size_t recipient_count = 0;
    std::size_t recipient_capacity = 0;
    std::size_t visible_count = 0;
    std::size_t visible_capacity = 0;
    std::size_t dirty_count = 0;
    std::size_t retained_bytes = 0;
    std::uint64_t syncs = 0;
    std::uint64_t incremental_syncs = 0;
    std::uint64_t full_rebuilds = 0;
    std::uint64_t stable_recipient_reuses = 0;
    std::uint64_t membership_changes = 0;
    std::uint64_t source_enqueues = 0;
    std::uint64_t inverse_edge_visits = 0;
    std::uint64_t dirty_enqueues = 0;
    std::uint64_t dirty_deduplications = 0;
    std::uint64_t dirty_acknowledgements = 0;
    std::uint64_t stale_view_rejections = 0;
    std::uint64_t growth_operations = 0;
};

namespace detail {

struct WorldState final {
    ds_world *world = nullptr;
    std::uint64_t view_generation = 0;

    ~WorldState()
    {
        if (world != nullptr) {
            ds_world_destroy(world);
        }
    }
};

inline void ensure_open(const std::shared_ptr<WorldState> &state)
{
    if (!state || state->world == nullptr) {
        throw std::logic_error("dense::World is closed");
    }
}

inline void ensure_view(
    const std::shared_ptr<WorldState> &state,
    std::uint64_t generation
)
{
    ensure_open(state);

    if (state->view_generation != generation) {
        throw std::logic_error("borrowed fanout view is no longer valid");
    }
}

inline ds_world_config to_c(const WorldConfig &config) noexcept
{
    return ds_world_config{
        config.cell_size,
        config.chunk_size,
        config.initial_entity_capacity,
        config.initial_observer_capacity,
    };
}

inline ds_observer_config to_c(const ObserverConfig &config) noexcept
{
    return ds_observer_config{
        config.radius,
        config.type_mask,
    };
}

inline ds_motion_plan to_c(const MotionPlan &plan) noexcept
{
    return ds_motion_plan{
        plan.start_tick,
        plan.until_tick,
        plan.x,
        plan.y,
        plan.vx,
        plan.vy,
    };
}

inline Entity from_c(const ds_entity_desc &entity) noexcept
{
    return Entity{
        entity.id,
        entity.spatial_x,
        entity.spatial_y,
        entity.type_mask,
    };
}

inline Observer from_c(const ds_observer_desc &observer) noexcept
{
    return Observer{
        observer.id,
        observer.spatial_x,
        observer.spatial_y,
        observer.radius,
        observer.type_mask,
        observer.anchor_entity_id,
        observer.positioned,
        observer.anchored,
    };
}

inline MotionMetrics from_c(const ds_motion_metrics &metrics) noexcept
{
    return MotionMetrics{
        metrics.scheduled_events,
        metrics.processed_events,
        metrics.stale_events,
        metrics.cell_crossings,
        metrics.expiries,
        metrics.plan_replacements,
        metrics.sampled_demotions,
        metrics.correction_steps,
    };
}

inline DeltaEntry from_c(const ds_delta_entry &entry) noexcept
{
    return DeltaEntry{
        entry.entity_id,
        entry.channel_mask,
        static_cast<DeltaOp>(entry.operation),
    };
}

} // namespace detail

class DeltaEntryRange final {
public:
    class Iterator final {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = DeltaEntry;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = DeltaEntry;

        Iterator() = default;

        [[nodiscard]] DeltaEntry operator*() const
        {
            detail::ensure_view(state_, generation_);
            return detail::from_c(entries_[index_]);
        }

        Iterator &operator++()
        {
            detail::ensure_view(state_, generation_);
            ++index_;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator copy = *this;
            ++(*this);
            return copy;
        }

        friend bool operator==(const Iterator &left, const Iterator &right)
        {
            return left.entries_ == right.entries_ && left.index_ == right.index_;
        }

        friend bool operator!=(const Iterator &left, const Iterator &right)
        {
            return !(left == right);
        }

    private:
        friend class DeltaEntryRange;

        Iterator(
            std::shared_ptr<detail::WorldState> state,
            std::uint64_t generation,
            const ds_delta_entry *entries,
            std::size_t index
        )
            : state_(std::move(state)),
              generation_(generation),
              entries_(entries),
              index_(index)
        {
        }

        std::shared_ptr<detail::WorldState> state_;
        std::uint64_t generation_ = 0;
        const ds_delta_entry *entries_ = nullptr;
        std::size_t index_ = 0;
    };

    [[nodiscard]] std::size_t size() const
    {
        detail::ensure_view(state_, generation_);
        return count_;
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

    [[nodiscard]] DeltaEntry at(std::size_t index) const
    {
        detail::ensure_view(state_, generation_);

        if (index >= count_) {
            throw std::out_of_range("dense::DeltaEntryRange index out of range");
        }

        return detail::from_c(entries_[index]);
    }

    [[nodiscard]] Iterator begin() const
    {
        detail::ensure_view(state_, generation_);
        return Iterator(state_, generation_, entries_, 0);
    }

    [[nodiscard]] Iterator end() const
    {
        detail::ensure_view(state_, generation_);
        return Iterator(state_, generation_, entries_, count_);
    }

private:
    friend class ChunkDeltaView;

    DeltaEntryRange(
        std::shared_ptr<detail::WorldState> state,
        std::uint64_t generation,
        const ds_delta_entry *entries,
        std::size_t count
    )
        : state_(std::move(state)),
          generation_(generation),
          entries_(entries),
          count_(count)
    {
    }

    std::shared_ptr<detail::WorldState> state_;
    std::uint64_t generation_ = 0;
    const ds_delta_entry *entries_ = nullptr;
    std::size_t count_ = 0;
};

class SubscriberRange final {
public:
    class Iterator final {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = ObserverId;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = ObserverId;

        Iterator() = default;

        [[nodiscard]] ObserverId operator*() const
        {
            detail::ensure_view(state_, generation_);
            return subscribers_[index_];
        }

        Iterator &operator++()
        {
            detail::ensure_view(state_, generation_);
            ++index_;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator copy = *this;
            ++(*this);
            return copy;
        }

        friend bool operator==(const Iterator &left, const Iterator &right)
        {
            return left.subscribers_ == right.subscribers_ && left.index_ == right.index_;
        }

        friend bool operator!=(const Iterator &left, const Iterator &right)
        {
            return !(left == right);
        }

    private:
        friend class SubscriberRange;

        Iterator(
            std::shared_ptr<detail::WorldState> state,
            std::uint64_t generation,
            const ds_observer_id *subscribers,
            std::size_t index
        )
            : state_(std::move(state)),
              generation_(generation),
              subscribers_(subscribers),
              index_(index)
        {
        }

        std::shared_ptr<detail::WorldState> state_;
        std::uint64_t generation_ = 0;
        const ds_observer_id *subscribers_ = nullptr;
        std::size_t index_ = 0;
    };

    [[nodiscard]] std::size_t size() const
    {
        detail::ensure_view(state_, generation_);
        return count_;
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

    [[nodiscard]] ObserverId at(std::size_t index) const
    {
        detail::ensure_view(state_, generation_);

        if (index >= count_) {
            throw std::out_of_range("dense::SubscriberRange index out of range");
        }

        return subscribers_[index];
    }

    [[nodiscard]] Iterator begin() const
    {
        detail::ensure_view(state_, generation_);
        return Iterator(state_, generation_, subscribers_, 0);
    }

    [[nodiscard]] Iterator end() const
    {
        detail::ensure_view(state_, generation_);
        return Iterator(state_, generation_, subscribers_, count_);
    }

private:
    friend class ChunkDeltaView;

    SubscriberRange(
        std::shared_ptr<detail::WorldState> state,
        std::uint64_t generation,
        const ds_observer_id *subscribers,
        std::size_t count
    )
        : state_(std::move(state)),
          generation_(generation),
          subscribers_(subscribers),
          count_(count)
    {
    }

    std::shared_ptr<detail::WorldState> state_;
    std::uint64_t generation_ = 0;
    const ds_observer_id *subscribers_ = nullptr;
    std::size_t count_ = 0;
};

class ChunkDeltaView final {
public:
    [[nodiscard]] std::int32_t chunk_x() const
    {
        validate();
        return group_->chunk_x;
    }

    [[nodiscard]] std::int32_t chunk_y() const
    {
        validate();
        return group_->chunk_y;
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        validate();
        return group_->entry_count;
    }

    [[nodiscard]] std::size_t subscriber_count() const
    {
        validate();
        return group_->subscriber_count;
    }

    [[nodiscard]] DeltaEntryRange entries() const
    {
        validate();
        return DeltaEntryRange(
            state_,
            generation_,
            group_->entries,
            group_->entry_count
        );
    }

    [[nodiscard]] SubscriberRange subscribers() const
    {
        validate();
        return SubscriberRange(
            state_,
            generation_,
            group_->subscribers,
            group_->subscriber_count
        );
    }

private:
    friend class FanoutView;

    ChunkDeltaView(
        std::shared_ptr<detail::WorldState> state,
        std::uint64_t generation,
        const ds_chunk_delta *group
    )
        : state_(std::move(state)),
          generation_(generation),
          group_(group)
    {
    }

    void validate() const
    {
        detail::ensure_view(state_, generation_);
    }

    std::shared_ptr<detail::WorldState> state_;
    std::uint64_t generation_ = 0;
    const ds_chunk_delta *group_ = nullptr;
};

class FanoutView final {
public:
    class Iterator final {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = ChunkDeltaView;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = ChunkDeltaView;

        Iterator() = default;

        [[nodiscard]] ChunkDeltaView operator*() const
        {
            detail::ensure_view(state_, generation_);
            return ChunkDeltaView(
                state_,
                generation_,
                &view_.chunks[index_]
            );
        }

        Iterator &operator++()
        {
            detail::ensure_view(state_, generation_);
            ++index_;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator copy = *this;
            ++(*this);
            return copy;
        }

        friend bool operator==(const Iterator &left, const Iterator &right)
        {
            return left.view_.chunks == right.view_.chunks && left.index_ == right.index_;
        }

        friend bool operator!=(const Iterator &left, const Iterator &right)
        {
            return !(left == right);
        }

    private:
        friend class FanoutView;

        Iterator(
            std::shared_ptr<detail::WorldState> state,
            std::uint64_t generation,
            ds_fanout_view view,
            std::size_t index
        )
            : state_(std::move(state)),
              generation_(generation),
              view_(view),
              index_(index)
        {
        }

        std::shared_ptr<detail::WorldState> state_;
        std::uint64_t generation_ = 0;
        ds_fanout_view view_{};
        std::size_t index_ = 0;
    };

    [[nodiscard]] Tick tick() const
    {
        validate();
        return view_.tick;
    }

    [[nodiscard]] std::size_t size() const
    {
        validate();
        return view_.chunk_count;
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

    [[nodiscard]] std::size_t delta_count() const
    {
        validate();
        return view_.delta_count;
    }

    [[nodiscard]] std::size_t subscriber_count() const
    {
        validate();
        return view_.subscriber_count;
    }

    [[nodiscard]] ChunkDeltaView at(std::size_t index) const
    {
        validate();

        if (index >= view_.chunk_count) {
            throw std::out_of_range("dense::FanoutView index out of range");
        }

        return ChunkDeltaView(
            state_,
            generation_,
            &view_.chunks[index]
        );
    }

    [[nodiscard]] Iterator begin() const
    {
        validate();
        return Iterator(state_, generation_, view_, 0);
    }

    [[nodiscard]] Iterator end() const
    {
        validate();
        return Iterator(state_, generation_, view_, view_.chunk_count);
    }

private:
    friend class World;

    FanoutView(
        std::shared_ptr<detail::WorldState> state,
        std::uint64_t generation,
        ds_fanout_view view
    )
        : state_(std::move(state)),
          generation_(generation),
          view_(view)
    {
    }

    void validate() const
    {
        detail::ensure_view(state_, generation_);
    }

    std::shared_ptr<detail::WorldState> state_;
    std::uint64_t generation_ = 0;
    ds_fanout_view view_{};
};

class RecipientWorkset;

class World final {
public:
    explicit World(const WorldConfig &config = WorldConfig{})
        : state_(std::make_shared<detail::WorldState>())
    {
        const ds_world_config c_config = detail::to_c(config);
        check(ds_world_create(&c_config, &state_->world), "ds_world_create");
        world_ = state_->world;
    }

    World(const World &) = delete;
    World &operator=(const World &) = delete;

    World(World &&other) noexcept
        : state_(std::move(other.state_)),
          world_(std::exchange(other.world_, nullptr))
    {
    }

    World &operator=(World &&other) noexcept
    {
        if (this != &other) {
            state_ = std::move(other.state_);
            world_ = std::exchange(other.world_, nullptr);
        }
        return *this;
    }

    ~World() = default;

    void close() noexcept
    {
        if (world_ != nullptr) {
            ds_world_destroy(world_);
            world_ = nullptr;
            state_->world = nullptr;
            ++state_->view_generation;
        }
    }

    [[nodiscard]] bool is_open() const noexcept
    {
        return world_ != nullptr;
    }

    void begin_tick(Tick tick)
    {
        require_open();
        check(ds_world_begin_tick(world_, tick), "ds_world_begin_tick");
        ++state_->view_generation;
    }

    void end_tick()
    {
        require_open();
        check(ds_world_end_tick(world_), "ds_world_end_tick");
    }

    [[nodiscard]] bool tick_is_open() const
    {
        require_open();
        return ds_world_tick_is_open(world_);
    }

    [[nodiscard]] Tick current_tick() const
    {
        require_open();
        return ds_world_current_tick(world_);
    }

    [[nodiscard]] std::size_t entity_count() const
    {
        require_open();
        return ds_world_entity_count(world_);
    }

    [[nodiscard]] std::size_t entity_capacity() const
    {
        require_open();
        return ds_world_entity_capacity(world_);
    }

    [[nodiscard]] std::size_t observer_count() const
    {
        require_open();
        return ds_world_observer_count(world_);
    }

    [[nodiscard]] MotionMetrics motion_metrics() const
    {
        require_open();
        ds_motion_metrics metrics{};
        check(
            ds_world_get_motion_metrics(world_, &metrics),
            "ds_world_get_motion_metrics"
        );
        return detail::from_c(metrics);
    }

    [[nodiscard]] WorldMemoryStats memory_stats() const
    {
        require_open();
        ds_world_memory_stats stats{};

        check(
            ds_world_get_memory_stats(world_, &stats),
            "ds_world_get_memory_stats"
        );
        return WorldMemoryStats{
            stats.entity_capacity,
            stats.entity_map_capacity,
            stats.cell_bucket_capacity,
            stats.chunk_bucket_capacity,
            stats.observer_capacity,
            stats.observer_map_capacity,
            stats.subscription_capacity,
            stats.membership_capacity,
            stats.crossing_capacity,
            stats.dirty_capacity,
            stats.observer_dirty_capacity,
            stats.fanout_chunk_capacity,
            stats.fanout_entry_capacity,
            stats.fanout_subscriber_capacity,
            stats.kinetic_event_capacity,
            stats.kinetic_bucket_capacity,
        };
    }

    [[nodiscard]] FanoutView fanout() const
    {
        require_open();
        ds_fanout_view view{};
        check(
            ds_world_get_fanout_view(world_, &view),
            "ds_world_get_fanout_view"
        );
        return FanoutView(state_, state_->view_generation, view);
    }

    void spawn(
        EntityId entity_id,
        std::int32_t spatial_x,
        std::int32_t spatial_y,
        TypeMask type_mask
    )
    {
        require_open();
        check(
            ds_entity_spawn(
                world_,
                entity_id,
                spatial_x,
                spatial_y,
                type_mask
            ),
            "ds_entity_spawn"
        );
    }

    void move(
        EntityId entity_id,
        std::int32_t spatial_x,
        std::int32_t spatial_y
    )
    {
        require_open();
        check(
            ds_entity_move(
                world_,
                entity_id,
                spatial_x,
                spatial_y
            ),
            "ds_entity_move"
        );
    }

    void remove(EntityId entity_id)
    {
        require_open();
        check(
            ds_entity_remove(world_, entity_id),
            "ds_entity_remove"
        );
    }

    void mark_dirty(EntityId entity_id, ChannelMask channel_mask)
    {
        require_open();
        check(
            ds_entity_mark_dirty(world_, entity_id, channel_mask),
            "ds_entity_mark_dirty"
        );
    }

    void set_motion_plan(EntityId entity_id, const MotionPlan &plan)
    {
        require_open();
        const ds_motion_plan c_plan = detail::to_c(plan);
        check(
            ds_entity_set_motion_plan(world_, entity_id, &c_plan),
            "ds_entity_set_motion_plan"
        );
    }

    void clear_motion_plan(EntityId entity_id)
    {
        require_open();
        check(
            ds_entity_clear_motion_plan(world_, entity_id),
            "ds_entity_clear_motion_plan"
        );
    }

    [[nodiscard]] MotionMode motion_mode(EntityId entity_id) const
    {
        require_open();
        ds_motion_mode mode = DS_MOTION_SAMPLED;
        check(
            ds_entity_motion_mode(world_, entity_id, &mode),
            "ds_entity_motion_mode"
        );
        return static_cast<MotionMode>(mode);
    }

    [[nodiscard]] bool entity_exists(EntityId entity_id) const
    {
        require_open();
        return ds_entity_exists(world_, entity_id);
    }

    [[nodiscard]] Entity entity(EntityId entity_id) const
    {
        require_open();
        ds_entity_desc entity{};
        check(
            ds_entity_get(world_, entity_id, &entity),
            "ds_entity_get"
        );
        return detail::from_c(entity);
    }

    [[nodiscard]] ObserverId create_observer(const ObserverConfig &config)
    {
        require_open();
        const ds_observer_config c_config = detail::to_c(config);
        ds_observer_id observer_id = 0;
        check(
            ds_observer_create(world_, &c_config, &observer_id),
            "ds_observer_create"
        );
        return observer_id;
    }

    void destroy_observer(ObserverId observer_id)
    {
        require_open();
        check(
            ds_observer_destroy(world_, observer_id),
            "ds_observer_destroy"
        );
    }

    void anchor_observer(ObserverId observer_id, EntityId entity_id)
    {
        require_open();
        check(
            ds_observer_anchor_entity(world_, observer_id, entity_id),
            "ds_observer_anchor_entity"
        );
    }

    void set_observer_position(
        ObserverId observer_id,
        std::int32_t spatial_x,
        std::int32_t spatial_y
    )
    {
        require_open();
        check(
            ds_observer_set_position(
                world_,
                observer_id,
                spatial_x,
                spatial_y
            ),
            "ds_observer_set_position"
        );
    }

    void set_observer_radius(ObserverId observer_id, std::int32_t radius)
    {
        require_open();
        check(
            ds_observer_set_radius(world_, observer_id, radius),
            "ds_observer_set_radius"
        );
    }

    [[nodiscard]] bool observer_exists(ObserverId observer_id) const
    {
        require_open();
        return ds_observer_exists(world_, observer_id);
    }

    [[nodiscard]] Observer observer(ObserverId observer_id) const
    {
        require_open();
        ds_observer_desc observer{};
        check(
            ds_observer_get(world_, observer_id, &observer),
            "ds_observer_get"
        );
        return detail::from_c(observer);
    }

private:
    friend class RecipientWorkset;

    void require_open() const
    {
        if (world_ == nullptr) {
            throw std::logic_error("dense::World is closed");
        }
    }

    std::shared_ptr<detail::WorldState> state_;
    ds_world *world_ = nullptr;
};

class RecipientWorkset final {
public:
    explicit RecipientWorkset(
        const RecipientWorksetConfig &config = RecipientWorksetConfig{}
    )
    {
        ds_recipient_workset_config c_config{};

        ds_recipient_workset_config_defaults(&c_config);
        c_config.initial_recipient_capacity = config.initial_recipient_capacity;
        c_config.initial_visible_capacity_per_recipient =
            config.initial_visible_capacity_per_recipient;
        check(
            ds_recipient_workset_create(&c_config, &workset_),
            "ds_recipient_workset_create"
        );
    }

    RecipientWorkset(const RecipientWorkset &) = delete;
    RecipientWorkset &operator=(const RecipientWorkset &) = delete;

    RecipientWorkset(RecipientWorkset &&other) noexcept
        : workset_(std::exchange(other.workset_, nullptr))
    {
    }

    RecipientWorkset &operator=(RecipientWorkset &&other) noexcept
    {
        if (this != &other) {
            ds_recipient_workset_destroy(workset_);
            workset_ = std::exchange(other.workset_, nullptr);
        }
        return *this;
    }

    ~RecipientWorkset()
    {
        ds_recipient_workset_destroy(workset_);
    }

    void sync(const World &world)
    {
        world.require_open();
        check(
            ds_recipient_workset_sync(workset_, world.world_),
            "ds_recipient_workset_sync"
        );
    }

    void enqueue_source(
        const World &world,
        EntityId entity_id,
        ChannelMask channel_mask
    )
    {
        world.require_open();
        check(
            ds_recipient_workset_enqueue_source(
                workset_,
                world.world_,
                entity_id,
                channel_mask
            ),
            "ds_recipient_workset_enqueue_source"
        );
    }

    [[nodiscard]] RecipientWorksetView view(ObserverId observer_id) const
    {
        ds_recipient_workset_view c_view{};
        RecipientWorksetView result;

        check(
            ds_recipient_workset_get_view(workset_, observer_id, &c_view),
            "ds_recipient_workset_get_view"
        );
        result.tick = c_view.tick;
        result.observer_id = c_view.observer_id;
        result.visible_set_generation = c_view.visible_set_generation;
        result.visible_set_fingerprint = c_view.visible_set_fingerprint;
        result.dirty_count = c_view.dirty_count;
        result.entries.reserve(c_view.visible_count);
        for (std::size_t index = 0; index < c_view.visible_count; ++index) {
            result.entries.push_back(RecipientWorksetEntry{
                c_view.entries[index].entity_id,
                c_view.entries[index].channel_mask,
            });
        }
        return result;
    }

    void acknowledge(
        const RecipientWorksetView &view,
        EntityId entity_id,
        ChannelMask channel_mask
    )
    {
        check(
            ds_recipient_workset_acknowledge(
                workset_,
                view.observer_id,
                view.visible_set_generation,
                view.visible_set_fingerprint,
                entity_id,
                channel_mask
            ),
            "ds_recipient_workset_acknowledge"
        );
    }

    void clear(const RecipientWorksetView &view)
    {
        check(
            ds_recipient_workset_clear(
                workset_,
                view.observer_id,
                view.visible_set_generation,
                view.visible_set_fingerprint
            ),
            "ds_recipient_workset_clear"
        );
    }

    [[nodiscard]] RecipientWorksetStats stats() const
    {
        ds_recipient_workset_stats c_stats{};

        check(
            ds_recipient_workset_get_stats(workset_, &c_stats),
            "ds_recipient_workset_get_stats"
        );
        return RecipientWorksetStats{
            c_stats.recipient_count,
            c_stats.recipient_capacity,
            c_stats.visible_count,
            c_stats.visible_capacity,
            c_stats.dirty_count,
            c_stats.retained_bytes,
            c_stats.syncs,
            c_stats.incremental_syncs,
            c_stats.full_rebuilds,
            c_stats.stable_recipient_reuses,
            c_stats.membership_changes,
            c_stats.source_enqueues,
            c_stats.inverse_edge_visits,
            c_stats.dirty_enqueues,
            c_stats.dirty_deduplications,
            c_stats.dirty_acknowledgements,
            c_stats.stale_view_rejections,
            c_stats.growth_operations,
        };
    }

private:
    ds_recipient_workset *workset_ = nullptr;
};

} // namespace dense

#endif
