"""Versioned sequence-lock shared-memory record for high-rate telemetry."""

from __future__ import annotations

import mmap
import os
import struct
from typing import Dict, Optional, Tuple

MAGIC = b"EBL3"
VERSION = 1
HEADER = struct.Struct("<4sIQQI")
CAPACITY = 4096

EMOTION_RECORD_VERSION = 0x00010001
EMOTION_WIRE = struct.Struct("<IQQdd16s64s128s")
SAFETY_RECORD_VERSION = 0x00010000
SAFETY_WIRE = struct.Struct("<I???5xQ")


def _encode_text(value: str, size: int, field: str) -> bytes:
    if not isinstance(value, str) or not value:
        raise ValueError("%s must be a non-empty string" % field)
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise ValueError("%s is too long for the shared record" % field)
    return encoded + b"\0" * (size - len(encoded))


def _decode_text(value: bytes) -> str:
    return value.split(b"\0", 1)[0].decode("utf-8")


def encode_emotion_record(envelope: Dict[str, object]) -> bytes:
    payload = envelope["payload"]
    return EMOTION_WIRE.pack(
        EMOTION_RECORD_VERSION,
        int(envelope["sequence"]),
        int(payload["sequence"]),
        float(payload["valence"]),
        float(payload["arousal"]),
        _encode_text(payload["emotion"], 16, "emotion"),
        _encode_text(envelope["session_id"], 64, "session_id"),
        _encode_text(payload["turn_id"], 128, "turn_id"),
    )


def decode_emotion_record(value: bytes) -> Dict[str, object]:
    if len(value) != EMOTION_WIRE.size:
        raise ValueError("unexpected emotion shared-record size")
    (version, transport_sequence, state_sequence, valence, arousal,
     emotion, session_id, turn_id) = EMOTION_WIRE.unpack(value)
    if version != EMOTION_RECORD_VERSION:
        raise ValueError("unsupported emotion shared-record version")
    return {
        "record_version": "1.1",
        "transport_sequence": transport_sequence,
        "state_sequence": state_sequence,
        "valence": valence,
        "arousal": arousal,
        "emotion": _decode_text(emotion),
        "session_id": _decode_text(session_id),
        "turn_id": _decode_text(turn_id),
    }


def encode_safety_record(stop: bool, observed_fresh: bool, axes_zero: bool,
                         sequence: int) -> bytes:
    return SAFETY_WIRE.pack(
        SAFETY_RECORD_VERSION, bool(stop), bool(observed_fresh),
        bool(axes_zero), int(sequence),
    )


def decode_safety_record(value: bytes) -> Dict[str, object]:
    if len(value) != SAFETY_WIRE.size:
        raise ValueError("unexpected safety shared-record size")
    version, stop, observed_fresh, axes_zero, sequence = SAFETY_WIRE.unpack(value)
    if version != SAFETY_RECORD_VERSION:
        raise ValueError("unsupported safety shared-record version")
    return {
        "record_version": "1.0",
        "stop": stop,
        "observed_fresh": observed_fresh,
        "axes_zero": axes_zero,
        "sequence": sequence,
    }


class SharedTelemetry:
    def __init__(self, path: str, create: bool = False):
        flags = os.O_RDWR | (os.O_CREAT if create else 0)
        self.fd = os.open(path, flags, 0o640)
        if create:
            os.ftruncate(self.fd, CAPACITY)
        if os.fstat(self.fd).st_size != CAPACITY:
            os.close(self.fd)
            raise ValueError("unexpected shared telemetry size")
        self.mapping = mmap.mmap(self.fd, CAPACITY)
        if create and self.mapping[:4] != MAGIC:
            self.mapping[: HEADER.size] = HEADER.pack(MAGIC, VERSION, 0, 0, 0)

    def close(self):
        self.mapping.close()
        os.close(self.fd)

    def write(self, receive_ns: int, payload: bytes):
        if len(payload) > CAPACITY - HEADER.size:
            raise ValueError("telemetry payload too large")
        _, _, sequence, _, _ = HEADER.unpack(self.mapping[: HEADER.size])
        odd = sequence + 1 if sequence % 2 == 0 else sequence + 2
        self.mapping[: HEADER.size] = HEADER.pack(MAGIC, VERSION, odd, receive_ns, len(payload))
        self.mapping[HEADER.size : HEADER.size + len(payload)] = payload
        self.mapping[: HEADER.size] = HEADER.pack(MAGIC, VERSION, odd + 1, receive_ns, len(payload))

    def read(self, retries: int = 4) -> Optional[Tuple[int, int, bytes]]:
        for _ in range(retries):
            first = HEADER.unpack(self.mapping[: HEADER.size])
            magic, version, sequence, receive_ns, length = first
            if magic != MAGIC or version != VERSION or sequence % 2 or length > CAPACITY - HEADER.size:
                continue
            payload = bytes(self.mapping[HEADER.size : HEADER.size + length])
            second = HEADER.unpack(self.mapping[: HEADER.size])
            if first == second:
                return sequence, receive_ns, payload
        return None
