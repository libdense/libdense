# libdense_gpu (experimental)

**Exact bulk spatial queries for local simulations, on the CPU by default and
on the GPU by request.**

Status: experimental, version 0.1.7 in libdense 0.4.8. The API and ABI may
change between releases. The module is opt-in: it is shipped as its own
header, libraries and pkg-config file, and it is outside the bindings, the
CMake package and the frozen API baselines. It is meant for local
simulations; networked servers keep using libdense_sim on the CPU.

```text
include/dense/dense_gpu.h
lib/linux-x86_64/libdense_gpu.so.0.1.7   (SONAME libdense_gpu.so.0)
lib/linux-x86_64/libdense_gpu.a
pkgconfig/libdense_gpu.pc.in
```

```bash
cc app.c $(pkg-config --cflags --libs libdense_gpu) -o app
```

The library depends only on libc. It links neither OpenGL nor SDL: on the
GPU it calls OpenGL through function pointers from the application's loader,
on the application's context.

## Kernels

| Kernel | Answer |
| --- | --- |
| `dg_context_bin` | Sorts the points into a grid of cells. |
| `dg_knn` | The k nearest matching points within a radius, for each query point. |
| `dg_aggregate_query` | Count, position sums and value sums of all matching points within a radius. |
| `dg_field` | The same sums for every grid cell. |
| `dg_context_cell_order` | The points' indices in cell order, for an application that keeps its entities sorted. |

Points are `int32` in 2D or 3D. The CPU backend is the reference and the
default. The GPU backend runs the same kernels as OpenGL 4.3 compute shaders
and returns the same bytes: every result is defined by integer arithmetic
and does not depend on the backend, the grid or the visiting order. A
simulation can switch backends, or run on a machine without a suitable GPU,
and replay identically.

## Quick start

```c
#include "dense_gpu.h"

dg_context_config config;
dg_context *context;

dg_context_config_defaults(&config);      /* CPU backend */
config.max_points = 1000000;
config.max_cells = 262144;
config.max_queries = 1000000;
config.max_k = 7;
dg_context_create(&config, &context, NULL, 0);

/* Each tick: the streams point at your own arrays. */
dg_points points = {0};
points.count = count;
points.x.data = x;                        /* int32 arrays */
points.y.data = y;
points.z.data = z;                        /* leave NULL for 2D */
points.value[0].data = vx;                /* optional, summed by aggregates */

dg_grid grid = {origin_x, origin_y, origin_z, cell_size, 64, 64, 64};
dg_context_bin(context, &points, &grid);

dg_knn_request knn = {0};
knn.set.query_count = count;              /* every point is a query */
knn.radius = cell_size;
knn.k = 7;
dg_knn_output out = {counts, neighbors, NULL};
dg_knn(context, &knn, &out);
```

A cell size near the query radius is the usual choice: a query then reads
27 cells in 3D, 9 in 2D. Points outside the grid fall into its edge cells,
so the grid bounds affect speed and never results.

### Matching

Point `j` matches query point `i` when `j != i`, `(group[j] & seek[i]) != 0`
and the squared distance is at most the squared radius. Without a `group`
stream every point has group 1; without a `seek` stream every point seeks
everything. Teams are bits: a red ship with `group = RED, seek = BLUE` finds
its nearest blue ships with one `dg_knn`. To query a place where no point
is, add a point with group 0.

The k nearest are the k smallest by (squared distance, index). The index
tie-break makes the answer unique, which is what lets two backends agree.

## The GPU backend

```c
static dg_gl_proc load(void *user, const char *name)
{
    (void)user;
    return (dg_gl_proc)SDL_GL_GetProcAddress(name);
}

config.backend = DG_BACKEND_GL_COMPUTE;
config.gl_loader = load;
```

- The OpenGL context (4.3 or later, core or compatibility) must be current
  on the calling thread for `dg_context_create` and every later call on that
  context, including `dg_context_destroy`.
- Nothing falls back. If the context cannot be created the call returns
  `DG_ERR_BACKEND_UNAVAILABLE` (no loader, OpenGL below 4.3, a missing entry
  point) or `DG_ERR_BACKEND_FAILURE` (a shader did not build) with the
  reason, for example the driver's compile log, in the error buffer. The
  application decides whether to create a CPU context instead.
- By default the library saves and restores the current program and the
  shader storage bindings 0 to 7 around every call
  (`DG_CONTEXT_GL_PRESERVE_STATE`). OpenGL errors raised before a call are
  cleared by it.
- A simulation thread other than the render thread needs its own context
  that shares objects with the render context (SDL3:
  `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`), current on the simulation thread.
- WebGL 2 and OpenGL ES have no path here. Web builds use the CPU backend.

### Without stalling a frame

`dg_knn` and the other synchronous calls wait for the GPU and read the
results back. The job form splits that:

```c
dg_job job;
dg_knn_submit(context, &knn, &job);       /* queues GPU work, returns */
/* ... render, or submit more jobs ... */
if (dg_job_poll(context, job) == DG_OK) { /* finished: collect will not wait */
    dg_knn_collect(context, job, &out);
}
```

A context holds up to `max_jobs` (at most 8) submitted jobs. The point set
cannot be replaced while a job is outstanding. `dg_job_gl_buffer` returns
the OpenGL buffer that holds a job's results, in a documented layout, for a
renderer or compute shader that consumes them on the GPU; release the job
with `dg_job_release` when done.

`DG_CONTEXT_PROFILE` in `dg_context_config.flags` fills
`dg_context_stats.profile_ns[]` with the time of each stage. It changes no
result but makes the GPU context slower, so leave it off for timing.

## The CPU backend on several threads

`dg_cpu_knn` and `dg_cpu_aggregate` are pure functions of the binned
context. Any number of threads may call them at once on one context, each
with its own part of the queries, as long as no thread bins meanwhile.
They are not bound by `max_queries`. Binning and `dg_field` run on one
thread.

## Order and speed

Results never depend on the order of the queries or of the points; speed
does. A range of query points (`queries == NULL`) covering at least 1/64 of
all points is answered in cell order on both backends. An explicit list is
answered as given: sort it by position, or pass a range, when it is long.

When the caller's arrays are in no spatial order, the writes of binning and
of the results land all over memory, and from a few million points on that
is most of the cost on either backend. `dg_context_cell_order(context,
order, capacity)` gives the order of the last bin: `order[s]` is the index
of the point that belongs at position s. An application that can renumber
its entities permutes its arrays by it every so many ticks. Measured on the
target machine: on the GPU it more than halves the tick at 10,000,000 points
and does nothing at 1,000,000 or below; reorder every 8 to 32 ticks, never
every tick. On the CPU it is worth 3% to 11% of the benchmark tick at
1,000,000 points and above.

## Measured

Version 0.1.6 (the same kernels as 0.1.7) on a Ryzen 5 5600X with an RTX
3070, NVIDIA driver 535.261.03. One benchmark tick bins every point, finds
the 7 nearest neighbors of 12,000 points, an aggregate for every point and a
field; flock scene; each arm in a run of its own; milliseconds:

| Points | Arm | p50 | p95 | p99 |
| ---: | --- | ---: | ---: | ---: |
| 100,000 | CPU, 1 thread | 97.6 | 131.9 | 136.4 |
| | CPU, aggregate on 12 threads | 30.6 | 44.0 | 47.6 |
| | GPU | 4.0 | 4.7 | 4.9 |
| 1,000,000 | CPU, 1 thread | 1,029.0 | 1,523.7 | 1,562.6 |
| | CPU, aggregate on 12 threads | 227.1 | 328.2 | 333.0 |
| | GPU | 27.9 | 36.6 | 41.5 |
| 10,000,000 | CPU, 1 thread | 14,655.3 | 15,701.5 | 15,701.5 |
| | CPU, aggregate on 12 threads | 3,357.0 | 3,711.1 | 3,711.1 |
| | GPU | 583.9 | 618.9 | 618.9 |

- Every compared GPU result equalled the CPU result, except in runs at
  10,000,000 points on the test machine where a single bit of host memory
  differed, in the CPU's arrays as often as in the GPU's. That machine's
  memory test found a failing cell; neither backend computed a wrong value.
- A GPU that idles starts slowly: after a 200 ms pause the GPU tick at
  100,000 points takes 16.1 ms instead of 3.4. A simulation at 20 Hz or
  faster is barely affected; one that ticks a few times a second on an
  otherwise idle GPU pays two to six times the tick.
- Not run: AMD or Intel drivers, Windows, a big-endian host.

## Limits

| Limit | Value |
| --- | --- |
| Coordinates and grid origin | within +/- 2^28 (`DG_COORD_MAX`, equal to `DC_COORD_MAX`) |
| Radius | 0 to 2^28 |
| k | 1 to 32 |
| Value channels | 4 |
| Jobs in flight | 8 |
| GPU buffers | each at most `GL_MAX_SHADER_STORAGE_BLOCK_SIZE` and never more than 2^34 bytes |

Memory is reserved at creation for the configured capacities:

| Backend | Per point | With value channels | Per cell |
| --- | ---: | ---: | ---: |
| CPU, host memory | 44 bytes | 76 bytes | 4 bytes |
| GPU, host memory (staging for the upload) | 20 bytes | 36 bytes | 0 |
| GPU, OpenGL buffers | 40 bytes | 72 bytes | 12 bytes |

On the CPU backend each job slot also holds 4 bytes per query of
`max_queries`. On the GPU backend a job slot's buffers grow to the largest
job seen and are then kept. `DG_CONTEXT_VALUES` reserves the value channels
and is on by default.
