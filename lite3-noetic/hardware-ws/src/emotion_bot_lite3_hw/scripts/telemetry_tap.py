#!/usr/bin/env python3
"""Minimal CAP_NET_RAW passive telemetry tap; never transmits packets."""

import json
import os
import socket
import struct
import time

import rospy
from std_msgs.msg import String

from emotion_bot_lite3_hw.feedback import DIRECT_JOINT_NAMES, valid_direct_feedback
from emotion_bot_lite3_hw.msg import DirectJointFeedback, RobotState
from emotion_bot_lite3_hw.protocol import (
    PacketError, SDK_STATE_CODE, decode_sdk_0906, filter_datagram,
    is_sdk_joint_command, parse_command, parse_ethernet_udp,
    parse_robot_state_0901,
)
from emotion_bot_lite3_hw.shared_memory import SharedTelemetry


class TelemetryTap:
    def __init__(self):
        self.interface = rospy.get_param("/emotion_bot/hardware/network/telemetry_interface", "eth0")
        self.source_ip = rospy.get_param("/emotion_bot/hardware/network/motion_host_ip", "192.168.1.120")
        self.port = int(rospy.get_param("/emotion_bot/hardware/network/telemetry_port", 43897))
        self.target_ip = rospy.get_param("/emotion_bot/hardware/network/telemetry_target_ip", "192.168.1.103")
        self.command_port = int(rospy.get_param("/emotion_bot/hardware/network/command_port", 43893))
        self.layout = rospy.get_param("/emotion_bot/hardware/safety/sdk_layout", {})
        self.maximum_temperature = float(rospy.get_param(
            "/emotion_bot/hardware/safety/maximum_joint_temperature_c", 70.0
        ))
        self.direct_publish_rate = float(rospy.get_param(
            "/emotion_bot/hardware/direct_joint/feedback_publish_rate", 100.0
        ))
        if self.direct_publish_rate < 50.0 or self.direct_publish_rate > 250.0:
            raise ValueError("direct feedback_publish_rate must be in [50, 250] Hz")
        self.state_pub = rospy.Publisher("/emotion_bot/hardware/robot_state", RobotState, queue_size=20)
        self.direct_pub = rospy.Publisher(
            "/emotion_bot/hardware/direct_joint/feedback", DirectJointFeedback, queue_size=100
        )
        self.health_pub = rospy.Publisher("/emotion_bot/hardware/telemetry_health", String, queue_size=20, latch=True)
        self.wire_pub = rospy.Publisher("/emotion_bot/hardware/wire_command", String, queue_size=100)
        shm_path = rospy.get_param("~shared_memory_path", "/dev/shm/emotion_bot_lite3_telemetry")
        robot_state_shm_path = rospy.get_param(
            "~robot_state_shared_memory_path", "/dev/shm/emotion_bot_lite3_robot_state"
        )
        self.shared = SharedTelemetry(shm_path, create=True)
        self.robot_state_shared = SharedTelemetry(robot_state_shm_path, create=True)
        self.packet_sequence = 0
        self.wire_sequence = 0
        self.last_0901 = None
        self.last_0906 = None
        self.last_direct_publish = None
        self.sdk_sequence = 0
        self.layout_compatible = False
        self.permanent_mismatch = False
        self.high_rate_healthy = False
        self.sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0003))
        self.sock.bind((self.interface, 0))
        self.sock.settimeout(0.1)
        rospy.on_shutdown(self.close)
        rospy.Timer(rospy.Duration(0.1), self.publish_health)

    def observe_wire_command(self, datagram):
        if not (
            datagram.source_ip == self.target_ip
            and datagram.destination_ip == self.source_ip
            and datagram.destination_port == self.command_port
        ):
            return False
        command = parse_command(datagram.payload)
        # The official SDK emits 0x0111 at 1 kHz. Publishing JSON for every
        # packet on the diagnostic ROS topic can starve the same raw-socket
        # loop that must copy inbound 0x0906 feedback into shared memory.
        # Consume only the exact reviewed 240-byte SDK command here; posture,
        # mode, heartbeat, and STOP observations remain fully inspectable.
        if is_sdk_joint_command(command):
            return True
        signed_value = struct.unpack("<i", struct.pack("<I", command.value_or_size))[0]
        self.wire_sequence += 1
        self.wire_pub.publish(String(data=json.dumps({
            "schema_version": "1.0",
            "sequence": self.wire_sequence,
            "receive_monotonic_ns": time.monotonic_ns(),
            "source_ip": datagram.source_ip,
            "source_port": datagram.source_port,
            "destination_ip": datagram.destination_ip,
            "destination_port": datagram.destination_port,
            "code": command.code,
            "code_hex": "0x%08X" % command.code,
            "value": signed_value,
            "type": command.command_type,
            "body_size": len(command.body),
        }, sort_keys=True)))
        return True

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass
        self.shared.close()
        self.robot_state_shared.close()

    def publish_health(self, _event):
        now = time.monotonic()
        timeout = float(rospy.get_param("/emotion_bot/hardware/safety/telemetry_timeout", 0.25))
        age_0901 = None if self.last_0901 is None else now - self.last_0901
        age_0906 = None if self.last_0906 is None else now - self.last_0906
        self.health_pub.publish(String(data=json.dumps({
            "fresh": age_0901 is not None and age_0901 <= timeout,
            "high_rate_fresh": age_0906 is not None and age_0906 <= timeout,
            "sdk_layout_compatible": self.layout_compatible and not self.permanent_mismatch,
            "permanent_mismatch": self.permanent_mismatch,
            "joints_healthy": self.high_rate_healthy,
            "imu_healthy": self.high_rate_healthy,
        }, sort_keys=True)))

    def run(self):
        while not rospy.is_shutdown():
            command = None
            try:
                frame = self.sock.recv(65535)
                datagram = parse_ethernet_udp(frame)
                if self.observe_wire_command(datagram):
                    continue
                payload = filter_datagram(datagram, self.source_ip, self.port, self.target_ip)
                command = parse_command(payload)
                now = time.monotonic()
                if command.code == SDK_STATE_CODE:
                    # Preserve the exact passive body before attempting a
                    # reviewed decode.  A blank/incorrect layout must keep all
                    # motion gates false, but must not prevent byte-for-byte
                    # commissioning against the deployed firmware.
                    self.shared.write(time.monotonic_ns(), command.body)
                    decoded = decode_sdk_0906(command, self.layout)
                    self.layout_compatible = True
                    positions = tuple(joint[0] for joint in decoded.joints)
                    velocities = tuple(joint[1] for joint in decoded.joints)
                    torques = tuple(joint[2] for joint in decoded.joints)
                    temperatures = tuple(joint[3] for joint in decoded.joints)
                    self.high_rate_healthy = valid_direct_feedback(
                        positions, velocities, temperatures, self.maximum_temperature
                    ) and len(decoded.imu) == 9 and len(decoded.contacts) == 12
                    contact_sign = int(self.layout.get("contact_z_sign", 0))
                    contact_minimum = float(self.layout.get("contact_minimum_z", 0.0))
                    contact_available = bool(self.layout.get("contact_feedback_available", False))
                    contacts_healthy = (
                        contact_available
                        and
                        contact_sign in (-1, 1)
                        and contact_minimum > 0.0
                        and all(
                            contact_sign * decoded.contacts[index] >= contact_minimum
                            for index in (2, 5, 8, 11)
                        )
                    )
                    self.last_0906 = now
                    if (
                        self.last_direct_publish is None
                        or now - self.last_direct_publish >= 1.0 / self.direct_publish_rate
                    ):
                        self.last_direct_publish = now
                        self.sdk_sequence += 1
                        feedback = DirectJointFeedback()
                        feedback.header.stamp = rospy.Time.now()
                        feedback.receive_monotonic_ns = time.monotonic_ns()
                        feedback.sdk_tick = decoded.tick
                        feedback.name = DIRECT_JOINT_NAMES
                        feedback.position = positions
                        feedback.velocity = velocities
                        feedback.effort = torques
                        feedback.temperature = temperatures
                        feedback.imu = decoded.imu
                        feedback.contact_force = decoded.contacts
                        feedback.contact_feedback_available = contact_available
                        feedback.all_contacts_healthy = contacts_healthy
                        self.direct_pub.publish(feedback)
                    continue
                state = parse_robot_state_0901(command)
            except socket.timeout:
                continue
            except (OSError, PacketError, ValueError) as exc:
                if command is not None and command.code == SDK_STATE_CODE and self.layout.get("id"):
                    self.permanent_mismatch = True
                    self.high_rate_healthy = False
                rospy.logwarn_throttle(2.0, "Rejected telemetry packet: %s", exc)
                continue
            self.packet_sequence += 1
            self.last_0901 = now
            self.robot_state_shared.write(time.monotonic_ns(), command.body)
            message = RobotState()
            message.header.stamp = rospy.Time.now()
            message.packet_sequence = self.packet_sequence
            message.robot_basic_state = state.robot_basic_state
            message.robot_gait_state = state.robot_gait_state
            message.robot_motion_state = state.robot_motion_state
            message.battery_level = state.battery_level
            message.error_flags = state.error_flags
            message.zero_position_flag = state.zero_position_flag
            message.roll_deg = state.roll_deg
            message.pitch_deg = state.pitch_deg
            message.yaw_deg = state.yaw_deg
            message.sdk_layout_compatible = self.layout_compatible and not self.permanent_mismatch
            message.high_rate_healthy = self.high_rate_healthy
            self.state_pub.publish(message)


def main():
    rospy.init_node("telemetry_tap")
    TelemetryTap().run()


if __name__ == "__main__":
    main()
