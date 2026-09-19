#!/usr/bin/env python3
"""Map EmotionBot state to measured-anchor, planted direct-joint commands."""

import json
import math
import time

import rospy
from std_msgs.msg import String

from emotion_bot_lite3_hw.feedback import DIRECT_JOINT_NAMES, valid_direct_feedback
from emotion_bot_lite3_hw.joint_expression import (
    ExpressionPlanner, PATTERNS, planted_joint_sample, validate_planted_symmetry,
)
from emotion_bot_lite3_hw.msg import DirectJointCommand, DirectJointFeedback, DirectJointSafety


class PlantedJointController:
    def __init__(self):
        ns = "/emotion_bot/hardware/direct_joint"
        self.auto_enable = bool(rospy.get_param("~auto_enable", False))
        self.hold_only = bool(rospy.get_param("~hold_only", False))
        self.commissioning_scale = float(rospy.get_param("~commissioning_scale", 1.0))
        if not 0.0 < self.commissioning_scale <= 1.0:
            raise ValueError("commissioning_scale must be in (0, 1]")
        self.stable_time = float(rospy.get_param(ns + "/stable_gate_time", 2.0))
        self.feedback_timeout = float(rospy.get_param(ns + "/feedback_timeout", 0.05))
        self.maximum_temperature = float(rospy.get_param(
            "/emotion_bot/hardware/safety/maximum_joint_temperature_c", 70.0
        ))
        self.kp = float(rospy.get_param(ns + "/kp", 30.0))
        self.kd = float(rospy.get_param(ns + "/kd", 0.7))
        self.planner = ExpressionPlanner(
            rospy.get_param(ns + "/neutral_return_time", 1.5),
            rospy.get_param(ns + "/neutral_hold_time", 0.35),
        )
        self.feedback = None
        self.feedback_at = None
        self.safety = None
        self.safety_at = None
        self.safe_since = None
        self.anchor = None
        self.enabled = False
        self.generation = 0
        self.requested_emotion = "neutral"
        self.publisher = rospy.Publisher(
            ns + "/command", DirectJointCommand, queue_size=5
        )
        rospy.Subscriber(ns + "/feedback", DirectJointFeedback, self.on_feedback, queue_size=100)
        rospy.Subscriber(ns + "/safety", DirectJointSafety, self.on_safety, queue_size=20)
        rospy.Subscriber(
            "/emotion_bot/hardware/emotion_state", String, self.on_emotion, queue_size=20
        )
        rate = float(rospy.get_param(ns + "/command_rate", 100.0))
        if rate < 50.0 or rate > 250.0:
            raise ValueError("direct joint command_rate must be in [50, 250] Hz")
        rospy.Timer(rospy.Duration(1.0 / rate), self.tick)
        rospy.on_shutdown(self.shutdown)

    def on_feedback(self, message):
        if tuple(message.name) != DIRECT_JOINT_NAMES:
            self.feedback = None
            self.feedback_at = None
            return
        if not valid_direct_feedback(
            message.position, message.velocity, message.temperature, self.maximum_temperature
        ):
            self.feedback = None
            self.feedback_at = None
            return
        self.feedback = message
        self.feedback_at = time.monotonic()

    def on_safety(self, message):
        self.safety = message
        self.safety_at = time.monotonic()

    def on_emotion(self, message):
        try:
            value = json.loads(message.data)
            emotion = value["emotion"]
            if emotion not in PATTERNS:
                return
        except (KeyError, TypeError, ValueError):
            return
        if emotion != self.requested_emotion:
            self.requested_emotion = emotion
            self.generation += 1
            self.planner.request(emotion, time.monotonic())

    def feedback_fresh(self, now):
        return self.feedback_at is not None and now - self.feedback_at <= self.feedback_timeout

    def safety_fresh(self, now):
        return self.safety_at is not None and now - self.safety_at <= 0.15

    def publish(self, enable, release, position, velocity=None, acceleration=None, emotion="neutral"):
        message = DirectJointCommand()
        message.header.stamp = rospy.Time.now()
        message.generation = self.generation
        message.emotion = emotion
        message.enable = bool(enable)
        message.release = bool(release)
        message.position = position
        message.velocity = [0.0] * 12 if velocity is None else velocity
        message.acceleration = [0.0] * 12 if acceleration is None else acceleration
        message.kp = [self.kp] * 12
        message.kd = [self.kd] * 12
        self.publisher.publish(message)

    def tick(self, _event):
        now = time.monotonic()
        fresh = self.feedback_fresh(now) and self.safety_fresh(now)
        preflight = bool(fresh and self.safety.ready and self.safety.planted_gate_satisfied)
        if preflight:
            self.safe_since = now if self.safe_since is None else self.safe_since
        elif not self.enabled:
            self.safe_since = None

        if (
            self.auto_enable and not self.enabled and self.safe_since is not None
            and now - self.safe_since >= self.stable_time
        ):
            self.anchor = tuple(float(value) for value in self.feedback.position)
            self.enabled = True
            self.generation += 1
            self.planner = ExpressionPlanner(
                rospy.get_param("/emotion_bot/hardware/direct_joint/neutral_return_time", 1.5),
                rospy.get_param("/emotion_bot/hardware/direct_joint/neutral_hold_time", 0.35),
            )
            # A neutral request is already selected in a fresh planner, so
            # request() is intentionally a no-op. Anchor the waveform clock here
            # to ensure commissioning always begins at exact measured neutral.
            self.planner.pattern_started = now
            self.planner.request(self.requested_emotion, now)

        runtime_safe = bool(
            fresh and self.safety.motion_safe and self.safety.link_fresh
            and self.safety.planted_gate_satisfied
        )
        if self.enabled and not runtime_safe:
            anchor = self.anchor or tuple(float(value) for value in self.feedback.position)
            self.publish(False, True, anchor)
            self.enabled = False
            self.anchor = None
            self.safe_since = None
            return
        if not self.enabled:
            return

        scalar = self.planner.sample(now)
        if self.hold_only:
            scalar = type(scalar)(0.0, 0.0, 0.0)
        scalar = type(scalar)(
            scalar.position * self.commissioning_scale,
            scalar.velocity * self.commissioning_scale,
            scalar.acceleration * self.commissioning_scale,
        )
        sample = planted_joint_sample(self.anchor, scalar)
        if not validate_planted_symmetry(self.anchor, sample):
            self.publish(False, True, self.anchor)
            self.enabled = False
            return
        if not all(math.isfinite(value) for values in (
            sample.position, sample.velocity, sample.acceleration,
        ) for value in values):
            self.publish(False, True, self.anchor)
            self.enabled = False
            return
        self.publish(
            True, False, sample.position, sample.velocity, sample.acceleration,
            self.planner.output_emotion,
        )

    def shutdown(self):
        if self.anchor is not None:
            try:
                self.publish(False, True, self.anchor)
            except rospy.ROSException:
                pass


def main():
    rospy.init_node("planted_joint_controller")
    PlantedJointController()
    rospy.spin()


if __name__ == "__main__":
    main()
