#!/usr/bin/env python3
"""Publish the ROS-independent MotionSDK runner's read-only status record."""

import json
import time

import rospy
from std_msgs.msg import String

from emotion_bot_lite3_hw.shared_memory import SharedTelemetry


class ExpressionStatusPublisher:
    def __init__(self):
        path = rospy.get_param(
            "~shared_memory_path", "/dev/shm/emotion_bot_lite3_expression_status"
        )
        self.record = None
        try:
            self.record = SharedTelemetry(path)
        except (OSError, ValueError):
            pass
        self.publisher = rospy.Publisher(
            "/emotion_bot/hardware/expression_status", String,
            queue_size=10, latch=True,
        )
        rospy.Timer(rospy.Duration(0.1), self.publish)
        rospy.on_shutdown(self.close)

    def close(self):
        if self.record is not None:
            self.record.close()
            self.record = None

    def publish(self, _event):
        if self.record is None:
            path = rospy.get_param(
                "~shared_memory_path", "/dev/shm/emotion_bot_lite3_expression_status"
            )
            try:
                self.record = SharedTelemetry(path)
            except (OSError, ValueError):
                self.publisher.publish(String(data=json.dumps({
                    "schema_version": "1.0",
                    "requested_emotion": None,
                    "resolved_emotion": None,
                    "resolved_profile": None,
                    "active_emotion": None,
                    "active_profile": None,
                    "fallback_active": False,
                    "fallback_reason": None,
                    "transport_sequence": 0,
                    "state_sequence": 0,
                    "session_id": None,
                    "turn_id": None,
                    "valence": None,
                    "arousal": None,
                    "phase": "not_running",
                    "profile_cycle": 0,
                    "pending_emotion": None,
                    "pending_profile": None,
                    "link_age": None,
                    "sdk_ownership": False,
                    "feedback_paused": False,
                    "last_fault": "runner status unavailable",
                    "release_state": "released",
                }, sort_keys=True)))
                return
        snapshot = self.record.read()
        if snapshot is None:
            return
        _sequence, receive_ns, payload = snapshot
        try:
            value = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            return
        if time.monotonic_ns() - receive_ns > 1_000_000_000:
            if not isinstance(value, dict) or value.get("release_state") != "released":
                value = {
                    "schema_version": "1.0",
                    "requested_emotion": None,
                    "resolved_emotion": None,
                    "resolved_profile": None,
                    "active_emotion": None,
                    "active_profile": None,
                    "fallback_active": False,
                    "fallback_reason": None,
                    "transport_sequence": 0,
                    "state_sequence": 0,
                    "session_id": None,
                    "turn_id": None,
                    "valence": None,
                    "arousal": None,
                    "phase": "not_running",
                    "profile_cycle": 0,
                    "pending_emotion": None,
                    "pending_profile": None,
                    "link_age": None,
                    "sdk_ownership": False,
                    "feedback_paused": False,
                    "last_fault": "runner status stale",
                    "release_state": "unknown",
                }
        if isinstance(value, dict) and value.get("schema_version") == "1.0":
            self.publisher.publish(String(data=json.dumps(value, sort_keys=True)))


def main():
    rospy.init_node("expression_status_publisher")
    ExpressionStatusPublisher()
    rospy.spin()


if __name__ == "__main__":
    main()
