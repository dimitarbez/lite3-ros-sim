"""Validation helpers for vendor ROS posture feedback."""

from __future__ import annotations

import math
from typing import Iterable, Sequence


EXPECTED_JOINT_NAMES = (
    "LF_Joint", "LF_Joint_1", "LF_Joint_2",
    "RF_Joint", "RF_Joint_1", "RF_Joint_2",
    "LB_Joint", "LB_Joint_1", "LB_Joint_2",
    "RB_Joint", "RB_Joint_1", "RB_Joint_2",
)

DIRECT_JOINT_NAMES = (
    "FL_HipX", "FL_HipY", "FL_Knee",
    "FR_HipX", "FR_HipY", "FR_Knee",
    "HL_HipX", "HL_HipY", "HL_Knee",
    "HR_HipX", "HR_HipY", "HR_Knee",
)

# Motion Development Manual V2.0.1-0 hard mechanical envelopes. Runtime
# commissioning uses much smaller measured-anchor delta limits as well.
DIRECT_POSITION_LIMITS = (
    (-0.4189, 0.4189), (-3.4907, 0.3491), (0.6021, 2.7227),
) * 4


def _finite(values: Iterable[float]) -> bool:
    return all(math.isfinite(float(value)) for value in values)


def valid_joint_feedback(names: Sequence[str], positions: Sequence[float]) -> bool:
    """Accept the deployed 12-joint ordering with bounded finite positions."""
    return (
        tuple(names) == EXPECTED_JOINT_NAMES
        and len(positions) == len(EXPECTED_JOINT_NAMES)
        and _finite(positions)
        and all(abs(float(position)) <= 10.0 for position in positions)
    )


def valid_direct_feedback(positions: Sequence[float], velocities: Sequence[float],
                          temperatures: Sequence[float], maximum_temperature: float) -> bool:
    if not (len(positions) == len(velocities) == len(temperatures) == 12):
        return False
    if not _finite(tuple(positions) + tuple(velocities) + tuple(temperatures)):
        return False
    return (
        all(low <= float(value) <= high for value, (low, high) in zip(positions, DIRECT_POSITION_LIMITS))
        and all(abs(float(value)) <= 30.0 for value in velocities)
        and all(-20.0 <= float(value) <= float(maximum_temperature) for value in temperatures)
    )


def valid_imu_feedback(orientation: Sequence[float], angular_velocity: Sequence[float],
                       linear_acceleration: Sequence[float]) -> bool:
    """Validate finite, normalized orientation and broad physical envelopes."""
    if len(orientation) != 4 or len(angular_velocity) != 3 or len(linear_acceleration) != 3:
        return False
    values = tuple(orientation) + tuple(angular_velocity) + tuple(linear_acceleration)
    if not _finite(values):
        return False
    norm = math.sqrt(sum(float(value) ** 2 for value in orientation))
    return (
        0.8 <= norm <= 1.2
        and all(abs(float(value)) <= 100.0 for value in angular_velocity)
        and all(abs(float(value)) <= 200.0 for value in linear_acceleration)
    )
