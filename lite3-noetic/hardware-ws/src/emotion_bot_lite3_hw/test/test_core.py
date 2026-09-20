import json
import os
import socket
import struct
import tempfile
import unittest

from emotion_bot_lite3_hw.action import ActionMachine, ActionObservation
from emotion_bot_lite3_hw.direct_pose import (
    HEARTBEAT_CODE,
    HEIGHT_CODE,
    MOVE_MODE_CODE,
    POSE_MODE_CODE,
    YAW_CODE,
    DirectPoseLimits,
    RateLimitedHeight,
    safe_neutral_gate,
)
from emotion_bot_lite3_hw.feedback import (
    DIRECT_POSITION_LIMITS, EXPECTED_JOINT_NAMES, valid_direct_feedback,
    valid_imu_feedback, valid_joint_feedback,
)
from emotion_bot_lite3_hw.mapping import ActionScheduler, EMOTIONS, NeutralBreather, Pose, load_profiles
from emotion_bot_lite3_hw.joint_expression import (
    DIRECT_JOINT_NAMES, ExpressionPlanner, PATTERNS, Quintic, ScalarSample, planted_joint_sample,
    sample_pattern, validate_patterns, validate_planted_symmetry,
)
from emotion_bot_lite3_hw.posture import AxisConfig, PostureLimiter
from emotion_bot_lite3_hw.protocol import (
    JOINT_ANGLE_CODE, JOINT_VELOCITY_CODE, PacketError, decode_sdk_0906,
    filter_datagram, is_sdk_joint_command, parse_command, parse_ethernet_udp,
    parse_joint_vector, parse_robot_state_0901, parse_simple_stop,
)
from emotion_bot_lite3_hw.shared_memory import (
    SharedTelemetry, decode_emotion_record, decode_safety_record,
    encode_emotion_record, encode_safety_record,
)
from emotion_bot_lite3_hw.supervisor import Gates, State, Supervisor
from emotion_bot_lite3_hw.stop_transport import (
    StopRelayError, StopRelayFrame, StopRelaySequenceGate, decode_frame as decode_stop_frame,
    encode_frame as encode_stop_frame,
)
from emotion_bot_lite3_hw.transport import SequenceGate, TransportError, decode_frame


def command(code, body=b"", command_type=1, declared=None):
    size = len(body) if declared is None else declared
    return struct.pack("<III", code, size, command_type) + body


def ethernet_udp(payload, source="192.168.1.120", destination="192.168.1.103", port=43897):
    source_bytes = socket.inet_aton(source)
    destination_bytes = socket.inet_aton(destination)
    udp = struct.pack("!HHHH", 50000, port, len(payload) + 8, 0) + payload
    ip = bytearray(20)
    ip[0] = 0x45
    struct.pack_into("!H", ip, 2, len(ip) + len(udp))
    ip[8] = 64
    ip[9] = 17
    ip[12:16] = source_bytes
    ip[16:20] = destination_bytes
    return b"\x00" * 12 + b"\x08\x00" + bytes(ip) + udp


def state_body():
    body = bytearray(200)
    struct.pack_into("<ii", body, 0, 6, 4)
    struct.pack_into("<ddd", body, 8, 1.0, -2.0, 3.0)
    struct.pack_into("<I", body, 160, 0)
    struct.pack_into("<i", body, 164, 9)
    struct.pack_into("<d", body, 168, 0.74)
    body[181] = 1
    return bytes(body)


class ProtocolTests(unittest.TestCase):
    def test_direct_pose_protocol_constants_match_observed_retroid_commands(self):
        self.assertEqual(HEARTBEAT_CODE, 0x21040001)
        self.assertEqual(HEIGHT_CODE, 0x21010102)
        self.assertEqual(YAW_CODE, 0x21010135)
        self.assertEqual(MOVE_MODE_CODE, 0x21010D06)
        self.assertEqual(POSE_MODE_CODE, 0x21010D05)

    def test_observed_0901_shape_decodes(self):
        datagram = parse_ethernet_udp(ethernet_udp(command(0x0901, state_body())))
        payload = filter_datagram(datagram, "192.168.1.120", 43897)
        state = parse_robot_state_0901(parse_command(payload))
        self.assertEqual(state.robot_basic_state, 6)
        self.assertEqual(state.robot_gait_state, 4)
        self.assertEqual(state.robot_motion_state, 9)
        self.assertAlmostEqual(state.battery_level, 0.74)
        self.assertTrue(state.zero_position_flag)

    def test_deployed_0901_battery_percentage_is_valid(self):
        body = bytearray(state_body())
        struct.pack_into("<d", body, 168, 25.0)
        packet = parse_command(command(0x0901, bytes(body)))
        self.assertEqual(parse_robot_state_0901(packet).battery_level, 25.0)

    def test_malformed_lengths_and_spoofed_source_are_rejected(self):
        with self.assertRaises(PacketError):
            parse_command(command(0x0901, state_body(), declared=199))
        spoofed = parse_ethernet_udp(ethernet_udp(command(0x0901, state_body()), source="192.168.1.121"))
        with self.assertRaises(PacketError):
            filter_datagram(spoofed, "192.168.1.120", 43897)
        truncated = ethernet_udp(command(0x0901, state_body()))[:-3]
        with self.assertRaises(PacketError):
            parse_ethernet_udp(truncated)

    def test_stop_is_exact_and_sdk_layout_is_fail_closed(self):
        stop = parse_command(struct.pack("<III", 0x21020C0E, 0, 0))
        self.assertTrue(parse_simple_stop(stop))
        sdk = parse_command(command(0x0906, bytes(368)))
        with self.assertRaises(PacketError):
            decode_sdk_0906(sdk, {})
        layout = {"id": "deeprcs-2.0.153-reviewed-0906-v1", "deeprcs_version": "2.0.153",
                  "body_size": 368, "tick_offset": 0, "imu_offset": 8,
                  "joint_offset": 44, "contact_offset": 240}
        decoded = decode_sdk_0906(sdk, layout)
        self.assertEqual(len(decoded.joints), 12)
        self.assertEqual(len(decoded.contacts), 12)
        rotated = bytearray(368)
        struct.pack_into("<9f", rotated, 8, 0.0, 0.0, -145.0, 0.0, 0.0, 0.0, 0.0, 0.0, 9.81)
        decoded = decode_sdk_0906(parse_command(command(0x0906, bytes(rotated))), layout)
        self.assertAlmostEqual(decoded.imu[2], -145.0)
        struct.pack_into("<f", rotated, 16, 360.1)
        with self.assertRaises(PacketError):
            decode_sdk_0906(parse_command(command(0x0906, bytes(rotated))), layout)
        overlapping = dict(layout, imu_offset=40)
        with self.assertRaises(PacketError):
            decode_sdk_0906(sdk, overlapping)
        with self.assertRaises(PacketError):
            decode_sdk_0906(parse_command(command(0x0906, bytes(367))), layout)

    def test_only_exact_official_sdk_joint_command_is_high_rate(self):
        self.assertTrue(is_sdk_joint_command(parse_command(command(0x0111, bytes(240)))))
        self.assertFalse(is_sdk_joint_command(parse_command(command(0x0111, bytes(239)))))
        self.assertFalse(is_sdk_joint_command(parse_command(command(0x0112, bytes(240)))))

    def test_documented_joint_vectors_use_twelve_little_endian_doubles(self):
        values = tuple(index / 10.0 for index in range(12))
        angles = parse_joint_vector(
            parse_command(command(JOINT_ANGLE_CODE, struct.pack("<12d", *values))),
            JOINT_ANGLE_CODE,
        )
        velocities = parse_joint_vector(
            parse_command(command(JOINT_VELOCITY_CODE, struct.pack("<12d", *values))),
            JOINT_VELOCITY_CODE,
        )
        self.assertEqual(angles, values)
        self.assertEqual(velocities, values)

    def test_direct_feedback_obeys_manual_joint_and_temperature_envelopes(self):
        positions = [(low + high) / 2.0 for low, high in DIRECT_POSITION_LIMITS]
        self.assertTrue(valid_direct_feedback(positions, [0.0] * 12, [30.0] * 12, 70.0))
        positions[0] = DIRECT_POSITION_LIMITS[0][1] + 0.001
        self.assertFalse(valid_direct_feedback(positions, [0.0] * 12, [30.0] * 12, 70.0))


class TransportTests(unittest.TestCase):
    def frame(self, session="s", sequence=1):
        payload = {
            "schema_version": "1.1", "stamp": {"secs": 1, "nsecs": 2}, "sequence": 3,
            "emotion": "joy", "valence": 0.8, "arousal": 0.9,
            "backend": "deterministic", "source": "user", "turn_id": "t",
        }
        return (json.dumps({"schema_version": "1.0", "session_id": session,
                            "sequence": sequence, "payload": payload}) + "\n").encode()

    def test_sequence_replay_disconnect_and_new_session(self):
        gate = SequenceGate()
        self.assertTrue(gate.accept(decode_frame(self.frame(sequence=1))))
        self.assertFalse(gate.accept(decode_frame(self.frame(sequence=1))))
        self.assertTrue(gate.accept(decode_frame(self.frame(sequence=2))))
        self.assertTrue(gate.accept(decode_frame(self.frame(session="new", sequence=1))))

    def test_malformed_frame_rejected(self):
        with self.assertRaises(TransportError):
            decode_frame(b"{}")
        with self.assertRaises(TransportError):
            decode_frame(b"not-json\n")

    def test_authenticated_stop_relay_and_replay_gate(self):
        key = b"k" * 32
        frame = StopRelayFrame("session-a", 1, True, True, True)
        decoded = decode_stop_frame(key, encode_stop_frame(key, frame))
        self.assertEqual(decoded, frame)
        with self.assertRaises(StopRelayError):
            decode_stop_frame(b"x" * 32, encode_stop_frame(key, frame))
        gate = StopRelaySequenceGate()
        self.assertTrue(gate.accept(decoded, current_session_stale=True))
        self.assertFalse(gate.accept(decoded, current_session_stale=False))
        next_frame = StopRelayFrame("session-a", 2, False, True, True)
        self.assertTrue(gate.accept(next_frame, current_session_stale=False))
        replacement = StopRelayFrame("session-b", 1, False, False, False)
        self.assertFalse(gate.accept(replacement, current_session_stale=False))
        self.assertTrue(gate.accept(replacement, current_session_stale=True))


class SharedMemoryTests(unittest.TestCase):
    def test_sequence_locked_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "telemetry")
            writer = SharedTelemetry(path, create=True)
            reader = SharedTelemetry(path)
            writer.write(1234, b"packet")
            sequence, receive_ns, payload = reader.read()
            self.assertEqual(sequence % 2, 0)
            self.assertEqual(receive_ns, 1234)
            self.assertEqual(payload, b"packet")
            reader.close()
            writer.close()

    def test_emotion_record_preserves_contract_and_transport_identity(self):
        envelope = {
            "schema_version": "1.0", "session_id": "session-a", "sequence": 9,
            "payload": {
                "schema_version": "1.1", "sequence": 42, "emotion": "curiosity",
                "valence": 0.25, "arousal": 0.75, "turn_id": "turn-4",
            },
        }
        decoded = decode_emotion_record(encode_emotion_record(envelope))
        self.assertEqual(decoded["record_version"], "1.1")
        self.assertEqual(decoded["transport_sequence"], 9)
        self.assertEqual(decoded["state_sequence"], 42)
        self.assertEqual(decoded["emotion"], "curiosity")
        self.assertEqual(decoded["session_id"], "session-a")
        self.assertEqual(decoded["turn_id"], "turn-4")
        self.assertAlmostEqual(decoded["valence"], 0.25)
        self.assertAlmostEqual(decoded["arousal"], 0.75)

    def test_safety_record_preserves_stop_and_relay_state(self):
        decoded = decode_safety_record(encode_safety_record(True, True, False, 17))
        self.assertEqual(decoded, {
            "record_version": "1.0", "stop": True,
            "observed_fresh": True, "axes_zero": False, "sequence": 17,
        })


class MappingAndSupervisorTests(unittest.TestCase):
    def profiles(self):
        return {emotion: {"height": 0, "roll": 0, "pitch": 0, "yaw": 0} for emotion in EMOTIONS}

    def ready(self):
        return Gates(True, True, True, True, True, True, True, True, True, True, True)

    def test_vendor_feedback_validation_is_fail_closed(self):
        positions = [0.0] * 12
        self.assertTrue(valid_joint_feedback(EXPECTED_JOINT_NAMES, positions))
        self.assertFalse(valid_joint_feedback(EXPECTED_JOINT_NAMES[:-1], positions[:-1]))
        self.assertFalse(valid_joint_feedback(EXPECTED_JOINT_NAMES, positions[:-1] + [float("nan")]))
        self.assertTrue(valid_imu_feedback((0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0), (0.0, 0.0, 9.81)))
        self.assertFalse(valid_imu_feedback((0.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 9.81)))

    def test_all_profiles_and_exact_neutral(self):
        profiles = load_profiles(self.profiles())
        self.assertEqual(set(profiles), set(EMOTIONS))
        limiter = PostureLimiter({"height": AxisConfig(1, 20000, 1000, 500)})
        self.assertEqual(limiter.update(Pose(height=1.0), 0.1, True), {1: 50})
        self.assertEqual(limiter.update(Pose(height=1.0), 0.1, False), {1: 0})
        self.assertEqual(limiter.neutral(), {1: 0})

    def test_neutral_breathing_matches_gazebo_ratios_and_seams(self):
        breathing = NeutralBreather()
        self.assertEqual(breathing.height(0.0), 0.0)
        self.assertAlmostEqual(breathing.height(0.25), 1.0)
        self.assertAlmostEqual(breathing.height(0.25 + 0.6875), 0.65)
        self.assertAlmostEqual(breathing.height(0.25 + 1.375), -0.39)
        self.assertAlmostEqual(breathing.height(0.25 + 2.0625), 0.65)
        with self.assertRaises(ValueError):
            NeutralBreather(entrance_height=1.01)

    def test_direct_neutral_breathing_is_amplitude_and_rate_bounded(self):
        limits = DirectPoseLimits()
        breathing = RateLimitedHeight(limits)
        tick = 1.0 / limits.publish_rate
        values = [breathing.update(index * tick, tick) for index in range(1250)]

        self.assertLessEqual(max(values), limits.height_limit)
        self.assertGreaterEqual(min(values), -limits.height_limit)
        maximum_step = limits.maximum_rate * tick
        self.assertTrue(all(
            abs(current - previous) <= maximum_step + 1.0
            for previous, current in zip([0] + values[:-1], values)
        ))
        self.assertLessEqual(max(values), 0)
        self.assertLess(min(values), -5000)
        breathing.reset()
        self.assertEqual(breathing.current, 0.0)

    def test_direct_pose_limits_reject_uncommissioned_envelopes(self):
        invalid = (
            {"height_limit": 0},
            {"height_limit": 10001},
            {"maximum_rate": 0.0},
            {"publish_rate": 0.0},
            {"entrance_duration": 0.0},
            {"idle_period": 0.0},
            {"entrance_ratio": -1.01},
            {"idle_low_ratio": 0.7, "idle_high_ratio": 0.6},
        )
        for overrides in invalid:
            with self.subTest(overrides=overrides), self.assertRaises(ValueError):
                DirectPoseLimits(**overrides)

    def test_direct_gate_requires_neutral_and_at_least_25_percent_battery(self):
        snapshot = {
            "status": {
                "state": "DISARMED",
                "posture_armed": False,
                "dynamic_armed": False,
                "robot_basic_state": 6,
                "stable_stand": True,
                "telemetry_fresh": True,
                "joint_feedback_healthy": True,
                "imu_feedback_healthy": True,
                "retroid_fresh": True,
                "retroid_axes_zero": True,
                "stop": False,
                "link_fresh": True,
            },
            "robot": {
                "basic_state": 6,
                "battery": 25.0,
                "errors": 0,
                "roll_deg": 0.0,
                "pitch_deg": 0.0,
            },
            "emotion": {"emotion": "neutral"},
            "joints": [0.0] * 12,
            "imu": [0.0, 0.0],
            "ages": {"status": 0.01, "robot": 0.01, "emotion": 0.01, "joints": 0.01, "imu": 0.01},
        }
        self.assertTrue(safe_neutral_gate(snapshot, minimum_battery=25.0))

        snapshot["robot"]["battery"] = 24.999
        self.assertFalse(safe_neutral_gate(snapshot, minimum_battery=25.0))
        snapshot["robot"]["battery"] = 25.0
        snapshot["emotion"]["emotion"] = "joy"
        self.assertFalse(safe_neutral_gate(snapshot, minimum_battery=25.0))

    def test_direct_gate_fails_closed_for_each_live_safety_input(self):
        def ready_snapshot():
            return {
                "status": {
                    "state": "DISARMED", "posture_armed": False, "dynamic_armed": False,
                    "robot_basic_state": 6, "telemetry_fresh": True,
                    "stable_stand": True,
                    "joint_feedback_healthy": True, "imu_feedback_healthy": True,
                    "retroid_fresh": True, "retroid_axes_zero": True,
                    "stop": False, "link_fresh": True,
                },
                "robot": {
                    "basic_state": 6, "battery": 25.0, "errors": 0,
                    "roll_deg": 0.0, "pitch_deg": 0.0,
                },
                "emotion": {"emotion": "neutral"},
                "joints": [0.0] * 12,
                "imu": [0.0, 0.0],
                "ages": {"status": 0.01, "robot": 0.01, "emotion": 0.01, "joints": 0.01, "imu": 0.01},
            }

        mutations = (
            ("supervisor armed", lambda value: value["status"].update(state="POSTURE_ARMED")),
            ("posture armed", lambda value: value["status"].update(posture_armed=True)),
            ("dynamic armed", lambda value: value["status"].update(dynamic_armed=True)),
            ("unstable stand", lambda value: value["status"].update(stable_stand=False)),
            ("telemetry stale", lambda value: value["status"].update(telemetry_fresh=False)),
            ("joint feedback unhealthy", lambda value: value["status"].update(joint_feedback_healthy=False)),
            ("IMU feedback unhealthy", lambda value: value["status"].update(imu_feedback_healthy=False)),
            ("Retroid stale", lambda value: value["status"].update(retroid_fresh=False)),
            ("Retroid active", lambda value: value["status"].update(retroid_axes_zero=False)),
            ("STOP active", lambda value: value["status"].update(stop=True)),
            ("emotion link stale", lambda value: value["status"].update(link_fresh=False)),
            ("robot error", lambda value: value["robot"].update(errors=1)),
            ("excess roll", lambda value: value["robot"].update(roll_deg=10.01)),
            ("incomplete joints", lambda value: value.update(joints=[0.0] * 11)),
            ("incomplete IMU", lambda value: value.update(imu=[0.0])),
            ("stale status age", lambda value: value["ages"].update(status=0.31)),
            ("stale robot age", lambda value: value["ages"].update(robot=0.31)),
            ("stale emotion age", lambda value: value["ages"].update(emotion=0.51)),
            ("stale joint age", lambda value: value["ages"].update(joints=0.21)),
            ("stale IMU age", lambda value: value["ages"].update(imu=0.21)),
        )
        for label, mutate in mutations:
            snapshot = ready_snapshot()
            mutate(snapshot)
            with self.subTest(label=label):
                self.assertFalse(safe_neutral_gate(snapshot))

    def test_cooldown_deduplication_and_stomp_alternation(self):
        scheduler = ActionScheduler()
        self.assertEqual(scheduler.on_emotion("anger", 0, True), ("stomp", "front_left"))
        self.assertIsNone(scheduler.on_emotion("anger", 1, True))
        scheduler.complete("stomp", True)
        scheduler.on_emotion("neutral", 11, True)
        self.assertEqual(scheduler.on_emotion("anger", 12, True), ("stomp", "front_right"))
        scheduler.complete("stomp", False)
        scheduler.on_emotion("neutral", 23, True)
        self.assertEqual(scheduler.on_emotion("anger", 24, True), ("stomp", "front_right"))

    def test_supervisor_exclusivity_link_loss_fault_and_crash_recovery(self):
        supervisor = Supervisor(transmit_enabled=True)
        gates = self.ready()
        self.assertTrue(supervisor.set_armed(True, gates))
        self.assertEqual(supervisor.state, State.POSTURE_ARMED)
        self.assertTrue(supervisor.set_dynamic(True, gates))
        self.assertTrue(supervisor.request_action("hop", gates))
        supervisor.posture_neutral()
        self.assertEqual(supervisor.state, State.SDK_TAKEOVER)
        supervisor.sdk_acquired()
        supervisor.ai_link_lost()
        self.assertEqual(supervisor.state, State.RECOVERY)
        supervisor.immediate_fault("timing failure")
        self.assertEqual(supervisor.state, State.FAULT)
        self.assertTrue(supervisor.release_required)
        restored = Supervisor()
        restored.restore_after_crash(True)
        self.assertEqual(restored.state, State.RECOVERY)
        self.assertTrue(restored.release_required)

    def test_uncommissioned_system_cannot_arm(self):
        supervisor = Supervisor(transmit_enabled=False)
        self.assertFalse(supervisor.set_armed(True, self.ready()))
        supervisor = Supervisor(transmit_enabled=True)
        incomplete = self.ready()
        incomplete.imu_healthy = False
        self.assertFalse(supervisor.set_armed(True, incomplete))
        incomplete = self.ready()
        incomplete.sdk_imu_healthy = False
        self.assertTrue(supervisor.set_armed(True, incomplete))
        self.assertFalse(supervisor.set_dynamic(True, incomplete))

    def test_action_phase_verification(self):
        hop = ActionMachine("hop")
        self.assertEqual(hop.update(ActionObservation(stable=True, four_contacts=True)), "crouch")
        hop.update(ActionObservation())
        hop.update(ActionObservation())
        self.assertEqual(hop.phase, "flight")
        self.assertEqual(hop.update(ActionObservation(all_unloaded=True)), "landing")
        self.assertEqual(hop.update(ActionObservation(four_contacts=True)), "recovery")
        self.assertFalse(hop.complete)
        self.assertEqual(hop.update(ActionObservation(stable=True, four_contacts=True)), "recovery")
        self.assertTrue(hop.complete)
        stomp = ActionMachine("stomp")
        self.assertEqual(stomp.update(ActionObservation(attitude_ok=False)), "release")
        self.assertTrue(stomp.failed)

    def test_all_nine_direct_joint_patterns_are_smooth_and_planted(self):
        self.assertEqual(set(PATTERNS), set(EMOTIONS))
        self.assertEqual(len(DIRECT_JOINT_NAMES), 12)
        validate_patterns()
        anchor = (0.0, -0.75, 1.40) * 4
        signatures = set()
        for emotion, pattern in PATTERNS.items():
            samples = [sample_pattern(pattern, pattern.duration * index / 80.0) for index in range(80)]
            joint_samples = [planted_joint_sample(anchor, value) for value in samples]
            self.assertTrue(all(validate_planted_symmetry(anchor, value) for value in joint_samples))
            self.assertTrue(all(value.position[0] == anchor[0] for value in joint_samples))
            self.assertTrue(all(value.position[3] == anchor[3] for value in joint_samples))
            self.assertGreater(max(value.position for value in samples), 0.0)
            signatures.add(tuple(round(value.position, 5) for value in samples))
        self.assertEqual(len(signatures), len(EMOTIONS))

    def test_quintic_preserves_position_velocity_acceleration_boundaries(self):
        start = ScalarSample(0.01, -0.02, 0.03)
        end = ScalarSample(0.0, 0.0, 0.0)
        segment = Quintic(start, end, 1.25)
        first = segment.sample(0.0)
        last = segment.sample(1.25)
        self.assertAlmostEqual(first.position, start.position)
        self.assertAlmostEqual(first.velocity, start.velocity)
        self.assertAlmostEqual(first.acceleration, start.acceleration)
        self.assertAlmostEqual(last.position, end.position)
        self.assertAlmostEqual(last.velocity, end.velocity)
        self.assertAlmostEqual(last.acceleration, end.acceleration)

    def test_planted_mapper_rejects_extension_and_asymmetry(self):
        anchor = (0.0, -0.75, 1.40) * 4
        with self.assertRaises(ValueError):
            planted_joint_sample(anchor, ScalarSample(-0.001, 0.0, 0.0))
        sample = planted_joint_sample(anchor, ScalarSample(0.01, 0.02, -0.03))
        self.assertTrue(validate_planted_symmetry(anchor, sample))
        broken = type(sample)(sample.position[:-1] + (sample.position[-1] + 0.001,),
                              sample.velocity, sample.acceleration)
        self.assertFalse(validate_planted_symmetry(anchor, broken))

    def test_expression_replacement_returns_through_exact_neutral(self):
        planner = ExpressionPlanner(neutral_return_time=1.0, neutral_hold_time=0.25)
        planner.request("joy", 0.0)
        planner.sample(1.3)
        active = planner.sample(1.8)
        self.assertGreaterEqual(active.position, 0.0)
        planner.request("anger", 1.8)
        neutral = planner.sample(2.8)
        self.assertAlmostEqual(neutral.position, 0.0, places=9)
        self.assertAlmostEqual(neutral.velocity, 0.0, places=9)
        self.assertAlmostEqual(neutral.acceleration, 0.0, places=9)
        self.assertEqual(planner.output_emotion, "neutral")
        planner.sample(3.06)
        self.assertEqual(planner.output_emotion, "anger")


if __name__ == "__main__":
    unittest.main()
