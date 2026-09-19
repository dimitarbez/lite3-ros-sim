"""Strict parsers for observed Lite3 Ethernet/UDP traffic."""

from __future__ import annotations

import ipaddress
import math
import struct
from dataclasses import dataclass
from typing import Dict, Optional, Tuple

COMMAND_TYPE_COMPLEX = 1
SDK_JOINT_COMMAND_CODE = 0x0111
ROBOT_STATE_CODE = 0x0901
JOINT_ANGLE_CODE = 0x0902
JOINT_VELOCITY_CODE = 0x0903
SDK_STATE_CODE = 0x0906
STOP_CODE = 0x21020C0E
AXIS_CODES = {0x21010131, 0x21010130, 0x21010135, 0x21010102}


class PacketError(ValueError):
    pass


@dataclass(frozen=True)
class UdpDatagram:
    source_ip: str
    destination_ip: str
    source_port: int
    destination_port: int
    payload: bytes


@dataclass(frozen=True)
class CommandPacket:
    code: int
    value_or_size: int
    command_type: int
    body: bytes


@dataclass(frozen=True)
class RobotState0901:
    robot_basic_state: int
    robot_gait_state: int
    roll_deg: float
    pitch_deg: float
    yaw_deg: float
    error_flags: int
    robot_motion_state: int
    battery_level: float
    zero_position_flag: bool


@dataclass(frozen=True)
class HighRate0906:
    tick: int
    joints: Tuple[Tuple[float, float, float, float], ...]
    imu: Tuple[float, ...]
    contacts: Tuple[float, ...]


def parse_ethernet_udp(frame: bytes) -> UdpDatagram:
    if len(frame) < 14:
        raise PacketError("truncated ethernet frame")
    ethertype = struct.unpack_from("!H", frame, 12)[0]
    offset = 14
    if ethertype in (0x8100, 0x88A8):
        if len(frame) < 18:
            raise PacketError("truncated VLAN frame")
        ethertype = struct.unpack_from("!H", frame, 16)[0]
        offset = 18
    if ethertype != 0x0800:
        raise PacketError("not IPv4")
    if len(frame) < offset + 20:
        raise PacketError("truncated IPv4 header")
    version_ihl = frame[offset]
    if version_ihl >> 4 != 4:
        raise PacketError("not IPv4")
    ihl = (version_ihl & 0x0F) * 4
    if ihl < 20 or len(frame) < offset + ihl:
        raise PacketError("invalid IPv4 header length")
    total_length = struct.unpack_from("!H", frame, offset + 2)[0]
    fragment = struct.unpack_from("!H", frame, offset + 6)[0]
    if fragment & 0x3FFF:
        raise PacketError("fragmented datagram rejected")
    if frame[offset + 9] != 17:
        raise PacketError("not UDP")
    if total_length < ihl + 8 or len(frame) < offset + total_length:
        raise PacketError("truncated IPv4 payload")
    source_ip = str(ipaddress.ip_address(frame[offset + 12 : offset + 16]))
    destination_ip = str(ipaddress.ip_address(frame[offset + 16 : offset + 20]))
    udp_offset = offset + ihl
    source_port, destination_port, udp_length = struct.unpack_from("!HHH", frame, udp_offset)
    if udp_length < 8 or udp_length != total_length - ihl:
        raise PacketError("invalid UDP length")
    payload = frame[udp_offset + 8 : udp_offset + udp_length]
    return UdpDatagram(source_ip, destination_ip, source_port, destination_port, payload)


def filter_datagram(datagram: UdpDatagram, source_ip: str, destination_port: int,
                    destination_ip: Optional[str] = None) -> bytes:
    if datagram.source_ip != source_ip:
        raise PacketError("unexpected source")
    if datagram.destination_port != destination_port:
        raise PacketError("unexpected destination port")
    if destination_ip is not None and datagram.destination_ip != destination_ip:
        raise PacketError("unexpected destination")
    return datagram.payload


def parse_command(payload: bytes) -> CommandPacket:
    if len(payload) < 12:
        raise PacketError("truncated command header")
    code, value_or_size, command_type = struct.unpack_from("<III", payload, 0)
    body = payload[12:]
    if command_type == 0:
        if body:
            raise PacketError("simple command has a body")
    elif command_type == COMMAND_TYPE_COMPLEX:
        if value_or_size != len(body):
            raise PacketError("complex command length mismatch")
    else:
        raise PacketError("unknown command type")
    return CommandPacket(code, value_or_size, command_type, body)


def is_sdk_joint_command(command: CommandPacket) -> bool:
    """Match the official 12-joint MotionSDK command without decoding it."""
    return (
        command.code == SDK_JOINT_COMMAND_CODE
        and command.command_type == COMMAND_TYPE_COMPLEX
        and command.value_or_size == 240
        and len(command.body) == 240
    )


def parse_robot_state_0901(command: CommandPacket) -> RobotState0901:
    if command.code != ROBOT_STATE_CODE or command.command_type != COMMAND_TYPE_COMPLEX:
        raise PacketError("not a 0x0901 complex state")
    if len(command.body) != 200:
        raise PacketError("0x0901 body must be 200 bytes")
    body = command.body
    robot_basic_state, robot_gait_state = struct.unpack_from("<ii", body, 0)
    roll_deg, pitch_deg, yaw_deg = struct.unpack_from("<ddd", body, 8)
    error_flags = struct.unpack_from("<I", body, 160)[0]
    robot_motion_state = struct.unpack_from("<i", body, 164)[0]
    battery_level = struct.unpack_from("<d", body, 168)[0]
    zero_position_flag = bool(body[181])
    finite = (roll_deg, pitch_deg, yaw_deg, battery_level)
    if not all(math.isfinite(value) for value in finite):
        raise PacketError("0x0901 contains non-finite values")
    # Deployed Deeprcs 2.0.153 reports percentage units (for example 25.0),
    # while older fixtures used a normalized fraction. Accept both encodings
    # without converting so the reported unit remains inspectable.
    if not 0.0 <= battery_level <= 100.0:
        raise PacketError("implausible battery level")
    return RobotState0901(
        robot_basic_state,
        robot_gait_state,
        roll_deg,
        pitch_deg,
        yaw_deg,
        error_flags,
        robot_motion_state,
        battery_level,
        zero_position_flag,
    )


def parse_joint_vector(command: CommandPacket, expected_code: int) -> Tuple[float, ...]:
    """Decode documented 0x0902/0x0903 twelve-double joint telemetry."""
    if expected_code not in (JOINT_ANGLE_CODE, JOINT_VELOCITY_CODE):
        raise PacketError("unsupported joint-vector code")
    if command.code != expected_code or command.command_type != COMMAND_TYPE_COMPLEX:
        raise PacketError("unexpected joint-vector command")
    if len(command.body) != 12 * 8:
        raise PacketError("joint-vector body must be 96 bytes")
    values = struct.unpack("<12d", command.body)
    if not all(math.isfinite(value) for value in values):
        raise PacketError("joint-vector contains non-finite values")
    return values


def decode_sdk_0906(command: CommandPacket, layout: Dict[str, object]) -> HighRate0906:
    if command.code != SDK_STATE_CODE or command.command_type != COMMAND_TYPE_COMPLEX:
        raise PacketError("not a 0x0906 complex state")
    if not isinstance(layout, dict) or layout.get("id") != "deeprcs-2.0.153-reviewed-0906-v1":
        raise PacketError("0x0906 layout is not reviewed for Deeprcs 2.0.153")
    if layout.get("deeprcs_version") != "2.0.153":
        raise PacketError("Deeprcs version mismatch")
    expected_size = int(layout.get("body_size", -1))
    if len(command.body) != expected_size or expected_size != 368:
        raise PacketError("0x0906 body size mismatch")
    tick_offset = int(layout.get("tick_offset", -1))
    offsets = [int(layout.get(name, -1)) for name in ("joint_offset", "imu_offset", "contact_offset")]
    sizes = (12 * 16, 9 * 4, 12 * 8)
    if tick_offset < 0 or tick_offset + 4 > len(command.body) or tick_offset % 4:
        raise PacketError("0x0906 tick offset is incomplete")
    if any(offset < 0 or offset + size > len(command.body) for offset, size in zip(offsets, sizes)):
        raise PacketError("0x0906 reviewed offsets are incomplete")
    ranges = sorted((offset, offset + size) for offset, size in zip(offsets, sizes))
    if any(start % 4 for start, _ in ranges) or any(
        end > next_start for (_, end), (next_start, _) in zip(ranges, ranges[1:])
    ):
        raise PacketError("0x0906 reviewed offsets overlap or are unaligned")
    joint_values = struct.unpack_from("<48f", command.body, offsets[0])
    imu = struct.unpack_from("<9f", command.body, offsets[1])
    contacts = struct.unpack_from("<12d", command.body, offsets[2])
    if not all(math.isfinite(value) for value in joint_values + imu + contacts):
        raise PacketError("0x0906 contains non-finite values")
    joints = tuple(tuple(joint_values[index : index + 4]) for index in range(0, 48, 4))
    for position, velocity, torque, temperature in joints:
        if abs(position) > 10.0 or abs(velocity) > 100.0 or abs(torque) > 200.0 or not -20.0 <= temperature <= 120.0:
            raise PacketError("0x0906 joint value outside validation envelope")
    # MotionSDK reports orientation and angular velocity in degrees and
    # degrees/second.  Yaw is an absolute heading and can legitimately span
    # the full circle even while roll/pitch remain near level.
    if any(abs(value) > limit for value, limit in zip(
        imu, (180, 180, 360, 2000, 2000, 2000, 200, 200, 200)
    )):
        raise PacketError("0x0906 IMU value outside validation envelope")
    if any(abs(value) > 5000.0 for value in contacts):
        raise PacketError("0x0906 contact value outside validation envelope")
    tick = struct.unpack_from("<I", command.body, tick_offset)[0]
    return HighRate0906(tick, joints, tuple(imu), tuple(contacts))


def validate_sdk_0906(command: CommandPacket, layout: Dict[str, object]) -> bool:
    decode_sdk_0906(command, layout)
    return True


def parse_simple_stop(command: CommandPacket) -> bool:
    return command.command_type == 0 and command.code == STOP_CODE and command.value_or_size == 0


def retroid_axes_zero(command: CommandPacket) -> Optional[bool]:
    if command.command_type != 0 or command.code not in AXIS_CODES:
        return None
    signed_value = struct.unpack("<i", struct.pack("<I", command.value_or_size))[0]
    return signed_value == 0
