#!/usr/bin/env python3
"""Fail-closed coordinator for a separately installed official MotionSDK sender."""

import json
import os
import select
import subprocess
import threading
import time

import rospy
from std_msgs.msg import String


class ActionController:
    def __init__(self):
        self.sender = rospy.get_param("~sdk_sender_path", "")
        self.active = None
        self.process = None
        self.selected_leg = "none"
        self.abort_requested = False
        self.marker = rospy.get_param(
            "/emotion_bot/hardware/safety/ownership_marker",
            "/dev/shm/emotion_bot_lite3_sdk_ownership",
        )
        self.lock = threading.Lock()
        self.phase_pub = rospy.Publisher("/emotion_bot/hardware/action_phase", String, queue_size=20)
        self.result_pub = rospy.Publisher("/emotion_bot/hardware/action_result", String, queue_size=10)
        rospy.Subscriber("/emotion_bot/hardware/status", String, self.on_status, queue_size=20)

    def emit(self, phase, **values):
        payload = {"schema_version": "1.0", "phase": phase}
        payload.update(values)
        self.phase_pub.publish(String(data=json.dumps(payload, sort_keys=True)))

    def on_status(self, message):
        try:
            status = json.loads(message.data)
        except (TypeError, ValueError):
            return
        state = status.get("state")
        action = status.get("current_action")
        self.selected_leg = status.get("selected_stomp_leg", "none")
        with self.lock:
            active = self.active
            process = self.process
        if state == "FAULT" and process is not None and process.poll() is None:
            with self.lock:
                self.abort_requested = True
            process.terminate()
            return
        if state == "RECOVERY" and status.get("release_required") and active is None and process is None:
            if not self.sender or not os.path.isfile(self.sender) or not os.access(self.sender, os.X_OK):
                self.emit("fault", reason="SDK recovery required but verified sender is unavailable")
                return
            with self.lock:
                self.active = "recovery"
            threading.Thread(target=self.run_recovery, daemon=True).start()
            return
        if state == "ACTION_PENDING" and action and active is None:
            with self.lock:
                self.active = action
            self.emit("posture_neutral")
            return
        if state != "SDK_TAKEOVER" or active is None or process is not None:
            return
        if not self.sender or not os.path.isfile(self.sender) or not os.access(self.sender, os.X_OK):
            self.emit("fault", reason="verified MotionSDK sender is not installed")
            self.result_pub.publish(String(data=json.dumps({"action": active, "success": False})))
            with self.lock:
                self.active = None
            return
        threading.Thread(target=self.run_sender, args=(active, self.selected_leg), daemon=True).start()

    def run_sender(self, action, leg):
        acquired = False
        finished = False
        released = False
        process = None
        success = False
        try:
            with self.lock:
                self.abort_requested = False
            process = subprocess.Popen(
                [self.sender, "--action", action, "--leg", leg,
                 "--config", rospy.get_param("~config_path", "")],
                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1,
            )
            with self.lock:
                self.process = process
            deadline = time.monotonic() + float(rospy.get_param("~action_timeout", 8.0))
            while time.monotonic() < deadline and not rospy.is_shutdown():
                ready, _, _ = select.select([process.stdout], [], [], 0.1)
                if not ready:
                    if process.poll() is not None:
                        break
                    continue
                marker = process.stdout.readline().strip()
                if marker == "SDK_ACQUIRED" and not acquired:
                    acquired = True
                    self.write_marker(action)
                    self.emit("sdk_acquired")
                elif marker == "ACTION_FINISHED" and acquired and not finished:
                    finished = True
                    self.emit("finished")
                elif marker == "ROBOT_RELEASED" and acquired:
                    released = True
                elif not marker and process.poll() is not None:
                    break
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            with self.lock:
                aborted = self.abort_requested
            success = process.returncode == 0 and acquired and finished and released and not aborted
        except OSError:
            success = False
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        if released and process is not None and process.returncode == 0:
            self.clear_marker()
            self.emit("released")
        else:
            self.emit("fault", reason="MotionSDK sender exited without verified release")
        self.result_pub.publish(String(data=json.dumps({"action": action, "success": success})))
        with self.lock:
            self.process = None
            self.active = None

    def run_recovery(self):
        released = False
        process = None
        try:
            process = subprocess.Popen(
                [self.sender, "--recover", "--config", rospy.get_param("~config_path", "")],
                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1,
            )
            with self.lock:
                self.process = process
            deadline = time.monotonic() + 2.0
            while time.monotonic() < deadline:
                ready, _, _ = select.select([process.stdout], [], [], 0.1)
                if ready:
                    marker = process.stdout.readline().strip()
                    if marker == "ROBOT_RELEASED":
                        released = True
                    if not marker and process.poll() is not None:
                        break
                elif process.poll() is not None:
                    break
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        except (OSError, subprocess.TimeoutExpired):
            released = False
        if released and process is not None and process.returncode == 0:
            self.clear_marker()
            self.emit("released")
        else:
            self.emit("fault", reason="crash recovery did not verify SDK release")
        with self.lock:
            self.process = None
            self.active = None

    def write_marker(self, action):
        descriptor = os.open(self.marker, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        try:
            os.write(descriptor, ("pid=%d action=%s\n" % (os.getpid(), action)).encode("ascii"))
            os.fsync(descriptor)
        finally:
            os.close(descriptor)

    def clear_marker(self):
        try:
            os.unlink(self.marker)
        except FileNotFoundError:
            pass


def main():
    rospy.init_node("action_controller")
    ActionController()
    rospy.spin()


if __name__ == "__main__":
    main()
