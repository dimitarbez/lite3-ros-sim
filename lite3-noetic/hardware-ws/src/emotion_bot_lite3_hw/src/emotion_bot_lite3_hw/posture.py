"""Bounded, rate-limited conversion from normalized pose to SimpleCMD values."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict

from .mapping import Pose


@dataclass(frozen=True)
class AxisConfig:
    code: int
    protocol_limit: int
    commissioned_limit: int
    max_rate: float


class PostureLimiter:
    def __init__(self, axes: Dict[str, AxisConfig]):
        self.axes = axes
        self.values = {axis: 0.0 for axis in axes}

    def update(self, target: Pose, dt: float, armed: bool) -> Dict[int, int]:
        result = {}
        for axis, config in self.axes.items():
            requested = getattr(target, axis) if armed and axis != "yaw" else 0.0
            requested = max(-1.0, min(1.0, requested)) * min(config.commissioned_limit, config.protocol_limit)
            previous = self.values[axis]
            step = max(0.0, config.max_rate) * max(0.0, dt)
            if requested > previous:
                current = min(requested, previous + step)
            else:
                current = max(requested, previous - step)
            if not armed:
                current = 0.0
            self.values[axis] = current
            result[config.code] = int(round(current))
        return result

    def neutral(self) -> Dict[int, int]:
        self.values = {axis: 0.0 for axis in self.axes}
        return {config.code: 0 for config in self.axes.values()}
