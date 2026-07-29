# dense-sim Python binding

The Python binding is a thin CPython extension over the canonical `libdense_sim`
C ABI.

The scalar surface is ergonomic:

```python
from dense_sim import CHANNEL_POSITION, World

world = World(cell_size=8, chunk_size=16)

world.begin_tick(1)
world.spawn(193, 100, 100, 1)
world.move(193, 101, 100)
world.mark_dirty(193, CHANNEL_POSITION)
world.end_tick()

for group in world.fanout_view():
    for delta in group.entries:
        print(delta.entity_id, delta.operation_name)
```

Stable linear motion can use the selective kinetic backend:

```python
from dense_sim import MotionMode

world.begin_tick(2)
world.set_motion_plan(
    193,
    2,
    500,
    101.0,
    100.0,
    0.25,
    0.10,
)
world.end_tick()

assert world.motion_mode(193) == MotionMode.KINETIC
print(world.motion_metrics)
```

`clear_motion_plan()` materializes the current sampled integer position and
demotes the entity. A successful scalar or batched sampled move also demotes an
active plan. Kinetic scheduling and certificate logic live entirely in the C
kernel; the Python binding only wraps the public C ABI.

Entities with anchored observers cannot currently enter kinetic mode because
observer coverage boundaries require their own subscription certificates.

The high-volume path uses contiguous native buffers:

```python
from array import array

entity_ids = array("Q", [193, 228, 482])
xs = array("i", [101, 88, 52])
ys = array("i", [100, 90, 61])

world.move_many(entity_ids, xs, ys)
world.mark_dirty_many(entity_ids, CHANNEL_POSITION)
```

`move_many()` requires native unsigned 64-bit IDs and signed 32-bit coordinate
buffers. `mark_dirty_many()` accepts a native unsigned 64-bit ID buffer plus
either one integer channel mask or a second native unsigned 64-bit mask buffer.
`array`, contiguous `memoryview` objects, and compatible NumPy arrays can use the
buffer fast path without Python-element conversion.

Batch calls are sequential, not transactional. If one entity operation fails,
earlier operations remain applied and the raised `DenseSimError` reports the
failing index.

`FanoutView`, `ChunkDeltaView`, `DeltaEntryView`, and `SubscriberView` are
read-only borrowed views. They retain the Python `World` object but are
invalidated by the next successful `World.begin_tick()` or by `World.close()`.
No nested fanout arrays are copied into Python lists when the view is created.

## Build and test

```bash
make test
make benchmark
make wheel
```

Select a specific CPython interpreter with `PYTHON`:

```bash
make PYTHON=python3.14 test
make PYTHON=python3.14 wheel
```

## Benchmarks

<p align="center">
  <img src="densebench/dense-bench-python-binding.png" alt="Dense logo" width="620">
</p>

<p align="center">
  <img src="densebench/dense-bench-budget.png" alt="Dense logo" width="620">
</p>

<p align="center">
  <img src="densebench/python-benchmark.png" alt="Dense logo" width="365" height="478">
</p>



