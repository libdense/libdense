#![deny(unsafe_op_in_unsafe_fn)]

use std::cell::Cell;
use std::error::Error as StdError;
use std::ffi::CStr;
use std::fmt;
use std::marker::PhantomData;
use std::ptr::NonNull;
use std::slice;

pub type EntityId = u64;
pub type ObserverId = u64;
pub type ChannelMask = u64;
pub type TypeMask = u64;
pub type Tick = u64;

pub const CHANNEL_POSITION: ChannelMask = 1_u64 << 0;
pub const CHANNEL_VITALS: ChannelMask = 1_u64 << 1;
pub const CHANNEL_ANIMATION: ChannelMask = 1_u64 << 2;
pub const CHANNEL_APPEARANCE: ChannelMask = 1_u64 << 3;
pub const CHANNEL_CUSTOM_0: ChannelMask = 1_u64 << 16;

mod ffi {
    use std::ffi::{c_char, c_int};

    #[repr(C)]
    pub struct DsWorld {
        _private: [u8; 0],
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsWorldConfig {
        pub cell_size: i32,
        pub chunk_size: i32,
        pub initial_entity_capacity: usize,
        pub initial_observer_capacity: usize,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsObserverConfig {
        pub radius: i32,
        pub type_mask: u64,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsObserverDesc {
        pub id: u64,
        pub spatial_x: i32,
        pub spatial_y: i32,
        pub radius: i32,
        pub type_mask: u64,
        pub anchor_entity_id: u64,
        pub positioned: bool,
        pub anchored: bool,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsEntityDesc {
        pub id: u64,
        pub spatial_x: i32,
        pub spatial_y: i32,
        pub type_mask: u64,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsMotionPlan {
        pub start_tick: u64,
        pub until_tick: u64,
        pub x: f64,
        pub y: f64,
        pub vx: f64,
        pub vy: f64,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsMotionMetrics {
        pub scheduled_events: u64,
        pub processed_events: u64,
        pub stale_events: u64,
        pub cell_crossings: u64,
        pub expiries: u64,
        pub plan_replacements: u64,
        pub sampled_demotions: u64,
        pub correction_steps: u64,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsDeltaEntry {
        pub entity_id: u64,
        pub channel_mask: u64,
        pub operation: c_int,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsChunkDelta {
        pub chunk_x: i32,
        pub chunk_y: i32,
        pub entries: *const DsDeltaEntry,
        pub entry_count: usize,
        pub subscribers: *const u64,
        pub subscriber_count: usize,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct DsFanoutView {
        pub tick: u64,
        pub chunks: *const DsChunkDelta,
        pub chunk_count: usize,
        pub delta_count: usize,
        pub subscriber_count: usize,
    }

    extern "C" {
        pub fn ds_result_string(result: c_int) -> *const c_char;

        pub fn ds_world_create(
            config: *const DsWorldConfig,
            out_world: *mut *mut DsWorld,
        ) -> c_int;
        pub fn ds_world_destroy(world: *mut DsWorld);
        pub fn ds_world_begin_tick(world: *mut DsWorld, tick: u64) -> c_int;
        pub fn ds_world_end_tick(world: *mut DsWorld) -> c_int;
        pub fn ds_world_tick_is_open(world: *const DsWorld) -> bool;
        pub fn ds_world_current_tick(world: *const DsWorld) -> u64;
        pub fn ds_world_entity_count(world: *const DsWorld) -> usize;
        pub fn ds_world_entity_capacity(world: *const DsWorld) -> usize;
        pub fn ds_world_observer_count(world: *const DsWorld) -> usize;
        pub fn ds_world_get_motion_metrics(
            world: *const DsWorld,
            out_metrics: *mut DsMotionMetrics,
        ) -> c_int;
        pub fn ds_world_get_fanout_view(
            world: *const DsWorld,
            out_view: *mut DsFanoutView,
        ) -> c_int;

        pub fn ds_entity_spawn(
            world: *mut DsWorld,
            entity_id: u64,
            spatial_x: i32,
            spatial_y: i32,
            type_mask: u64,
        ) -> c_int;
        pub fn ds_entity_move(
            world: *mut DsWorld,
            entity_id: u64,
            spatial_x: i32,
            spatial_y: i32,
        ) -> c_int;
        pub fn ds_entity_remove(world: *mut DsWorld, entity_id: u64) -> c_int;
        pub fn ds_entity_mark_dirty(
            world: *mut DsWorld,
            entity_id: u64,
            channel_mask: u64,
        ) -> c_int;
        pub fn ds_entity_set_motion_plan(
            world: *mut DsWorld,
            entity_id: u64,
            plan: *const DsMotionPlan,
        ) -> c_int;
        pub fn ds_entity_clear_motion_plan(
            world: *mut DsWorld,
            entity_id: u64,
        ) -> c_int;
        pub fn ds_entity_motion_mode(
            world: *const DsWorld,
            entity_id: u64,
            out_mode: *mut c_int,
        ) -> c_int;
        pub fn ds_entity_exists(world: *const DsWorld, entity_id: u64) -> bool;
        pub fn ds_entity_get(
            world: *const DsWorld,
            entity_id: u64,
            out_entity: *mut DsEntityDesc,
        ) -> c_int;

        pub fn ds_observer_create(
            world: *mut DsWorld,
            config: *const DsObserverConfig,
            out_observer_id: *mut u64,
        ) -> c_int;
        pub fn ds_observer_destroy(world: *mut DsWorld, observer_id: u64) -> c_int;
        pub fn ds_observer_anchor_entity(
            world: *mut DsWorld,
            observer_id: u64,
            entity_id: u64,
        ) -> c_int;
        pub fn ds_observer_set_position(
            world: *mut DsWorld,
            observer_id: u64,
            spatial_x: i32,
            spatial_y: i32,
        ) -> c_int;
        pub fn ds_observer_set_radius(
            world: *mut DsWorld,
            observer_id: u64,
            radius: i32,
        ) -> c_int;
        pub fn ds_observer_exists(world: *const DsWorld, observer_id: u64) -> bool;
        pub fn ds_observer_get(
            world: *const DsWorld,
            observer_id: u64,
            out_observer: *mut DsObserverDesc,
        ) -> c_int;
    }
}

const DS_OK: i32 = 0;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(i32)]
pub enum ResultCode {
    InvalidArgument = 1,
    OutOfMemory = 2,
    Capacity = 3,
    EntityExists = 4,
    EntityNotFound = 5,
    ObserverNotFound = 6,
    TickAlreadyOpen = 7,
    TickNotOpen = 8,
    TickOrder = 9,
    TickNotFinalized = 10,
    MotionConflict = 11,
}

impl ResultCode {
    fn from_raw(value: i32) -> Option<Self> {
        match value {
            1 => Some(Self::InvalidArgument),
            2 => Some(Self::OutOfMemory),
            3 => Some(Self::Capacity),
            4 => Some(Self::EntityExists),
            5 => Some(Self::EntityNotFound),
            6 => Some(Self::ObserverNotFound),
            7 => Some(Self::TickAlreadyOpen),
            8 => Some(Self::TickNotOpen),
            9 => Some(Self::TickOrder),
            10 => Some(Self::TickNotFinalized),
            11 => Some(Self::MotionConflict),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Error {
    raw_code: i32,
    code: Option<ResultCode>,
    context: &'static str,
    message: String,
}

impl Error {
    pub fn raw_code(&self) -> i32 {
        self.raw_code
    }

    pub fn code(&self) -> Option<ResultCode> {
        self.code
    }

    pub fn context(&self) -> &'static str {
        self.context
    }
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "{}: {}", self.context, self.message)
    }
}

impl StdError for Error {}

pub type Result<T> = std::result::Result<T, Error>;

fn check(raw_code: i32, context: &'static str) -> Result<()> {
    if raw_code == DS_OK {
        return Ok(());
    }

    let message = unsafe {
        let pointer = ffi::ds_result_string(raw_code);
        if pointer.is_null() {
            format!("unknown dense_sim result {}", raw_code)
        } else {
            CStr::from_ptr(pointer).to_string_lossy().into_owned()
        }
    };

    Err(Error {
        raw_code,
        code: ResultCode::from_raw(raw_code),
        context,
        message,
    })
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct WorldConfig {
    pub cell_size: i32,
    pub chunk_size: i32,
    pub initial_entity_capacity: usize,
    pub initial_observer_capacity: usize,
}

impl Default for WorldConfig {
    fn default() -> Self {
        Self {
            cell_size: 8,
            chunk_size: 16,
            initial_entity_capacity: 1024,
            initial_observer_capacity: 64,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ObserverConfig {
    pub radius: i32,
    pub type_mask: TypeMask,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Entity {
    pub id: EntityId,
    pub spatial_x: i32,
    pub spatial_y: i32,
    pub type_mask: TypeMask,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Observer {
    pub id: ObserverId,
    pub spatial_x: i32,
    pub spatial_y: i32,
    pub radius: i32,
    pub type_mask: TypeMask,
    pub anchor_entity_id: EntityId,
    pub positioned: bool,
    pub anchored: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(i32)]
pub enum MotionMode {
    Sampled = 0,
    Kinetic = 1,
}

impl MotionMode {
    fn from_raw(value: i32) -> Self {
        match value {
            0 => Self::Sampled,
            1 => Self::Kinetic,
            _ => panic!("libdense_sim returned invalid motion mode {}", value),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MotionPlan {
    pub start_tick: Tick,
    pub until_tick: Tick,
    pub x: f64,
    pub y: f64,
    pub vx: f64,
    pub vy: f64,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct MotionMetrics {
    pub scheduled_events: u64,
    pub processed_events: u64,
    pub stale_events: u64,
    pub cell_crossings: u64,
    pub expiries: u64,
    pub plan_replacements: u64,
    pub sampled_demotions: u64,
    pub correction_steps: u64,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(i32)]
pub enum DeltaOp {
    Update = 0,
    Enter = 1,
    Leave = 2,
    Spawn = 3,
    Remove = 4,
}

impl DeltaOp {
    fn from_raw(value: i32) -> Self {
        match value {
            0 => Self::Update,
            1 => Self::Enter,
            2 => Self::Leave,
            3 => Self::Spawn,
            4 => Self::Remove,
            _ => panic!("libdense_sim returned invalid delta operation {}", value),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DeltaEntry {
    pub entity_id: EntityId,
    pub channel_mask: ChannelMask,
    pub operation: DeltaOp,
}

pub struct World {
    raw: NonNull<ffi::DsWorld>,
    _not_sync: PhantomData<Cell<()>>,
}

unsafe impl Send for World {}

impl World {
    pub fn new(config: WorldConfig) -> Result<Self> {
        let raw_config = ffi::DsWorldConfig {
            cell_size: config.cell_size,
            chunk_size: config.chunk_size,
            initial_entity_capacity: config.initial_entity_capacity,
            initial_observer_capacity: config.initial_observer_capacity,
        };
        let mut raw_world = std::ptr::null_mut();
        let code = unsafe { ffi::ds_world_create(&raw_config, &mut raw_world) };
        check(code, "ds_world_create")?;
        let raw = NonNull::new(raw_world).expect("DS_OK returned a null ds_world");

        Ok(Self {
            raw,
            _not_sync: PhantomData,
        })
    }

    pub fn begin_tick(&mut self, tick: Tick) -> Result<()> {
        let code = unsafe { ffi::ds_world_begin_tick(self.raw.as_ptr(), tick) };
        check(code, "ds_world_begin_tick")
    }

    pub fn end_tick(&mut self) -> Result<()> {
        let code = unsafe { ffi::ds_world_end_tick(self.raw.as_ptr()) };
        check(code, "ds_world_end_tick")
    }

    pub fn tick_is_open(&self) -> bool {
        unsafe { ffi::ds_world_tick_is_open(self.raw.as_ptr()) }
    }

    pub fn current_tick(&self) -> Tick {
        unsafe { ffi::ds_world_current_tick(self.raw.as_ptr()) }
    }

    pub fn entity_count(&self) -> usize {
        unsafe { ffi::ds_world_entity_count(self.raw.as_ptr()) }
    }

    pub fn entity_capacity(&self) -> usize {
        unsafe { ffi::ds_world_entity_capacity(self.raw.as_ptr()) }
    }

    pub fn observer_count(&self) -> usize {
        unsafe { ffi::ds_world_observer_count(self.raw.as_ptr()) }
    }

    pub fn motion_metrics(&self) -> Result<MotionMetrics> {
        let mut metrics = ffi::DsMotionMetrics {
            scheduled_events: 0,
            processed_events: 0,
            stale_events: 0,
            cell_crossings: 0,
            expiries: 0,
            plan_replacements: 0,
            sampled_demotions: 0,
            correction_steps: 0,
        };
        let code = unsafe {
            ffi::ds_world_get_motion_metrics(self.raw.as_ptr(), &mut metrics)
        };
        check(code, "ds_world_get_motion_metrics")?;

        Ok(MotionMetrics {
            scheduled_events: metrics.scheduled_events,
            processed_events: metrics.processed_events,
            stale_events: metrics.stale_events,
            cell_crossings: metrics.cell_crossings,
            expiries: metrics.expiries,
            plan_replacements: metrics.plan_replacements,
            sampled_demotions: metrics.sampled_demotions,
            correction_steps: metrics.correction_steps,
        })
    }

    pub fn fanout_view(&self) -> Result<FanoutView<'_>> {
        let mut view = ffi::DsFanoutView {
            tick: 0,
            chunks: std::ptr::null(),
            chunk_count: 0,
            delta_count: 0,
            subscriber_count: 0,
        };
        let code = unsafe { ffi::ds_world_get_fanout_view(self.raw.as_ptr(), &mut view) };
        check(code, "ds_world_get_fanout_view")?;
        Ok(FanoutView { world: self, raw: view })
    }

    pub fn spawn(
        &mut self,
        entity_id: EntityId,
        spatial_x: i32,
        spatial_y: i32,
        type_mask: TypeMask,
    ) -> Result<()> {
        let code = unsafe {
            ffi::ds_entity_spawn(
                self.raw.as_ptr(),
                entity_id,
                spatial_x,
                spatial_y,
                type_mask,
            )
        };
        check(code, "ds_entity_spawn")
    }

    pub fn move_entity(
        &mut self,
        entity_id: EntityId,
        spatial_x: i32,
        spatial_y: i32,
    ) -> Result<()> {
        let code = unsafe {
            ffi::ds_entity_move(self.raw.as_ptr(), entity_id, spatial_x, spatial_y)
        };
        check(code, "ds_entity_move")
    }

    pub fn remove(&mut self, entity_id: EntityId) -> Result<()> {
        let code = unsafe { ffi::ds_entity_remove(self.raw.as_ptr(), entity_id) };
        check(code, "ds_entity_remove")
    }

    pub fn mark_dirty(
        &mut self,
        entity_id: EntityId,
        channel_mask: ChannelMask,
    ) -> Result<()> {
        let code = unsafe {
            ffi::ds_entity_mark_dirty(self.raw.as_ptr(), entity_id, channel_mask)
        };
        check(code, "ds_entity_mark_dirty")
    }

    pub fn set_motion_plan(
        &mut self,
        entity_id: EntityId,
        plan: MotionPlan,
    ) -> Result<()> {
        let raw_plan = ffi::DsMotionPlan {
            start_tick: plan.start_tick,
            until_tick: plan.until_tick,
            x: plan.x,
            y: plan.y,
            vx: plan.vx,
            vy: plan.vy,
        };
        let code = unsafe {
            ffi::ds_entity_set_motion_plan(self.raw.as_ptr(), entity_id, &raw_plan)
        };
        check(code, "ds_entity_set_motion_plan")
    }

    pub fn clear_motion_plan(&mut self, entity_id: EntityId) -> Result<()> {
        let code = unsafe {
            ffi::ds_entity_clear_motion_plan(self.raw.as_ptr(), entity_id)
        };
        check(code, "ds_entity_clear_motion_plan")
    }

    pub fn motion_mode(&self, entity_id: EntityId) -> Result<MotionMode> {
        let mut mode = 0_i32;
        let code = unsafe {
            ffi::ds_entity_motion_mode(self.raw.as_ptr(), entity_id, &mut mode)
        };
        check(code, "ds_entity_motion_mode")?;
        Ok(MotionMode::from_raw(mode))
    }

    pub fn entity_exists(&self, entity_id: EntityId) -> bool {
        unsafe { ffi::ds_entity_exists(self.raw.as_ptr(), entity_id) }
    }

    pub fn entity(&self, entity_id: EntityId) -> Result<Entity> {
        let mut entity = ffi::DsEntityDesc {
            id: 0,
            spatial_x: 0,
            spatial_y: 0,
            type_mask: 0,
        };
        let code = unsafe {
            ffi::ds_entity_get(self.raw.as_ptr(), entity_id, &mut entity)
        };
        check(code, "ds_entity_get")?;

        Ok(Entity {
            id: entity.id,
            spatial_x: entity.spatial_x,
            spatial_y: entity.spatial_y,
            type_mask: entity.type_mask,
        })
    }

    pub fn create_observer(&mut self, config: ObserverConfig) -> Result<ObserverId> {
        let raw_config = ffi::DsObserverConfig {
            radius: config.radius,
            type_mask: config.type_mask,
        };
        let mut observer_id = 0;
        let code = unsafe {
            ffi::ds_observer_create(
                self.raw.as_ptr(),
                &raw_config,
                &mut observer_id,
            )
        };
        check(code, "ds_observer_create")?;
        Ok(observer_id)
    }

    pub fn destroy_observer(&mut self, observer_id: ObserverId) -> Result<()> {
        let code = unsafe {
            ffi::ds_observer_destroy(self.raw.as_ptr(), observer_id)
        };
        check(code, "ds_observer_destroy")
    }

    pub fn anchor_observer(
        &mut self,
        observer_id: ObserverId,
        entity_id: EntityId,
    ) -> Result<()> {
        let code = unsafe {
            ffi::ds_observer_anchor_entity(
                self.raw.as_ptr(),
                observer_id,
                entity_id,
            )
        };
        check(code, "ds_observer_anchor_entity")
    }

    pub fn set_observer_position(
        &mut self,
        observer_id: ObserverId,
        spatial_x: i32,
        spatial_y: i32,
    ) -> Result<()> {
        let code = unsafe {
            ffi::ds_observer_set_position(
                self.raw.as_ptr(),
                observer_id,
                spatial_x,
                spatial_y,
            )
        };
        check(code, "ds_observer_set_position")
    }

    pub fn set_observer_radius(
        &mut self,
        observer_id: ObserverId,
        radius: i32,
    ) -> Result<()> {
        let code = unsafe {
            ffi::ds_observer_set_radius(self.raw.as_ptr(), observer_id, radius)
        };
        check(code, "ds_observer_set_radius")
    }

    pub fn observer_exists(&self, observer_id: ObserverId) -> bool {
        unsafe { ffi::ds_observer_exists(self.raw.as_ptr(), observer_id) }
    }

    pub fn observer(&self, observer_id: ObserverId) -> Result<Observer> {
        let mut observer = ffi::DsObserverDesc {
            id: 0,
            spatial_x: 0,
            spatial_y: 0,
            radius: 0,
            type_mask: 0,
            anchor_entity_id: 0,
            positioned: false,
            anchored: false,
        };
        let code = unsafe {
            ffi::ds_observer_get(self.raw.as_ptr(), observer_id, &mut observer)
        };
        check(code, "ds_observer_get")?;

        Ok(Observer {
            id: observer.id,
            spatial_x: observer.spatial_x,
            spatial_y: observer.spatial_y,
            radius: observer.radius,
            type_mask: observer.type_mask,
            anchor_entity_id: observer.anchor_entity_id,
            positioned: observer.positioned,
            anchored: observer.anchored,
        })
    }
}

impl Drop for World {
    fn drop(&mut self) {
        unsafe { ffi::ds_world_destroy(self.raw.as_ptr()) }
    }
}

pub struct FanoutView<'world> {
    world: &'world World,
    raw: ffi::DsFanoutView,
}

impl<'world> FanoutView<'world> {
    pub fn tick(&self) -> Tick {
        self.raw.tick
    }

    pub fn chunk_count(&self) -> usize {
        self.raw.chunk_count
    }

    pub fn delta_count(&self) -> usize {
        self.raw.delta_count
    }

    pub fn subscriber_count(&self) -> usize {
        self.raw.subscriber_count
    }

    pub fn is_empty(&self) -> bool {
        self.raw.chunk_count == 0
    }

    pub fn groups(&self) -> ChunkDeltaIter<'_> {
        let chunks = unsafe { borrowed_slice(self.raw.chunks, self.raw.chunk_count) };
        ChunkDeltaIter {
            inner: chunks.iter(),
        }
    }

    pub fn world(&self) -> &'world World {
        self.world
    }
}

unsafe fn borrowed_slice<'a, T>(pointer: *const T, count: usize) -> &'a [T] {
    if count == 0 {
        return &[];
    }

    assert!(!pointer.is_null(), "libdense_sim returned a null non-empty span");
    unsafe { slice::from_raw_parts(pointer, count) }
}

pub struct ChunkDeltaIter<'a> {
    inner: slice::Iter<'a, ffi::DsChunkDelta>,
}

impl<'a> Iterator for ChunkDeltaIter<'a> {
    type Item = ChunkDeltaView<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|raw| ChunkDeltaView { raw })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

impl ExactSizeIterator for ChunkDeltaIter<'_> {}

#[derive(Clone, Copy)]
pub struct ChunkDeltaView<'a> {
    raw: &'a ffi::DsChunkDelta,
}

impl<'a> ChunkDeltaView<'a> {
    pub fn chunk_x(&self) -> i32 {
        self.raw.chunk_x
    }

    pub fn chunk_y(&self) -> i32 {
        self.raw.chunk_y
    }

    pub fn entry_count(&self) -> usize {
        self.raw.entry_count
    }

    pub fn subscriber_count(&self) -> usize {
        self.raw.subscriber_count
    }

    pub fn entries(&self) -> DeltaEntryIter<'a> {
        let entries = unsafe { borrowed_slice(self.raw.entries, self.raw.entry_count) };
        DeltaEntryIter {
            inner: entries.iter(),
        }
    }

    pub fn subscribers(&self) -> &'a [ObserverId] {
        unsafe { borrowed_slice(self.raw.subscribers, self.raw.subscriber_count) }
    }
}

pub struct DeltaEntryIter<'a> {
    inner: slice::Iter<'a, ffi::DsDeltaEntry>,
}

impl Iterator for DeltaEntryIter<'_> {
    type Item = DeltaEntry;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|entry| DeltaEntry {
            entity_id: entry.entity_id,
            channel_mask: entry.channel_mask,
            operation: DeltaOp::from_raw(entry.operation),
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

impl ExactSizeIterator for DeltaEntryIter<'_> {}
