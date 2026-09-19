#!/usr/bin/env python3
"""Loopback-only direct-joint ownership and release integration test.

The target socket is bound to 127.0.0.1.  This test must never be pointed at a
robot: it verifies exact MotionSDK command codes, lease exclusion, graceful
neutral release, and the independent crash watchdog without hardware access.
"""

import fcntl
import os
import signal
import socket
import struct
import subprocess
import tempfile
import threading
import time


ACQUIRE_SDK = 0x0114
RELEASE_ROBOT = 0x0113
JOINT_COMMAND = 0x0111
NAMES = [
    "FL_HipX", "FL_HipY", "FL_Knee",
    "FR_HipX", "FR_HipY", "FR_Knee",
    "HL_HipX", "HL_HipY", "HL_Knee",
    "HR_HipX", "HR_HipY", "HR_Knee",
]
ANCHOR = [0.0, -0.75, 1.40] * 4


def wait_for_port(port, timeout=8.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("temporary ROS master did not start")


def receive_code(listener, expected, timeout=4.0):
    deadline = time.monotonic() + timeout
    observed = []
    while time.monotonic() < deadline:
        listener.settimeout(max(0.01, deadline - time.monotonic()))
        try:
            payload, _ = listener.recvfrom(4096)
        except socket.timeout:
            break
        if len(payload) < 12:
            continue
        code, value_or_size, type_and_count = struct.unpack_from("<III", payload)
        observed.append(code)
        if code == JOINT_COMMAND:
            if len(payload) != 252 or value_or_size != 240 or type_and_count != 1:
                raise AssertionError("malformed direct-joint command packet")
            torques = [struct.unpack_from("<f", payload, 12 + 20 * i + 8)[0] for i in range(12)]
            if any(abs(value) > 1e-9 for value in torques):
                raise AssertionError("feedforward torque must remain exactly zero")
        if code == expected:
            return observed
    raise AssertionError("did not observe code 0x{:04x}; saw {}".format(expected, observed))


def drain(listener):
    listener.setblocking(False)
    try:
        while True:
            listener.recvfrom(4096)
    except BlockingIOError:
        pass
    finally:
        listener.setblocking(True)


def assert_no_packet(listener, timeout=0.5):
    listener.settimeout(timeout)
    try:
        payload, _ = listener.recvfrom(4096)
    except socket.timeout:
        return
    raise AssertionError("fail-closed bridge emitted {} UDP bytes".format(len(payload)))


def wait_absent(path, timeout=2.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not os.path.exists(path):
            return
        time.sleep(0.02)
    raise AssertionError("ownership marker remained after release: " + path)


def wait_lock_available(path, timeout=2.0):
    """Wait until the crash watchdog has released its inherited process lease."""
    deadline = time.monotonic() + timeout
    with open(path, "r+") as lock_file:
        while time.monotonic() < deadline:
            try:
                fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                time.sleep(0.02)
                continue
            fcntl.flock(lock_file, fcntl.LOCK_UN)
            return
    raise AssertionError("ownership lease remained after watchdog release: " + path)


def main():
    ros_port = 11329
    env = os.environ.copy()
    env["ROS_MASTER_URI"] = "http://127.0.0.1:{}".format(ros_port)
    env["ROS_HOSTNAME"] = "127.0.0.1"
    os.environ.update(env)

    roscore = subprocess.Popen(
        ["roscore", "-p", str(ros_port)], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    bridges = []
    stop_publish = threading.Event()
    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.bind(("127.0.0.1", 0))
    target_port = listener.getsockname()[1]

    try:
        wait_for_port(ros_port)
        import rospy
        from emotion_bot_lite3_hw.msg import (
            DirectJointCommand, DirectJointFeedback, DirectJointSafety)

        rospy.init_node("direct_bridge_loopback_test", anonymous=True, disable_signals=True)
        command_pub = rospy.Publisher(
            "/emotion_bot/hardware/direct_joint/command", DirectJointCommand, queue_size=2)
        feedback_pub = rospy.Publisher(
            "/emotion_bot/hardware/direct_joint/feedback", DirectJointFeedback, queue_size=2)
        safety_pub = rospy.Publisher(
            "/emotion_bot/hardware/direct_joint/safety", DirectJointSafety, queue_size=2)

        command = DirectJointCommand()
        command.generation = 1
        command.emotion = "neutral"
        command.enable = True
        command.release = False
        command.position = ANCHOR
        command.velocity = [0.0] * 12
        command.acceleration = [0.0] * 12
        command.kp = [30.0] * 12
        command.kd = [0.7] * 12

        feedback = DirectJointFeedback()
        feedback.name = NAMES
        feedback.position = ANCHOR
        feedback.velocity = [0.0] * 12
        feedback.effort = [0.0] * 12
        feedback.temperature = [30.0] * 12
        feedback.imu = [0.0] * 9
        feedback.contact_force = [100.0] * 12
        feedback.contact_feedback_available = True
        feedback.all_contacts_healthy = True

        safety = DirectJointSafety()
        safety.ready = True
        safety.motion_safe = True
        safety.exclusive_path_clear = True
        safety.link_fresh = True
        safety.telemetry_fresh = True
        safety.sdk_layout_compatible = True
        safety.sdk_feedback_healthy = True
        safety.vendor_joint_healthy = True
        safety.vendor_imu_healthy = True
        safety.retroid_fresh = True
        safety.retroid_axes_zero = True
        safety.stop = False
        safety.stable_stand = True
        safety.contacts_healthy = True
        safety.operator_planted_confirmed = False
        safety.planted_gate_satisfied = True
        safety.robot_basic_state = 6
        safety.battery_level = 80.0

        def publish_inputs():
            rate = rospy.Rate(100)
            while not stop_publish.is_set() and not rospy.is_shutdown():
                stamp = rospy.Time.now()
                command.header.stamp = stamp
                feedback.header.stamp = stamp
                feedback.receive_monotonic_ns = time.monotonic_ns()
                feedback.sdk_tick += 1
                safety.header.stamp = stamp
                command_pub.publish(command)
                feedback_pub.publish(feedback)
                safety_pub.publish(safety)
                rate.sleep()

        publisher = threading.Thread(target=publish_inputs, daemon=True)
        publisher.start()

        with tempfile.TemporaryDirectory(prefix="lite3-direct-test-") as tempdir:
            lock_path = os.path.join(tempdir, "sender.lock")
            marker_path = os.path.join(tempdir, "sender.owned")
            executable = (
                "/workspaces/lite3-noetic/hardware-ws/devel/lib/"
                "emotion_bot_lite3_hw/motion_sdk_bridge")

            def start_bridge(name, takeover_transition_commissioned=True):
                process = subprocess.Popen([
                    executable,
                    "__name:=" + name,
                    "_transmit_enabled:=true",
                    "_takeover_transition_commissioned:={}".format(
                        str(takeover_transition_commissioned).lower()),
                    "_target_ip:=127.0.0.1",
                    "_target_port:={}".format(target_port),
                    "_ownership_lock:=" + lock_path,
                    "_ownership_marker:=" + marker_path,
                    "_command_timeout:=0.20",
                    "_feedback_timeout:=0.20",
                    "_safety_timeout:=0.30",
                    "_maximum_send_gap:=0.05",
                    "_neutral_return_time:=0.10",
                    "_neutral_hold_time:=0.05",
                ], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                bridges.append(process)
                return process

            blocked = start_bridge(
                "direct_bridge_uncommissioned_test",
                takeover_transition_commissioned=False)
            time.sleep(0.25)
            drain(listener)
            assert_no_packet(listener)
            blocked.terminate()
            blocked.wait(timeout=2.0)
            if blocked.returncode != 0:
                raise AssertionError(
                    "uncommissioned bridge returned {}".format(blocked.returncode))

            first = start_bridge("direct_bridge_graceful_test")
            sequence = receive_code(listener, ACQUIRE_SDK)
            if JOINT_COMMAND not in sequence:
                raise AssertionError("measured hold was not sent before acquire")
            receive_code(listener, JOINT_COMMAND)

            with open(lock_path, "r+") as lock_file:
                try:
                    fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    pass
                else:
                    raise AssertionError("primary sender did not hold its process lease")

            contender = start_bridge("direct_bridge_lease_contender")
            output, _ = contender.communicate(timeout=3.0)
            if contender.returncode != 2:
                raise AssertionError(
                    "second sender did not fail the exclusive lease: rc={} output={!r}".format(
                        contender.returncode, output))

            command.release = True
            receive_code(listener, RELEASE_ROBOT, timeout=3.0)
            first.wait(timeout=3.0)
            if first.returncode != 0:
                raise AssertionError("graceful release bridge returned {}".format(first.returncode))
            wait_absent(marker_path)

            command.release = False
            command.generation += 1
            drain(listener)
            crashed = start_bridge("direct_bridge_watchdog_test")
            receive_code(listener, ACQUIRE_SDK)
            deadline = time.monotonic() + 2.0
            while not os.path.exists(marker_path) and time.monotonic() < deadline:
                time.sleep(0.01)
            if not os.path.exists(marker_path):
                raise AssertionError("bridge did not persist the ownership marker")

            os.kill(crashed.pid, signal.SIGKILL)
            crashed.wait(timeout=2.0)
            receive_code(listener, RELEASE_ROBOT, timeout=2.0)
            wait_absent(marker_path)
            wait_lock_available(lock_path)

        print(
            "PASS: loopback fail-closed gate/acquire/lease/graceful release/"
            "crash-watchdog release")
        return 0
    finally:
        stop_publish.set()
        for process in bridges:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    process.kill()
        listener.close()
        roscore.terminate()
        try:
            roscore.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            roscore.kill()


if __name__ == "__main__":
    raise SystemExit(main())
