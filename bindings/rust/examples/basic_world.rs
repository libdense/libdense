use dense_sim::{DeltaOp, ObserverConfig, World, WorldConfig};

const PLAYER_TYPE: u64 = 1_u64 << 0;

fn main() -> dense_sim::Result<()> {
    let mut world = World::new(WorldConfig::default())?;

    world.begin_tick(1)?;
    world.spawn(193, 100, 100, PLAYER_TYPE)?;
    let observer_id = world.create_observer(ObserverConfig {
        radius: 40,
        type_mask: PLAYER_TYPE,
    })?;
    world.set_observer_position(observer_id, 100, 100)?;
    world.end_tick()?;

    let fanout = world.fanout_view()?;
    for group in fanout.groups() {
        println!("chunk ({}, {})", group.chunk_x(), group.chunk_y());
        for entry in group.entries() {
            match entry.operation {
                DeltaOp::Enter => println!("  enter {}", entry.entity_id),
                DeltaOp::Update => println!("  update {}", entry.entity_id),
                DeltaOp::Leave => println!("  leave {}", entry.entity_id),
                DeltaOp::Spawn | DeltaOp::Remove => {}
            }
        }
    }

    Ok(())
}
