from __future__ import annotations

from array import array
from statistics import median
from time import perf_counter_ns

import dense_sim


ENTITY_COUNT = 10_000
REPEATS = 9
PLAYER = 1


def median_and_worst(samples: list[float]) -> tuple[float, float]:
    ordered = sorted(samples)
    return median(ordered), ordered[-1]


def main() -> None:
    ids = array("Q", range(ENTITY_COUNT))
    xs_a = array("i", (index * 16 for index in range(ENTITY_COUNT)))
    xs_b = array("i", (index * 16 + 1 for index in range(ENTITY_COUNT)))
    ys = array("i", [0]) * ENTITY_COUNT

    world = dense_sim.World(
        initial_entity_capacity=ENTITY_COUNT,
        initial_observer_capacity=0,
    )
    world.begin_tick(1)
    for entity_id, x in zip(ids, xs_a):
        world.spawn(entity_id, x, 0, PLAYER)
    world.end_tick()

    tick = 2
    scalar_target = xs_b
    batch_target = xs_b
    scalar_move_samples: list[float] = []
    batch_move_samples: list[float] = []
    scalar_dirty_samples: list[float] = []
    batch_dirty_samples: list[float] = []

    for _ in range(REPEATS + 1):
        world.begin_tick(tick)
        tick += 1
        start = perf_counter_ns()
        for index in range(ENTITY_COUNT):
            world.move(ids[index], scalar_target[index], 0)
        elapsed = (perf_counter_ns() - start) / 1_000_000.0
        world.end_tick()
        scalar_target = xs_a if scalar_target is xs_b else xs_b
        if len(scalar_move_samples) < REPEATS:
            scalar_move_samples.append(elapsed)

        world.begin_tick(tick)
        tick += 1
        start = perf_counter_ns()
        world.move_many(ids, batch_target, ys)
        elapsed = (perf_counter_ns() - start) / 1_000_000.0
        world.end_tick()
        batch_target = xs_a if batch_target is xs_b else xs_b
        if len(batch_move_samples) < REPEATS:
            batch_move_samples.append(elapsed)

        world.begin_tick(tick)
        tick += 1
        start = perf_counter_ns()
        for entity_id in ids:
            world.mark_dirty(entity_id, dense_sim.CHANNEL_POSITION)
        elapsed = (perf_counter_ns() - start) / 1_000_000.0
        world.end_tick()
        if len(scalar_dirty_samples) < REPEATS:
            scalar_dirty_samples.append(elapsed)

        world.begin_tick(tick)
        tick += 1
        start = perf_counter_ns()
        world.mark_dirty_many(ids, dense_sim.CHANNEL_POSITION)
        elapsed = (perf_counter_ns() - start) / 1_000_000.0
        world.end_tick()
        if len(batch_dirty_samples) < REPEATS:
            batch_dirty_samples.append(elapsed)

    scalar_move_ms, scalar_move_worst = median_and_worst(scalar_move_samples)
    batch_move_ms, batch_move_worst = median_and_worst(batch_move_samples)
    scalar_dirty_ms, scalar_dirty_worst = median_and_worst(scalar_dirty_samples)
    batch_dirty_ms, batch_dirty_worst = median_and_worst(batch_dirty_samples)

    print("dense_sim Python binding call-overhead benchmark")
    print(f"  Python: {__import__('sys').version.split()[0]}")
    print(f"  entities: {ENTITY_COUNT}")
    print("  timed region excludes begin_tick/end_tick finalization")
    print("")
    print("  movement calls")
    print(f"    scalar median: {scalar_move_ms:9.3f} ms   worst: {scalar_move_worst:9.3f} ms")
    print(f"    batch  median: {batch_move_ms:9.3f} ms   worst: {batch_move_worst:9.3f} ms")
    print(f"    batch speedup: {scalar_move_ms / batch_move_ms:9.2f}x")
    print("")
    print("  dirty-mark calls")
    print(f"    scalar median: {scalar_dirty_ms:9.3f} ms   worst: {scalar_dirty_worst:9.3f} ms")
    print(f"    batch  median: {batch_dirty_ms:9.3f} ms   worst: {batch_dirty_worst:9.3f} ms")
    print(f"    batch speedup: {scalar_dirty_ms / batch_dirty_ms:9.2f}x")


if __name__ == "__main__":
    main()
