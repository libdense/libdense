from enum import IntEnum

from ._dense_sim import (
    CHANNEL_ANIMATION,
    CHANNEL_APPEARANCE,
    CHANNEL_CUSTOM_0,
    CHANNEL_POSITION,
    CHANNEL_VITALS,
    DELTA_ENTER,
    DELTA_LEAVE,
    DELTA_REMOVE,
    DELTA_SPAWN,
    DELTA_UPDATE,
    MOTION_KINETIC,
    MOTION_SAMPLED,
    ChunkDeltaView,
    DeltaEntry,
    DenseSimError,
    FanoutView,
    World,
)


class MotionMode(IntEnum):
    SAMPLED = MOTION_SAMPLED
    KINETIC = MOTION_KINETIC


class DeltaOp(IntEnum):
    UPDATE = DELTA_UPDATE
    ENTER = DELTA_ENTER
    LEAVE = DELTA_LEAVE
    SPAWN = DELTA_SPAWN
    REMOVE = DELTA_REMOVE


__all__ = [
    "CHANNEL_ANIMATION",
    "CHANNEL_APPEARANCE",
    "CHANNEL_CUSTOM_0",
    "CHANNEL_POSITION",
    "CHANNEL_VITALS",
    "ChunkDeltaView",
    "DeltaEntry",
    "DeltaOp",
    "DenseSimError",
    "FanoutView",
    "MotionMode",
    "World",
]

__version__ = "0.1.0rc1"
