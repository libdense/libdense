from __future__ import annotations

from array import array
import gc
import unittest

import dense_sim


PLAYER = 1 << 0
MONSTER = 1 << 1


class WorldBindingTests(unittest.TestCase):
    def test_scalar_lifecycle_and_entity_access(self) -> None:
        world = dense_sim.World(
            cell_size=8,
            chunk_size=16,
            initial_entity_capacity=2,
            initial_observer_capacity=1,
        )

        world.begin_tick(1)
        world.spawn(193, -1, 17, PLAYER)
        world.move(193, 7, 18)
        world.mark_dirty(193, dense_sim.CHANNEL_POSITION)
        self.assertTrue(world.entity_exists(193))
        self.assertEqual(world.entity_count, 1)
        self.assertEqual(
            world.entity(193),
            {
                "id": 193,
                "spatial_x": 7,
                "spatial_y": 18,
                "type_mask": PLAYER,
            },
        )
        world.end_tick()
        view = world.fanout_view()
        self.assertEqual(view.tick, 1)
        self.assertEqual(view.chunk_count, 0)
        world.close()
        with self.assertRaises(dense_sim.DenseSimError):
            _ = world.entity_count

    def test_buffer_batch_move_and_dirty(self) -> None:
        world = dense_sim.World(initial_entity_capacity=4)
        ids = array("Q", [10, 11, 12, 13])
        xs = array("i", [100, 101, 102, 103])
        ys = array("i", [200, 201, 202, 203])
        masks = array(
            "Q",
            [
                dense_sim.CHANNEL_POSITION,
                dense_sim.CHANNEL_ANIMATION,
                dense_sim.CHANNEL_POSITION | dense_sim.CHANNEL_ANIMATION,
                dense_sim.CHANNEL_VITALS,
            ],
        )

        world.begin_tick(1)
        for entity_id in ids:
            world.spawn(entity_id, 0, 0, PLAYER)
        self.assertEqual(world.move_many(ids, xs, ys), 4)
        self.assertEqual(world.mark_dirty_many(ids, masks), 4)
        self.assertEqual(
            world.mark_dirty_many(ids, dense_sim.CHANNEL_APPEARANCE),
            4,
        )
        world.end_tick()

        for index, entity_id in enumerate(ids):
            entity = world.entity(entity_id)
            self.assertEqual(entity["spatial_x"], xs[index])
            self.assertEqual(entity["spatial_y"], ys[index])

    def test_batch_buffers_reject_wrong_types_and_lengths(self) -> None:
        world = dense_sim.World(initial_entity_capacity=2)
        world.begin_tick(1)
        world.spawn(1, 0, 0, PLAYER)

        with self.assertRaises(TypeError):
            world.move_many([1], array("i", [1]), array("i", [1]))
        with self.assertRaises(TypeError):
            world.move_many(array("q", [1]), array("i", [1]), array("i", [1]))
        with self.assertRaises(ValueError):
            world.move_many(array("Q", [1]), array("i", [1, 2]), array("i", [1]))
        with self.assertRaises(ValueError):
            world.mark_dirty_many(array("Q", [1]), array("Q", [1, 2]))

    def test_batch_failure_is_sequential_and_reports_index(self) -> None:
        world = dense_sim.World(initial_entity_capacity=2)
        ids = array("Q", [1, 999, 2])
        xs = array("i", [10, 20, 30])
        ys = array("i", [11, 21, 31])

        world.begin_tick(1)
        world.spawn(1, 0, 0, PLAYER)
        world.spawn(2, 0, 0, PLAYER)
        with self.assertRaisesRegex(dense_sim.DenseSimError, "index 1"):
            world.move_many(ids, xs, ys)
        self.assertEqual(world.entity(1)["spatial_x"], 10)
        self.assertEqual(world.entity(2)["spatial_x"], 0)

    def test_borrowed_fanout_views_are_read_only_and_invalidate(self) -> None:
        world = dense_sim.World(
            initial_entity_capacity=2,
            initial_observer_capacity=1,
        )

        world.begin_tick(1)
        world.spawn(193, 0, 0, PLAYER)
        observer_id = world.create_observer(40, PLAYER | MONSTER)
        world.set_observer_position(observer_id, 0, 0)
        world.end_tick()

        view = world.fanout_view()
        self.assertEqual(view.tick, 1)
        self.assertEqual(len(view), 1)
        group = view[0]
        self.assertEqual(group.chunk_x, 0)
        self.assertEqual(group.chunk_y, 0)
        self.assertEqual(list(group.subscribers), [observer_id])
        self.assertEqual(len(group.entries), 1)
        entry = group.entries[0]
        self.assertEqual(entry.entity_id, 193)
        self.assertEqual(entry.channel_mask, 0)
        self.assertEqual(entry.operation, dense_sim.DeltaOp.ENTER)
        self.assertEqual(entry.operation_name, "ENTER")
        with self.assertRaises(AttributeError):
            group.chunk_x = 99
        with self.assertRaises(TypeError):
            group.subscribers[0] = 99

        world.begin_tick(2)
        with self.assertRaises(RuntimeError):
            _ = view.tick
        with self.assertRaises(RuntimeError):
            _ = group.entry_count
        with self.assertRaises(RuntimeError):
            _ = len(group.subscribers)

    def test_fanout_view_keeps_world_alive_until_view_is_invalidated_or_released(self) -> None:
        world = dense_sim.World(initial_entity_capacity=1)
        world.begin_tick(1)
        world.spawn(1, 0, 0, PLAYER)
        world.end_tick()
        view = world.fanout_view()

        del world
        gc.collect()
        self.assertEqual(view.tick, 1)
        self.assertEqual(len(view), 0)

    def test_update_fanout_uses_borrowed_sequences(self) -> None:
        world = dense_sim.World(
            initial_entity_capacity=2,
            initial_observer_capacity=2,
        )

        world.begin_tick(1)
        world.spawn(1, 0, 0, PLAYER)
        for _ in range(2):
            observer_id = world.create_observer(40, PLAYER)
            world.set_observer_position(observer_id, 0, 0)
        world.end_tick()

        world.begin_tick(2)
        world.move(1, 1, 0)
        world.mark_dirty(
            1,
            dense_sim.CHANNEL_POSITION | dense_sim.CHANNEL_ANIMATION,
        )
        world.end_tick()
        view = world.fanout_view()

        self.assertEqual(view.delta_count, 1)
        self.assertEqual(view.subscriber_count, 2)
        group = view.groups[0]
        self.assertEqual(len(group.entries), 1)
        self.assertEqual(len(group.subscribers), 2)
        entry = group.entries[-1]
        self.assertEqual(entry.operation, dense_sim.DeltaOp.UPDATE)
        self.assertEqual(
            entry.channel_mask,
            dense_sim.CHANNEL_POSITION | dense_sim.CHANNEL_ANIMATION,
        )

    def test_observer_scalar_surface(self) -> None:
        world = dense_sim.World(initial_entity_capacity=1, initial_observer_capacity=1)
        world.begin_tick(1)
        world.spawn(7, 10, 20, PLAYER)
        observer_id = world.create_observer(40, PLAYER)
        self.assertTrue(world.observer_exists(observer_id))
        world.anchor_observer(observer_id, 7)
        observer = world.observer(observer_id)
        self.assertTrue(observer["anchored"])
        self.assertTrue(observer["positioned"])
        self.assertEqual(observer["anchor_entity_id"], 7)
        self.assertEqual(observer["spatial_x"], 10)
        world.set_observer_radius(observer_id, 56)
        self.assertEqual(world.observer(observer_id)["radius"], 56)
        world.destroy_observer(observer_id)
        self.assertFalse(world.observer_exists(observer_id))

    def test_failed_begin_tick_does_not_invalidate_view(self) -> None:
        world = dense_sim.World(initial_entity_capacity=1)
        world.begin_tick(1)
        world.spawn(1, 0, 0, PLAYER)
        world.end_tick()
        view = world.fanout_view()

        with self.assertRaises(dense_sim.DenseSimError):
            world.begin_tick(1)
        self.assertEqual(view.tick, 1)

    def test_close_invalidates_borrowed_view(self) -> None:
        world = dense_sim.World(initial_entity_capacity=1)
        world.begin_tick(1)
        world.end_tick()
        view = world.fanout_view()
        world.close()

        with self.assertRaises(RuntimeError):
            _ = view.tick

    def test_memoryview_buffers_use_batch_path(self) -> None:
        world = dense_sim.World(initial_entity_capacity=2)
        ids = array("Q", [1, 2])
        xs = array("i", [8, 16])
        ys = array("i", [-8, -16])

        world.begin_tick(1)
        world.spawn(1, 0, 0, PLAYER)
        world.spawn(2, 0, 0, PLAYER)
        self.assertEqual(
            world.move_many(memoryview(ids), memoryview(xs), memoryview(ys)),
            2,
        )
        self.assertEqual(
            world.mark_dirty_many(
                memoryview(ids),
                dense_sim.CHANNEL_POSITION,
            ),
            2,
        )
        world.end_tick()
        self.assertEqual(world.entity(2)["spatial_y"], -16)


    def test_kinetic_motion_plan_surface_and_metrics(self) -> None:
        world = dense_sim.World(initial_entity_capacity=1)
        world.begin_tick(1)
        world.spawn(300, 1, 1, MONSTER)
        world.set_motion_plan(
            300,
            1,
            40,
            1.0,
            1.0,
            1.0,
            0.0,
        )
        self.assertEqual(
            world.motion_mode(300),
            dense_sim.MotionMode.KINETIC,
        )
        world.end_tick()

        for tick in range(2, 9):
            world.begin_tick(tick)
            world.end_tick()

        self.assertEqual(world.entity(300)["spatial_x"], 8)
        metrics = world.motion_metrics
        self.assertEqual(metrics["processed_events"], 1)
        self.assertEqual(metrics["cell_crossings"], 1)
        self.assertEqual(metrics["stale_events"], 0)

    def test_sampled_move_demotes_kinetic_plan(self) -> None:
        world = dense_sim.World(initial_entity_capacity=1)
        world.begin_tick(1)
        world.spawn(301, 1, 1, MONSTER)
        world.set_motion_plan(301, 1, 40, 1.0, 1.0, 1.0, 0.0)
        world.end_tick()

        world.begin_tick(2)
        world.move(301, 3, 1)
        self.assertEqual(
            world.motion_mode(301),
            dense_sim.MotionMode.SAMPLED,
        )
        world.end_tick()
        self.assertEqual(world.motion_metrics["sampled_demotions"], 1)

    def test_kinetic_motion_rejects_anchored_observer_conflict(self) -> None:
        world = dense_sim.World(
            initial_entity_capacity=1,
            initial_observer_capacity=1,
        )
        world.begin_tick(1)
        world.spawn(302, 1, 1, MONSTER)
        observer_id = world.create_observer(40, MONSTER)
        world.anchor_observer(observer_id, 302)
        with self.assertRaises(dense_sim.DenseSimError):
            world.set_motion_plan(302, 1, 40, 1.0, 1.0, 1.0, 0.0)

        world.set_observer_position(observer_id, 0, 0)
        world.set_motion_plan(302, 1, 40, 1.0, 1.0, 1.0, 0.0)
        with self.assertRaises(dense_sim.DenseSimError):
            world.anchor_observer(observer_id, 302)


if __name__ == "__main__":
    unittest.main()
