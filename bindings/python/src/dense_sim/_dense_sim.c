#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "dense_sim.h"

typedef struct {
    PyObject_HEAD
    ds_world *world;
    uint64_t view_generation;
} DenseWorld;

typedef struct {
    PyObject_HEAD
    DenseWorld *world;
    ds_fanout_view view;
    uint64_t generation;
} DenseFanoutView;

typedef struct {
    PyObject_HEAD
    DenseFanoutView *fanout;
    Py_ssize_t index;
} DenseChunkDeltaView;

typedef struct {
    PyObject_HEAD
    DenseFanoutView *fanout;
    Py_ssize_t chunk_index;
} DenseEntrySequence;

typedef struct {
    PyObject_HEAD
    DenseFanoutView *fanout;
    Py_ssize_t chunk_index;
} DenseSubscriberSequence;

typedef struct {
    PyObject_HEAD
    ds_entity_id entity_id;
    ds_channel_mask channel_mask;
    ds_delta_op operation;
} DenseDeltaEntry;

typedef struct {
    Py_buffer view;
    size_t count;
    bool acquired;
} DenseBuffer;

static PyTypeObject DenseWorldType;
static PyTypeObject DenseFanoutViewType;
static PyTypeObject DenseChunkDeltaViewType;
static PyTypeObject DenseEntrySequenceType;
static PyTypeObject DenseSubscriberSequenceType;
static PyTypeObject DenseDeltaEntryType;
static PyObject *DenseSimError;

PyMODINIT_FUNC PyInit__dense_sim(void);

static int dense_raise_result(ds_result result, const char *context)
{
    PyErr_Format(
        DenseSimError,
        "%s: %s",
        context,
        ds_result_string(result)
    );
    return -1;
}

static int dense_world_require_open(DenseWorld *self)
{
    if (self->world == NULL) {
        PyErr_SetString(DenseSimError, "world is closed");
        return -1;
    }
    return 0;
}

static int dense_fanout_require_valid(DenseFanoutView *self)
{
    if (
        self->world == NULL ||
        self->world->world == NULL ||
        self->world->view_generation != self->generation
    ) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "fanout view was invalidated by the next successful begin_tick() or world close"
        );
        return -1;
    }
    return 0;
}

static int dense_normalize_index(
    Py_ssize_t *index,
    Py_ssize_t length
)
{
    if (*index < 0) {
        *index += length;
    }
    if (*index < 0 || *index >= length) {
        PyErr_SetString(PyExc_IndexError, "dense_sim view index out of range");
        return -1;
    }
    return 0;
}

static const ds_chunk_delta *dense_chunk_at(
    DenseFanoutView *fanout,
    Py_ssize_t index
)
{
    Py_ssize_t length;

    if (dense_fanout_require_valid(fanout) < 0) {
        return NULL;
    }

    if (fanout->view.chunk_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "fanout group count exceeds Python sequence limits");
        return NULL;
    }
    length = (Py_ssize_t)fanout->view.chunk_count;
    if (dense_normalize_index(&index, length) < 0) {
        return NULL;
    }
    return &fanout->view.chunks[(size_t)index];
}

static const char *dense_delta_operation_name(ds_delta_op operation)
{
    switch (operation) {
        case DS_DELTA_UPDATE:
            return "UPDATE";
        case DS_DELTA_ENTER:
            return "ENTER";
        case DS_DELTA_LEAVE:
            return "LEAVE";
        case DS_DELTA_SPAWN:
            return "SPAWN";
        case DS_DELTA_REMOVE:
            return "REMOVE";
        default:
            return "UNKNOWN";
    }
}

static bool dense_is_little_endian(void)
{
    const uint16_t value = UINT16_C(1);
    unsigned char first;

    memcpy(&first, &value, sizeof(first));
    return first == 1u;
}

static const char *dense_buffer_format_code(
    const char *format,
    bool *native_endian
)
{
    const char *code = format;

    *native_endian = true;
    if (code == NULL || *code == '\0') {
        return NULL;
    }

    if (*code == '@' || *code == '=') {
        code++;
    } else if (*code == '<') {
        *native_endian = dense_is_little_endian();
        code++;
    } else if (*code == '>' || *code == '!') {
        *native_endian = !dense_is_little_endian();
        code++;
    }

    if (code[0] == '\0' || code[1] != '\0') {
        return NULL;
    }
    return code;
}

static bool dense_format_is_u64(const Py_buffer *view)
{
    const char *code;
    bool native_endian;

    if (view->itemsize != (Py_ssize_t)sizeof(uint64_t)) {
        return false;
    }
    code = dense_buffer_format_code(view->format, &native_endian);
    if (code == NULL || !native_endian) {
        return false;
    }
    if (*code == 'Q') {
        return sizeof(unsigned long long) == sizeof(uint64_t);
    }
    if (*code == 'L') {
        return sizeof(unsigned long) == sizeof(uint64_t);
    }
    return false;
}

static bool dense_format_is_i32(const Py_buffer *view)
{
    const char *code;
    bool native_endian;

    if (view->itemsize != (Py_ssize_t)sizeof(int32_t)) {
        return false;
    }
    code = dense_buffer_format_code(view->format, &native_endian);
    if (code == NULL || !native_endian) {
        return false;
    }
    if (*code == 'i') {
        return sizeof(int) == sizeof(int32_t);
    }
    if (*code == 'l') {
        return sizeof(long) == sizeof(int32_t);
    }
    return false;
}

static int dense_buffer_acquire(
    PyObject *object,
    DenseBuffer *buffer,
    bool expect_u64,
    const char *name
)
{
    memset(buffer, 0, sizeof(*buffer));
    if (
        PyObject_GetBuffer(
            object,
            &buffer->view,
            PyBUF_FORMAT | PyBUF_ND | PyBUF_C_CONTIGUOUS
        ) < 0
    ) {
        PyErr_Format(
            PyExc_TypeError,
            "%s must expose a contiguous one-dimensional buffer",
            name
        );
        return -1;
    }
    buffer->acquired = true;

    if (buffer->view.ndim != 1) {
        PyErr_Format(PyExc_ValueError, "%s must be one-dimensional", name);
        return -1;
    }
    if (
        expect_u64
            ? !dense_format_is_u64(&buffer->view)
            : !dense_format_is_i32(&buffer->view)
    ) {
        PyErr_Format(
            PyExc_TypeError,
            "%s must use native %s elements; got format %s with itemsize %zd",
            name,
            expect_u64 ? "uint64" : "int32",
            buffer->view.format == NULL ? "<none>" : buffer->view.format,
            buffer->view.itemsize
        );
        return -1;
    }
    if (buffer->view.itemsize <= 0 || buffer->view.len < 0) {
        PyErr_Format(PyExc_ValueError, "%s has an invalid buffer length", name);
        return -1;
    }
    if (buffer->view.len % buffer->view.itemsize != 0) {
        PyErr_Format(PyExc_ValueError, "%s has a partial trailing element", name);
        return -1;
    }

    buffer->count = (size_t)(buffer->view.len / buffer->view.itemsize);
    return 0;
}

static void dense_buffer_release(DenseBuffer *buffer)
{
    if (buffer->acquired) {
        PyBuffer_Release(&buffer->view);
        buffer->acquired = false;
    }
}

static uint64_t dense_buffer_read_u64(const DenseBuffer *buffer, size_t index)
{
    uint64_t value;
    const unsigned char *source =
        (const unsigned char *)buffer->view.buf +
        index * sizeof(value);

    memcpy(&value, source, sizeof(value));
    return value;
}

static int32_t dense_buffer_read_i32(const DenseBuffer *buffer, size_t index)
{
    int32_t value;
    const unsigned char *source =
        (const unsigned char *)buffer->view.buf +
        index * sizeof(value);

    memcpy(&value, source, sizeof(value));
    return value;
}

static PyObject *DenseWorld_new(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs
)
{
    DenseWorld *self = (DenseWorld *)type->tp_alloc(type, 0);

    (void)args;
    (void)kwargs;
    if (self != NULL) {
        self->world = NULL;
        self->view_generation = 1u;
    }
    return (PyObject *)self;
}

static int DenseWorld_init(
    DenseWorld *self,
    PyObject *args,
    PyObject *kwargs
)
{
    static char *keywords[] = {
        "cell_size",
        "chunk_size",
        "initial_entity_capacity",
        "initial_observer_capacity",
        NULL,
    };
    int cell_size = 8;
    int chunk_size = 16;
    Py_ssize_t entity_capacity = 1024;
    Py_ssize_t observer_capacity = 64;
    ds_world_config config;
    ds_world *world = NULL;
    ds_result result;

    if (
        !PyArg_ParseTupleAndKeywords(
            args,
            kwargs,
            "|iinn:World",
            keywords,
            &cell_size,
            &chunk_size,
            &entity_capacity,
            &observer_capacity
        )
    ) {
        return -1;
    }
    if (entity_capacity < 0 || observer_capacity < 0) {
        PyErr_SetString(PyExc_ValueError, "initial capacities must be non-negative");
        return -1;
    }

    config = (ds_world_config) {
        .cell_size = (int32_t)cell_size,
        .chunk_size = (int32_t)chunk_size,
        .initial_entity_capacity = (size_t)entity_capacity,
        .initial_observer_capacity = (size_t)observer_capacity,
    };
    result = ds_world_create(&config, &world);
    if (result != DS_OK) {
        return dense_raise_result(result, "World creation failed");
    }

    if (self->world != NULL) {
        ds_world_destroy(self->world);
        self->view_generation++;
    }
    self->world = world;
    return 0;
}

static void DenseWorld_dealloc(DenseWorld *self)
{
    ds_world_destroy(self->world);
    self->world = NULL;
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *DenseWorld_close(DenseWorld *self, PyObject *Py_UNUSED(ignored))
{
    if (self->world != NULL) {
        ds_world_destroy(self->world);
        self->world = NULL;
        self->view_generation++;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_begin_tick(DenseWorld *self, PyObject *args)
{
    unsigned long long tick;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:begin_tick", &tick)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }

    result = ds_world_begin_tick(self->world, (ds_tick)tick);
    if (result != DS_OK) {
        dense_raise_result(result, "begin_tick failed");
        return NULL;
    }
    self->view_generation++;
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_end_tick(DenseWorld *self, PyObject *Py_UNUSED(ignored))
{
    ds_result result;

    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_world_end_tick(self->world);
    if (result != DS_OK) {
        dense_raise_result(result, "end_tick failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *dense_fanout_view_create(DenseWorld *world)
{
    DenseFanoutView *view;
    ds_fanout_view native_view;
    ds_result result = ds_world_get_fanout_view(world->world, &native_view);

    if (result != DS_OK) {
        dense_raise_result(result, "fanout_view failed");
        return NULL;
    }

    view = PyObject_New(DenseFanoutView, &DenseFanoutViewType);
    if (view == NULL) {
        return NULL;
    }
    Py_INCREF(world);
    view->world = world;
    view->view = native_view;
    view->generation = world->view_generation;
    return (PyObject *)view;
}

static PyObject *DenseWorld_fanout_view(DenseWorld *self, PyObject *Py_UNUSED(ignored))
{
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return dense_fanout_view_create(self);
}

static PyObject *DenseWorld_spawn(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    unsigned long long type_mask;
    int spatial_x;
    int spatial_y;
    ds_result result;

    if (
        !PyArg_ParseTuple(
            args,
            "KiiK:spawn",
            &entity_id,
            &spatial_x,
            &spatial_y,
            &type_mask
        )
    ) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_entity_spawn(
        self->world,
        (ds_entity_id)entity_id,
        (int32_t)spatial_x,
        (int32_t)spatial_y,
        (ds_type_mask)type_mask
    );
    if (result != DS_OK) {
        dense_raise_result(result, "spawn failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_move(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    int spatial_x;
    int spatial_y;
    ds_result result;

    if (!PyArg_ParseTuple(args, "Kii:move", &entity_id, &spatial_x, &spatial_y)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_entity_move(
        self->world,
        (ds_entity_id)entity_id,
        (int32_t)spatial_x,
        (int32_t)spatial_y
    );
    if (result != DS_OK) {
        dense_raise_result(result, "move failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_move_many(DenseWorld *self, PyObject *args)
{
    PyObject *ids_object;
    PyObject *xs_object;
    PyObject *ys_object;
    DenseBuffer ids;
    DenseBuffer xs;
    DenseBuffer ys;
    ds_result result = DS_OK;
    size_t processed = 0u;

    memset(&ids, 0, sizeof(ids));
    memset(&xs, 0, sizeof(xs));
    memset(&ys, 0, sizeof(ys));

    if (!PyArg_ParseTuple(args, "OOO:move_many", &ids_object, &xs_object, &ys_object)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    if (dense_buffer_acquire(ids_object, &ids, true, "entity_ids") < 0) {
        goto error;
    }
    if (dense_buffer_acquire(xs_object, &xs, false, "xs") < 0) {
        goto error;
    }
    if (dense_buffer_acquire(ys_object, &ys, false, "ys") < 0) {
        goto error;
    }
    if (ids.count != xs.count || ids.count != ys.count) {
        PyErr_SetString(
            PyExc_ValueError,
            "entity_ids, xs, and ys must contain the same number of elements"
        );
        goto error;
    }

    for (processed = 0u; processed < ids.count; processed++) {
        result = ds_entity_move(
            self->world,
            (ds_entity_id)dense_buffer_read_u64(&ids, processed),
            dense_buffer_read_i32(&xs, processed),
            dense_buffer_read_i32(&ys, processed)
        );
        if (result != DS_OK) {
            break;
        }
    }

    dense_buffer_release(&ys);
    dense_buffer_release(&xs);
    dense_buffer_release(&ids);

    if (result != DS_OK) {
        PyErr_Format(
            DenseSimError,
            "move_many failed at index %zu after %zu successful moves: %s; batch operations are sequential and are not rolled back",
            processed,
            processed,
            ds_result_string(result)
        );
        return NULL;
    }
    return PyLong_FromSize_t(processed);

error:
    dense_buffer_release(&ys);
    dense_buffer_release(&xs);
    dense_buffer_release(&ids);
    return NULL;
}

static PyObject *DenseWorld_set_motion_plan(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    unsigned long long start_tick;
    unsigned long long until_tick;
    double x;
    double y;
    double vx;
    double vy;
    ds_motion_plan plan;
    ds_result result;

    if (
        !PyArg_ParseTuple(
            args,
            "KKKdddd:set_motion_plan",
            &entity_id,
            &start_tick,
            &until_tick,
            &x,
            &y,
            &vx,
            &vy
        )
    ) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }

    plan = (ds_motion_plan) {
        .start_tick = (ds_tick)start_tick,
        .until_tick = (ds_tick)until_tick,
        .x = x,
        .y = y,
        .vx = vx,
        .vy = vy,
    };
    result = ds_entity_set_motion_plan(
        self->world,
        (ds_entity_id)entity_id,
        &plan
    );
    if (result != DS_OK) {
        dense_raise_result(result, "set_motion_plan failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_clear_motion_plan(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:clear_motion_plan", &entity_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }

    result = ds_entity_clear_motion_plan(
        self->world,
        (ds_entity_id)entity_id
    );
    if (result != DS_OK) {
        dense_raise_result(result, "clear_motion_plan failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_motion_mode(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    ds_motion_mode mode;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:motion_mode", &entity_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }

    result = ds_entity_motion_mode(
        self->world,
        (ds_entity_id)entity_id,
        &mode
    );
    if (result != DS_OK) {
        dense_raise_result(result, "motion_mode failed");
        return NULL;
    }
    return PyLong_FromLong((long)mode);
}

static PyObject *DenseWorld_remove(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:remove", &entity_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_entity_remove(self->world, (ds_entity_id)entity_id);
    if (result != DS_OK) {
        dense_raise_result(result, "remove failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_mark_dirty(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    unsigned long long channel_mask;
    ds_result result;

    if (!PyArg_ParseTuple(args, "KK:mark_dirty", &entity_id, &channel_mask)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_entity_mark_dirty(
        self->world,
        (ds_entity_id)entity_id,
        (ds_channel_mask)channel_mask
    );
    if (result != DS_OK) {
        dense_raise_result(result, "mark_dirty failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_mark_dirty_many(DenseWorld *self, PyObject *args)
{
    PyObject *ids_object;
    PyObject *masks_object;
    DenseBuffer ids;
    DenseBuffer masks;
    bool scalar_mask = false;
    uint64_t channel_mask = 0u;
    ds_result result = DS_OK;
    size_t processed = 0u;

    memset(&ids, 0, sizeof(ids));
    memset(&masks, 0, sizeof(masks));

    if (!PyArg_ParseTuple(args, "OO:mark_dirty_many", &ids_object, &masks_object)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    if (dense_buffer_acquire(ids_object, &ids, true, "entity_ids") < 0) {
        goto error;
    }

    if (PyLong_Check(masks_object)) {
        unsigned long long parsed = PyLong_AsUnsignedLongLong(masks_object);

        if (parsed == (unsigned long long)-1 && PyErr_Occurred()) {
            goto error;
        }
        channel_mask = (uint64_t)parsed;
        scalar_mask = true;
    } else {
        if (dense_buffer_acquire(masks_object, &masks, true, "channel_masks") < 0) {
            goto error;
        }
        if (ids.count != masks.count) {
            PyErr_SetString(
                PyExc_ValueError,
                "entity_ids and channel_masks must contain the same number of elements"
            );
            goto error;
        }
    }

    for (processed = 0u; processed < ids.count; processed++) {
        uint64_t current_mask = scalar_mask
            ? channel_mask
            : dense_buffer_read_u64(&masks, processed);

        result = ds_entity_mark_dirty(
            self->world,
            (ds_entity_id)dense_buffer_read_u64(&ids, processed),
            (ds_channel_mask)current_mask
        );
        if (result != DS_OK) {
            break;
        }
    }

    dense_buffer_release(&masks);
    dense_buffer_release(&ids);

    if (result != DS_OK) {
        PyErr_Format(
            DenseSimError,
            "mark_dirty_many failed at index %zu after %zu successful marks: %s; batch operations are sequential and are not rolled back",
            processed,
            processed,
            ds_result_string(result)
        );
        return NULL;
    }
    return PyLong_FromSize_t(processed);

error:
    dense_buffer_release(&masks);
    dense_buffer_release(&ids);
    return NULL;
}

static PyObject *DenseWorld_entity_exists(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;

    if (!PyArg_ParseTuple(args, "K:entity_exists", &entity_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyBool_FromLong(
        ds_entity_exists(self->world, (ds_entity_id)entity_id) ? 1L : 0L
    );
}

static PyObject *DenseWorld_entity(DenseWorld *self, PyObject *args)
{
    unsigned long long entity_id;
    ds_entity_desc entity;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:entity", &entity_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_entity_get(self->world, (ds_entity_id)entity_id, &entity);
    if (result != DS_OK) {
        dense_raise_result(result, "entity failed");
        return NULL;
    }
    return Py_BuildValue(
        "{s:K,s:i,s:i,s:K}",
        "id",
        (unsigned long long)entity.id,
        "spatial_x",
        entity.spatial_x,
        "spatial_y",
        entity.spatial_y,
        "type_mask",
        (unsigned long long)entity.type_mask
    );
}

static PyObject *DenseWorld_create_observer(DenseWorld *self, PyObject *args)
{
    int radius;
    unsigned long long type_mask;
    ds_observer_config config;
    ds_observer_id observer_id;
    ds_result result;

    if (!PyArg_ParseTuple(args, "iK:create_observer", &radius, &type_mask)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    config = (ds_observer_config) {
        .radius = (int32_t)radius,
        .type_mask = (ds_type_mask)type_mask,
    };
    result = ds_observer_create(self->world, &config, &observer_id);
    if (result != DS_OK) {
        dense_raise_result(result, "create_observer failed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)observer_id);
}

static PyObject *DenseWorld_destroy_observer(DenseWorld *self, PyObject *args)
{
    unsigned long long observer_id;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:destroy_observer", &observer_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_observer_destroy(self->world, (ds_observer_id)observer_id);
    if (result != DS_OK) {
        dense_raise_result(result, "destroy_observer failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_anchor_observer(DenseWorld *self, PyObject *args)
{
    unsigned long long observer_id;
    unsigned long long entity_id;
    ds_result result;

    if (!PyArg_ParseTuple(args, "KK:anchor_observer", &observer_id, &entity_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_observer_anchor_entity(
        self->world,
        (ds_observer_id)observer_id,
        (ds_entity_id)entity_id
    );
    if (result != DS_OK) {
        dense_raise_result(result, "anchor_observer failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_set_observer_position(DenseWorld *self, PyObject *args)
{
    unsigned long long observer_id;
    int spatial_x;
    int spatial_y;
    ds_result result;

    if (
        !PyArg_ParseTuple(
            args,
            "Kii:set_observer_position",
            &observer_id,
            &spatial_x,
            &spatial_y
        )
    ) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_observer_set_position(
        self->world,
        (ds_observer_id)observer_id,
        (int32_t)spatial_x,
        (int32_t)spatial_y
    );
    if (result != DS_OK) {
        dense_raise_result(result, "set_observer_position failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_set_observer_radius(DenseWorld *self, PyObject *args)
{
    unsigned long long observer_id;
    int radius;
    ds_result result;

    if (!PyArg_ParseTuple(args, "Ki:set_observer_radius", &observer_id, &radius)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_observer_set_radius(
        self->world,
        (ds_observer_id)observer_id,
        (int32_t)radius
    );
    if (result != DS_OK) {
        dense_raise_result(result, "set_observer_radius failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *DenseWorld_observer_exists(DenseWorld *self, PyObject *args)
{
    unsigned long long observer_id;

    if (!PyArg_ParseTuple(args, "K:observer_exists", &observer_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyBool_FromLong(
        ds_observer_exists(self->world, (ds_observer_id)observer_id) ? 1L : 0L
    );
}

static PyObject *DenseWorld_observer(DenseWorld *self, PyObject *args)
{
    unsigned long long observer_id;
    ds_observer_desc observer;
    ds_result result;

    if (!PyArg_ParseTuple(args, "K:observer", &observer_id)) {
        return NULL;
    }
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_observer_get(
        self->world,
        (ds_observer_id)observer_id,
        &observer
    );
    if (result != DS_OK) {
        dense_raise_result(result, "observer failed");
        return NULL;
    }
    return Py_BuildValue(
        "{s:K,s:i,s:i,s:i,s:K,s:K,s:O,s:O}",
        "id",
        (unsigned long long)observer.id,
        "spatial_x",
        observer.spatial_x,
        "spatial_y",
        observer.spatial_y,
        "radius",
        observer.radius,
        "type_mask",
        (unsigned long long)observer.type_mask,
        "anchor_entity_id",
        (unsigned long long)observer.anchor_entity_id,
        "positioned",
        observer.positioned ? Py_True : Py_False,
        "anchored",
        observer.anchored ? Py_True : Py_False
    );
}

static PyObject *DenseWorld_get_tick_is_open(DenseWorld *self, void *closure)
{
    (void)closure;
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyBool_FromLong(ds_world_tick_is_open(self->world) ? 1L : 0L);
}

static PyObject *DenseWorld_get_current_tick(DenseWorld *self, void *closure)
{
    (void)closure;
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyLong_FromUnsignedLongLong(
        (unsigned long long)ds_world_current_tick(self->world)
    );
}

static PyObject *DenseWorld_get_entity_count(DenseWorld *self, void *closure)
{
    (void)closure;
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyLong_FromSize_t(ds_world_entity_count(self->world));
}

static PyObject *DenseWorld_get_entity_capacity(DenseWorld *self, void *closure)
{
    (void)closure;
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyLong_FromSize_t(ds_world_entity_capacity(self->world));
}

static PyObject *DenseWorld_get_motion_metrics(DenseWorld *self, void *closure)
{
    ds_motion_metrics metrics;
    ds_result result;

    (void)closure;
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    result = ds_world_get_motion_metrics(self->world, &metrics);
    if (result != DS_OK) {
        dense_raise_result(result, "motion_metrics failed");
        return NULL;
    }

    return Py_BuildValue(
        "{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K}",
        "scheduled_events",
        (unsigned long long)metrics.scheduled_events,
        "processed_events",
        (unsigned long long)metrics.processed_events,
        "stale_events",
        (unsigned long long)metrics.stale_events,
        "cell_crossings",
        (unsigned long long)metrics.cell_crossings,
        "expiries",
        (unsigned long long)metrics.expiries,
        "plan_replacements",
        (unsigned long long)metrics.plan_replacements,
        "sampled_demotions",
        (unsigned long long)metrics.sampled_demotions,
        "correction_steps",
        (unsigned long long)metrics.correction_steps
    );
}

static PyObject *DenseWorld_get_observer_count(DenseWorld *self, void *closure)
{
    (void)closure;
    if (dense_world_require_open(self) < 0) {
        return NULL;
    }
    return PyLong_FromSize_t(ds_world_observer_count(self->world));
}

static PyMethodDef DenseWorld_methods[] = {
    {"close", (PyCFunction)DenseWorld_close, METH_NOARGS, "Destroy the native world."},
    {"begin_tick", (PyCFunction)DenseWorld_begin_tick, METH_VARARGS, "Begin a monotonically increasing tick."},
    {"end_tick", (PyCFunction)DenseWorld_end_tick, METH_NOARGS, "Finalize subscriptions and fanout for the current tick."},
    {"fanout_view", (PyCFunction)DenseWorld_fanout_view, METH_NOARGS, "Return a borrowed read-only finalized fanout view."},
    {"spawn", (PyCFunction)DenseWorld_spawn, METH_VARARGS, "Spawn one spatial entity."},
    {"move", (PyCFunction)DenseWorld_move, METH_VARARGS, "Move one entity."},
    {"move_many", (PyCFunction)DenseWorld_move_many, METH_VARARGS, "Move entities from native uint64/int32 buffers in one Python-to-C call."},
    {"set_motion_plan", (PyCFunction)DenseWorld_set_motion_plan, METH_VARARGS, "Assign a stable linear kinetic motion plan."},
    {"clear_motion_plan", (PyCFunction)DenseWorld_clear_motion_plan, METH_VARARGS, "Demote an entity from kinetic motion to sampled motion."},
    {"motion_mode", (PyCFunction)DenseWorld_motion_mode, METH_VARARGS, "Return the entity motion mode."},
    {"remove", (PyCFunction)DenseWorld_remove, METH_VARARGS, "Remove one entity."},
    {"mark_dirty", (PyCFunction)DenseWorld_mark_dirty, METH_VARARGS, "Mark one entity's application-defined channels dirty."},
    {"mark_dirty_many", (PyCFunction)DenseWorld_mark_dirty_many, METH_VARARGS, "Mark many entities dirty from a uint64 ID buffer and scalar or uint64 mask buffer."},
    {"entity_exists", (PyCFunction)DenseWorld_entity_exists, METH_VARARGS, "Return whether an entity exists."},
    {"entity", (PyCFunction)DenseWorld_entity, METH_VARARGS, "Return one entity description."},
    {"create_observer", (PyCFunction)DenseWorld_create_observer, METH_VARARGS, "Create an observer and return its ID."},
    {"destroy_observer", (PyCFunction)DenseWorld_destroy_observer, METH_VARARGS, "Destroy an observer."},
    {"anchor_observer", (PyCFunction)DenseWorld_anchor_observer, METH_VARARGS, "Anchor an observer to an entity."},
    {"set_observer_position", (PyCFunction)DenseWorld_set_observer_position, METH_VARARGS, "Move an independent observer."},
    {"set_observer_radius", (PyCFunction)DenseWorld_set_observer_radius, METH_VARARGS, "Set an observer radius."},
    {"observer_exists", (PyCFunction)DenseWorld_observer_exists, METH_VARARGS, "Return whether an observer exists."},
    {"observer", (PyCFunction)DenseWorld_observer, METH_VARARGS, "Return one observer description."},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef DenseWorld_getset[] = {
    {"tick_is_open", (getter)DenseWorld_get_tick_is_open, NULL, "Whether a tick is open.", NULL},
    {"current_tick", (getter)DenseWorld_get_current_tick, NULL, "Current tick or zero before the first tick.", NULL},
    {"entity_count", (getter)DenseWorld_get_entity_count, NULL, "Active entity count.", NULL},
    {"entity_capacity", (getter)DenseWorld_get_entity_capacity, NULL, "Reserved entity capacity.", NULL},
    {"observer_count", (getter)DenseWorld_get_observer_count, NULL, "Active observer count.", NULL},
    {"motion_metrics", (getter)DenseWorld_get_motion_metrics, NULL, "Cumulative kinetic motion metrics.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

static void DenseFanoutView_dealloc(DenseFanoutView *self)
{
    Py_XDECREF(self->world);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static Py_ssize_t DenseFanoutView_length(PyObject *object)
{
    DenseFanoutView *self = (DenseFanoutView *)object;

    if (dense_fanout_require_valid(self) < 0) {
        return -1;
    }
    if (self->view.chunk_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "fanout group count exceeds Python sequence limits");
        return -1;
    }
    return (Py_ssize_t)self->view.chunk_count;
}

static PyObject *DenseFanoutView_item(PyObject *object, Py_ssize_t index)
{
    DenseFanoutView *self = (DenseFanoutView *)object;
    DenseChunkDeltaView *chunk;
    Py_ssize_t length;

    if (dense_fanout_require_valid(self) < 0) {
        return NULL;
    }
    if (self->view.chunk_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "fanout group count exceeds Python sequence limits");
        return NULL;
    }
    length = (Py_ssize_t)self->view.chunk_count;
    if (dense_normalize_index(&index, length) < 0) {
        return NULL;
    }

    chunk = PyObject_New(DenseChunkDeltaView, &DenseChunkDeltaViewType);
    if (chunk == NULL) {
        return NULL;
    }
    Py_INCREF(self);
    chunk->fanout = self;
    chunk->index = index;
    return (PyObject *)chunk;
}

static PyObject *DenseFanoutView_get_tick(DenseFanoutView *self, void *closure)
{
    (void)closure;
    if (dense_fanout_require_valid(self) < 0) {
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->view.tick);
}

static PyObject *DenseFanoutView_get_chunk_count(DenseFanoutView *self, void *closure)
{
    (void)closure;
    if (dense_fanout_require_valid(self) < 0) {
        return NULL;
    }
    return PyLong_FromSize_t(self->view.chunk_count);
}

static PyObject *DenseFanoutView_get_delta_count(DenseFanoutView *self, void *closure)
{
    (void)closure;
    if (dense_fanout_require_valid(self) < 0) {
        return NULL;
    }
    return PyLong_FromSize_t(self->view.delta_count);
}

static PyObject *DenseFanoutView_get_subscriber_count(DenseFanoutView *self, void *closure)
{
    (void)closure;
    if (dense_fanout_require_valid(self) < 0) {
        return NULL;
    }
    return PyLong_FromSize_t(self->view.subscriber_count);
}

static PyObject *DenseFanoutView_get_groups(DenseFanoutView *self, void *closure)
{
    (void)closure;
    if (dense_fanout_require_valid(self) < 0) {
        return NULL;
    }
    return Py_NewRef(self);
}

static PyObject *DenseFanoutView_repr(DenseFanoutView *self)
{
    if (dense_fanout_require_valid(self) < 0) {
        PyErr_Clear();
        return PyUnicode_FromString("<dense_sim.FanoutView invalidated>");
    }
    return PyUnicode_FromFormat(
        "<dense_sim.FanoutView tick=%llu groups=%zu deltas=%zu subscribers=%zu>",
        (unsigned long long)self->view.tick,
        self->view.chunk_count,
        self->view.delta_count,
        self->view.subscriber_count
    );
}

static PySequenceMethods DenseFanoutView_sequence = {
    .sq_length = DenseFanoutView_length,
    .sq_item = DenseFanoutView_item,
};

static PyGetSetDef DenseFanoutView_getset[] = {
    {"tick", (getter)DenseFanoutView_get_tick, NULL, "Finalized tick.", NULL},
    {"chunk_count", (getter)DenseFanoutView_get_chunk_count, NULL, "Exact fanout group count.", NULL},
    {"delta_count", (getter)DenseFanoutView_get_delta_count, NULL, "Total delta entries.", NULL},
    {"subscriber_count", (getter)DenseFanoutView_get_subscriber_count, NULL, "Total subscriber references.", NULL},
    {"groups", (getter)DenseFanoutView_get_groups, NULL, "Read-only sequence of exact fanout groups.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

static void DenseChunkDeltaView_dealloc(DenseChunkDeltaView *self)
{
    Py_XDECREF(self->fanout);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *DenseChunkDeltaView_get_chunk_x(DenseChunkDeltaView *self, void *closure)
{
    const ds_chunk_delta *chunk;

    (void)closure;
    chunk = dense_chunk_at(self->fanout, self->index);
    if (chunk == NULL) {
        return NULL;
    }
    return PyLong_FromLong((long)chunk->chunk_x);
}

static PyObject *DenseChunkDeltaView_get_chunk_y(DenseChunkDeltaView *self, void *closure)
{
    const ds_chunk_delta *chunk;

    (void)closure;
    chunk = dense_chunk_at(self->fanout, self->index);
    if (chunk == NULL) {
        return NULL;
    }
    return PyLong_FromLong((long)chunk->chunk_y);
}

static PyObject *DenseChunkDeltaView_get_entry_count(DenseChunkDeltaView *self, void *closure)
{
    const ds_chunk_delta *chunk;

    (void)closure;
    chunk = dense_chunk_at(self->fanout, self->index);
    if (chunk == NULL) {
        return NULL;
    }
    return PyLong_FromSize_t(chunk->entry_count);
}

static PyObject *DenseChunkDeltaView_get_subscriber_count(DenseChunkDeltaView *self, void *closure)
{
    const ds_chunk_delta *chunk;

    (void)closure;
    chunk = dense_chunk_at(self->fanout, self->index);
    if (chunk == NULL) {
        return NULL;
    }
    return PyLong_FromSize_t(chunk->subscriber_count);
}

static PyObject *DenseChunkDeltaView_get_entries(DenseChunkDeltaView *self, void *closure)
{
    DenseEntrySequence *sequence;

    (void)closure;
    if (dense_chunk_at(self->fanout, self->index) == NULL) {
        return NULL;
    }
    sequence = PyObject_New(DenseEntrySequence, &DenseEntrySequenceType);
    if (sequence == NULL) {
        return NULL;
    }
    Py_INCREF(self->fanout);
    sequence->fanout = self->fanout;
    sequence->chunk_index = self->index;
    return (PyObject *)sequence;
}

static PyObject *DenseChunkDeltaView_get_subscribers(DenseChunkDeltaView *self, void *closure)
{
    DenseSubscriberSequence *sequence;

    (void)closure;
    if (dense_chunk_at(self->fanout, self->index) == NULL) {
        return NULL;
    }
    sequence = PyObject_New(DenseSubscriberSequence, &DenseSubscriberSequenceType);
    if (sequence == NULL) {
        return NULL;
    }
    Py_INCREF(self->fanout);
    sequence->fanout = self->fanout;
    sequence->chunk_index = self->index;
    return (PyObject *)sequence;
}

static PyObject *DenseChunkDeltaView_repr(DenseChunkDeltaView *self)
{
    const ds_chunk_delta *chunk = dense_chunk_at(self->fanout, self->index);

    if (chunk == NULL) {
        PyErr_Clear();
        return PyUnicode_FromString("<dense_sim.ChunkDeltaView invalidated>");
    }
    return PyUnicode_FromFormat(
        "<dense_sim.ChunkDeltaView chunk=(%d, %d) entries=%zu subscribers=%zu>",
        chunk->chunk_x,
        chunk->chunk_y,
        chunk->entry_count,
        chunk->subscriber_count
    );
}

static PyGetSetDef DenseChunkDeltaView_getset[] = {
    {"chunk_x", (getter)DenseChunkDeltaView_get_chunk_x, NULL, "Chunk x coordinate.", NULL},
    {"chunk_y", (getter)DenseChunkDeltaView_get_chunk_y, NULL, "Chunk y coordinate.", NULL},
    {"entry_count", (getter)DenseChunkDeltaView_get_entry_count, NULL, "Delta entry count.", NULL},
    {"subscriber_count", (getter)DenseChunkDeltaView_get_subscriber_count, NULL, "Subscriber count.", NULL},
    {"entries", (getter)DenseChunkDeltaView_get_entries, NULL, "Borrowed read-only delta-entry sequence.", NULL},
    {"subscribers", (getter)DenseChunkDeltaView_get_subscribers, NULL, "Borrowed read-only observer-ID sequence.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

static void DenseEntrySequence_dealloc(DenseEntrySequence *self)
{
    Py_XDECREF(self->fanout);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static Py_ssize_t DenseEntrySequence_length(PyObject *object)
{
    DenseEntrySequence *self = (DenseEntrySequence *)object;
    const ds_chunk_delta *chunk = dense_chunk_at(
        self->fanout,
        self->chunk_index
    );

    if (chunk == NULL) {
        return -1;
    }
    if (chunk->entry_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "delta entry count exceeds Python sequence limits");
        return -1;
    }
    return (Py_ssize_t)chunk->entry_count;
}

static PyObject *DenseEntrySequence_item(PyObject *object, Py_ssize_t index)
{
    DenseEntrySequence *self = (DenseEntrySequence *)object;
    const ds_chunk_delta *chunk = dense_chunk_at(self->fanout, self->chunk_index);
    DenseDeltaEntry *entry;
    Py_ssize_t length;

    if (chunk == NULL) {
        return NULL;
    }
    if (chunk->entry_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "delta entry count exceeds Python sequence limits");
        return NULL;
    }
    length = (Py_ssize_t)chunk->entry_count;
    if (dense_normalize_index(&index, length) < 0) {
        return NULL;
    }

    entry = PyObject_New(DenseDeltaEntry, &DenseDeltaEntryType);
    if (entry == NULL) {
        return NULL;
    }
    entry->entity_id = chunk->entries[(size_t)index].entity_id;
    entry->channel_mask = chunk->entries[(size_t)index].channel_mask;
    entry->operation = chunk->entries[(size_t)index].operation;
    return (PyObject *)entry;
}

static PySequenceMethods DenseEntrySequence_sequence = {
    .sq_length = DenseEntrySequence_length,
    .sq_item = DenseEntrySequence_item,
};

static PyObject *DenseEntrySequence_repr(DenseEntrySequence *self)
{
    Py_ssize_t length = DenseEntrySequence_length((PyObject *)self);

    if (length < 0) {
        PyErr_Clear();
        return PyUnicode_FromString("<dense_sim.DeltaEntryView invalidated>");
    }
    return PyUnicode_FromFormat("<dense_sim.DeltaEntryView length=%zd>", length);
}

static void DenseSubscriberSequence_dealloc(DenseSubscriberSequence *self)
{
    Py_XDECREF(self->fanout);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static Py_ssize_t DenseSubscriberSequence_length(PyObject *object)
{
    DenseSubscriberSequence *self = (DenseSubscriberSequence *)object;
    const ds_chunk_delta *chunk = dense_chunk_at(
        self->fanout,
        self->chunk_index
    );

    if (chunk == NULL) {
        return -1;
    }
    if (chunk->subscriber_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "subscriber count exceeds Python sequence limits");
        return -1;
    }
    return (Py_ssize_t)chunk->subscriber_count;
}

static PyObject *DenseSubscriberSequence_item(PyObject *object, Py_ssize_t index)
{
    DenseSubscriberSequence *self = (DenseSubscriberSequence *)object;
    const ds_chunk_delta *chunk = dense_chunk_at(self->fanout, self->chunk_index);
    Py_ssize_t length;

    if (chunk == NULL) {
        return NULL;
    }
    if (chunk->subscriber_count > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "subscriber count exceeds Python sequence limits");
        return NULL;
    }
    length = (Py_ssize_t)chunk->subscriber_count;
    if (dense_normalize_index(&index, length) < 0) {
        return NULL;
    }
    return PyLong_FromUnsignedLongLong(
        (unsigned long long)chunk->subscribers[(size_t)index]
    );
}

static PySequenceMethods DenseSubscriberSequence_sequence = {
    .sq_length = DenseSubscriberSequence_length,
    .sq_item = DenseSubscriberSequence_item,
};

static PyObject *DenseSubscriberSequence_repr(DenseSubscriberSequence *self)
{
    Py_ssize_t length = DenseSubscriberSequence_length((PyObject *)self);

    if (length < 0) {
        PyErr_Clear();
        return PyUnicode_FromString("<dense_sim.SubscriberView invalidated>");
    }
    return PyUnicode_FromFormat("<dense_sim.SubscriberView length=%zd>", length);
}

static PyObject *DenseDeltaEntry_get_entity_id(DenseDeltaEntry *self, void *closure)
{
    (void)closure;
    return PyLong_FromUnsignedLongLong((unsigned long long)self->entity_id);
}

static PyObject *DenseDeltaEntry_get_channel_mask(DenseDeltaEntry *self, void *closure)
{
    (void)closure;
    return PyLong_FromUnsignedLongLong((unsigned long long)self->channel_mask);
}

static PyObject *DenseDeltaEntry_get_operation(DenseDeltaEntry *self, void *closure)
{
    (void)closure;
    return PyLong_FromLong((long)self->operation);
}

static PyObject *DenseDeltaEntry_get_operation_name(DenseDeltaEntry *self, void *closure)
{
    (void)closure;
    return PyUnicode_FromString(dense_delta_operation_name(self->operation));
}

static PyObject *DenseDeltaEntry_repr(DenseDeltaEntry *self)
{
    return PyUnicode_FromFormat(
        "<dense_sim.DeltaEntry entity_id=%llu channel_mask=%llu operation=%s>",
        (unsigned long long)self->entity_id,
        (unsigned long long)self->channel_mask,
        dense_delta_operation_name(self->operation)
    );
}

static PyGetSetDef DenseDeltaEntry_getset[] = {
    {"entity_id", (getter)DenseDeltaEntry_get_entity_id, NULL, "Entity ID.", NULL},
    {"channel_mask", (getter)DenseDeltaEntry_get_channel_mask, NULL, "Dirty channel mask.", NULL},
    {"operation", (getter)DenseDeltaEntry_get_operation, NULL, "Delta operation integer.", NULL},
    {"operation_name", (getter)DenseDeltaEntry_get_operation_name, NULL, "Delta operation name.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

static PyTypeObject DenseWorldType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dense_sim.World",
    .tp_basicsize = sizeof(DenseWorld),
    .tp_dealloc = (destructor)DenseWorld_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Single-writer DenseSim spatial world.",
    .tp_methods = DenseWorld_methods,
    .tp_getset = DenseWorld_getset,
    .tp_init = (initproc)DenseWorld_init,
    .tp_new = DenseWorld_new,
};

static PyTypeObject DenseFanoutViewType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dense_sim.FanoutView",
    .tp_basicsize = sizeof(DenseFanoutView),
    .tp_dealloc = (destructor)DenseFanoutView_dealloc,
    .tp_repr = (reprfunc)DenseFanoutView_repr,
    .tp_as_sequence = &DenseFanoutView_sequence,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Borrowed read-only finalized fanout view.",
    .tp_getset = DenseFanoutView_getset,
};

static PyTypeObject DenseChunkDeltaViewType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dense_sim.ChunkDeltaView",
    .tp_basicsize = sizeof(DenseChunkDeltaView),
    .tp_dealloc = (destructor)DenseChunkDeltaView_dealloc,
    .tp_repr = (reprfunc)DenseChunkDeltaView_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Borrowed read-only exact fanout group.",
    .tp_getset = DenseChunkDeltaView_getset,
};

static PyTypeObject DenseEntrySequenceType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dense_sim.DeltaEntryView",
    .tp_basicsize = sizeof(DenseEntrySequence),
    .tp_dealloc = (destructor)DenseEntrySequence_dealloc,
    .tp_repr = (reprfunc)DenseEntrySequence_repr,
    .tp_as_sequence = &DenseEntrySequence_sequence,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Borrowed read-only sequence of delta entries.",
};

static PyTypeObject DenseSubscriberSequenceType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dense_sim.SubscriberView",
    .tp_basicsize = sizeof(DenseSubscriberSequence),
    .tp_dealloc = (destructor)DenseSubscriberSequence_dealloc,
    .tp_repr = (reprfunc)DenseSubscriberSequence_repr,
    .tp_as_sequence = &DenseSubscriberSequence_sequence,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Borrowed read-only sequence of observer IDs.",
};

static PyTypeObject DenseDeltaEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dense_sim.DeltaEntry",
    .tp_basicsize = sizeof(DenseDeltaEntry),
    .tp_repr = (reprfunc)DenseDeltaEntry_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Immutable scalar copy of one delta entry.",
    .tp_getset = DenseDeltaEntry_getset,
};

static int dense_add_int_constant(
    PyObject *module,
    const char *name,
    unsigned long long value
)
{
    PyObject *object = PyLong_FromUnsignedLongLong(value);

    if (object == NULL) {
        return -1;
    }
    if (PyModule_AddObject(module, name, object) < 0) {
        Py_DECREF(object);
        return -1;
    }
    return 0;
}

static struct PyModuleDef dense_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_dense_sim",
    .m_doc = "CPython binding for libdense_sim.",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__dense_sim(void)
{
    PyObject *module;

    if (
        PyType_Ready(&DenseWorldType) < 0 ||
        PyType_Ready(&DenseFanoutViewType) < 0 ||
        PyType_Ready(&DenseChunkDeltaViewType) < 0 ||
        PyType_Ready(&DenseEntrySequenceType) < 0 ||
        PyType_Ready(&DenseSubscriberSequenceType) < 0 ||
        PyType_Ready(&DenseDeltaEntryType) < 0
    ) {
        return NULL;
    }

    module = PyModule_Create(&dense_module);
    if (module == NULL) {
        return NULL;
    }

    DenseSimError = PyErr_NewException(
        "dense_sim.DenseSimError",
        PyExc_RuntimeError,
        NULL
    );
    if (DenseSimError == NULL) {
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(DenseSimError);
    if (PyModule_AddObject(module, "DenseSimError", DenseSimError) < 0) {
        Py_DECREF(DenseSimError);
        Py_DECREF(module);
        return NULL;
    }

    Py_INCREF(&DenseWorldType);
    if (PyModule_AddObject(module, "World", (PyObject *)&DenseWorldType) < 0) {
        Py_DECREF(&DenseWorldType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&DenseFanoutViewType);
    if (PyModule_AddObject(module, "FanoutView", (PyObject *)&DenseFanoutViewType) < 0) {
        Py_DECREF(&DenseFanoutViewType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&DenseChunkDeltaViewType);
    if (PyModule_AddObject(module, "ChunkDeltaView", (PyObject *)&DenseChunkDeltaViewType) < 0) {
        Py_DECREF(&DenseChunkDeltaViewType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&DenseDeltaEntryType);
    if (PyModule_AddObject(module, "DeltaEntry", (PyObject *)&DenseDeltaEntryType) < 0) {
        Py_DECREF(&DenseDeltaEntryType);
        Py_DECREF(module);
        return NULL;
    }

    if (
        dense_add_int_constant(module, "CHANNEL_POSITION", DS_CHANNEL_POSITION) < 0 ||
        dense_add_int_constant(module, "CHANNEL_VITALS", DS_CHANNEL_VITALS) < 0 ||
        dense_add_int_constant(module, "CHANNEL_ANIMATION", DS_CHANNEL_ANIMATION) < 0 ||
        dense_add_int_constant(module, "CHANNEL_APPEARANCE", DS_CHANNEL_APPEARANCE) < 0 ||
        dense_add_int_constant(module, "CHANNEL_CUSTOM_0", DS_CHANNEL_CUSTOM_0) < 0 ||
        dense_add_int_constant(module, "DELTA_UPDATE", DS_DELTA_UPDATE) < 0 ||
        dense_add_int_constant(module, "DELTA_ENTER", DS_DELTA_ENTER) < 0 ||
        dense_add_int_constant(module, "DELTA_LEAVE", DS_DELTA_LEAVE) < 0 ||
        dense_add_int_constant(module, "DELTA_SPAWN", DS_DELTA_SPAWN) < 0 ||
        dense_add_int_constant(module, "DELTA_REMOVE", DS_DELTA_REMOVE) < 0 ||
        dense_add_int_constant(module, "MOTION_SAMPLED", DS_MOTION_SAMPLED) < 0 ||
        dense_add_int_constant(module, "MOTION_KINETIC", DS_MOTION_KINETIC) < 0
    ) {
        Py_DECREF(module);
        return NULL;
    }

    return module;
}
