"""Hardware-only posture mapping and one-shot action scheduling."""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Dict, Optional, Tuple

EMOTIONS = ("neutral", "joy", "sadness", "anger", "fear", "surprise", "disgust", "curiosity", "affection")


@dataclass(frozen=True)
class Pose:
    height: float = 0.0
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0


class MappingError(ValueError):
    pass


class NeutralBreather:
    """Bounded height-only analogue of the Gazebo neutral idle pattern."""

    def __init__(
        self,
        entrance_height: float = 1.0,
        idle_high_height: float = 0.65,
        idle_low_height: float = -0.39,
        entrance_duration: float = 0.25,
        idle_period: float = 1.375,
    ):
        values = (entrance_height, idle_high_height, idle_low_height)
        if any(not math.isfinite(value) or abs(value) > 1.0 for value in values):
            raise MappingError("neutral breathing height outside [-1, 1]")
        if not math.isfinite(entrance_duration) or entrance_duration <= 0.0:
            raise MappingError("neutral breathing entrance duration must be positive")
        if not math.isfinite(idle_period) or idle_period <= 0.0:
            raise MappingError("neutral breathing idle period must be positive")
        self.entrance_height = float(entrance_height)
        self.idle_high_height = float(idle_high_height)
        self.idle_low_height = float(idle_low_height)
        self.entrance_duration = float(entrance_duration)
        self.idle_period = float(idle_period)

    @staticmethod
    def _smootherstep(value: float) -> float:
        value = max(0.0, min(1.0, float(value)))
        return value * value * value * (value * (value * 6.0 - 15.0) + 10.0)

    @classmethod
    def _mix(cls, first: float, second: float, amount: float) -> float:
        return first + (second - first) * cls._smootherstep(amount)

    def height(self, elapsed: float) -> float:
        """Return the normalized entrance/idle height at ``elapsed`` seconds."""
        elapsed = max(0.0, float(elapsed))
        if elapsed < self.entrance_duration:
            return self._mix(0.0, self.entrance_height, elapsed / self.entrance_duration)

        idle_elapsed = elapsed - self.entrance_duration
        half_period = self.idle_period / 2.0
        cycle = int(idle_elapsed / self.idle_period)
        phase = idle_elapsed % self.idle_period
        if phase < half_period:
            previous = self.entrance_height if cycle == 0 else self.idle_low_height
            return self._mix(previous, self.idle_high_height, phase / half_period)
        return self._mix(
            self.idle_high_height,
            self.idle_low_height,
            (phase - half_period) / half_period,
        )


def load_profiles(raw: Dict[str, Dict[str, float]]) -> Dict[str, Pose]:
    if set(raw) != set(EMOTIONS):
        raise MappingError("all nine hardware profiles are required")
    profiles = {}
    for emotion in EMOTIONS:
        values = raw[emotion]
        pose = Pose(*(float(values.get(axis, 0.0)) for axis in ("height", "roll", "pitch", "yaw")))
        if pose.yaw != 0.0:
            raise MappingError("hardware yaw must remain zero")
        if any(abs(getattr(pose, axis)) > 1.0 for axis in ("height", "roll", "pitch")):
            raise MappingError("normalized posture value outside [-1, 1]")
        profiles[emotion] = pose
    return profiles


class ActionScheduler:
    def __init__(self, hop_cooldown: float = 15.0, stomp_cooldown: float = 10.0):
        self.cooldowns = {"hop": float(hop_cooldown), "stomp": float(stomp_cooldown)}
        self.last_emotion = None
        self.last_action_at = {"hop": float("-inf"), "stomp": float("-inf")}
        self.next_stomp_leg = "front_left"
        self.pending_stomp_leg = None

    def on_emotion(self, emotion: str, now: float, dynamic_armed: bool) -> Optional[Tuple[str, str]]:
        changed = emotion != self.last_emotion
        self.last_emotion = emotion
        if not changed or not dynamic_armed:
            return None
        action = "hop" if emotion in ("joy", "surprise") else "stomp" if emotion == "anger" else None
        if action is None or now - self.last_action_at[action] < self.cooldowns[action]:
            return None
        self.last_action_at[action] = now
        if action == "stomp":
            self.pending_stomp_leg = self.next_stomp_leg
            return action, self.pending_stomp_leg
        return action, "none"

    def complete(self, action: str, success: bool):
        if action == "stomp" and success and self.pending_stomp_leg is not None:
            self.next_stomp_leg = "front_right" if self.pending_stomp_leg == "front_left" else "front_left"
        self.pending_stomp_leg = None
