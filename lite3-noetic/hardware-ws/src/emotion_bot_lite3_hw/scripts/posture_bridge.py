#!/usr/bin/env python3
"""Bounded /simple_cmd posture publisher. Disabled by configuration by default."""

import json
import time

import rospy
from std_msgs.msg import Bool, String

from emotion_bot_lite3_hw.mapping import Pose
from emotion_bot_lite3_hw.posture import AxisConfig, PostureLimiter


class PostureBridge:
    def __init__(self):
        from message_transformer.msg import SimpleCMD

        self.message_type = SimpleCMD
        raw_axes = rospy.get_param("/emotion_bot/hardware/posture/axes")
        axes = {name: AxisConfig(
            int(value["code"]), int(value["protocol_limit"]),
            int(value["commissioned_limit"]), float(value["max_rate"]),
        ) for name, value in raw_axes.items()}
        self.limiter = PostureLimiter(axes)
        self.commissioned_codes = {
            config.code for config in axes.values()
            if config.commissioned_limit > 0 and config.max_rate > 0.0
        }
        self.transmit_enabled = bool(rospy.get_param("/emotion_bot/hardware/safety/transmit_enabled", False))
        self.heartbeat_code = int(rospy.get_param("/emotion_bot/hardware/posture/heartbeat_code", 0x21040001))
        self.pose_mode_code = int(rospy.get_param("/emotion_bot/hardware/posture/pose_mode_code", 0x21010D05))
        self.target = Pose()
        self.last_pose_at = None
        self.pose_stale = True
        self.intent_timeout = float(
            rospy.get_param("/emotion_bot/hardware/posture/intent_timeout", 0.25)
        )
        self.armed = False
        self.last_tick = time.monotonic()
        self.last_heartbeat = 0.0
        self.pose_mode_sent = False
        self.publisher = rospy.Publisher("/simple_cmd", SimpleCMD, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/posture_intent", String, self.on_pose, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/status", String, self.on_status, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/neutral_requested", Bool, self.on_neutral, queue_size=10)
        rate = float(rospy.get_param("/emotion_bot/hardware/posture/command_rate", 20.0))
        rospy.Timer(rospy.Duration(1.0 / rate), self.tick)
        rospy.on_shutdown(self.shutdown)

    def publish_simple(self, code, value=0):
        if not self.transmit_enabled or rospy.is_shutdown():
            return
        message = self.message_type()
        message.cmd_code = int(code)
        message.cmd_value = int(value)
        message.type = 0
        try:
            self.publisher.publish(message)
        except rospy.ROSException:
            # ROS can close the publisher before shutdown callbacks and latched
            # neutral callbacks finish. Runtime publish failures still surface.
            if not rospy.is_shutdown():
                raise

    def on_pose(self, message):
        try:
            value = json.loads(message.data)
            self.target = Pose(float(value["height"]), float(value["roll"]), float(value["pitch"]), 0.0)
            self.last_pose_at = time.monotonic()
            self.pose_stale = False
        except (KeyError, TypeError, ValueError):
            self.target = Pose()
            self.last_pose_at = None
            self.pose_stale = True

    def on_status(self, message):
        was_armed = self.armed
        try:
            status = json.loads(message.data)
            self.armed = status.get("state") == "POSTURE_ARMED" and bool(status.get("posture_armed"))
        except (TypeError, ValueError):
            self.armed = False
        if not self.armed:
            self.pose_mode_sent = False
            if was_armed:
                self.send_neutral()

    def on_neutral(self, message):
        if message.data:
            self.target = Pose()
            self.pose_stale = True
            self.send_neutral()

    def send_neutral(self):
        if self.transmit_enabled:
            for code, value in self.limiter.neutral().items():
                if code in self.commissioned_codes:
                    self.publish_simple(code, value)

    def tick(self, _event):
        now = time.monotonic()
        dt = now - self.last_tick
        self.last_tick = now
        if not self.armed or not self.transmit_enabled:
            self.limiter.neutral()
            return
        if self.last_pose_at is None or now - self.last_pose_at > self.intent_timeout:
            self.target = Pose()
            if not self.pose_stale:
                self.send_neutral()
            self.pose_stale = True
            self.pose_mode_sent = False
            return
        heartbeat_period = 1.0 / float(rospy.get_param("/emotion_bot/hardware/posture/heartbeat_rate", 4.0))
        if now - self.last_heartbeat >= heartbeat_period:
            self.publish_simple(self.heartbeat_code)
            self.last_heartbeat = now
            return
        if not self.pose_mode_sent:
            self.publish_simple(self.pose_mode_code)
            self.pose_mode_sent = True
            return
        for code, value in self.limiter.update(self.target, dt, True).items():
            if code in self.commissioned_codes:
                self.publish_simple(code, value)

    def shutdown(self):
        if self.transmit_enabled:
            self.send_neutral()


def main():
    rospy.init_node("posture_bridge")
    PostureBridge()
    rospy.spin()


if __name__ == "__main__":
    main()
