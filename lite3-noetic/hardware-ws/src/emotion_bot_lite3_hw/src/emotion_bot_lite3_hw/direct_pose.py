"""Pure safety and waveform logic for the Retroid-compatible Pose path."""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Any, Dict, Optional

from .mapping import NeutralBreather


HEARTBEAT_CODE = 0x21040001
HEIGHT_CODE = 0x21010102
YAW_CODE = 0x21010135
MOVE_MODE_CODE = 0x21010D06
POSE_MODE_CODE = 0x21010D05


@dataclass(frozen=True)
class DirectPoseLimits:
    """Commissioned limits for the neutral, height-only physical animation."""

    height_limit: int = 10000
    maximum_rate: float = 4000.0
    entrance_duration: float = 4.0
    idle_period: float = 10.0
    entrance_ratio: float = -1.0
    idle_high_ratio: float = 0.0
    idle_low_ratio: float = -1.0
    publish_rate: float = 50.0

    def __post_init__(self):
        if not 0 < self.height_limit <= 10000:
            raise ValueError("height limit must remain within the commissioned 50% bound")
        if not math.isfinite(self.maximum_rate) or self.maximum_rate <= 0.0:
            raise ValueError("maximum rate must be positive")
        if not math.isfinite(self.publish_rate) or self.publish_rate <= 0.0:
            raise ValueError("publish rate must be positive")
        if not math.isfinite(self.entrance_duration) or self.entrance_duration <= 0.0:
            raise ValueError("entrance duration must be positive")
        if not math.isfinite(self.idle_period) or self.idle_period <= 0.0:
            raise ValueError("idle period must be positive")
        if not -1.0 <= self.idle_low_ratio <= self.idle_high_ratio <= 1.0:
            raise ValueError("idle ratios must be ordered and within [-1, 1]")
        if not -1.0 <= self.entrance_ratio <= 1.0:
            raise ValueError("entrance ratio must be within [-1, 1]")


class RateLimitedHeight:
    """Generate the smooth neutral waveform with an explicit units/s limit."""

    def __init__(self, limits: DirectPoseLimits = DirectPoseLimits()):
        self.limits = limits
        self.wave = NeutralBreather(
            entrance_height=limits.entrance_ratio,
            idle_high_height=limits.idle_high_ratio,
            idle_low_height=limits.idle_low_ratio,
            entrance_duration=limits.entrance_duration,
            idle_period=limits.idle_period,
        )
        self.current = 0.0

    def reset(self):
        self.current = 0.0

    def update(self, elapsed: float, delta: float) -> int:
        return self.update_target(self.wave.height(elapsed), delta)

    def update_target(self, normalized: float, delta: float) -> int:
        normalized = max(-1.0, min(1.0, float(normalized)))
        requested = normalized * self.limits.height_limit
        step = self.limits.maximum_rate * max(0.0, float(delta))
        if requested > self.current:
            self.current = min(requested, self.current + step)
        else:
            self.current = max(requested, self.current - step)
        self.current = max(-self.limits.height_limit, min(self.limits.height_limit, self.current))
        return int(round(self.current))


def gate_failure(snapshot: Dict[str, Any], minimum_battery: float = 25.0) -> Optional[str]:
    """Return the first reason direct neutral Pose output must remain stopped."""

    status = snapshot.get("status") or {}
    robot = snapshot.get("robot") or {}
    emotion = snapshot.get("emotion") or {}
    ages = snapshot.get("ages") or {}
    checks = (
        (status.get("state") == "DISARMED", "supervisor is not DISARMED"),
        (status.get("posture_armed") is False, "posture sender is armed"),
        (status.get("dynamic_armed") is False, "dynamic sender is armed"),
        (status.get("robot_basic_state") == 6, "supervisor does not report stable stand"),
        (status.get("stable_stand") is True, "robot has not held a stable stand"),
        (status.get("telemetry_fresh") is True, "telemetry is stale"),
        (status.get("joint_feedback_healthy") is True, "joint feedback is unhealthy"),
        (status.get("imu_feedback_healthy") is True, "IMU feedback is unhealthy"),
        (status.get("retroid_fresh") is True, "Retroid observation is stale"),
        (status.get("retroid_axes_zero") is True, "Retroid axes are not centered"),
        (status.get("stop") is False, "Retroid STOP is active"),
        (status.get("link_fresh") is True, "emotion link is stale"),
        (robot.get("basic_state") == 6, "robot is not standing"),
        (robot.get("errors") == 0, "robot reports an error"),
        (emotion.get("emotion") == "neutral", "emotion is not neutral"),
        (len(snapshot.get("joints") or []) == 12, "joint sample is incomplete"),
        (len(snapshot.get("imu") or []) == 2, "IMU sample is incomplete"),
    )
    for accepted, reason in checks:
        if not accepted:
            return reason
    maximum_ages = {"status": 0.3, "robot": 0.3, "emotion": 0.5, "joints": 0.2, "imu": 0.2}
    for name, maximum in maximum_ages.items():
        try:
            age = float(ages[name])
        except (KeyError, TypeError, ValueError):
            return "%s age is missing" % name
        if not math.isfinite(age) or age < 0.0 or age > maximum:
            return "%s input is stale" % name
    try:
        battery = float(robot["battery"])
        roll = float(robot["roll_deg"])
        pitch = float(robot["pitch_deg"])
    except (KeyError, TypeError, ValueError):
        return "robot state is incomplete"
    if not all(math.isfinite(value) for value in (battery, roll, pitch)):
        return "robot state is non-finite"
    if battery < float(minimum_battery):
        return "battery is below %.1f%%" % float(minimum_battery)
    if abs(roll) > 10.0 or abs(pitch) > 10.0:
        return "robot attitude exceeds the commissioned bound"
    return None


def safe_neutral_gate(snapshot: Dict[str, Any], minimum_battery: float = 25.0) -> bool:
    return gate_failure(snapshot, minimum_battery) is None
