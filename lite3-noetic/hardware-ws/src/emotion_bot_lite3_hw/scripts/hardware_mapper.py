#!/usr/bin/env python3
"""Map remote emotional state into hardware-only pose and one-shot intentions."""

import json
import time

import rospy
from std_msgs.msg import String

from emotion_bot_lite3_hw.mapping import ActionScheduler, NeutralBreather, Pose, load_profiles


class HardwareMapper:
    def __init__(self):
        raw_profiles = rospy.get_param("/emotion_bot/hardware/posture/profiles")
        self.profiles = load_profiles(raw_profiles)
        self.scheduler = ActionScheduler(
            rospy.get_param("/emotion_bot/hardware/actions/hop_cooldown", 15.0),
            rospy.get_param("/emotion_bot/hardware/actions/stomp_cooldown", 10.0),
        )
        breathing = rospy.get_param("/emotion_bot/hardware/posture/neutral_breathing", {})
        self.breathing_enabled = bool(breathing.get("enabled", False))
        self.breather = NeutralBreather(
            entrance_height=float(breathing.get("entrance_height", 1.0)),
            idle_high_height=float(breathing.get("idle_high_height", 0.65)),
            idle_low_height=float(breathing.get("idle_low_height", -0.39)),
            entrance_duration=float(breathing.get("entrance_duration", 0.25)),
            idle_period=float(breathing.get("idle_period", 1.375)),
        )
        self.dynamic_armed = False
        self.posture_armed = False
        self.current_emotion = "neutral"
        self.current_sequence = None
        self.breath_started = None
        self.pose_pub = rospy.Publisher("/emotion_bot/hardware/posture_intent", String, queue_size=10, latch=True)
        self.action_pub = rospy.Publisher("/emotion_bot/hardware/action_request", String, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/status", String, self.on_status, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/emotion_state", String, self.on_emotion, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/action_result", String, self.on_action_result, queue_size=10)
        rate = float(breathing.get("publish_rate", 20.0))
        rospy.Timer(rospy.Duration(1.0 / max(1.0, rate)), self.on_timer)
        self.publish_pose("neutral", self.profiles["neutral"])

    def on_status(self, message):
        try:
            status = json.loads(message.data)
            self.dynamic_armed = bool(status.get("dynamic_armed", False))
            was_armed = self.posture_armed
            self.posture_armed = bool(status.get("posture_armed", False))
            if self.posture_armed and not was_armed and self.current_emotion == "neutral":
                self.breath_started = time.monotonic()
        except (TypeError, ValueError):
            self.dynamic_armed = False
            self.posture_armed = False

    def publish_pose(self, emotion, pose):
        self.pose_pub.publish(String(data=json.dumps({
            "schema_version": "1.0", "emotion": emotion,
            "height": pose.height, "roll": pose.roll, "pitch": pose.pitch, "yaw": 0.0,
        }, sort_keys=True)))

    def on_emotion(self, message):
        try:
            state = json.loads(message.data)
            emotion = state["emotion"]
            pose = self.profiles[emotion]
        except (KeyError, TypeError, ValueError):
            return
        sequence = state.get("sequence")
        if emotion != self.current_emotion or sequence != self.current_sequence:
            self.breath_started = time.monotonic() if emotion == "neutral" else None
        self.current_emotion = emotion
        self.current_sequence = sequence
        self.publish_pose(emotion, pose)
        request = self.scheduler.on_emotion(emotion, time.monotonic(), self.dynamic_armed)
        if request is not None:
            action, leg = request
            self.action_pub.publish(String(data=json.dumps({
                "schema_version": "1.0", "action": action, "leg": leg,
                "emotion_sequence": state.get("sequence", 0),
            }, sort_keys=True)))

    def on_action_result(self, message):
        try:
            result = json.loads(message.data)
            self.scheduler.complete(result["action"], bool(result["success"]))
        except (KeyError, TypeError, ValueError):
            return

    def on_timer(self, _event):
        if not self.breathing_enabled or not self.posture_armed or self.current_emotion != "neutral":
            return
        if self.breath_started is None:
            self.breath_started = time.monotonic()
        height = self.breather.height(time.monotonic() - self.breath_started)
        self.publish_pose("neutral", Pose(height=height))


def main():
    rospy.init_node("hardware_mapper")
    HardwareMapper()
    rospy.spin()


if __name__ == "__main__":
    main()
