#!/usr/bin/env python3
"""ROS wrapper for exclusive hardware supervisor states and arming services."""

import json
import os
import time

import rospy
from sensor_msgs.msg import Imu, JointState
from std_msgs.msg import Bool, String
from std_srvs.srv import SetBool, SetBoolResponse, Trigger, TriggerResponse

from emotion_bot_lite3_hw.msg import DirectJointFeedback, DirectJointSafety, RobotState
from emotion_bot_lite3_hw.feedback import valid_imu_feedback, valid_joint_feedback
from emotion_bot_lite3_hw.supervisor import Gates, State, Supervisor


class SafetySupervisor:
    def __init__(self):
        self.supervisor = Supervisor(
            rospy.get_param("/emotion_bot/hardware/safety/transmit_enabled", False),
            rospy.get_param("/emotion_bot/hardware/safety/dynamic_actions_enabled", False),
        )
        self.ownership_marker = rospy.get_param(
            "/emotion_bot/hardware/safety/ownership_marker",
            "/dev/shm/emotion_bot_lite3_sdk_ownership",
        )
        self.supervisor.restore_after_crash(os.path.exists(self.ownership_marker))
        self.gates = Gates()
        self.last_robot_state = None
        self.last_telemetry_message = None
        self.last_retroid_message = None
        self.last_joint_feedback = None
        self.last_imu_feedback = None
        self.last_direct_feedback = None
        self.direct_feedback = None
        self.standing_since = None
        self.robot = None
        self.stop_active = False
        self.selected_stomp_leg = "front_left"
        self.cooldown_until = {"hop": 0.0, "stomp": 0.0}
        self.sdk_released = False
        self.status_pub = rospy.Publisher("/emotion_bot/hardware/status", String, queue_size=10, latch=True)
        self.direct_safety_pub = rospy.Publisher(
            "/emotion_bot/hardware/direct_joint/safety", DirectJointSafety, queue_size=20, latch=True
        )
        self.neutral_pub = rospy.Publisher("/emotion_bot/hardware/neutral_requested", Bool, queue_size=10, latch=True)
        rospy.Subscriber("/emotion_bot/hardware/robot_state", RobotState, self.on_robot, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/telemetry_health", String, self.on_telemetry, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/link", String, self.on_link, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/retroid", String, self.on_retroid, queue_size=20)
        rospy.Subscriber("/emotion_bot/hardware/retroid_stop", Bool, self.on_stop, queue_size=20)
        rospy.Subscriber("/joint_states", JointState, self.on_joint_feedback, queue_size=20)
        rospy.Subscriber("/imu/data", Imu, self.on_imu_feedback, queue_size=20)
        rospy.Subscriber(
            "/emotion_bot/hardware/direct_joint/feedback", DirectJointFeedback,
            self.on_direct_feedback, queue_size=100,
        )
        rospy.Subscriber("/emotion_bot/hardware/action_request", String, self.on_action_request, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/action_phase", String, self.on_action_phase, queue_size=20)
        rospy.Service("/emotion_bot/hardware/set_armed", SetBool, self.set_armed)
        rospy.Service("/emotion_bot/hardware/set_dynamic_actions_enabled", SetBool, self.set_dynamic)
        rospy.Service("/emotion_bot/hardware/neutral", Trigger, self.neutral)
        rospy.Timer(rospy.Duration(0.05), self.tick)
        rospy.on_shutdown(self.shutdown)

    def on_robot(self, message):
        now = time.monotonic()
        self.robot = message
        self.last_robot_state = now
        if message.robot_basic_state == 6:
            if self.standing_since is None:
                self.standing_since = now
        else:
            self.standing_since = None
        maximum_roll = float(rospy.get_param("/emotion_bot/hardware/safety/maximum_abs_roll_deg", 10.0))
        maximum_pitch = float(rospy.get_param("/emotion_bot/hardware/safety/maximum_abs_pitch_deg", 10.0))
        if message.robot_basic_state == 8:
            self.supervisor.immediate_fault("robot protection state")
        elif message.error_flags:
            self.supervisor.immediate_fault("robot error flags")
        elif abs(message.roll_deg) > maximum_roll or abs(message.pitch_deg) > maximum_pitch:
            self.supervisor.immediate_fault("excessive attitude")

    def on_telemetry(self, message):
        self.last_telemetry_message = time.monotonic()
        try:
            value = json.loads(message.data)
            self.gates.telemetry_fresh = bool(value.get("fresh"))
            self.gates.sdk_layout_compatible = bool(value.get("sdk_layout_compatible"))
            high_rate_fresh = bool(value.get("high_rate_fresh"))
            self.gates.sdk_joints_healthy = high_rate_fresh and bool(value.get("joints_healthy"))
            self.gates.sdk_imu_healthy = high_rate_fresh and bool(value.get("imu_healthy"))
            if value.get("permanent_mismatch"):
                self.supervisor.immediate_fault("SDK telemetry layout mismatch")
        except (TypeError, ValueError):
            self.gates.telemetry_fresh = False
            self.gates.sdk_joints_healthy = False
            self.gates.sdk_imu_healthy = False

    def on_joint_feedback(self, message):
        healthy = valid_joint_feedback(message.name, message.position)
        self.gates.joints_healthy = healthy
        self.last_joint_feedback = time.monotonic() if healthy else None

    def on_imu_feedback(self, message):
        healthy = valid_imu_feedback(
            (message.orientation.x, message.orientation.y, message.orientation.z, message.orientation.w),
            (message.angular_velocity.x, message.angular_velocity.y, message.angular_velocity.z),
            (message.linear_acceleration.x, message.linear_acceleration.y, message.linear_acceleration.z),
        )
        self.gates.imu_healthy = healthy
        self.last_imu_feedback = time.monotonic() if healthy else None

    def on_direct_feedback(self, message):
        self.direct_feedback = message
        self.last_direct_feedback = time.monotonic()

    def on_link(self, message):
        try:
            fresh = bool(json.loads(message.data).get("fresh"))
        except (TypeError, ValueError):
            fresh = False
        if self.gates.ai_link_fresh and not fresh:
            self.supervisor.ai_link_lost()
            self.neutral_pub.publish(Bool(data=True))
        self.gates.ai_link_fresh = fresh

    def on_retroid(self, message):
        self.last_retroid_message = time.monotonic()
        try:
            value = json.loads(message.data)
            self.gates.retroid_axes_zero = bool(value.get("axes_zero")) and bool(value.get("fresh"))
        except (TypeError, ValueError):
            self.gates.retroid_axes_zero = False

    def on_stop(self, message):
        self.stop_active = bool(message.data)
        if self.stop_active:
            self.supervisor.immediate_fault("Retroid STOP observed")
            self.neutral_pub.publish(Bool(data=True))

    def on_action_request(self, message):
        try:
            request = json.loads(message.data)
            action = request["action"]
            leg = request.get("leg", "none")
        except (KeyError, TypeError, ValueError):
            return
        if self.supervisor.request_action(action, self.gates):
            if action == "stomp":
                self.selected_stomp_leg = leg
            cooldown = float(rospy.get_param("/emotion_bot/hardware/actions/%s_cooldown" % action, 0.0))
            self.cooldown_until[action] = time.monotonic() + cooldown
            self.neutral_pub.publish(Bool(data=True))

    def on_action_phase(self, message):
        try:
            phase = json.loads(message.data).get("phase")
        except (TypeError, ValueError):
            return
        if phase == "posture_neutral":
            self.supervisor.posture_neutral()
        elif phase == "sdk_acquired":
            self.standing_since = None
            self.sdk_released = False
            self.supervisor.sdk_acquired()
        elif phase == "finished":
            self.supervisor.action_finished()
        elif phase == "released":
            self.sdk_released = True
            self.supervisor.sdk_released(self.gates.stable_state_6)
        elif phase == "fault":
            self.supervisor.immediate_fault("action controller failure")

    def set_armed(self, request):
        accepted = self.supervisor.set_armed(request.data, self.gates)
        if not request.data:
            self.neutral_pub.publish(Bool(data=True))
        return SetBoolResponse(success=accepted, message=self.supervisor.state.value)

    def set_dynamic(self, request):
        accepted = self.supervisor.set_dynamic(request.data, self.gates)
        return SetBoolResponse(success=accepted, message=self.supervisor.state.value)

    def neutral(self, _request):
        self.supervisor.set_dynamic(False, self.gates)
        self.supervisor.set_armed(False, self.gates)
        self.neutral_pub.publish(Bool(data=True))
        return TriggerResponse(success=True, message=self.supervisor.state.value)

    def tick(self, _event):
        now = time.monotonic()
        telemetry_timeout = float(rospy.get_param("/emotion_bot/hardware/safety/telemetry_timeout", 0.25))
        retroid_timeout = float(rospy.get_param("/emotion_bot/hardware/safety/retroid_timeout", 0.75))
        if self.last_telemetry_message is None or now - self.last_telemetry_message > max(0.5, 2.0 * telemetry_timeout):
            self.gates.telemetry_fresh = False
            self.gates.sdk_joints_healthy = False
            self.gates.sdk_imu_healthy = False
        feedback_timeout = float(rospy.get_param("/emotion_bot/hardware/safety/vendor_feedback_timeout", 0.1))
        if self.last_joint_feedback is None or now - self.last_joint_feedback > feedback_timeout:
            self.gates.joints_healthy = False
        if self.last_imu_feedback is None or now - self.last_imu_feedback > feedback_timeout:
            self.gates.imu_healthy = False
        if self.last_retroid_message is None or now - self.last_retroid_message > retroid_timeout:
            self.gates.retroid_axes_zero = False
        stable_time = float(rospy.get_param("/emotion_bot/hardware/safety/stable_stand_time", 2.0))
        self.gates.stable_state_6 = self.standing_since is not None and now - self.standing_since >= stable_time
        self.gates.stop_preemption_verified = bool(rospy.get_param("/emotion_bot/hardware/safety/stop_preemption_verified", False))
        trajectories = rospy.get_param("/emotion_bot/hardware/actions/hop_trajectory", [])
        left = rospy.get_param("/emotion_bot/hardware/actions/stomp_left_trajectory", [])
        right = rospy.get_param("/emotion_bot/hardware/actions/stomp_right_trajectory", [])
        self.gates.trajectories_commissioned = bool(trajectories and left and right)
        if self.supervisor.state in (State.POSTURE_ARMED, State.ACTION_PENDING) and not self.gates.telemetry_fresh:
            self.supervisor.set_armed(False, self.gates)
            self.neutral_pub.publish(Bool(data=True))
        if self.supervisor.state in (State.POSTURE_ARMED, State.ACTION_PENDING) and not self.gates.retroid_axes_zero:
            self.supervisor.set_armed(False, self.gates)
            self.neutral_pub.publish(Bool(data=True))
        if self.supervisor.state in (State.SDK_TAKEOVER, State.ACTION_ACTIVE, State.RECOVERY) and not self.gates.telemetry_fresh:
            self.supervisor.immediate_fault("telemetry stale during action")
        if self.supervisor.state in (State.SDK_TAKEOVER, State.ACTION_ACTIVE, State.RECOVERY) and not self.gates.retroid_axes_zero:
            self.supervisor.immediate_fault("Retroid observation stale or axes nonzero during action")
        if self.supervisor.state == State.RECOVERY and self.sdk_released and self.gates.stable_state_6:
            self.supervisor.recovered(True)
        status = {
            "schema_version": "1.0",
            "state": self.supervisor.state.value,
            "link_fresh": self.gates.ai_link_fresh,
            "telemetry_fresh": self.gates.telemetry_fresh,
            "joint_feedback_healthy": self.gates.joints_healthy,
            "imu_feedback_healthy": self.gates.imu_healthy,
            "retroid_fresh": self.last_retroid_message is not None and now - self.last_retroid_message <= retroid_timeout,
            "retroid_axes_zero": self.gates.retroid_axes_zero,
            "robot_basic_state": None if self.robot is None else self.robot.robot_basic_state,
            "stable_stand": self.gates.stable_state_6,
            "posture_armed": self.supervisor.state == State.POSTURE_ARMED,
            "dynamic_armed": self.supervisor.dynamic_requested and self.gates.dynamic_ready(),
            "current_action": self.supervisor.current_action,
            "selected_stomp_leg": self.selected_stomp_leg,
            "cooldowns": {name: max(0.0, deadline - now) for name, deadline in self.cooldown_until.items()},
            "sdk_layout_compatible": self.gates.sdk_layout_compatible,
            "sdk_feedback_healthy": self.gates.sdk_joints_healthy and self.gates.sdk_imu_healthy,
            "stop": self.stop_active,
            "fault_reason": self.supervisor.fault_reason,
            "release_required": self.supervisor.release_required,
        }
        self.status_pub.publish(String(data=json.dumps(status, sort_keys=True)))
        direct_timeout = float(rospy.get_param(
            "/emotion_bot/hardware/direct_joint/feedback_timeout", 0.05
        ))
        direct_fresh = (
            self.last_direct_feedback is not None
            and now - self.last_direct_feedback <= direct_timeout
        )
        battery = 0.0 if self.robot is None else float(self.robot.battery_level)
        battery_percent = battery * 100.0 if 0.0 <= battery <= 1.0 else battery
        minimum_battery = float(rospy.get_param(
            "/emotion_bot/hardware/direct_joint/minimum_battery", 25.0
        ))
        contacts_healthy = bool(
            direct_fresh and self.direct_feedback is not None
            and self.direct_feedback.contact_feedback_available
            and self.direct_feedback.all_contacts_healthy
        )
        require_contact_feedback = bool(rospy.get_param(
            "/emotion_bot/hardware/direct_joint/require_contact_feedback", True
        ))
        operator_planted = bool(rospy.get_param(
            "/emotion_bot/hardware/direct_joint/operator_planted_confirmed", False
        ))
        planted_gate = bool(
            contacts_healthy if require_contact_feedback
            else operator_planted and direct_fresh and self.gates.stable_state_6
        )
        direct_marker = rospy.get_param(
            "/emotion_bot/hardware/direct_joint/ownership_marker",
            "/dev/shm/emotion_bot_lite3_direct_joint.owned",
        )
        exclusive_clear = (
            self.supervisor.state == State.DISARMED
            and not os.path.exists(self.ownership_marker)
            and not os.path.exists(direct_marker)
        )
        attitude_ok = bool(
            self.robot is not None
            and abs(self.robot.roll_deg) <= float(rospy.get_param(
                "/emotion_bot/hardware/safety/maximum_abs_roll_deg", 10.0
            ))
            and abs(self.robot.pitch_deg) <= float(rospy.get_param(
                "/emotion_bot/hardware/safety/maximum_abs_pitch_deg", 10.0
            ))
        )
        motion_safe = bool(
            self.robot is not None
            and self.robot.robot_basic_state == 6
            and self.robot.error_flags == 0
            and battery_percent >= minimum_battery
            and self.gates.telemetry_fresh
            and self.gates.joints_healthy
            and self.gates.imu_healthy
            and self.gates.sdk_layout_compatible
            and self.gates.sdk_joints_healthy
            and self.gates.sdk_imu_healthy
            and direct_fresh
            and planted_gate
            and self.last_retroid_message is not None
            and now - self.last_retroid_message <= retroid_timeout
            and self.gates.retroid_axes_zero
            and not self.stop_active
            and self.gates.stable_state_6
            and attitude_ok
        )
        ready = motion_safe and self.gates.ai_link_fresh and exclusive_clear
        failures = []
        if battery_percent < minimum_battery:
            failures.append("battery below %.1f%%" % minimum_battery)
        if not planted_gate:
            failures.append(
                "four-contact sensor gate unavailable or operator planted confirmation absent"
            )
        if not self.gates.sdk_layout_compatible:
            failures.append("SDK layout not reviewed")
        if not direct_fresh:
            failures.append("direct feedback stale")
        if self.stop_active:
            failures.append("STOP active")
        if not self.gates.retroid_axes_zero:
            failures.append("Retroid input active or stale")
        if not self.gates.ai_link_fresh:
            failures.append("EmotionBot link stale")
        if not exclusive_clear:
            failures.append("another hardware path owns control")
        direct = DirectJointSafety()
        direct.header.stamp = rospy.Time.now()
        direct.ready = ready
        direct.motion_safe = motion_safe
        direct.exclusive_path_clear = exclusive_clear
        direct.link_fresh = self.gates.ai_link_fresh
        direct.telemetry_fresh = self.gates.telemetry_fresh
        direct.sdk_layout_compatible = self.gates.sdk_layout_compatible
        direct.sdk_feedback_healthy = self.gates.sdk_joints_healthy and self.gates.sdk_imu_healthy
        direct.vendor_joint_healthy = self.gates.joints_healthy
        direct.vendor_imu_healthy = self.gates.imu_healthy
        direct.retroid_fresh = self.last_retroid_message is not None and now - self.last_retroid_message <= retroid_timeout
        direct.retroid_axes_zero = self.gates.retroid_axes_zero
        direct.stop = self.stop_active
        direct.stable_stand = self.gates.stable_state_6
        direct.contacts_healthy = contacts_healthy
        direct.operator_planted_confirmed = operator_planted
        direct.planted_gate_satisfied = planted_gate
        direct.robot_basic_state = 0 if self.robot is None else self.robot.robot_basic_state
        direct.battery_level = battery_percent
        direct.error_flags = 0 if self.robot is None else self.robot.error_flags
        direct.roll_deg = 0.0 if self.robot is None else self.robot.roll_deg
        direct.pitch_deg = 0.0 if self.robot is None else self.robot.pitch_deg
        direct.yaw_deg = 0.0 if self.robot is None else self.robot.yaw_deg
        direct.reason = "; ".join(failures)
        self.direct_safety_pub.publish(direct)

    def shutdown(self):
        self.neutral_pub.publish(Bool(data=True))


def main():
    rospy.init_node("safety_supervisor")
    SafetySupervisor()
    rospy.spin()


if __name__ == "__main__":
    main()
