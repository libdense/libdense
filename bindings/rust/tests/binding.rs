use dense_sim::{
    allocation_metrics, reset_allocation_counters, runtime_has_avx2,
    runtime_init, DeltaOp, MotionMode, MotionPlan, ObserverConfig,
    RecipientWorkset, RecipientWorksetConfig, ResultCode, World, WorldConfig,
    CHANNEL_POSITION,
};

const PLAYER_TYPE: u64 = 1_u64 << 0;
const MONSTER_TYPE: u64 = 1_u64 << 1;

#[test]
fn lifecycle_and_error_mapping() {
    let mut world = World::new(WorldConfig::default()).unwrap();

    world.begin_tick(1).unwrap();
    world.spawn(193, 0, 0, PLAYER_TYPE).unwrap();
    world.move_entity(193, 4, 5).unwrap();
    world.mark_dirty(193, CHANNEL_POSITION).unwrap();
    world.end_tick().unwrap();

    let entity = world.entity(193).unwrap();
    assert_eq!(entity.spatial_x, 4);
    assert_eq!(entity.spatial_y, 5);

    let error = world.begin_tick(1).unwrap_err();
    assert_eq!(error.code(), Some(ResultCode::TickOrder));
}

#[test]
fn borrowed_fanout_has_exact_lifetime() {
    let mut world = World::new(WorldConfig::default()).unwrap();

    world.begin_tick(1).unwrap();
    world.spawn(1, 0, 0, PLAYER_TYPE).unwrap();
    world.spawn(2, 2, 0, MONSTER_TYPE).unwrap();
    let observer_id = world
        .create_observer(ObserverConfig {
            radius: 40,
            type_mask: PLAYER_TYPE | MONSTER_TYPE,
        })
        .unwrap();
    world
        .set_observer_position(observer_id, 0, 0)
        .unwrap();
    world.end_tick().unwrap();

    let fanout = world.fanout_view().unwrap();
    assert_eq!(fanout.tick(), 1);
    assert_eq!(fanout.delta_count(), 2);

    let entries: Vec<_> = fanout
        .groups()
        .flat_map(|group| group.entries())
        .collect();
    assert_eq!(entries.len(), 2);
    assert!(entries.iter().all(|entry| entry.operation == DeltaOp::Enter));

    let subscribers: Vec<_> = fanout
        .groups()
        .flat_map(|group| group.subscribers().iter().copied())
        .collect();
    assert!(subscribers.iter().all(|id| *id == observer_id));

    drop(fanout);
    world.begin_tick(2).unwrap();
    world.end_tick().unwrap();
}

#[test]
fn kinetic_motion_and_sampled_demotion() {
    let mut world = World::new(WorldConfig::default()).unwrap();

    world.begin_tick(1).unwrap();
    world.spawn(50, 7, 0, PLAYER_TYPE).unwrap();
    world
        .set_motion_plan(
            50,
            MotionPlan {
                start_tick: 1,
                until_tick: 100,
                x: 7.0,
                y: 0.0,
                vx: 1.0,
                vy: 0.0,
            },
        )
        .unwrap();
    world.end_tick().unwrap();

    assert_eq!(world.motion_mode(50).unwrap(), MotionMode::Kinetic);

    world.begin_tick(2).unwrap();
    world.end_tick().unwrap();
    assert_eq!(world.entity(50).unwrap().spatial_x, 8);
    assert!(world.motion_metrics().unwrap().cell_crossings >= 1);

    world.begin_tick(3).unwrap();
    world.move_entity(50, 20, 0).unwrap();
    world.end_tick().unwrap();

    assert_eq!(world.motion_mode(50).unwrap(), MotionMode::Sampled);
    assert!(world.motion_metrics().unwrap().sampled_demotions >= 1);
}

#[test]
fn world_is_send_between_threads() {
    let world = World::new(WorldConfig::default()).unwrap();
    let entity_count = std::thread::spawn(move || world.entity_count())
        .join()
        .unwrap();
    assert_eq!(entity_count, 0);
}

#[test]
fn recipient_workset_and_metrics() {
    runtime_init().unwrap();
    let _ = runtime_has_avx2();
    let mut world = World::new(WorldConfig::default()).unwrap();
    let mut workset = RecipientWorkset::new(RecipientWorksetConfig::default())
        .unwrap();

    world.begin_tick(1).unwrap();
    world.spawn(90, 0, 0, MONSTER_TYPE).unwrap();
    let observer_id = world
        .create_observer(ObserverConfig {
            radius: 0,
            type_mask: MONSTER_TYPE,
        })
        .unwrap();
    world
        .set_observer_position(observer_id, 0, 0)
        .unwrap();
    world.end_tick().unwrap();

    workset.sync(&world).unwrap();
    workset
        .enqueue_source(&world, 90, CHANNEL_POSITION)
        .unwrap();
    let view = workset.view(observer_id).unwrap();
    assert_eq!(view.entries.len(), 1);
    assert_eq!(view.entries[0].entity_id, 90);
    assert_eq!(view.entries[0].channel_mask, CHANNEL_POSITION);
    assert_eq!(view.dirty_count, 1);

    workset
        .acknowledge(&view, 90, CHANNEL_POSITION)
        .unwrap();
    assert_eq!(workset.view(observer_id).unwrap().dirty_count, 0);
    assert_eq!(workset.stats().unwrap().inverse_edge_visits, 1);
    assert!(world.memory_stats().unwrap().observer_capacity >= 1);

    reset_allocation_counters();
    assert_eq!(allocation_metrics().allocation_failures, 0);
}
