#!/usr/bin/env python3
"""Receive authenticated Retroid observations from the ROS-less motion host."""

import json
import socket
import threading
import time

import rospy
from std_msgs.msg import Bool, String

from emotion_bot_lite3_hw.stop_transport import (
    MAX_FRAME_BYTES, StopRelayError, StopRelaySequenceGate, decode_frame, load_key,
)
from emotion_bot_lite3_hw.shared_memory import SharedTelemetry, encode_safety_record


class StopRelayReceiver:
    def __init__(self):
        network = "/emotion_bot/hardware/network"
        self.bind_ip = rospy.get_param(network + "/stop_relay_bind_ip", "192.168.1.103")
        self.port = int(rospy.get_param(network + "/stop_relay_port", 43910))
        self.source_ip = rospy.get_param(network + "/motion_host_ip", "192.168.1.120")
        self.source_port = int(rospy.get_param(network + "/stop_relay_source_port", 43911))
        key_path = rospy.get_param(network + "/stop_relay_key_file", "/etc/emotion-bot/stop-relay.key")
        self.key = load_key(key_path)
        self.timeout = float(rospy.get_param("/emotion_bot/hardware/safety/retroid_timeout", 0.75))
        self.stop_hold = float(rospy.get_param("/emotion_bot/hardware/safety/stop_hold", 2.0))
        self.gate = StopRelaySequenceGate()
        self.last_receive = None
        self.last_frame = None
        self.stop_until = 0.0
        self.lock = threading.Lock()
        shared_path = rospy.get_param(
            "~shared_memory_path", "/dev/shm/emotion_bot_lite3_safety_state"
        )
        self.shared = SharedTelemetry(shared_path, create=True)
        self.stop_pub = rospy.Publisher("/emotion_bot/hardware/retroid_stop", Bool, queue_size=20, latch=True)
        self.status_pub = rospy.Publisher("/emotion_bot/hardware/retroid", String, queue_size=20, latch=True)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.bind_ip, self.port))
        self.sock.settimeout(0.1)
        rospy.on_shutdown(self.close)
        rospy.Timer(rospy.Duration(0.05), self.publish)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass
        self.shared.close()

    def publish(self, _event):
        now = time.monotonic()
        with self.lock:
            age = None if self.last_receive is None else now - self.last_receive
            frame = self.last_frame
            stop = now <= self.stop_until
        link_fresh = age is not None and age <= self.timeout
        observed_fresh = link_fresh and frame is not None and frame.observed_fresh
        axes_zero = observed_fresh and frame.axes_zero
        self.shared.write(time.monotonic_ns(), encode_safety_record(
            stop, observed_fresh, axes_zero, self.gate.sequence,
        ))
        self.stop_pub.publish(Bool(data=stop))
        self.status_pub.publish(String(data=json.dumps({
            "stop": stop,
            "axes_zero": axes_zero,
            "fresh": observed_fresh,
            "relay_fresh": link_fresh,
            "sequence": self.gate.sequence,
        }, sort_keys=True)))

    def run(self):
        while not rospy.is_shutdown():
            try:
                encoded, address = self.sock.recvfrom(MAX_FRAME_BYTES + 1)
                if address != (self.source_ip, self.source_port):
                    raise StopRelayError("unexpected STOP relay source")
                frame = decode_frame(self.key, encoded)
                now = time.monotonic()
                with self.lock:
                    stale = self.last_receive is None or now - self.last_receive > self.timeout
                    if not self.gate.accept(frame, stale):
                        raise StopRelayError("replayed or conflicting STOP relay frame")
                    self.last_receive = now
                    self.last_frame = frame
                    if frame.stop:
                        self.stop_until = max(self.stop_until, now + self.stop_hold)
            except socket.timeout:
                continue
            except (OSError, StopRelayError) as exc:
                rospy.logwarn_throttle(2.0, "Rejected STOP relay frame: %s", exc)


def main():
    rospy.init_node("stop_relay_receiver")
    StopRelayReceiver().run()


if __name__ == "__main__":
    main()
