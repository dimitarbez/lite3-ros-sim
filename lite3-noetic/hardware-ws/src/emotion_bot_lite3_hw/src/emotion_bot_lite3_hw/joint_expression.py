"""Smooth, symmetric, four-leg-planted direct-joint expression planning."""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Dict, Iterable, Sequence, Tuple

from .mapping import EMOTIONS


DIRECT_JOINT_NAMES = (
    "FL_HipX", "FL_HipY", "FL_Knee",
    "FR_HipX", "FR_HipY", "FR_Knee",
    "HL_HipX", "HL_HipY", "HL_Knee",
    "HR_HipX", "HR_HipY", "HR_Knee",
)


class JointExpressionError(ValueError):
    pass


@dataclass(frozen=True)
class ScalarSample:
    position: float
    velocity: float
    acceleration: float


@dataclass(frozen=True)
class JointSample:
    position: Tuple[float, ...]
    velocity: Tuple[float, ...]
    acceleration: Tuple[float, ...]


@dataclass(frozen=True)
class Pattern:
    """Piecewise quintic compression levels; every loop starts and ends neutral."""

    amplitude: float
    keyframes: Tuple[Tuple[float, float], ...]

    def __post_init__(self):
        if not math.isfinite(self.amplitude) or not 0.0 <= self.amplitude <= 0.05:
            raise JointExpressionError("pattern amplitude must be in [0, 0.05] rad")
        if not self.keyframes or self.keyframes[-1][1] != 0.0:
            raise JointExpressionError("pattern must end at measured neutral")
        for duration, level in self.keyframes:
            if not math.isfinite(duration) or duration < 0.25:
                raise JointExpressionError("each planted segment must last at least 0.25 s")
            if not math.isfinite(level) or not 0.0 <= level <= 1.0:
                raise JointExpressionError("compression level must be in [0, 1]")

    @property
    def duration(self) -> float:
        return sum(duration for duration, _ in self.keyframes)


# All spatial profiles are the same contact-preserving four-leg compression.
# Emotions remain distinct through bounded amplitude and temporal phrasing. No
# profile commands HipX, planar translation, yaw, foot lift, or extension above
# the freshly measured starting stance.
PATTERNS: Dict[str, Pattern] = {
    "neutral": Pattern(0.012, ((3.0, 0.55), (3.0, 0.0))),
    "joy": Pattern(0.020, ((0.65, 0.75), (0.65, 0.0), (0.65, 0.75), (1.55, 0.0))),
    "sadness": Pattern(0.026, ((2.5, 1.0), (2.0, 0.82), (2.5, 0.0))),
    "anger": Pattern(0.018, ((0.75, 0.75), (0.75, 0.35), (0.75, 0.75), (1.25, 0.0))),
    "fear": Pattern(0.010, ((0.45, 0.70), (0.45, 0.25), (0.45, 0.70), (0.45, 0.25), (1.20, 0.0))),
    "surprise": Pattern(0.024, ((1.0, 1.0), (1.5, 0.0), (2.5, 0.0))),
    "disgust": Pattern(0.017, ((1.4, 0.75), (0.8, 0.30), (0.8, 0.72), (1.6, 0.0))),
    "curiosity": Pattern(0.014, ((1.8, 0.55), (1.0, 0.20), (1.0, 0.55), (1.8, 0.0))),
    "affection": Pattern(0.015, ((0.75, 0.45), (0.75, 0.0), (0.75, 0.75), (2.75, 0.0))),
}


class Quintic:
    """Quintic scalar segment with explicit p/v/a endpoint conditions."""

    def __init__(self, start: ScalarSample, end: ScalarSample, duration: float):
        if not math.isfinite(duration) or duration <= 0.0:
            raise JointExpressionError("quintic duration must be positive")
        values = (start.position, start.velocity, start.acceleration,
                  end.position, end.velocity, end.acceleration)
        if not all(math.isfinite(value) for value in values):
            raise JointExpressionError("quintic boundary contains a non-finite value")
        self.duration = float(duration)
        t = self.duration
        a0 = start.position
        a1 = start.velocity
        a2 = 0.5 * start.acceleration
        p = end.position - (a0 + a1 * t + a2 * t * t)
        v = end.velocity - (a1 + 2.0 * a2 * t)
        a = end.acceleration - 2.0 * a2
        a3 = (10.0 * p - 4.0 * v * t + 0.5 * a * t * t) / (t ** 3)
        a4 = (-15.0 * p + 7.0 * v * t - a * t * t) / (t ** 4)
        a5 = (6.0 * p - 3.0 * v * t + 0.5 * a * t * t) / (t ** 5)
        self.coefficients = (a0, a1, a2, a3, a4, a5)

    def sample(self, elapsed: float) -> ScalarSample:
        t = min(self.duration, max(0.0, float(elapsed)))
        a0, a1, a2, a3, a4, a5 = self.coefficients
        position = a0 + a1*t + a2*t**2 + a3*t**3 + a4*t**4 + a5*t**5
        velocity = a1 + 2*a2*t + 3*a3*t**2 + 4*a4*t**3 + 5*a5*t**4
        acceleration = 2*a2 + 6*a3*t + 12*a4*t**2 + 20*a5*t**3
        return ScalarSample(position, velocity, acceleration)


class ExpressionPlanner:
    """Neutral-transition state machine for continuous emotion replacement."""

    def __init__(self, neutral_return_time: float = 1.5, neutral_hold_time: float = 0.35):
        if neutral_return_time < 0.5 or neutral_hold_time < 0.25:
            raise JointExpressionError("neutral transition timings are too short")
        self.neutral_return_time = float(neutral_return_time)
        self.neutral_hold_time = float(neutral_hold_time)
        self.requested = "neutral"
        self.active = "neutral"
        self.pattern_started = 0.0
        self.transition_started = 0.0
        self.transition = Quintic(ScalarSample(0.0, 0.0, 0.0), ScalarSample(0.0, 0.0, 0.0), 1.0)
        self.hold_until = 0.0
        self.phase = "pattern"

    def sample(self, now: float) -> ScalarSample:
        now = float(now)
        if self.phase == "transition":
            elapsed = now - self.transition_started
            if elapsed < self.transition.duration:
                return self.transition.sample(elapsed)
            self.phase = "hold"
            self.hold_until = self.transition_started + self.transition.duration + self.neutral_hold_time
        if self.phase == "hold":
            if now < self.hold_until:
                return ScalarSample(0.0, 0.0, 0.0)
            self.phase = "pattern"
            self.active = self.requested
            self.pattern_started = now
        return sample_pattern(PATTERNS[self.active], now - self.pattern_started)

    def request(self, emotion: str, now: float) -> None:
        if emotion not in PATTERNS:
            raise JointExpressionError("unknown emotion")
        if emotion == self.requested:
            return
        current = self.sample(now)
        self.requested = emotion
        self.transition = Quintic(
            current, ScalarSample(0.0, 0.0, 0.0), self.neutral_return_time
        )
        self.transition_started = float(now)
        self.phase = "transition"

    @property
    def output_emotion(self) -> str:
        return self.active if self.phase == "pattern" else "neutral"


def sample_pattern(pattern: Pattern, elapsed: float) -> ScalarSample:
    elapsed = max(0.0, float(elapsed)) % pattern.duration
    previous = 0.0
    for duration, target in pattern.keyframes:
        if elapsed <= duration:
            segment = Quintic(
                ScalarSample(previous * pattern.amplitude, 0.0, 0.0),
                ScalarSample(target * pattern.amplitude, 0.0, 0.0),
                duration,
            )
            return segment.sample(elapsed)
        elapsed -= duration
        previous = target
    return ScalarSample(0.0, 0.0, 0.0)


def planted_joint_sample(anchor: Sequence[float], compression: ScalarSample) -> JointSample:
    """Convert compression to identical HipY/Knee changes on all four legs.

    Direct SDK stance convention is HipY negative and Knee positive. A positive
    compression therefore applies ``-q`` to every HipY and ``+2q`` to every
    Knee. HipX remains exactly at its measured value.
    """
    if len(anchor) != 12 or not all(math.isfinite(float(value)) for value in anchor):
        raise JointExpressionError("a finite 12-joint measured anchor is required")
    if compression.position < -1e-12:
        raise JointExpressionError("extension above the measured stance is prohibited")
    position = [float(value) for value in anchor]
    velocity = [0.0] * 12
    acceleration = [0.0] * 12
    for leg in range(4):
        hip_y = 3 * leg + 1
        knee = 3 * leg + 2
        position[hip_y] -= compression.position
        position[knee] += 2.0 * compression.position
        velocity[hip_y] = -compression.velocity
        velocity[knee] = 2.0 * compression.velocity
        acceleration[hip_y] = -compression.acceleration
        acceleration[knee] = 2.0 * compression.acceleration
    return JointSample(tuple(position), tuple(velocity), tuple(acceleration))


def validate_planted_symmetry(anchor: Sequence[float], sample: JointSample, tolerance: float = 1e-9) -> bool:
    """Prove the command contains no HipX, unilateral-leg, or non-2:1 motion."""
    if len(anchor) != 12 or any(len(values) != 12 for values in (
        sample.position, sample.velocity, sample.acceleration,
    )):
        return False
    compressions = []
    for leg in range(4):
        hip_x, hip_y, knee = 3 * leg, 3 * leg + 1, 3 * leg + 2
        if abs(sample.position[hip_x] - anchor[hip_x]) > tolerance:
            return False
        compression = anchor[hip_y] - sample.position[hip_y]
        if compression < -tolerance:
            return False
        if abs((sample.position[knee] - anchor[knee]) - 2.0 * compression) > tolerance:
            return False
        if abs(sample.velocity[knee] + 2.0 * sample.velocity[hip_y]) > tolerance:
            return False
        if abs(sample.acceleration[knee] + 2.0 * sample.acceleration[hip_y]) > tolerance:
            return False
        compressions.append(compression)
    return max(compressions) - min(compressions) <= tolerance


def validate_patterns(patterns: Dict[str, Pattern] = PATTERNS) -> None:
    if set(patterns) != set(EMOTIONS):
        raise JointExpressionError("all nine planted emotion patterns are required")
    anchor = (0.0, -0.75, 1.40) * 4
    for emotion, pattern in patterns.items():
        peak_velocity = 0.0
        peak_acceleration = 0.0
        for index in range(2001):
            scalar = sample_pattern(pattern, pattern.duration * index / 2000.0)
            sample = planted_joint_sample(anchor, scalar)
            if not validate_planted_symmetry(anchor, sample):
                raise JointExpressionError("%s violates planted symmetry" % emotion)
            peak_velocity = max(peak_velocity, *(abs(value) for value in sample.velocity))
            peak_acceleration = max(peak_acceleration, *(abs(value) for value in sample.acceleration))
        if peak_velocity > 0.20 or peak_acceleration > 0.80:
            raise JointExpressionError("%s exceeds conservative p/v/a envelope" % emotion)


validate_patterns()
