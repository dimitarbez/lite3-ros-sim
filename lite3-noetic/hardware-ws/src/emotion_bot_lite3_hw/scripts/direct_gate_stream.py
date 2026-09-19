#!/usr/bin/env python3
"""Stream the read-only hardware gates consumed by the host Pose bridge."""

import json
import math
import threading
import time

import rospy
from sensor_msgs.msg import Imu, JointState
from std_msgs.msg import String

from emotion_bot_lite3_hw.msg import RobotState


class GateStream:
    def __init__(self):
        self.lock = threading.Lock()
        self.status = None
        self.robot = None
        self.emotion = None
        self.joints = None
        self.imu = None
        self.received_at = {}
        rospy.Subscriber("/emotion_bot/hardware/status", String, self.on_status, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/robot_state", RobotState, self.on_robot, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/emotion_state", String, self.on_emotion, queue_size=20)
        rospy.Subscriber("/joint_states", JointState, self.on_joint, queue_size=100)
        rospy.Subscriber("/imu/data", Imu, self.on_imu, queue_size=100)

    def on_status(self, message):
        self._store_json("status", message.data)

    def on_emotion(self, message):
        self._store_json("emotion", message.data)

    def _store_json(self, field, raw):
        try:
            value = json.loads(raw)
        except (TypeError, ValueError):
            return
        if not isinstance(value, dict):
            return
        with self.lock:
            setattr(self, field, value)
            self.received_at[field] = time.monotonic()

    def on_robot(self, message):
        with self.lock:
            self.robot = {
                "basic_state": int(message.robot_basic_state),
                "battery": float(message.battery_level),
                "errors": int(message.error_flags),
                "roll_deg": float(message.roll_deg),
                "pitch_deg": float(message.pitch_deg),
            }
            self.received_at["robot"] = time.monotonic()

    def on_joint(self, message):
        values = [float(value) for value in message.position]
        if len(values) != 12 or not all(math.isfinite(value) for value in values):
            return
        with self.lock:
            self.joints = values
            self.received_at["joints"] = time.monotonic()

    def on_imu(self, message):
        values = [float(message.orientation.x), float(message.orientation.y)]
        if not all(math.isfinite(value) for value in values):
            return
        with self.lock:
            self.imu = values
            self.received_at["imu"] = time.monotonic()

    def snapshot(self):
        with self.lock:
            now = time.monotonic()
            return {
                "status": dict(self.status or {}),
                "robot": dict(self.robot or {}),
                "emotion": dict(self.emotion or {}),
                "joints": list(self.joints or []),
                "imu": list(self.imu or []),
                "ages": {
                    name: now - self.received_at.get(name, float("-inf"))
                    for name in ("status", "robot", "emotion", "joints", "imu")
                },
            }


def main():
    rospy.init_node("direct_breathing_gate_stream", anonymous=True)
    stream = GateStream()
    rate = rospy.Rate(20)
    sequence = 0
    while not rospy.is_shutdown():
        value = stream.snapshot()
        value["sequence"] = sequence
        value["monotonic"] = time.monotonic()
        print(json.dumps(value, separators=(",", ":")), flush=True)
        sequence += 1
        rate.sleep()


if __name__ == "__main__":
    main()
