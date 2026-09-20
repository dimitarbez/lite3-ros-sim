#!/usr/bin/env python3
"""Loopback-only receiver for validated emotion-state NDJSON."""

import json
import socket
import threading
import time

import rospy
from std_msgs.msg import String

from emotion_bot_lite3_hw.transport import MAX_FRAME_BYTES, SequenceGate, TransportError, decode_frame
from emotion_bot_lite3_hw.shared_memory import SharedTelemetry, encode_emotion_record


class EmotionReceiver:
    def __init__(self):
        self.host = rospy.get_param("/emotion_bot/hardware/network/emotion_bind_host", "127.0.0.1")
        self.port = int(rospy.get_param("/emotion_bot/hardware/network/emotion_port", 8767))
        if self.host not in ("127.0.0.1", "::1", "localhost"):
            raise RuntimeError("emotion receiver must bind to loopback")
        self.gate = SequenceGate()
        self.connection_lock = threading.Lock()
        self.connection_generation = 0
        self.last_receive = None
        shared_path = rospy.get_param(
            "~shared_memory_path", "/dev/shm/emotion_bot_lite3_emotion_state"
        )
        self.shared = SharedTelemetry(shared_path, create=True)
        self.pub = rospy.Publisher("/emotion_bot/hardware/emotion_state", String, queue_size=10, latch=True)
        self.link_pub = rospy.Publisher("/emotion_bot/hardware/link", String, queue_size=10, latch=True)
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.listen(2)
        self.server.settimeout(0.25)
        rospy.on_shutdown(self.close)
        threading.Thread(target=self.accept_loop, daemon=True).start()
        rospy.Timer(rospy.Duration(0.1), self.publish_link)

    def close(self):
        try:
            self.server.close()
        except OSError:
            pass
        self.shared.close()

    def accept_loop(self):
        while not rospy.is_shutdown():
            try:
                connection, address = self.server.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            if address[0] not in ("127.0.0.1", "::1"):
                connection.close()
                continue
            with self.connection_lock:
                self.connection_generation += 1
                generation = self.connection_generation
            threading.Thread(target=self.read_connection, args=(connection, generation), daemon=True).start()

    def read_connection(self, connection, generation):
        buffer = b""
        session_id = None
        sequence = 0
        connection.settimeout(1.0)
        with connection:
            while not rospy.is_shutdown():
                with self.connection_lock:
                    if generation != self.connection_generation:
                        return
                try:
                    chunk = connection.recv(1024)
                except socket.timeout:
                    continue
                except OSError:
                    return
                if not chunk:
                    return
                buffer += chunk
                if len(buffer) > MAX_FRAME_BYTES and b"\n" not in buffer:
                    return
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    frame = raw + b"\n"
                    try:
                        envelope = decode_frame(frame)
                        if session_id is None:
                            session_id = envelope["session_id"]
                        if envelope["session_id"] != session_id or envelope["sequence"] <= sequence:
                            continue
                    except TransportError as exc:
                        rospy.logwarn("Rejected emotion frame: %s", exc)
                        continue
                    if not self.gate.accept(envelope):
                        continue
                    sequence = envelope["sequence"]
                    receive_ns = time.monotonic_ns()
                    self.last_receive = receive_ns * 1e-9
                    self.shared.write(receive_ns, encode_emotion_record(envelope))
                    self.pub.publish(String(data=json.dumps(envelope["payload"], sort_keys=True, separators=(",", ":"))))

    def publish_link(self, _event):
        timeout = float(rospy.get_param("/emotion_bot/hardware/safety/emotion_timeout", 0.75))
        age = None if self.last_receive is None else time.monotonic() - self.last_receive
        self.link_pub.publish(String(data=json.dumps({
            "fresh": age is not None and age <= timeout,
            "age": age,
            "session_id": self.gate.session_id,
            "sequence": self.gate.sequence,
        }, sort_keys=True)))


def main():
    rospy.init_node("emotion_receiver")
    EmotionReceiver()
    rospy.spin()


if __name__ == "__main__":
    main()
