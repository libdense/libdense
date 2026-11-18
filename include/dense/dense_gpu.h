#ifndef DENSE_GPU_H
#define DENSE_GPU_H

/*
 * libdense_gpu - exact bulk spatial queries for local simulations, with an
 * optional OpenGL 4.3 compute shader backend. EXPERIMENTAL since 0.4.7.1:
 * the API and ABI may change, and the module is outside `make all`,
 * `make test` and `make install`.
 *
 * Kernels: grid binning, k nearest neighbors within a radius, in-radius
 * aggregates and per-cell aggregate fields, over int32 points in 2D or 3D.
 *
 * Every result is defined by integer arithmetic only and does not depend on
 * the backend, the grid or the order in which candidates are visited. The
 * CPU backend is the reference and the default. The GPU backend must return
 * byte-identical results; tests/test_dense_gpu_gl.c enforces that.
 *
 * The library links neither OpenGL nor SDL. The GPU backend calls OpenGL
 * through function pointers that the application's loader supplies (for
 * SDL3: SDL_GL_GetProcAddress), on a context the application owns.
 * See DESIGN.md for the full contract.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DG_VERSION_MAJOR 0
#define DG_VERSION_MINOR 1
#define DG_VERSION_PATCH 7
#define DG_VERSION_STRING "0.1.7"
/* 0 while experimental: no ABI stability is promised. */
#define DG_ABI_VERSION 0

#if defined(_WIN32) && defined(DG_SHARED)
#if defined(DG_BUILD)
#define DG_API __declspec(dllexport)
#else
#define DG_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(DG_SHARED) && defined(DG_BUILD)
#define DG_API __attribute__((visibility("default")))
#else
#define DG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dg_result {
    DG_OK = 0,
    /* dg_job_poll only: the job has not finished on the GPU. */
    DG_PENDING,
    DG_ERR_INVALID_ARGUMENT,
    DG_ERR_OUT_OF_MEMORY,
    /* More points, cells, queries, neighbors or jobs than the context
     * holds, or a GPU buffer larger than the OpenGL context allows for one
     * shader storage block. */
    DG_ERR_CAPACITY,
    /* Coordinate, origin, radius or cell size outside the documented limits. */
    DG_ERR_RANGE,
    /* A query was submitted before the first successful dg_context_bin. */
    DG_ERR_NOT_BINNED,
    DG_ERR_JOB_NOT_FOUND,
    /* The job exists but is of another kind than the collect call. */
    DG_ERR_JOB_KIND,
    /* dg_context_bin while submitted jobs are not collected or released. */
    DG_ERR_JOBS_PENDING,
    /* The requested backend cannot run here: no loader, an OpenGL version
     * below 4.3, a missing entry point or a limit below the requirement.
     * Nothing falls back to the CPU on its own. */
    DG_ERR_BACKEND_UNAVAILABLE,
    /* The backend reported an error while running (shader build, OpenGL
     * error, lost context). The text is in dg_context_last_error or in the
     * error buffer of dg_context_create. */
    DG_ERR_BACKEND_FAILURE,
} dg_result;

DG_API const char *dg_result_string(dg_result result);

/* ------------------------------------------------------------------ */
/* Limits                                                             */
/* ------------------------------------------------------------------ */

/* Coordinates and grid origins must be within +/- DG_COORD_MAX, a radius
 * within [0, DG_RADIUS_MAX]. With these limits every difference fits int32
 * and every squared distance fits uint64 (below 2^60). The coordinate limit
 * equals DC_COORD_MAX of libdense_collision. */
#define DG_COORD_MAX ((int32_t)(1 << 28))
#define DG_RADIUS_MAX ((int32_t)(1 << 28))
#define DG_KNN_MAX_K 32u
#define DG_VALUE_CHANNELS 4u
#define DG_MAX_JOBS 8u
#define DG_INDEX_NONE UINT32_MAX
#define DG_MASK_ALL UINT32_MAX
/* group of a point whose group stream is absent */
#define DG_GROUP_DEFAULT 1u

/* ------------------------------------------------------------------ */
/* Context                                                            */
/* ------------------------------------------------------------------ */

typedef enum dg_backend {
    /* Scalar reference. The default: a zeroed config selects it. */
    DG_BACKEND_CPU = 0,
    /* OpenGL 4.3 compute shaders on the application's context. */
    DG_BACKEND_GL_COMPUTE = 1,
} dg_backend;

typedef void (*dg_gl_proc)(void);
/* Returns the OpenGL entry point `name` of the current context, or NULL.
 * For SDL3: return (dg_gl_proc)SDL_GL_GetProcAddress(name). */
typedef dg_gl_proc (*dg_gl_loader_fn)(void *user, const char *name);

/* Save and restore the OpenGL state the backend changes (current program,
 * shader storage buffer bindings 0..7 and the generic binding) around every
 * call. On by default; clear it when nothing else on the context depends on
 * that state, to save about thirty glGet calls per library call. */
#define DG_CONTEXT_GL_PRESERVE_STATE ((uint32_t)0x1u)
/* Reserve storage for the value channels (32 bytes per point on the CPU
 * backend; 16 bytes of host memory and 32 bytes of OpenGL buffers per point
 * on the GPU backend).
 * Without it a dg_context_bin that supplies a value stream returns
 * DG_ERR_INVALID_ARGUMENT. On by default. */
#define DG_CONTEXT_VALUES ((uint32_t)0x2u)

/* Time the stages of the work into dg_context_stats.profile_ns. Off by
 * default; it changes no result. The GPU backend calls glFinish around
 * every stage, so jobs no longer overlap and the context is slower: a
 * profiled context says where time goes, its totals are not benchmark
 * evidence. (Bits 0x4 and 0x8 were two switches of 0.1.3 that the target
 * machine rejected; they are not accepted.) */
#define DG_CONTEXT_PROFILE ((uint32_t)0x10u)

typedef struct dg_context_config {
    dg_backend backend;
    /* Hard capacities, reserved at creation. Exceeding one returns
     * DG_ERR_CAPACITY; nothing grows afterwards. */
    uint32_t max_points;
    uint32_t max_cells;
    uint32_t max_queries;   /* per job; dg_cpu_knn and dg_cpu_aggregate
                             * are not jobs and are not bound by it */
    uint32_t max_k;         /* 1..DG_KNN_MAX_K */
    uint32_t max_jobs;      /* submitted and not yet collected, 1..DG_MAX_JOBS */
    uint32_t flags;         /* DG_CONTEXT_* */
    /* DG_BACKEND_GL_COMPUTE only. The context must be current on the calling
     * thread for dg_context_create and for every later call on the context. */
    dg_gl_loader_fn gl_loader;
    void *gl_loader_user;
    /* DG_BACKEND_GL_COMPUTE only. Work groups (of 64 items) per
     * glDispatchCompute; a larger pass is split into several dispatches.
     * 0 selects 16,384, about a million items. Lower it when one dispatch
     * of a crowded query could run long enough to trip the driver's
     * watchdog. A value above the context's
     * GL_MAX_COMPUTE_WORK_GROUP_COUNT (dg_gl_info) is DG_ERR_CAPACITY. It
     * does not affect results. */
    uint32_t gl_groups_per_dispatch;
} dg_context_config;

typedef struct dg_context dg_context;

/* CPU backend, 65,536 points, 65,536 cells, 65,536 queries, k 16, 4 jobs,
 * DG_CONTEXT_GL_PRESERVE_STATE | DG_CONTEXT_VALUES. */
DG_API void dg_context_config_defaults(dg_context_config *config);

/* error_text (optional) receives a zero-terminated description when the
 * call fails, for example the shader compiler's log. */
DG_API dg_result dg_context_create(
    const dg_context_config *config,
    dg_context **out_context,
    char *error_text,
    size_t error_capacity
);
/* GPU backend: the OpenGL context must be current. */
DG_API void dg_context_destroy(dg_context *context);

DG_API dg_backend dg_context_backend(const dg_context *context);
/* Text of the most recent DG_ERR_BACKEND_* on this context; "" if none. */
DG_API const char *dg_context_last_error(const dg_context *context);

typedef struct dg_gl_info {
    char vendor[64];
    char renderer[128];
    char version[64];
    int32_t version_major;
    int32_t version_minor;
    int64_t max_shader_storage_block_bytes;
    int32_t max_compute_shader_storage_blocks;
    int32_t max_compute_work_group_count_x;
} dg_gl_info;

/* DG_ERR_BACKEND_UNAVAILABLE on a CPU context. */
DG_API dg_result dg_context_gl_info(
    const dg_context *context,
    dg_gl_info *out_info
);

/* Stages timed with DG_CONTEXT_PROFILE. */
typedef enum dg_profile_stage {
    /* Both backends, in dg_context_bin: the range check and the copy of the
     * caller's streams. */
    DG_PROFILE_PACK = 0,
    /* GPU: glBufferSubData of points, seek masks and values. */
    DG_PROFILE_UPLOAD,
    /* Both: points per cell. Prefix sum over the cells. Points into cell
     * order. */
    DG_PROFILE_COUNT,
    DG_PROFILE_PREFIX,
    DG_PROFILE_SCATTER,
    /* GPU: a job from submit to the end of its last dispatch, by kind. The
     * CPU backend does not fill these; time its calls directly. */
    DG_PROFILE_KNN,
    DG_PROFILE_AGGREGATE,
    DG_PROFILE_FIELD,
    /* GPU: glGetBufferSubData in collect. */
    DG_PROFILE_READBACK,
    DG_PROFILE_STAGES
} dg_profile_stage;

DG_API const char *dg_profile_stage_string(dg_profile_stage stage);

typedef struct dg_context_stats {
    dg_backend backend;
    uint32_t point_count;
    uint32_t cell_count;
    uint32_t jobs_in_flight;
    uint64_t bins;
    uint64_t knn_jobs;
    uint64_t aggregate_jobs;
    uint64_t field_jobs;
    uint64_t queries;
    /* GPU backend only. */
    uint64_t gl_dispatches;
    uint64_t gl_bytes_uploaded;
    uint64_t gl_bytes_read_back;
    uint64_t gl_buffer_reallocations;
    size_t gl_buffer_bytes;
    /* Heap memory of this context. */
    size_t retained_bytes;
    /* Nanoseconds per stage since creation, by the wall clock
     * (timespec_get). All zero without DG_CONTEXT_PROFILE. */
    uint64_t profile_ns[DG_PROFILE_STAGES];
} dg_context_stats;

DG_API dg_result dg_context_get_stats(
    const dg_context *context,
    dg_context_stats *out_stats
);

/* ------------------------------------------------------------------ */
/* Points and grid                                                    */
/* ------------------------------------------------------------------ */

/* A run of 32-bit elements inside caller memory. stride is in bytes;
 * zero means 4 (a plain array). Elements need not be aligned. */
typedef struct dg_stream {
    const void *data;
    size_t stride;
} dg_stream;

typedef struct dg_points {
    uint32_t count;
    dg_stream x;                         /* int32, required */
    dg_stream y;                         /* int32, required */
    dg_stream z;                         /* int32; absent: 0 (2D) */
    dg_stream group;                     /* uint32; absent: DG_GROUP_DEFAULT */
    dg_stream seek;                      /* uint32; absent: DG_MASK_ALL */
    dg_stream value[DG_VALUE_CHANNELS];  /* int32; absent: 0 */
} dg_points;

/*
 * A bounded grid of cubic cells. A point outside the grid belongs to the
 * nearest edge cell, so the grid bounds affect speed and never results.
 * cells_z is 1 for flat worlds. cells_x * cells_y * cells_z must not exceed
 * the context's max_cells.
 */
typedef struct dg_grid {
    int32_t origin_x;
    int32_t origin_y;
    int32_t origin_z;
    int32_t cell_size;   /* 1..DG_COORD_MAX */
    uint32_t cells_x;
    uint32_t cells_y;
    uint32_t cells_z;
} dg_grid;

/*
 * Copy the points and sort them into the grid. Replaces the previous point
 * set. On an error other than DG_ERR_BACKEND_FAILURE the previous point set
 * and its binning are unchanged; after DG_ERR_BACKEND_FAILURE the context
 * has no point set. Point i is "index i" in every request and result until
 * the next call.
 */
DG_API dg_result dg_context_bin(
    dg_context *context,
    const dg_points *points,
    const dg_grid *grid
);

/*
 * The indices of the points of the last dg_context_bin in cell order:
 * out_index[0 .. point count) is a permutation of the indices, by cell
 * (x fastest, then y, then z) and by index within a cell. The same array on
 * both backends.
 *
 * For an application that can reorder its own entities. Every kernel is
 * faster when the caller's arrays are already close to cell order, by a
 * large factor at millions of points (README.md, "Input order"): entity s
 * of the reordered arrays is entity out_index[s] of the current ones.
 * Points move, so the order decays; reorder again every so many ticks.
 * Reordering changes which index names which point and nothing else.
 *
 * The call does not change the context. On the GPU backend it reads 4
 * bytes per point and 4 per cell back and orders each cell on the calling
 * thread.
 * DG_ERR_NOT_BINNED before the first bin; DG_ERR_CAPACITY when capacity is
 * below the point count.
 */
DG_API dg_result dg_context_cell_order(
    dg_context *context,
    uint32_t *out_index,
    size_t capacity
);

/* ------------------------------------------------------------------ */
/* Queries                                                            */
/* ------------------------------------------------------------------ */

/*
 * A candidate j matches query point i when
 *   j != i,
 *   (group[j] & seek[i]) != 0, and
 *   dx*dx + dy*dy + dz*dz <= radius*radius   (exact, in uint64).
 * To query at a place where no point is, add a point with group 0 there:
 * nothing can match it and it can still be a query.
 *
 * Query points are queries[0..query_count-1] (point indices, repeats
 * allowed), or, when queries is NULL, the points first_point ..
 * first_point + query_count - 1.
 *
 * Speed, not results: a range that covers at least 1/64 of the points is
 * answered in cell order, which keeps consecutive queries on the same
 * cells; an explicit list is answered in the order given, so a list sorted
 * by position is faster than a scattered one.
 */
typedef struct dg_query_set {
    const uint32_t *queries;
    uint32_t first_point;
    uint32_t query_count;
} dg_query_set;

/* Fill dg_knn_output.distances2. */
#define DG_KNN_WANT_DISTANCES ((uint32_t)0x1u)

/*
 * The k matching candidates that are smallest by (squared distance, index),
 * in that order. The tie-break by index makes the result unique.
 */
typedef struct dg_knn_request {
    dg_query_set set;
    int32_t radius;
    uint32_t k;        /* 1..max_k */
    uint32_t flags;    /* DG_KNN_* */
} dg_knn_request;

typedef struct dg_knn_output {
    /* [query_count] number of neighbors found, 0..k */
    uint32_t *counts;
    /* [query_count * k] point indices; unused slots are DG_INDEX_NONE */
    uint32_t *neighbors;
    /* [query_count * k] squared distances; unused slots are 0. Required
     * with DG_KNN_WANT_DISTANCES, ignored without it. */
    uint64_t *distances2;
} dg_knn_output;

/* Sums over all matching candidates of one query, or over the points of one
 * cell. 64 bytes; the sums cannot overflow within the documented limits. */
typedef struct dg_aggregate {
    uint32_t count;
    uint32_t reserved;   /* 0 */
    int64_t sum_x;
    int64_t sum_y;
    int64_t sum_z;
    int64_t sum_value[DG_VALUE_CHANNELS];
} dg_aggregate;

typedef struct dg_aggregate_request {
    dg_query_set set;
    int32_t radius;
    uint32_t flags;      /* 0 */
} dg_aggregate_request;

/*
 * One dg_aggregate per grid cell, cell (cx, cy, cz) at index
 * cx + cells_x * (cy + cells_y * cz), over the cell's points whose group
 * intersects group_mask. A point outside the grid counts in its edge cell.
 */
typedef struct dg_field_request {
    uint32_t group_mask;
    uint32_t flags;      /* 0 */
} dg_field_request;

/* Synchronous forms: submit, wait and collect in one call. Each uses one
 * free job slot for the duration of the call (DG_ERR_CAPACITY if none). */
DG_API dg_result dg_knn(
    dg_context *context,
    const dg_knn_request *request,
    const dg_knn_output *output
);
DG_API dg_result dg_aggregate_query(
    dg_context *context,
    const dg_aggregate_request *request,
    dg_aggregate *out_aggregates,   /* [query_count] */
    size_t capacity
);
DG_API dg_result dg_field(
    dg_context *context,
    const dg_field_request *request,
    dg_aggregate *out_cells,        /* [cell count] */
    size_t capacity
);

/* ------------------------------------------------------------------ */
/* Jobs: submit now, collect later                                    */
/* ------------------------------------------------------------------ */

/*
 * A submitted job holds one of the context's max_jobs slots until it is
 * collected or released. On the GPU a slot keeps its result buffer; a job
 * takes the free slot whose buffer fits it, so jobs of different sizes that
 * repeat every tick each settle on a buffer of their own. Its query indices are copied at submit. The point
 * set cannot be replaced while a job is outstanding (DG_ERR_JOBS_PENDING).
 * On the GPU backend submit only queues OpenGL commands; collect blocks
 * until they have run and reads the results back. On the CPU backend the
 * work runs inside collect. Results do not depend on which is used.
 */
typedef uint64_t dg_job;
#define DG_JOB_NONE ((dg_job)0)

DG_API dg_result dg_knn_submit(
    dg_context *context,
    const dg_knn_request *request,
    dg_job *out_job
);
DG_API dg_result dg_aggregate_submit(
    dg_context *context,
    const dg_aggregate_request *request,
    dg_job *out_job
);
DG_API dg_result dg_field_submit(
    dg_context *context,
    const dg_field_request *request,
    dg_job *out_job
);

/* DG_OK when collect will not block, DG_PENDING otherwise. Always DG_OK on
 * the CPU backend. */
DG_API dg_result dg_job_poll(dg_context *context, dg_job job);

/* A successful collect releases the job. After an error other than
 * DG_ERR_BACKEND_FAILURE the job is still held and collect can be repeated. */
DG_API dg_result dg_knn_collect(
    dg_context *context,
    dg_job job,
    const dg_knn_output *output
);
DG_API dg_result dg_aggregate_collect(
    dg_context *context,
    dg_job job,
    dg_aggregate *out_aggregates,
    size_t capacity
);
DG_API dg_result dg_field_collect(
    dg_context *context,
    dg_job job,
    dg_aggregate *out_cells,
    size_t capacity
);
/* Give the slot back without reading results. */
DG_API dg_result dg_job_release(dg_context *context, dg_job job);

/*
 * GPU backend: the OpenGL buffer that holds a job's results, for consumers
 * that stay on the GPU (a renderer or the application's own compute shader)
 * and want no readback. The buffer is owned by the context and is reused by
 * a later job once this one is collected or released. Layout, as uint32
 * (std430 `uint data[]`), with q = query_count:
 *   k nearest:  counts [0, q), neighbors [q, q + q*k), then with
 *               DG_KNN_WANT_DISTANCES distances as (low, high) pairs
 *               [q + q*k, q + 3*q*k).
 *   aggregate and field: 16 words per query or cell: count, 0, then
 *               sum_x, sum_y, sum_z, sum_value[0..3] as (low, high) pairs.
 * Wait for dg_job_poll to return DG_OK, or insert your own
 * glMemoryBarrier, before reading it. DG_ERR_BACKEND_UNAVAILABLE on a CPU
 * context.
 */
DG_API dg_result dg_job_gl_buffer(
    dg_context *context,
    dg_job job,
    uint32_t *out_buffer_name,
    size_t *out_bytes
);

/* ------------------------------------------------------------------ */
/* CPU backend from several threads                                   */
/* ------------------------------------------------------------------ */

/*
 * Pure functions over the current point set of a CPU context: they change
 * no context state and may run concurrently on one context from any number
 * of threads, provided no thread calls a non-const function of that context
 * meanwhile. Give each thread its own part of the queries and outputs.
 * They return DG_ERR_BACKEND_UNAVAILABLE on a GPU context and do not count
 * in dg_context_stats. They keep no copy of the queries, so query_count may
 * exceed max_queries.
 */
DG_API dg_result dg_cpu_knn(
    const dg_context *context,
    const dg_knn_request *request,
    const dg_knn_output *output
);
DG_API dg_result dg_cpu_aggregate(
    const dg_context *context,
    const dg_aggregate_request *request,
    dg_aggregate *out_aggregates,
    size_t capacity
);

/* ------------------------------------------------------------------ */
/* Family-wide allocation metrics                                     */
/* ------------------------------------------------------------------ */

typedef struct dg_allocation_metrics {
    size_t current_retained_bytes;
    size_t peak_retained_bytes;
    uint64_t growth_operations;
    size_t live_object_count;
    uint64_t allocation_failures;
    uint64_t steady_state_allocations;
} dg_allocation_metrics;

/*
 * Process-wide metrics for heap memory owned by this library (OpenGL
 * buffers are reported per context in dg_context_stats). Reset starts a
 * post-prewarm window by clearing failure and steady-state allocation
 * counters only.
 */
DG_API void dg_get_allocation_metrics(dg_allocation_metrics *out_metrics);
DG_API void dg_reset_allocation_counters(void);

#ifdef __cplusplus
}
#endif

#endif
